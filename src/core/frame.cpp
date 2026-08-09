#include "game_internal.hpp"
#include "trig.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace th03::detail {
namespace {

constexpr std::uint16_t input_up = 0x0001U;
constexpr std::uint16_t input_down = 0x0002U;
constexpr std::uint16_t input_left = 0x0004U;
constexpr std::uint16_t input_right = 0x0008U;
constexpr std::uint16_t input_bomb = 0x0010U;
constexpr std::uint16_t input_shot = 0x0020U;
constexpr std::uint16_t input_up_left = 0x0100U;
constexpr std::uint16_t input_down_left = 0x0200U;
constexpr std::uint16_t input_up_right = 0x0400U;
constexpr std::uint16_t input_down_right = 0x0800U;
constexpr std::uint16_t input_cancel = 0x1000U;
constexpr std::uint16_t input_movement =
	input_up | input_down | input_left | input_right |
	input_up_left | input_down_left | input_up_right | input_down_right;
constexpr std::int16_t player_x_min = 8 << 4;
constexpr std::int16_t player_x_max = (288 - 8) << 4;
constexpr std::int16_t player_y_min = 24 << 4;
constexpr std::int16_t player_y_max = (368 - 16) << 4;
constexpr std::uint16_t gauge_64 = 64U << 4U;
constexpr std::uint16_t gauge_128 = 128U << 4U;
constexpr std::uint16_t gauge_192 = 192U << 4U;
constexpr std::uint16_t gauge_max = 0xFFU << 4U;
constexpr std::uint8_t bomb_frames = 180U;
constexpr std::int16_t ellen_bomb_vector_free = 9999;
constexpr std::int16_t ellen_bomb_vector_inactive = 19999;
constexpr std::uint8_t miss_invincibility_frames = 110U;
constexpr std::uint8_t knockback_frames = 64U;
constexpr std::uint8_t hit_damage_max = 6U;
constexpr std::uint8_t game_mode_story = 1U;
constexpr std::uint8_t game_mode_demo = 0x7FU;
constexpr std::uint8_t extends_max = 2U;
constexpr std::uint8_t extends_disabled = 255U;
constexpr std::uint8_t position_clip_x = 0x01U;
constexpr std::uint8_t position_clip_bottom = 0x02U;
constexpr std::uint8_t position_do_not_mirror_x = 0x80U;
constexpr std::uint8_t op_stop = 0x00U;
constexpr std::uint8_t op_move_linear = 0x01U;
constexpr std::uint8_t op_move_circular = 0x02U;
constexpr std::uint8_t op_wait = 0x03U;
constexpr std::uint8_t op_move_sine_x = 0x04U;
constexpr std::uint8_t op_move_sine_y = 0x05U;
constexpr std::uint8_t op_move = 0x06U;
constexpr std::uint8_t op_move_with_speed = 0x07U;
constexpr std::uint8_t op_move_linear_stop_at_player_y = 0x08U;
constexpr std::uint8_t op_move_linear_stop_at_player_x = 0x09U;
constexpr std::uint8_t op_spawn = 0x10U;
constexpr std::uint8_t op_move_circular_plus = 0x0AU;
constexpr std::uint8_t op_clip_x = 0x82U;
constexpr std::uint8_t op_clip_bottom = 0x83U;
constexpr std::int16_t shot_velocity_y = -(12 << 4);
constexpr std::int16_t shotpair_width = 32 << 4;
constexpr std::int16_t shotpair_height = 16 << 4;
constexpr std::int16_t shotpair_half_width = shotpair_width / 2;
constexpr std::int16_t shotpair_half_height = shotpair_height / 2;
constexpr std::int16_t bullet_center_min = -(8 << 4);
constexpr std::int16_t bullet_center_x_max = (288 + 8) << 4;
constexpr std::int16_t bullet_center_y_max = (368 + 8) << 4;
constexpr std::uint8_t bullet_group_one = 0U;
constexpr std::uint8_t bullet_group_one_aimed = 1U;
constexpr std::uint8_t bullet_group_two_spread_narrow = 2U;
constexpr std::uint8_t bullet_group_two_spread_medium = 3U;
constexpr std::uint8_t bullet_group_two_spread_wide = 4U;
constexpr std::uint8_t bullet_group_two_spread_narrow_aimed = 5U;
constexpr std::uint8_t bullet_group_two_spread_medium_aimed = 6U;
constexpr std::uint8_t bullet_group_two_spread_wide_aimed = 7U;
constexpr std::uint8_t bullet_group_three_spread_narrow = 8U;
constexpr std::uint8_t bullet_group_three_spread_medium = 9U;
constexpr std::uint8_t bullet_group_three_spread_wide = 10U;
constexpr std::uint8_t bullet_group_three_spread_narrow_aimed = 11U;
constexpr std::uint8_t bullet_group_three_spread_medium_aimed = 12U;
constexpr std::uint8_t bullet_group_three_spread_wide_aimed = 13U;
constexpr std::uint8_t bullet_group_four_spread_narrow = 14U;
constexpr std::uint8_t bullet_group_four_spread_medium = 15U;
constexpr std::uint8_t bullet_group_four_spread_wide = 16U;
constexpr std::uint8_t bullet_group_four_spread_narrow_aimed = 17U;
constexpr std::uint8_t bullet_group_four_spread_medium_aimed = 18U;
constexpr std::uint8_t bullet_group_four_spread_wide_aimed = 19U;
constexpr std::uint8_t bullet_group_five_spread_narrow = 20U;
constexpr std::uint8_t bullet_group_five_spread_medium = 21U;
constexpr std::uint8_t bullet_group_five_spread_wide = 22U;
constexpr std::uint8_t bullet_group_five_spread_narrow_aimed = 23U;
constexpr std::uint8_t bullet_group_five_spread_medium_aimed = 24U;
constexpr std::uint8_t bullet_group_five_spread_wide_aimed = 25U;
constexpr std::uint8_t bullet_group_one_random_angle = 26U;
constexpr std::uint8_t bullet_group_random_angle = 27U;
constexpr std::uint8_t bullet_group_random_angle_and_speed = 28U;
constexpr std::uint8_t bullet_group_random_constrained_aimed = 29U;
constexpr std::uint8_t bullet_group_four_ring = 32U;
constexpr std::uint8_t bullet_group_two_ring = 33U;
constexpr std::uint8_t bullet_group_two_spread_horizontally_symmetric = 34U;
constexpr std::uint8_t bullet_group_eight_ring = 35U;
constexpr std::uint8_t bullet_group_sixteen_ring = 36U;
constexpr std::uint8_t bullet_group_thirty_two_ring = 37U;
constexpr std::uint8_t bullet_group_ring = 38U;
constexpr std::uint8_t bullet_group_two_ring_aimed = 39U;
constexpr std::uint8_t bullet_group_four_ring_aimed = 40U;
constexpr std::uint8_t bullet_group_eight_ring_aimed = 41U;
constexpr std::uint8_t bullet_group_sixteen_ring_aimed = 42U;
constexpr std::uint8_t bullet_group_thirty_two_ring_aimed = 43U;
constexpr std::uint8_t bullet_group_ring_aimed = 44U;
constexpr std::uint16_t anniversary_collmap_dgroup_offset = 0x4BA0U;
constexpr std::uint16_t anniversary_runtime_profile_offset = 0x1076U;
// player_hittest() is a source landmine: a broad CPU probe can wrap before
// collmap and mask bits from initialized Borland runtime data in DGROUP. This
// compatibility profile covers 0x1076..0x1181 in the Anniversary MAIN layout.
// Its linker-map ownership is, in order: cputype, fbrk, signal, _pathops,
// srchenv, srchstr, setvbuf, sysnerr, setenvp, strings, loadprog, and runtime
// initializer records. Exact bytes matter because the scan applies a changing
// bit mask; a mere occupied-address set is not equivalent.
constexpr std::array<std::uint8_t, 0x10CU>
	anniversary_borland_runtime_collision_alias_profile{{
	0x04U, 0x00U, 0xADU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U, 0x04U,
	0x08U, 0x0BU, 0x0FU, 0x16U, 0x00U, 0x15U, 0x8CU, 0x0BU,
	0x00U, 0x00U, 0x02U, 0x00U, 0xF6U, 0x03U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x5CU, 0x00U, 0x01U, 0x00U,
	0x01U, 0x00U, 0x30U, 0x00U, 0x3CU, 0x6EU, 0x6FU, 0x74U,
	0x79U, 0x70U, 0x65U, 0x3EU, 0x00U, 0x2AU, 0x2AU, 0x42U,
	0x43U, 0x43U, 0x78U, 0x68U, 0x31U, 0x00U, 0x04U, 0x00U,
	0x32U, 0x34U, 0x01U, 0x00U, 0x00U, 0x00U, 0x01U, 0x00U,
	0x3FU, 0x00U, 0x40U, 0x00U, 0x3FU, 0x00U, 0x53U, 0x74U,
	0x72U, 0x69U, 0x6EU, 0x67U, 0x20U, 0x72U, 0x65U, 0x66U,
	0x65U, 0x72U, 0x65U, 0x6EU, 0x63U, 0x65U, 0x20U, 0x6FU,
	0x75U, 0x74U, 0x20U, 0x6FU, 0x66U, 0x20U, 0x72U, 0x61U,
	0x6EU, 0x67U, 0x65U, 0x00U, 0x53U, 0x74U, 0x72U, 0x69U,
	0x6EU, 0x67U, 0x20U, 0x72U, 0x65U, 0x66U, 0x65U, 0x72U,
	0x65U, 0x6EU, 0x63U, 0x65U, 0x20U, 0x6FU, 0x75U, 0x74U,
	0x20U, 0x6FU, 0x66U, 0x20U, 0x72U, 0x61U, 0x6EU, 0x67U,
	0x65U, 0x00U, 0x53U, 0x74U, 0x72U, 0x69U, 0x6EU, 0x67U,
	0x20U, 0x72U, 0x65U, 0x66U, 0x65U, 0x72U, 0x65U, 0x6EU,
	0x63U, 0x65U, 0x20U, 0x6FU, 0x75U, 0x74U, 0x20U, 0x6FU,
	0x66U, 0x20U, 0x72U, 0x61U, 0x6EU, 0x67U, 0x65U, 0x00U,
	0x50U, 0x41U, 0x54U, 0x48U, 0x00U, 0x2FU, 0x5CU, 0x00U,
	0x00U, 0x2EU, 0x42U, 0x41U, 0x54U, 0x00U, 0x2EU, 0x43U,
	0x4FU, 0x4DU, 0x00U, 0x2EU, 0x45U, 0x58U, 0x45U, 0x00U,
	0x2EU, 0x42U, 0x41U, 0x54U, 0x00U, 0x43U, 0x4FU, 0x4DU,
	0x53U, 0x50U, 0x45U, 0x43U, 0x00U, 0x2FU, 0x63U, 0x00U,
	0xFFU, 0x20U, 0xE4U, 0x36U, 0x24U, 0x09U, 0xFFU, 0x02U,
	0x9FU, 0x38U, 0x00U, 0x00U, 0xFFU, 0x10U, 0x8BU, 0x3CU,
	0x00U, 0x00U, 0xFFU, 0x0FU, 0xDEU, 0x45U, 0x00U, 0x00U,
	0xFFU, 0x10U, 0x4CU, 0x91U, 0x00U, 0x00U, 0x01U, 0x20U,
	0x3FU, 0x37U, 0x24U, 0x09U,
}};
constexpr std::array<std::array<std::uint16_t, 9>, 6> cpu_collision_routes{{
	{{0x0400U, 0x0008U, 0x0800U, 0x0001U, 0x0008U, 0x0008U, 0x0100U, 0x0400U, 0x0400U}},
	{{0x0004U, 0x0004U, 0x0100U, 0x0002U, 0x0002U, 0x0002U, 0x0100U, 0x0001U, 0x0400U}},
	{{0x0004U, 0x0004U, 0x0100U, 0x0004U, 0x0004U, 0x0100U, 0x0100U, 0x0004U, 0x0100U}},
	{{0x0200U, 0x0200U, 0x0008U, 0x0002U, 0x0800U, 0x0800U, 0x0004U, 0x0008U, 0x0008U}},
	{{0x0200U, 0x0200U, 0x0800U, 0x0001U, 0x0200U, 0x0001U, 0x0001U, 0x0004U, 0x0008U}},
	{{0x0004U, 0x0200U, 0x0002U, 0x0200U, 0x0100U, 0x0400U, 0x0100U, 0x0004U, 0x0001U}},
}};
constexpr std::array<std::array<std::uint16_t, 9>, 3> cpu_retry_routes{{
	{{0x0008U, 0x0800U, 0x0002U, 0x0000U, 0x0400U, 0x0200U, 0x0100U, 0x0004U, 0x0001U}},
	{{0x0004U, 0x0008U, 0x0002U, 0x0200U, 0x0800U, 0x0000U, 0x0100U, 0x0400U, 0x0001U}},
	{{0x0004U, 0x0200U, 0x0002U, 0x0000U, 0x0100U, 0x0800U, 0x0400U, 0x0008U, 0x0001U}},
}};

enum class MoveResult {
	invalid,
	valid,
	no_input,
};

struct Velocity {
	std::int16_t x = 0;
	std::int16_t y = 0;
};

void score_add(
	std::array<std::uint8_t, score_digits> &score,
	std::uint16_t delta
) noexcept
{
	std::uint16_t carry = delta;
	for(std::size_t index = 0U; index < (score_digits - 1U); ++index) {
		auto &digit = score[index];
		const auto sum = static_cast<std::uint16_t>(digit + (carry % 10U));
		digit = static_cast<std::uint8_t>(sum % 10U);
		carry = static_cast<std::uint16_t>((carry / 10U) + (sum / 10U));
	}
	// score_add.asm intentionally skips AAA for the most-significant digit.
	// Preserve its raw byte addition, including values above 9 and byte wrap.
	score.back() = exact::wrap_add(
		score.back(), static_cast<std::uint8_t>(carry)
	);
}

void gauge_avail_add(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t charge
) noexcept
{
	auto &player = state.players[player_id];
	auto &gauge = player.gauge_avail;
	if(player.hyper_active != 0U || gauge >= gauge_max) {
		return;
	}
	gauge = std::min<std::uint16_t>(
		gauge_max, static_cast<std::uint16_t>(gauge + charge)
	);
}

std::uint16_t combo_add(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t hits,
	std::uint16_t bonus
) noexcept
{
	auto &combo = state.combos[player_id];
	if(hits < 2U) {
		return combo.bonus_total;
	}
	auto total = static_cast<std::uint32_t>(combo.bonus_total) + bonus;
	combo.bonus_total = static_cast<std::uint16_t>(
		std::min<std::uint32_t>(total, 0xFFFFU)
	);
	auto &player = state.players[player_id];
	player.combo_hits_max = std::max(player.combo_hits_max, hits);
	player.combo_bonus_max = std::max(player.combo_bonus_max, combo.bonus_total);
	const auto display_hits = std::min<std::uint8_t>(hits, 99U);
	if(hits > 99U && combo.bonus_total != 0xFFFFU) {
		combo.time = 80U;
	}
	if(
		display_hits > combo.hits_highest || combo.time < 32U
	) {
		combo.hits_highest = display_hits;
		if(combo.bonus_total != 0xFFFFU) {
			combo.time = 80U;
		}
	}
	return combo.bonus_total;
}

void fire_point_based_boss_attack_or_panic(
	DeterministicState &state,
	std::uint16_t points,
	std::uint8_t player_id,
	std::uint8_t chain_slot
) noexcept
{
	if(points < state.combo_points_for_boss_attack) {
		state.boss_panic_fired_in_combo[player_id] = 0U;
		return;
	}
	if(state.warning_flag[player_id] != WarningFlag::none) {
		return;
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		if(state.gba_flag_active[player_id] == GaugeBossAttackFlag::boss) {
			return;
		}
		state.gba_flag_next[player_id] = GaugeBossAttackFlag::boss;
		state.warning_flag[player_id] = WarningFlag::portrait;
		state.chains.pellet_and_fireball_value[player_id][chain_slot] = 0U;
		state.chains.charge_fireball[player_id][chain_slot] = 0U;
		state.chains.charge_exatt[player_id][chain_slot] = 0U;
		state.combo_points_for_boss_attack = static_cast<std::uint16_t>(
			state.combo_points_for_boss_attack + 5120U
		);
		if(state.gba_boss_level < 16U) {
			state.gba_boss_level++;
		}
		if(state.gba_boss_launched_by < 0) {
			state.players[player_id].boss_attacks_fired++;
		} else {
			state.players[player_id].boss_attacks_reversed++;
		}
		return;
	}
	if(
		points < 30000U ||
		state.boss_panic_fired_in_combo[player_id] != 0U ||
		state.gba_flag_active[player_id] != GaugeBossAttackFlag::none
	) {
		return;
	}
	state.gba_flag_next[player_id] = GaugeBossAttackFlag::gauge_bullet_init;
	state.warning_flag[player_id] = WarningFlag::portrait;
	state.boss_panic_fired_in_combo[player_id] = 1U;
	state.gba_gauge_level[player_id] = std::min<std::uint8_t>(
		16U, static_cast<std::uint8_t>(state.gba_gauge_level[player_id] + 3U)
	);
	state.players[player_id].boss_panics_fired++;
}

void combos_update(DeterministicState &state) noexcept
{
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		auto &combo = state.combos[player_id];
		if(combo.time == 0U) {
			continue;
		}
		combo.time--;
		if(combo.time == 0U) {
			combo.hits_highest = 0U;
			score_add(state.score[player_id], combo.bonus_total);
			combo.bonus_total = 0U;
		}
	}
}

std::uint8_t bullet_speed_adjusted(
	const DeterministicState &state,
	std::uint8_t speed
) noexcept
{
	const auto adjusted = static_cast<std::uint8_t>(
		speed + state.bullet_base_speed
	);
	if(adjusted > (8U << 4U)) {
		return 8U << 4U;
	}
	if(adjusted < (1U << 4U)) {
		return 1U << 4U;
	}
	return adjusted;
}

BulletState *bullet_free_slot(DeterministicState &state) noexcept
{
	const auto slot = std::find_if(
		state.bullets.begin(), state.bullets.end(),
		[](const BulletState &bullet) { return bullet.flag == BulletFlag::free; }
	);
	return (slot == state.bullets.end()) ? nullptr : &*slot;
}

bool bullet_center_is_spawnable(
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	return (
		center_y > bullet_center_min && center_y < bullet_center_y_max &&
		center_x > bullet_center_min && center_x < bullet_center_x_max
	);
}

void bullets_add_group(
	DeterministicState &state,
	BulletFlag type,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id,
	std::uint8_t speed,
	std::uint8_t angle,
	std::uint8_t group,
	std::uint8_t count = 0U,
	std::uint8_t accel_type = 0U,
	bool has_trail = false
) noexcept
{
	state.bullet_template_angle = angle;
	if(!bullet_center_is_spawnable(center_x, center_y)) {
		return;
	}
	const auto adjusted_speed = bullet_speed_adjusted(state, speed);
	for(std::uint16_t group_index = 0U;; ++group_index) {
		auto *bullet = bullet_free_slot(state);
		if(bullet == nullptr) {
			return;
		}

		std::int16_t angle_offset = 0;
		std::int16_t actual_speed = adjusted_speed;
		bool aimed = false;
		bool done = false;
		switch(group) {
		case bullet_group_one:
			done = true;
			break;

		case bullet_group_one_aimed:
			aimed = true;
			done = true;
			break;

		case bullet_group_two_spread_narrow:
		case bullet_group_two_spread_medium:
		case bullet_group_two_spread_wide:
		case bullet_group_two_spread_narrow_aimed:
		case bullet_group_two_spread_medium_aimed:
		case bullet_group_two_spread_wide_aimed: {
			aimed = (group >= bullet_group_two_spread_narrow_aimed);
			const auto variant = aimed
				? static_cast<std::int16_t>(
					group - bullet_group_two_spread_narrow_aimed
				)
				: static_cast<std::int16_t>(
					group - bullet_group_two_spread_narrow
				);
			const auto extra = static_cast<std::int16_t>(variant * 3);
			if(group_index == 0U) {
				angle_offset = static_cast<std::int16_t>(3 + extra);
			} else {
				angle_offset = static_cast<std::int16_t>(-3 - extra);
				done = true;
			}
			break;
		}

		case bullet_group_two_spread_horizontally_symmetric:
			if(group_index == 0U) {
				angle_offset = 0;
			} else {
				angle_offset = static_cast<std::int16_t>(
					0x80 - (static_cast<std::int16_t>(angle) * 2)
				);
				done = true;
			}
			break;

		case bullet_group_three_spread_narrow:
		case bullet_group_three_spread_medium:
		case bullet_group_three_spread_wide:
		case bullet_group_three_spread_narrow_aimed:
		case bullet_group_three_spread_medium_aimed:
		case bullet_group_three_spread_wide_aimed: {
			aimed = (group >= bullet_group_three_spread_narrow_aimed);
			const auto variant = aimed
				? static_cast<std::int16_t>(
					group - bullet_group_three_spread_narrow_aimed
				)
				: static_cast<std::int16_t>(
					group - bullet_group_three_spread_narrow
				);
			const auto extra = static_cast<std::int16_t>(variant * 8);
			if(group_index == 0U) {
				angle_offset = 0;
			} else if(group_index == 1U) {
				angle_offset = static_cast<std::int16_t>(8 + extra);
			} else {
				angle_offset = static_cast<std::int16_t>(-8 - extra);
				done = true;
			}
			break;
		}

		case bullet_group_four_spread_narrow:
		case bullet_group_four_spread_medium:
		case bullet_group_four_spread_wide:
		case bullet_group_four_spread_narrow_aimed:
		case bullet_group_four_spread_medium_aimed:
		case bullet_group_four_spread_wide_aimed: {
			aimed = (group >= bullet_group_four_spread_narrow_aimed);
			const auto variant = aimed
				? static_cast<std::int16_t>(
					group - bullet_group_four_spread_narrow_aimed
				)
				: static_cast<std::int16_t>(
					group - bullet_group_four_spread_narrow
				);
			const auto extra = static_cast<std::int16_t>(variant * 6);
			switch(group_index) {
			case 0U:
				angle_offset = static_cast<std::int16_t>(3 + extra);
				break;
			case 1U:
				angle_offset = static_cast<std::int16_t>(-3 - extra);
				break;
			case 2U:
				angle_offset = static_cast<std::int16_t>(9 + (extra * 3));
				break;
			default:
				angle_offset = static_cast<std::int16_t>(-9 - (extra * 3));
				done = true;
				break;
			}
			break;
		}

		case bullet_group_five_spread_narrow:
		case bullet_group_five_spread_medium:
		case bullet_group_five_spread_wide:
		case bullet_group_five_spread_narrow_aimed:
		case bullet_group_five_spread_medium_aimed:
		case bullet_group_five_spread_wide_aimed: {
			aimed = (group >= bullet_group_five_spread_narrow_aimed);
			const auto variant = aimed
				? static_cast<std::int16_t>(
					group - bullet_group_five_spread_narrow_aimed
				)
				: static_cast<std::int16_t>(
					group - bullet_group_five_spread_narrow
				);
			const auto extra = static_cast<std::int16_t>(variant * 6);
			switch(group_index) {
			case 0U: angle_offset = 0; break;
			case 1U:
				angle_offset = static_cast<std::int16_t>(6 + extra);
				break;
			case 2U:
				angle_offset = static_cast<std::int16_t>(-6 - extra);
				break;
			case 3U:
				angle_offset = static_cast<std::int16_t>(12 + (extra * 2));
				break;
			default:
				angle_offset = static_cast<std::int16_t>(-12 - (extra * 2));
				done = true;
				break;
			}
			break;
		}

		case bullet_group_one_random_angle:
			angle_offset = static_cast<std::uint8_t>(randring_next16(state));
			done = true;
			break;

		case bullet_group_random_angle:
			angle_offset = static_cast<std::uint8_t>(randring_next16(state));
			done = (group_index >= count);
			break;

		case bullet_group_random_angle_and_speed:
			angle_offset = static_cast<std::uint8_t>(randring_next16(state));
			actual_speed = static_cast<std::int16_t>(
				actual_speed + static_cast<std::int16_t>(
					randring_next16(state) & 0x1FU
				)
			);
			done = (group_index >= count);
			break;

		case bullet_group_random_constrained_aimed:
			aimed = true;
			angle_offset = static_cast<std::int16_t>(
				(randring_next16(state) & 0x1FU) - 0x10
			);
			done = (group_index >= count);
			break;

		case bullet_group_four_ring:
			angle_offset = static_cast<std::int16_t>(group_index << 6U);
			done = (group_index >= 3U);
			break;

		case bullet_group_two_ring:
			angle_offset = static_cast<std::int16_t>(group_index << 7U);
			done = (group_index >= 1U);
			break;

		case bullet_group_eight_ring:
			angle_offset = static_cast<std::int16_t>(group_index << 5U);
			done = (group_index >= 7U);
			break;

		case bullet_group_sixteen_ring:
			angle_offset = static_cast<std::int16_t>(group_index << 4U);
			done = (group_index >= 15U);
			break;

		case bullet_group_thirty_two_ring:
			angle_offset = static_cast<std::int16_t>(group_index << 3U);
			done = (group_index >= 31U);
			break;

		case bullet_group_ring:
			if(count == 0U) {
				return;
			}
			angle_offset = static_cast<std::int16_t>(
				exact::wrap_multiply(
					static_cast<std::int16_t>(group_index),
					static_cast<std::int16_t>(0x100)
				) / static_cast<std::int16_t>(count)
			);
			done = (group_index >= static_cast<std::uint16_t>(count - 1U));
			break;

		case bullet_group_two_ring_aimed:
			aimed = true;
			angle_offset = static_cast<std::int16_t>(group_index << 7U);
			done = (group_index >= 1U);
			break;

		case bullet_group_four_ring_aimed:
			aimed = true;
			angle_offset = static_cast<std::int16_t>(group_index << 6U);
			done = (group_index >= 3U);
			break;

		case bullet_group_eight_ring_aimed:
			aimed = true;
			angle_offset = static_cast<std::int16_t>(group_index << 5U);
			done = (group_index >= 7U);
			break;

		case bullet_group_sixteen_ring_aimed:
			aimed = true;
			angle_offset = static_cast<std::int16_t>(group_index << 4U);
			done = (group_index >= 15U);
			break;

		case bullet_group_thirty_two_ring_aimed:
			aimed = true;
			angle_offset = static_cast<std::int16_t>(group_index << 3U);
			done = (group_index >= 31U);
			break;

		case bullet_group_ring_aimed:
			if(count == 0U) {
				return;
			}
			aimed = true;
			angle_offset = static_cast<std::int16_t>(
				exact::wrap_multiply(
					static_cast<std::int16_t>(group_index),
					static_cast<std::int16_t>(0x100)
				) / static_cast<std::int16_t>(count)
			);
			done = (group_index >= static_cast<std::uint16_t>(count - 1U));
			break;

		default:
			aimed = true;
			break;
		}

		bullet->flag = type;
		bullet->age = 0U;
		bullet->center_x = center_x;
		bullet->center_y = center_y;
		bullet->player_id = player_id;
		bullet->group_next = group;
		bullet->speed_next = adjusted_speed;
		bullet->accel_type = accel_type;
		bullet->is_collidable = true;
		bullet->has_trail = has_trail;
		if(has_trail) {
			bullet->trail_index = state.bullet_trail_cursor;
			auto &trail = state.bullet_trails[state.bullet_trail_cursor];
			trail.center_x.fill(center_x);
			trail.center_y.fill(center_y);
			state.bullet_trail_cursor++;
			if(state.bullet_trail_cursor >= state.bullet_trails.size()) {
				state.bullet_trail_cursor = 0U;
			}
		}
		bullet->angle_next = static_cast<std::uint8_t>(angle_offset + angle);
		const auto velocity = aimed
			? vector2_between_plus(
				center_x,
				center_y,
				state.players[player_id].center_x.raw(),
				state.players[player_id].center_y.raw(),
				bullet->angle_next,
				actual_speed
			)
			: vector2(
				bullet->angle_next,
				actual_speed
			);
		bullet->velocity_x = velocity.x;
		bullet->velocity_y = velocity.y;
		if(done) {
			return;
		}
	}
}

bool bullet_trail_update_and_clip(
	std::array<std::int16_t, 6> &coordinates,
	std::int16_t previous,
	std::int16_t minimum,
	std::int16_t maximum
) noexcept
{
	coordinates[5] = coordinates[4];
	if(coordinates[5] <= minimum || coordinates[5] >= maximum) {
		return false;
	}
	coordinates[4] = coordinates[3];
	coordinates[3] = coordinates[2];
	coordinates[2] = coordinates[1];
	coordinates[1] = coordinates[0];
	coordinates[0] = previous;
	return true;
}

void bullet_add_kana_extra_attack(
	DeterministicState &state,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id
) noexcept
{
	// exatt_update_kana() writes 0x40 to the shared source template before
	// attempting the allocation. Preserve the write even if the arena is full
	// or the center is clipped; a later callback can inherit it.
	state.bullet_template_angle = 0x40U;
	if(!bullet_center_is_spawnable(center_x, center_y)) {
		return;
	}
	auto *bullet = bullet_free_slot(state);
	if(bullet == nullptr) {
		return;
	}
	bullet->flag = BulletFlag::bullet16;
	bullet->age = 0U;
	bullet->center_x = center_x;
	bullet->center_y = center_y;
	bullet->player_id = player_id;
	bullet->group_next = bullet_group_one;
	bullet->angle_next = 0x40U;
	bullet->speed_next = bullet_speed_adjusted(state, 1U << 4U);
	bullet->accel_type = 0x80U;
	bullet->is_collidable = true;
	bullet->has_trail = false;
	const auto velocity = vector2(
		bullet->angle_next, static_cast<std::int16_t>(bullet->speed_next)
	);
	bullet->velocity_x = velocity.x;
	bullet->velocity_y = velocity.y;
}

std::int16_t playfield_x_to_screen_subpixel(
	const DeterministicState &state,
	std::int16_t x,
	std::uint8_t player_id
) noexcept
{
	const auto pixel = exact::arithmetic_shift_right(x, 4U);
	return static_cast<std::int16_t>(
		(pixel + 16) + (static_cast<int>(player_id) * 320) +
		state.playfield_fg_shift_x[player_id]
	);
}

std::int16_t screen_x_to_playfield_subpixel(
	std::int16_t x,
	std::uint8_t player_id
) noexcept
{
	const auto pixel = static_cast<std::int16_t>(
		x - (static_cast<int>(player_id) * 320) - 16
	);
	return exact::wrap_shift_left(pixel, 4U);
}

enum class FireballVariant : std::uint8_t {
	blue = 0,
	red = 1,
};

bool fireball_add(
	DeterministicState &state,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id,
	FireballVariant variant
) noexcept
{
	const auto slot = std::find_if(
		state.fireballs.begin(), state.fireballs.end(),
		[](const FireballState &fireball) {
			return fireball.flag == FireballFlag::free;
		}
	);
	if(slot == state.fireballs.end()) {
		return false;
	}
	slot->flag = FireballFlag::transfer;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->player_id = player_id;
	const auto fall_speed = static_cast<std::uint16_t>(
		20U + (state.round_speed / 2U) + (randring_next16(state) & 31U)
	);
	slot->fall_velocity_y = static_cast<std::uint8_t>(
		std::min<std::uint16_t>(fall_speed, 8U << 4U)
	);
	const auto variant_value = static_cast<std::uint8_t>(variant);
	slot->hp = static_cast<std::uint8_t>(variant_value + 2U);
	slot->variant_as_explosion_target = static_cast<std::uint8_t>(
		2U + variant_value
	);
	slot->size_pixels = 48U;
	slot->generation = (variant == FireballVariant::blue)
		? 0U
		: static_cast<std::uint8_t>(state.fireball_generation_previous + 1U);

	const auto origin_screen = playfield_x_to_screen_subpixel(
		state, center_x, player_id
	);
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	std::int16_t target_x = 0;
	if((state.round_or_result_frame % 512U) < 256U) {
		target_x = static_cast<std::int16_t>(
			randring_next16(state) % static_cast<std::uint16_t>(288U << 4U)
		);
	} else {
		target_x = exact::wrap_add(
			state.players[target_player].center_x.raw(),
			static_cast<std::int16_t>(randring_next16(state) & 511U)
		);
	}
	slot->target_x_for_target_player = target_x;
	const auto target_screen = playfield_x_to_screen_subpixel(
		state, target_x, target_player
	);
	const auto travel_speed = static_cast<std::int16_t>(
		70U + (state.round_speed / 3U)
	);
	const auto velocity = vector2_between_plus(
		exact::wrap_shift_left(origin_screen, 4U),
		center_y,
		exact::wrap_shift_left(target_screen, 4U),
		0,
		0U,
		travel_speed
	);
	slot->velocity_x = velocity.x;
	slot->velocity_y = velocity.y;
	slot->target_x_for_origin_player = screen_x_to_playfield_subpixel(
		target_screen, player_id
	);
	return true;
}

