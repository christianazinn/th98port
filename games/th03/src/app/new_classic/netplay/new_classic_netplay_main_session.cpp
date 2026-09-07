#include "th03/app/new_classic/netplay/new_classic_netplay_main_session.hpp"

#include <algorithm>
#include <utility>

namespace th03::app {
namespace {

constexpr std::uint16_t input_cancel = 0x1000U;

[[nodiscard]] NewClassicNetplayMainSessionError session_error(
	NewClassicNetplayMainSessionErrorCode code
) noexcept
{
	return NewClassicNetplayMainSessionError{.code = code};
}

[[nodiscard]] NewClassicNetplayMainSessionError timeline_error(
	NewClassicNetplayTimelineError error
) noexcept
{
	return NewClassicNetplayMainSessionError{
		.code = NewClassicNetplayMainSessionErrorCode::timeline_failed,
		.timeline = error,
	};
}

[[nodiscard]] NewClassicNetplayMainSessionError rollback_error(
	MainRollbackCoordinatorError error
) noexcept
{
	return NewClassicNetplayMainSessionError{
		.code = NewClassicNetplayMainSessionErrorCode::rollback_failed,
		.rollback = error,
	};
}

[[nodiscard]] NewClassicNetplayMainSessionError input_delay_error(
	NewClassicNetplayInputDelayError error
) noexcept
{
	return NewClassicNetplayMainSessionError{
		.code = NewClassicNetplayMainSessionErrorCode::input_delay_failed,
		.input_delay = error,
	};
}

} // namespace

struct NewClassicNetplayMainSession::Impl final {
	static constexpr std::size_t digest_queue_size = 8U;
	struct FrameFact final {
		bool valid = false;
		bool gameplay = false;
		std::uint64_t frame = 0U;
		CoreInput input{};
		StepResult step_result{};
		std::uint8_t source_vsyncs = 1U;
	};

	struct AudioSlot final {
		bool valid = false;
		std::uint64_t frame = 0U;
		std::array<
			ClassicAudioCommand,
			new_classic_netplay_audio_commands_per_frame
		> commands{};
		std::uint8_t command_count = 0U;
		std::uint8_t source_vsyncs = 1U;
	};

	std::unique_ptr<NewClassicNetplayInputTimeline> timeline{};
	std::optional<NewClassicNetplayInputDelay> input_delay{};
	std::unique_ptr<MainRollbackCoordinator> coordinator{};
	std::array<AudioSlot, new_classic_netplay_timeline_slots> audio_slots{};
	std::array<
		ClassicAudioCommand,
		new_classic_netplay_audio_commands_per_frame
	> bootstrap_audio{};
	std::uint8_t bootstrap_audio_head = 0U;
	std::uint8_t bootstrap_audio_count = 0U;
	std::array<
		ClassicAudioCommand,
		new_classic_netplay_audio_commands_per_frame
	> pause_audio{};
	std::uint8_t pause_audio_head = 0U;
	std::uint8_t pause_audio_count = 0U;
	std::uint64_t next_audio_frame = 0U;
	std::uint64_t next_transcript_frame = 0U;
	std::array<
		NewClassicNetplayConfirmedSimulationDigest,
		digest_queue_size
	> digest_queue{};
	std::uint8_t digest_head = 0U;
	std::uint8_t digest_count = 0U;
	std::array<
		FrameFact,
		new_classic_netplay_timeline_slots
	> frame_facts{};
	std::uint64_t digest_scan_frame = 0U;
	std::uint64_t confirmed_gameplay_frames = 0U;
	std::uint64_t next_digest_gameplay_frame =
		new_classic_netplay_simulation_digest_interval;
	NewClassicNetplayMainSessionStatus status =
		NewClassicNetplayMainSessionStatus::running;
	std::optional<std::uint64_t> boundary_frame{};
	std::optional<StepStatus> boundary{};
	std::optional<SessionTransition> transition{};
	InputContext input_context = InputContext::gameplay;
	std::uint8_t warning_samples_remaining = 0U;
	std::uint8_t hold_samples_remaining = 0U;
	std::uint8_t pending_hold_samples = 0U;
	std::uint64_t resimulated_frames = 0U;
	std::uint64_t prediction_stall_events = 0U;
	std::uint64_t remote_wait_events = 0U;
	std::uint8_t maximum_prediction_depth = 0U;
	bool gameplay_reliable_warmup = false;

