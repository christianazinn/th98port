#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_MAIN_SESSION_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_MAIN_SESSION_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "th03/app/main_rollback_coordinator.hpp"
#include "th03/app/new_classic_netplay_input_delay.hpp"
#include "th03/app/new_classic_netplay_timeline.hpp"
#include "th03/replay/session.hpp"

namespace th03::app {

enum class NewClassicNetplayMainSessionStatus : std::uint8_t {
	running,
	prediction_stalled,
	remote_waiting,
	boundary_waiting_confirmation,
	boundary_confirmed,
	paused,
	complete,
};

enum class NewClassicNetplayMainSessionErrorCode : std::uint8_t {
	invalid_process,
	timeline_failed,
	rollback_failed,
	not_running,
	resimulation_left_gameplay,
	invalid_boundary,
	audio_journal_failed,
	digest_journal_failed,
	transcript_journal_failed,
	pause_failed,
	input_delay_failed,
};

struct NewClassicNetplayMainSessionError final {
	NewClassicNetplayMainSessionErrorCode code =
		NewClassicNetplayMainSessionErrorCode::invalid_process;
	std::optional<NewClassicNetplayTimelineError> timeline{};
	std::optional<MainRollbackCoordinatorError> rollback{};
	std::optional<NewClassicNetplayInputDelayError> input_delay{};
};

struct NewClassicNetplayMainSessionTick final {
	NewClassicNetplayMainSessionStatus status =
		NewClassicNetplayMainSessionStatus::running;
	std::optional<std::uint64_t> frame{};
	std::optional<StepStatus> boundary{};
	std::optional<StepResult> pause_completion{};
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

// Deterministic local observations for diagnostics and the later source-styled
// post-match report. These counters never participate in simulation, replay
// data, or peer agreement.
struct NewClassicNetplayMainSessionStats final {
	std::uint64_t resolved_frames = 0U;
	std::uint64_t predicted_frames = 0U;
	std::uint64_t rollback_requests = 0U;
	std::uint64_t rewinds = 0U;
	std::uint64_t resimulated_frames = 0U;
	std::uint64_t prediction_stall_events = 0U;
	std::uint64_t remote_wait_events = 0U;
	std::uint8_t maximum_prediction_depth = 0U;
};

inline constexpr std::size_t new_classic_netplay_audio_commands_per_frame = 16U;
inline constexpr std::uint64_t
	new_classic_netplay_simulation_digest_interval = 120U;

// One contiguous-confirmed source frame. Commands are the corrected semantic
// MAIN transaction; `source_vsyncs` advances PMD through the frame's complete
// wait without consulting wall time or the prediction clock.
struct NewClassicNetplayConfirmedAudioFrame final {
	std::uint64_t frame = 0U;
	std::array<
		ClassicAudioCommand,
		new_classic_netplay_audio_commands_per_frame
	> commands{};
	std::uint8_t command_count = 0U;
	std::uint8_t source_vsyncs = 1U;
};

// Canonical corrected simulation state after `frame` contiguous source inputs.
// Periodic proofs count gameplay inputs; lifecycle proofs additionally name
// the phase, round, or terminal boundary whose post-frame state they freeze.
// No proof is derived from a predicted suffix.
struct NewClassicNetplayConfirmedSimulationDigest final {
	std::uint64_t frame = 0U;
	std::uint64_t confirmed_gameplay_frame_count = 0U;
	NewClassicNetplaySimulationDigestKind kind =
		NewClassicNetplaySimulationDigestKind::periodic;
	NewClassicNetplayBoundaryProof boundary =
		NewClassicNetplayBoundaryProof::none;
	AppSha256 digest{};
};

// One corrected frame suitable for the in-memory V13 recorder. It becomes
// visible only after contiguous confirmation; a round/terminal sample remains
// private until its source lifecycle transition has completed.
struct NewClassicNetplayConfirmedInputFrame final {
	std::uint64_t frame = 0U;
	replay::InputPhase phase = replay::InputPhase::gameplay;
	CoreInput input{};
	StepResult step_result{};
	std::uint8_t source_vsyncs = 1U;
};

// Retained active-MAIN owner. Network values enter only through T3NET002's
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
		std::unique_ptr<MainProcess> process,
		std::uint8_t input_delay_frames = 0U
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
	// after its complete source startup transaction. Pause retains the source
	// live-pause owner until resume_pause().
	[[nodiscard]] std::expected<
		NewClassicNetplayMainSessionTick,
		NewClassicNetplayMainSessionError
	> continue_confirmed_boundary() noexcept;
	// Completes one canonical release -> Resume -> release sequence. Arbitrary
	// network waiting never enters Game, V13 timing, PMD, or slowdown.
	[[nodiscard]] std::expected<
		NewClassicNetplayMainSessionTick,
		NewClassicNetplayMainSessionError
	> resume_pause() noexcept;

	[[nodiscard]] std::expected<
		present::SourceSurface,
		present::SourceSurfaceError
	> compose_shown_surface(
		std::span<
			present::ComposedPixel,
			present::graphics_page_pixels
		> output
	) const;
	// Process-start commands are non-speculative and may be applied before the
	// first input frame. Later commands are exposed only through confirmed
	// audio frames and are replaced transactionally during rollback.
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] std::expected<
		std::optional<NewClassicNetplayConfirmedAudioFrame>,
		NewClassicNetplayMainSessionError
	> take_confirmed_audio_frame() noexcept;
	[[nodiscard]] std::expected<
		std::optional<NewClassicNetplayConfirmedSimulationDigest>,
		NewClassicNetplayMainSessionError
	> take_confirmed_simulation_digest() noexcept;
	[[nodiscard]] std::expected<
		std::optional<NewClassicNetplayConfirmedInputFrame>,
		NewClassicNetplayMainSessionError
	> take_confirmed_input_frame() noexcept;
	[[nodiscard]] const MainProcess &process() const noexcept;
	[[nodiscard]] std::uint64_t next_frame() const noexcept;
	[[nodiscard]] std::uint64_t confirmed_frame_count() const noexcept;
	[[nodiscard]] NewClassicNetplayMainSessionStats stats() const noexcept;
	[[nodiscard]] NewClassicNetplayMainSessionStatus status() const noexcept;
	[[nodiscard]] std::optional<StepStatus> boundary() const noexcept;
	[[nodiscard]] InputContext input_context() const noexcept;
	[[nodiscard]] const std::optional<SessionTransition> &transition()
		const noexcept;
	[[nodiscard]] std::expected<AppSha256, NewClassicNetplayMainSessionError>
	simulation_digest(std::uint64_t frame) noexcept;
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
