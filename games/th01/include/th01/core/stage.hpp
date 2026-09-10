#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace th01::core {
struct State;

inline constexpr std::size_t stage_columns = 20U;
inline constexpr std::size_t stage_rows = 10U;
inline constexpr std::size_t cards_per_stage_byte = 4U;
inline constexpr std::size_t stage_count_per_scene = 5U;
inline constexpr std::size_t stage_object_capacity = stage_columns * stage_rows;
inline constexpr std::size_t bomb_item_capacity = 4U;
inline constexpr std::size_t point_item_capacity = 10U;

// `stagedat_stage_t`, kept as bytes rather than a packed host struct.
struct StageSceneRecord {
	std::array<std::array<std::uint8_t, stage_columns / cards_per_stage_byte>,
		stage_rows> cards{};
	std::array<std::array<std::int8_t, stage_columns>, stage_rows> obstacles{};
	bool operator==(const StageSceneRecord &) const = default;
};

enum class CardFlag : std::uint8_t {
	alive = 0U,
	flipping = 1U,
	removed = 2U,
};

struct StageCard {
	std::int16_t left{};
	std::int16_t top{};
	CardFlag flag{CardFlag::alive};
	std::int16_t flip_frame{};
	std::uint8_t hp{};
	std::uint32_t score{};
	bool operator==(const StageCard &) const = default;
};

enum class ObstacleType : std::uint8_t {
	none = 0U,
	bumper = 1U,
	turret_slow_1_aimed = 2U,
	turret_slow_1_random_narrow_aimed = 3U,
	turret_slow_2_spread_wide_aimed = 4U,
	turret_slow_3_spread_wide_aimed = 5U,
	turret_slow_4_spread_wide_aimed = 6U,
	turret_slow_5_spread_wide_aimed = 7U,
	turret_quick_1_aimed = 8U,
	turret_quick_1_random_narrow_aimed = 9U,
	turret_quick_2_spread_wide_aimed = 10U,
	turret_quick_3_spread_wide_aimed = 11U,
	turret_quick_4_spread_wide_aimed = 12U,
	turret_quick_5_spread_wide_aimed = 13U,
	card_two_flip = 14U,
	card_three_flip = 15U,
	card_four_flip = 16U,
	portal = 17U,
	bar_top = 18U,
	bar_bottom = 19U,
	bar_left = 20U,
	bar_right = 21U,
};

enum class TurretFlag : std::uint8_t {
	ready = 0U,
	warmup = 1U,
	fire = 2U,
	cooldown = 3U,
};

struct StageObstacle {
	std::int16_t left{};
	std::int16_t top{};
	ObstacleType type{ObstacleType::none};
	std::int16_t frame{};
	TurretFlag turret_flag{TurretFlag::ready};
	bool operator==(const StageObstacle &) const = default;
};

enum class ItemFlag : std::uint8_t {
	free = 0U,
	splash = 1U,
	fall = 2U,
	bounce = 3U,
	collected = 99U,
	collected_over_cap = 100U,
};

struct StageItem {
	std::int16_t left{};
	std::int16_t top{};
	std::int16_t unknown_zero{};
	std::int16_t velocity_y{};
	ItemFlag flag{ItemFlag::free};
	// splash radius while `flag == splash`, collection duration otherwise.
	std::uint8_t state{};
	bool operator==(const StageItem &) const = default;
};

struct StageState {
	std::array<StageSceneRecord, stage_count_per_scene> scene{};
	std::array<StageCard, stage_object_capacity> cards{};
	std::array<StageObstacle, stage_object_capacity> obstacles{};
	std::array<StageItem, bomb_item_capacity> bombs{};
	std::array<StageItem, point_item_capacity> points{};
	std::uint16_t card_count{};
	std::uint16_t obstacle_count{};
	std::uint16_t timer{};
	std::uint16_t frames_since_harryup{};
	std::uint8_t harryup_cycle{};
	std::uint8_t card_flip_cycle{};
	bool vertical_bars_blocked{};
	bool portals_blocked{};
	std::int16_t entered_portal_slot{};
	std::int16_t portal_destination_left{};
	std::int16_t portal_destination_top{};
	bool operator==(const StageState &) const = default;
};

enum class StageArchiveResult : std::uint8_t {
	completed,
	invalid_scene_id,
	truncated_directory,
	invalid_directory_entry,
	stage_file_missing,
	truncated_payload,
	unsupported_raw_size,
	truncated_rle,
	invalid_stage_data,
};

// Loads source `STAGE?.DAT` from a 64-entry TH01 PF archive.
// The caller owns archive I/O; no original bytes enter the native tree.
[[nodiscard]] StageArchiveResult stage_scene_load_from_archive(
	std::span<const std::uint8_t> archive, std::uint8_t scene_id,
	StageState &stage
) noexcept;

enum class StageSetupResult : std::uint8_t {
	completed,
	invalid_stage_id,
	no_cards,
};

// Source: stageobjs_init_and_render(). `first_stage_in_scene` only controls
// the blocking visual flip-in, which this gameplay core intentionally omits.
[[nodiscard]] StageSetupResult stage_setup(State &state,
	bool first_stage_in_scene) noexcept;

// Source: items_unput_update_render(), before per-frame counters and timer.
void stage_items_update(State &state) noexcept;

// Source: timer_tick_and_put(). The caller invokes this only when frame_rand
// has its low two bits clear, after incrementing the source counters.
// Source position: hud_bg_snap_and_put(), after the stage entrance and for
// boss stages as well. A false return refuses an invalid non-retail route/id.
[[nodiscard]] bool stage_timer_init(State &state) noexcept;
void stage_timer_tick(State &state) noexcept;
void stage_timer_extend(State &state) noexcept;

// Source order inside orb_and_pellets_and_stage_unput_update_render__vsync_wait:
// after pellet update, before orb/player hittest. Cards then update after that
// hittest; the caller makes that second call through `stage_cards_update`.
[[nodiscard]] bool stage_cards_hittest_and_obstacles_update(
	State &state, bool reset_obstacles = false
) noexcept;
void stage_cards_update(State &state) noexcept;

void stage_items_reset(State &state) noexcept;

} // namespace th01::core