	[[nodiscard]] bool drain_audio(
		std::span<ClassicAudioCommand> destination,
		std::uint8_t &count
	) noexcept
	{
		while(const auto command = coordinator->take_audio_command()) {
			if(count >= destination.size()) {
				return false;
			}
			destination[count++] = *command;
		}
		return true;
	}

	[[nodiscard]] bool record_audio(
		std::uint64_t frame,
		std::uint8_t source_vsyncs
	) noexcept
	{
		auto &slot = audio_slots[frame % audio_slots.size()];
		slot = AudioSlot{
			.valid = true,
			.frame = frame,
			.source_vsyncs = source_vsyncs,
		};
		return drain_audio(slot.commands, slot.command_count);
	}

	[[nodiscard]] bool append_audio(std::uint64_t frame) noexcept
	{
		auto &slot = audio_slots[frame % audio_slots.size()];
		return slot.valid && slot.frame == frame &&
			drain_audio(slot.commands, slot.command_count);
	}

	void update_boundary_confirmation() noexcept
	{
		if(boundary_frame &&
			timeline->confirmed_frame_count() > *boundary_frame) {
			status = NewClassicNetplayMainSessionStatus::boundary_confirmed;
		}
	}

	[[nodiscard]] std::expected<void, NewClassicNetplayMainSessionError>
	enqueue_confirmed_digest(
		std::uint64_t frame,
		NewClassicNetplaySimulationDigestKind kind,
		NewClassicNetplayBoundaryProof boundary_proof
	) noexcept
	{
		if(digest_count >= digest_queue.size()) {
			return std::unexpected(session_error(
				NewClassicNetplayMainSessionErrorCode::digest_journal_failed
			));
		}
		const auto digest = coordinator->simulation_digest(frame);
		if(!digest) {
			return std::unexpected(rollback_error(digest.error()));
		}
		const auto index = static_cast<std::size_t>(
			(digest_head + digest_count) % digest_queue.size()
		);
		digest_queue[index] = {
			.frame = frame,
			.confirmed_gameplay_frame_count = confirmed_gameplay_frames,
			.kind = kind,
			.boundary = boundary_proof,
			.digest = *digest,
		};
		++digest_count;
		return {};
	}

	[[nodiscard]] std::expected<void, NewClassicNetplayMainSessionError>
	capture_confirmed_digests() noexcept
	{
		const auto confirmed = timeline->confirmed_frame_count();
		while(digest_scan_frame < confirmed) {
			const auto &fact = frame_facts[
				digest_scan_frame % frame_facts.size()
			];
			if(!fact.valid || fact.frame != digest_scan_frame) {
				return std::unexpected(session_error(
					NewClassicNetplayMainSessionErrorCode::
						digest_journal_failed
				));
			}
			bool periodic_due = false;
			if(fact.gameplay) {
				++confirmed_gameplay_frames;
				if(confirmed_gameplay_frames ==
					next_digest_gameplay_frame) {
					periodic_due = true;
					next_digest_gameplay_frame +=
						new_classic_netplay_simulation_digest_interval;
				}
			}
			const auto post_frame = digest_scan_frame + 1U;
			const auto boundary_proof = new_classic_netplay_boundary_proof(
				fact.step_result.status
			);
			if(boundary_proof != NewClassicNetplayBoundaryProof::none) {
				if(const auto queued = enqueue_confirmed_digest(
					post_frame,
					NewClassicNetplaySimulationDigestKind::boundary,
					boundary_proof
				); !queued) {
					return queued;
				}
			} else if(periodic_due) {
				if(const auto queued = enqueue_confirmed_digest(
					post_frame,
					NewClassicNetplaySimulationDigestKind::periodic,
					NewClassicNetplayBoundaryProof::none
				); !queued) {
					return queued;
				}
			}
			++digest_scan_frame;
		}
		return {};
	}

