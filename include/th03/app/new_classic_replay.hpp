#ifndef TH03_APP_NEW_CLASSIC_REPLAY_HPP
#define TH03_APP_NEW_CLASSIC_REPLAY_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <utility>

#include "th03/app/replay_slot_store.hpp"
#include "th03/core/game.hpp"
#include "th03/formats/replay.hpp"
#include "th03/replay/session.hpp"

namespace th03::app {

enum class NewClassicReplayErrorCode : std::uint8_t {
	invalid_round_start,
	invalid_sequence,
	document_invalid,
	journal_conflict,
	journal_store,
};

struct NewClassicReplayError final {
	NewClassicReplayErrorCode code =
		NewClassicReplayErrorCode::invalid_sequence;
	std::optional<Error> format{};
	std::optional<ReplaySlotStoreError> store{};
};

// Replay Patch 0.4.12's replay_finish() preserves master.lib's live RNG only
// for an Esc+R restart handoff. The ordinary post-round resident seed remains
// authoritative for every other exit and transition.
void preserve_new_classic_restart_random_seed(
	GameInit &resident,
	std::uint32_t live_random_seed
) noexcept;

// Application-only Replay Patch capture adapter. It observes accepted Classic
// process boundaries and builds a neutral ReplayDocument; neither experience
// identity nor storage/UI policy crosses into deterministic gameplay.
class NewClassicReplayRecorder final {
public:
	explicit NewClassicReplayRecorder(std::uint16_t recording_date = 0U) noexcept :
		recording_date_(recording_date)
	{}
	explicit NewClassicReplayRecorder(
		std::filesystem::path replay_root,
		std::uint16_t recording_date = 0U
	) noexcept :
		replay_root_(std::move(replay_root)), recording_date_(recording_date)
	{}
	[[nodiscard]] std::expected<void, NewClassicReplayError> round_started(
		const RoundStartObservation &observation,
		std::uint16_t dos_date = 0U
	);
	[[nodiscard]] std::expected<void, NewClassicReplayError> configure_practice(
		std::optional<formats::ReplayPracticeConfig> config
	) noexcept;
	[[nodiscard]] std::expected<void, NewClassicReplayError> sample(
		replay::InputPhase phase,
		CoreInput input
	);
	// source_vsync() advances the MAIN-local wall clock. frame_io() is called
	// only for a real replay_frame_io() boundary, never for pause-menu polling
	// or a driver's defeat-animation hold. A pause-opening frame contributes to
	// real time but is excluded from the Replay Patch slowdown denominator.
	void source_vsync() noexcept;
	void frame_io(
		const StepResult &result,
		bool slowdown_eligible = true
	) noexcept;
	void classify_pending_frame(bool deadline_missed) noexcept;
	[[nodiscard]] std::expected<void, NewClassicReplayError> round_completed(
		const CanonicalState &state,
		std::optional<std::uint32_t> real_frames_override = std::nullopt,
		std::optional<MainResultState> result_state = std::nullopt
	);
	[[nodiscard]] std::expected<void, NewClassicReplayError> process_completed(
		replay::ProcessId process
	);
	[[nodiscard]] std::expected<void, NewClassicReplayError> finish(
		formats::ReplayEndReason reason,
		const GameInit &resident
	);
	// Replay Patch replay_pause_save_refresh() flushes the live fallback every
	// time Replay Pause opens. This call is also safe at explicit native
	// process handoff boundaries.
	[[nodiscard]] std::expected<void, NewClassicReplayError>
	periodic_flush();

	[[nodiscard]] bool recording() const noexcept;
	[[nodiscard]] const formats::ReplayDocument *document() const noexcept;
	[[nodiscard]] const CanonicalState *last_terminal() const noexcept;
	[[nodiscard]] std::optional<formats::ReplayDocument> take_completed();
	void discard() noexcept;
	[[nodiscard]] std::expected<void, NewClassicReplayError>
	discard_persistent() noexcept;

private:
	[[nodiscard]] std::expected<void, NewClassicReplayError>
	checkpoint_journal(const formats::ReplayDocument &document);
	void clear_memory() noexcept;

	std::optional<formats::ReplayDocument> document_{};
	std::optional<formats::ReplayDocument> completed_{};
	std::optional<CanonicalState> last_terminal_{};
	std::optional<formats::ReplayPracticeConfig> practice_config_{};
	std::filesystem::path replay_root_{};
	std::uint16_t recording_date_ = 0U;
	bool journal_claimed_ = false;
	std::uint32_t source_vsync_clock_ = 0U;
	std::uint32_t round_vsync_last_ = 0U;
	std::uint32_t round_real_frames_ = 0U;
	std::uint32_t round_timed_frames_ = 0U;
	bool round_timer_active_ = false;
	bool slow_sample_pending_ = false;
};

} // namespace th03::app

#endif
