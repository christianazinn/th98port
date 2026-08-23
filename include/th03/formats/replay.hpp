#ifndef TH03_FORMATS_REPLAY_HPP
#define TH03_FORMATS_REPLAY_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "th03/error.hpp"
#include "th03/replay/session.hpp"

namespace th03::formats {

struct ReplayRoundSplit {
	std::uint8_t story_stage = 0;
	std::uint8_t round_id = 0;
	std::uint8_t route = 0;
	std::int8_t winner = -1;
	std::array<std::uint8_t, 4> p1_score{};
	std::array<std::uint8_t, 4> p2_score{};
	// Host-observed VSync time is reporting metadata, never a gameplay oracle.
	std::optional<std::uint32_t> real_frames;
};

enum class ReplayStatus : std::uint8_t {
	empty = 0,
	recording = 1,
	finalized = 2,
	partial = 3,
	error = 4,
};

enum class ReplayEndReason : std::uint8_t {
	none = 0,
	complete = 1,
	menu_return = 2,
	input_end = 3,
	partial = 4,
	error = 5,
	game_over = 6,
};

enum class ReplayRuleset : std::uint8_t {
	stock = 0,
	dopamine_test_v1 = 1,
};

inline constexpr std::uint8_t replay_recording_flag_netplay = 0x01U;
inline constexpr std::uint8_t replay_recording_flags_known =
	replay_recording_flag_netplay;

enum class ReplayRecorderRole : std::uint8_t {
	unknown = 0,
	player_one = 1,
	player_two = 2,
};

enum class ReplayRecorderSource : std::uint8_t {
	unknown = 0,
	pc98 = 1,
	native = 2,
	imported = 3,
};

using ReplayAccountUuid = std::array<std::uint8_t, 16U>;
using ReplayMatchId = std::array<std::uint8_t, 16U>;

struct ReplayStorySummary {
	std::array<std::uint8_t, 9> stage_opponents{};
	std::array<std::array<std::uint8_t, 4>, 9> stage_scores{};
};

struct ReplayPracticeConfig {
	std::uint8_t preset = 0U;
	std::uint8_t stage = 0U;
	std::uint8_t round = 0U;
	std::uint8_t stock = 0U;
	std::uint8_t extends_gained = 0U;
	std::uint8_t cpu_timer = 0U;
	std::uint8_t round_speed = 0U;
	std::uint8_t bullet_speed = 0U;
	std::uint8_t p1_spell = 0U;
	std::uint8_t cpu_spell = 0U;
	std::uint8_t boss_level = 0U;
	std::uint8_t cpu_damage = 0U;
	std::uint16_t initial_cpu_safety_frames = 0U;
	std::uint8_t p1_gauge = 0U;
	std::uint8_t cpu_gauge = 0U;
};

struct ReplayMetadata {
	std::uint16_t flags = 0U;
	ReplayStatus status = ReplayStatus::empty;
	ReplayEndReason end_reason = ReplayEndReason::none;
	std::uint8_t game_mode = 0U;
	std::uint8_t rank = 0U;
	std::uint8_t key_mode = 0U;
	std::array<std::int8_t, 2> paletted_character{};
	std::uint8_t story_stage = 0U;
	std::array<std::uint8_t, 2> is_cpu{};
	std::uint32_t sample_count = 0U;
	std::uint32_t final_frame_count = 0U;
	std::int32_t resident_rand = 0;
	std::uint32_t random_seed = 0U;
	std::uint16_t summary_flags = 0U;
	std::optional<std::uint8_t> final_route;
	std::optional<std::uint8_t> final_game_mode;
	std::optional<std::uint8_t> final_story_stage;
	std::optional<std::uint8_t> final_round_id;
	std::optional<std::int8_t> final_winner;
	std::uint8_t final_story_lives = 0U;
	std::optional<std::uint8_t> final_misses;
	std::uint8_t stage_reached_count = 0U;
	std::optional<ReplayStorySummary> story;
	std::optional<ReplayPracticeConfig> practice;
	std::array<std::uint8_t, 4> final_score{};
	std::uint8_t autofire = 0U;
	std::uint16_t dos_date = 0U;
	std::array<char, 8> name{};
	ReplayRuleset ruleset = ReplayRuleset::stock;
	std::uint8_t recording_flags = 0U;
	ReplayRecorderRole recorder_role = ReplayRecorderRole::unknown;
	ReplayRecorderSource recorder_source = ReplayRecorderSource::unknown;
	std::array<ReplayAccountUuid, 2U> player_uuid{};
	ReplayMatchId match_id{};
	// Match-time public-name snapshots for portable/offline display. UUIDs are
	// authoritative and an online viewer should prefer the latest profile name.
	std::array<std::string, 2U> player_nametag_snapshot{};
};

struct ReplayStageClearBonus {
	std::array<std::uint8_t, 4> total{};
	std::uint8_t max_combo = 0U;
	std::uint8_t gauge_attacks = 0U;
	std::uint8_t boss_attacks = 0U;
	std::uint8_t boss_reversals = 0U;
	std::uint8_t boss_panics = 0U;
	std::uint8_t remaining_lives = 0U;
};

struct ReplaySlowdown {
	std::uint32_t timed_frames = 0U;
	std::uint32_t slow_frames = 0U;
};

// A format-neutral round-start checkpoint. The compressed input cursor remains
// document metadata; deterministic reconstruction stays expressed as GameInit.
struct ReplayCheckpoint {
	std::uint8_t stage = 0U;
	std::uint8_t round = 0U;
	std::uint32_t sample_index = 0U;
	std::uint32_t global_frame = 0U;
	std::uint32_t input_byte_cursor = 0U;
	GameInit init{};
};

struct ReplayAcceleratorBackgroundRecord {
	std::int16_t x_q12_4 = 0;
	std::int16_t y_q12_4 = 0;
	std::int16_t velocity_x_q12_4 = 0;
	std::int16_t velocity_y_q12_4 = 0;
	std::uint8_t angle = 0U;
	std::uint8_t sprite_variant = 0U;

