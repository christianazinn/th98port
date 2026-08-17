#include "th03/app/main_rollback_coordinator.hpp"

#include <array>
#include <utility>

namespace th03::app {
namespace {

[[nodiscard]] MainRollbackCoordinatorError coordinator_error(
	MainRollbackCoordinatorErrorCode code
) noexcept
{
	return MainRollbackCoordinatorError{.code = code};
}

} // namespace

struct MainRollbackCoordinator::Impl final {
	struct Slot final {
		std::uint64_t frame = 0U;
		bool valid = false;
		MainProcessRollbackSnapshot snapshot{};
	};

	std::unique_ptr<MainProcess> process{};
	std::unique_ptr<std::array<Slot, main_rollback_ring_slots>> ring{};
	std::unique_ptr<MainProcessRollbackSnapshot> recovery{};
	std::unique_ptr<RollbackSimulationSnapshot> digest_scratch{};
	std::uint64_t next_frame = 0U;
	MainRollbackCoordinatorStats stats{};

	explicit Impl(std::unique_ptr<MainProcess> process_) :
		process(std::move(process_)),
		ring(std::make_unique<
			std::array<Slot, main_rollback_ring_slots>
		>()),
		recovery(std::make_unique<MainProcessRollbackSnapshot>()),
		digest_scratch(std::make_unique<RollbackSimulationSnapshot>())
	{}

	[[nodiscard]] std::expected<Slot *, MainRollbackCoordinatorError>
	capture_current() noexcept
	{
		auto &slot = (*ring)[next_frame % ring->size()];
		slot.valid = false;
		const auto captured = process->capture_rollback(slot.snapshot);
		if(!captured) {
			return std::unexpected(MainRollbackCoordinatorError{
				.code = MainRollbackCoordinatorErrorCode::boundary_unavailable,
				.rollback = captured.error(),
			});
		}
		slot.frame = next_frame;
		slot.valid = true;
		return &slot;
	}

	[[nodiscard]] std::expected<void, MainRollbackCoordinatorError>
	restore(const Slot &slot) noexcept
	{
		const auto restored = process->restore_rollback(
			slot.snapshot, *recovery
		);
		if(!restored) {
			return std::unexpected(MainRollbackCoordinatorError{
				.code = MainRollbackCoordinatorErrorCode::restore_failed,
				.rollback = restored.error(),
			});
		}
		++stats.restores;
		return {};
	}

