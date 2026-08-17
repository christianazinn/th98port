#include "th03/app/new_classic_netplay_main_session.hpp"

#include <utility>

namespace th03::app {
namespace {

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

} // namespace

struct NewClassicNetplayMainSession::Impl final {
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
	std::unique_ptr<MainRollbackCoordinator> coordinator{};
	std::array<AudioSlot, new_classic_netplay_timeline_slots> audio_slots{};
	std::array<
		ClassicAudioCommand,
		new_classic_netplay_audio_commands_per_frame
	> bootstrap_audio{};
	std::uint8_t bootstrap_audio_head = 0U;
	std::uint8_t bootstrap_audio_count = 0U;
	std::uint64_t next_audio_frame = 0U;
	NewClassicNetplayMainSessionStatus status =
		NewClassicNetplayMainSessionStatus::running;
	std::optional<std::uint64_t> boundary_frame{};
	std::optional<StepStatus> boundary{};
	std::optional<SessionTransition> transition{};
	InputContext input_context = InputContext::gameplay;
	std::uint8_t warning_samples_remaining = 0U;
	std::uint8_t hold_samples_remaining = 0U;
	std::uint8_t pending_hold_samples = 0U;
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

	[[nodiscard]] std::expected<
		MainRollbackCoordinatorTick,
		NewClassicNetplayMainSessionError
	> execute(const NewClassicNetplayResolvedFrame &resolved) noexcept
	{
		auto stepped = input_context == InputContext::gameplay &&
			!gameplay_reliable_warmup ?
			coordinator->advance_confirmed(resolved.combined) :
			coordinator->advance_reliable(resolved.combined, input_context);
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
		return *stepped;
	}
};

std::expected<
	std::unique_ptr<NewClassicNetplayMainSession>,
	NewClassicNetplayMainSessionError
> NewClassicNetplayMainSession::create(
	NewClassicNetplayPlayer local_player,
	std::unique_ptr<MainProcess> process
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
	auto coordinator = MainRollbackCoordinator::create(std::move(process));
	if(!coordinator) {
		return std::unexpected(rollback_error(coordinator.error()));
	}
	auto impl = std::make_unique<Impl>();
	impl->timeline = std::move(*timeline);
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
	const auto submitted = impl_->timeline->submit_local(frame, input);
	if(!submitted) {
		return std::unexpected(timeline_error(submitted.error()));
	}
	const auto resolved = impl_->input_context == InputContext::gameplay &&
		!impl_->gameplay_reliable_warmup ?
		impl_->timeline->resolve_next() :
		impl_->timeline->resolve_next_exact();
	if(!resolved) {
		if(resolved.error().code ==
			NewClassicNetplayTimelineErrorCode::prediction_limit) {
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
	if(impl_->status ==
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
			if(impl_->boundary_frame &&
				impl_->timeline->next_frame() < target) {
				return std::unexpected(session_error(
					NewClassicNetplayMainSessionErrorCode::
						resimulation_left_gameplay
				));
			}
		}
		result.surface_publishable = result.resimulated_frames != 0U;
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
			impl_->input_context = InputContext::interstitial;
			impl_->warning_samples_remaining = 27U;
		} else if(impl_->input_context == InputContext::interstitial) {
			if(impl_->warning_samples_remaining != 0U) {
				return std::unexpected(session_error(
					NewClassicNetplayMainSessionErrorCode::invalid_boundary
				));
			}
			impl_->input_context = InputContext::gameplay;
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
	case StepStatus::running:
	case StepStatus::paused:
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
	if(impl_->bootstrap_audio_head >= impl_->bootstrap_audio_count) {
		return std::nullopt;
	}
	return impl_->bootstrap_audio[impl_->bootstrap_audio_head++];
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
