#include "th01/core/stage.hpp"

#include "th01/core/orb.hpp"
#include "th01/core/projectiles.hpp"
#include "th01/core/state.hpp"
#include "pc98/random.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace th01::core {
namespace {

inline constexpr std::int16_t playfield_top = 64;
inline constexpr std::int16_t playfield_bottom = 400;
inline constexpr std::int16_t player_top = playfield_bottom - 32;
inline constexpr std::int16_t stage_object_width = 32;
inline constexpr std::int16_t stage_object_orb_distance = 24;
inline constexpr std::int16_t pellet_half_size = 4;
inline constexpr std::int16_t item_size = 8;
inline constexpr std::int16_t item_player_distance = 24;
inline constexpr std::int16_t boss_stage = 4;
inline constexpr std::int16_t card_frames_per_cel = 6;
inline constexpr std::int16_t card_cels = 5;
inline constexpr std::int16_t card_flip_done = 25;
inline constexpr std::int16_t card_score_done = 28;
inline constexpr std::uint32_t card_score_cap = 25600U;
inline constexpr std::int16_t turret_slow_interval = 200;
inline constexpr std::int16_t turret_quick_interval = 100;
inline constexpr std::int16_t turret_done = 10;
inline constexpr std::int16_t collision_block_frames = 8;
inline constexpr std::size_t archive_entry_count = 64U;
inline constexpr std::size_t archive_entry_size = 32U;
inline constexpr std::size_t archive_directory_size =
	archive_entry_count * archive_entry_size;
inline constexpr std::size_t archive_name_offset = 3U;
inline constexpr std::size_t archive_name_size = 13U;
inline constexpr std::size_t archive_pack_size_offset = 16U;
inline constexpr std::size_t archive_original_size_offset = 20U;
inline constexpr std::size_t archive_payload_offset = 24U;
inline constexpr std::size_t stage_file_header_size = 22U;
inline constexpr std::size_t stage_record_size =
	(stage_rows * (stage_columns / cards_per_stage_byte)) +
	(stage_rows * stage_columns);
inline constexpr std::size_t stage_file_required_size =
	stage_file_header_size + (stage_count_per_scene * stage_record_size);
inline constexpr std::uint8_t archive_key = 0x76U;
inline constexpr std::uint8_t compressed_type_byte = 0x95U;

[[nodiscard]] constexpr std::int16_t add_i16(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) + static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::int16_t sub_i16(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) - static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::uint16_t abs_i16(std::int16_t value) noexcept
{
	const auto raw = std::bit_cast<std::uint16_t>(value);
	return (value < 0) ? static_cast<std::uint16_t>(0U - raw) : raw;
}

[[nodiscard]] constexpr std::uint16_t abs_difference(
	std::int16_t left, std::int16_t right
) noexcept
{
	return abs_i16(sub_i16(left, right));
}

[[nodiscard]] constexpr std::int16_t mul_i16(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(left) * static_cast<std::int32_t>(right)
	));
}

[[nodiscard]] constexpr bool stage_is_boss(std::int16_t stage_id) noexcept
{
	return stage_id >= 0 && (stage_id % 5) == boss_stage;
}

[[nodiscard]] constexpr bool is_actual_obstacle(ObstacleType type) noexcept
{
	switch(type) {
	case ObstacleType::bumper:
	case ObstacleType::turret_slow_1_aimed:
	case ObstacleType::turret_slow_1_random_narrow_aimed:
	case ObstacleType::turret_slow_2_spread_wide_aimed:
	case ObstacleType::turret_slow_3_spread_wide_aimed:
	case ObstacleType::turret_slow_4_spread_wide_aimed:
	case ObstacleType::turret_slow_5_spread_wide_aimed:
	case ObstacleType::turret_quick_1_aimed:
	case ObstacleType::turret_quick_1_random_narrow_aimed:
	case ObstacleType::turret_quick_2_spread_wide_aimed:
	case ObstacleType::turret_quick_3_spread_wide_aimed:
	case ObstacleType::turret_quick_4_spread_wide_aimed:
	case ObstacleType::turret_quick_5_spread_wide_aimed:
	case ObstacleType::portal:
	case ObstacleType::bar_top:
	case ObstacleType::bar_bottom:
	case ObstacleType::bar_left:
	case ObstacleType::bar_right:
		return true;
	default:
		return false;
	}
}

[[nodiscard]] constexpr bool is_quick_turret(ObstacleType type) noexcept
{
	return type >= ObstacleType::turret_quick_1_aimed &&
		type <= ObstacleType::turret_quick_5_spread_wide_aimed;
}