void extra_attack_transfer_init(
	DeterministicState &state,
	ExtraAttackState &slot,
	std::int16_t center_x,
	std::int16_t center_y,
	std::int16_t target_x,
	std::int16_t target_y,
	std::uint8_t player_id,
	std::int16_t velocity_base
) noexcept
{
	slot = ExtraAttackState{};
	slot.state = 2U;
	slot.center_x = center_x;
	slot.center_y = center_y;
	slot.player_id = player_id;
	slot.target_x_for_target_player = target_x;
	const auto origin_screen = playfield_x_to_screen_subpixel(
		state, center_x, player_id
	);
	const auto target_screen = playfield_x_to_screen_subpixel(
		state, target_x, static_cast<std::uint8_t>(1U - player_id)
	);
	const auto velocity = vector2_between_plus(
		exact::wrap_shift_left(origin_screen, 4U),
		center_y,
		exact::wrap_shift_left(target_screen, 4U),
		target_y,
		0U,
		static_cast<std::int16_t>(
			velocity_base + static_cast<std::int16_t>(state.round_speed / 4U)
		)
	);
	slot.velocity_x = velocity.x;
	slot.velocity_y = velocity.y;
	slot.target_x_for_origin_player = screen_x_to_playfield_subpixel(
		target_screen, player_id
	);
}

void extra_attack_add(
	DeterministicState &state,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id
) noexcept
{
	const auto character =
		(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
	if(character == 0) {
		const auto end = state.extra_attacks[player_id].begin() +
			extra_attack_regular_count;
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(),
			end,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == end) {
			return;
		}
		const auto target_x = static_cast<std::int16_t>(
			randring_next16(state) % (288U << 4U)
		);
		const auto target_y = static_cast<std::int16_t>(
			randring_next16(state) & 2047U
		);
		extra_attack_transfer_init(
			state,
			*slot,
			center_x,
			center_y,
			target_x,
			target_y,
			player_id,
			0x5A
		);
		return;
	}
	if(character == 1) {
		const auto end = state.extra_attacks[player_id].begin() +
			extra_attack_mima_count;
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(), end,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == end) {
			return;
		}
		// ZUN uses the playfield width for both coordinates here, allowing
		// Mima's transfer target to extend well below the visible playfield.
		const auto target_x = static_cast<std::int16_t>(
			randring_next16(state) % (288U << 4U)
		);
		const auto target_y = static_cast<std::int16_t>(
			randring_next16(state) % (288U << 4U)
		);
		extra_attack_transfer_init(
			state, *slot, center_x, center_y, target_x, target_y, player_id, 0x5A
		);
		slot->mima_timer = 0U;
		slot->frame = 3U;
		return;
	}
	if(character == 2) {
		const auto end = state.extra_attacks[player_id].begin() +
			extra_attack_marisa_count;
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(), end,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == end) {
			return;
		}
		extra_attack_transfer_init(
			state,
			*slot,
			center_x,
			center_y,
			static_cast<std::int16_t>(
				randring_next16(state) % (288U << 4U)
			),
			368 << 4,
			player_id,
			0x6E
		);
		return;
	}
	if(character == 3) {
		const auto target_x = static_cast<std::int16_t>(
			(randring_next16(state) % (200U << 4U)) + (44U << 4U)
		);
		const auto target_y = static_cast<std::int16_t>(
			(randring_next16(state) % (200U << 4U)) + (80U << 4U)
		);
		const auto angle_delta = ((randring_next16(state) & 1U) == 0U)
			? std::int8_t{-1}
			: std::int8_t{1};
		const auto angle = static_cast<std::uint8_t>(randring_next16(state));
		bool spawned = false;
		for(std::size_t slot_id = 0U;
			slot_id < extra_attack_ellen_count; ++slot_id) {
			auto &slot = state.extra_attacks[player_id][slot_id];
			if(slot.state != 0U) {
				continue;
			}
			const auto trail_x = slot.ellen_trail_x;
			const auto trail_y = slot.ellen_trail_y;
			extra_attack_transfer_init(
				state,
				slot,
				center_x,
				center_y,
				target_x,
				target_y,
				player_id,
				0x46
			);
			slot.ellen_trail_x = trail_x;
			slot.ellen_trail_y = trail_y;
			slot.ellen_radius = 0U;
			slot.ellen_angle_delta = angle_delta;
			slot.angle = spawned
				? static_cast<std::uint8_t>(angle + 0x80U)
				: angle;
			if(spawned) {
				return;
			}
			spawned = true;
		}
		return;
	}
	if(character == 4) {
		const auto end = state.extra_attacks[player_id].begin() +
			extra_attack_regular_count;
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(), end,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == end) {
			return;
		}
		const auto target_x = static_cast<std::int16_t>(
			randring_next16(state) % (288U << 4U)
		);
		const auto target_y = static_cast<std::int16_t>(
			randring_next16(state) % (64U << 4U)
		);
		extra_attack_transfer_init(
			state,
			*slot,
			center_x,
			center_y,
			target_x,
			target_y,
			player_id,
			0x5A
		);
		slot->kotohime_target_y = static_cast<std::int16_t>(
			(randring_next16(state) & 4095U) + (96U << 4U)
		);
		slot->kotohime_velocity_y = static_cast<std::uint16_t>(
			(randring_next16(state) & 0x1FU) + 0x10U
		);
		slot->kotohime_milestone = 0U;
		slot->behavior_variant = 0U;
		return;
	}
	if(character == 5) {
		// Kana chooses this target even if all eight slots are occupied.
		const auto target_x = static_cast<std::int16_t>(
			randring_next16(state) % (288U << 4U)
		);
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(),
			state.extra_attacks[player_id].begin() + extra_attack_regular_count,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == state.extra_attacks[player_id].begin() + extra_attack_regular_count) {
			return;
		}
		extra_attack_transfer_init(
			state,
			*slot,
			center_x,
			center_y,
			target_x,
			static_cast<std::int16_t>(randring_next16(state) % (128U << 4U)),
			player_id,
			0x5A
		);
		slot->angle = static_cast<std::uint8_t>(
			(randring_next16(state) & 0x1FU) - 0x0FU
		);
		if(target_x >= (144 << 4)) {
			slot->angle = static_cast<std::uint8_t>(slot->angle + 0x80U);
		}
		return;
	}
	if(character == 6) {
		const auto end = state.extra_attacks[player_id].begin() +
			extra_attack_regular_count;
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(), end,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == end) {
			return;
		}
		extra_attack_transfer_init(
			state,
			*slot,
			center_x,
			center_y,
			static_cast<std::int16_t>(
				randring_next16(state) % (288U << 4U)
			),
			16 << 4,
			player_id,
			0x64
		);
		slot->behavior_variant = static_cast<std::uint8_t>(
			randring_next16(state) & 1U
		);
		slot->angle = 0x40U;
		slot->speed = static_cast<std::uint8_t>(
			(randring_next16(state) & 0x0FU) + 0x20U
		);
		return;
	}
	if(character == 7) {
		for(std::size_t slot_id = 0U; slot_id < extra_attack_count; slot_id += 2U) {
			auto &top = state.extra_attacks[player_id][slot_id];
			if(top.state != 0U) {
				continue;
			}
			auto &bottom = state.extra_attacks[player_id][slot_id + 1U];
			extra_attack_transfer_init(
				state,
				top,
				center_x,
				center_y,
				static_cast<std::int16_t>(
					(randring_next16(state) % (276U << 4U)) + 0x60U
				),
				0,
				player_id,
				0x50
			);
			extra_attack_transfer_init(
				state,
				bottom,
				center_x,
				center_y,
				static_cast<std::int16_t>(
					(randring_next16(state) % (276U << 4U)) + 0x60U
				),
				376 << 4,
				player_id,
				0x50
			);
			return;
		}
		return;
	}
	if(character == 8) {
		const auto end = state.extra_attacks[player_id].begin() +
			extra_attack_regular_count;
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(), end,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == end) {
			return;
		}
		// Turbo C++ evaluates these arguments from left to right. Keep the two
		// ring reads explicitly sequenced so host compiler argument evaluation
		// order cannot swap Yumemi's destination coordinates.
		const auto target_x = static_cast<std::int16_t>(
			randring_next16(state) % (288U << 4U)
		);
		const auto target_y = static_cast<std::int16_t>(
			(randring_next16(state) % (256U << 4U)) + (96U << 4U)
		);
		extra_attack_transfer_init(
			state,
			*slot,
			center_x,
			center_y,
			target_x,
			target_y,
			player_id,
			0x46
		);
		slot->yumemi_extent = 0U;
		slot->yumemi_duration = 0x60U;
	}
}

void chain_fire_charged_exatt(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t chain_slot,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	auto &charge = state.chains.charge_exatt[player_id][chain_slot];
	const auto threshold = static_cast<std::uint8_t>(
		6U - (state.round_speed / 32U)
	);
	if(charge < threshold) {
		return;
	}
	charge = 0U;
	extra_attack_add(state, center_x, center_y, player_id);
}

void fireball_add_if_charged(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t chain_slot,
	std::int16_t origin_x,
	std::int16_t origin_y,
	FireballVariant variant = FireballVariant::blue
) noexcept
{
	auto &charge = state.chains.charge_fireball[player_id][chain_slot];
	const auto threshold = static_cast<std::uint8_t>(
		12U - (state.round_speed / 16U)
	);
	if(charge < threshold) {
		return;
	}
	charge = 0U;
	(void)fireball_add(
		state, origin_x, origin_y, player_id, variant
	);
}

bool bullet_add_transfer_pellet(
	DeterministicState &state,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id,
	std::uint8_t template_speed
) noexcept
{
	if(!bullet_center_is_spawnable(center_x, center_y)) {
		return false;
	}
	auto *bullet = bullet_free_slot(state);
	if(bullet == nullptr) {
		return false;
	}
	bullet->flag = BulletFlag::transfer_pellet;
	bullet->age = 0U;
	bullet->center_x = center_x;
	bullet->center_y = center_y;
	bullet->player_id = player_id;
	bullet->group_next = bullet_group_random_constrained_aimed;
	bullet->speed_next = bullet_speed_adjusted(state, template_speed);
	bullet->angle_next = 0U;
	bullet->accel_type = 0U;
	bullet->is_collidable = true;
	bullet->has_trail = false;

	const auto origin_screen = playfield_x_to_screen_subpixel(
		state, center_x, player_id
	);
	const auto target_x = static_cast<std::int16_t>(
		randring_next16(state) % static_cast<std::uint16_t>(288U << 4U)
	);
	bullet->target_x_for_target_player = target_x;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto target_screen = playfield_x_to_screen_subpixel(
		state, target_x, target_player
	);
	const auto target_y = static_cast<std::int16_t>(
		randring_next16(state) & 0x00FFU
	);
	const auto travel_speed = static_cast<std::int16_t>(
		70U + (state.round_speed / 3U)
	);
	const auto velocity = vector2_between_plus(
		exact::wrap_shift_left(origin_screen, 4U),
		center_y,
		exact::wrap_shift_left(target_screen, 4U),
		target_y,
		0U,
		travel_speed
	);
	bullet->velocity_x = velocity.x;
	bullet->velocity_y = velocity.y;
	bullet->target_x_for_origin_player = screen_x_to_playfield_subpixel(
		target_screen, player_id
	);
	return true;
}

bool bullet_add_cloud_from_transfer(
	DeterministicState &state,
	const BulletState &source
) noexcept
{
	if(!bullet_center_is_spawnable(source.center_x, source.center_y)) {
		return false;
	}
	auto *bullet = bullet_free_slot(state);
	if(bullet == nullptr) {
		return false;
	}
	bullet->flag = BulletFlag::pellet_cloud;
	bullet->age = 0U;
	bullet->center_x = source.center_x;
	bullet->center_y = source.center_y;
	bullet->player_id = source.player_id;
	bullet->group_next = source.group_next;
	bullet->speed_next = bullet_speed_adjusted(state, source.speed_next);
	bullet->accel_type = 0U;
	bullet->is_collidable = true;
	bullet->has_trail = false;
	const auto angle_offset = static_cast<std::uint8_t>(
		(randring_next16(state) & 0x001FU) - 0x10U
	);
	bullet->angle_next = angle_offset;
	const auto &target = state.players[bullet->player_id];
	const auto velocity = vector2_between_plus(
		bullet->center_x,
		bullet->center_y,
		target.center_x.raw(),
		target.center_y.raw(),
		angle_offset,
		static_cast<std::int16_t>(bullet->speed_next)
	);
	bullet->velocity_x = velocity.x;
	bullet->velocity_y = velocity.y;
	return true;
}

void collmap_set_bullet(
	DeterministicState &state,
	const BulletState &bullet
) noexcept
{
	if(
		bullet.center_x < 0 || bullet.center_x >= (288 << 4) ||
		bullet.center_y < 0 || bullet.center_y >= (368 << 4)
	) {
		return;
	}
	const auto x = static_cast<std::size_t>(bullet.center_x >> 5U);
	const auto y = static_cast<std::size_t>(bullet.center_y >> 5U);
	state.collision_map[bullet.player_id][x][y] = true;
}

template <typename Callback>
void for_each_explosion(
	DeterministicState &state,
	Callback &&callback
) noexcept
{
	// Enemy and fireball objects share one 64-slot EFE arena in the original.
	// Preserve that physical order even though the native state stores the two
	// subclasses separately.
	for(auto &explosion : state.enemies) {
		callback(explosion);
	}
	for(auto &explosion : state.fireballs) {
		callback(explosion);
	}
}

template <typename Explosion>
bool explosion_overlaps(
	const Explosion &explosion,
	std::uint8_t player_id,
	std::int16_t left,
	std::int16_t right,
	std::int16_t top,
	std::int16_t bottom
) noexcept
{
	const auto flag = static_cast<std::uint8_t>(explosion.flag);
	if(
		flag < static_cast<std::uint8_t>(EnemyFlag::explosion_ignoring_enemies) ||
		explosion.player_id != player_id || (explosion.frame & 3U) != 0U
	) {
		return false;
	}
	const auto radius = static_cast<std::int16_t>(explosion.size_pixels * 6U);
	return !(
		right < exact::wrap_subtract(explosion.center_x, radius) ||
		bottom < exact::wrap_subtract(explosion.center_y, radius) ||
		left > exact::wrap_add(explosion.center_x, radius) ||
		top > exact::wrap_add(explosion.center_y, radius)
	);
}

template <typename Explosion>
void explosion_enemy_hit_increment(Explosion &explosion) noexcept
{
	using Flag = decltype(explosion.flag);
	explosion.flag = static_cast<Flag>(
		static_cast<std::uint8_t>(explosion.flag) + 1U
	);
}

std::uint8_t explosion_max_enemy_hits_half(
	const EnemyState &explosion
) noexcept
{
	return explosion.explosion_max_enemy_hits_half;
}

std::uint8_t explosion_max_enemy_hits_half(
	const FireballState &explosion
) noexcept
{
	// This byte is deliberately overlaid with the fireball variant in the
	// original EFE subclass.
	return explosion.variant_as_explosion_target;
}

bool pellet_explosions_hittest(
	DeterministicState &state,
	const BulletState &pellet
) noexcept
{
	constexpr std::int16_t pellet_radius = 6 << 4;
	const auto left = exact::wrap_subtract(pellet.center_x, pellet_radius);
	const auto right = exact::wrap_add(pellet.center_x, pellet_radius);
	const auto top = exact::wrap_subtract(pellet.center_y, pellet_radius);
	const auto bottom = exact::wrap_add(pellet.center_y, pellet_radius);
	bool collided = false;
	for_each_explosion(state, [&](auto &explosion) {
		if(!explosion_overlaps(
			explosion, pellet.player_id, left, right, top, bottom
		)) {
			return;
		}
		collided = true;
		const auto slot = explosion.chain_slot;
		auto &hits = state.chains.hits[pellet.player_id][slot];
		if(hits != 0xFFU) {
			hits++;
		}
		score_add(state.score[pellet.player_id], 1U);
		auto bonus = static_cast<std::uint16_t>(hits * 2U);
		bonus = static_cast<std::uint16_t>(bonus + state.round_speed);
		if(state.round_speed <= (3U << 4U)) {
			bonus >>= 1U;
		} else if(state.round_speed >= (6U << 4U)) {
			bonus = static_cast<std::uint16_t>(bonus + bonus);
		}
		auto &value = state.chains.pellet_and_fireball_value
			[pellet.player_id][slot];
		value++;
		gauge_avail_add(state, pellet.player_id, 4U);
		if(value < 4U) {
			state.chains.charge_fireball[pellet.player_id][slot] =
				static_cast<std::uint8_t>(
					state.chains.charge_fireball[pellet.player_id][slot] + 2U
				);
		} else if(value < 10U) {
			state.chains.charge_fireball[pellet.player_id][slot] =
				static_cast<std::uint8_t>(
					state.chains.charge_fireball[pellet.player_id][slot] + 5U
				);
		} else if(value < 20U) {
			state.chains.charge_fireball[pellet.player_id][slot] =
				static_cast<std::uint8_t>(
					state.chains.charge_fireball[pellet.player_id][slot] + 2U
				);
			state.chains.charge_exatt[pellet.player_id][slot]++;
		} else {
			state.chains.charge_exatt[pellet.player_id][slot] =
				static_cast<std::uint8_t>(
					state.chains.charge_exatt[pellet.player_id][slot] + 2U
				);
		}
		bonus = combo_add(state, pellet.player_id, hits, bonus);
		fire_point_based_boss_attack_or_panic(
			state, bonus, pellet.player_id, slot
		);
		fireball_add_if_charged(
			state,
			pellet.player_id,
			slot,
			explosion.center_x,
			explosion.center_y
		);
	});
	return collided;
}

void bullets_update(DeterministicState &state) noexcept
{
	for(auto &bullet : state.bullets) {
		if(bullet.flag == BulletFlag::free) {
			continue;
		}
		bullet.age++;
		if(bullet.flag == BulletFlag::pellet_cloud) {
			if((bullet.age & 3U) == 0U && bullet.age >= 12U) {
				bullet.flag = BulletFlag::pellet;
			}
			continue;
		}
		if(
			bullet.flag != BulletFlag::transfer_pellet &&
			(state.bomb_flag[bullet.player_id] != BombFlag::inactive ||
			 state.damage_all_on[bullet.player_id] != 0U)
		) {
			bullet.flag = BulletFlag::free;
			continue;
		}

		auto next_x = exact::wrap_add(
			bullet.center_x, bullet.velocity_x
		);
		if(bullet.flag == BulletFlag::transfer_pellet) {
			const auto reached = (bullet.player_id == 0U)
				? (next_x >= bullet.target_x_for_origin_player)
				: (next_x <= bullet.target_x_for_origin_player);
			if(reached) {
				bullet.flag = BulletFlag::pellet_cloud;
				bullet.player_id ^= 1U;
				bullet.center_x = bullet.target_x_for_target_player;
				bullet.center_y = 2 << 4;
				bullet.speed_next = static_cast<std::uint8_t>(
					bullet.speed_next + 8U
				);
				(void)bullet_add_cloud_from_transfer(state, bullet);
				bullet.flag = BulletFlag::free;
				continue;
			}
		} else if(bullet.has_trail) {
			if(next_x <= bullet_center_min) {
				next_x = bullet_center_min;
			} else if(next_x >= bullet_center_x_max) {
				next_x = bullet_center_x_max;
			}
			auto &trail = state.bullet_trails[bullet.trail_index];
			if(!bullet_trail_update_and_clip(
				trail.center_x,
				bullet.center_x,
				bullet_center_min,
				bullet_center_x_max
			)) {
				bullet.flag = BulletFlag::free;
				continue;
			}
		} else if(
			next_x <= bullet_center_min || next_x >= bullet_center_x_max
		) {
			bullet.flag = BulletFlag::free;
			continue;
		}
		bullet.center_x = next_x;

		const auto next_y = exact::wrap_add(
			bullet.center_y, bullet.velocity_y
		);
		if(bullet.has_trail) {
			auto &trail = state.bullet_trails[bullet.trail_index];
			if(!bullet_trail_update_and_clip(
				trail.center_y,
				bullet.center_y,
				bullet_center_min,
				bullet_center_y_max
			)) {
				bullet.flag = BulletFlag::free;
				continue;
			}
		} else if(next_y <= bullet_center_min || next_y >= bullet_center_y_max) {
			bullet.flag = BulletFlag::free;
			continue;
		}
		bullet.center_y = next_y;
		if(bullet.accel_type == 0x80U && bullet.velocity_y < (5 << 4)) {
			bullet.velocity_y = exact::wrap_add(
				bullet.velocity_y, static_cast<std::int16_t>(1)
			);
		}
		if(
			bullet.flag == BulletFlag::pellet &&
			pellet_explosions_hittest(state, bullet)
		) {
			if(
				state.gba_flag_active[bullet.player_id ^ 1U] ==
				GaugeBossAttackFlag::none
			) {
				(void)bullet_add_transfer_pellet(
					state,
					bullet.center_x,
					bullet.center_y,
					bullet.player_id,
					bullet.speed_next
				);
			}
			bullet.flag = BulletFlag::free;
			continue;
		}
		if(
			bullet.flag != BulletFlag::transfer_pellet &&
			bullet.is_collidable
		) {
			collmap_set_bullet(state, bullet);
		}
	}
}