	[[nodiscard]] std::expected<StepResult, MainRollbackCoordinatorError>
	step(CoreInput input, InputContext context = InputContext::gameplay) noexcept
	{
		auto stepped = process->step(input, context);
		if(!stepped) {
			return std::unexpected(MainRollbackCoordinatorError{
				.code = MainRollbackCoordinatorErrorCode::process_failed,
				.process = std::move(stepped.error()),
			});
		}
		return *stepped;
	}
};

std::expected<
	std::unique_ptr<MainRollbackCoordinator>,
	MainRollbackCoordinatorError
> MainRollbackCoordinator::create(std::unique_ptr<MainProcess> process)
{
	if(!process) {
		return std::unexpected(coordinator_error(
			MainRollbackCoordinatorErrorCode::invalid_process
		));
	}
	if(process->input_protocol() != InputProtocol::live) {
		return std::unexpected(coordinator_error(
			MainRollbackCoordinatorErrorCode::unsupported_input_protocol
		));
	}
	auto impl = std::make_unique<Impl>(std::move(process));
	if(const auto initial = impl->capture_current(); !initial) {
		return std::unexpected(initial.error());
	}
	return std::unique_ptr<MainRollbackCoordinator>(
		new MainRollbackCoordinator(std::move(impl))
	);
}

std::expected<MainRollbackCoordinatorTick, MainRollbackCoordinatorError>
MainRollbackCoordinator::advance_reliable(
	CoreInput input,
	InputContext context
) noexcept
{
	if(context == InputContext::pause) {
		return std::unexpected(coordinator_error(
			MainRollbackCoordinatorErrorCode::left_gameplay_domain
		));
	}
	auto stepped = impl_->step(input, context);
	if(!stepped) {
		return std::unexpected(stepped.error());
	}
	const auto frame = impl_->next_frame++;
	++impl_->stats.frames;
	return MainRollbackCoordinatorTick{
		.frame = frame,
		.step_result = *stepped,
		.process_executions = 1U,
		.surface_publishable = true,
		.audio_publishable = true,
	};
}

MainRollbackCoordinator::MainRollbackCoordinator(
	std::unique_ptr<Impl> impl
) noexcept :
	impl_(std::move(impl))
{}

MainRollbackCoordinator::~MainRollbackCoordinator() = default;

std::expected<MainRollbackCoordinatorTick, MainRollbackCoordinatorError>
MainRollbackCoordinator::advance_confirmed(CoreInput input) noexcept
{
	auto saved = impl_->capture_current();
	if(!saved) {
		return std::unexpected(saved.error());
	}
	auto stepped = impl_->step(input);
	if(!stepped) {
		const auto restored = impl_->restore(**saved);
		return std::unexpected(restored ? stepped.error() : restored.error());
	}
	const auto frame = impl_->next_frame++;
	++impl_->stats.frames;
	return MainRollbackCoordinatorTick{
		.frame = frame,
		.step_result = *stepped,
		.process_executions = 1U,
		.surface_publishable = true,
		.audio_publishable = true,
	};
}

std::expected<MainRollbackCoordinatorTick, MainRollbackCoordinatorError>
MainRollbackCoordinator::advance_predicted_then_corrected(
	CoreInput predicted,
	CoreInput corrected
) noexcept
{
	auto saved = impl_->capture_current();
	if(!saved) {
		return std::unexpected(saved.error());
	}
	const auto speculative = impl_->step(predicted);
	if(!speculative) {
		const auto restored = impl_->restore(**saved);
		return std::unexpected(restored ? speculative.error() : restored.error());
	}
	++impl_->stats.speculative_executions;
	if(const auto restored = impl_->restore(**saved); !restored) {
		return std::unexpected(restored.error());
	}
	auto stepped = impl_->step(corrected);
	if(!stepped) {
		const auto restored = impl_->restore(**saved);
		return std::unexpected(restored ? stepped.error() : restored.error());
	}
	const auto frame = impl_->next_frame++;
	++impl_->stats.frames;
	return MainRollbackCoordinatorTick{
		.frame = frame,
		.step_result = *stepped,
		.process_executions = 2U,
		.surface_publishable = true,
		.audio_publishable = true,
	};
}

std::expected<void, MainRollbackCoordinatorError>
MainRollbackCoordinator::restore_saved_frame(std::uint64_t frame) noexcept
{
	const auto &slot = (*impl_->ring)[frame % impl_->ring->size()];
	if(!slot.valid || slot.frame != frame) {
		return std::unexpected(coordinator_error(
			MainRollbackCoordinatorErrorCode::frame_unavailable
		));
	}
	if(const auto restored = impl_->restore(slot); !restored) {
		return restored;
	}
	for(auto &candidate : *impl_->ring) {
		if(candidate.valid && candidate.frame > frame) {
			candidate.valid = false;
		}
	}
	impl_->next_frame = frame;
	return {};
}

std::expected<bool, MainRollbackCoordinatorError>
MainRollbackCoordinator::try_reset_rollback_boundary() noexcept
{
	for(auto &slot : *impl_->ring) {
		slot.valid = false;
	}
	const auto captured = impl_->capture_current();
	if(!captured) {
		if(captured.error().rollback &&
			captured.error().rollback->code ==
				MainProcessRollbackErrorCode::boundary_unavailable) {
			return false;
		}
		return std::unexpected(captured.error());
	}
	return true;
}

bool MainRollbackCoordinator::rollback_boundary_available() const noexcept
{
	return impl_->process->rollback_boundary_available();
}

std::expected<void, MainRollbackCoordinatorError>
MainRollbackCoordinator::start_next_round() noexcept
{
	const auto started = impl_->process->start_next_round();
	if(!started) {
		return std::unexpected(MainRollbackCoordinatorError{
			.code = MainRollbackCoordinatorErrorCode::process_failed,
			.process = std::move(started.error()),
		});
	}
	return {};
}

std::expected<void, MainRollbackCoordinatorError>
MainRollbackCoordinator::finish_main_process() noexcept
{
	const auto finished = impl_->process->finish_main_process();
	if(!finished) {
		return std::unexpected(MainRollbackCoordinatorError{
			.code = MainRollbackCoordinatorErrorCode::process_failed,
			.process = std::move(finished.error()),
		});
	}
	return {};
}

std::expected<SessionTransition, MainRollbackCoordinatorError>
MainRollbackCoordinator::session_transition() const noexcept
{
	const auto transition = impl_->process->session_transition();
	if(!transition) {
		return std::unexpected(MainRollbackCoordinatorError{
			.code = MainRollbackCoordinatorErrorCode::process_failed,
			.process = std::move(transition.error()),
		});
	}
	return *transition;
}

std::expected<AppSha256, MainRollbackCoordinatorError>
MainRollbackCoordinator::simulation_digest(std::uint64_t frame) noexcept
{
	const RollbackSimulationSnapshot *simulation = nullptr;
	const auto &slot = (*impl_->ring)[frame % impl_->ring->size()];
	if(slot.valid && slot.frame == frame) {
		simulation = &slot.snapshot.simulation;
	} else if(frame == impl_->next_frame) {
		const auto captured = impl_->process->capture_rollback_simulation();
		if(!captured) {
			return std::unexpected(MainRollbackCoordinatorError{
				.code = MainRollbackCoordinatorErrorCode::boundary_unavailable,
				.rollback = captured.error(),
			});
		}
		*impl_->digest_scratch = *captured;
		simulation = impl_->digest_scratch.get();
	} else {
		return std::unexpected(coordinator_error(
			MainRollbackCoordinatorErrorCode::frame_unavailable
		));
	}
	if(simulation->used_size == 0U ||
		simulation->used_size > simulation->bytes.size()) {
		return std::unexpected(coordinator_error(
			MainRollbackCoordinatorErrorCode::frame_unavailable
		));
	}
	return app_sha256(std::span<const std::byte>(
		simulation->bytes.data(), simulation->used_size
	));
}

std::expected<present::SourceSurface, present::SourceSurfaceError>
MainRollbackCoordinator::compose_shown_surface(
	std::span<present::ComposedPixel, present::graphics_page_pixels> output
) const
{
	return impl_->process->compose_shown_surface(output);
}

std::optional<ClassicAudioCommand>
MainRollbackCoordinator::take_audio_command() noexcept
{
	return impl_->process->take_audio_command();
}

const MainProcess &MainRollbackCoordinator::process() const noexcept
{
	return *impl_->process;
}

std::uint64_t MainRollbackCoordinator::next_frame() const noexcept
{
	return impl_->next_frame;
}

MainRollbackCoordinatorStats MainRollbackCoordinator::stats() const noexcept
{
	return impl_->stats;
}

} // namespace th03::app