[[nodiscard]] constexpr PelletGroup turret_group(ObstacleType type) noexcept
{
	switch(type) {
	case ObstacleType::turret_slow_1_aimed:
	case ObstacleType::turret_quick_1_aimed:
		return PelletGroup::one_aimed;
	case ObstacleType::turret_slow_1_random_narrow_aimed:
	case ObstacleType::turret_quick_1_random_narrow_aimed:
		return PelletGroup::one_random_narrow_aimed;
	case ObstacleType::turret_slow_2_spread_wide_aimed:
	case ObstacleType::turret_quick_2_spread_wide_aimed:
		return PelletGroup::two_spread_wide_aimed;
	case ObstacleType::turret_slow_3_spread_wide_aimed:
	case ObstacleType::turret_quick_3_spread_wide_aimed:
		return PelletGroup::three_spread_wide_aimed;
	case ObstacleType::turret_slow_4_spread_wide_aimed:
	case ObstacleType::turret_quick_4_spread_wide_aimed:
		return PelletGroup::four_spread_wide_aimed;
	case ObstacleType::turret_slow_5_spread_wide_aimed:
	case ObstacleType::turret_quick_5_spread_wide_aimed:
		return PelletGroup::five_spread_wide_aimed;
	default:
		return PelletGroup::none;
	}
}