void add_reimu_hyper_amulet(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept;

void shots_add(DeterministicState &state, std::uint8_t player_id) noexcept
{
	const auto &player = state.players[player_id];
	if(player.shot_mode == 0U) {
		return;
	}
	std::int16_t left = player.center_x.raw();
	std::int16_t top = player.center_y.raw();
	std::uint8_t pairs_fired = 0U;
	if(player.shot_mode == 3U) {
		left = exact::wrap_subtract(left, static_cast<std::int16_t>(64 << 4));
	} else if(player.shot_mode == 2U) {
		left = exact::wrap_subtract(left, static_cast<std::int16_t>(32 << 4));
		pairs_fired = 2U;
	} else if(player.shot_mode == 4U) {
		left = exact::wrap_subtract(left, static_cast<std::int16_t>(24 << 4));
		top = exact::wrap_subtract(top, static_cast<std::int16_t>(1 << 4));
		add_reimu_hyper_amulet(state, player_id, left, top);
		left = exact::wrap_add(left, static_cast<std::int16_t>(48 << 4));
		add_reimu_hyper_amulet(state, player_id, left, top);
		left = exact::wrap_subtract(
			player.center_x.raw(), static_cast<std::int16_t>(16 << 4)
		);
		pairs_fired = 3U;
	} else {
		left = exact::wrap_subtract(left, static_cast<std::int16_t>(16 << 4));
		pairs_fired = 3U;
	}
	while(left <= -(32 << 4)) {
		left = exact::wrap_add(left, static_cast<std::int16_t>(32 << 4));
		pairs_fired++;
	}
	for(auto &slot : state.shotpairs) {
		if(slot.alive) {
			continue;
		}
		slot.alive = true;
		slot.topleft_x = left;
		slot.topleft_y = top;
		slot.velocity_y = shot_velocity_y;
		slot.player_id = player_id;
		pairs_fired++;
		if(pairs_fired >= 4U) {
			return;
		}
		left = exact::wrap_add(left, static_cast<std::int16_t>(32 << 4));
		if(left >= (288 << 4)) {
			return;
		}
	}
}

void shots_update(DeterministicState &state) noexcept
{
	for(auto &shot : state.shotpairs) {
		if(!shot.alive) {
			continue;
		}
		shot.topleft_y = exact::wrap_add(shot.topleft_y, shot.velocity_y);
		if(shot.topleft_y <= -16) {
			shot.alive = false;
		}
	}
}

bool playfield_clip(
	std::int16_t x,
	std::int16_t y,
	std::int16_t negative_radius
) noexcept
{
	return (
		x <= negative_radius ||
		x >= exact::wrap_subtract(
			static_cast<std::int16_t>(288 << 4), negative_radius
		) ||
		y <= negative_radius ||
		y >= exact::wrap_subtract(
			static_cast<std::int16_t>(368 << 4), negative_radius
		)
	);
}

struct CpuTargetPoint {
	std::int16_t x = 144 << 4;
	std::int16_t y = 300 << 4;
};

CpuTargetPoint cpu_target_point(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	CpuTargetPoint target{};
	std::uint8_t target_id = 0xFFU;
	if(state.boss_target_active[player_id] != 0U) {
		target_id = 0xFEU;
		target.x = state.boss_target_x;
		target.y = state.boss_target_y;
	} else {
		for(const auto &enemy : state.enemies) {
			if(
				enemy.flag != EnemyFlag::running_spawned ||
				enemy.player_id != player_id ||
				enemy.center_x < 0 || enemy.center_x > (288 << 4) ||
				enemy.center_y < 0 || enemy.center_y > (384 << 4) ||
				enemy.formation_index >= target_id
			) {
				continue;
			}
			target_id = enemy.formation_index;
			target.x = static_cast<std::int16_t>(enemy.center_x - (16 << 4));
			target.y = static_cast<std::int16_t>(enemy.center_y + (110 << 4));
			if(target_id == 0U) {
				break;
			}
		}
	}
	state.cpu_target_kind_scratch = (target_id == 0xFFU)
		? static_cast<std::int8_t>(-1)
		: ((target_id == 0xFEU)
			? static_cast<std::int8_t>(-2)
			: static_cast<std::int8_t>(target_id));
	if(
		target.x <= 0 || target.x >= (288 << 4) ||
		target.y <= 0 || target.y >= (384 << 4)
	) {
		target.x = 144 << 4;
		target.y = 300 << 4;
	}
	return target;
}

void chargeshot_update_reimu(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	for(std::size_t group_id = 0U; group_id < 12U; ++group_id) {
		auto &group = state.reimu_charge[player_id][group_id];
		if(group.state == 0U) {
			continue;
		}
		state.players[player_id].gauge_charged = 0U;
		for(std::size_t point = group.x.size() - 1U; point > 0U; --point) {
			group.x[point] = group.x[point - 1U];
			group.y[point] = group.y[point - 1U];
		}
		const auto velocity = vector2(group.angle, group.speed);
		group.x[0] = exact::wrap_add(group.x[0], velocity.x);
		group.y[0] = exact::wrap_add(group.y[0], velocity.y);
		if(group.state == 1U) {
			if(group.speed > 0U) {
				group.speed = static_cast<std::uint8_t>(group.speed - 4U);
			} else if(group.frame >= 0x40U) {
				group.state = 2U;
				group.angle = static_cast<std::uint8_t>(
					(group_id < 2U) ? 0xB0U : 0xD0U
				);
				group.speed = 0xE0U;
			}
		}
		group.frame++;
		if(playfield_clip(group.x.back(), group.y.back(), -(24 << 4))) {
			group.state = 0U;
		}
	}
}

void chargeshot_update_mima(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	if(state.mima_charge_active[player_id] == 0U) {
		return;
	}
	auto &frames = state.mima_charge_frames[player_id];
	frames++;
	auto &nodes = state.mima_charge[player_id];
	for(std::size_t node_id = 0U; node_id < nodes.size(); ++node_id) {
		auto &node = nodes[node_id];
		if(node.active == 0U) {
			continue;
		}
		node.x = exact::wrap_add(node.x, node.velocity_x);
		node.y = exact::wrap_add(node.y, node.velocity_y);
		if(playfield_clip(node.x, node.y, -(16 << 4))) {
			node.active = 0U;
			// The last node clears the global flag even if earlier nodes remain.
			if(node_id == (nodes.size() - 1U)) {
				state.mima_charge_active[player_id] = 0U;
			}
		}
	}
	if(frames < 0x3CU && (frames % 4U) == 0U) {
		auto &node = nodes[frames / 4U];
		node.active = 1U;
		node.x = state.players[player_id].center_x.raw();
		node.y = state.players[player_id].center_y.raw();
	}
	state.players[player_id].gauge_charged = 0U;
}

void chargeshot_update_marisa(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.marisa_charge[player_id];
	if(shot.active == 0U) {
		return;
	}
	auto &player = state.players[player_id];
	player.gauge_charged = 0U;
	shot.age++;
	for(std::size_t node = shot.x.size() - 1U; node > 0U; --node) {
		shot.x[node] = shot.x[node - 1U];
		shot.y[node] = exact::wrap_add(
			shot.y[node - 1U], std::int16_t{0x18}
		);
	}
	if(shot.age < 0x18U) {
		shot.x[0] = player.center_x.raw();
		shot.y[0] = exact::wrap_subtract(
			player.center_y.raw(), std::int16_t{0x100}
		);
		return;
	}
	if(shot.age > 0x24U) {
		player.shot_active = 1U;
		shot.active = 0U;
	}
}

void chargeshot_update_kana(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto charge_state = state.kana_charge_state[player_id];
	if(charge_state == 0U) {
		return;
	}
	state.players[player_id].gauge_charged = 0U;
	for(auto &group : state.kana_charge[player_id]) {
		const auto velocity = vector2(group.angle, group.speed);
		for(std::size_t point = group.x.size() - 1U; point > 0U; --point) {
			group.x[point] = group.x[point - 1U];
			group.y[point] = group.y[point - 1U];
		}
		group.x[0] = exact::wrap_add(group.x[0], velocity.x);
		group.y[0] = exact::wrap_add(group.y[0], velocity.y);
		if(charge_state == 1U) {
			group.speed--;
		}
	}
	if(charge_state == 1U) {
		state.kana_charge[player_id][3].angle = static_cast<std::uint8_t>(
			state.kana_charge[player_id][3].angle - 2U
		);
		state.kana_charge[player_id][0].angle = static_cast<std::uint8_t>(
			state.kana_charge[player_id][0].angle + 2U
		);
		if(state.kana_charge_frames[player_id] > 0x28U) {
			state.kana_charge_state[player_id] = 2U;
			for(auto &group : state.kana_charge[player_id]) {
				group.speed = 0x80U;
			}
		}
	}
	if(state.kana_charge[player_id][0].y.back() <= -0x100) {
		state.kana_charge_state[player_id] = 0U;
	}
	state.kana_charge_frames[player_id]++;
}

void chargeshot_update_rikako(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto charge_state = state.rikako_charge_state[player_id];
	if(charge_state == 0U) {
		return;
	}
	state.players[player_id].gauge_charged = 0U;
	const auto frame = state.rikako_charge_frames[player_id];
	auto radius = state.rikako_charge_radius[player_id];
	if(charge_state == 1U) {
		state.rikako_charge_origin_y[player_id] = exact::wrap_subtract(
			state.rikako_charge_origin_y[player_id], std::int16_t{0x20}
		);
	} else {
		state.rikako_charge_origin_x[player_id] =
			state.players[player_id].center_x.raw();
		state.rikako_charge_origin_y[player_id] =
			state.players[player_id].center_y.raw();
	}

	for(std::size_t node_id = 0U; node_id < 4U; ++node_id) {
		auto &node = state.rikako_charge[player_id][node_id];
		const auto offset = vector2(
			node.angle, static_cast<std::int16_t>(radius)
		);
		node.x = exact::wrap_add(
			state.rikako_charge_origin_x[player_id], offset.x
		);
		node.y = exact::wrap_add(
			state.rikako_charge_origin_y[player_id], offset.y
		);
		const auto angle_delta = static_cast<std::uint8_t>(
			(node_id & 1U) != 0U ? 8U : 0xF8U
		);
		node.angle = static_cast<std::uint8_t>(node.angle + angle_delta);
	}
	if(frame < 0x20U) {
		radius = static_cast<std::uint16_t>(radius + 0x20U);
	} else if(charge_state == 1U && frame > 0x80U) {
		radius = static_cast<std::uint16_t>(radius + 0x60U);
		if(frame > 144U) {
			state.rikako_charge_state[player_id] = 0U;
		}
	} else if(charge_state != 1U) {
		state.rikako_charge_frames[player_id] = 0x20U;
	}
	state.rikako_charge_radius[player_id] = radius;
	state.rikako_charge_frames[player_id]++;
}

void chargeshot_update_chiyuri(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &nodes = state.chiyuri_charge[player_id];
	if(nodes.back().state == 0U) {
		return;
	}
	state.players[player_id].gauge_charged = 0U;
	for(std::size_t reverse_index = 0U; reverse_index < nodes.size(); ++reverse_index) {
		auto &node = nodes[nodes.size() - 1U - reverse_index];
		if(node.state == 0U) {
			return;
		}
		if(node.state == 1U) {
			node.y = exact::wrap_subtract(node.y, std::int16_t{14 << 4});
			if(node.y < -0x300) {
				if(reverse_index == 0U) {
					state.players[player_id].shot_active = 1U;
				}
				node.state = 0U;
				continue;
			}
		} else {
			node.state--;
			if(node.state == 1U) {
				node.x = state.players[player_id].center_x.raw();
				node.y = state.players[player_id].center_y.raw();
			}
		}
		node.frame++;
	}
}

void chargeshot_update_ellen(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &nodes = state.ellen_charge[player_id];
	if(std::none_of(nodes.begin(), nodes.end(), [](const auto &node) {
		return node.state != 0U;
	})) {
		return;
	}
	const auto target = cpu_target_point(state, player_id);
	const auto target_y = exact::wrap_subtract(
		target.y, static_cast<std::int16_t>(110 << 4)
	);
	auto &player = state.players[player_id];
	player.gauge_charged = 0U;
	const auto player_x = player.center_x.raw();
	const auto player_y = player.center_y.raw();
	for(auto &node : nodes) {
		if(node.state == 0U) {
			continue;
		}
		if(node.state == 1U) {
			node.x = exact::wrap_add(node.x, node.velocity_x);
			node.y = exact::wrap_add(node.y, node.velocity_y);
			if(node.phase != 0U) {
				if(playfield_clip(node.x, node.y, -(8 << 4))) {
					node.state = 0U;
				}
				continue;
			}
			node.speed = static_cast<std::uint8_t>(node.speed - 2U);
			const auto velocity = vector2(
				node.angle, static_cast<std::int16_t>(node.speed)
			);
			node.velocity_x = velocity.x;
			node.velocity_y = velocity.y;
			if(node.speed != 0U) {
				continue;
			}
			node.phase = 1U;
			node.angle = iatan2(
				exact::wrap_subtract(target_y, node.y),
				exact::wrap_subtract(target.x, node.x)
			);
			const auto homing_velocity = vector2(node.angle, 160);
			node.velocity_x = homing_velocity.x;
			node.velocity_y = homing_velocity.y;
			continue;
		}
		node.state--;
		node.x = player_x;
		node.y = player_y;
	}
}

void chargeshot_update_kotohime(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.kotohime_charge[player_id];
	if(shot.active == 0U) {
		return;
	}
	state.players[player_id].gauge_charged = 0U;
	shot.y = exact::wrap_add(shot.y, shot.velocity_y);
	if(shot.y <= -0x100) {
		shot.active = 0U;
		return;
	}
	shot.velocity_y = exact::wrap_subtract(
		shot.velocity_y, std::int16_t{2}
	);
}

void chargeshot_update_yumemi(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.yumemi_charge[player_id];
	if(shot.state == 0U) {
		return;
	}
	shot.frame = static_cast<std::uint8_t>(shot.frame + 1U);
	const auto frame = shot.frame;
	if(shot.state == 1U) {
		shot.y = exact::wrap_subtract(shot.y, std::int16_t{0x40});
		if(frame >= 0x40U || shot.y <= 0) {
			shot.state = 2U;
			shot.frame = 0U;
		}
	} else if(frame >= 0x40U) {
		shot.state = 0U;
	}
	state.players[player_id].gauge_charged = 0U;
}

void chargeshots_update(DeterministicState &state) noexcept
{
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		const auto character =
			(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
		if(character == 0) {
			chargeshot_update_reimu(state, player_id);
		} else if(character == 1) {
			chargeshot_update_mima(state, player_id);
		} else if(character == 2) {
			chargeshot_update_marisa(state, player_id);
		} else if(character == 3) {
			chargeshot_update_ellen(state, player_id);
		} else if(character == 4) {
			chargeshot_update_kotohime(state, player_id);
		} else if(character == 5) {
			chargeshot_update_kana(state, player_id);
		} else if(character == 6) {
			chargeshot_update_rikako(state, player_id);
		} else if(character == 7) {
			chargeshot_update_chiyuri(state, player_id);
		} else if(character == 8) {
			chargeshot_update_yumemi(state, player_id);
		}
	}
}

std::uint8_t chargeshot_hittest(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t left,
	std::int16_t right,
	std::int16_t top,
	std::int16_t bottom,
	bool *one_hit = nullptr
) noexcept
{
	if(one_hit != nullptr) {
		*one_hit = false;
	}
	const auto character =
		(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
	if(character == 0) {
		if(
			state.reimu_chargeshot_primary != 0U &&
			(state.round_or_result_frame & 1U) != 0U
		) {
			return 0U;
		}
		for(std::size_t group_id = 0U; group_id < 6U; ++group_id) {
			const auto &group = state.reimu_charge[player_id][group_id];
			if(
				group.state == 0U || group.x[0] <= -(16 << 4) ||
				group.x[0] >= (288 << 4) || group.y[0] <= -(16 << 4)
			) {
				continue;
			}
			if(
				exact::wrap_subtract(group.x[0], right) <= (20 << 4) &&
				exact::wrap_subtract(left, group.x[0]) <= (20 << 4) &&
				exact::wrap_subtract(group.y[0], bottom) <= (20 << 4) &&
				exact::wrap_subtract(top, group.y[0]) <= (20 << 4)
			) {
				// hitcircles_enemy_add() randomly centers the visual inside this
				// hitbox. Those two presentation-owned draws share the gameplay
				// ring and therefore remain part of deterministic call order.
				(void)randring_next16(state);
				(void)randring_next16(state);
				return 1U;
			}
		}
		return 0U;
	}
	if(character == 1) {
		if(state.mima_charge_active[player_id] == 0U) {
			return 0U;
		}
		std::uint8_t hits = 0U;
		for(const auto &node : state.mima_charge[player_id]) {
			if(
				node.active == 0U ||
				exact::wrap_add(node.x, std::int16_t{-(16 << 4)}) > right ||
				exact::wrap_add(node.x, std::int16_t{16 << 4}) < left ||
				node.y < top || node.y > bottom
			) {
				continue;
			}
			(void)randring_next16(state);
			(void)randring_next16(state);
			hits++;
		}
		return hits;
	}
	if(character == 2) {
		const auto &shot = state.marisa_charge[player_id];
		if(shot.active == 0U) {
			return 0U;
		}
		for(std::size_t node = 0U; node < shot.x.size(); node += 4U) {
			if(
				shot.x[node] >= left && shot.x[node] <= right &&
				shot.y[node] >= top
			) {
				(void)randring_next16(state);
				(void)randring_next16(state);
				return 1U;
			}
		}
		return 0U;
	}
	if(character == 3) {
		std::uint8_t hits = 0U;
		for(auto &node : state.ellen_charge[player_id]) {
			if(
				node.state != 1U || node.x < left || node.x > right ||
				node.y < top || node.y > bottom
			) {
				continue;
			}
			(void)randring_next16(state);
			(void)randring_next16(state);
			node.state = 0U;
			hits = static_cast<std::uint8_t>(hits + 2U);
		}
		return hits;
	}
	if(character == 4) {
		const auto &shot = state.kotohime_charge[player_id];
		if(
			shot.active != 0U &&
			exact::wrap_subtract(shot.x, right) <= (40 << 4) &&
			exact::wrap_subtract(left, shot.x) <= (40 << 4) &&
			shot.y >= top && shot.y <= bottom
		) {
			(void)randring_next16(state);
			(void)randring_next16(state);
			if(one_hit != nullptr) {
				*one_hit = true;
			}
			return 3U;
		}
		return 0U;
	}
	if(character == 5 && state.kana_charge_state[player_id] != 0U) {
		for(const auto &group : state.kana_charge[player_id]) {
			for(std::size_t point = 0U; point <= 12U; point += 4U) {
				if(
					exact::wrap_subtract(group.x[point], right) <= (12 << 4) &&
					exact::wrap_subtract(left, group.x[point]) <= (12 << 4) &&
					exact::wrap_subtract(group.y[point], bottom) <= (12 << 4) &&
					exact::wrap_subtract(top, group.y[point]) <= (12 << 4)
				) {
					(void)randring_next16(state);
					(void)randring_next16(state);
					return 1U;
				}
			}
		}
	}
	if(character == 6 && state.rikako_charge_state[player_id] != 0U) {
		std::uint8_t hits = 0U;
		for(const auto &node : state.rikako_charge[player_id]) {
			if(
				exact::wrap_subtract(node.x, right) <= (12 << 4) &&
				exact::wrap_subtract(left, node.x) <= (12 << 4) &&
				exact::wrap_subtract(node.y, bottom) <= (12 << 4) &&
				exact::wrap_subtract(top, node.y) <= (12 << 4)
			) {
				(void)randring_next16(state);
				(void)randring_next16(state);
				hits++;
			}
		}
		return hits;
	}
	if(character == 7) {
		std::uint8_t hits = 0U;
		const auto &nodes = state.chiyuri_charge[player_id];
		for(std::size_t reverse_index = 0U; reverse_index < nodes.size(); ++reverse_index) {
			const auto &node = nodes[nodes.size() - 1U - reverse_index];
			if(node.state == 0U) {
				break;
			}
			if(node.state != 1U) {
				continue;
			}
			if(
				exact::wrap_subtract(node.x, right) <= (14 << 4) &&
				exact::wrap_subtract(left, node.x) <= (14 << 4) &&
				exact::wrap_subtract(node.y, bottom) <= (32 << 4) &&
				exact::wrap_subtract(top, node.y) <= -(16 << 4)
			) {
				(void)randring_next16(state);
				(void)randring_next16(state);
				hits++;
			}
		}
		return hits;
	}
	if(character == 8) {
		auto &shot = state.yumemi_charge[player_id];
		if(shot.state == 0U) {
			return 0U;
		}
		const auto overlaps = [&](std::int16_t radius) {
			return !(
				exact::wrap_subtract(shot.x, radius) > right ||
				exact::wrap_add(shot.x, radius) < left ||
				exact::wrap_subtract(shot.y, radius) > bottom ||
				exact::wrap_add(shot.y, radius) < top
			);
		};
		if(shot.state == 1U) {
			if(overlaps(8 << 4)) {
				// ZUN turns the projectile into its large explosion but reports a
				// miss to this caller on the triggering collision frame.
				shot.state = 2U;
				shot.frame = 0U;
			}
			return 0U;
		}
		if(!overlaps(56 << 4)) {
			return 0U;
		}
		// hitcircles_enemy_add() consumes two shared-ring values to choose
		// the visual center. They remain replay-visible in the headless core.
		(void)randring_next16(state);
		(void)randring_next16(state);
		return 1U;
	}
	return 0U;
}

MoveResult movement_velocity(
	const PlayerState &player,
	std::uint16_t input,
	Velocity &velocity
) noexcept
{
	auto set = [&](std::int8_t x, std::int8_t y) {
		const bool diagonal = x != 0 && y != 0;
		auto speed_x = diagonal
			? player.speed_diagonal_x
			: player.speed_aligned_x;
		auto speed_y = diagonal
			? player.speed_diagonal_y
			: player.speed_aligned_y;
		const auto character =
			(static_cast<int>(player.paletted_character) - 1) / 2;
		if(player.hyper_active != 0U && player.hyper_effect_active_this_frame) {
			if(
				character == 0 || character == 1 || character == 4 ||
				character == 5 || character == 7 || character == 8
			) {
				const auto boost = static_cast<std::uint8_t>(diagonal ? 24U : 32U);
				speed_x = static_cast<std::uint8_t>(speed_x + boost);
				speed_y = static_cast<std::uint8_t>(speed_y + boost);
			} else if(character == 2) {
				const auto penalty = static_cast<std::uint8_t>(diagonal ? 12U : 16U);
				speed_x = static_cast<std::uint8_t>(speed_x - penalty);
				speed_y = static_cast<std::uint8_t>(speed_y - penalty);
			} else if(character == 6) {
				const auto penalty = static_cast<std::uint8_t>(diagonal ? 8U : 16U);
				speed_x = static_cast<std::uint8_t>(speed_x - penalty);
				speed_y = static_cast<std::uint8_t>(speed_y - penalty);
			}
		}
		velocity.x = static_cast<std::int16_t>(static_cast<int>(speed_x) * x);
		velocity.y = static_cast<std::int16_t>(static_cast<int>(speed_y) * y);
	};

	switch(input) {
	case input_left: set(-1, 0); break;
	case input_down | input_left:
	case input_down_left: set(-1, 1); break;
	case input_down: set(0, 1); break;
	case input_down | input_right:
	case input_down_right: set(1, 1); break;
	case input_right: set(1, 0); break;
	case input_up | input_right:
	case input_up_right: set(1, -1); break;
	case input_up: set(0, -1); break;
	case input_up | input_left:
	case input_up_left: set(-1, -1); break;
	case 0U: return MoveResult::no_input;
	default: return MoveResult::invalid;
	}
	return MoveResult::valid;
}

void apply_velocity(PlayerState &player, const Velocity &velocity) noexcept
{
	const auto x = exact::wrap_add(player.center_x.raw(), velocity.x);
	const auto y = exact::wrap_add(player.center_y.raw(), velocity.y);
	player.center_x = exact::Subpixel::from_raw(
		std::clamp(x, player_x_min, player_x_max)
	);
	player.center_y = exact::Subpixel::from_raw(
		std::clamp(y, player_y_min, player_y_max)
	);
}

void move_human(PlayerState &player, std::uint16_t live_input) noexcept
{
	auto movement = static_cast<std::uint16_t>(live_input & input_movement);
	bool use_previous = true;
	Velocity velocity{};
	for(;;) {
		const auto result = movement_velocity(player, movement, velocity);
		if(result == MoveResult::invalid && use_previous) {
			if(player.human_movement_last != movement) {
				movement = static_cast<std::uint16_t>(
					movement & static_cast<std::uint16_t>(~player.human_movement_last)
				);
				use_previous = false;
				continue;
			}
			return;
		}
		if(result == MoveResult::valid) {
			apply_velocity(player, velocity);
		}
		if(use_previous) {
			player.human_movement_last = movement;
		}
		return;
	}
}

void add_reimu_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	auto angle = std::uint8_t{0x90U};
	for(std::size_t group_id = 0U; group_id < 4U; ++group_id) {
		auto &group = state.reimu_charge[player_id][group_id];
		group.state = 1U;
		group.frame = 0U;
		group.x.fill(center_x);
		group.y.fill(center_y);
		group.angle = angle;
		group.speed = 0x60U;
		angle = static_cast<std::uint8_t>(angle + 0x20U);
	}
	state.reimu_chargeshot_primary = 1U;
}

void add_reimu_hyper_amulet(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	for(auto &group : state.reimu_charge[player_id]) {
		if(group.state != 0U) {
			continue;
		}
		group.state = 2U;
		group.frame = 0U;
		group.x.fill(center_x);
		group.y.fill(center_y);
		group.angle = 0xC0U;
		group.speed = 0xC0U;
		break;
	}
	state.reimu_chargeshot_primary = 0U;
}

void add_mima_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	constexpr std::array<std::uint8_t, 6> angles{
		192U, 182U, 202U, 192U, 176U, 208U,
	};
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	state.mima_charge_active[player_id] = 1U;
	state.mima_charge_frames[player_id] = 0U;
	auto &nodes = state.mima_charge[player_id];
	for(std::size_t node_id = 0U; node_id < nodes.size(); ++node_id) {
		auto &node = nodes[node_id];
		node.active = (node_id == 0U) ? 1U : 0U;
		node.visual_frame = 0U;
		node.x = center_x;
		node.y = center_y;
		const auto velocity = vector2(angles[node_id % angles.size()], 176);
		node.velocity_x = velocity.x;
		node.velocity_y = velocity.y;
	}
}

void marisa_charge_initialize(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.marisa_charge[player_id];
	shot.active = 1U;
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = exact::wrap_subtract(
		state.players[player_id].center_y.raw(), std::int16_t{0x100}
	);
	shot.x.fill(center_x);
	shot.y.fill(center_y);
}

void add_marisa_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	marisa_charge_initialize(state, player_id);
	state.marisa_charge[player_id].age = 0U;
	state.players[player_id].shot_active = 0U;
}

void activate_marisa_hyper(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.marisa_charge[player_id];
	if(shot.active == 0U) {
		marisa_charge_initialize(state, player_id);
	}
	shot.age = 0x10U;
	state.players[player_id].shot_active = 0U;
}

void add_ellen_hyper_node(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	for(auto &node : state.ellen_charge[player_id]) {
		if(node.state != 0U) {
			continue;
		}
		node.state = 1U;
		node.phase = 0U;
		node.x = state.players[player_id].center_x.raw();
		node.y = state.players[player_id].center_y.raw();
		node.angle = static_cast<std::uint8_t>(randring_next16(state));
		node.speed = 0x50U;
		return;
	}
}

void add_kana_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	state.kana_charge_state[player_id] = 1U;
	state.kana_charge_frames[player_id] = 0U;
	constexpr std::array<std::uint8_t, 4> angles{0x90U, 0xB8U, 0xC8U, 0xF0U};
	for(std::size_t group_id = 0U; group_id < 4U; ++group_id) {
		auto &group = state.kana_charge[player_id][group_id];
		group.x.fill(center_x);
		group.y.fill(center_y);
		group.angle = angles[group_id];
		group.speed = 0x30U;
	}
}

void add_rikako_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	state.rikako_charge_state[player_id] = 1U;
	state.rikako_charge_frames[player_id] = 0U;
	state.rikako_charge_radius[player_id] = 0x80U;
	auto angle = std::uint8_t{0x20U};
	for(auto &node : state.rikako_charge[player_id]) {
		node.x = center_x;
		node.y = center_y;
		node.angle = angle;
		angle = static_cast<std::uint8_t>(angle + 0x40U);
	}
	state.rikako_charge_origin_x[player_id] = center_x;
	state.rikako_charge_origin_y[player_id] = center_y;
}

void add_chiyuri_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &nodes = state.chiyuri_charge[player_id];
	nodes[0].frame = 0U;
	nodes[0].x = state.players[player_id].center_x.raw();
	nodes[0].y = state.players[player_id].center_y.raw();
	state.players[player_id].shot_active = 0U;
	for(std::size_t node_id = 0U; node_id < nodes.size(); ++node_id) {
		nodes[node_id].state = static_cast<std::uint8_t>((node_id * 4U) + 2U);
	}
}

void add_ellen_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	auto &nodes = state.ellen_charge[player_id];
	for(std::size_t node_id = 0U; node_id < nodes.size(); ++node_id) {
		auto &node = nodes[node_id];
		node.state = static_cast<std::uint8_t>(node_id * 2U);
		node.phase = 0U;
		node.x = center_x;
		node.y = center_y;
		node.angle = static_cast<std::uint8_t>(randring_next16(state));
		node.speed = 0x50U;
	}
}

void add_kotohime_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.kotohime_charge[player_id];
	shot.active = 1U;
	shot.x = state.players[player_id].center_x.raw();
	shot.y = state.players[player_id].center_y.raw();
	shot.velocity_y = -0x10;
}

void add_yumemi_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.yumemi_charge[player_id];
	shot.state = 1U;
	shot.frame = 0U;
	shot.x = state.players[player_id].center_x.raw();
	shot.y = state.players[player_id].center_y.raw();
}

void release_gauge(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &player = state.players[player_id];
	const auto charged = player.gauge_charged;
	if(charged >= gauge_64) {
		const auto character =
			(static_cast<int>(player.paletted_character) - 1) / 2;
		if(character == 0) {
			add_reimu_chargeshot(state, player_id);
		} else if(character == 1) {
			add_mima_chargeshot(state, player_id);
		} else if(character == 2) {
			add_marisa_chargeshot(state, player_id);
		} else if(character == 3) {
			add_ellen_chargeshot(state, player_id);
		} else if(character == 4) {
			add_kotohime_chargeshot(state, player_id);
		} else if(character == 5) {
			add_kana_chargeshot(state, player_id);
		} else if(character == 6) {
			add_rikako_chargeshot(state, player_id);
		} else if(character == 7) {
			add_chiyuri_chargeshot(state, player_id);
		} else if(character == 8) {
			add_yumemi_chargeshot(state, player_id);
		}
		if(charged >= gauge_128) {
			state.warning_flag[player_id] = WarningFlag::portrait;
			if(charged >= gauge_max) {
				if(state.gba_boss_launched_by == static_cast<std::int8_t>(player_id)) {
					player.gauge_attacks_fired++;
					state.gba_flag_next[player_id] =
						GaugeBossAttackFlag::gauge_bullet_init;
					player.gauge_avail = static_cast<std::uint16_t>(
						player.gauge_avail - gauge_128
					);
					if(state.gba_gauge_level[player_id] < 16U) {
						state.gba_gauge_level[player_id]++;
					}
				} else {
					state.gba_flag_next[player_id] = GaugeBossAttackFlag::boss;
					player.gauge_avail = gauge_64;
					if(state.gba_boss_level < 16U) {
						state.gba_boss_level++;
					}
					state.combo_points_for_boss_attack =
						static_cast<std::uint16_t>(
							state.combo_points_for_boss_attack + 10240U
						);
					if(state.gba_boss_launched_by < 0) {
						player.boss_attacks_fired++;
					} else {
						player.boss_attacks_reversed++;
					}
				}
			} else if(charged >= gauge_192) {
				player.gauge_attacks_fired++;
				state.gba_flag_next[player_id] =
					GaugeBossAttackFlag::gauge_bullet_init;
				player.gauge_avail = static_cast<std::uint16_t>(
					player.gauge_avail - gauge_128
				);
				if(state.gba_gauge_level[player_id] < 16U) {
					state.gba_gauge_level[player_id]++;
				}
			} else {
				player.gauge_attacks_fired++;
				state.gba_flag_next[player_id] =
					GaugeBossAttackFlag::gauge_pellet_init;
				player.gauge_avail = static_cast<std::uint16_t>(
					player.gauge_avail - gauge_64
				);
				if(state.gba_gauge_level[player_id] < 16U) {
					state.gba_gauge_level[player_id]++;
				}
			}
		}
		player.invincibility_time = static_cast<std::uint8_t>(
			player.invincibility_time + 6U
		);
	}
	player.gauge_charged = 0U;
}

void player_bomb(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &player = state.players[player_id];
	if(
		state.bomb_flag[player_id] != BombFlag::inactive ||
		player.hyper_active != 0U
	) {
		return;
	}
	if(player.bombs != 0U) {
		state.bomb_flag[player_id] = BombFlag::preparing;
		player.bombs--;
		player.invincibility_time = bomb_frames;
		if(player_id == 0U && state.skill > 0U) {
			state.skill--;
		}
		return;
	}
	if(player.gauge_avail < gauge_max) {
		return;
	}
	state.damage_all_on[player_id] = 1U;
	player.hyper_active = static_cast<std::uint8_t>(player.paletted_character);
	if(player_id == 0U && state.skill > 0U) {
		state.skill--;
	}
}