	[[nodiscard]] std::expected<
		MainRollbackCoordinatorTick,
		NewClassicNetplayMainSessionError
	> execute(const NewClassicNetplayResolvedFrame &resolved) noexcept
	{
		const bool gameplay_frame = input_context == InputContext::gameplay;
		if(resolved.pause_requested && !gameplay_frame) {
			return std::unexpected(session_error(
				NewClassicNetplayMainSessionErrorCode::invalid_boundary
			));
		}
		auto process_input = resolved.combined;
		if(resolved.pause_requested) {
			process_input.system |= input_cancel;
		}
		auto stepped = input_context == InputContext::gameplay &&
			!gameplay_reliable_warmup ?
			coordinator->advance_confirmed(process_input) :
			coordinator->advance_reliable(process_input, input_context);
		if(!stepped) {
			return std::unexpected(rollback_error(stepped.error()));
		}
		if(!record_audio(
			resolved.frame, stepped->step_result.next_step_vsyncs
		)) {
			return std::unexpected(session_error(
				NewClassicNetplayMainSessionErrorCode::audio_journal_failed
			));
		}
		if(input_context == InputContext::interstitial) {
			const bool final_sample = warning_samples_remaining == 1U;
			const bool valid_intermediate = !final_sample &&
				stepped->step_result.status == StepStatus::running;
			const bool valid_final = final_sample &&
				(stepped->step_result.status == StepStatus::phase_transition ||
				 stepped->step_result.status == StepStatus::round_transition ||
				 stepped->step_result.status == StepStatus::terminal);
			if(warning_samples_remaining == 0U ||
				(!valid_intermediate && !valid_final)) {
				return std::unexpected(session_error(
					NewClassicNetplayMainSessionErrorCode::invalid_boundary
				));
			}
			--warning_samples_remaining;
		}
		if(stepped->step_result.status != StepStatus::running) {
			boundary_frame = resolved.frame;
			boundary = stepped->step_result.status;
			status = NewClassicNetplayMainSessionStatus::
				boundary_waiting_confirmation;
			update_boundary_confirmation();
		} else {
			status = NewClassicNetplayMainSessionStatus::running;
		}
		pending_hold_samples = stepped->step_result.next_step_vsyncs > 1U ?
			static_cast<std::uint8_t>(
				stepped->step_result.next_step_vsyncs - 1U
			) : 0U;
		if(stepped->step_result.status == StepStatus::running) {
			hold_samples_remaining = pending_hold_samples;
			pending_hold_samples = 0U;
		}
		if(gameplay_reliable_warmup &&
			stepped->step_result.status == StepStatus::running) {
			const auto reset = coordinator->try_reset_rollback_boundary();
			if(!reset) {
				return std::unexpected(rollback_error(reset.error()));
			}
			gameplay_reliable_warmup = !*reset;
		} else if(input_context == InputContext::gameplay &&
			stepped->step_result.status == StepStatus::running) {
			gameplay_reliable_warmup =
				!coordinator->rollback_boundary_available();
		}
		frame_facts[resolved.frame % frame_facts.size()] = {
			.valid = true,
			.gameplay = gameplay_frame,
			.frame = resolved.frame,
			.input = process_input,
			.step_result = stepped->step_result,
			.source_vsyncs = stepped->step_result.next_step_vsyncs,
		};
		return *stepped;
	}
};

std::expected<
	std::unique_ptr<NewClassicNetplayMainSession>,
	NewClassicNetplayMainSessionError
> NewClassicNetplayMainSession::create(
	NewClassicNetplayPlayer local_player,
	std::unique_ptr<MainProcess> process,
	std::uint8_t input_delay_frames
)
{
	if(!process) {
		return std::unexpected(session_error(
			NewClassicNetplayMainSessionErrorCode::invalid_process
		));
	}
	auto timeline = NewClassicNetplayInputTimeline::create(local_player);
	if(!timeline) {
		return std::unexpected(timeline_error(timeline.error()));
	}
	auto input_delay = NewClassicNetplayInputDelay::create(input_delay_frames);
	if(!input_delay) {
		return std::unexpected(input_delay_error(input_delay.error()));
	}
	auto coordinator = MainRollbackCoordinator::create(std::move(process));
	if(!coordinator) {
		return std::unexpected(rollback_error(coordinator.error()));
	}
	auto impl = std::make_unique<Impl>();
	impl->timeline = std::move(*timeline);
	impl->input_delay = std::move(*input_delay);
	impl->coordinator = std::move(*coordinator);
	if(!impl->drain_audio(
		impl->bootstrap_audio, impl->bootstrap_audio_count
	)) {
		return std::unexpected(session_error(
			NewClassicNetplayMainSessionErrorCode::audio_journal_failed
		));
	}
	return std::unique_ptr<NewClassicNetplayMainSession>(
		new NewClassicNetplayMainSession(std::move(impl))
	);
}

NewClassicNetplayMainSession::NewClassicNetplayMainSession(
	std::unique_ptr<Impl> impl
) noexcept :
	impl_(std::move(impl))
{}

NewClassicNetplayMainSession::~NewClassicNetplayMainSession() = default;

std::expected<
	NewClassicNetplayMainSessionTick,
	NewClassicNetplayMainSessionError
> NewClassicNetplayMainSession::advance_local(
	NewClassicNetplayOwnedInput input
) noexcept
{
	if(impl_->boundary_frame || impl_->status ==
		NewClassicNetplayMainSessionStatus::complete) {
		return std::unexpected(session_error(
			NewClassicNetplayMainSessionErrorCode::not_running
		));
	}
	if(impl_->hold_samples_remaining != 0U) {
		--impl_->hold_samples_remaining;
		return NewClassicNetplayMainSessionTick{
			.status = impl_->status,
			.confirmed_frame_count =
				impl_->timeline->confirmed_frame_count(),
		};
	}
	const auto frame = impl_->timeline->next_frame();
	auto effective = input;
	if(impl_->input_context == InputContext::gameplay) {
		if(!impl_->input_delay) {
			return std::unexpected(input_delay_error(
				NewClassicNetplayInputDelayError::invalid_delay
			));
		}
		const auto delayed = impl_->input_delay->stage(frame, input);
		if(!delayed) {
			return std::unexpected(input_delay_error(delayed.error()));
		}
		effective = *delayed;
	}
	const auto submitted = impl_->timeline->submit_local(frame, effective);
	if(!submitted) {
		return std::unexpected(timeline_error(submitted.error()));
	}
	const auto prior_status = impl_->status;
	const auto resolved = impl_->input_context == InputContext::gameplay &&
		!impl_->gameplay_reliable_warmup ?
		impl_->timeline->resolve_next() :
		impl_->timeline->resolve_next_exact();
	if(!resolved) {
		if(resolved.error().code ==
			NewClassicNetplayTimelineErrorCode::prediction_limit) {
			if(prior_status !=
				NewClassicNetplayMainSessionStatus::prediction_stalled) {
				++impl_->prediction_stall_events;
			}
			impl_->status =
				NewClassicNetplayMainSessionStatus::prediction_stalled;
			return NewClassicNetplayMainSessionTick{
				.status = impl_->status,
				.confirmed_frame_count =
					impl_->timeline->confirmed_frame_count(),
			};
		}
		if(resolved.error().code ==
			NewClassicNetplayTimelineErrorCode::remote_input_unavailable) {
			if(prior_status !=
				NewClassicNetplayMainSessionStatus::remote_waiting) {
				++impl_->remote_wait_events;
			}
			impl_->status =
				NewClassicNetplayMainSessionStatus::remote_waiting;
			return NewClassicNetplayMainSessionTick{
				.status = impl_->status,
				.confirmed_frame_count =
					impl_->timeline->confirmed_frame_count(),
			};
		}
		return std::unexpected(timeline_error(resolved.error()));
	}
	const auto stepped = impl_->execute(*resolved);
	if(!stepped) {
		return std::unexpected(stepped.error());
	}
	if(const auto captured = impl_->capture_confirmed_digests(); !captured) {
		return std::unexpected(captured.error());
	}
	impl_->maximum_prediction_depth = std::max(
		impl_->maximum_prediction_depth, resolved->prediction_depth
	);
	return NewClassicNetplayMainSessionTick{
		.status = impl_->status,
		.frame = resolved->frame,
		.boundary = impl_->boundary,
		.confirmed_frame_count =
			impl_->timeline->confirmed_frame_count(),
		.prediction_depth = resolved->prediction_depth,
		.remote_predicted = resolved->remote_predicted,
		.surface_publishable = stepped->surface_publishable,
	};
}

std::expected<
	NewClassicNetplayMainRemoteResult,
	NewClassicNetplayMainSessionError
> NewClassicNetplayMainSession::submit_remote(
	std::uint64_t frame,
	NewClassicNetplayOwnedInput input
) noexcept
{
	if(impl_->status == NewClassicNetplayMainSessionStatus::paused ||
		impl_->status ==
		NewClassicNetplayMainSessionStatus::boundary_confirmed ||
		impl_->status == NewClassicNetplayMainSessionStatus::complete) {
		return std::unexpected(session_error(
			NewClassicNetplayMainSessionErrorCode::not_running
		));
	}
	const auto submitted = impl_->timeline->submit_remote(frame, input);
	if(!submitted) {
		return std::unexpected(timeline_error(submitted.error()));
	}
	NewClassicNetplayMainRemoteResult result{
		.status = impl_->status,
		.rollback_frame = submitted->rollback_frame,
		.confirmed_frame_count = submitted->confirmed_frame_count,
	};
	if(submitted->rollback_frame) {
		if(impl_->input_context != InputContext::gameplay) {
			return std::unexpected(session_error(
				NewClassicNetplayMainSessionErrorCode::invalid_boundary
			));
		}
		const auto target = impl_->timeline->next_frame();
		const auto restored = impl_->coordinator->restore_saved_frame(
			*submitted->rollback_frame
		);
		if(!restored) {
			return std::unexpected(rollback_error(restored.error()));
		}
		const auto rewound = impl_->timeline->rewind_to_rollback();
		if(!rewound) {
			return std::unexpected(timeline_error(rewound.error()));
		}
		impl_->boundary_frame.reset();
		impl_->boundary.reset();
		impl_->status = NewClassicNetplayMainSessionStatus::running;
		while(impl_->timeline->next_frame() < target) {
			const auto resolved = impl_->timeline->resolve_next();
			if(!resolved) {
				return std::unexpected(timeline_error(resolved.error()));
			}
			const auto stepped = impl_->execute(*resolved);
			if(!stepped) {
				return std::unexpected(stepped.error());
			}
			++result.resimulated_frames;
			if(impl_->boundary_frame && impl_->boundary == StepStatus::paused) {
				const auto discarded =
					impl_->timeline->discard_unconfirmed_suffix();
				if(!discarded) {
					return std::unexpected(timeline_error(discarded.error()));
				}
				break;
			}
			if(impl_->boundary_frame &&
				impl_->timeline->next_frame() < target) {
				return std::unexpected(session_error(
					NewClassicNetplayMainSessionErrorCode::
						resimulation_left_gameplay
				));
			}
		}
		result.surface_publishable = result.resimulated_frames != 0U;
		impl_->resimulated_frames += result.resimulated_frames;
	}
	impl_->update_boundary_confirmation();
	if(!impl_->boundary_frame &&
		(impl_->status ==
			NewClassicNetplayMainSessionStatus::prediction_stalled ||
		 impl_->status == NewClassicNetplayMainSessionStatus::remote_waiting)) {
		impl_->status = NewClassicNetplayMainSessionStatus::running;
	}
	result.status = impl_->status;
	result.boundary = impl_->boundary;
	result.confirmed_frame_count =
		impl_->timeline->confirmed_frame_count();
	if(const auto captured = impl_->capture_confirmed_digests(); !captured) {
		return std::unexpected(captured.error());
	}
	return result;
}

std::expected<
	NewClassicNetplayMainSessionTick,
	NewClassicNetplayMainSessionError
> NewClassicNetplayMainSession::continue_confirmed_boundary() noexcept
{
	if(impl_->status !=
		NewClassicNetplayMainSessionStatus::boundary_confirmed ||
		!impl_->boundary || !impl_->boundary_frame) {
		return std::unexpected(session_error(
			NewClassicNetplayMainSessionErrorCode::invalid_boundary
		));
	}
	const auto completed_frame = impl_->boundary_frame;
	const auto completed = *impl_->boundary;
	switch(completed) {
	case StepStatus::phase_transition:
		if(impl_->input_context == InputContext::gameplay) {
			impl_->input_delay->reset();
			impl_->input_context = InputContext::interstitial;
			impl_->warning_samples_remaining = 27U;
		} else if(impl_->input_context == InputContext::interstitial) {
			if(impl_->warning_samples_remaining != 0U) {
				return std::unexpected(session_error(
					NewClassicNetplayMainSessionErrorCode::invalid_boundary
				));
			}
			impl_->input_context = InputContext::gameplay;
			impl_->input_delay->reset();
			impl_->gameplay_reliable_warmup = true;
		} else {
			return std::unexpected(session_error(
				NewClassicNetplayMainSessionErrorCode::invalid_boundary
			));
		}
		impl_->status = NewClassicNetplayMainSessionStatus::running;
		break;
	case StepStatus::round_transition: {
		if((impl_->input_context != InputContext::gameplay &&
			impl_->input_context != InputContext::interstitial) ||
			impl_->warning_samples_remaining != 0U) {
			return std::unexpected(session_error(
				NewClassicNetplayMainSessionErrorCode::invalid_boundary
			));
		}
		const auto started = impl_->coordinator->start_next_round();
		if(!started) {
			return std::unexpected(rollback_error(started.error()));
		}
		impl_->input_context = InputContext::gameplay;
		impl_->input_delay->reset();
		impl_->gameplay_reliable_warmup = true;
		impl_->status = NewClassicNetplayMainSessionStatus::running;
		break;
	}
	case StepStatus::terminal: {
		if((impl_->input_context != InputContext::gameplay &&
			impl_->input_context != InputContext::interstitial) ||
			impl_->warning_samples_remaining != 0U) {
			return std::unexpected(session_error(
				NewClassicNetplayMainSessionErrorCode::invalid_boundary
			));
		}
		const auto finished = impl_->coordinator->finish_main_process();
		if(!finished) {
			return std::unexpected(rollback_error(finished.error()));
		}
		auto transition = impl_->coordinator->session_transition();
		if(!transition) {
			return std::unexpected(rollback_error(transition.error()));
		}
		impl_->transition = std::move(*transition);
		impl_->status = NewClassicNetplayMainSessionStatus::complete;
		break;
	}
	case StepStatus::paused:
		if(impl_->input_context != InputContext::gameplay) {
			return std::unexpected(session_error(
				NewClassicNetplayMainSessionErrorCode::invalid_boundary
			));
		}
		impl_->status = NewClassicNetplayMainSessionStatus::paused;
		impl_->input_delay->reset();
		break;
	case StepStatus::running:
	case StepStatus::invalid_input_context:
	case StepStatus::simulation_not_implemented:
		return std::unexpected(session_error(
			NewClassicNetplayMainSessionErrorCode::invalid_boundary
		));
	}
	if(!impl_->append_audio(*completed_frame)) {
		return std::unexpected(session_error(
			NewClassicNetplayMainSessionErrorCode::audio_journal_failed
		));
	}
	impl_->boundary_frame.reset();
	impl_->boundary.reset();
	impl_->hold_samples_remaining = impl_->pending_hold_samples;
	impl_->pending_hold_samples = 0U;
	return NewClassicNetplayMainSessionTick{
		.status = impl_->status,
		.frame = completed_frame,
		.boundary = completed,
		.confirmed_frame_count = impl_->timeline->confirmed_frame_count(),
		.surface_publishable = true,
	};
}

std::expected<
	NewClassicNetplayMainSessionTick,
	NewClassicNetplayMainSessionError
> NewClassicNetplayMainSession::resume_pause() noexcept
{
	if(impl_->status != NewClassicNetplayMainSessionStatus::paused ||
		impl_->boundary_frame || impl_->boundary ||
		impl_->input_context != InputContext::gameplay) {
		return std::unexpected(session_error(
			NewClassicNetplayMainSessionErrorCode::not_running
		));
	}
	impl_->input_delay->reset();
	constexpr std::array<CoreInput, 3U> canonical_resume{{
		{},
		{.system = input_cancel},
		{},
	}};
	StepResult completed{};
	for(std::size_t index = 0U; index < canonical_resume.size(); ++index) {
		auto stepped = impl_->coordinator->advance_pause(
			canonical_resume[index]
		);
		if(!stepped) {
			return std::unexpected(rollback_error(stepped.error()));
		}
		const bool final = index + 1U == canonical_resume.size();
		if((!final && stepped->status != StepStatus::paused) ||
			(final && stepped->status == StepStatus::paused)) {
			return std::unexpected(session_error(
				NewClassicNetplayMainSessionErrorCode::pause_failed
			));
		}
		completed = *stepped;
	}
	impl_->pause_audio_head = 0U;
	impl_->pause_audio_count = 0U;
	if(!impl_->drain_audio(
		impl_->pause_audio, impl_->pause_audio_count
	)) {
		return std::unexpected(session_error(
			NewClassicNetplayMainSessionErrorCode::audio_journal_failed
		));
	}
	const auto completed_frame = impl_->timeline->next_frame() - 1U;
	if(completed.status == StepStatus::running) {
		const auto reset = impl_->coordinator->try_reset_rollback_boundary();
		if(!reset || !*reset) {
			return std::unexpected(reset ? session_error(
				NewClassicNetplayMainSessionErrorCode::pause_failed
			) : rollback_error(reset.error()));
		}
		impl_->status = NewClassicNetplayMainSessionStatus::running;
	} else {
		const auto proof = new_classic_netplay_boundary_proof(completed.status);
		if(proof == NewClassicNetplayBoundaryProof::none) {
			return std::unexpected(session_error(
				NewClassicNetplayMainSessionErrorCode::invalid_boundary
			));
		}
		impl_->boundary_frame = completed_frame;
		impl_->boundary = completed.status;
		impl_->status =
			NewClassicNetplayMainSessionStatus::boundary_waiting_confirmation;
		if(const auto queued = impl_->enqueue_confirmed_digest(
			impl_->timeline->next_frame(),
			NewClassicNetplaySimulationDigestKind::boundary, proof
		); !queued) {
			return std::unexpected(queued.error());
		}
		impl_->update_boundary_confirmation();
	}
	impl_->hold_samples_remaining = completed.next_step_vsyncs > 1U ?
		static_cast<std::uint8_t>(completed.next_step_vsyncs - 1U) : 0U;
	impl_->pending_hold_samples = 0U;
	return NewClassicNetplayMainSessionTick{
		.status = impl_->status,
		.frame = completed_frame,
		.boundary = completed.status == StepStatus::running ?
			std::nullopt : std::optional{completed.status},
		.pause_completion = completed,
		.confirmed_frame_count = impl_->timeline->confirmed_frame_count(),
		.surface_publishable = true,
	};
}

std::expected<present::SourceSurface, present::SourceSurfaceError>
NewClassicNetplayMainSession::compose_shown_surface(
	std::span<present::ComposedPixel, present::graphics_page_pixels> output
) const
{
	return impl_->coordinator->compose_shown_surface(output);
}

std::optional<ClassicAudioCommand>
NewClassicNetplayMainSession::take_audio_command() noexcept
{
	if(impl_->bootstrap_audio_head < impl_->bootstrap_audio_count) {
		return impl_->bootstrap_audio[impl_->bootstrap_audio_head++];
	}
	if(impl_->pause_audio_head < impl_->pause_audio_count) {
		return impl_->pause_audio[impl_->pause_audio_head++];
	}
	return std::nullopt;
}

std::expected<
	std::optional<NewClassicNetplayConfirmedAudioFrame>,
	NewClassicNetplayMainSessionError
> NewClassicNetplayMainSession::take_confirmed_audio_frame() noexcept
{
	if(impl_->next_audio_frame >= impl_->timeline->confirmed_frame_count() ||
		(impl_->boundary_frame &&
		 impl_->next_audio_frame == *impl_->boundary_frame)) {
		return std::optional<NewClassicNetplayConfirmedAudioFrame>{};
	}
	auto &slot = impl_->audio_slots[
		impl_->next_audio_frame % impl_->audio_slots.size()
	];
	if(!slot.valid || slot.frame != impl_->next_audio_frame ||
		slot.command_count > slot.commands.size() || slot.source_vsyncs == 0U) {
		return std::unexpected(session_error(
			NewClassicNetplayMainSessionErrorCode::audio_journal_failed
		));
	}
	NewClassicNetplayConfirmedAudioFrame frame{
		.frame = slot.frame,
		.commands = slot.commands,
		.command_count = slot.command_count,
		.source_vsyncs = slot.source_vsyncs,
	};
	slot.valid = false;
	++impl_->next_audio_frame;
	return std::optional{frame};
}

std::expected<
	std::optional<NewClassicNetplayConfirmedSimulationDigest>,
	NewClassicNetplayMainSessionError
> NewClassicNetplayMainSession::take_confirmed_simulation_digest() noexcept
{
	if(impl_->digest_count == 0U) {
		return std::optional<
			NewClassicNetplayConfirmedSimulationDigest
		>{};
	}
	auto result = impl_->digest_queue[impl_->digest_head];
	impl_->digest_head = static_cast<std::uint8_t>(
		(impl_->digest_head + 1U) % impl_->digest_queue.size()
	);
	--impl_->digest_count;
	return std::optional{result};
}

std::expected<
	std::optional<NewClassicNetplayConfirmedInputFrame>,
	NewClassicNetplayMainSessionError
> NewClassicNetplayMainSession::take_confirmed_input_frame() noexcept
{
	if(impl_->next_transcript_frame >=
		impl_->timeline->confirmed_frame_count() ||
		(impl_->boundary_frame &&
		 impl_->next_transcript_frame == *impl_->boundary_frame)) {
		return std::optional<NewClassicNetplayConfirmedInputFrame>{};
	}
	const auto &fact = impl_->frame_facts[
		impl_->next_transcript_frame % impl_->frame_facts.size()
	];
	if(!fact.valid || fact.frame != impl_->next_transcript_frame ||
		fact.source_vsyncs == 0U) {
		return std::unexpected(session_error(
			NewClassicNetplayMainSessionErrorCode::transcript_journal_failed
		));
	}
	NewClassicNetplayConfirmedInputFrame result{
		.frame = fact.frame,
		.phase = fact.gameplay ? replay::InputPhase::gameplay :
			replay::InputPhase::interstitial,
		.input = fact.input,
		.step_result = fact.step_result,
		.source_vsyncs = fact.source_vsyncs,
	};
	++impl_->next_transcript_frame;
	return std::optional{result};
}

const MainProcess &NewClassicNetplayMainSession::process() const noexcept
{
	return impl_->coordinator->process();
}

std::uint64_t NewClassicNetplayMainSession::next_frame() const noexcept
{
	return impl_->timeline->next_frame();
}

std::uint64_t
NewClassicNetplayMainSession::confirmed_frame_count() const noexcept
{
	return impl_->timeline->confirmed_frame_count();
}

NewClassicNetplayMainSessionStats
NewClassicNetplayMainSession::stats() const noexcept
{
	const auto timeline = impl_->timeline->stats();
	return {
		.resolved_frames = timeline.resolved_frames,
		.predicted_frames = timeline.predicted_frames,
		.rollback_requests = timeline.rollback_requests,
		.rewinds = timeline.rewinds,
		.resimulated_frames = impl_->resimulated_frames,
		.prediction_stall_events = impl_->prediction_stall_events,
		.remote_wait_events = impl_->remote_wait_events,
		.maximum_prediction_depth = impl_->maximum_prediction_depth,
	};
}

NewClassicNetplayMainSessionStatus
NewClassicNetplayMainSession::status() const noexcept
{
	return impl_->status;
}

std::optional<StepStatus>
NewClassicNetplayMainSession::boundary() const noexcept
{
	return impl_->boundary;
}

InputContext NewClassicNetplayMainSession::input_context() const noexcept
{
	return impl_->input_context;
}

const std::optional<SessionTransition> &
NewClassicNetplayMainSession::transition() const noexcept
{
	return impl_->transition;
}

std::expected<AppSha256, NewClassicNetplayMainSessionError>
NewClassicNetplayMainSession::simulation_digest(std::uint64_t frame) noexcept
{
	const auto digest = impl_->coordinator->simulation_digest(frame);
	if(!digest) {
		return std::unexpected(rollback_error(digest.error()));
	}
	return *digest;
}

std::optional<NewClassicNetplayOwnedInput>
NewClassicNetplayMainSession::local_input(std::uint64_t frame) const noexcept
{
	return impl_->timeline->local_input(frame);
}

} // namespace th03::app
