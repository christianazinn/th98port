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
	std::uint64_t next_frame = 0U;
	MainRollbackCoordinatorStats stats{};

	explicit Impl(std::unique_ptr<MainProcess> process_) :
		process(std::move(process_)),
		ring(std::make_unique<
			std::array<Slot, main_rollback_ring_slots>
		>()),
		recovery(std::make_unique<MainProcessRollbackSnapshot>())
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
	step(CoreInput input) noexcept
	{
		auto stepped = process->step(input, InputContext::gameplay);
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
