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
	std::unique_ptr<NewClassicNetplayInputTimeline> timeline{};
	std::unique_ptr<MainRollbackCoordinator> coordinator{};
	NewClassicNetplayMainSessionStatus status =
		NewClassicNetplayMainSessionStatus::running;
	std::optional<std::uint64_t> boundary_frame{};
	std::optional<StepStatus> boundary{};

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
		auto stepped = coordinator->advance_confirmed(resolved.combined);
		if(!stepped) {
			return std::unexpected(rollback_error(stepped.error()));
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
	if(impl_->boundary_frame) {
		return std::unexpected(session_error(
			NewClassicNetplayMainSessionErrorCode::not_running
		));
	}
	const auto frame = impl_->timeline->next_frame();
	const auto submitted = impl_->timeline->submit_local(frame, input);
	if(!submitted) {
		return std::unexpected(timeline_error(submitted.error()));
	}
	const auto resolved = impl_->timeline->resolve_next();
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
		NewClassicNetplayMainSessionStatus::boundary_confirmed) {
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
	if(!impl_->boundary_frame && impl_->status ==
		NewClassicNetplayMainSessionStatus::prediction_stalled) {
		impl_->status = NewClassicNetplayMainSessionStatus::running;
	}
	result.status = impl_->status;
	result.boundary = impl_->boundary;
	result.confirmed_frame_count =
		impl_->timeline->confirmed_frame_count();
	return result;
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
	return impl_->coordinator->take_audio_command();
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

} // namespace th03::app