void bomb_character_update(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.bomb_flag[player_id];
	if(flag == BombFlag::inactive) {
		return;
	}
	const auto character =
		(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
	if(flag == BombFlag::preparing) {
		flag = BombFlag::active;
		state.bomb_frame[player_id] = 0U;
		if(character == 3) {
			for(auto &vector : state.ellen_bomb_vectors[player_id]) {
				vector.x = ellen_bomb_vector_inactive;
			}
		}
	}
	state.bomb_frame[player_id]++;
	if(
		character == 1 && state.bomb_frame[player_id] <= 64U &&
		(state.bomb_frame[player_id] % 8U) == 0U
	) {
		// mima_17043() chooses each visual column from the shared random ring.
		(void)randring_next16(state);
	}
	if(character == 3) {
		for(auto &vector : state.ellen_bomb_vectors[player_id]) {
			bool respawn = (vector.x == ellen_bomb_vector_free);
			if(vector.x == ellen_bomb_vector_inactive) {
				continue;
			}
			if(!respawn) {
				vector.x = exact::wrap_add(vector.x, vector.velocity_x);
				vector.y = exact::wrap_add(vector.y, vector.velocity_y);
				respawn = playfield_clip(
					vector.x, vector.y, static_cast<std::int16_t>(-(32 << 4))
				);
			}
			if(respawn) {
				vector.x = 144 << 4;
				vector.y = 184 << 4;
				const auto velocity = vector2(
					static_cast<std::uint8_t>(randring_next16(state)), 224
				);
				vector.velocity_x = velocity.x;
				vector.velocity_y = velocity.y;
			}
		}
	}
	if(state.bomb_frame[player_id] >= bomb_frames) {
		flag = BombFlag::inactive;
	}
}

void bombs_render_phase_update(DeterministicState &state) noexcept
{
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		if(state.bomb_flag[player_id] == BombFlag::inactive) {
			continue;
		}
		const auto character =
			(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
		if(character == 0 && state.bomb_frame[player_id] < 32U) {
			// reimu_bomb() creates one visual record with two shared-ring draws.
			(void)randring_next16(state);
			(void)randring_next16(state);
		}
		if(character == 0) {
			const auto frame = state.bomb_frame[player_id];
			if(frame >= 64U && frame < 112U) {
				state.playfield_fg_shift_x[player_id] =
					(((frame % 8U) & 3U) < 2U) ? 4 : -4;
			} else if(frame >= 112U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == 1) {
			const auto frame = state.bomb_frame[player_id];
			if(frame > 80U && frame < 144U) {
				state.playfield_fg_shift_x[player_id] =
					(((frame % 8U) & 3U) < 2U) ? 4 : -4;
			} else if(frame >= 144U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == 2) {
			const auto frame = state.bomb_frame[player_id];
			if(frame >= 64U && frame < 128U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
				if((frame % 8U) == 0U) {
					(void)randring_next16(state);
					(void)randring_next16(state);
				}
			} else if(frame >= 128U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == 3) {
			const auto frame = state.bomb_frame[player_id];
			if(frame < 64U) {
				state.ellen_bomb_vector_cursor = 0U;
			} else if(frame < 128U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
				if(
					(frame % 4U) == 0U &&
					state.ellen_bomb_vector_cursor < 8U
				) {
					state.ellen_bomb_vectors[player_id]
						[state.ellen_bomb_vector_cursor].x =
							ellen_bomb_vector_free;
					state.ellen_bomb_vector_cursor++;
				}
			} else {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == 6) {
			const auto frame = state.bomb_frame[player_id];
			if(frame >= 64U && frame < 128U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
				if((frame % 8U) == 0U) {
					(void)randring_next16(state);
				}
			} else if(frame >= 128U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == 7) {
			const auto frame = state.bomb_frame[player_id];
			if(frame >= 64U && frame < 144U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
				if((frame % 16U) == 0U) {
					// Four SUB_CE5B() visuals share the gameplay random ring.
					for(std::size_t effect = 0U; effect < 4U; ++effect) {
						(void)randring_next16(state);
					}
				}
			} else if(frame >= 144U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == 8) {
			const auto frame = state.bomb_frame[player_id];
			if(frame > 64U && frame < 144U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
				if((frame & 7U) == 0U) {
					// Yumemi's periodic SUB_CE5B() visual consumes one ring value.
					(void)randring_next16(state);
				}
			} else if(frame >= 144U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == 4 || character == 5) {
			const auto frame = state.bomb_frame[player_id];
			if(frame >= 64U && frame < 128U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
			} else if(frame >= 128U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
	}
}

void gauge_pattern_reimu(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.reimu_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x20U
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			0x20U - state.gba_gauge_level[player_id]
		);
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto frame = state.reimu_gauge_pattern_frame[player_id];
	if((frame % state.gauge_pattern_param_b[player_id]) == 0U) {
		bullets_add_group(
			state,
			type,
			32 << 4,
			8 << 4,
			target_player,
			2 << 4,
			0U,
			bullet_group_five_spread_medium_aimed
		);
		bullets_add_group(
			state,
			type,
			256 << 4,
			8 << 4,
			target_player,
			2 << 4,
			0U,
			bullet_group_five_spread_medium_aimed
		);
	}
	if((frame % 0x20U) == 0x10U) {
		bullets_add_group(
			state,
			type,
			64 << 4,
			8 << 4,
			target_player,
			(3 << 4) + 2,
			0U,
			bullet_group_ring,
			state.gauge_pattern_param_a[player_id]
		);
		bullets_add_group(
			state,
			type,
			(288 - 64) << 4,
			8 << 4,
			target_player,
			(3 << 4) + 2,
			0U,
			bullet_group_ring,
			state.gauge_pattern_param_a[player_id]
		);
	}
	if(frame > 0x40U) {
		flag = GaugeBossAttackFlag::none;
	}
	state.reimu_gauge_pattern_frame[player_id]++;
}

void gauge_pattern_marisa(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.marisa_gauge_pattern_y[player_id] = static_cast<std::int16_t>(
			- static_cast<std::int16_t>((randring_next16(state) & 0x1FU) << 4U)
		);
		state.marisa_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			0x30U - state.gba_gauge_level[player_id]
		);
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet
		: BulletFlag::bullet16;
	auto &frame = state.marisa_gauge_pattern_frame[player_id];
	auto &center_y = state.marisa_gauge_pattern_y[player_id];
	const auto phase = static_cast<std::uint8_t>(frame % 0x18U);
	if(phase == 0U || phase == 0x0CU) {
		auto speed = std::uint8_t{(3U << 4U) + 8U};
		const auto center_x = static_cast<std::int16_t>(
			(phase == 0U) ? 0 : (288 << 4)
		);
		const auto angle = static_cast<std::uint8_t>(
			(phase == 0U) ? 0x20U : 0x60U
		);
		for(std::size_t shot = 0U; shot < 5U; ++shot) {
			bullets_add_group(
				state,
				type,
				center_x,
				center_y,
				static_cast<std::uint8_t>(1U - player_id),
				speed,
				angle,
				bullet_group_one
			);
			speed = static_cast<std::uint8_t>(speed - 6U);
		}
		center_y = exact::wrap_add(
			center_y,
			static_cast<std::int16_t>(
				static_cast<std::uint16_t>(
					state.gauge_pattern_param_a[player_id]
				) << 4U
			)
		);
	}
	if(center_y >= (368 << 4)) {
		flag = GaugeBossAttackFlag::none;
	}
	frame++;
}

void gauge_pattern_kotohime(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? BulletFlag::pellet : BulletFlag::bullet16;
		state.kotohime_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			(state.gba_gauge_level[player_id] / 2U) + 4U
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			randring_next16(state) & 1U
		);
		state.kotohime_gauge_pattern_type[player_id] = type;
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	auto &frame = state.kotohime_gauge_pattern_frame[player_id];
	if(frame == 0U) {
		auto *slot = &state.extra_attacks[player_id][extra_attack_regular_count];
		if(slot->state != 0U) {
			slot = &state.extra_attacks[player_id][extra_attack_regular_count + 1U];
		}
		const auto target_x = static_cast<std::int16_t>(
			(randring_next16(state) % (160U << 4U)) + (64U << 4U)
		);
		extra_attack_transfer_init(
			state,
			*slot,
			state.players[player_id].center_x.raw(),
			state.players[player_id].center_y.raw(),
			target_x,
			0,
			player_id,
			0x78
		);
		slot->kotohime_target_y = 0x0C00;
		slot->kotohime_velocity_y = 0x20U;
		slot->kotohime_milestone = 0U;
		slot->behavior_variant = 1U;
	} else if(frame >= 0x80U) {
		flag = GaugeBossAttackFlag::none;
	}
	frame++;
}

void gauge_pattern_kana(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.kana_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.kana_gauge_pattern_x[player_id] = 0;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			8U - (state.gba_gauge_level[player_id] / 4U)
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x30U
		);
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto frame = state.kana_gauge_pattern_frame[player_id];
	auto &center_x = state.kana_gauge_pattern_x[player_id];
	if((frame % state.gauge_pattern_param_a[player_id]) == 0U) {
		bullets_add_group(
			state,
			type,
			center_x,
			0,
			target_player,
			state.gauge_pattern_param_b[player_id],
			0x40U,
			bullet_group_one
		);
		bullets_add_group(
			state,
			type,
			static_cast<std::int16_t>((288 << 4) - center_x),
			0,
			target_player,
			state.gauge_pattern_param_b[player_id],
			0x40U,
			bullet_group_one
		);
	}
	if((frame % 0x40U) < 0x20U) {
		center_x = exact::wrap_add(center_x, static_cast<std::int16_t>(0x80));
	} else {
		center_x = exact::wrap_subtract(
			center_x, static_cast<std::int16_t>(0x80)
		);
	}
	if(frame >= 0x80U) {
		flag = GaugeBossAttackFlag::none;
	}
	state.kana_gauge_pattern_frame[player_id]++;
}

void gauge_pattern_mima(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.mima_gauge_pattern_angle_delta[player_id] =
			((randring_next16(state) & 1U) == 0U) ? -7 : 7;
		state.mima_gauge_pattern_angle[player_id] = 0x40U;
		state.mima_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x26U
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			(state.gba_gauge_level[player_id] * 2U) + 0x18U
		);
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	auto &frame = state.mima_gauge_pattern_frame[player_id];
	if((frame % 8U) == 0U) {
		const auto center_x =
			(static_cast<std::uint8_t>(
				state.mima_gauge_pattern_angle_delta[player_id]
			) > 7U)
			? static_cast<std::int16_t>(32 << 4)
			: static_cast<std::int16_t>(256 << 4);
		const auto angle = state.mima_gauge_pattern_angle[player_id];
		bullets_add_group(
			state, type, center_x, 16 << 4, target_player,
			state.gauge_pattern_param_b[player_id], angle,
			bullet_group_five_spread_narrow
		);
		if((frame % 0x10U) != 0U) {
			bullets_add_group(
				state, type, center_x, 16 << 4, target_player,
				(3 << 4) + 6, angle, bullet_group_ring,
				state.gauge_pattern_param_a[player_id]
			);
		}
		state.mima_gauge_pattern_angle[player_id] =
			static_cast<std::uint8_t>(
				angle + state.mima_gauge_pattern_angle_delta[player_id]
			);
	}
	if(frame > 0x40U) {
		flag = GaugeBossAttackFlag::none;
	}
	frame++;
}

void gauge_pattern_ellen(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.ellen_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.ellen_gauge_pattern_x[player_id] = 144 << 4;
		state.ellen_gauge_pattern_y[player_id] = 0;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x0EU
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x1CU
		);
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	auto &frame = state.ellen_gauge_pattern_frame[player_id];
	if((frame % 8U) != 0U) {
		frame++;
		return;
	}
	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	auto &center_x = state.ellen_gauge_pattern_x[player_id];
	if(center_x > 0) {
		bullets_add_group(
			state, type, center_x, 0, target_player, (3 << 4) + 8, 0U,
			bullet_group_ring_aimed, state.gauge_pattern_param_a[player_id]
		);
		bullets_add_group(
			state, type, static_cast<std::int16_t>((288 << 4) - center_x),
			0, target_player, (3 << 4) + 8, 0U,
			bullet_group_ring_aimed, state.gauge_pattern_param_a[player_id]
		);
		center_x = exact::wrap_subtract(
			center_x, static_cast<std::int16_t>(24 << 4)
		);
		frame++;
		return;
	}

	auto &center_y = state.ellen_gauge_pattern_y[player_id];
	if(center_y < (144 << 4)) {
		const auto angle = static_cast<std::uint8_t>(randring_next16(state));
		bullets_add_group(
			state, type, 0, center_y, target_player,
			state.gauge_pattern_param_b[player_id], angle,
			bullet_group_sixteen_ring
		);
		bullets_add_group(
			state, type, 288 << 4, center_y, target_player,
			state.gauge_pattern_param_b[player_id], angle,
			bullet_group_sixteen_ring
		);
		// ZUN overwrites the template angle before the next firing, but the
		// shared-ring read remains part of the deterministic call sequence.
		(void)randring_next16(state);
		center_y = exact::wrap_add(
			center_y, static_cast<std::int16_t>(24 << 4)
		);
		frame++;
		return;
	}

	flag = GaugeBossAttackFlag::none;
	frame++;
}

void gauge_pattern_rikako(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.rikako_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			0x10U - (state.gba_gauge_level[player_id] / 2U)
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x30U
		);
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto frame = state.rikako_gauge_pattern_frame[player_id];
	if((frame % state.gauge_pattern_param_a[player_id]) == 0U) {
		const auto angle = static_cast<std::uint8_t>(0x20U - (frame / 4U));
		const auto speed = state.gauge_pattern_param_b[player_id];
		bullets_add_group(
			state, type, 0, 0, target_player, speed, angle,
			bullet_group_one_aimed
		);
		bullets_add_group(
			state, type, 288 << 4, 0, target_player, speed, angle,
			bullet_group_one_aimed
		);
		const auto mirrored_angle = static_cast<std::uint8_t>(-angle);
		bullets_add_group(
			state, type, 288 << 4, 0, target_player, speed,
			mirrored_angle, bullet_group_one_aimed
		);
		bullets_add_group(
			state, type, 0, 0, target_player, speed, mirrored_angle,
			bullet_group_one_aimed
		);
	}
	if(frame >= 0x80U) {
		flag = GaugeBossAttackFlag::none;
	}
	state.rikako_gauge_pattern_frame[player_id]++;
}

void gauge_pattern_chiyuri(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.chiyuri_gauge_pattern_frame[player_id] = 0U;
		state.chiyuri_gauge_pattern_x[player_id] = -(24 << 4);
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			((state.gba_gauge_level[player_id] / 2U) << 4U) + 0x20U
		);
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	auto &frame = state.chiyuri_gauge_pattern_frame[player_id];
	auto &center_x = state.chiyuri_gauge_pattern_x[player_id];
	if((frame % 0x10U) == 0U) {
		center_x = exact::wrap_add(
			center_x, static_cast<std::int16_t>(24 << 4)
		);
	}
	if((frame & 1U) != 0U) {
		const auto speed = static_cast<std::uint8_t>(
			((frame % 0x10U) << 2U) + ((1U << 4U) + 8U)
		);
		bullets_add_group(
			state, type, center_x, 0, target_player, speed, 0U,
			bullet_group_one_aimed
		);
		bullets_add_group(
			state, type,
			static_cast<std::int16_t>((288 << 4) - center_x),
			0, target_player, speed, 0U, bullet_group_one_aimed
		);
	}
	frame++;
	if(frame >= state.gauge_pattern_param_a[player_id]) {
		flag = GaugeBossAttackFlag::none;
	}
}

void gauge_pattern_yumemi(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.yumemi_gauge_pattern_frame[player_id] = 0U;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x18U
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x20U
		);
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	auto &frame = state.yumemi_gauge_pattern_frame[player_id];
	if((frame % 8U) == 0U) {
		std::int16_t fire_x = 0;
		std::int16_t fire_y = 0;
		bool fire = false;
		switch(frame) {
		case 0U:
		case 8U:
			break;
		case 0x10U:
			fire_x = 144 << 4;
			fire_y = 32 << 4;
			fire = true;
			break;
		case 0x18U:
			fire_x = 96 << 4;
			fire_y = 80 << 4;
			fire = true;
			break;
		case 0x20U:
			fire_x = 192 << 4;
			fire_y = 80 << 4;
			fire = true;
			break;
		case 0x28U:
			fire_x = 96 << 4;
			fire_y = 56 << 4;
			fire = true;
			break;
		case 0x30U:
			fire_x = 192 << 4;
			fire_y = 56 << 4;
			fire = true;
			break;
		case 0x38U:
			fire_x = 144 << 4;
			fire_y = 104 << 4;
			fire = true;
			flag = GaugeBossAttackFlag::none;
			break;
		default:
			break;
		}
		if(frame <= 0x28U) {
			// SUB_CE5B() is visual, but consumes the shared random ring.
			(void)randring_next16(state);
		}
		if(fire) {
			bullets_add_group(
				state, type, fire_x, fire_y,
				static_cast<std::uint8_t>(1U - player_id),
				state.gauge_pattern_param_b[player_id], 0U,
				bullet_group_ring_aimed,
				state.gauge_pattern_param_a[player_id]
			);
		}
	}
	frame++;
}

void gauge_patterns_update(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto character =
		(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
	if(character == 0) {
		gauge_pattern_reimu(state, player_id);
	} else if(character == 1) {
		gauge_pattern_mima(state, player_id);
	} else if(character == 2) {
		gauge_pattern_marisa(state, player_id);
	} else if(character == 3) {
		gauge_pattern_ellen(state, player_id);
	} else if(character == 4) {
		gauge_pattern_kotohime(state, player_id);
	} else if(character == 5) {
		gauge_pattern_kana(state, player_id);
	} else if(character == 6) {
		gauge_pattern_rikako(state, player_id);
	} else if(character == 7) {
		gauge_pattern_chiyuri(state, player_id);
	} else if(character == 8) {
		gauge_pattern_yumemi(state, player_id);
	}
}

bool player_hittest(
	const DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t hitbox_size,
	std::int16_t &collision_x,
	std::int16_t &collision_y
) noexcept;

void player_defeat_update(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &player = state.players[player_id];
	if(player.lose_anim_time == 0x30U) {
		state.defeat_flag = DefeatFlag::explode;
		state.damage_all_on[1U - player_id] = 1U;
	}
	state.playfield_fg_shift_x[player_id] =
		((player.lose_anim_time & 3U) < 2U) ? 6 : -6;
	player.lose_anim_time--;
	if(player.lose_anim_time != 0U) {
		return;
	}
	player.lose_anim_time = 0xFFU;
	state.playfield_fg_shift_x[player_id] = 0;
	state.defeat_flag = DefeatFlag::banner;
	state.players[1U - player_id].rounds_won++;
	state.winner = static_cast<std::int8_t>(1U - player_id);
	state.round_or_result_frame = 0U;
}

bool player_update_begin(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &player = state.players[player_id];
	if(player.invincibility_time != 0U) {
		player.invincibility_time--;
	}
	if(state.damage_all_on[player_id] != 0U) {
		state.damage_all_on[player_id]--;
	}
	if(state.defeat_flag == DefeatFlag::banner) {
		return false;
	}
	if(player.lose_anim_time != 0U) {
		if(player.lose_anim_time != 0xFFU) {
			player_defeat_update(state, player_id);
		}
		return false;
	}

	player.hyper_effect_active_this_frame = false;
	if(!player.hyper_character_callback) {
		player.shot_mode = 1U;
		if(player.hyper_active != 0U) {
			player.hyper_character_callback = true;
		}
	} else if(player.hyper_active == 0U) {
		const auto character =
			(static_cast<int>(player.paletted_character) - 1) / 2;
		if(character == 6) {
			state.rikako_charge_state[player_id] = 0U;
		}
		player.hyper_character_callback = false;
	} else {
		player.hyper_effect_active_this_frame = true;
		const auto character =
			(static_cast<int>(player.paletted_character) - 1) / 2;
		if(character == 0) {
			player.shot_mode = 4U;
		} else if(character == 1) {
			player.shot_mode = 2U;
			player.shot_active = 2U;
		} else if(character == 2) {
			player.shot_mode = 0U;
			activate_marisa_hyper(state, player_id);
		} else if(character == 3) {
			player.shot_mode = 1U;
			if((state.round_frame & 3U) == 0U) {
				add_ellen_hyper_node(state, player_id);
			}
		} else if(
			character == 4 || character == 5 ||
			character == 7 || character == 8
		) {
			player.shot_mode = 3U;
		} else if(character == 6 && player.gauge_avail > (250U << 4U)) {
			add_rikako_chargeshot(state, player_id);
			state.rikako_charge_state[player_id] = 2U;
		}
	}
	return true;
}

void player_knockback_update(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &player = state.players[player_id];
	if(player.knockback_time == knockback_frames) {
		player.knockback_angle = static_cast<std::uint8_t>(
			randring_next16(state) & 0x3FU
		);
		if(player.center_x.raw() < (144 << 4)) {
			if(player.center_y.raw() > (184 << 4)) {
				player.knockback_angle = static_cast<std::uint8_t>(
					player.knockback_angle + 192U
				);
			}
		} else if(player.center_y.raw() < (184 << 4)) {
			player.knockback_angle = static_cast<std::uint8_t>(
				player.knockback_angle + 0x40U
			);
		} else {
			player.knockback_angle = static_cast<std::uint8_t>(
				player.knockback_angle + 0x80U
			);
		}
		player.knockback_length = 0x40U;
	}
	if(player.knockback_time > 0x20U) {
		player.knockback_length = static_cast<std::uint8_t>(
			player.knockback_length - 2U
		);
		const auto knockback = vector2(
			player.knockback_angle,
			static_cast<std::int16_t>(player.knockback_length)
		);
		const Velocity velocity{knockback.x, knockback.y};
		apply_velocity(player, velocity);
	} else if(player.knockback_time == 0U) {
		player.knockback_active = false;
	}
	player.knockback_time--;
}

std::uint8_t players_hit_damage_update(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &player = state.players[player_id];
	auto damage = player.hit_damage_next;
	if(player.is_cpu != 0U) {
		damage = static_cast<std::uint8_t>(
			damage + state.cpu_hit_damage_additional
		);
	}
	player.hit_damage_next = 3U;
	auto &other = state.players[1U - player_id];
	if(other.hit_damage_next > 3U) {
		other.hit_damage_next--;
	}
	if(
		static_cast<int>(player.halfhearts) - static_cast<int>(damage) <= 0 &&
		player.halfhearts > 1U
	) {
		damage = static_cast<std::uint8_t>(player.halfhearts - 1U);
	}
	if(player_id == 0U && state.skill > 0U) {
		state.skill--;
	}
	return damage;
}

void player_update_finish(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint16_t live_input
) noexcept
{
	auto &player = state.players[player_id];
	bool acted = false;
	if(player.knockback_active) {
		player_knockback_update(state, player_id);
		player.gauge_charged = 0U;
		player.cpu_frame = 0U;
		acted = true;
	} else if(
		state.autofire_shot_gate[player_id] <= 2U &&
		player.hyper_active == 0U
	) {
		if(player.gauge_charged < player.gauge_avail) {
			const auto charged = static_cast<std::uint16_t>(
				player.gauge_charged + player.gauge_charge_speed
			);
			if(charged > player.gauge_avail) {
				player.gauge_charged = player.gauge_avail;
				player.spell_ready_frames = 0U;
			} else {
				player.gauge_charged = charged;
			}
		} else {
			player.spell_ready_frames++;
			if(player.spell_ready_frames >= 128U) {
				acted = (player.gauge_charged >= gauge_64);
				release_gauge(state, player_id);
			}
		}
	} else if(state.autofire_shot_gate[player_id] <= 4U) {
		acted = (player.gauge_charged >= gauge_64);
		release_gauge(state, player_id);
	}

	if(player.shot_active == 2U) {
		live_input = 0U;
	}
	if(player.move_lock_time == 0U && player.shot_active != 0U) {
		bool shot_fired = false;
		if(
			(live_input & input_shot) != 0U || player.shot_active == 2U
		) {
			if(state.autofire_shot_gate[player_id] > 2U) {
				shots_add(state, player_id);
				state.autofire_shot_gate[player_id] = 0U;
				shot_fired = true;
			}
		}
		if(
			!shot_fired && (live_input & input_shot) == 0U &&
			state.autofire_shot_gate[player_id] < 8U
		) {
			state.autofire_shot_gate[player_id]++;
		}
		if((live_input & input_bomb) != 0U) {
			player_bomb(state, player_id);
		}
		if(player.shot_active == 2U) {
			player.shot_active = 1U;
		}
	}

	if(player.hyper_active != 0U) {
		if(player.gauge_avail > gauge_64) {
			player.gauge_avail = static_cast<std::uint16_t>(
				player.gauge_avail - 6U
			);
		} else {
			player.hyper_active = 0U;
			player.gauge_avail = gauge_64;
		}
		acted = true;
	}
	if(state.defeat_flag != DefeatFlag::none || acted) {
		return;
	}
	std::int16_t collision_x = 0;
	std::int16_t collision_y = 0;
	player.is_hit = player_hittest(
		state, player_id, 4U, collision_x, collision_y
	);
	if(!player.is_hit) {
		return;
	}
	if(player.halfhearts != 0U) {
		const auto damage = players_hit_damage_update(state, player_id);
		player.halfhearts = (damage > player.halfhearts)
			? 0U
			: static_cast<std::uint8_t>(player.halfhearts - damage);
		if(player.halfhearts == 0U) {
			player.lose_anim_time = 0x30U;
			state.last_fatal_player = player_id;
		} else {
			player.knockback_active = true;
			player.knockback_time = knockback_frames;
			player.invincibility_time = miss_invincibility_frames;
			player.move_lock_time = knockback_frames;
		}
	}
	player.is_hit = false;
}

void player_update_human(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint16_t live_input
) noexcept
{
	auto &player = state.players[player_id];
	if(!player_update_begin(state, player_id)) {
		return;
	}
	if(player.move_lock_time != 0U) {
		player.move_lock_time--;
	} else {
		move_human(player, live_input);
	}
	player_update_finish(state, player_id, live_input);
}

std::uint16_t cpu_initial_avoidance_input(
	const PlayerState &player,
	std::int16_t collision_x,
	std::int16_t collision_y
) noexcept
{
	const auto delta_x = static_cast<std::int16_t>(
		collision_x - player.center_x.raw()
	);
	const auto delta_y = static_cast<std::int16_t>(
		collision_y - player.center_y.raw()
	);
	std::size_t row = 0U;
	if(delta_x <= -(16 << 4)) {
		row = 0U;
	} else if(delta_x <= (16 << 4)) {
		row = 1U;
	} else {
		row = 2U;
	}
	row += static_cast<std::size_t>(player.cpu_dodge_strategy) * 3U;
	row = std::min<std::size_t>(row, cpu_collision_routes.size() - 1U);

	std::size_t direction = 0U;
	if(delta_y < -(16 << 4)) {
		if(player.center_x.raw() >= (240 << 4)) {
			if(player.center_y.raw() < (128 << 4)) {
				return input_left;
			}
			direction = 0U;
		} else if(player.center_x.raw() >= (16 << 4)) {
			direction = 1U;
		} else {
			if(player.center_y.raw() < (128 << 4)) {
				return input_right;
			}
			direction = 2U;
		}
	} else if(delta_y < (32 << 4)) {
		direction = (player.center_x.raw() >= (240 << 4))
			? 3U : ((player.center_x.raw() >= (16 << 4)) ? 4U : 5U);
	} else {
		direction = (player.center_x.raw() >= (240 << 4))
			? 6U : ((player.center_x.raw() >= (16 << 4)) ? 7U : 8U);
	}
	auto input = cpu_collision_routes[row][direction];
	if(
		player.center_y.raw() >= (304 << 4) && input == input_down
	) {
		input = (player.center_x.raw() >= (128 << 4)) ? input_left : input_right;
	}
	return input;
}

std::uint16_t cpu_target_movement(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto &player = state.players[player_id];
	const auto target = cpu_target_point(state, player_id);
	std::uint8_t target_flags = static_cast<std::uint8_t>(
		exact::wrap_add(target.x, static_cast<std::int16_t>(16 << 4)) <
		player.center_x.raw()
	);
	target_flags = static_cast<std::uint8_t>(target_flags | (
		static_cast<std::uint8_t>(
			exact::wrap_subtract(
				target.x, static_cast<std::int16_t>(16 << 4)
			) > player.center_x.raw()
		) << 1U
	));
	target_flags = static_cast<std::uint8_t>(target_flags | (
		static_cast<std::uint8_t>(
			exact::wrap_add(target.y, static_cast<std::int16_t>(80 << 4)) <
			player.center_y.raw()
		) << 2U
	));
	target_flags = static_cast<std::uint8_t>(target_flags | (
		static_cast<std::uint8_t>(player.center_y.raw() < target.y) << 3U
	));
	switch(target_flags) {
	case 1U: return input_left;
	case 2U: return input_right;
	case 4U: return input_up;
	case 8U: return input_down;
	case 5U: return input_up | input_left;
	case 9U: return input_down | input_left;
	case 6U: return input_up | input_right;
	case 10U: return input_down | input_right;
	default: return 0U;
	}
}

bool cpu_safe_move(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &player = state.players[player_id];
	std::int16_t collision_x = 0;
	std::int16_t collision_y = 0;
	bool broad_hit = false;
	if((state.round_or_result_frame & 3U) == 0U) {
		for(std::uint8_t size = 32U; size <= 80U; size += 16U) {
			const auto hit = player_hittest(
				state, player_id, size, collision_x, collision_y
			);
			if(hit) {
				broad_hit = true;
				break;
			}
		}
	}
	auto input = broad_hit
		? cpu_initial_avoidance_input(player, collision_x, collision_y)
		: cpu_target_movement(state, player_id);
	const auto center_x = player.center_x;
	const auto center_y = player.center_y;
	std::uint8_t route_attempt = 0U;
	std::uint8_t fallback_attempt = 0U;
	std::size_t route_group = 0U;
	for(;;) {
		Velocity velocity{};
		if(movement_velocity(player, input, velocity) == MoveResult::valid) {
			apply_velocity(player, velocity);
		}
		const auto hitbox_size = static_cast<std::uint8_t>(
			(fallback_attempt == 0U) ? 20U : 4U
		);
		const auto route_hit = player_hittest(
			state, player_id, hitbox_size, collision_x, collision_y
		);
		if(!route_hit) {
			return broad_hit;
		}
		player.center_x = center_x;
		player.center_y = center_y;
		route_attempt++;
		if(route_attempt < 9U) {
			route_group = (collision_x < (80 << 4))
				? 0U : ((collision_x < (176 << 4)) ? 1U : 2U);
			input = cpu_retry_routes[route_group][route_attempt];
			continue;
		}
		if(fallback_attempt < 9U) {
			input = cpu_retry_routes[route_group][fallback_attempt];
			fallback_attempt++;
			continue;
		}
		if(player.gauge_charged <= gauge_64) {
			player_bomb(state, player_id);
		} else {
			player.is_hit = true;
		}
		return broad_hit;
	}
}

void player_update_cpu(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &player = state.players[player_id];
	auto live_input = std::uint16_t{0};
	if(!player_update_begin(state, player_id)) {
		return;
	}
	if(player.move_lock_time != 0U) {
		player.move_lock_time--;
		player_update_finish(state, player_id, live_input);
		return;
	}

	player.cpu_frame++;
	if(player.cpu_frame <= player.cpu_safety_frames) {
		// A broad collision makes sub_C568() skip target selection. The shared
		// target scratch therefore retains the value written by an earlier
		// callback in this frame.
		(void)cpu_safe_move(state, player_id);
	} else {
		const auto movement = cpu_target_movement(state, player_id);
		(void)(randring_next16(state) % 3U);
		Velocity velocity{};
		if(movement_velocity(player, movement, velocity) == MoveResult::valid) {
			apply_velocity(player, velocity);
		}
		std::int16_t collision_x = 0;
		std::int16_t collision_y = 0;
		const auto small_hit = player_hittest(
			state, player_id, 4U, collision_x, collision_y
		);
		if(small_hit) {
			if(player.gauge_charged > (80U << 4U)) {
				player.is_hit = true;
			} else if((randring_next16(state) & 3U) == 0U) {
				player_bomb(state, player_id);
			} else {
				player.is_hit = true;
			}
		}
	}

	std::uint16_t cadence = 10U;
	if(state.cpu_target_kind_scratch == 0) {
		cadence = 6U;
	} else if(state.cpu_target_kind_scratch == -1) {
		cadence = 32U;
	} else if(state.cpu_target_kind_scratch == -2) {
		cadence = 7U;
	}
	if((state.round_or_result_frame % cadence) == 0U) {
		state.autofire_shot_gate[player_id] = 4U;
		live_input = input_shot;
	}
	state.cpu_target_kind_scratch = 0;
	if(player.is_hit) {
		player.is_hit = false;
		state.autofire_shot_gate[player_id] = 4U;
		live_input = 0U;
	} else if(!(
		player.bombs == 0U &&
		static_cast<std::int8_t>(player.halfhearts) <= 3
	)) {
		if(
			player.cpu_charge_ring[player.cpu_charge_cursor] <
				(player.gauge_avail >> 4U) ||
			player.gauge_avail == gauge_max
		) {
			state.autofire_shot_gate[player_id] = 0U;
			live_input = input_shot;
			if(player.gauge_charged >= player.gauge_avail) {
				state.autofire_shot_gate[player_id] = 4U;
				live_input = 0U;
				player.cpu_charge_cursor = static_cast<std::uint8_t>(
					(player.cpu_charge_cursor + 1U) % 64U
				);
			}
		}
	}

	player_update_finish(state, player_id, live_input);
}

std::int16_t enemy_speedtune(std::int16_t value, std::uint8_t speed) noexcept
{
	const auto tuned_one = static_cast<std::int16_t>(
		exact::wrap_multiply(
			static_cast<std::int16_t>(speed), value
		) / static_cast<std::int16_t>(9)
	);
	const auto tuned_two = static_cast<std::int16_t>(
		exact::wrap_multiply(static_cast<std::int16_t>(2), value) /
		static_cast<std::int16_t>(3)
	);
	return exact::wrap_add(tuned_one, tuned_two);
}

bool enemy_move_and_clip(EnemyState &enemy, std::uint8_t speed) noexcept
{
	const auto velocity_x = enemy_speedtune(enemy.velocity_x, speed);
	if((enemy.position_type & position_do_not_mirror_x) != 0U) {
		enemy.center_x = exact::wrap_add(enemy.center_x, velocity_x);
	} else {
		enemy.center_x = exact::wrap_subtract(enemy.center_x, velocity_x);
	}
	if(
		(enemy.position_type & position_clip_x) != 0U &&
		(enemy.center_x <= (-32 << 4) || enemy.center_x >= (320 << 4))
	) {
		enemy.flag = EnemyFlag::free;
		return true;
	}

	const auto velocity_y = enemy_speedtune(enemy.velocity_y, speed);
	enemy.center_y = exact::wrap_add(enemy.center_y, velocity_y);
	if(
		(enemy.position_type & position_clip_bottom) != 0U &&
		enemy.center_y >= (400 << 4)
	) {
		enemy.flag = EnemyFlag::free;
		return true;
	}
	return false;
}

void set_linear_velocity(EnemyState &enemy, std::uint8_t angle) noexcept
{
	const auto velocity = vector2(angle, static_cast<std::int16_t>(enemy.speed));
	enemy.velocity_x = velocity.x;
	enemy.velocity_y = velocity.y;
}

void set_linear_angle_and_velocity(
	EnemyState &enemy,
	std::uint8_t angle
) noexcept
{
	set_linear_velocity(enemy, angle);
	enemy.angle = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(angle) << 8U
	);
}

void enemy_angle_update(EnemyState &enemy, std::uint8_t speed) noexcept
{
	const auto initial = static_cast<std::int16_t>(
		exact::wrap_multiply(
			static_cast<std::int16_t>(enemy.angle_speed),
			static_cast<std::int16_t>(0x100 * 2)
		) / static_cast<std::int16_t>(3)
	);
	const auto tuned = exact::wrap_add(
		initial,
		static_cast<std::int16_t>(
			exact::wrap_multiply(
				static_cast<std::int16_t>(speed), initial
			) / static_cast<std::int16_t>(9)
		)
	);
	enemy.angle = exact::wrap_add(
		enemy.angle, static_cast<std::uint16_t>(tuned)
	);
}

bool wait_and_advance(
	EnemyState &enemy,
	std::uint8_t duration,
	std::uint8_t op_size,
	std::uint8_t speed
) noexcept
{
	const auto tuned =
		((static_cast<int>(duration) / 2) * 3) -
		((static_cast<int>(speed) * static_cast<int>(duration)) / 6);
	if(tuned <= static_cast<int>(enemy.script_op_frame)) {
		enemy.script_op_frame = 0U;
		enemy.script_ip = static_cast<std::uint16_t>(enemy.script_ip + op_size);
	} else {
		enemy.script_op_frame++;
	}
	return true;
}

bool enemy_run(
	EnemyState &enemy,
	const GameContent &content,
	const PlayerState &player,
	std::uint8_t speed
) noexcept
{
	if(enemy.formation_type >= content.enemy_formations.size()) {
		return false;
	}
	const auto &formation = content.enemy_formations[enemy.formation_type];
	if(enemy.formation_index >= formation.enemy_scripts.size()) {
		return false;
	}
	const auto &script = formation.enemy_scripts[enemy.formation_index];
	for(;;) {
		if(enemy.script_ip >= script.size()) {
			return false;
		}
		const auto code = script[enemy.script_ip];
		if(code == op_stop) {
			enemy.flag = EnemyFlag::free;
			return true;
		}
		if(code == op_wait) {
			if(enemy.script_ip + 2U > script.size()) {
				return false;
			}
			return wait_and_advance(
				enemy, script[enemy.script_ip + 1U], 2U, speed
			);
		}
		if(code == op_spawn) {
			constexpr std::uint8_t spawn_size = 14U;
			if(
				static_cast<std::size_t>(enemy.script_ip) + spawn_size > script.size() ||
				speed >= 4U
			) {
				return false;
			}
			enemy.flag = EnemyFlag::running_spawned;
			const auto spawn_x = static_cast<std::int16_t>(
				std::bit_cast<std::int8_t>(script[enemy.script_ip + 1U]) * 128
			);
			enemy.center_x = (
				(enemy.position_type & position_do_not_mirror_x) != 0U
			) ? spawn_x : static_cast<std::int16_t>((288 << 4) - spawn_x);
			enemy.center_y = static_cast<std::int16_t>(
				std::bit_cast<std::int8_t>(script[enemy.script_ip + 2U]) * 128
			);
			const auto size_words = script[enemy.script_ip + 3U + speed];
			enemy.explosion_max_enemy_hits_half = size_words;
			enemy.size_pixels = static_cast<std::uint8_t>(size_words * 16U);
			enemy.hp = script[enemy.script_ip + 7U + speed];
			if(script[enemy.script_ip + 11U] != 0U) {
				enemy.position_type |= position_clip_x;
			}
			if(script[enemy.script_ip + 12U] != 0U) {
				enemy.position_type |= position_clip_bottom;
			}
			enemy.script_ip = static_cast<std::uint16_t>(
				enemy.script_ip + spawn_size
			);
			continue;
		}
		if(
			code == op_move_linear ||
			code == op_move_linear_stop_at_player_y ||
			code == op_move_linear_stop_at_player_x
		) {
			constexpr std::uint8_t linear_size = 4U;
			if(
				static_cast<std::size_t>(enemy.script_ip) + linear_size > script.size()
			) {
				return false;
			}
			if(enemy.script_op_frame == 0U) {
				enemy.speed = script[enemy.script_ip + 2U];
				set_linear_angle_and_velocity(
					enemy, script[enemy.script_ip + 1U]
				);
			}
			if(enemy_move_and_clip(enemy, speed)) {
				return true;
			}
			if(
				(code == op_move_linear_stop_at_player_y &&
					enemy.center_y >= exact::wrap_subtract(
						player.center_y.raw(), static_cast<std::int16_t>(32 << 4)
					) &&
					enemy.center_y <= exact::wrap_add(
						player.center_y.raw(), static_cast<std::int16_t>(32 << 4)
					)) ||
				(code == op_move_linear_stop_at_player_x &&
					enemy.center_x >= exact::wrap_subtract(
						player.center_x.raw(), static_cast<std::int16_t>(16 << 4)
					) &&
					enemy.center_x <= exact::wrap_add(
						player.center_x.raw(), static_cast<std::int16_t>(16 << 4)
					))
			) {
				enemy.script_op_frame = 0U;
				enemy.script_ip = static_cast<std::uint16_t>(
					enemy.script_ip + linear_size
				);
				continue;
			}
			return wait_and_advance(
				enemy, script[enemy.script_ip + 3U], linear_size, speed
			);
		}
		if(code == op_move_circular || code == op_move_circular_plus) {
			const auto op_size = static_cast<std::uint8_t>(
				(code == op_move_circular) ? 5U : 7U
			);
			if(
				static_cast<std::size_t>(enemy.script_ip) + op_size > script.size()
			) {
				return false;
			}
			if(enemy.script_op_frame == 0U) {
				enemy.angle = static_cast<std::uint16_t>(
					static_cast<std::uint16_t>(script[enemy.script_ip + 1U]) << 8U
				);
				enemy.angle_speed = std::bit_cast<std::int8_t>(
					script[enemy.script_ip + 3U]
				);
			}
			enemy.speed = script[enemy.script_ip + 2U];
			const auto velocity = vector2(
				static_cast<std::uint8_t>(enemy.angle >> 8U), enemy.speed
			);
			enemy.velocity_x = velocity.x;
			enemy.velocity_y = velocity.y;
			std::uint8_t duration_offset = 4U;
			if(code == op_move_circular_plus) {
				enemy.velocity_x = exact::wrap_add(
					enemy.velocity_x,
					static_cast<std::int16_t>(std::bit_cast<std::int8_t>(
						script[enemy.script_ip + 4U]
					))
				);
				enemy.velocity_y = exact::wrap_add(
					enemy.velocity_y,
					static_cast<std::int16_t>(std::bit_cast<std::int8_t>(
						script[enemy.script_ip + 5U]
					))
				);
				duration_offset = 6U;
			}
			if(enemy_move_and_clip(enemy, speed)) {
				return true;
			}
			enemy_angle_update(enemy, speed);
			return wait_and_advance(
				enemy, script[enemy.script_ip + duration_offset], op_size, speed
			);
		}
		if(code == op_move) {
			constexpr std::uint8_t move_size = 2U;
			if(
				static_cast<std::size_t>(enemy.script_ip) + move_size > script.size()
			) {
				return false;
			}
			if(enemy_move_and_clip(enemy, speed)) {
				return true;
			}
			return wait_and_advance(
				enemy, script[enemy.script_ip + 1U], move_size, speed
			);
		}
		if(code == op_move_with_speed) {
			constexpr std::uint8_t speed_size = 3U;
			if(
				static_cast<std::size_t>(enemy.script_ip) + speed_size > script.size()
			) {
				return false;
			}
			if(enemy.script_op_frame == 0U) {
				enemy.speed = script[enemy.script_ip + 1U];
				set_linear_velocity(
					enemy, static_cast<std::uint8_t>(enemy.angle >> 8U)
				);
			}
			if(enemy_move_and_clip(enemy, speed)) {
				return true;
			}
			return wait_and_advance(
				enemy, script[enemy.script_ip + 2U], speed_size, speed
			);
		}
		if(code == op_move_sine_x || code == op_move_sine_y) {
			constexpr std::uint8_t sine_size = 5U;
			if(
				static_cast<std::size_t>(enemy.script_ip) + sine_size > script.size()
			) {
				return false;
			}
			if(enemy.script_op_frame == 0U) {
				enemy.angle = 0U;
				enemy.angle_speed = std::bit_cast<std::int8_t>(
					script[enemy.script_ip + 2U]
				);
			}
			const auto oscillating = vector2(
				static_cast<std::uint8_t>(enemy.angle >> 8U),
				static_cast<std::int16_t>(script[enemy.script_ip + 1U])
			).x;
			const auto linear = static_cast<std::int16_t>(
				std::bit_cast<std::int8_t>(script[enemy.script_ip + 3U])
			);
			if(code == op_move_sine_x) {
				enemy.velocity_x = oscillating;
				enemy.velocity_y = linear;
			} else {
				enemy.velocity_x = linear;
				enemy.velocity_y = oscillating;
			}
			if(enemy_move_and_clip(enemy, speed)) {
				return true;
			}
			enemy_angle_update(enemy, speed);
			return wait_and_advance(
				enemy, script[enemy.script_ip + 4U], sine_size, speed
			);
		}
		if(code == op_clip_x || code == op_clip_bottom) {
			if(code == op_clip_x) {
				enemy.position_type |= position_clip_x;
			} else {
				enemy.position_type |= position_clip_bottom;
			}
			enemy.script_ip++;
			continue;
		}
		return false;
	}
}

bool add_formation(
	DeterministicState &state,
	const GameContent &content,
	std::uint8_t player_id
) noexcept
{
	if(state.enemies_alive[player_id] != 0U) {
		return true;
	}
	state.enemy_speed = static_cast<std::uint8_t>(state.round_speed / 32U);
	const auto cursor = state.formation_cursor[player_id];
	const auto formation_id = state.formation_type_ring[cursor];
	if(formation_id >= content.enemy_formations.size()) {
		return false;
	}
	const auto position_type = state.formation_position_ring[cursor];
	const auto &formation = content.enemy_formations[formation_id];
	for(std::size_t index = 0U; index < formation.enemy_scripts.size(); ++index) {
		auto slot = std::find_if(
			state.enemies.begin(), state.enemies.end(),
			[](const EnemyState &enemy) { return enemy.flag == EnemyFlag::free; }
		);
		if(slot == state.enemies.end()) {
			break;
		}
		slot->flag = EnemyFlag::running_unspawned;
		slot->script_ip = 0U;
		slot->script_op_frame = 0U;
		slot->loop_index = 0U;
		slot->player_id = player_id;
		slot->formation_type = formation_id;
		slot->formation_index = static_cast<std::uint8_t>(index);
		slot->position_type = position_type;
	}
	state.formation_cursor[player_id] = static_cast<std::uint8_t>(cursor + 1U);
	return true;
}

void collmap_set_enemy(
	DeterministicState &state,
	const EnemyState &enemy
) noexcept
{
	if(enemy.center_y < 0 || enemy.center_y >= (368 << 4)) {
		return;
	}
	const auto tile_w = static_cast<int>((enemy.size_pixels * 3U) / 16U);
	const auto tile_h = tile_w;
	int top = static_cast<int>(enemy.center_y) >> 5;
	top -= tile_h / 2;
	int bottom = top + tile_h;
	top = std::max(top, 0);
	bottom = std::min(bottom, static_cast<int>(collmap_height) - 1);
	const auto h = bottom - top;
	if(h <= 0) {
		return;
	}
	int left = static_cast<int>(enemy.center_x) >> 5;
	left -= tile_w / 2;
	const int right = left + tile_w;
	left = std::max(left, 0);
	for(int x = left; x < right && x < static_cast<int>(collmap_width); ++x) {
		for(int row = 0; row < h; row += 4) {
			state.collision_map[enemy.player_id][static_cast<std::size_t>(x)]
				[static_cast<std::size_t>(top + row)] = true;
		}
	}
}

void collmap_set_fireball(
	DeterministicState &state,
	const FireballState &fireball
) noexcept
{
	constexpr int tile_w = 16 / 2;
	constexpr int tile_h = 16 / 2;
	const auto center_y = exact::wrap_add(
		fireball.center_y, static_cast<std::int16_t>(8 << 4)
	);
	if(center_y < 0 || center_y >= (368 << 4)) {
		return;
	}
	int top = static_cast<int>(center_y) >> 5;
	top -= tile_h / 2;
	int bottom = top + tile_h;
	top = std::max(top, 0);
	bottom = std::min(bottom, static_cast<int>(collmap_height) - 1);
	const auto h = bottom - top;
	if(h <= 0) {
		return;
	}
	int left = static_cast<int>(fireball.center_x) >> 5;
	left -= tile_w / 2;
	const int right = left + tile_w;
	left = std::max(left, 0);
	for(int x = left; x < right && x < static_cast<int>(collmap_width); ++x) {
		for(int row = 0; row < h; row += 4) {
			state.collision_map[fireball.player_id][static_cast<std::size_t>(x)]
				[static_cast<std::size_t>(top + row)] = true;
		}
	}
}

void collmap_set_rect_striped(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y,
	int tile_w,
	int tile_h
) noexcept
{
	if(center_y < 0 || center_y >= (368 << 4)) {
		return;
	}
	int top = static_cast<int>(center_y) >> 5;
	top -= tile_h / 2;
	int bottom = top + tile_h;
	top = std::max(top, 0);
	bottom = std::min(bottom, static_cast<int>(collmap_height) - 1);
	const auto height = bottom - top;
	if(height <= 0) {
		return;
	}
	int left = static_cast<int>(center_x) >> 5;
	left -= tile_w / 2;
	int width = tile_w;
	if(left < 0) {
		width += left;
		left = 0;
	}
	if(width <= 0) {
		return;
	}
	const auto right = std::min(
		left + width, static_cast<int>(collmap_width)
	);
	for(int x = left; x < right; ++x) {
		for(int row = 0; row < height; row += 4) {
			state.collision_map[player_id][static_cast<std::size_t>(x)]
				[static_cast<std::size_t>(top + row)] = true;
		}
	}
}

void collmap_set_slope_striped(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t top_x,
	std::int16_t bottom_x
) noexcept
{
	constexpr std::int16_t stripe_count =
		static_cast<std::int16_t>(collmap_height / 2U);
	const auto slope_width = exact::wrap_subtract(bottom_x, top_x);
	auto left = bottom_x;
	auto y_double = static_cast<std::int16_t>(stripe_count - 1);
	int row = static_cast<int>(collmap_height) - 1;
	for(;;) {
		const auto tile_x = static_cast<std::uint16_t>(left) >> 5U;
		for(std::uint16_t column = 0U; column < 2U; ++column) {
			const auto x = static_cast<std::uint16_t>(tile_x + column);
			if(x < collmap_width) {
				state.collision_map[player_id][x][static_cast<std::size_t>(row)] =
					true;
			}
		}
		y_double--;
		if(y_double == 0) {
			return;
		}
		const auto interpolated = static_cast<std::int16_t>(
			(static_cast<std::int32_t>(slope_width) * y_double) / stripe_count
		);
		left = exact::wrap_add(top_x, interpolated);
		row -= 2;
	}
}

bool extra_attack_transfer_update(ExtraAttackState &attack) noexcept
{
	const auto next_x = exact::wrap_add(attack.center_x, attack.velocity_x);
	const auto reached = (attack.player_id == 0U)
		? (next_x >= attack.target_x_for_origin_player)
		: (next_x <= attack.target_x_for_origin_player);
	if(reached) {
		attack.center_x = attack.target_x_for_target_player;
		attack.state = 3U;
		attack.player_id ^= 1U;
		return true;
	}
	attack.center_x = next_x;
	attack.center_y = exact::wrap_add(attack.center_y, attack.velocity_y);
	return false;
}

bool extra_attack_bounds_hittest(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t left,
	std::int16_t right,
	std::int16_t top,
	std::int16_t bottom
) noexcept
{
	bool hit = false;
	for(auto &shot : state.shotpairs) {
		if(!shot.alive || shot.player_id != player_id) {
			continue;
		}
		const auto shot_center_x = exact::wrap_add(
			shot.topleft_x, shotpair_half_width
		);
		const auto shot_center_y = exact::wrap_add(
			shot.topleft_y, shotpair_half_height
		);
		if(
			exact::wrap_subtract(shot_center_x, shotpair_half_width) > right ||
			exact::wrap_add(shot_center_x, shotpair_half_width) < left ||
			shot_center_y < top || shot_center_y > bottom
		) {
			continue;
		}
		shot.alive = false;
		hit = true;
	}
	const auto charge_hit = (
		chargeshot_hittest(state, player_id, left, right, top, bottom) != 0U
	);
	const auto bomb_hit = (
		state.bomb_flag[player_id] != BombFlag::inactive &&
		(state.round_or_result_frame & 1U) == 0U
	);
	return (hit || charge_hit || bomb_hit);
}

bool extra_attack_shot_hittest(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	if(center_y < 0) {
		return false;
	}
	constexpr std::int16_t radius = 16 << 4;
	return extra_attack_bounds_hittest(
		state,
		player_id,
		exact::wrap_subtract(center_x, radius),
		exact::wrap_add(center_x, radius),
		exact::wrap_subtract(center_y, radius),
		exact::wrap_add(center_y, radius)
	);
}

void extra_attacks_update_reimu(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot = 0U; slot < extra_attack_regular_count; ++slot) {
		auto &attack = state.extra_attacks[player_id][slot];
		if(attack.state == 0U) {
			continue;
		}
		if(attack.state == 1U) {
			attack.center_x = exact::wrap_add(
				attack.center_x, attack.velocity_x
			);
			if(attack.center_x <= 0 || attack.center_x >= (288 << 4)) {
				attack.velocity_x = exact::wrap_negate(attack.velocity_x);
				attack.center_x = exact::wrap_add(
					attack.center_x, attack.velocity_x
				);
			}
			attack.center_y = exact::wrap_add(
				attack.center_y, attack.velocity_y
			);
			if(attack.center_y < -(24 << 4)) {
				attack.velocity_y = exact::wrap_negate(attack.velocity_y);
				attack.center_y = -(24 << 4);
			}
			if(attack.center_y >= (368 << 4)) {
				attack.state = 0U;
				continue;
			}
			attack.velocity_y = exact::wrap_add(
				attack.velocity_y, static_cast<std::int16_t>(1)
			);
			extra_attack_shot_hittest(
				state, target_player, attack.center_x, attack.center_y
			);
			collmap_set_rect_striped(
				state, target_player, attack.center_x, attack.center_y, 8, 16
			);
			collmap_set_rect_striped(
				state,
				target_player,
				exact::wrap_subtract(
					attack.center_x, static_cast<std::int16_t>(12 << 4)
				),
				attack.center_y,
				4,
				8
			);
			collmap_set_rect_striped(
				state,
				target_player,
				exact::wrap_add(
					attack.center_x, static_cast<std::int16_t>(12 << 4)
				),
				attack.center_y,
				4,
				8
			);
		} else if(attack.state == 2U) {
			if(extra_attack_transfer_update(attack)) {
				auto angle = static_cast<std::uint8_t>(
					randring_next16(state) & 0x1FU
				);
				angle = static_cast<std::uint8_t>(
					angle + ((randring_next16(state) & 1U) * 0x60U)
				);
				attack.angle = static_cast<std::uint8_t>(angle + 0x80U);
				const auto velocity = vector2(
					attack.angle,
					static_cast<std::int16_t>((state.round_speed / 8U) + 0x32U)
				);
				attack.velocity_x = velocity.x;
				attack.velocity_y = velocity.y;
			}
		} else if(attack.state <= 0x14U) {
			attack.state++;
		} else {
			attack.state = 1U;
		}
	}
}

void extra_attacks_update_mima(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot_id = 0U; slot_id < extra_attack_mima_count; ++slot_id) {
		auto &attack = state.extra_attacks[player_id][slot_id];
		if(attack.state == 0U) {
			continue;
		}

		if(attack.state == 1U) {
			const auto center_x = exact::wrap_add(
				attack.center_x, attack.velocity_x
			);
			const auto center_y = exact::wrap_add(
				attack.center_y, attack.velocity_y
			);
			if(center_x <= 0 || center_x >= (288 << 4)) {
				if(attack.frame == 0U) {
					attack.state = 0U;
					continue;
				}
				attack.frame--;
				attack.velocity_x = exact::wrap_negate(attack.velocity_x);
				attack.angle = iatan2(attack.velocity_y, attack.velocity_x);
			} else if(center_y <= 0 || center_y >= (368 << 4)) {
				if(attack.frame == 0U) {
					attack.state = 0U;
					continue;
				}
				attack.frame--;
				attack.velocity_y = exact::wrap_negate(attack.velocity_y);
				attack.angle = iatan2(attack.velocity_y, attack.velocity_x);
			} else {
				attack.center_x = center_x;
				attack.center_y = center_y;
			}

			const auto timer = attack.mima_timer;
			if(timer >= 0x10U && timer < 0x20U && (timer & 1U) != 0U) {
				attack.speed = static_cast<std::uint8_t>(attack.speed + 1U);
			} else if(timer >= 0x20U && timer < 0x50U) {
				attack.speed = static_cast<std::uint8_t>(attack.speed + 1U);
			}
			if(timer < 0x50U) {
				const auto velocity = vector2(
					attack.angle, static_cast<std::int16_t>(attack.speed)
				);
				attack.velocity_x = velocity.x;
				attack.velocity_y = velocity.y;
			}
			if(
				state.defeat_flag != DefeatFlag::banner &&
				extra_attack_shot_hittest(
					state, target_player, center_x, center_y
				)
			) {
				attack.velocity_y = exact::wrap_subtract(
					attack.velocity_y, std::int16_t{8}
				);
			}
			collmap_set_rect_striped(
				state, target_player, center_x, center_y, 8, 8
			);
		} else if(attack.state == 2U) {
			if(extra_attack_transfer_update(attack)) {
				attack.angle = static_cast<std::uint8_t>(randring_next16(state));
				attack.speed = 0U;
				attack.velocity_x = 0;
				attack.velocity_y = 0;
				continue;
			}
		} else if(attack.state <= 0x1CU) {
			attack.state++;
		} else {
			attack.mima_timer = 0U;
			attack.state = 1U;
		}
		attack.mima_timer = exact::wrap_add(
			attack.mima_timer, std::uint16_t{1}
		);
	}
}

void extra_attacks_update_marisa(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot_id = 0U; slot_id < extra_attack_marisa_count; ++slot_id) {
		auto &attack = state.extra_attacks[player_id][slot_id];
		if(attack.state == 0U) {
			continue;
		}

		if(attack.state == 1U) {
			if(attack.frame < 0x30U) {
				attack.center_y = exact::wrap_subtract(
					attack.center_y, std::int16_t{16 << 4}
				);
				if(attack.center_y < -(16 << 4)) {
					attack.frame = 0x30U;
					attack.center_y = -(16 << 4);
				}
			} else if(attack.frame >= 0x78U) {
				attack.state = 0U;
				continue;
			}

			auto top = attack.center_y;
			if(top < 0) {
				top = 0;
			}
			const auto collmap_h = static_cast<std::int16_t>((368 << 4) - top);
			if(attack.frame < 0x64U) {
				collmap_set_rect_striped(
					state,
					target_player,
					attack.center_x,
					static_cast<std::int16_t>(top + (collmap_h / 2)),
					2,
					static_cast<int>(collmap_h / 32)
				);
			}
		} else if(attack.state == 2U) {
			if(extra_attack_transfer_update(attack)) {
				attack.center_y = 368 << 4;
				continue;
			}
		} else if(attack.state <= 0x1CU) {
			attack.state++;
			// State 0x0C only spawns a SUB_CE0C visual, which has no RNG or
			// gameplay-owned state in the headless core.
		} else {
			attack.frame = 0U;
			attack.state = 1U;
		}
		attack.frame++;
	}
}

void extra_attacks_update_kana(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot = 0U; slot < extra_attack_regular_count; ++slot) {
		auto &attack = state.extra_attacks[player_id][slot];
		if(attack.state == 0U) {
			continue;
		}
		if(attack.state == 1U) {
			const auto velocity = vector2(
				attack.angle, static_cast<std::int16_t>(attack.speed)
			);
			attack.speed++;
			attack.center_x = exact::wrap_add(attack.center_x, velocity.x);
			attack.center_y = exact::wrap_add(attack.center_y, velocity.y);
			if((attack.frame & 0x0FU) == 0U) {
				bullet_add_kana_extra_attack(
					state, attack.center_x, attack.center_y, target_player
				);
			}
			if(playfield_clip(
				attack.center_x, attack.center_y, -(24 << 4)
			)) {
				attack.state = 0U;
				continue;
			}
		} else if(attack.state == 2U) {
			(void)extra_attack_transfer_update(attack);
		} else if(attack.state <= 0x1CU) {
			attack.state++;
		} else {
			attack.frame = 0U;
			attack.state = 1U;
			attack.speed = 8U;
		}
		attack.frame++;
	}
}

void extra_attacks_update_kotohime(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot = 0U; slot < 10U; ++slot) {
		auto &attack = state.extra_attacks[player_id][slot];
		if(attack.state == 0U) {
			continue;
		}
		if(attack.state == 1U) {
			if(attack.center_y >= attack.kotohime_target_y) {
				if(attack.kotohime_milestone == 4U) {
					if(attack.behavior_variant != 0U) {
						auto speed = static_cast<std::uint8_t>(1U << 4U);
						auto angle = static_cast<std::uint8_t>(randring_next16(state));
						const auto angle_delta =
							(state.gauge_pattern_param_b[player_id] != 0U)
							? std::int8_t{-12}
							: std::int8_t{12};
						for(std::size_t firing = 0U; firing < 4U; ++firing) {
							bullets_add_group(
								state,
								state.kotohime_gauge_pattern_type[player_id],
								attack.center_x,
								attack.center_y,
								target_player,
								speed,
								angle,
								bullet_group_ring,
								state.gauge_pattern_param_a[player_id]
							);
							speed = static_cast<std::uint8_t>(speed + 2U);
							angle = static_cast<std::uint8_t>(angle + angle_delta);
						}
					}
				} else if(attack.kotohime_milestone == 0x10U) {
					auto angle = static_cast<std::uint8_t>(randring_next16(state));
					if(attack.behavior_variant == 0U) {
						bullets_add_group(
							state,
							BulletFlag::bullet16,
							attack.center_x,
							attack.center_y,
							target_player,
							(1U << 4U) + 8U,
							angle,
							bullet_group_ring,
							10U
						);
					} else {
						auto speed = static_cast<std::uint8_t>(1U << 4U);
						const auto angle_delta =
							(state.gauge_pattern_param_b[player_id] != 0U)
							? std::int8_t{7}
							: std::int8_t{-7};
						for(std::size_t firing = 0U; firing < 10U; ++firing) {
							bullets_add_group(
								state,
								state.kotohime_gauge_pattern_type[player_id],
								attack.center_x,
								attack.center_y,
								target_player,
								speed,
								angle,
								bullet_group_ring,
								state.gauge_pattern_param_a[player_id]
							);
							speed = static_cast<std::uint8_t>(speed + 2U);
							angle = static_cast<std::uint8_t>(angle + angle_delta);
						}
					}
					attack.state = 0U;
				}
				attack.kotohime_milestone++;
			} else {
				attack.center_y = exact::wrap_add(
					attack.center_y,
					static_cast<std::int16_t>(attack.kotohime_velocity_y)
				);
				extra_attack_shot_hittest(
					state, target_player, attack.center_x, attack.center_y
				);
				collmap_set_rect_striped(
					state, target_player, attack.center_x, attack.center_y, 6, 6
				);
			}
		} else if(attack.state == 2U) {
			(void)extra_attack_transfer_update(attack);
		} else if(attack.state <= 0x1CU) {
			attack.state++;
		} else {
			attack.frame = 0U;
			attack.state = 1U;
		}
		attack.frame++;
	}
}

void extra_attacks_update_ellen(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot_id = 0U;
		slot_id < extra_attack_ellen_count; ++slot_id) {
		auto &attack = state.extra_attacks[player_id][slot_id];
		if(attack.state == 0U) {
			continue;
		}
		if(attack.state == 1U) {
			for(std::size_t trail = attack.ellen_trail_x.size() - 1U;
				trail > 0U; --trail) {
				attack.ellen_trail_x[trail] = attack.ellen_trail_x[trail - 1U];
				attack.ellen_trail_y[trail] = attack.ellen_trail_y[trail - 1U];
			}
			if(attack.ellen_radius <= 0x0D00) {
				const auto offset = vector2(attack.angle, attack.ellen_radius);
				attack.ellen_trail_x[0] = exact::wrap_add(
					attack.center_x, offset.x
				);
				attack.ellen_trail_y[0] = exact::wrap_add(
					attack.center_y, offset.y
				);
				attack.velocity_x = exact::wrap_subtract(
					attack.ellen_trail_x[0], attack.ellen_trail_x[1]
				);
				attack.velocity_y = exact::wrap_subtract(
					attack.ellen_trail_y[0], attack.ellen_trail_y[1]
				);
				attack.ellen_radius = exact::wrap_add(
					attack.ellen_radius, static_cast<std::int16_t>(0x18)
				);
				attack.angle = static_cast<std::uint8_t>(
					attack.angle + attack.ellen_angle_delta
				);
			} else {
				if(
					playfield_clip(
						attack.ellen_trail_x.front(),
						attack.ellen_trail_y.front(),
						-(24 << 4)
					) &&
					playfield_clip(
						attack.ellen_trail_x.back(),
						attack.ellen_trail_y.back(),
						-(24 << 4)
					)
				) {
					attack.state = 0U;
					continue;
				}
				attack.ellen_trail_x[0] = exact::wrap_add(
					attack.ellen_trail_x[0], attack.velocity_x
				);
				attack.ellen_trail_y[0] = exact::wrap_add(
					attack.ellen_trail_y[0], attack.velocity_y
				);
			}
			extra_attack_shot_hittest(
				state,
				target_player,
				attack.ellen_trail_x[0],
				attack.ellen_trail_y[0]
			);
			collmap_set_rect_striped(
				state,
				target_player,
				attack.ellen_trail_x[0],
				attack.ellen_trail_y[0],
				6,
				6
			);
		} else if(attack.state == 2U) {
			if(extra_attack_transfer_update(attack)) {
				attack.ellen_trail_x.fill(attack.center_x);
				attack.ellen_trail_y.fill(attack.center_y);
			}
		} else if(attack.state <= 0x1CU) {
			attack.state++;
		} else {
			attack.frame = 0U;
			attack.state = 1U;
		}
		attack.frame++;
	}
}

void extra_attacks_update_rikako(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot = 0U; slot < extra_attack_rikako_count; ++slot) {
		auto &attack = state.extra_attacks[player_id][slot];
		if(attack.state == 0U) {
			continue;
		}
		if(attack.state == 1U) {
			const auto velocity = vector2(
				attack.angle, static_cast<std::int16_t>(attack.speed)
			);
			attack.center_x = exact::wrap_add(attack.center_x, velocity.x);
			attack.center_y = exact::wrap_add(attack.center_y, velocity.y);
			if(
				attack.behavior_variant == 0U && attack.frame >= 0x40U &&
				attack.frame <= 0x50U
			) {
				const auto target_angle = iatan2(
					exact::wrap_subtract(
						state.players[target_player].center_y.raw(), attack.center_y
					),
					exact::wrap_subtract(
						state.players[target_player].center_x.raw(), attack.center_x
					)
				);
				attack.speed++;
				const auto angle_delta = std::bit_cast<std::int8_t>(
					static_cast<std::uint8_t>(target_angle - attack.angle)
				);
				attack.angle = static_cast<std::uint8_t>(
					attack.angle + static_cast<std::int8_t>(angle_delta / 8)
				);
			}
			if(playfield_clip(
				attack.center_x, attack.center_y, -(32 << 4)
			)) {
				attack.state = 0U;
				continue;
			}
			extra_attack_shot_hittest(
				state, target_player, attack.center_x, attack.center_y
			);
			collmap_set_rect_striped(
				state, target_player, attack.center_x, attack.center_y, 8, 16
			);
			collmap_set_rect_striped(
				state,
				target_player,
				exact::wrap_subtract(
					attack.center_x, static_cast<std::int16_t>(12 << 4)
				),
				attack.center_y,
				4,
				8
			);
			collmap_set_rect_striped(
				state,
				target_player,
				exact::wrap_add(
					attack.center_x, static_cast<std::int16_t>(12 << 4)
				),
				attack.center_y,
				4,
				8
			);
		} else if(attack.state == 2U) {
			(void)extra_attack_transfer_update(attack);
		} else if(attack.state <= 0x1CU) {
			attack.state++;
		} else {
			attack.frame = 0U;
			attack.state = 1U;
		}
		attack.frame++;
	}
}

void extra_attacks_update_chiyuri(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot_id = 0U; slot_id < extra_attack_count; slot_id += 2U) {
		auto &top = state.extra_attacks[player_id][slot_id];
		if(top.state == 0U) {
			continue;
		}
		auto &bottom = state.extra_attacks[player_id][slot_id + 1U];
		if(top.state == 1U) {
			top.frame++;
			if(top.frame > 0x28U && top.frame < 0x70U) {
				collmap_set_slope_striped(
					state, target_player, top.center_x, bottom.center_x
				);
			} else if(top.frame > 0x88U) {
				top.state = 0U;
				bottom.state = 0U;
			}
			continue;
		}

		bool top_done = false;
		if(top.state == 2U) {
			if(extra_attack_transfer_update(top)) {
				top.center_y = 0;
			}
		} else if(top.state <= 0x1CU) {
			top.state++;
		} else {
			top.frame = 0U;
			top_done = true;
		}
		top.frame++;

		if(bottom.state == 2U) {
			if(extra_attack_transfer_update(bottom)) {
				bottom.center_y = 368 << 4;
			}
		} else if(bottom.state <= 0x1CU) {
			bottom.state++;
		} else {
			bottom.frame = 0U;
			if(top_done) {
				top.state = 1U;
				// ZUN decrements the raw slot pointer back to the top half here.
				// The shared slot[1]++ below therefore advances the top frame a
				// second time and leaves the bottom frame at zero.
				top.frame++;
				continue;
			}
		}
		bottom.frame++;
	}
}

void extra_attacks_update_yumemi(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(auto &attack : state.extra_attacks[player_id]) {
		if(attack.state == 0U) {
			continue;
		}
		if(attack.state == 1U) {
			if(attack.frame <= 8U) {
				attack.yumemi_extent = exact::wrap_add(
					attack.yumemi_extent, std::uint16_t{6U}
				);
			} else if(
				attack.frame >= attack.yumemi_duration &&
				attack.frame < static_cast<std::uint16_t>(
					attack.yumemi_duration + 8U
				)
			) {
				attack.yumemi_extent = exact::wrap_subtract(
					attack.yumemi_extent, std::uint16_t{6U}
				);
			} else if(
				attack.frame >= static_cast<std::uint16_t>(
					attack.yumemi_duration + 8U
				)
			) {
				// The source still executes this frame's hit tests and map writes.
				attack.state = 0U;
			}

			const auto extent_radius = static_cast<std::int16_t>(
				attack.yumemi_extent << 3U
			);
			constexpr std::int16_t narrow_radius = 4 << 4;
			(void)extra_attack_bounds_hittest(
				state,
				target_player,
				exact::wrap_subtract(attack.center_x, extent_radius),
				exact::wrap_add(attack.center_x, extent_radius),
				exact::wrap_subtract(attack.center_y, narrow_radius),
				exact::wrap_add(attack.center_y, narrow_radius)
			);
			(void)extra_attack_bounds_hittest(
				state,
				target_player,
				exact::wrap_subtract(attack.center_x, narrow_radius),
				exact::wrap_add(attack.center_x, narrow_radius),
				exact::wrap_subtract(attack.center_y, extent_radius),
				exact::wrap_add(attack.center_y, extent_radius)
			);
			collmap_set_rect_striped(
				state,
				target_player,
				attack.center_x,
				attack.center_y,
				static_cast<int>(attack.yumemi_extent),
				8
			);
			const auto vertical_center_y = exact::wrap_add(
				attack.center_y,
				static_cast<std::int16_t>(attack.yumemi_extent << 2U)
			);
			const auto extent_signed = exact::from_bits<std::int16_t>(
				attack.yumemi_extent
			);
			const auto vertical_height = static_cast<int>(exact::wrap_add(
				extent_signed,
				static_cast<std::int16_t>(extent_signed / 4)
			));
			collmap_set_rect_striped(
				state,
				target_player,
				attack.center_x,
				vertical_center_y,
				8,
				vertical_height
			);
		} else if(attack.state == 2U) {
			if(extra_attack_transfer_update(attack)) {
				continue;
			}
		} else if(attack.state <= 0x28U) {
			attack.state++;
		} else {
			attack.yumemi_extent = 0x10U;
			attack.frame = 0U;
			attack.state = 1U;
		}
		attack.frame++;
	}
}

void extra_attacks_update(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto character =
		(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
	if(character == 0) {
		extra_attacks_update_reimu(state, player_id);
	} else if(character == 1) {
		extra_attacks_update_mima(state, player_id);
	} else if(character == 2) {
		extra_attacks_update_marisa(state, player_id);
	} else if(character == 3) {
		extra_attacks_update_ellen(state, player_id);
	} else if(character == 4) {
		extra_attacks_update_kotohime(state, player_id);
	} else if(character == 5) {
		extra_attacks_update_kana(state, player_id);
	} else if(character == 6) {
		extra_attacks_update_rikako(state, player_id);
	} else if(character == 7) {
		extra_attacks_update_chiyuri(state, player_id);
	} else if(character == 8) {
		extra_attacks_update_yumemi(state, player_id);
	}
}

void extra_attacks_render_phase_update(DeterministicState &state) noexcept
{
	// Reimu advances this byte in her renderer; Kana already advances it in
	// the update callback.
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		const auto character =
			(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
		if(character != 0) {
			continue;
		}
		for(std::size_t slot = 0U; slot < extra_attack_regular_count; ++slot) {
			auto &attack = state.extra_attacks[player_id][slot];
			if(attack.state != 0U) {
				attack.frame++;
			}
		}
	}
}

void extra_attack_add_direct(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t angle,
	std::uint8_t speed
) noexcept
{
	const auto slot = std::find_if(
		state.extra_attacks[player_id].begin(),
		state.extra_attacks[player_id].begin() + extra_attack_regular_count,
		[](const ExtraAttackState &attack) { return attack.state == 0U; }
	);
	if(slot == state.extra_attacks[player_id].begin() + extra_attack_regular_count) {
		return;
	}
	*slot = ExtraAttackState{};
	slot->state = 1U;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->player_id = static_cast<std::uint8_t>(1U - player_id);
	slot->angle = angle;
	slot->speed = speed;
	const auto velocity = vector2(angle, speed);
	slot->velocity_x = velocity.x;
	slot->velocity_y = velocity.y;
}

void extra_attack_add_direct_marisa(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	const auto end = state.extra_attacks[player_id].begin() +
		extra_attack_marisa_count;
	const auto slot = std::find_if(
		state.extra_attacks[player_id].begin(), end,
		[](const ExtraAttackState &attack) { return attack.state == 0U; }
	);
	if(slot == end) {
		return;
	}
	// marisa_19B06() receives the target PID and selects the opposite buffer.
	// The Boss-side API receives the owner instead, but preserves the source's
	// partial overwrite and therefore all stale, unmentioned slot fields.
	slot->state = 3U;
	slot->frame = 0U;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->player_id = static_cast<std::uint8_t>(1U - player_id);
}

void extra_attack_add_direct_yumemi(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	const auto begin = state.extra_attacks[player_id].begin() +
		extra_attack_regular_count;
	const auto end = state.extra_attacks[player_id].end();
	const auto slot = std::find_if(
		begin, end,
		[](const ExtraAttackState &attack) { return attack.state == 0U; }
	);
	if(slot == end) {
		return;
	}
	// yumemi_1A95F() overwrites only the fields below, preserving the source's
	// stale contents in every other member of the reused slot.
	slot->state = 3U;
	slot->frame = 0U;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->player_id = static_cast<std::uint8_t>(1U - player_id);
	slot->yumemi_extent = 0U;
	slot->yumemi_duration = 0x20U;
}

void extra_attack_add_direct_rikako(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t angle
) noexcept
{
	const auto end = state.extra_attacks[player_id].begin() +
		extra_attack_rikako_count;
	const auto slot = std::find_if(
		state.extra_attacks[player_id].begin(), end,
		[](const ExtraAttackState &attack) { return attack.state == 0U; }
	);
	if(slot == end) {
		return;
	}
	*slot = ExtraAttackState{};
	slot->state = 1U;
	slot->frame = 0x40U;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->player_id = static_cast<std::uint8_t>(1U - player_id);
	slot->angle = angle;
	slot->speed = 0x20U;
	slot->behavior_variant = 1U;
}

void extra_attack_add_direct_ellen(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t angle,
	std::int8_t angle_delta
) noexcept
{
	const auto end = state.extra_attacks[player_id].begin() +
		extra_attack_ellen_count;
	const auto slot = std::find_if(
		state.extra_attacks[player_id].begin(), end,
		[](const ExtraAttackState &attack) { return attack.state == 0U; }
	);
	if(slot == end) {
		return;
	}
	// Ellen's direct allocator overwrites only these fields. In particular,
	// the trail is filled naturally during the initial expanding orbit.
	slot->state = 1U;
	slot->frame = 0U;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->ellen_radius = 0;
	slot->ellen_angle_delta = angle_delta;
	slot->angle = angle;
	slot->player_id = static_cast<std::uint8_t>(1U - player_id);
}

void boss_move(BossState &boss) noexcept
{
	boss.center_x = exact::wrap_add(boss.center_x, boss.velocity_x);
	boss.center_y = exact::wrap_add(boss.center_y, boss.velocity_y);
	boss.motion_angle++;
	boss.velocity_y = vector2(boss.motion_angle, 16).y;
	if(boss.center_x <= (48 << 4)) {
		boss.velocity_x = 32;
	} else if(boss.center_x >= (240 << 4)) {
		boss.velocity_x = -32;
	}
}

void boss_exit_move(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &boss = state.boss;
	boss.center_x = exact::wrap_add(boss.center_x, boss.velocity_x);
	boss.center_y = exact::wrap_add(boss.center_y, static_cast<std::int16_t>(32));
	if(boss.center_x <= (48 << 4)) {
		boss.velocity_x = 32;
	} else if(boss.center_x >= (240 << 4)) {
		boss.velocity_x = -32;
	}
	state.boss_target_active[1U - player_id] = 0U;
	if(boss.center_y >= (416 << 4)) {
		boss.state = 0U;
		state.gba_boss_launched_by = -1;
		state.combo_points_for_boss_attack = 5120U;
	}
}

bool boss_launch_or_reverse(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	if(state.gba_flag_active[player_id] != GaugeBossAttackFlag::boss) {
		return false;
	}
	if(state.gba_boss_launched_by < 0) {
		const auto initial = state.boss_initial[player_id];
		// ZUN keeps this byte outside the 32-byte boss record copied at launch,
		// so a new boss attack inherits the previous attack's last pattern.
		const auto pattern_previous = state.boss.pattern_previous;
		state.boss = BossState{};
		static_cast<BossInitialState &>(state.boss) = initial;
		state.boss.pattern_previous = pattern_previous;
		state.boss.pattern_count = static_cast<std::uint8_t>(
			(randring_next16(state) & 7U) + 1U
		);
		state.boss.pattern_frame = 0U;
		state.gba_boss_launched_by = static_cast<std::int8_t>(player_id);
		state.gba_flag_active[player_id] = GaugeBossAttackFlag::none;
		state.boss_target_active[1U - player_id] = 1U;
		return true;
	}
	if(state.boss.state != 0xFFU) {
		state.boss.state = 0xFFU;
		state.boss.pattern_frame = 0U;
		state.boss_target_active[player_id] = 0U;
	}
	return false;
}

void boss_target_update(DeterministicState &state) noexcept
{
	if(state.boss.state != 0x80U) {
		state.boss_target_x = state.boss.center_x;
		state.boss_target_y = exact::wrap_add(
			state.boss.center_y, static_cast<std::int16_t>(128 << 4)
		);
	}
}

void boss_choose_pattern(DeterministicState &state) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if(boss.pattern_frame < 0x50U) {
		return;
	}
	std::uint8_t next = 0U;
	for(;;) {
		next = static_cast<std::uint8_t>(randring_next16(state) & 0x0FU);
		if((static_cast<int>(boss.pattern_previous) + 2) < next) {
			break;
		}
		if((static_cast<int>(boss.pattern_previous) - 2) <= next) {
			continue;
		}
		break;
	}
	boss.pattern_previous = next;
	boss.pattern_frame = 0U;
	boss.state = static_cast<std::uint8_t>(next + 2U);
	boss.pattern_index++;
	if(boss.pattern_index > boss.pattern_count) {
		boss.state = 0x80U;
	}
}

std::uint8_t boss_hittest_damage(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t radius
) noexcept
{
	if(
		state.boss.center_y < 0 ||
		state.defeat_flag == DefeatFlag::banner
	) {
		return 0U;
	}
	const auto left = exact::wrap_subtract(state.boss.center_x, radius);
	const auto right = exact::wrap_add(state.boss.center_x, radius);
	const auto top = exact::wrap_subtract(state.boss.center_y, radius);
	const auto bottom = exact::wrap_add(state.boss.center_y, radius);
	std::uint8_t damage = 0U;
	for(auto &shot : state.shotpairs) {
		if(!shot.alive || shot.player_id != player_id) {
			continue;
		}
		const auto shot_center_x = exact::wrap_add(
			shot.topleft_x, shotpair_half_width
		);
		const auto shot_center_y = exact::wrap_add(
			shot.topleft_y, shotpair_half_height
		);
		if(
			exact::wrap_subtract(shot_center_x, shotpair_half_width) > right ||
			exact::wrap_add(shot_center_x, shotpair_half_width) < left ||
			shot_center_y < top || shot_center_y > bottom
		) {
			continue;
		}
		shot.alive = false;
		damage = static_cast<std::uint8_t>(damage + 2U);
	}
	damage = static_cast<std::uint8_t>(damage + chargeshot_hittest(
		state, player_id, left, right, top, bottom
	));
	if(
		state.bomb_flag[player_id] != BombFlag::inactive &&
		(state.round_or_result_frame & 1U) == 0U
	) {
		damage++;
	}
	return damage;
}

void boss_defeat_if_needed(
	DeterministicState &state,
	std::uint8_t target_player
) noexcept
{
	if(
		static_cast<std::int16_t>(state.boss.hp) > 0 ||
		state.boss.state == 0xFFU
	) {
		return;
	}
	state.boss.state = 0xFFU;
	state.boss.pattern_frame = 0U;
	if(state.gba_flag_active[target_player] != GaugeBossAttackFlag::boss) {
		state.combo_points_for_boss_attack = 5120U;
	}
	state.boss_target_active[target_player] = 0U;
	if(state.gba_boss_level < 16U) {
		state.gba_boss_level++;
	}
}

void boss_pattern_reimu_orbit_pellets(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 0U) {
		boss.anchor_x = boss.center_x;
		boss.anchor_y = exact::wrap_add(
			boss.center_y, static_cast<std::int16_t>(48 << 4)
		);
		boss.direction = static_cast<std::uint8_t>(randring_next16(state) & 1U);
		boss.pattern_angle = 0x10U;
	}
	auto angle = static_cast<std::uint8_t>(boss.pattern_frame * 2U);
	if(boss.direction != 0U) {
		angle = static_cast<std::uint8_t>(0U - angle);
	}
	if((boss.pattern_frame % boss.params[0]) == 0U) {
		const auto offset = vector2(angle, 32 << 4);
		bullets_add_group(
			state,
			BulletFlag::pellet_cloud,
			exact::wrap_add(boss.center_x, offset.x),
			exact::wrap_add(boss.center_y, offset.y),
			static_cast<std::uint8_t>(1U - player_id),
			boss.pattern_angle,
			static_cast<std::uint8_t>(angle * 2U),
			bullet_group_one
		);
		angle = static_cast<std::uint8_t>(angle + 0x80U);
		const auto opposite = vector2(angle, 32 << 4);
		bullets_add_group(
			state,
			BulletFlag::pellet_cloud,
			exact::wrap_add(boss.center_x, opposite.x),
			exact::wrap_add(boss.center_y, opposite.y),
			static_cast<std::uint8_t>(1U - player_id),
			boss.pattern_angle,
			static_cast<std::uint8_t>((angle - 0x80U) * 2U + 0x80U),
			bullet_group_one
		);
	}
	if((boss.pattern_frame & 1U) == 0U) {
		boss.pattern_angle++;
	}
	const auto y_angle = static_cast<std::uint8_t>(
		static_cast<std::uint8_t>(boss.pattern_frame * 2U) - 0x40U
	);
	boss.center_y = exact::wrap_add(
		boss.anchor_y, vector2(y_angle, 48 << 4).y
	);
	if(boss.pattern_frame >= 0x80U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_reimu_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if(boss.pattern_frame < 0x40U) {
		if((state.round_frame & 0x0FU) != 0U) {
			return;
		}
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[2], 0x20U,
			bullet_group_three_spread_narrow
		);
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[2], 0x60U,
			bullet_group_three_spread_narrow
		);
		return;
	}
	if(boss.pattern_frame < 0x64U) {
		if((state.round_frame & 0x0FU) != 0U) {
			return;
		}
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[2], 0x40U,
			bullet_group_five_spread_medium
		);
		return;
	}
	boss.state = 1U;
	boss.pattern_frame = 0U;
}

void boss_pattern_reimu_orbit_exatt(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if(boss.pattern_frame == 0U) {
		boss.render_state = 1U;
		boss.pattern_angle = static_cast<std::uint8_t>(randring_next16(state));
	}
	const auto count = boss.params[3];
	if(boss.pattern_frame < 0x18U) {
		const auto radius = static_cast<std::int16_t>(boss.pattern_frame << 5U);
		for(std::size_t i = 0U; i < count; ++i) {
			const auto angle = static_cast<std::uint8_t>(
				((i << 8U) / count) + boss.pattern_angle
			);
			const auto offset = vector2(angle, radius);
			boss.orbit_x[i] = exact::wrap_add(boss.center_x, offset.x);
			boss.orbit_y[i] = exact::wrap_add(boss.center_y, offset.y);
		}
		boss.pattern_angle = static_cast<std::uint8_t>(boss.pattern_angle + 8U);
		return;
	}
	if(boss.pattern_frame < 0x50U) {
		for(std::size_t i = 0U; i < count; ++i) {
			const auto angle = static_cast<std::uint8_t>(
				((i << 8U) / count) + boss.pattern_angle
			);
			const auto offset = vector2(angle, 48 << 4);
			boss.orbit_x[i] = exact::wrap_add(boss.center_x, offset.x);
			boss.orbit_y[i] = exact::wrap_add(boss.center_y, offset.y);
		}
		boss.pattern_angle = static_cast<std::uint8_t>(boss.pattern_angle + 8U);
		if((state.round_frame & 0x0FU) == 0U) {
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id), boss.params[4],
				static_cast<std::uint8_t>(randring_next16(state)),
				bullet_group_ring, boss.params[5]
			);
		}
		return;
	}
	if(boss.pattern_frame == 0x50U) {
		for(std::size_t i = 0U; i < count; ++i) {
			const auto angle = static_cast<std::uint8_t>(
				((i << 8U) / count) + boss.pattern_angle
			);
			extra_attack_add_direct(
				state, player_id, boss.orbit_x[i], boss.orbit_y[i], angle, 0x50U
			);
		}
		boss.state = 1U;
		boss.pattern_frame = 0U;
		boss.render_state = 0U;
	}
}

void boss_pattern_reimu_four_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if(boss.pattern_frame < 0x80U) {
		if((boss.pattern_frame & 0x0FU) != 0U) {
			return;
		}
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[2],
			static_cast<std::uint8_t>(boss.pattern_frame),
			bullet_group_four_spread_narrow
		);
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[2],
			static_cast<std::uint8_t>(0x78U - boss.pattern_frame),
			bullet_group_four_spread_narrow
		);
		return;
	}
	boss.state = 1U;
	boss.pattern_frame = 0U;
}

void boss_update_reimu(DeterministicState &state, std::uint8_t player_id) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			5U - (state.gba_boss_level / 5U)
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x18U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 2U) + 0x28U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 8U) + 4U
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x10U
		);
		state.boss.params[5] = state.boss.params[4];
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	if(state.boss.state == 0U) {
		if(state.boss.pattern_frame != 0x64U) {
			return;
		}
		state.boss.pattern_frame = 0U;
		state.boss.state = 1U;
	}
	if(state.boss.state == 1U) {
		boss_choose_pattern(state);
	}
	switch(state.boss.state) {
	case 2U: case 3U: case 4U: case 5U: case 6U:
		boss_pattern_reimu_orbit_pellets(state, player_id);
		break;
	case 8U: case 9U: case 10U: case 13U:
		boss_pattern_reimu_spreads(state, player_id);
		break;
	case 15U: case 16U: case 17U:
		boss_pattern_reimu_orbit_exatt(state, player_id);
		break;
	case 7U: case 11U: case 12U: case 14U:
		boss_pattern_reimu_four_spreads(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 28, 28
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_pattern_mima_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame < 2U) {
		boss.render_state = 2U;
		state.mima_boss_spread_angle = 0U;
		state.mima_boss_pellet_angle = 192U;
		return;
	}
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	if((boss.pattern_frame & 1U) == 0U) {
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			boss.center_x,
			boss.center_y,
			target_player,
			boss.params[0],
			state.mima_boss_spread_angle,
			bullet_group_two_spread_horizontally_symmetric
		);
		state.mima_boss_spread_angle = static_cast<std::uint8_t>(
			state.mima_boss_spread_angle + 9U
		);
		if(
			static_cast<std::uint16_t>(boss.params[5]) > boss.pattern_frame &&
			(boss.pattern_frame % 6U) == 0U
		) {
			bullets_add_group(
				state,
				BulletFlag::pellet,
				boss.center_x,
				boss.center_y,
				target_player,
				boss.params[4],
				state.mima_boss_pellet_angle,
				bullet_group_four_spread_medium
			);
			bullets_add_group(
				state,
				BulletFlag::pellet,
				boss.center_x,
				boss.center_y,
				target_player,
				boss.params[4],
				static_cast<std::uint8_t>(
					0x80U - state.mima_boss_pellet_angle
				),
				bullet_group_four_spread_medium
			);
		}
		state.mima_boss_pellet_angle = static_cast<std::uint8_t>(
			state.mima_boss_pellet_angle + 3U
		);
	}
	if(boss.pattern_frame > 0x82U) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_mima_random_bursts(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 0x10U) {
		boss.render_state = 5U;
	} else if(boss.pattern_frame < 0x20U) {
		return;
	}
	const auto center_x = exact::wrap_add(
		boss.center_x, static_cast<std::int16_t>(24 << 4)
	);
	const auto center_y = exact::wrap_add(
		boss.center_y, static_cast<std::int16_t>(-(32 << 4))
	);
	if(
		boss.pattern_frame == 0x20U || boss.pattern_frame == 0x24U ||
		boss.pattern_frame == 0x28U
	) {
		return;
	}
	if(boss.pattern_frame <= 0x30U) {
		return;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	if((boss.pattern_frame & 3U) == 0U) {
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			center_x,
			center_y,
			target_player,
			boss.params[2],
			0U,
			bullet_group_random_angle_and_speed,
			boss.params[1]
		);
		bullets_add_group(
			state,
			BulletFlag::pellet,
			center_x,
			center_y,
			target_player,
			boss.params[2],
			0U,
			bullet_group_random_angle_and_speed,
			boss.params[1]
		);
	}
	if(boss.pattern_frame == 0x40U) {
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			center_x,
			center_y,
			target_player,
			2U << 4U,
			static_cast<std::uint8_t>(randring_next16(state)),
			bullet_group_thirty_two_ring
		);
	}
	if(boss.pattern_frame >= 0x80U) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_mima_aimed_trails(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	const auto phase_mod = static_cast<std::uint8_t>(
		boss.pattern_frame % boss.params[3]
	);
	boss_move(boss);
	boss.render_state = 5U;
	const auto half_phase = static_cast<std::uint8_t>(boss.params[3] / 2U);
	if(phase_mod == 1U || phase_mod == half_phase) {
		const auto group = (phase_mod == half_phase)
			? bullet_group_four_spread_medium_aimed
			: bullet_group_five_spread_medium_aimed;
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			exact::wrap_add(
				boss.center_x, static_cast<std::int16_t>(24 << 4)
			),
			exact::wrap_add(
				boss.center_y, static_cast<std::int16_t>(-(32 << 4))
			),
			static_cast<std::uint8_t>(1U - player_id),
			(5U << 4U) + 8U,
			0U,
			group,
			0U,
			0U,
			true
		);
	}
	if(boss.pattern_frame >= 150U) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_mima_orbit_clouds(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if(boss.pattern_frame == 1U) {
		state.mima_boss_orbit_angle_delta =
			((randring_next16(state) & 1U) != 0U)
				? std::int8_t{7}
				: std::int8_t{-7};
		state.mima_boss_orbit_angle = static_cast<std::uint8_t>(
			randring_next16(state)
		);
	}
	if((boss.pattern_frame & 1U) == 0U) {
		const auto offset = vector2(state.mima_boss_orbit_angle, 48 << 4);
		bullets_add_group(
			state,
			BulletFlag::pellet_cloud,
			exact::wrap_add(boss.center_x, offset.x),
			exact::wrap_add(boss.center_y, offset.y),
			static_cast<std::uint8_t>(1U - player_id),
			boss.params[6],
			static_cast<std::uint8_t>(
				state.mima_boss_orbit_angle + 0x40U
			),
			bullet_group_two_ring
		);
		state.mima_boss_orbit_angle = static_cast<std::uint8_t>(
			state.mima_boss_orbit_angle + state.mima_boss_orbit_angle_delta
		);
	}
	if(boss.pattern_frame >= 0x48U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_update_mima(DeterministicState &state, std::uint8_t player_id) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x20U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 4U) + 2U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x20U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			0x40U - (state.gba_boss_level * 2U)
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x28U
		);
		state.boss.params[5] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 4U) + 0x40U
		);
		state.boss.params[6] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x36U
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	const auto dispatch_state = state.boss.state;
	switch(dispatch_state) {
	case 0U:
		if(state.boss.pattern_frame != 0x64U) {
			return;
		}
		state.boss.pattern_frame = 0U;
		state.boss.state = 1U;
		break;
	case 1U:
		boss_choose_pattern(state);
		break;
	case 2U: case 3U: case 4U: case 5U:
		boss_pattern_mima_spreads(state, player_id);
		break;
	case 6U: case 7U: case 8U: case 9U: case 10U:
		boss_pattern_mima_random_bursts(state, player_id);
		break;
	case 11U: case 12U: case 13U:
		boss_pattern_mima_aimed_trails(state, player_id);
		break;
	case 14U: case 15U: case 16U: case 17U:
		boss_pattern_mima_orbit_clouds(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 28, 28
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_pattern_marisa_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss.render_state = 1U;
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	if(boss.pattern_frame == 0x20U) {
		state.marisa_boss_spread_angle = 0U;
	}
	if(boss.pattern_frame < 0x50U) {
		if((boss.pattern_frame & 3U) != 0U) {
			return;
		}
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			boss.center_x,
			boss.center_y,
			target_player,
			boss.params[0],
			state.marisa_boss_spread_angle,
			bullet_group_two_spread_horizontally_symmetric
		);
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			boss.center_x,
			boss.center_y,
			target_player,
			static_cast<std::uint8_t>(boss.params[0] / 2U),
			state.marisa_boss_spread_angle,
			bullet_group_two_spread_horizontally_symmetric
		);
		state.marisa_boss_spread_angle = static_cast<std::uint8_t>(
			state.marisa_boss_spread_angle + 7U
		);
		return;
	}
	boss.render_state = 0U;
	boss.state = 1U;
	boss.pattern_frame = 0U;
}

void boss_marisa_add_direct_pair(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t first_x,
	std::int16_t second_x
) noexcept
{
	constexpr auto center_y = static_cast<std::int16_t>(368 << 4);
	extra_attack_add_direct_marisa(state, player_id, first_x, center_y);
	extra_attack_add_direct_marisa(state, player_id, second_x, center_y);
}

void boss_pattern_marisa_four_direct_pairs(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss.render_state = 1U;
	if(boss.pattern_frame < 0x38U) {
		return;
	}
	switch(boss.pattern_frame) {
	case 0x40U:
		boss_marisa_add_direct_pair(state, player_id, 8 << 4, 280 << 4);
		return;
	case 0x50U:
		boss_marisa_add_direct_pair(state, player_id, 56 << 4, 232 << 4);
		return;
	case 0x60U:
		boss_marisa_add_direct_pair(state, player_id, 104 << 4, 184 << 4);
		return;
	case 0x70U:
		boss_marisa_add_direct_pair(state, player_id, 152 << 4, 136 << 4);
		return;
	default:
		break;
	}
	if(boss.pattern_frame == 0x84U) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_marisa_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame & 0x1FU) == 0U) {
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		auto angle = static_cast<std::uint8_t>(randring_next16(state));
		std::uint8_t cloud_count = boss.params[1];
		if(state.gba_boss_level >= 8U) {
			bullets_add_group(
				state,
				BulletFlag::bullet16,
				boss.center_x,
				boss.center_y,
				target_player,
				boss.params[3],
				angle,
				bullet_group_ring,
				boss.params[4]
			);
			angle = static_cast<std::uint8_t>(
				angle + (256U / static_cast<unsigned>(boss.params[4]))
			);
			cloud_count = boss.params[4];
		}
		bullets_add_group(
			state,
			BulletFlag::pellet_cloud,
			boss.center_x,
			boss.center_y,
			target_player,
			boss.params[2],
			angle,
			bullet_group_ring,
			cloud_count
		);
	}
	if(boss.pattern_frame >= 0x82U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_marisa_three_direct_pairs(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss.render_state = 1U;
	if(boss.pattern_frame < 0x38U) {
		return;
	}
	switch(boss.pattern_frame) {
	case 0x40U:
		boss_marisa_add_direct_pair(state, player_id, 32 << 4, 256 << 4);
		return;
	case 0x50U:
		boss_marisa_add_direct_pair(state, player_id, 80 << 4, 208 << 4);
		return;
	case 0x60U:
		boss_marisa_add_direct_pair(state, player_id, 128 << 4, 160 << 4);
		return;
	default:
		break;
	}
	if(boss.pattern_frame == 0x70U) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_update_marisa(DeterministicState &state, std::uint8_t player_id) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 2U) + 0x32U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 2U) + 0x18U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x20U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x0AU
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x16U
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	const auto dispatch_state = state.boss.state;
	switch(dispatch_state) {
	case 0U:
		if(state.boss.pattern_frame != 0x64U) {
			return;
		}
		state.boss.pattern_frame = 0U;
		state.boss.state = 1U;
		break;
	case 1U:
		boss_choose_pattern(state);
		break;
	case 2U: case 3U: case 4U: case 5U:
		boss_pattern_marisa_spreads(state, player_id);
		break;
	case 6U: case 7U: case 17U:
		boss_pattern_marisa_three_direct_pairs(state, player_id);
		break;
	case 8U: case 9U: case 10U:
		boss_pattern_marisa_four_direct_pairs(state, player_id);
		break;
	case 11U: case 12U: case 13U: case 14U: case 15U: case 16U:
		boss_pattern_marisa_rings(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 28, 28
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_pattern_ellen_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 0x18U) {
		state.ellen_boss_spread_angle = 0x40U;
		return;
	}
	if(boss.pattern_frame < 0x1CU) {
		return;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	if(boss.pattern_frame < 0x24U) {
		if((boss.pattern_frame & 1U) != 0U) {
			return;
		}
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			target_player, boss.params[0], state.ellen_boss_spread_angle,
			bullet_group_two_spread_horizontally_symmetric
		);
		state.ellen_boss_spread_angle = static_cast<std::uint8_t>(
			state.ellen_boss_spread_angle + 8U
		);
	} else if(boss.pattern_frame == 0x24U) {
		for(std::size_t i = 0U; i < 8U; ++i) {
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				target_player, boss.params[0], state.ellen_boss_spread_angle,
				bullet_group_two_spread_horizontally_symmetric
			);
			state.ellen_boss_spread_angle = static_cast<std::uint8_t>(
				state.ellen_boss_spread_angle + 8U
			);
		}
	} else if(boss.pattern_frame <= 0x2CU) {
		if((boss.pattern_frame & 1U) != 0U) {
			return;
		}
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			target_player, boss.params[0], state.ellen_boss_spread_angle,
			bullet_group_two_spread_horizontally_symmetric
		);
		state.ellen_boss_spread_angle = static_cast<std::uint8_t>(
			state.ellen_boss_spread_angle + 8U
		);
	}
	if(boss.pattern_frame >= 0x2CU) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_ellen_orbits(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	if(
		boss.pattern_frame == 0x20U || boss.pattern_frame == 0x24U ||
		boss.pattern_frame == 0x28U
	) {
		return;
	}
	if(boss.pattern_frame == 0x30U || boss.pattern_frame == 0x60U) {
		auto angle = static_cast<std::uint8_t>(randring_next16(state));
		const auto count = boss.params[1];
		const auto delta = (boss.pattern_frame == 0x30U)
			? std::int8_t{2} : std::int8_t{-2};
		for(std::uint8_t i = 0U; i < count; ++i) {
			extra_attack_add_direct_ellen(
				state, player_id, boss.center_x, boss.center_y, angle, delta
			);
			angle = static_cast<std::uint8_t>(
				angle + (256U / static_cast<unsigned>(count))
			);
		}
		if(boss.pattern_frame == 0x60U) {
			boss.state = 1U;
			boss.pattern_frame = 0U;
		}
	}
}

void boss_pattern_ellen_pellet_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame % boss.params[3]) == 1U) {
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		bullets_add_group(
			state, BulletFlag::pellet, boss.center_x, boss.center_y,
			target_player, boss.params[2], state.ellen_boss_pellet_angle,
			bullet_group_four_spread_narrow
		);
		bullets_add_group(
			state, BulletFlag::pellet, boss.center_x, boss.center_y,
			target_player, boss.params[2],
			static_cast<std::uint8_t>(state.ellen_boss_pellet_angle + 0x80U),
			bullet_group_four_spread_narrow
		);
		const auto speed_half = static_cast<std::uint8_t>(boss.params[2] / 2U);
		bullets_add_group(
			state, BulletFlag::pellet, boss.center_x, boss.center_y,
			target_player, speed_half,
			static_cast<std::uint8_t>(state.ellen_boss_pellet_angle + 0x80U),
			bullet_group_four_spread_narrow
		);
		bullets_add_group(
			state, BulletFlag::pellet, boss.center_x, boss.center_y,
			target_player, speed_half, state.ellen_boss_pellet_angle,
			bullet_group_four_spread_narrow
		);
		state.ellen_boss_pellet_angle = static_cast<std::uint8_t>(
			state.ellen_boss_pellet_angle + 0x10U
		);
	}
	if(boss.pattern_frame >= 0x60U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_update_ellen(DeterministicState &state, std::uint8_t player_id) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 2U) + 0x32U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 10U) + 4U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x30U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			10U - (state.gba_boss_level / 3U)
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	const auto dispatch_state = state.boss.state;
	if(dispatch_state == 0U) {
		if(state.boss.pattern_frame != 0x64U) {
			return;
		}
		state.boss.pattern_frame = 0U;
		state.boss.state = 1U;
	} else if(dispatch_state == 1U) {
		boss_choose_pattern(state);
	}
	switch(dispatch_state) {
	case 2U: case 3U: case 4U: case 5U:
		boss_pattern_ellen_spreads(state, player_id);
		break;
	case 6U: case 7U: case 8U: case 9U: case 10U:
		boss_pattern_ellen_orbits(state, player_id);
		break;
	case 11U: case 12U: case 13U: case 14U:
	case 15U: case 16U: case 17U:
		boss_pattern_ellen_pellet_spreads(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 20, 24
	);
	const auto damage = boss_hittest_damage(state, target_player, 24 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_kotohime_add_from_orbit(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t group,
	std::uint8_t speed,
	bool randomize_angle
) noexcept
{
	auto &boss = state.boss;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto center_count = boss.params[1];
	for(std::uint8_t i = 0U; i < center_count; ++i) {
		const auto center_angle = static_cast<std::uint8_t>(
			((static_cast<unsigned>(i) << 8U) / center_count) +
			boss.pattern_angle
		);
		const auto offset = vector2(center_angle, boss.anchor_x);
		const auto angle = randomize_angle
			? static_cast<std::uint8_t>(randring_next16(state))
			: std::uint8_t{0U};
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			exact::wrap_add(boss.center_x, offset.x),
			exact::wrap_add(boss.center_y, offset.y),
			target_player,
			speed,
			angle,
			group
		);
	}
}

void boss_pattern_kotohime_random_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		boss.anchor_x = 0;
		boss.render_frame = 0U;
	}
	boss.pattern_angle = static_cast<std::uint8_t>(boss.pattern_angle + 4U);
	if(boss.anchor_x > (64 << 4)) {
		boss.render_state = 2U;
		boss.pattern_angle = static_cast<std::uint8_t>(
			boss.pattern_angle + 2U
		);
		boss.render_frame++;
		if(boss.render_frame > 0x40U) {
			boss_kotohime_add_from_orbit(
				state,
				player_id,
				bullet_group_sixteen_ring,
				boss.params[0],
				true
			);
			boss.render_state = 0U;
			boss.state = 1U;
			boss.pattern_frame = 0U;
		}
		return;
	}
	boss_move(boss);
	boss.anchor_x = exact::wrap_add(
		boss.anchor_x, static_cast<std::int16_t>(0x20)
	);
}

void boss_pattern_kotohime_aimed_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		boss.anchor_x = 0;
		boss.render_frame = 0U;
		boss.pattern_angle = 0U;
	}
	boss.pattern_angle = static_cast<std::uint8_t>(boss.pattern_angle - 2U);
	if(boss.anchor_x > (64 << 4)) {
		boss.render_state = 2U;
		boss.anchor_x = exact::wrap_add(
			boss.anchor_x, static_cast<std::int16_t>(8)
		);
		boss.pattern_angle = static_cast<std::uint8_t>(
			boss.pattern_angle + 2U
		);
		boss.render_frame++;
		if(boss.render_frame > 0x40U) {
			boss_kotohime_add_from_orbit(
				state,
				player_id,
				bullet_group_two_spread_narrow_aimed,
				boss.params[3],
				false
			);
			boss_kotohime_add_from_orbit(
				state,
				player_id,
				bullet_group_five_spread_medium_aimed,
				boss.params[2],
				false
			);
			boss.render_state = 0U;
			boss.state = 1U;
			boss.pattern_frame = 0U;
		}
		return;
	}
	boss_move(boss);
	boss.anchor_x = exact::wrap_add(
		boss.anchor_x, static_cast<std::int16_t>(0x20)
	);
}

void boss_pattern_kotohime_pellet_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame % boss.params[5]) == 1U) {
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		const auto angle = static_cast<std::uint8_t>(boss.pattern_frame);
		constexpr std::array<std::int16_t, 5> offsets_x{
			-(48 << 4), -(24 << 4), 0, 24 << 4, 48 << 4,
		};
		constexpr std::array<std::int16_t, 5> offsets_y{
			0, 12 << 4, 24 << 4, 12 << 4, 0,
		};
		for(std::size_t i = 0U; i < offsets_x.size(); ++i) {
			bullets_add_group(
				state,
				BulletFlag::pellet,
				exact::wrap_add(boss.center_x, offsets_x[i]),
				exact::wrap_add(boss.center_y, offsets_y[i]),
				target_player,
				boss.params[4],
				angle,
				bullet_group_ring,
				4U
			);
		}
	}
	if(boss.pattern_frame >= 0x60U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_kotohime_bursts(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		boss.anchor_x = 0;
		boss.render_frame = 0U;
	}
	boss.pattern_angle = static_cast<std::uint8_t>(boss.pattern_angle - 4U);
	if(boss.anchor_x > (64 << 4)) {
		boss.render_state = 2U;
		boss.anchor_x = exact::wrap_add(
			boss.anchor_x, static_cast<std::int16_t>(8)
		);
		boss.pattern_angle = static_cast<std::uint8_t>(
			boss.pattern_angle - 2U
		);
		boss.render_frame++;
		if(boss.render_frame > 0x40U) {
			const auto target_player = static_cast<std::uint8_t>(1U - player_id);
			const auto center_count = boss.params[1];
			const auto burst_bullet_count = boss.params[6];
			for(std::uint8_t outer = 0U; outer < center_count; ++outer) {
				const auto center_angle = static_cast<std::uint8_t>(
					((static_cast<unsigned>(outer) << 8U) / center_count) +
					boss.pattern_angle
				);
				const auto offset = vector2(center_angle, boss.anchor_x);
				const auto center_x = exact::wrap_add(boss.center_x, offset.x);
				const auto center_y = exact::wrap_add(boss.center_y, offset.y);
				for(std::uint8_t inner = 0U; inner < burst_bullet_count; ++inner) {
					const auto angle = static_cast<std::uint8_t>(
						((static_cast<unsigned>(inner) << 7U) / burst_bullet_count) +
						center_angle + 0x40U
					);
					const auto speed = static_cast<std::uint8_t>(
						((static_cast<unsigned>(inner) * 0x30U) / burst_bullet_count) +
						0x10U
					);
					bullets_add_group(
						state,
						BulletFlag::bullet16,
						center_x,
						center_y,
						target_player,
						speed,
						angle,
						bullet_group_one
					);
				}
			}
			boss.render_state = 0U;
			boss.state = 1U;
			boss.pattern_frame = 0U;
		}
		return;
	}
	boss_move(boss);
	boss.anchor_x = exact::wrap_add(
		boss.anchor_x, static_cast<std::int16_t>(0x20)
	);
}

void boss_update_kotohime(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 3U) + 6U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x20U
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x18U
		);
		state.boss.params[5] = static_cast<std::uint8_t>(
			0x20U - (state.gba_boss_level / 2U)
		);
		state.boss.params[6] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 8U
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	const auto dispatch_state = state.boss.state;
	switch(dispatch_state) {
	case 0U:
		if(state.boss.pattern_frame != 0x64U) {
			return;
		}
		state.boss.pattern_frame = 0U;
		state.boss.state = 1U;
		break;
	case 1U:
		boss_choose_pattern(state);
		break;
	case 2U: case 3U: case 4U: case 5U: case 6U:
		boss_pattern_kotohime_random_rings(state, player_id);
		break;
	case 7U: case 8U: case 9U:
		boss_pattern_kotohime_aimed_spreads(state, player_id);
		break;
	case 12U: case 13U: case 14U:
		boss_pattern_kotohime_pellet_rings(state, player_id);
		break;
	case 10U: case 11U: case 15U: case 16U: case 17U:
		boss_pattern_kotohime_bursts(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 32, 24
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_pattern_kana_accel_ring(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		return;
	}
	if(boss.pattern_frame == 0x10U) {
		boss.render_frame = 0U;
		boss.render_state = 2U;
		return;
	}
	if(boss.pattern_frame == 0x28U) {
		extra_attack_add_direct(state, player_id, boss.center_x, boss.center_y, 224U, 8U);
		extra_attack_add_direct(state, player_id, boss.center_x, boss.center_y, 8U, 8U);
		extra_attack_add_direct(state, player_id, boss.center_x, boss.center_y, 0x78U, 8U);
		extra_attack_add_direct(state, player_id, boss.center_x, boss.center_y, 160U, 8U);
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), 3U << 4U,
			state.bullet_template_angle,
			bullet_group_ring, boss.params[0], 0x80U
		);
		return;
	}
	if(boss.pattern_frame > 0x50U) {
		boss.render_state = 0U;
		boss.pattern_frame = 0U;
		boss.state = 1U;
	}
}

void boss_pattern_kana_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		return;
	}
	if(boss.pattern_frame == 0x10U) {
		boss.render_frame = 0U;
		boss.render_state = 2U;
		return;
	}
	if(boss.pattern_frame >= 0x28U && boss.pattern_frame < 0x50U) {
		if(boss.pattern_frame == 0x28U) {
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id), 1U << 4U, 0U,
				bullet_group_ring, boss.params[1]
			);
			return;
		}
		if(boss.pattern_frame == 0x40U) {
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id), (2U << 4U) + 8U, 0U,
				bullet_group_ring, boss.params[2]
			);
			auto angle = std::uint8_t{0x20U};
			auto speed = std::uint8_t{1U << 4U};
			for(std::size_t i = 0U; i < 0x10U; ++i) {
				bullets_add_group(
					state, BulletFlag::pellet, boss.center_x, boss.center_y,
					static_cast<std::uint8_t>(1U - player_id), speed, angle,
					bullet_group_two_spread_horizontally_symmetric
				);
				angle = static_cast<std::uint8_t>(angle + 6U);
				speed = static_cast<std::uint8_t>(speed + 3U);
			}
			return;
		}
		if(boss.pattern_frame == 0x48U) {
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id), (3U << 4U) + 8U, 0U,
				bullet_group_ring, boss.params[3]
			);
		}
		return;
	}
	if(boss.pattern_frame > 0x60U) {
		boss.render_state = 0U;
		boss.pattern_frame = 0U;
		boss.state = 1U;
	}
}

void boss_pattern_kana_clouds(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame % boss.params[4]) == 0U) {
		auto angle = static_cast<std::uint8_t>(
			(randring_next16(state) & 0x1FU) - 0x10U
		);
		if((randring_next16(state) & 1U) != 0U) {
			angle = static_cast<std::uint8_t>(0x80U - angle);
		}
		extra_attack_add_direct(
			state, player_id, boss.center_x, boss.center_y, angle, 8U
		);
		bullets_add_group(
			state, BulletFlag::pellet_cloud, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), 3U << 4U, 0U,
			bullet_group_ring, boss.params[5]
		);
	}
	if(boss.pattern_frame > 0x64U) {
		boss.render_state = 0U;
		boss.pattern_frame = 0U;
		boss.state = 1U;
	}
}

void boss_pattern_kana_spiral(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		return;
	}
	if(boss.pattern_frame == 0x10U) {
		boss.render_frame = 0U;
		boss.render_state = 2U;
		boss.pattern_angle = static_cast<std::uint8_t>(-0x40);
		switch(randring_next16(state) & 3U) {
		case 0U: boss.angle_delta = -3; break;
		case 1U: boss.angle_delta = -1; break;
		case 2U: boss.angle_delta = 1; break;
		default: boss.angle_delta = 3; break;
		}
		return;
	}
	if(boss.pattern_frame >= 0x20U && boss.pattern_frame < 0x6CU) {
		if((boss.pattern_frame & 3U) == 0U) {
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id), boss.params[6],
				boss.pattern_angle, bullet_group_ring, 5U
			);
		}
		if(boss.pattern_frame > 0x40U) {
			boss.pattern_angle = static_cast<std::uint8_t>(
				boss.pattern_angle + boss.angle_delta
			);
		}
		return;
	}
	if(boss.pattern_frame > 0x70U) {
		boss.render_state = 0U;
		boss.pattern_frame = 0U;
		boss.state = 1U;
	}
}

void boss_update_kana(DeterministicState &state, std::uint8_t player_id) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x18U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x14U
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			0x20U - state.gba_boss_level
		);
		state.boss.params[5] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x18U
		);
		state.boss.params[6] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x40U
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	const auto dispatch_state = state.boss.state;
	switch(dispatch_state) {
	case 0U:
		if(state.boss.pattern_frame != 0x64U) {
			return;
		}
		state.boss.pattern_frame = 0U;
		state.boss.state = 1U;
		break;
	case 1U:
		boss_choose_pattern(state);
		break;
	case 2U: case 3U: case 4U:
		boss_pattern_kana_accel_ring(state, player_id);
		break;
	case 7U: case 8U: case 9U: case 10U: case 11U: case 12U:
		boss_pattern_kana_rings(state, player_id);
		break;
	case 13U: case 14U: case 15U:
		boss_pattern_kana_clouds(state, player_id);
		break;
	case 5U: case 6U: case 16U: case 17U:
		boss_pattern_kana_spiral(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	state.boss.render_frame++;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 32, 24
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_pattern_rikako_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 0x28U) {
		const auto angle_delta = ((randring_next16(state) & 1U) != 0U)
			? static_cast<std::uint8_t>(0x30U)
			: static_cast<std::uint8_t>(-0x30);
		const auto count = boss.params[0];
		for(std::size_t i = 0U; i < count; ++i) {
			const auto angle = static_cast<std::uint8_t>(
				(i << 8U) / count
			);
			const auto offset = vector2(angle, 48 << 4);
			bullets_add_group(
				state,
				BulletFlag::bullet16,
				exact::wrap_add(boss.center_x, offset.x),
				exact::wrap_add(boss.center_y, offset.y),
				static_cast<std::uint8_t>(1U - player_id),
				(1U << 4U) + 12U,
				static_cast<std::uint8_t>(angle + angle_delta),
				bullet_group_five_spread_wide
			);
		}
	}
	if(boss.pattern_frame > 0x50U) {
		boss.pattern_frame = 0U;
		boss.state = 1U;
	}
}

void boss_pattern_rikako_exatt_and_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 0x10U) {
		// SUB_CE5B() is a presentation effect with one shared-ring draw.
		(void)randring_next16(state);
	}
	if(boss.pattern_frame == 0x28U) {
		// This draw is overwritten before first use in ZUN's loop.
		(void)randring_next16(state);
		const auto count = boss.params[1];
		for(std::size_t i = 0U; i < count; ++i) {
			const auto angle = static_cast<std::uint8_t>(
				(i << 8U) / count
			);
			extra_attack_add_direct_rikako(
				state, player_id, boss.center_x, boss.center_y, angle
			);
		}
	}
	if(boss.pattern_frame == 0x40U) {
		auto speed = std::uint8_t{2U << 4U};
		for(std::size_t i = 0U; i < 4U; ++i) {
			speed = static_cast<std::uint8_t>(speed + 8U);
			bullets_add_group(
				state,
				BulletFlag::pellet,
				boss.center_x,
				boss.center_y,
				static_cast<std::uint8_t>(1U - player_id),
				speed,
				static_cast<std::uint8_t>(randring_next16(state)),
				bullet_group_ring,
				boss.params[2]
			);
		}
	}
	if(boss.pattern_frame > 0x50U) {
		boss.pattern_frame = 0U;
		boss.state = 1U;
	}
}

void boss_pattern_rikako_four_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	const auto phase_mod = static_cast<std::uint16_t>(
		boss.pattern_frame % boss.params[3]
	);
	if(phase_mod == 1U) {
		boss.pattern_angle = static_cast<std::uint8_t>(randring_next16(state));
		boss.direction = static_cast<std::uint8_t>(
			(randring_next16(state) & 1U) + 1U
		);
	}
	if(
		phase_mod == 1U || phase_mod == 5U ||
		phase_mod == 9U || phase_mod == 0x0DU
	) {
		const auto type = (boss.direction == 1U)
			? BulletFlag::pellet
			: BulletFlag::bullet16;
		bullets_add_group(
			state, type, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[4],
			boss.pattern_angle, bullet_group_four_spread_narrow
		);
		bullets_add_group(
			state, type, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[4],
			static_cast<std::uint8_t>(boss.pattern_angle + 0x80U),
			bullet_group_four_spread_narrow
		);
	}
	if(boss.pattern_frame >= 0x80U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_update_rikako(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 4U) + 8U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x1CU
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			0x20U - state.gba_boss_level
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x40U
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	const auto dispatch_state = state.boss.state;
	switch(dispatch_state) {
	case 0U:
		if(state.boss.pattern_frame == 0x64U) {
			state.boss.pattern_frame = 0U;
			state.boss.state = 1U;
		} else {
			// The original switch branch skips the common collision tail while
			// Rikako's Boss Attack is still materializing.
			return;
		}
		break;
	case 1U:
		boss_choose_pattern(state);
		break;
	case 2U: case 3U: case 4U: case 5U: case 6U:
		boss_pattern_rikako_spreads(state, player_id);
		break;
	case 8U: case 9U: case 10U: case 11U: case 12U: case 13U:
		boss_pattern_rikako_exatt_and_rings(state, player_id);
		break;
	case 7U: case 14U: case 15U: case 16U: case 17U:
		boss_pattern_rikako_four_spreads(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 32, 24
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_chiyuri_random_effect(DeterministicState &state) noexcept
{
	// SUB_CE5B() is a presentation effect with one shared-ring draw.
	(void)randring_next16(state);
}

void boss_chiyuri_finish_pattern(BossState &boss) noexcept
{
	boss.state = 1U;
	boss.pattern_frame = 0U;
	boss.render_state = 0x20U;
}

void boss_pattern_chiyuri_cardinals(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if((boss.pattern_frame % boss.params[1]) != 1U) {
		return;
	}
	if(boss.pattern_frame == 1U) {
		state.chiyuri_boss_pattern_angle = 224U;
		state.chiyuri_boss_pattern_direction = 0U;
		boss_chiyuri_random_effect(state);
		boss_chiyuri_random_effect(state);
		boss_chiyuri_random_effect(state);
		boss_chiyuri_random_effect(state);
	} else if(boss.pattern_frame >= 0x80U) {
		// ZUN resets the pattern before emitting this invocation's four pellets.
		boss_chiyuri_finish_pattern(boss);
	}

	if(state.chiyuri_boss_pattern_direction == 0U) {
		state.chiyuri_boss_pattern_angle = static_cast<std::uint8_t>(
			state.chiyuri_boss_pattern_angle + 8U
		);
		if(state.chiyuri_boss_pattern_angle == 0x20U) {
			state.chiyuri_boss_pattern_direction = 1U;
			state.chiyuri_boss_pattern_angle = 0x24U;
		}
	} else {
		state.chiyuri_boss_pattern_angle = static_cast<std::uint8_t>(
			state.chiyuri_boss_pattern_angle - 8U
		);
		if(state.chiyuri_boss_pattern_angle == 228U) {
			state.chiyuri_boss_pattern_direction = 0U;
			state.chiyuri_boss_pattern_angle = 224U;
		}
	}

	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	auto angle = state.chiyuri_boss_pattern_angle;
	bullets_add_group(
		state, BulletFlag::pellet,
		exact::wrap_add(boss.center_x, static_cast<std::int16_t>(56 << 4)),
		boss.center_y, target_player, boss.params[0], angle, bullet_group_one
	);
	angle = static_cast<std::uint8_t>(angle + 0x40U);
	bullets_add_group(
		state, BulletFlag::pellet, boss.center_x,
		exact::wrap_add(boss.center_y, static_cast<std::int16_t>(56 << 4)),
		target_player, boss.params[0], angle, bullet_group_one
	);
	angle = static_cast<std::uint8_t>(angle + 0x40U);
	bullets_add_group(
		state, BulletFlag::pellet,
		exact::wrap_add(boss.center_x, static_cast<std::int16_t>(-(56 << 4))),
		boss.center_y, target_player, boss.params[0], angle, bullet_group_one
	);
	angle = static_cast<std::uint8_t>(angle + 0x40U);
	bullets_add_group(
		state, BulletFlag::pellet, boss.center_x,
		exact::wrap_add(boss.center_y, static_cast<std::int16_t>(-(56 << 4))),
		target_player, boss.params[0], angle, bullet_group_one
	);
}

void boss_pattern_chiyuri_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if((boss.pattern_frame % boss.params[3]) != 1U) {
		return;
	}
	boss_chiyuri_random_effect(state);
	if(boss.pattern_frame >= 0x80U) {
		// As above, the final scheduled ring is emitted after the reset.
		boss_chiyuri_finish_pattern(boss);
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto angle = static_cast<std::uint8_t>(randring_next16(state));
	bullets_add_group(
		state, BulletFlag::bullet16, boss.center_x, boss.center_y,
		target_player, boss.params[2], angle, bullet_group_thirty_two_ring
	);
	if(state.gba_boss_level < 8U) {
		return;
	}
	const auto speed = static_cast<std::uint8_t>(boss.params[2] / 4U);
	bullets_add_group(
		state, BulletFlag::pellet,
		exact::wrap_add(boss.center_x, static_cast<std::int16_t>(56 << 4)),
		boss.center_y, target_player, speed, angle, bullet_group_eight_ring
	);
	bullets_add_group(
		state, BulletFlag::pellet,
		exact::wrap_add(boss.center_x, static_cast<std::int16_t>(-(56 << 4))),
		boss.center_y, target_player, speed, angle, bullet_group_eight_ring
	);
	bullets_add_group(
		state, BulletFlag::pellet, boss.center_x,
		exact::wrap_add(boss.center_y, static_cast<std::int16_t>(56 << 4)),
		target_player, speed, angle, bullet_group_eight_ring
	);
	bullets_add_group(
		state, BulletFlag::pellet, boss.center_x,
		exact::wrap_add(boss.center_y, static_cast<std::int16_t>(-(56 << 4))),
		target_player, speed, angle, bullet_group_eight_ring
	);
}

void boss_pattern_chiyuri_trail_ring(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if((boss.pattern_frame & 7U) == 0U) {
		boss_chiyuri_random_effect(state);
	}
	if(boss.pattern_frame != 0x30U) {
		return;
	}
	const auto angle = static_cast<std::uint8_t>(randring_next16(state));
	bullets_add_group(
		state, BulletFlag::bullet16, boss.center_x, boss.center_y,
		static_cast<std::uint8_t>(1U - player_id), boss.params[4], angle,
		bullet_group_ring, 48U, 0U, true
	);
	boss_chiyuri_finish_pattern(boss);
}

void boss_pattern_chiyuri_lattices(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto count = boss.params[6];
	if(boss.pattern_frame == 0x20U) {
		boss_chiyuri_random_effect(state);
		boss_chiyuri_random_effect(state);
		auto speed = boss.params[5];
		for(std::uint8_t pass = 0U; pass < 2U; ++pass) {
			for(std::uint8_t i = 0U; i < count; ++i) {
				const auto offset = static_cast<std::int16_t>(
					((static_cast<unsigned>(i) * 0x70U) / count) << 4U
				);
				const auto center_x = exact::wrap_add(
					exact::wrap_add(
						boss.center_x, static_cast<std::int16_t>(-(56 << 4))
					),
					offset
				);
				const auto angle = static_cast<std::uint8_t>(
					0x60U - ((static_cast<unsigned>(i) << 6U) / count)
				);
				bullets_add_group(
					state, BulletFlag::pellet, center_x, boss.center_y,
					target_player, speed, angle, bullet_group_one
				);
				bullets_add_group(
					state, BulletFlag::pellet, center_x, boss.center_y,
					target_player, speed,
					static_cast<std::uint8_t>(0U - angle), bullet_group_one
				);
				speed = (pass == 0U)
					? static_cast<std::uint8_t>(speed + 2U)
					: static_cast<std::uint8_t>(speed - 2U);
			}
		}
		return;
	}
	if(boss.pattern_frame != 0x40U) {
		return;
	}
	boss_chiyuri_random_effect(state);
	boss_chiyuri_random_effect(state);
	auto speed = boss.params[5];
	for(std::uint8_t pass = 0U; pass < 2U; ++pass) {
		for(std::uint8_t i = 0U; i < count; ++i) {
			const auto offset = static_cast<std::int16_t>(
				((static_cast<unsigned>(i) * 0x70U) / count) << 4U
			);
			const auto center_y = exact::wrap_add(
				exact::wrap_add(
					boss.center_y, static_cast<std::int16_t>(-(56 << 4))
				),
				offset
			);
			const auto angle = static_cast<std::uint8_t>(
				static_cast<int>((static_cast<unsigned>(i) << 6U) / count) -
				0x20
			);
			bullets_add_group(
				state, BulletFlag::pellet, boss.center_x, center_y,
				target_player, speed, angle,
				bullet_group_two_spread_horizontally_symmetric
			);
			speed = (pass == 0U)
				? static_cast<std::uint8_t>(speed + 2U)
				: static_cast<std::uint8_t>(speed - 2U);
		}
	}
	boss_chiyuri_finish_pattern(boss);
}

void boss_update_chiyuri(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x30U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			6U - (state.gba_boss_level / 4U)
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x34U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			0x20U - state.gba_boss_level
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 2U) + 0x38U
		);
		state.boss.params[5] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x10U
		);
		state.boss.params[6] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x0CU
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}

	boss_target_update(state);
	auto &boss = state.boss;
	boss.pattern_frame++;
	const auto dispatch_state = boss.state;
	switch(dispatch_state) {
	case 0U:
		if(boss.pattern_frame == 0x60U) {
			boss.pattern_frame = 0U;
			boss.state = 1U;
			boss.pattern_angle = 0x10U;
			boss.render_state = 0x10U;
		} else if(boss.pattern_frame != 0x50U) {
			// State 0 skips the common tail except for both effect frames.
			return;
		}
		break;

	case 1U:
		if(boss.render_state == 0x10U) {
			constexpr std::array<std::int16_t, 5> target_x{
				48 << 4, 96 << 4, 144 << 4, 192 << 4, 240 << 4,
			};
			constexpr std::array<std::int16_t, 5> target_y{
				112 << 4, 96 << 4, 80 << 4, 96 << 4, 112 << 4,
			};
			const auto point = static_cast<std::size_t>(
				randring_next16(state) % target_x.size()
			);
			boss.center_x = target_x[point];
			boss.center_y = target_y[point];
			boss.pattern_angle = 0x10U;
		}
		if(boss.pattern_frame >= 0x30U) {
			boss.pattern_frame = 0U;
			boss.state = static_cast<std::uint8_t>(
				(randring_next16(state) & 0x0FU) + 2U
			);
			boss.pattern_index++;
			if(boss.pattern_index > boss.pattern_count) {
				boss.state = 0x80U;
			}
		}
		break;

	case 2U: case 3U: case 4U: case 5U:
		boss_pattern_chiyuri_cardinals(state, player_id);
		break;
	case 6U: case 7U: case 8U: case 9U:
		boss_pattern_chiyuri_rings(state, player_id);
		break;
	case 10U: case 11U: case 12U: case 13U:
		boss_pattern_chiyuri_trail_ring(state, player_id);
		break;
	case 14U: case 15U: case 16U: case 17U:
		boss_pattern_chiyuri_lattices(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}

	boss.render_frame = static_cast<std::uint8_t>(
		boss.render_frame + ((state.round_or_result_frame & 3U) == 0U)
	);
	boss.render_frame &= 3U;
	if(boss.pattern_angle != 0U) {
		boss.pattern_angle--;
	}
	if(boss.render_state != 0U) {
		boss.render_state--;
	}

	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, boss.center_x, boss.center_y, 32, 24
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	boss.hit = damage;
	boss.hp = exact::wrap_subtract(
		boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_yumemi_random_effect(DeterministicState &state) noexcept
{
	// SUB_CE5B() is a presentation effect with one shared-ring draw.
	(void)randring_next16(state);
}

BulletFlag boss_yumemi_random_bullet_type(DeterministicState &state) noexcept
{
	return ((randring_next16(state) & 1U) == 0U)
		? BulletFlag::pellet
		: BulletFlag::bullet16;
}

void boss_pattern_yumemi_accel_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		state.yumemi_boss_angle = static_cast<std::uint8_t>(
			randring_next16(state)
		);
		state.yumemi_boss_speed_offset = 0U;
		state.yumemi_boss_radius = 256U;
		state.yumemi_boss_angle_delta =
			((randring_next16(state) & 1U) == 0U) ? 3 : -3;
	}
	if(boss.pattern_frame < 4U) {
		boss_yumemi_random_effect(state);
		return;
	}
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	if(boss.pattern_frame == 0x20U) {
		boss.render_state = 2U;
	}
	if((static_cast<std::uint8_t>(boss.pattern_frame) & 3U) == 0U) {
		bullets_add_group(
			state, BulletFlag::bullet16,
			state.yumemi_boss_muzzle_x, state.yumemi_boss_muzzle_y,
			static_cast<std::uint8_t>(1U - player_id),
			static_cast<std::uint8_t>(
				boss.params[0] + state.yumemi_boss_speed_offset
			),
			state.yumemi_boss_angle, bullet_group_ring, boss.params[1]
		);
		state.yumemi_boss_angle = static_cast<std::uint8_t>(
			state.yumemi_boss_angle + state.yumemi_boss_angle_delta
		);
		state.yumemi_boss_speed_offset = static_cast<std::uint8_t>(
			state.yumemi_boss_speed_offset + 8U
		);
	}
	if(boss.pattern_frame > 0x3CU) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_yumemi_random_groups(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		state.yumemi_boss_target_x =
			state.players[target_player].center_x.raw();
		state.yumemi_boss_target_y =
			state.players[target_player].center_y.raw();
		state.yumemi_boss_angle = iatan2(
			exact::wrap_subtract(
				state.yumemi_boss_target_y, state.yumemi_boss_muzzle_y
			),
			exact::wrap_subtract(
				state.yumemi_boss_target_x, state.yumemi_boss_muzzle_x
			)
		);
		state.yumemi_boss_radius = 256U;
		boss.render_frame = 0x20U;
	}
	if(boss.pattern_frame < 0x10U) {
		if((boss.pattern_frame & 3U) == 1U) {
			boss_yumemi_random_effect(state);
		}
		return;
	}
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	if(boss.pattern_frame == 0x20U) {
		boss.render_state = 2U;
	}
	if((static_cast<std::uint8_t>(boss.pattern_frame) & 1U) == 0U) {
		constexpr std::array<std::uint8_t, 5> groups{
			bullet_group_three_spread_narrow,
			bullet_group_four_spread_narrow,
			bullet_group_two_spread_narrow,
			bullet_group_five_spread_narrow,
			bullet_group_one,
		};
		const auto speed = static_cast<std::uint8_t>(
			(randring_next16(state) & 0x1FU) + boss.params[2]
		);
		const auto type = boss_yumemi_random_bullet_type(state);
		const auto group = groups[
			static_cast<std::size_t>(randring_next16(state) % groups.size())
		];
		bullets_add_group(
			state, type,
			state.yumemi_boss_muzzle_x, state.yumemi_boss_muzzle_y,
			static_cast<std::uint8_t>(1U - player_id), speed,
			state.yumemi_boss_angle, group
		);
	}
	if(boss.pattern_frame > 0x60U) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_yumemi_four_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame % boss.params[3]) == 0U) {
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		const auto type = boss_yumemi_random_bullet_type(state);
		constexpr std::array<std::int16_t, 4> offset_x{
			0, 0, -(32 << 4), 32 << 4,
		};
		constexpr std::array<std::int16_t, 4> offset_y{
			-(32 << 4), 32 << 4, 0, 0,
		};
		for(std::size_t i = 0U; i < offset_x.size(); ++i) {
			bullets_add_group(
				state, type,
				exact::wrap_add(boss.center_x, offset_x[i]),
				exact::wrap_add(boss.center_y, offset_y[i]),
				target_player, 2U << 4U,
				static_cast<std::uint8_t>(randring_next16(state)),
				bullet_group_four_ring
			);
		}
	}
	if(boss.pattern_frame >= 0x60U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_yumemi_crosses(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_angle == 0U) {
		if(boss.center_x > (56 << 4) && boss.center_x < (232 << 4)) {
			boss_move(boss);
			return;
		}
		boss.pattern_angle = 1U;
		boss.pattern_frame = 0U;
		return;
	}

	boss.render_frame = 2U;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	state.yumemi_boss_target_x = state.players[target_player].center_x.raw();
	state.yumemi_boss_target_y = state.players[target_player].center_y.raw();
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	boss_move(boss);
	boss_move(boss);
	if((boss.pattern_frame & 0x0FU) == 0x0FU) {
		boss.render_state = 1U;
		boss_yumemi_random_effect(state);
		extra_attack_add_direct_yumemi(
			state, player_id,
			state.yumemi_boss_target_x, state.yumemi_boss_target_y
		);
		return;
	}
	if(static_cast<std::uint16_t>(boss.params[4]) < boss.pattern_frame) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
		boss.pattern_angle = 0U;
	}
}

void boss_pattern_yumemi_rain(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame % boss.params[5]) == 0U) {
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		if(boss.pattern_frame < 0x40U) {
			bullets_add_group(
				state, BulletFlag::bullet16,
				state.yumemi_boss_muzzle_x, state.yumemi_boss_muzzle_y,
				target_player, 6U << 4U, static_cast<std::uint8_t>(-0x40),
				bullet_group_five_spread_wide
			);
			bullets_add_group(
				state, BulletFlag::bullet16,
				state.yumemi_boss_muzzle_x, state.yumemi_boss_muzzle_y,
				target_player, 6U << 4U, static_cast<std::uint8_t>(-0x40),
				bullet_group_four_spread_wide
			);
		}
		if(boss.pattern_frame >= 0x20U) {
			for(std::size_t i = 0U; i < 12U; ++i) {
				const auto center_x = static_cast<std::int16_t>(
					randring_next16(state) % (288U << 4U)
				);
				const auto angle = static_cast<std::uint8_t>(
					(randring_next16(state) & 0x3FU) + 0x20U
				);
				bullets_add_group(
					state, BulletFlag::bullet16, center_x, 0,
					target_player, (2U << 4U) + 4U, angle,
					bullet_group_one
				);
			}
		}
	}
	if(boss.pattern_frame >= 0x60U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_update_yumemi(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x40U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			0x12U - state.gba_boss_level
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 4U) + 0x50U
		);
		state.boss.params[5] = static_cast<std::uint8_t>(
			0x10U - (state.gba_boss_level / 2U)
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}

	boss_target_update(state);
	auto &boss = state.boss;
	boss.pattern_frame++;
	state.yumemi_boss_muzzle_x = exact::wrap_add(
		boss.center_x, static_cast<std::int16_t>(9 << 4)
	);
	state.yumemi_boss_muzzle_y = exact::wrap_add(
		boss.center_y, static_cast<std::int16_t>(-(24 << 4))
	);
	const auto dispatch_state = boss.state;
	switch(dispatch_state) {
	case 0U:
		if(boss.pattern_frame != 0x60U) {
			return;
		}
		boss.pattern_frame = 0U;
		boss.state = 1U;
		break;
	case 1U:
		boss_choose_pattern(state);
		break;
	case 2U: case 3U: case 4U:
		boss_pattern_yumemi_accel_rings(state, player_id);
		break;
	case 7U: case 8U: case 9U:
		boss_pattern_yumemi_random_groups(state, player_id);
		break;
	case 11U: case 12U: case 13U: case 14U:
		boss_pattern_yumemi_four_rings(state, player_id);
		break;
	case 15U: case 16U: case 17U:
		boss_pattern_yumemi_crosses(state, player_id);
		break;
	case 5U: case 6U: case 10U:
		boss_pattern_yumemi_rain(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}

	if(state.yumemi_boss_radius != 0U) {
		state.yumemi_boss_radius = exact::wrap_subtract(
			state.yumemi_boss_radius, std::uint16_t{8U}
		);
	}
	if(boss.render_frame != 0U) {
		boss.render_frame--;
	}

	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, boss.center_x, boss.center_y, 28, 28
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	boss.hit = damage;
	boss.hp = exact::wrap_subtract(
		boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void bosses_update(DeterministicState &state, std::uint8_t player_id) noexcept
{
	const auto character =
		(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
	if(character == 0) {
		boss_update_reimu(state, player_id);
	} else if(character == 1) {
		boss_update_mima(state, player_id);
	} else if(character == 2) {
		boss_update_marisa(state, player_id);
	} else if(character == 3) {
		boss_update_ellen(state, player_id);
	} else if(character == 4) {
		boss_update_kotohime(state, player_id);
	} else if(character == 5) {
		boss_update_kana(state, player_id);
	} else if(character == 6) {
		boss_update_rikako(state, player_id);
	} else if(character == 7) {
		boss_update_chiyuri(state, player_id);
	} else if(character == 8) {
		boss_update_yumemi(state, player_id);
	}
}

void boss_render_phase_update(DeterministicState &state) noexcept
{
	if(state.gba_boss_launched_by < 0) {
		return;
	}
	const auto owner = static_cast<std::uint8_t>(state.gba_boss_launched_by);
	const auto character =
		(static_cast<int>(state.players[owner].paletted_character) - 1) / 2;
	if(state.boss.state == 0U) {
		// kotohime_12103() advances shared Boss state from the otherwise
		// presentational materialization renderer. Later attack patterns use
		// both values as their orbit angle and radius.
		if(
			character == 4 &&
			(
				(state.round_frame & 1U) == 0U ||
				state.boss.pattern_frame >= 0x40U
			)
		) {
			state.boss.pattern_angle = static_cast<std::uint8_t>(
				state.boss.pattern_angle + 2U
			);
			state.boss.anchor_x = static_cast<std::int16_t>(
				(200U - (state.boss.pattern_frame * 2U)) << 4U
			);
		}
		return;
	}
	if(state.boss.state != 0xFFU) {
		// mima_10053() advances this animation byte from the active Boss
		// Attack renderer. A hit returns before this block, and state 5
		// deliberately resets the frame without changing the state byte.
		if(character == 1 && state.boss.hit == 0U) {
			if(state.boss.render_state == 5U) {
				state.boss.render_frame = 0U;
			}
			if(
				state.boss.render_state == 2U ||
				state.boss.render_state == 5U
			) {
				if(state.boss.render_frame >= 0x20U) {
					state.boss.render_state = 3U;
				}
				state.boss.render_frame++;
			}
		}
		return;
	}
	if(static_cast<std::int16_t>(state.boss.pattern_frame * 8U) >= 200) {
		const auto winner = static_cast<std::uint8_t>(
			1 - state.gba_boss_launched_by
		);
		score_add(state.score[winner], 2560U);
		state.boss.state = 0U;
		state.gba_boss_launched_by = -1;
	}
}

void fireballs_update(DeterministicState &state) noexcept
{
	constexpr std::int16_t fireball_x_min = -(16 << 4);
	constexpr std::int16_t fireball_x_max = (288 + 16) << 4;
	constexpr std::int16_t fireball_y_max = (368 + 16) << 4;
	for(auto &fireball : state.fireballs) {
		if(fireball.flag == FireballFlag::free) {
			continue;
		}
		fireball.frame++;
		if(
			static_cast<std::uint8_t>(fireball.flag) >=
			static_cast<std::uint8_t>(FireballFlag::explosion_ignoring_enemies)
		) {
			if(fireball.frame == 14U) {
				fireball.flag = FireballFlag::explosion_hitting_enemies;
			} else if(fireball.frame == 32U) {
				fireball.flag = FireballFlag::explosion_ignoring_enemies;
			} else if(fireball.frame >= 36U) {
				fireball.flag = FireballFlag::free;
			}
			continue;
		}

		if(fireball.flag == FireballFlag::fall) {
			if(state.bomb_flag[fireball.player_id] != BombFlag::inactive) {
				fireball.flag = FireballFlag::free;
				continue;
			}
			const auto next_x = exact::wrap_add(
				fireball.center_x, fireball.velocity_x
			);
			if(next_x <= fireball_x_min || next_x >= fireball_x_max) {
				fireball.flag = FireballFlag::free;
				continue;
			}
			fireball.center_x = next_x;
			const auto next_y = exact::wrap_add(
				fireball.center_y, fireball.velocity_y
			);
			if(next_y >= fireball_y_max) {
				fireball.flag = FireballFlag::free;
				continue;
			}
			fireball.center_y = next_y;
			collmap_set_fireball(state, fireball);
			continue;
		}

		const auto next_x = exact::wrap_add(
			fireball.center_x, fireball.velocity_x
		);
		const auto reached = (fireball.player_id == 0U)
			? (next_x >= fireball.target_x_for_origin_player)
			: (next_x <= fireball.target_x_for_origin_player);
		if(reached) {
			fireball.center_x = fireball.target_x_for_target_player;
			fireball.center_y = 0;
			fireball.velocity_x = 0;
			fireball.velocity_y = fireball.fall_velocity_y;
			fireball.flag = FireballFlag::fall;
			fireball.player_id ^= 1U;
			continue;
		}
		fireball.center_x = next_x;
		fireball.center_y = exact::wrap_add(
			fireball.center_y, fireball.velocity_y
		);
	}
}

void fireballs_hittest(DeterministicState &state) noexcept
{
	if(state.defeat_flag == DefeatFlag::banner) {
		return;
	}
	// Anniversary initializes this once before the reverse fireball scan. A
	// later non-explosion collision therefore reuses the most recent explosion
	// chain slot, rather than resetting to the fallback for each fireball.
	std::uint8_t explosion_chain_slot = chain_ring_size - 1U;
	for(auto target_it = state.fireballs.rbegin();
		target_it != state.fireballs.rend(); ++target_it) {
		auto &target = *target_it;
		if(target.flag != FireballFlag::fall) {
			continue;
		}
		constexpr std::int16_t radius_x = 12 << 4;
		constexpr std::int16_t radius_y = 10 << 4;
		const auto center_y = exact::wrap_add(
			target.center_y, static_cast<std::int16_t>(3 << 4)
		);
		const auto left = exact::wrap_subtract(target.center_x, radius_x);
		const auto right = exact::wrap_add(target.center_x, radius_x);
		const auto top = exact::wrap_subtract(center_y, radius_y);
		const auto bottom = exact::wrap_add(center_y, radius_y);
		bool hit = false;
		bool explosion_collision = false;

		for(auto &shot : state.shotpairs) {
			if(!shot.alive || shot.player_id != target.player_id) {
				continue;
			}
			const auto shot_center_x = exact::wrap_add(
				shot.topleft_x, shotpair_half_width
			);
			const auto shot_center_y = exact::wrap_add(
				shot.topleft_y, shotpair_half_height
			);
			if(
				exact::wrap_subtract(shot_center_x, shotpair_half_width) > right ||
				exact::wrap_add(shot_center_x, shotpair_half_width) < left ||
				shot_center_y < top || shot_center_y > bottom
			) {
				continue;
			}
			shot.alive = false;
			hit = true;
		}

		const auto against = target.variant_as_explosion_target;
		const auto against_value = static_cast<std::uint8_t>(against * 3U);
		for_each_explosion(state, [&](auto &explosion) {
			if(!explosion_overlaps(
				explosion, target.player_id, left, right, top, bottom
			)) {
				return;
			}
			explosion_collision = true;
			explosion_chain_slot = explosion.chain_slot;
			hit = true;
			auto &hits = state.chains.hits[target.player_id]
				[explosion_chain_slot];
			if(hits != 0xFFU) {
				hits++;
			}
			std::uint16_t bonus = 0U;
			if(against == 2U) {
				bonus = static_cast<std::uint16_t>(100U + (hits * 16U));
				score_add(state.score[target.player_id], 40U);
			} else {
				bonus = static_cast<std::uint16_t>(444U + (hits * 16U));
				score_add(state.score[target.player_id], 80U);
			}
			bonus = static_cast<std::uint16_t>(bonus + state.round_speed);
			if(state.round_speed <= (3U << 4U)) {
				bonus >>= 1U;
			} else if(state.round_speed >= (6U << 4U)) {
				bonus = static_cast<std::uint16_t>(bonus + bonus);
			}
			auto &value = state.chains.pellet_and_fireball_value
				[target.player_id][explosion_chain_slot];
			value = static_cast<std::uint8_t>(value + against_value);
			gauge_avail_add(
				state,
				target.player_id,
				static_cast<std::uint8_t>(against_value * 3U)
			);
			auto &fireball_charge = state.chains.charge_fireball
				[target.player_id][explosion_chain_slot];
			if(value < 4U) {
				fireball_charge = static_cast<std::uint8_t>(fireball_charge + 2U);
			} else if(value < 10U) {
				fireball_charge = static_cast<std::uint8_t>(fireball_charge + 5U);
			} else if(value < 20U) {
				fireball_charge = static_cast<std::uint8_t>(fireball_charge + 2U);
				state.chains.charge_exatt[target.player_id]
					[explosion_chain_slot]++;
			} else {
				state.chains.charge_exatt[target.player_id]
					[explosion_chain_slot] = static_cast<std::uint8_t>(
						state.chains.charge_exatt[target.player_id]
							[explosion_chain_slot] + 2U
					);
			}
			bonus = combo_add(state, target.player_id, hits, bonus);
			fire_point_based_boss_attack_or_panic(
				state,
				bonus,
				target.player_id,
				explosion_chain_slot
			);
			fireball_add_if_charged(
				state,
				target.player_id,
				explosion_chain_slot,
				explosion.center_x,
				explosion.center_y,
				FireballVariant::red
			);
		});

		bool charge_one_hit = false;
		const auto charge_damage = chargeshot_hittest(
			state, target.player_id, left, right, top, bottom,
			&charge_one_hit
		);
		if(charge_damage != 0U) {
			hit = true;
		}
		if(
			state.bomb_flag[target.player_id] != BombFlag::inactive &&
			(state.round_or_result_frame & 1U) == 0U
		) {
			hit = true;
		}
		if(!hit) {
			continue;
		}

		if(target.hp == 0U || explosion_collision || charge_one_hit) {
			state.fireball_generation_previous = target.generation;
			if(explosion_collision) {
				target.flag = FireballFlag::explosion_ignoring_enemies;
				target.chain_slot = explosion_chain_slot;
				auto &extra_charge = state.chains.charge_exatt
					[target.player_id][explosion_chain_slot];
				if(against == 3U) {
					extra_charge++;
				}
				const auto threshold = static_cast<std::uint8_t>(
					7U - (state.round_speed / 32U)
				);
				if(extra_charge >= threshold) {
					extra_charge = 0U;
					extra_attack_add(
						state,
						target.center_x,
						target.center_y,
						target.player_id
					);
				}
				if(state.fireball_generation_previous < 4U) {
					(void)fireball_add(
						state,
						target.center_x,
						target.center_y,
						target.player_id,
						FireballVariant::red
					);
				}
			} else {
				const auto cursor = state.chains.cursor[target.player_id];
				state.chains.charge_exatt[target.player_id][cursor] =
					static_cast<std::uint8_t>(
						state.chains.charge_exatt[target.player_id][cursor] +
						(against - 1U)
					);
				target.flag = FireballFlag::free;
				chain_fire_charged_exatt(
					state,
					target.player_id,
					cursor,
					target.center_x,
					target.center_y
				);
				gauge_avail_add(state, target.player_id, 16U);
				score_add(state.score[target.player_id], 50U);
				(void)fireball_add(
					state,
					target.center_x,
					target.center_y,
					target.player_id,
					FireballVariant::red
				);
			}
			chain_fire_charged_exatt(
				state,
				target.player_id,
				explosion_chain_slot,
				target.center_x,
				target.center_y
			);
		} else {
			target.hp--;
		}
		target.frame = 0U;
	}
}

[[maybe_unused]] bool player_hittest(
	const DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t hitbox_size,
	std::int16_t &collision_x,
	std::int16_t &collision_y
) noexcept
{
	const auto &player = state.players[player_id];
	if(player.invincibility_time != 0U || player.hyper_active != 0U) {
		return false;
	}

	auto memory_read = [&](std::uint16_t offset) {
		const auto collmap_offset = static_cast<std::uint16_t>(
			offset - anniversary_collmap_dgroup_offset
		);
		constexpr auto collmap_bytes = static_cast<std::uint16_t>(
			player_count * (collmap_width / 8U) * collmap_height
		);
		if(collmap_offset < collmap_bytes) {
			const auto playfield = static_cast<std::size_t>(
				collmap_offset / ((collmap_width / 8U) * collmap_height)
			);
			const auto within_playfield = static_cast<std::size_t>(
				collmap_offset % ((collmap_width / 8U) * collmap_height)
			);
			const auto byte_x = within_playfield / collmap_height;
			const auto y = within_playfield % collmap_height;
			std::uint8_t packed = 0U;
			for(std::size_t bit = 0U; bit < 8U; ++bit) {
				if(state.collision_map[playfield][(byte_x * 8U) + bit][y]) {
					packed |= static_cast<std::uint8_t>(0x80U >> bit);
				}
			}
			return packed;
		}
		const auto runtime_offset = static_cast<std::uint16_t>(
			offset - anniversary_runtime_profile_offset
		);
		if(
			runtime_offset <
			anniversary_borland_runtime_collision_alias_profile.size()
		) {
			return anniversary_borland_runtime_collision_alias_profile[
				runtime_offset
			];
		}
		return std::uint8_t{0U};
	};

	const auto radius = static_cast<std::int16_t>(hitbox_size >> 1U);
	const auto center_x = exact::arithmetic_shift_right(
		player.center_x.raw(), 5U
	);
	const auto center_y = exact::arithmetic_shift_right(
		player.center_y.raw(), 5U
	);
	const auto left = static_cast<std::int16_t>(center_x - radius);
	auto top = static_cast<std::int16_t>(center_y - radius);
	auto bottom = static_cast<std::int16_t>(top + hitbox_size);
	if(top < 0) {
		top = 0;
	} else if(bottom >= static_cast<std::int16_t>(collmap_height)) {
		bottom = static_cast<std::int16_t>(collmap_height - 1U);
	}
	const auto first_bit = static_cast<std::uint8_t>(left & 7);
	auto byte_x = static_cast<std::int8_t>(
		exact::arithmetic_shift_right(left, 3U)
	);
	auto width_remaining = static_cast<std::int16_t>(hitbox_size + first_bit);
	std::uint8_t mask = static_cast<std::uint8_t>(0xFFU >> first_bit);
	if(width_remaining <= 8) {
		mask ^= static_cast<std::uint8_t>(0xFFU >> width_remaining);
	}
	constexpr auto collmap_size = static_cast<std::uint16_t>(
		(collmap_width / 8U) * collmap_height
	);
	auto memory_offset = static_cast<std::uint16_t>(
		anniversary_collmap_dgroup_offset +
		(static_cast<std::uint8_t>(byte_x) * collmap_height) +
		(player_id * collmap_size) + static_cast<std::uint16_t>(top)
	);
	// CL still contains first_bit until the first in-range column initializes
	// the stride. This register alias is observable after a negative byte_x.
	auto column_stride = first_bit;
	const auto tile_bottom = static_cast<std::uint8_t>(bottom);
	do {
		if(byte_x >= static_cast<std::int8_t>(collmap_width / 8U)) {
			break;
		}
		if(byte_x >= 0) {
			auto tile_y = static_cast<std::uint8_t>(top);
			column_stride = static_cast<std::uint8_t>(collmap_height);
			do {
				if((memory_read(memory_offset) & mask) != 0U) {
					collision_x = static_cast<std::int16_t>(byte_x * 256);
					collision_y = static_cast<std::int16_t>(top * 32);
					return true;
				}
				memory_offset++;
				column_stride--;
				tile_y++;
			} while(tile_y < tile_bottom);
		}
		byte_x++;
		memory_offset = static_cast<std::uint16_t>(
			memory_offset + column_stride
		);
		width_remaining = static_cast<std::int16_t>(width_remaining - 8);
		mask = 0xFFU;
		if(static_cast<std::uint16_t>(width_remaining) < 8U) {
			mask = static_cast<std::uint8_t>(~(
				0xFFU >> static_cast<std::uint8_t>(width_remaining)
			));
		}
	} while(width_remaining > 0);
	return false;
}

bool enemies_update(
	DeterministicState &state,
	const GameContent &content
) noexcept
{
	state.enemies_alive = {};
	for(auto &enemy : state.enemies) {
		if(enemy.flag == EnemyFlag::free) {
			continue;
		}
		enemy.frame++;
		if(
			static_cast<std::uint8_t>(enemy.flag) >=
			static_cast<std::uint8_t>(EnemyFlag::explosion_ignoring_enemies)
		) {
			if(enemy.frame == 10U) {
				enemy.flag = EnemyFlag::explosion_hitting_enemies;
			} else if(enemy.frame == 32U) {
				enemy.flag = EnemyFlag::explosion_ignoring_enemies;
			} else if(enemy.frame >= 36U) {
				enemy.flag = EnemyFlag::free;
			}
			continue;
		}
		if(!enemy_run(
			enemy, content, state.players[enemy.player_id], state.enemy_speed
		)) {
			return false;
		}
		if(enemy.flag != EnemyFlag::free) {
			state.enemies_alive[enemy.player_id]++;
			if(enemy.flag == EnemyFlag::running_spawned) {
				collmap_set_enemy(state, enemy);
			}
		}
	}

	for(auto target_it = state.enemies.rbegin(); target_it != state.enemies.rend(); ++target_it) {
		auto &target = *target_it;
		if(target.flag != EnemyFlag::running_spawned || target.center_y <= 0) {
			continue;
		}
		if(state.damage_all_on[target.player_id] != 0U) {
			target.hp = 0U;
			target.flag = EnemyFlag::explosion_ignoring_enemies;
			target.frame = 0U;
			const auto slot = state.chains.cursor[target.player_id];
			target.chain_slot = slot;
			state.chains.hits[target.player_id][slot] = 1U;
			state.chains.pellet_and_fireball_value[target.player_id][slot] = 0U;
			state.chains.cursor[target.player_id] = static_cast<std::uint8_t>(
				(slot + 1U) % chain_ring_size
			);
			continue;
		}
		if(state.defeat_flag == DefeatFlag::banner) {
			continue;
		}
		const auto radius = static_cast<std::int16_t>(
			(target.size_pixels == 16U) ? (10 << 4) : (target.size_pixels * 7)
		);
		const auto left = exact::wrap_subtract(target.center_x, radius);
		const auto right = exact::wrap_add(target.center_x, radius);
		const auto top = exact::wrap_subtract(target.center_y, radius);
		const auto bottom = exact::wrap_add(target.center_y, radius);
		bool hit = false;
		bool explosion_collision = false;
		std::uint8_t explosion_chain_slot = 0U;

		for(auto &shot : state.shotpairs) {
			if(!shot.alive || shot.player_id != target.player_id) {
				continue;
			}
			const auto shot_center_x = exact::wrap_add(
				shot.topleft_x, shotpair_half_width
			);
			const auto shot_center_y = exact::wrap_add(
				shot.topleft_y, shotpair_half_height
			);
			if(
				exact::wrap_subtract(shot_center_x, shotpair_half_width) > right ||
				exact::wrap_add(shot_center_x, shotpair_half_width) < left ||
				shot_center_y < top || shot_center_y > bottom
			) {
				continue;
			}
			shot.alive = false;
			hit = true;
		}

		for_each_explosion(state, [&](auto &explosion) {
			const auto flag = static_cast<std::uint8_t>(explosion.flag);
			if(
				flag < static_cast<std::uint8_t>(EnemyFlag::explosion_ignoring_enemies) ||
				explosion.player_id != target.player_id ||
				flag == static_cast<std::uint8_t>(
					EnemyFlag::explosion_ignoring_enemies
				) ||
				flag >= static_cast<std::uint8_t>(EnemyFlag::explosion_hitting_enemies) +
					static_cast<std::uint8_t>(
						explosion_max_enemy_hits_half(explosion) * 2U
					) ||
				(explosion.frame & 3U) != 0U
			) {
				return;
			}
			const auto explosion_radius = static_cast<std::int16_t>(
				explosion.size_pixels * 6
			);
			if(
				right < exact::wrap_subtract(explosion.center_x, explosion_radius) ||
				bottom < exact::wrap_subtract(explosion.center_y, explosion_radius) ||
				left > exact::wrap_add(explosion.center_x, explosion_radius) ||
				top > exact::wrap_add(explosion.center_y, explosion_radius)
			) {
				return;
			}
			explosion_enemy_hit_increment(explosion);
			explosion_collision = true;
			explosion_chain_slot = explosion.chain_slot;
			hit = true;
		});
		bool charge_one_hit = false;
		const auto charge_damage = chargeshot_hittest(
			state, target.player_id, left, right, top, bottom,
			&charge_one_hit
		);
		if(charge_damage != 0U) {
			hit = true;
		}
		if(
			state.bomb_flag[target.player_id] != BombFlag::inactive &&
			(state.round_or_result_frame & 1U) == 0U
		) {
			hit = true;
		}

		if(!hit) {
			continue;
		}
		if(target.hp != 0U && !charge_one_hit) {
			target.hp--;
			target.frame = 0U;
			continue;
		}

		target.flag = EnemyFlag::explosion_ignoring_enemies;
		target.frame = 0U;
		if(!explosion_collision) {
			const auto slot = state.chains.cursor[target.player_id];
			target.chain_slot = slot;
			state.chains.hits[target.player_id][slot] = 1U;
			state.chains.pellet_and_fireball_value[target.player_id][slot] = 0U;
			state.chains.cursor[target.player_id] = static_cast<std::uint8_t>(
				(slot + 1U) % chain_ring_size
			);
		} else {
			target.chain_slot = explosion_chain_slot;
			auto &hits = state.chains.hits[target.player_id][explosion_chain_slot];
			if(hits != 0xFFU) {
				hits++;
			}
			gauge_avail_add(state, target.player_id, 32U);
			const auto bonus = combo_add(
				state, target.player_id, hits,
				static_cast<std::uint16_t>(hits * 16U)
			);
			fire_point_based_boss_attack_or_panic(
				state, bonus, target.player_id, explosion_chain_slot
			);
			const auto charge_slot = static_cast<std::uint8_t>(
				state.round_speed / 32U
			);
			if(hits <= 2U) {
				state.chains.charge_fireball[target.player_id][charge_slot]++;
			} else if(hits <= static_cast<std::uint8_t>(9U - charge_slot)) {
				std::uint16_t pellet_speed = 24U;
				pellet_speed = static_cast<std::uint16_t>(
					((pellet_speed * state.round_speed) >> 7U) + pellet_speed
				);
				pellet_speed = std::min<std::uint16_t>(pellet_speed, 0x7FU);
				(void)bullet_add_transfer_pellet(
					state,
					target.center_x,
					target.center_y,
					target.player_id,
					static_cast<std::uint8_t>(pellet_speed)
				);
				state.chains.charge_fireball[target.player_id][charge_slot] =
					static_cast<std::uint8_t>(
						state.chains.charge_fireball[target.player_id][charge_slot] + 2U
					);
			} else if(hits <= static_cast<std::uint8_t>(14U - (charge_slot * 2U))) {
				state.chains.charge_fireball[target.player_id][charge_slot] =
					static_cast<std::uint8_t>(
						state.chains.charge_fireball[target.player_id][charge_slot] + 3U
					);
				if((hits & 1U) == 0U) {
					std::uint16_t pellet_speed = 24U;
					pellet_speed = static_cast<std::uint16_t>(
						((pellet_speed * state.round_speed) >> 7U) + pellet_speed
					);
					pellet_speed = std::min<std::uint16_t>(pellet_speed, 0x7FU);
					(void)bullet_add_transfer_pellet(
						state,
						target.center_x,
						target.center_y,
						target.player_id,
						static_cast<std::uint8_t>(pellet_speed)
					);
					state.chains.charge_exatt[target.player_id][charge_slot]++;
				}
			} else {
				state.chains.charge_fireball[target.player_id][charge_slot] =
					static_cast<std::uint8_t>(
						state.chains.charge_fireball[target.player_id][charge_slot] + 4U
					);
				state.chains.charge_exatt[target.player_id][charge_slot]++;
			}
			chain_fire_charged_exatt(
				state,
				target.player_id,
				charge_slot,
				target.center_x,
				target.center_y
			);
			fireball_add_if_charged(
				state,
				target.player_id,
				explosion_chain_slot,
				target.center_x,
				target.center_y
			);
		}
		score_add(
			state.score[target.player_id],
			static_cast<std::uint16_t>(
				target.explosion_max_enemy_hits_half * 10U
			)
		);
	}
	return true;
}

void hud_start_update_prefix(DeterministicState &state) noexcept
{
	if(state.hud_start_flag == 1U) {
		state.hud_start_state = 0U;
		state.hud_start_frame = 0U;
		state.hud_start_flag = 2U;
	}
	if(state.hud_start_state == 0U) {
		// The first banner chunk is also the last to settle. Its velocity passes
		// through zero and becomes negative before it returns to x=64.
		state.hud_start_frame++;
		if(state.hud_start_frame >= 69U) {
			state.hud_start_state = 1U;
			state.hud_start_frame = 0U;
		}
	} else if(state.hud_start_state == 1U) {
		state.hud_start_frame++;
		if(state.hud_start_frame > 0x56U) {
			state.hud_start_state = 2U;
			state.hud_start_frame = 0U;
		}
	} else if(state.hud_start_state == 2U) {
		// The particle initialization is gameplay-relevant despite being purely
		// visual: each of 128 particles consumes two 16-bit ring values and two
		// master.lib IRAND values, in that order.
		for(std::size_t i = 0; i < 128U; ++i) {
			(void)(randring_next16(state) & 511U);
			(void)randring_next16(state);
			(void)(irand(state.random_seed) % 3U);
			(void)(irand(state.random_seed) % 3U);
		}
		state.hud_start_state = 3U;
		state.hud_start_frame = 0U;
	} else if(state.hud_start_state == 3U) {
		state.hud_start_frame++;
		if(state.hud_start_frame >= 0x40U) {
			state.hud_start_state = 4U;
		}
	} else {
		state.hud_start_flag = 0U;
	}
}

void background_update(DeterministicState &state) noexcept
{
	if(state.background_phase != BackgroundPhase::initial) {
		return;
	}

	if(state.background_initial_kind == BackgroundInitialKind::type_a) {
		// sub_D135(): 24 GRCG particles followed by 24 sprite particles.
		for(std::size_t i = 0; i < 48U; ++i) {
			(void)irand(state.random_seed);
		}
		if(state.round_frame > 128U) {
			state.background_phase = BackgroundPhase::type_a_steady;
		}
	} else {
		// sub_D340(): 24 GRCG particles, then only the first 16 of its
		// 26 sprite particles call irand(). These presentation calls share the
		// gameplay RNG and are therefore part of deterministic call order.
		for(std::size_t i = 0; i < 40U; ++i) {
			(void)irand(state.random_seed);
		}
		if(state.round_frame > 144U) {
			state.background_phase = BackgroundPhase::type_b_steady;
		}
	}
}

void result_bonus_update(DeterministicState &state) noexcept
{
	const auto winner = static_cast<std::uint8_t>(1U - state.last_fatal_player);
	if(winner >= player_count) {
		return;
	}
	if(state.round_or_result_frame == 1U) {
		const auto &player = state.players[winner];
		state.result_bonus_remaining =
			(static_cast<std::uint32_t>(player.combo_hits_max) * 1000U) +
			(static_cast<std::uint32_t>(player.gauge_attacks_fired) * 10000U) +
			(static_cast<std::uint32_t>(player.boss_attacks_fired) * 15000U) +
			(static_cast<std::uint32_t>(player.boss_attacks_reversed) * 20000U) +
			(static_cast<std::uint32_t>(player.boss_panics_fired) * 30000U);
		if(state.story_stage == 8U) {
			state.result_bonus_remaining +=
				static_cast<std::uint32_t>(state.story_lives) * 100000U;
			state.extends_gained = extends_disabled;
		}
		const auto per_frame = state.result_bonus_remaining / 192U;
		state.result_bonus_per_frame = static_cast<std::uint16_t>(
			std::min<std::uint32_t>(per_frame, 0xFFFFU)
		);
	}
	if(state.result_bonus_per_frame < state.result_bonus_remaining) {
		score_add(state.score[winner], state.result_bonus_per_frame);
		state.result_bonus_remaining -= state.result_bonus_per_frame;
	} else if(state.result_bonus_remaining != 0U) {
		score_add(
			state.score[winner],
			static_cast<std::uint16_t>(state.result_bonus_remaining)
		);
		state.result_bonus_remaining = 0U;
	}
}

bool result_control_update(DeterministicState &state) noexcept
{
	if(state.defeat_flag != DefeatFlag::banner) {
		return false;
	}
	const auto start_result_transition = [&state]() {
		state.result_phase = ResultPhase::opening;
	};
	const auto finish_route = [&state](std::uint8_t route) {
		if(!state.result_transition_complete) {
			return false;
		}
		state.route = route;
		state.round_complete = true;
		return true;
	};
	const auto result_gate = static_cast<std::uint16_t>(
		(state.story_stage < 8U) ? 320U : 460U
	);

	if(state.game_mode != game_mode_story) {
		const auto match_complete =
			state.players[0].rounds_won >= 2U ||
			state.players[1].rounds_won >= 2U;
		if(match_complete) {
			result_bonus_update(state);
			if(state.round_or_result_frame == result_gate) {
				start_result_transition();
			}
			return finish_route(2U);
		}
		if(state.round_or_result_frame == 160U) {
			start_result_transition();
		}
		return finish_route(1U);
	}

	if(state.winner == 0) {
		result_bonus_update(state);
		if(state.round_or_result_frame == result_gate) {
			start_result_transition();
		}
		return finish_route(2U);
	}
	if(state.round_or_result_frame == 160U) {
		start_result_transition();
	}
	if(!state.result_transition_complete) {
		return false;
	}
	if(state.story_lives != 0U) {
		state.story_lives--;
		state.route = 1U;
	} else {
		state.route = 2U;
	}
	state.round_complete = true;
	return true;
}

void result_transition_update(DeterministicState &state) noexcept
{
	if(state.result_phase == ResultPhase::idle) {
		return;
	}
	state.result_phase_frame++;
	if(state.result_phase_frame < 16U) {
		return;
	}
	state.result_phase_frame = 0U;
	if(state.result_phase == ResultPhase::opening) {
		state.result_transition_complete = true;
		state.result_phase = ResultPhase::closing;
	} else {
		state.result_phase = ResultPhase::idle;
	}
}

void story_extend_update(DeterministicState &state) noexcept
{
	if(
		state.game_mode != game_mode_story ||
		state.extends_gained >= state.score[0][6]
	) {
		return;
	}
	state.story_lives = static_cast<std::uint8_t>(state.story_lives + 1U);
	state.extends_gained = static_cast<std::uint8_t>(
		state.extends_gained + 1U
	);
	if(state.extends_gained == extends_max) {
		state.extends_gained = extends_disabled;
	}
}

void round_mode_update(DeterministicState &state) noexcept
{
	if(state.demo_number == 0U || state.game_mode != game_mode_demo) {
		return;
	}
	if(state.demo_frame < 7000U) {
		state.demo_frame++;
	} else if(state.demo_frame == 7000U) {
		// The source reassigns the same opening callback every frame without
		// resetting that callback's independent phase byte.
		state.result_phase = ResultPhase::opening;
	}
	if(state.result_transition_complete || state.input_system != 0U) {
		state.session_exit_requested = true;
	}
}

} // namespace

bool player_hittest_legacy(
	const DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t hitbox_size,
	std::int16_t &collision_x,
	std::int16_t &collision_y
) noexcept
{
	return player_hittest(
		state, player_id, hitbox_size, collision_x, collision_y
	);
}

bool gameplay_frame_prefix(
	DeterministicState &state,
	const GameContent &content
) noexcept
{
	state.collision_map = {};
	state.bullet_template_angle = 0U;
	extra_attacks_update(state, 0U);
	bosses_update(state, 0U);
	bomb_character_update(state, 0U);
	extra_attacks_update(state, 1U);
	bosses_update(state, 1U);
	bomb_character_update(state, 1U);
	shots_update(state);
	chargeshots_update(state);
	hud_start_update_prefix(state);
	if(state.hud_start_flag == 0U) {
		if(content.enemy_formations.empty()) {
			return false;
		}
		if(!add_formation(state, content, 0U) || !add_formation(state, content, 1U)) {
			return false;
		}
	}
	bullets_update(state);
	if(!enemies_update(state, content)) {
		return false;
	}
	fireballs_update(state);
	gauge_patterns_update(state, 0U);
	gauge_patterns_update(state, 1U);
	return true;
}

void gameplay_input_latch(
	DeterministicState &state,
	CoreInput &input,
	std::uint8_t autofire_mask
) noexcept
{
	auto apply = [&](std::uint16_t value, std::uint8_t player_id) {
		if(state.players[player_id].is_cpu != 0U) {
			return value;
		}
		if((autofire_mask & (1U << player_id)) == 0U) {
			return value;
		}
		if((input.charge_mask & (1U << player_id)) != 0U) {
			return static_cast<std::uint16_t>(value | input_shot);
		}
		if(
			(value & input_shot) != 0U &&
			state.autofire_shot_gate[player_id] <= 3U
		) {
			return static_cast<std::uint16_t>(value & ~input_shot);
		}
		return value;
	};
	input.p1 = apply(input.p1, 0U);
	input.p2 = apply(input.p2, 1U);
	state.input_player = {input.p1, input.p2};
	state.input_system = input.system;
	state.global_frame++;
}

bool warning_interstitial_begin(DeterministicState &state) noexcept
{
	if(
		state.warning_flag[0] != WarningFlag::flash_white &&
		state.warning_flag[1] != WarningFlag::flash_white
	) {
		return false;
	}
	for(auto &flag : state.warning_flag) {
		if(flag == WarningFlag::flash_white) {
			flag = WarningFlag::flash_red;
		}
	}
	return true;
}

void warning_frame_update(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.warning_flag[player_id];
	if(static_cast<std::uint8_t>(flag) <
		static_cast<std::uint8_t>(WarningFlag::flash_red)) {
		return;
	}
	if(static_cast<std::uint8_t>(flag) >=
		static_cast<std::uint8_t>(WarningFlag::flash_red_end)) {
		flag = WarningFlag::none;
		state.gba_flag_active[player_id] = state.gba_flag_next[player_id];
		return;
	}
	flag = static_cast<WarningFlag>(static_cast<std::uint8_t>(flag) + 1U);
}

void warning_portrait_update(DeterministicState &state) noexcept
{
	for(auto &flag : state.warning_flag) {
		if(flag == WarningFlag::portrait) {
			flag = WarningFlag::flash_white;
		}
	}
}

bool gameplay_frame_suffix_before_interstitial(DeterministicState &state) noexcept
{
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		if(state.players[player_id].is_cpu != 0U) {
			player_update_cpu(state, player_id);
		} else {
			player_update_human(
				state, player_id, state.input_player[player_id]
			);
		}
	}
	if(
		(state.input_system & input_cancel) != 0U &&
		state.players[0].is_cpu != 0U && state.players[1].is_cpu != 0U
	) {
		state.session_exit_requested = true;
	}
	return warning_interstitial_begin(state);
}

void interstitial_input_latch(
	DeterministicState &state,
	const CoreInput &input
) noexcept
{
	state.input_player = {input.p1, input.p2};
	state.input_system = input.system;
	state.global_frame++;
	// SUB_CA3C() calls the ordinary Cancel handler after every one of its 27
	// held-input samples. CPU-vs-CPU therefore remembers an exit requested by
	// any interstitial sample, even if later samples release Cancel.
	if(
		(input.system & input_cancel) != 0U &&
		state.players[0].is_cpu != 0U && state.players[1].is_cpu != 0U
	) {
		state.session_exit_requested = true;
	}
}

void gameplay_frame_suffix_after_interstitial(DeterministicState &state) noexcept
{
	warning_frame_update(state, 0U);
	warning_frame_update(state, 1U);
	background_update(state);
	bombs_render_phase_update(state);
	boss_render_phase_update(state);
	fireballs_hittest(state);
	extra_attacks_render_phase_update(state);
	if(result_control_update(state)) {
		return;
	}
	story_extend_update(state);
	warning_portrait_update(state);
	combos_update(state);
	result_transition_update(state);
	round_mode_update(state);

	state.round_frame++;
	state.round_or_result_frame++;
	if((state.round_or_result_frame & 63U) == 0U) {
		if(state.round_speed < 0x7FU) {
			state.round_speed++;
		}
		if((state.round_or_result_frame & 1023U) == 0U) {
			randring_fill(state);
			for(auto &player : state.players) {
				if(player.hit_damage_next < hit_damage_max) {
					player.hit_damage_next++;
				}
			}
		}
		if(state.round_or_result_frame < 2000U) {
			state.players[0].cpu_frame = 0U;
			state.players[1].cpu_frame = 0U;
		}
	}
	if(state.session_exit_requested) {
		state.route = 0U;
		state.round_complete = true;
	}
}

} // namespace th03::detail