[[nodiscard]] constexpr std::uint32_t read_le32(
	std::span<const std::uint8_t> bytes, std::size_t offset
) noexcept
{
	return static_cast<std::uint32_t>(bytes[offset]) |
		(static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
		(static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
		(static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

[[nodiscard]] bool archive_name_is_stage(
	std::span<const std::uint8_t> entry, std::uint8_t scene_id
) noexcept
{
	std::array<std::uint8_t, archive_name_size> expected{};
	expected[0U] = static_cast<std::uint8_t>('S');
	expected[1U] = static_cast<std::uint8_t>('T');
	expected[2U] = static_cast<std::uint8_t>('A');
	expected[3U] = static_cast<std::uint8_t>('G');
	expected[4U] = static_cast<std::uint8_t>('E');
	expected[5U] = static_cast<std::uint8_t>('0' + scene_id);
	expected[6U] = static_cast<std::uint8_t>('.');
	expected[7U] = static_cast<std::uint8_t>('D');
	expected[8U] = static_cast<std::uint8_t>('A');
	expected[9U] = static_cast<std::uint8_t>('T');
	for(std::size_t index = 0U; index < archive_name_size; ++index) {
		// arc_load stops before complementing the on-disk NUL terminator.
		if(entry[archive_name_offset + index] == 0U) return expected[index] == 0U;
		const auto decoded = static_cast<std::uint8_t>(
			~entry[archive_name_offset + index]
		);
		if(decoded != expected[index]) return false;
		if(expected[index] == 0U) return true;
	}
	return false;
}

[[nodiscard]] bool payload_is_available(
	std::span<const std::uint8_t> archive, std::uint32_t offset,
	std::uint32_t size
) noexcept
{
	const auto first = static_cast<std::size_t>(offset);
	const auto count = static_cast<std::size_t>(size);
	return first <= archive.size() && count <= (archive.size() - first);
}

[[nodiscard]] StageArchiveResult decode_stage_records(
	std::span<const std::uint8_t> data, StageState &stage
) noexcept
{
	if(data.size() < stage_file_required_size) {
		return StageArchiveResult::invalid_stage_data;
	}
	std::array<StageSceneRecord, stage_count_per_scene> decoded{};
	std::size_t cursor = stage_file_header_size;
	for(auto &record : decoded) {
		for(auto &row : record.cards) {
			for(auto &cell : row) cell = data[cursor++];
		}
		for(auto &row : record.obstacles) {
			for(auto &cell : row) {
				cell = std::bit_cast<std::int8_t>(data[cursor++]);
			}
		}
	}
	stage.scene = decoded;
	return StageArchiveResult::completed;
}

[[nodiscard]] StageArchiveResult decode_compressed_stage_records(
	std::span<const std::uint8_t> payload, std::uint32_t original_size,
	StageState &stage
) noexcept
{
	if(static_cast<std::size_t>(original_size) < stage_file_required_size) {
		return StageArchiveResult::invalid_stage_data;
	}
	std::array<std::uint8_t, stage_file_required_size> decoded{};
	std::size_t source = 0U;
	auto next = [&payload, &source](std::uint8_t &out) noexcept {
		if(source == payload.size()) return false;
		out = static_cast<std::uint8_t>(payload[source++] ^ archive_key);
		return true;
	};
	std::uint8_t value{};
	if(!next(value)) return StageArchiveResult::truncated_rle;
	std::uint8_t runs{};
	for(std::size_t written = 0U; written < decoded.size(); ++written) {
		decoded[written] = value;
		if((written + 1U) == decoded.size()) break;
		if(runs == 0U) {
			const auto run_value = value;
			if(!next(value)) return StageArchiveResult::truncated_rle;
			if(run_value == value) {
				if(!next(runs)) return StageArchiveResult::truncated_rle;
			}
		} else {
			--runs;
		}
	}
	return decode_stage_records(decoded, stage);
}

void items_add(std::span<StageItem> items, const StageCard &card) noexcept
{
	for(auto &item : items) {
		if(item.flag != ItemFlag::free) continue;
		item.flag = ItemFlag::splash;
		item.left = card.left;
		item.top = card.top;
		item.unknown_zero = 0;
		item.velocity_y = 2;
		item.state = 4U;
		return;
	}
}

[[nodiscard]] bool item_player_hittest(
	const State &state, const StageItem &item
) noexcept
{
	return item.left > sub_i16(state.player.left, item_player_distance) &&
		item.left < add_i16(state.player.left, item_player_distance) &&
		item.top > (player_top - item_player_distance) &&
		item.flag > ItemFlag::splash && item.flag < ItemFlag::collected;
}

void item_collect_init(StageItem &item) noexcept
{
	item.velocity_y = -2;
	item.state = 16U;
	if(item.top >= (playfield_bottom - item_size)) {
		item.top = playfield_bottom - item_size;
	}
}

void update_bomb_item(State &state, StageItem &item) noexcept
{
	if(item_player_hittest(state, item)) {
		item_collect_init(item);
		if(state.rem_bombs < 5) {
			++state.rem_bombs;
			item.flag = ItemFlag::collected;
		} else {
			state.score += 10000U;
			item.flag = ItemFlag::collected_over_cap;
		}
	}

	switch(item.flag) {
	case ItemFlag::splash:
		item.state = static_cast<std::uint8_t>(item.state + 4U);
		if(item.state >= 48U) item.flag = ItemFlag::fall;
		break;
	case ItemFlag::fall:
		item.top = add_i16(item.top, item.velocity_y);
		if(item.top >= (playfield_bottom - item_size)) {
			item.velocity_y = -8;
			item.flag = ItemFlag::bounce;
			item.top = add_i16(item.top, item.velocity_y);
			item.velocity_y = add_i16(item.velocity_y, 1);
		}
		break;
	case ItemFlag::bounce:
		item.top = add_i16(item.top, item.velocity_y);
		item.velocity_y = add_i16(item.velocity_y, 1);
		if(item.top >= playfield_bottom) item.flag = ItemFlag::free;
		break;
	case ItemFlag::collected:
	case ItemFlag::collected_over_cap:
		item.top = add_i16(item.top, item.velocity_y);
		--item.state;
		if(item.state == 0U) item.flag = ItemFlag::free;
		break;
	default:
		break;
	}
}

void update_point_item(State &state, StageItem &item) noexcept
{
	if(item_player_hittest(state, item)) {
		item_collect_init(item);
		if(state.point_value < 59999U) {
			state.point_value = static_cast<std::uint16_t>(state.point_value +
				((state.point_value < 10000U) ? 1000U : 10000U));
		}
		if(state.point_value >= 60000U) state.point_value = 65530U;
		state.score += state.point_value;
		item.flag = ItemFlag::collected_over_cap;
	}

	switch(item.flag) {
	case ItemFlag::splash:
		item.state = static_cast<std::uint8_t>(item.state + 4U);
		if(item.state >= 48U) item.flag = ItemFlag::fall;
		break;
	case ItemFlag::fall:
		item.top = add_i16(item.top, item.velocity_y);
		if(item.top >= (playfield_bottom - item_size)) {
			item.velocity_y = -8;
			item.flag = ItemFlag::bounce;
			item.top = add_i16(item.top, item.velocity_y);
			item.velocity_y = add_i16(item.velocity_y, 1);
		}
		break;
	case ItemFlag::bounce:
		item.top = add_i16(item.top, item.velocity_y);
		item.velocity_y = add_i16(item.velocity_y, 1);
		if(item.top >= playfield_bottom) {
			item.flag = ItemFlag::free;
			state.point_value = 0U;
		}
		break;
	case ItemFlag::collected:
	case ItemFlag::collected_over_cap:
		item.top = add_i16(item.top, item.velocity_y);
		--item.state;
		if(item.state == 0U) item.flag = ItemFlag::free;
		break;
	default:
		break;
	}
}

void cards_hittest(State &state) noexcept
{
	for(std::size_t index = 0U; index < state.stage.card_count; ++index) {
		auto &card = state.stage.cards[index];
		const bool alive = card.flag == CardFlag::alive;
		const bool orb_hit = abs_difference(card.left, state.orb.horizontal.left) <
			static_cast<std::uint16_t>(stage_object_orb_distance) &&
			abs_difference(card.top, state.orb.top) <
			static_cast<std::uint16_t>(stage_object_orb_distance);
		const bool bomb_hit = state.bomb_damaging &&
			(static_cast<std::uint32_t>(state.bomb_frame) % state.stage.card_count) == index &&
			(pc98::irand(state.random_seed) % 4U) != 0U;
		if(!alive || !(orb_hit || state.test_damage || bomb_hit)) continue;

		card.flag = CardFlag::flipping;
		const auto combo_square = mul_i16(
			state.cardcombo_cur, state.cardcombo_cur
		);
		const auto stage_score = static_cast<std::uint32_t>(
			(static_cast<std::int32_t>(state.stage_id / 5) * 100) + 100
		);
		const auto multiplier = (state.rank == 3) ? 35U : 20U;
		card.score = stage_score +
			(static_cast<std::uint32_t>(static_cast<std::int32_t>(combo_square)) * multiplier);
		if(card.score > card_score_cap) card.score = card_score_cap;
		state.score += card.score;
		state.cardcombo_cur = add_i16(state.cardcombo_cur, 1);
		if(state.cardcombo_max < state.cardcombo_cur) {
			state.cardcombo_max = state.cardcombo_cur;
		}

		const auto previous_cycle = state.stage.card_flip_cycle;
		state.stage.card_flip_cycle = static_cast<std::uint8_t>(
			state.stage.card_flip_cycle + 1U
		);
		if((previous_cycle % 10U) == 0U) {
			if(state.stage.card_flip_cycle >= 140U) {
				state.stage.card_flip_cycle = 1U;
				if(state.bomb_damaging) {
					items_add(state.stage.points, card);
				} else {
					items_add(state.stage.bombs, card);
				}
			} else {
				items_add(state.stage.points, card);
			}
		}
	}
}

[[nodiscard]] bool apply_immediate_force(
	State &state, double immediate
) noexcept
{
	return apply_orb_force(
		state.orb.vertical, state.orb.horizontal.velocity, state.random_seed,
		immediate, OrbForce::immediate
	);
}

[[nodiscard]] bool apply_surface_force(State &state) noexcept
{
	return apply_orb_force(
		state.orb.vertical, state.orb.horizontal.velocity, state.random_seed,
		0.0, OrbForce::surface
	);
}

void reset_turret_flags(StageState &stage) noexcept
{
	for(std::size_t index = 0U; index < stage.obstacle_count; ++index) {
		stage.obstacles[index].turret_flag = TurretFlag::ready;
	}
}

void turret_update(State &state, StageObstacle &obstacle, bool reset) noexcept
{
	if(state.rank != 0) obstacle.frame = add_i16(obstacle.frame, 1);
	const auto interval = is_quick_turret(obstacle.type) ?
		turret_quick_interval : turret_slow_interval;
	if(obstacle.frame >= interval) {
		switch(obstacle.turret_flag) {
		case TurretFlag::ready:
			obstacle.turret_flag = TurretFlag::warmup;
			break;
		case TurretFlag::warmup:
			if((obstacle.frame % 10) == 9) {
				obstacle.turret_flag = TurretFlag::fire;
			}
			break;
		case TurretFlag::fire:
			pellets_add_group(
				state,
				add_i16(obstacle.left, stage_object_width / 2 - pellet_half_size),
				add_i16(obstacle.top, stage_object_width / 2 - pellet_half_size),
				turret_group(obstacle.type), 56
			);
			obstacle.turret_flag = TurretFlag::cooldown;
			break;
		case TurretFlag::cooldown:
			obstacle.turret_flag = static_cast<TurretFlag>(
				static_cast<std::uint8_t>(obstacle.turret_flag) + 1U
			);
			if(static_cast<std::uint8_t>(obstacle.turret_flag) >= turret_done) {
				obstacle.turret_flag = TurretFlag::ready;
				obstacle.frame = 0;
			}
			break;
		}
	}
	if(reset) {
		obstacle.frame = 0;
		reset_turret_flags(state.stage);
	}
}

[[nodiscard]] bool portal_update(
	State &state, std::size_t index, bool reset
) noexcept
{
	auto &stage = state.stage;
	auto &obstacle = stage.obstacles[index];
	if(reset) {
		stage.entered_portal_slot = 0;
		stage.portals_blocked = false;
		if(state.orb.in_portal) state.orb.in_portal = false;
		return true;
	}
	if(stage.portals_blocked && stage.entered_portal_slot !=
		static_cast<std::int16_t>(index)) {
		return true;
	}
	if(obstacle.frame != 0) obstacle.frame = add_i16(obstacle.frame, 1);
	if(obstacle.frame == 0) {
		state.orb.in_portal = true;
		obstacle.frame = 1;
		stage.entered_portal_slot = static_cast<std::int16_t>(index);
		stage.portals_blocked = true;
	} else if(obstacle.frame == 20) {
		std::size_t destination = 0U;
		std::int16_t completed_loops = 0;
		while(true) {
			if(stage.obstacles[destination].type == ObstacleType::portal &&
				(pc98::irand(state.random_seed) % 4U) == 0U && destination != index) {
				break;
			}
			++destination;
			if(completed_loops > 15) {
				destination = index;
				break;
			}
			if(destination >= stage.obstacle_count) {
				destination = 0U;
				++completed_loops;
			}
		}
		stage.portal_destination_left = stage.obstacles[destination].left;
		stage.portal_destination_top = stage.obstacles[destination].top;
	} else if(obstacle.frame == 40) {
		state.orb.horizontal.velocity = static_cast<OrbVelocityX>(
			pc98::irand(state.random_seed) % 5U
		);
		const auto immediate = static_cast<std::int16_t>(
			static_cast<std::int16_t>(pc98::irand(state.random_seed) % 19U) - 9
		);
		if(!apply_immediate_force(state, static_cast<double>(immediate))) return false;
		state.orb.horizontal.left = stage.portal_destination_left;
		state.orb.top = stage.portal_destination_top;
		state.orb.in_portal = false;
	} else if(obstacle.frame == 60) {
		obstacle.frame = 0;
		stage.portals_blocked = false;
	}
	return true;
}

[[nodiscard]] bool obstacles_update(State &state, bool reset) noexcept
{
	auto &stage = state.stage;
	if(reset) stage.vertical_bars_blocked = false;
	for(std::size_t index = 0U; index < stage.obstacle_count; ++index) {
		auto &obstacle = stage.obstacles[index];
		const auto delta_abs_x = abs_difference(
			obstacle.left, state.orb.horizontal.left
		);
		const auto delta_abs_y = abs_difference(obstacle.top, state.orb.top);
		const auto delta_x = sub_i16(state.orb.horizontal.left, obstacle.left);
		const auto delta_y = sub_i16(state.orb.top, obstacle.top);

		switch(obstacle.type) {
		case ObstacleType::bumper:
			if(delta_abs_x < static_cast<std::uint16_t>(stage_object_orb_distance) &&
				delta_abs_y < static_cast<std::uint16_t>(stage_object_orb_distance) &&
				obstacle.frame == 0) {
				obstacle.frame = 1;
				state.orb.top = (delta_y <= 0) ?
					sub_i16(obstacle.top, stage_object_orb_distance) :
					add_i16(obstacle.top, stage_object_orb_distance);
				if(state.orb.horizontal.velocity == OrbVelocityX::right4 && delta_x < 0) {
					state.orb.horizontal.velocity = OrbVelocityX::left4;
				} else if(state.orb.horizontal.velocity == OrbVelocityX::left4 && delta_x > 0) {
					state.orb.horizontal.velocity = OrbVelocityX::right4;
				}
				if(!apply_surface_force(state)) return false;
			}
			if(obstacle.frame != 0) {
				obstacle.frame = add_i16(obstacle.frame, 1);
				if(obstacle.frame >= collision_block_frames) obstacle.frame = 0;
			}
			if(reset) obstacle.frame = 0;
			continue;

		case ObstacleType::turret_slow_1_aimed:
		case ObstacleType::turret_slow_1_random_narrow_aimed:
		case ObstacleType::turret_slow_2_spread_wide_aimed:
		case ObstacleType::turret_slow_3_spread_wide_aimed:
		case ObstacleType::turret_slow_4_spread_wide_aimed:
		case ObstacleType::turret_slow_5_spread_wide_aimed:
		case ObstacleType::turret_quick_1_aimed:
		case ObstacleType::turret_quick_1_random_narrow_aimed:
		case ObstacleType::turret_quick_2_spread_wide_aimed:
		case ObstacleType::turret_quick_3_spread_wide_aimed:
		case ObstacleType::turret_quick_4_spread_wide_aimed:
		case ObstacleType::turret_quick_5_spread_wide_aimed:
			turret_update(state, obstacle, reset);
			continue;

		case ObstacleType::portal:
			if((delta_abs_x < static_cast<std::uint16_t>(stage_object_orb_distance) &&
				delta_abs_y < static_cast<std::uint16_t>(stage_object_orb_distance)) ||
				obstacle.frame != 0) {
				if(!portal_update(state, index, false)) return false;
			}
			if(reset) {
				obstacle.frame = 0;
				if(!portal_update(state, index, true)) return false;
			}
			continue;

		case ObstacleType::bar_bottom:
			if(delta_abs_x < static_cast<std::uint16_t>(stage_object_width) &&
				delta_y <= stage_object_width && delta_y > 0 && obstacle.frame == 0) {
				obstacle.frame = 1;
				if(!apply_immediate_force(state, -state.orb.vertical.velocity)) return false;
			}
			break;
		case ObstacleType::bar_top:
			if(delta_abs_x < static_cast<std::uint16_t>(stage_object_width) &&
				delta_y < 0 && delta_y >= -stage_object_width && obstacle.frame == 0) {
				obstacle.frame = 1;
				if(!apply_immediate_force(state, -state.orb.vertical.velocity)) return false;
			}
			break;
		case ObstacleType::bar_left: {
			const auto x_delta = sub_i16(obstacle.left, state.orb.horizontal.left);
			if(delta_abs_y < static_cast<std::uint16_t>(stage_object_width) &&
				x_delta <= stage_object_width && x_delta >= 0 && obstacle.frame == 0 &&
				!stage.vertical_bars_blocked) {
				obstacle.frame = 1;
				stage.vertical_bars_blocked = true;
				if(state.orb.horizontal.velocity == OrbVelocityX::stationary) {
					if(!apply_immediate_force(state, -state.orb.vertical.velocity)) return false;
				} else {
					state.orb.horizontal.velocity = reflect_orb_x(state.orb.horizontal.velocity);
				}
			}
			break;
		}
		case ObstacleType::bar_right: {
			const auto x_delta = sub_i16(obstacle.left, state.orb.horizontal.left);
			if(delta_abs_y < static_cast<std::uint16_t>(stage_object_width) &&
				x_delta <= 0 && x_delta >= -stage_object_width && obstacle.frame == 0 &&
				!stage.vertical_bars_blocked) {
				obstacle.frame = 1;
				stage.vertical_bars_blocked = true;
				if(state.orb.horizontal.velocity == OrbVelocityX::stationary) {
					if(!apply_immediate_force(state, -state.orb.vertical.velocity)) return false;
				} else {
					state.orb.horizontal.velocity = reflect_orb_x(state.orb.horizontal.velocity);
				}
			}
			break;
		}
		default:
			continue;
		}

		if(obstacle.frame != 0) {
			obstacle.frame = add_i16(obstacle.frame, 1);
			if(obstacle.frame >= collision_block_frames) {
				obstacle.frame = 0;
				stage.vertical_bars_blocked = false;
			}
		}
		if(reset) obstacle.frame = 0;
	}
	return true;
}

} // namespace

StageArchiveResult stage_scene_load_from_archive(
	std::span<const std::uint8_t> archive, std::uint8_t scene_id,
	StageState &stage
) noexcept
{
	if(scene_id > 9U) return StageArchiveResult::invalid_scene_id;
	if(archive.size() < archive_directory_size) {
		return StageArchiveResult::truncated_directory;
	}
	for(std::size_t index = 0U; index < archive_entry_count; ++index) {
		const auto entry_offset = index * archive_entry_size;
		const auto entry = archive.subspan(entry_offset, archive_entry_size);
		if(entry[0U] == 0U) break;
		if(!archive_name_is_stage(entry, scene_id)) continue;
		const auto pack_size = read_le32(entry, archive_pack_size_offset);
		const auto original_size = read_le32(entry, archive_original_size_offset);
		const auto payload_offset = read_le32(entry, archive_payload_offset);
		if(!payload_is_available(archive, payload_offset, pack_size)) {
			return StageArchiveResult::truncated_payload;
		}
		const auto payload = archive.subspan(
			static_cast<std::size_t>(payload_offset), static_cast<std::size_t>(pack_size)
		);
		if(entry[0U] == compressed_type_byte && entry[1U] == compressed_type_byte) {
			return decode_compressed_stage_records(payload, original_size, stage);
		}
		if(pack_size != original_size ||
			static_cast<std::size_t>(original_size) < stage_file_required_size) {
			return StageArchiveResult::unsupported_raw_size;
		}
		std::array<std::uint8_t, stage_file_required_size> decoded{};
		for(std::size_t byte = 0U; byte < decoded.size(); ++byte) {
			decoded[byte] = static_cast<std::uint8_t>(payload[byte] ^ archive_key);
		}
		return decode_stage_records(decoded, stage);
	}
	return StageArchiveResult::stage_file_missing;
}

StageSetupResult stage_setup(State &state, bool first_stage_in_scene) noexcept
{
	(void)first_stage_in_scene;
	if(state.stage_id < 0) return StageSetupResult::invalid_stage_id;
	const auto stage_in_scene = static_cast<std::size_t>(state.stage_id % 5);
	state.stage.card_count = 0U;
	state.stage.obstacle_count = 0U;
	if(stage_in_scene == static_cast<std::size_t>(boss_stage)) {
		return StageSetupResult::completed;
	}
	const auto &source = state.stage.scene[stage_in_scene];
	for(const auto &row : source.cards) {
		for(const auto packed : row) {
			for(std::uint8_t bit = 0x08U; bit != 0U; bit >>= 1U) {
				if((packed & bit) != 0U) ++state.stage.card_count;
			}
		}
	}
	for(const auto &row : source.obstacles) {
		for(const auto raw_type : row) {
			if(is_actual_obstacle(static_cast<ObstacleType>(
				static_cast<std::uint8_t>(raw_type)))) {
				++state.stage.obstacle_count;
			}
		}
	}
	if(state.stage.card_count == 0U) return StageSetupResult::no_cards;

	(void)(pc98::irand(state.random_seed) % state.stage.card_count);
	state.stage.cards = {};
	state.stage.obstacles = {};
	state.stage.card_count = 0U;
	state.stage.obstacle_count = 0U;
	for(std::size_t row = 0U; row < stage_rows; ++row) {
		for(std::size_t packed_column = 0U;
			packed_column < (stage_columns / cards_per_stage_byte); ++packed_column) {
			const auto packed = source.cards[row][packed_column];
			for(std::uint8_t bit = 0x08U; bit != 0U; bit >>= 1U) {
				if((packed & bit) == 0U) continue;
				const auto bit_index = static_cast<std::size_t>(
					(bit == 0x08U) ? 0U : (bit == 0x04U) ? 1U :
					(bit == 0x02U) ? 2U : 3U
				);
				auto &card = state.stage.cards[state.stage.card_count++];
				card.left = static_cast<std::int16_t>(
					((packed_column * cards_per_stage_byte) + bit_index) *
					stage_object_width
				);
				card.top = static_cast<std::int16_t>(
					playfield_top + static_cast<std::int16_t>(row * stage_object_width)
				);
			}
		}
	}
	for(std::size_t row = 0U; row < stage_rows; ++row) {
		for(std::size_t column = 0U; column < stage_columns; ++column) {
			const auto type = static_cast<ObstacleType>(
				static_cast<std::uint8_t>(source.obstacles[row][column])
			);
			if(type >= ObstacleType::card_two_flip &&
				type <= ObstacleType::card_four_flip) {
				const auto left = static_cast<std::int16_t>(column * stage_object_width);
				const auto top = static_cast<std::int16_t>(
					playfield_top + static_cast<std::int16_t>(row * stage_object_width)
				);
				for(std::size_t card_index = 0U;
					card_index < state.stage.card_count; ++card_index) {
					auto &card = state.stage.cards[card_index];
					if(card.left == left && card.top == top) {
						card.hp = static_cast<std::uint8_t>(
							static_cast<std::uint8_t>(type) -
							static_cast<std::uint8_t>(ObstacleType::card_two_flip) + 1U
						);
						break;
					}
				}
				continue;
			}
			if(!is_actual_obstacle(type)) continue;
			auto &obstacle = state.stage.obstacles[state.stage.obstacle_count++];
			obstacle.left = static_cast<std::int16_t>(column * stage_object_width);
			obstacle.top = static_cast<std::int16_t>(
				playfield_top + static_cast<std::int16_t>(row * stage_object_width)
			);
			obstacle.type = type;
		}
	}
	return StageSetupResult::completed;
}

void stage_items_update(State &state) noexcept
{
	for(auto &item : state.stage.bombs) {
		if(item.flag != ItemFlag::free) update_bomb_item(state, item);
	}
	for(auto &item : state.stage.points) {
		if(item.flag != ItemFlag::free) update_point_item(state, item);
	}
}

bool stage_timer_init(State &state) noexcept
{
	constexpr std::array<std::uint16_t, 35U> stage_times{
		1000U, 1000U, 1000U, 1000U, 2000U,
		1200U, 1200U, 1200U, 1200U, 3000U,
		1400U, 1400U, 1400U, 1400U, 4000U,
		1600U, 1600U, 1600U, 600U, 6000U,
		1200U, 1200U, 1200U, 1200U, 2000U,
		1400U, 1400U, 1400U, 1400U, 4000U,
		1600U, 1800U, 1800U, 1600U, 8000U,
	};
	if(state.stage_id < 0 || state.stage_id >= 20 ||
		(state.route != 0 && state.route != 1)) {
		return false;
	}
	const auto stage_id = static_cast<std::size_t>(state.stage_id);
	const auto offset = (stage_id < 5U) ? 0U :
		(static_cast<std::size_t>(state.route) * 15U);
	state.stage.timer = stage_times[offset + stage_id];
	state.stage.frames_since_harryup = 0U;
	return true;
}

void stage_timer_tick(State &state) noexcept
{
	if(state.stage.timer > 0U) {
		state.stage.timer = static_cast<std::uint16_t>(state.stage.timer - 2U);
		if(state.stage.timer == 0U) {
			pellet_speed_raise(state, pellet_speed_from_fortieths(2));
		}
		return;
	}
	state.stage.frames_since_harryup = static_cast<std::uint16_t>(
		state.stage.frames_since_harryup + 1U
	);
	state.stage.harryup_cycle = static_cast<std::uint8_t>(
		state.stage.harryup_cycle + 1U
	);
	const auto frame = state.stage.frames_since_harryup;
	const auto cycle = state.stage.harryup_cycle;
	const bool fire = (frame < 200U) ? ((cycle % 5U) == 0U) :
		(frame < 300U) ? ((cycle % 4U) == 0U) :
		(frame < 400U) ? ((cycle % 3U) == 0U) :
		(frame < 500U) ? ((cycle % 2U) == 0U) : true;
	if(!fire) return;
	for(std::int16_t index = 0; index <= state.rank; ++index) {
		pellets_add_group(
			state,
			static_cast<std::int16_t>(pc98::irand(state.random_seed) % 640U),
			playfield_top, PelletGroup::one_random_wide, pellet_pixels_to_subpixels(3)
		);
	}
}

void stage_timer_extend(State &state) noexcept
{
	state.stage.timer = static_cast<std::uint16_t>(state.stage.timer +
		((state.stage.timer != 0U) ? 200U : 1000U));
	state.stage.frames_since_harryup = 0U;
}

bool stage_cards_hittest_and_obstacles_update(
	State &state, bool reset_obstacles
) noexcept
{
	if(!reset_obstacles && !stage_is_boss(state.stage_id)) cards_hittest(state);
	return obstacles_update(state, reset_obstacles);
}

void stage_cards_update(State &state) noexcept
{
	if(stage_is_boss(state.stage_id)) return;
	std::uint16_t cards_removed = 0U;
	for(std::size_t index = 0U; index < state.stage.card_count; ++index) {
		auto &card = state.stage.cards[index];
		if(card.score != 0U && card.flip_frame >= card_score_done) card.score = 0U;
		if(card.flag == CardFlag::flipping) {
			card.flip_frame = add_i16(card.flip_frame, 1);
			if(card.flip_frame >= (card_frames_per_cel * 4)) {
				if(state.rank == 3 && card.flip_frame == card_flip_done) {
					const auto group = (state.stage_id + 1) < 10 ?
						PelletGroup::one_aimed : PelletGroup::one_random_narrow_aimed;
					pellets_add_group(
						state,
						add_i16(card.left, stage_object_width / 2 - pellet_half_size),
						add_i16(card.top, stage_object_width / 2 - pellet_half_size),
						group, 56
					);
				}
				if(card.flip_frame >= (card_frames_per_cel * card_cels)) {
					if(card.hp == 0U) {
						card.flag = CardFlag::removed;
						card.flip_frame = 0;
					} else {
						card.flag = CardFlag::alive;
						card.flip_frame = 0;
						--card.hp;
					}
				}
			}
		}
		if(card.flag == CardFlag::removed) ++cards_removed;
	}
	if(cards_removed == state.stage.card_count) {
		state.stage_cleared = true;
		state.input.player_is_hit = true;
	}
}

void stage_items_reset(State &state) noexcept
{
	for(auto &item : state.stage.bombs) item.flag = ItemFlag::free;
	for(auto &item : state.stage.points) item.flag = ItemFlag::free;
}

} // namespace th01::core
