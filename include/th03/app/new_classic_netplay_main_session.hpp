#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_MAIN_SESSION_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_MAIN_SESSION_HPP

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "th03/app/main_rollback_coordinator.hpp"
#include "th03/app/new_classic_netplay_timeline.hpp"

namespace th03::app {

enum class NewClassicNetplayMainSessionStatus : std::uint8_t {
	running,
	prediction_stalled,
	remote_waiting,
	boundary_waiting_confirmation,
	boundary_confirmed,
	complete,
};

enum class NewClassicNetplayMainSessionErrorCode : std::uint8_t {
	invalid_process,
	timeline_failed,
	rollback_failed,
	not_running,
	resimulation_left_gameplay,
	invalid_boundary,
};

struct NewClassicNetplayMainSessionError final {
	NewClassicNetplayMainSessionErrorCode code =
		NewClassicNetplayMainSessionErrorCode::invalid_process;
	std::optional<NewClassicNetplayTimelineError> timeline{};
	std::optional<MainRollbackCoordinatorError> rollback{};
};

struct NewClassicNetplayMainSessionTick final {
	NewClassicNetplayMainSessionStatus status =
		NewClassicNetplayMainSessionStatus::running;
	std::optional<std::uint64_t> frame{};
	std::optional<StepStatus> boundary{};
	std::uint64_t confirmed_frame_count = 0U;
	std::uint8_t prediction_depth = 0U;
	bool remote_predicted = false;
	bool surface_publishable = false;
};

struct NewClassicNetplayMainRemoteResult final {
	NewClassicNetplayMainSessionStatus status =
		NewClassicNetplayMainSessionStatus::running;
	std::optional<std::uint64_t> rollback_frame{};
	std::optional<StepStatus> boundary{};
	std::uint64_t confirmed_frame_count = 0U;
	std::uint64_t resimulated_frames = 0U;
	bool surface_publishable = false;
};

// Retained active-MAIN owner. Network values enter only through T3NET001's
// player-local value. A late mismatch restores the complete T3RBM001 image,
// recomputes the affected suffix synchronously, and publishes only the newest
// corrected surface. A non-gameplay boundary remains private until the input
// that produced it is contiguous-confirmed.
class NewClassicNetplayMainSession final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<NewClassicNetplayMainSession>,
		NewClassicNetplayMainSessionError
	> create(
		NewClassicNetplayPlayer local_player,
		std::unique_ptr<MainProcess> process
	);

	~NewClassicNetplayMainSession();
	NewClassicNetplayMainSession(
		const NewClassicNetplayMainSession &
	) = delete;
	NewClassicNetplayMainSession &operator=(
		const NewClassicNetplayMainSession &
	) = delete;
	NewClassicNetplayMainSession(NewClassicNetplayMainSession &&) = delete;
	NewClassicNetplayMainSession &operator=(
		NewClassicNetplayMainSession &&
	) = delete;

	[[nodiscard]] std::expected<
		NewClassicNetplayMainSessionTick,
		NewClassicNetplayMainSessionError
	> advance_local(NewClassicNetplayOwnedInput input) noexcept;

	[[nodiscard]] std::expected<
		NewClassicNetplayMainRemoteResult,
		NewClassicNetplayMainSessionError
	> submit_remote(
		std::uint64_t frame,
		NewClassicNetplayOwnedInput input
	) noexcept;

	// Applies one already-confirmed process boundary. Phase/result frames then
	// run exact-input lockstep; the next gameplay domain reopens rollback only
	// after its complete source startup transaction.
	[[nodiscard]] std::expected<
		NewClassicNetplayMainSessionTick,
		NewClassicNetplayMainSessionError
	> continue_confirmed_boundary() noexcept;

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
	[[nodiscard]] std::uint64_t confirmed_frame_count() const noexcept;
	[[nodiscard]] NewClassicNetplayMainSessionStatus status() const noexcept;
	[[nodiscard]] std::optional<StepStatus> boundary() const noexcept;
	[[nodiscard]] InputContext input_context() const noexcept;
	[[nodiscard]] const std::optional<SessionTransition> &transition()
		const noexcept;
	[[nodiscard]] std::optional<NewClassicNetplayOwnedInput>
	local_input(std::uint64_t frame) const noexcept;

private:
	struct Impl;
	explicit NewClassicNetplayMainSession(
		std::unique_ptr<Impl> impl
	) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif
