#ifndef TH03_APP_NEW_CLASSIC_REPLAY_HPP
#define TH03_APP_NEW_CLASSIC_REPLAY_HPP

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "th03/core/game.hpp"
#include "th03/formats/replay.hpp"
#include "th03/replay/session.hpp"

namespace th03::app {

struct NewClassicReplayIdentity final {
	formats::ReplayRuleset ruleset = formats::ReplayRuleset::stock;
	std::uint8_t recording_flags = 0U;
	formats::ReplayRecorderRole recorder_role =
		formats::ReplayRecorderRole::unknown;
	formats::ReplayRecorderSource recorder_source =
		formats::ReplayRecorderSource::native;
	std::array<formats::ReplayAccountUuid, 2U> player_uuid{};
	formats::ReplayMatchId match_id{};
	std::array<std::string, 2U> player_nametag_snapshot{};
};

[[nodiscard]] std::optional<formats::ReplayAccountUuid>
parse_new_classic_replay_account_uuid(std::string_view text) noexcept;

enum class NewClassicReplayErrorCode : std::uint8_t {
	invalid_round_start,
	invalid_sequence,
	document_invalid,
};

struct NewClassicReplayError final {
	NewClassicReplayErrorCode code =
		NewClassicReplayErrorCode::invalid_sequence;
	std::optional<Error> format{};
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
	explicit NewClassicReplayRecorder(
		std::uint16_t recording_date = 0U,
		bool capture_enabled = true,
		NewClassicReplayIdentity identity = {}
	) noexcept :
		recording_date_(recording_date),
		capture_enabled_(capture_enabled),
		identity_(identity)
	{}
	[[nodiscard]] bool set_identity(NewClassicReplayIdentity identity) noexcept;
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

	[[nodiscard]] bool recording() const noexcept;
	[[nodiscard]] bool capture_enabled() const noexcept
	{
		return capture_enabled_;
	}
	// Options can select a named Arrange ruleset only while OP owns no active
	// recording. Arrange runs deliberately have no stock replay/save contract.
	[[nodiscard]] bool set_capture_enabled(bool enabled) noexcept
	{
		if(recording()) {
			return false;
		}
		capture_enabled_ = enabled;
		return true;
	}
	[[nodiscard]] const formats::ReplayDocument *document() const noexcept;
	[[nodiscard]] const CanonicalState *last_terminal() const noexcept;
	[[nodiscard]] std::optional<formats::ReplayDocument> take_completed();
	void discard() noexcept;

private:
	void clear_memory() noexcept;

	std::optional<formats::ReplayDocument> document_{};
	std::optional<formats::ReplayDocument> completed_{};
	std::optional<CanonicalState> last_terminal_{};
	std::optional<formats::ReplayPracticeConfig> practice_config_{};
	std::uint16_t recording_date_ = 0U;
	std::uint32_t source_vsync_clock_ = 0U;
	std::uint32_t round_vsync_last_ = 0U;
	std::uint32_t round_real_frames_ = 0U;
	std::uint32_t round_timed_frames_ = 0U;
	bool round_timer_active_ = false;
	bool slow_sample_pending_ = false;
	bool capture_enabled_ = true;
	NewClassicReplayIdentity identity_{};
};

} // namespace th03::app

#endif