	[[nodiscard]] friend constexpr bool operator==(
		const ReplayAcceleratorBackgroundRecord &,
		const ReplayAcceleratorBackgroundRecord &
	) = default;
};

struct ReplayAcceleratorRgb8 {
	std::uint8_t r = 0U;
	std::uint8_t g = 0U;
	std::uint8_t b = 0U;

	[[nodiscard]] friend constexpr bool operator==(
		const ReplayAcceleratorRgb8 &,
		const ReplayAcceleratorRgb8 &
	) = default;
};

// The optional T3RACC1 image is a cache, not replay authority. These are the
// accelerator-private presentation facts needed by the native player: the
// source's first publication shows [page_back] while drawing the following
// frame into [page_front], player pose selection inherits the shared velocity
// scratch when an update performs no new velocity write, and the animated
// background pool continues from the selected round rather than the process
// anchor used for portable semantic reconstruction.
struct ReplayAcceleratorPresentation {
	std::uint8_t checkpoint = 0U;
	std::uint8_t page_back = 0U;
	std::uint8_t page_front = 1U;
	std::array<ReplayAcceleratorRgb8, 16U> palette{};
	std::int8_t shared_velocity_x_q4_4 = 0;
	std::array<ReplayAcceleratorBackgroundRecord, 50U> background{};
	std::uint8_t background_magnitude = 0x40U;

	[[nodiscard]] friend constexpr bool operator==(
		const ReplayAcceleratorPresentation &,
		const ReplayAcceleratorPresentation &
	) = default;
};

struct ReplayDocument {
	replay::ReplaySession session{};
	bool practice = false;
	std::optional<ReplayMetadata> metadata;
	std::vector<ReplayCheckpoint> checkpoints;
	// Donor-valid T3RACC1 process-state images with sane source page roles. The
	// native player reconstructs gameplay from portable V14 state and imports
	// only the typed presentation facts above, avoiding PC-98
	// decompression/loading latency and native-layout snapshots.
	std::vector<ReplayAcceleratorPresentation> accelerators;
	std::vector<ReplayRoundSplit> expected_round_splits;
	std::optional<std::array<ReplayStageClearBonus, 9>> stage_clear_bonuses;
	std::optional<ReplaySlowdown> slowdown;
};

ReplayRoundSplit make_round_split(
	const CanonicalState &state,
	bool practice
) noexcept;
bool round_splits_match(
	const ReplayRoundSplit &expected,
	const ReplayRoundSplit &actual
) noexcept;

// Adapters without status metadata are finalized reference documents. V14
// preserves its status and must be finalized before an oracle reports success.
bool replay_can_be_verified(const ReplayDocument &document) noexcept;

// Version-neutral adapter for player-facing V14. Historical V11/V12 inputs
// are converted externally; format changes do not cross into the deterministic
// core or replay runner.
std::expected<ReplayDocument, Error> parse_replay(
	std::span<const std::byte> bytes
);

} // namespace th03::formats

#endif
