#ifndef TH03_APP_MAIN_ROLLBACK_COORDINATOR_HPP
#define TH03_APP_MAIN_ROLLBACK_COORDINATOR_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "th03/app/main_process.hpp"

namespace th03::app {

inline constexpr std::size_t main_rollback_ring_slots = 16U;

enum class MainRollbackCoordinatorErrorCode : std::uint8_t {
	invalid_process,
	unsupported_input_protocol,
	boundary_unavailable,
	frame_unavailable,
	process_failed,
	restore_failed,
	left_gameplay_domain,
};

struct MainRollbackCoordinatorError final {
	MainRollbackCoordinatorErrorCode code =
		MainRollbackCoordinatorErrorCode::invalid_process;
	std::optional<MainProcessError> process{};
	std::optional<MainProcessRollbackError> rollback{};
	StepStatus step_status = StepStatus::running;
};

struct MainRollbackCoordinatorTick final {
	std::uint64_t frame = 0U;
	StepResult step_result{};
	// One for an uninterrupted/confirmed advance; two when a private prediction
	// was discarded and the corrected frame was executed from the saved image.
	std::uint8_t process_executions = 0U;
	bool surface_publishable = false;
	bool audio_publishable = false;
};

struct MainRollbackCoordinatorStats final {
	std::uint64_t frames = 0U;
	std::uint64_t speculative_executions = 0U;
	std::uint64_t restores = 0U;
};

// Phase-N0 local owner for active ordinary MAIN gameplay. It deliberately
// performs no network prediction policy: callers either submit one confirmed
// input or one private prediction plus its corrected input. The private step
// cannot be composed and its semantic audio remains inside the restorable
// process image. The fixed ring is the later Save/Load boundary for middleware.
class MainRollbackCoordinator final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainRollbackCoordinator>,
		MainRollbackCoordinatorError
	> create(std::unique_ptr<MainProcess> process);

	~MainRollbackCoordinator();
	MainRollbackCoordinator(const MainRollbackCoordinator &) = delete;
	MainRollbackCoordinator &operator=(const MainRollbackCoordinator &) = delete;
	MainRollbackCoordinator(MainRollbackCoordinator &&) = delete;
	MainRollbackCoordinator &operator=(MainRollbackCoordinator &&) = delete;

	[[nodiscard]] std::expected<
		MainRollbackCoordinatorTick,
		MainRollbackCoordinatorError
	> advance_confirmed(CoreInput input) noexcept;
	// Confirmed non-gameplay MAIN phases are never predicted. They still share
	// the process/frame owner so both peers reach the same next rollback domain.
	[[nodiscard]] std::expected<
		MainRollbackCoordinatorTick,
		MainRollbackCoordinatorError
	> advance_reliable(CoreInput input, InputContext context) noexcept;
	[[nodiscard]] std::expected<
		MainRollbackCoordinatorTick,
		MainRollbackCoordinatorError
	> advance_predicted_then_corrected(
		CoreInput predicted,
		CoreInput corrected
	) noexcept;

	// Restore a retained pre-frame image. Later history is invalidated.
	// This is a local trusted-frame lookup, never a peer-supplied state load.
	[[nodiscard]] std::expected<void, MainRollbackCoordinatorError>
	restore_saved_frame(std::uint64_t frame) noexcept;
	// Drop history from the preceding gameplay domain and capture the current
	// process image as the first boundary of the next rollback domain.
	[[nodiscard]] std::expected<bool, MainRollbackCoordinatorError>
	try_reset_rollback_boundary() noexcept;
	[[nodiscard]] bool rollback_boundary_available() const noexcept;
	[[nodiscard]] std::expected<void, MainRollbackCoordinatorError>
	start_next_round() noexcept;
	[[nodiscard]] std::expected<void, MainRollbackCoordinatorError>
	finish_main_process() noexcept;
	[[nodiscard]] std::expected<
		SessionTransition,
		MainRollbackCoordinatorError
	> session_transition() const noexcept;
	// SHA-256 of the active T3RBK001 simulation prefix at one retained
	// pre-frame boundary. This never hashes C++ object bytes, pointers,
	// presentation state, or host state and never accepts remote bytes.
	[[nodiscard]] std::expected<AppSha256, MainRollbackCoordinatorError>
	simulation_digest(std::uint64_t frame) noexcept;

	[[nodiscard]] std::expected<
		present::SourceSurface,
		present::SourceSurfaceError
	> compose_shown_surface(
		std::span<
			present::ComposedPixel,
			present::graphics_page_pixels
		> output
	) const;
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] const MainProcess &process() const noexcept;
	[[nodiscard]] std::uint64_t next_frame() const noexcept;
	[[nodiscard]] MainRollbackCoordinatorStats stats() const noexcept;

private:
	struct Impl;
	explicit MainRollbackCoordinator(std::unique_ptr<Impl> impl) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif
