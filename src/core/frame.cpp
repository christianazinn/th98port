#include "game_internal.hpp"
#include "trig.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
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
constexpr std::uint16_t anniversary_sine_table_dgroup_offset = 0x0316U;
constexpr std::uint16_t anniversary_cosine_table_dgroup_offset = 0x0396U;
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

// These implementation partitions deliberately remain in one translation
// unit. The source-complete Classic core has many order-sensitive private
// helpers; a separately compiled module boundary would be an architectural
// change rather than the mechanical source split performed here.
#include "frame/bullets_core.inl"
#include "frame/characters/kana_extra_bullet.inl"
#include "frame/bullets_transfer.inl"
#include "frame/characters/extra_spawn.inl"
#include "frame/bullets_world.inl"
#include "frame/shots.inl"
#include "frame/characters/charge_update.inl"
#include "frame/characters/charge_collision.inl"
#include "frame/player_movement.inl"
#include "frame/characters/charge_add.inl"
#include "frame/player_actions.inl"
#include "frame/characters/bombs.inl"
#include "frame/characters/gauge.inl"
#include "frame/player_collision.inl"
#include "frame/player_cpu.inl"
#include "frame/enemies_motion.inl"
#include "frame/extra_common.inl"
#include "frame/characters/extra_update.inl"
#include "frame/characters/extra_dispatch.inl"
#include "frame/boss_common.inl"
#include "frame/characters/bosses_reimu_mima.inl"
#include "frame/characters/bosses_marisa_ellen.inl"
#include "frame/characters/bosses_kotohime_kana.inl"
#include "frame/characters/bosses_rikako_chiyuri.inl"
#include "frame/characters/bosses_yumemi.inl"
#include "frame/characters/boss_dispatch.inl"
#include "frame/world_collision.inl"
#include "frame/round.inl"
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
