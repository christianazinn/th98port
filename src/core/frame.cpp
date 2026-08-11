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

MainPhaseContext main_phase_context(
	const DeterministicState &state
) noexcept
{
	return MainPhaseContext{
		.round_id = state.round_id,
		.round_or_result_frame = state.round_or_result_frame,
		.playfield_fg_shift_x = state.playfield_fg_shift_x,
	};
}

constexpr GaugeBossAttackKind gauge_boss_attack_kind(
	GaugeBossAttackFlag flag
) noexcept
{
	switch(flag) {
	case GaugeBossAttackFlag::gauge_pellet_init:
	case GaugeBossAttackFlag::gauge_pellet_active:
		return GaugeBossAttackKind::gauge_pellet;
	case GaugeBossAttackFlag::gauge_bullet_init:
	case GaugeBossAttackFlag::gauge_bullet_active:
		return GaugeBossAttackKind::gauge_bullet;
	case GaugeBossAttackFlag::boss:
		return GaugeBossAttackKind::boss;
	case GaugeBossAttackFlag::none:
	default:
		return GaugeBossAttackKind::none;
	}
}

void gauge_attack_finish(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	state.gba_flag_active[player_id] = GaugeBossAttackFlag::none;
	if(observer != nullptr) {
		observer->gauge_attack_finished(GaugeAttackFinished{
			.attacker = static_cast<PlayerId>(player_id),
			.target = static_cast<PlayerId>(1U - player_id),
		});
	}
}

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

void observe_common_effect(
	const DeterministicState &state,
	GameplayObserver *observer,
	CommonEffectKind kind,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id,
	std::uint8_t initial_angle = 0U
) noexcept
{
	if(observer == nullptr) {
		return;
	}
	observer->common_effect_spawned(CommonEffectSpawned{
		.kind = kind,
		.player = static_cast<PlayerId>(player_id),
		.center_x = center_x,
		.center_y = center_y,
		.fg_shift_x_at_spawn = state.playfield_fg_shift_x[player_id],
		.initial_angle = initial_angle,
	});
}

void observe_main_feedback(
	GameplayObserver *observer,
	MainFeedbackCueSite site,
	std::optional<std::uint8_t> player_id = std::nullopt
) noexcept
{
	if(observer == nullptr) {
		return;
	}
	observer->main_feedback_cue_requested(MainFeedbackCueRequested{
		.site = site,
		.player = player_id.transform([](std::uint8_t player) {
			return static_cast<PlayerId>(player);
		}),
	});
}

void observe_common_square(
	DeterministicState &state,
	GameplayObserver *observer,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id
) noexcept
{
	const auto random_word = randring_next16(state);
	observe_common_effect(
		state, observer, CommonEffectKind::contracting_square,
		center_x, center_y, player_id,
		static_cast<std::uint8_t>(random_word)
	);
}

void observe_hit_circle_exact(
	const DeterministicState &state,
	GameplayObserver *observer,
	HitCircleKind kind,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id
) noexcept
{
	if(observer == nullptr) {
		return;
	}
	observer->hit_circle_spawned(HitCircleSpawned{
		.kind = kind,
		.player = static_cast<PlayerId>(player_id),
		.center_x = center_x,
		.center_y = center_y,
		.fg_shift_x_at_spawn = state.playfield_fg_shift_x[player_id],
	});
}

void observe_hit_circle_random(
	DeterministicState &state,
	GameplayObserver *observer,
	std::int16_t left,
	std::int16_t right,
	std::int16_t top,
	std::int16_t bottom,
	std::uint8_t player_id
) noexcept
{
	const auto random_x = randring_next16(state);
	const auto random_y = randring_next16(state);
	if(observer == nullptr) {
		return;
	}
	const auto width = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(right) - static_cast<std::uint16_t>(left)
	);
	const auto height = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(bottom) - static_cast<std::uint16_t>(top)
	);
	const auto center_x = exact::wrap_add(
		left, static_cast<std::int16_t>(random_x % width)
	);
	const auto center_y = exact::wrap_add(
		top, static_cast<std::int16_t>(random_y % height)
	);
	observe_hit_circle_exact(
		state, observer, HitCircleKind::enemy,
		center_x, center_y, player_id
	);
}

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

namespace {

template <typename Callback>
void observe_bullet16_spawns(
	DeterministicState &state,
	GameplayObserver &observer,
	std::uint8_t source_player,
	Bullet16ConstructionKind construction,
	bool source_template_animation_enabled,
	Callback &&callback
) noexcept
{
	std::array<bool, bullet_count> was_free{};
	for(std::size_t slot = 0U; slot < state.bullets.size(); ++slot) {
		was_free[slot] = (state.bullets[slot].flag == BulletFlag::free);
	}
	callback();

	// Each currently wrapped source producer callback allocates the first free
	// slot and never frees or retags a newly allocated bullet before returning.
	// Ascending newly-live slots are therefore the exact allocation order.
	for(std::size_t slot = 0U; slot < state.bullets.size(); ++slot) {
		if(
			was_free[slot] &&
			state.bullets[slot].flag == BulletFlag::bullet16
		) {
			observer.bullet16_spawned(Bullet16Spawned{
				.slot = static_cast<BulletSlot>(slot),
				.source_player = static_cast<PlayerId>(source_player),
				.construction = construction,
				.source_template_animation_enabled =
					source_template_animation_enabled,
			});
		}
	}
}

void extra_attacks_update_observed(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	if(observer == nullptr) {
		extra_attacks_update(state, player_id, nullptr);
		return;
	}
	const auto character = character_index_from_paletted(
		state.players[player_id].paletted_character
	);
	observe_bullet16_spawns(
		state,
		*observer,
		player_id,
		(character == character_kana)
			? Bullet16ConstructionKind::kana_extra_attack
			: Bullet16ConstructionKind::default_attack,
		true,
		[&]() noexcept { extra_attacks_update(state, player_id, observer); }
	);
}

void bosses_update_observed(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	if(observer == nullptr) {
		bosses_update(state, player_id, nullptr);
		return;
	}
	const auto character = character_index_from_paletted(
		state.players[player_id].paletted_character
	);
	const auto is_static = (
		character == character_mima || character == character_chiyuri
	);
	observe_bullet16_spawns(
		state,
		*observer,
		player_id,
		Bullet16ConstructionKind::default_attack,
		!is_static,
		[&]() noexcept { bosses_update(state, player_id, observer); }
	);
}

void gauge_patterns_update_observed(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	if(observer == nullptr) {
		gauge_patterns_update(state, player_id, nullptr);
		return;
	}
	const auto character = character_index_from_paletted(
		state.players[player_id].paletted_character
	);
	const auto is_static = (
		character == character_mima || character == character_chiyuri
	);
	observe_bullet16_spawns(
		state,
		*observer,
		player_id,
		Bullet16ConstructionKind::default_attack,
		!is_static,
		[&]() noexcept { gauge_patterns_update(state, player_id, observer); }
	);
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
	const GameContent &content,
	GameplayObserver *observer
) noexcept
{
	state.collision_map = {};
	state.bullet_template_angle = 0U;
	extra_attacks_update_observed(state, 0U, observer);
	bosses_update_observed(state, 0U, observer);
	bomb_character_update(state, 0U, observer);
	extra_attacks_update_observed(state, 1U, observer);
	bosses_update_observed(state, 1U, observer);
	bomb_character_update(state, 1U, observer);
	if(observer != nullptr) {
		observer->hit_circles_advanced();
	}
	shots_update(state);
	chargeshots_update(state, observer);
	hud_start_update_prefix(state, observer);
	if(state.hud_start_flag == 0U) {
		if(content.enemy_formations.empty()) {
			return false;
		}
		if(!add_formation(state, content, 0U) || !add_formation(state, content, 1U)) {
			return false;
		}
	}
	bullets_update(state);
	if(!enemies_update(state, content, observer)) {
		return false;
	}
	fireballs_update(state);
	gauge_patterns_update_observed(state, 0U, observer);
	gauge_patterns_update_observed(state, 1U, observer);
	if(observer != nullptr) {
		observer->common_effects_advanced();
	}
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

bool warning_interstitial_begin(
	DeterministicState &state,
	GameplayObserver *observer
) noexcept
{
	if(
		state.warning_flag[0] != WarningFlag::flash_white &&
		state.warning_flag[1] != WarningFlag::flash_white
	) {
		return false;
	}
	if(observer != nullptr) {
		observer->warning_interstitial_started(WarningInterstitialStarted{
			.flashing_players = {
				state.warning_flag[0] == WarningFlag::flash_white,
				state.warning_flag[1] == WarningFlag::flash_white,
			},
			.next_attacks = {
				gauge_boss_attack_kind(state.gba_flag_next[0]),
				gauge_boss_attack_kind(state.gba_flag_next[1]),
			},
			.gauge_levels = state.gba_gauge_level,
			.boss_level = state.gba_boss_level,
		});
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

void warning_portrait_update(
	DeterministicState &state,
	GameplayObserver *observer
) noexcept
{
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		auto &flag = state.warning_flag[player_id];
		if(flag == WarningFlag::portrait) {
			if(observer != nullptr) {
				observer->warning_portrait_transition(
					WarningPortraitTransition{
						.player = static_cast<PlayerId>(player_id),
					}
				);
			}
			flag = WarningFlag::flash_white;
		}
	}
}

bool gameplay_frame_suffix_through_feedback_boundary(
	DeterministicState &state,
	GameplayObserver *observer
) noexcept
{
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		if(state.players[player_id].is_cpu != 0U) {
			player_update_cpu(state, player_id, observer);
		} else {
			player_update_human(
				state, player_id, state.input_player[player_id], observer
			);
		}
	}
	if(observer != nullptr) {
		observer->main_feedback_dispatch_boundary_reached();
	}
	if(
		(state.input_system & input_cancel) != 0U &&
		state.players[0].is_cpu != 0U && state.players[1].is_cpu != 0U
	) {
		state.session_exit_requested = true;
		return false;
	}
	return (state.input_system & input_cancel) != 0U;
}

bool gameplay_frame_suffix_before_interstitial(
	DeterministicState &state,
	GameplayObserver *observer
) noexcept
{
	(void)gameplay_frame_suffix_through_feedback_boundary(state, observer);
	return warning_interstitial_begin(state, observer);
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

void gameplay_frame_suffix_after_interstitial(
	DeterministicState &state,
	GameplayObserver *observer
) noexcept
{
	const auto warning_before = state.warning_flag;
	warning_frame_update(state, 0U);
	warning_frame_update(state, 1U);
	if(observer != nullptr) {
		observer->warning_frame_lifecycle(WarningFrameLifecycle{
			.players = {
				WarningPlayerLifecycle{
					.warning_step_before = static_cast<std::uint8_t>(
						warning_before[0]
					),
					.warning_step_after = static_cast<std::uint8_t>(
						state.warning_flag[0]
					),
					.next_attack = gauge_boss_attack_kind(
						state.gba_flag_next[0]
					),
					.active_attack = gauge_boss_attack_kind(
						state.gba_flag_active[0]
					),
				},
				WarningPlayerLifecycle{
					.warning_step_before = static_cast<std::uint8_t>(
						warning_before[1]
					),
					.warning_step_after = static_cast<std::uint8_t>(
						state.warning_flag[1]
					),
					.next_attack = gauge_boss_attack_kind(
						state.gba_flag_next[1]
					),
					.active_attack = gauge_boss_attack_kind(
						state.gba_flag_active[1]
					),
				},
			},
			.gauge_levels = state.gba_gauge_level,
			.boss_level = state.gba_boss_level,
		});
	}
	background_update(state, observer);
	bombs_render_phase_update(state, observer);
	if(observer != nullptr) {
		const auto view = ObserverAccess::main_boss(state);
		observer->observe(view);
	}
	boss_render_phase_update(state, observer);
	if(observer != nullptr) {
		const auto view = ObserverAccess::shots_enemies(state);
		observer->observe(view);
	}
	fireballs_hittest(state, observer);
	if(observer != nullptr) {
		const auto view = ObserverAccess::fireballs(state);
		observer->observe(view);
	}
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		const auto player = static_cast<PlayerId>(player_id);
		if(observer != nullptr) {
			const auto view = ObserverAccess::main_extra_attack(state, player);
			observer->observe(view);
		}
		extra_attacks_render_phase_update_player(state, player_id);
		if(observer != nullptr) {
			const auto view = ObserverAccess::main_charge_shot(state, player);
			observer->observe(view);
		}
	}
	if(observer != nullptr) {
		const auto view = ObserverAccess::players_bullets(state);
		observer->observe(view);
	}
	if(observer != nullptr) {
		std::optional<PlayerId> defeated_player;
		std::optional<PlayerId> winner;
		MainResultWinnerStats winner_stats{};
		if(
			state.defeat_flag == DefeatFlag::banner &&
			state.last_fatal_player < player_count
		) {
			defeated_player = static_cast<PlayerId>(state.last_fatal_player);
			const auto winner_id = static_cast<std::uint8_t>(
				1U - state.last_fatal_player
			);
			winner = static_cast<PlayerId>(winner_id);
			const auto &player = state.players[winner_id];
			winner_stats = MainResultWinnerStats{
				.combo_hits_max = player.combo_hits_max,
				.gauge_attacks_fired = player.gauge_attacks_fired,
				.boss_attacks_fired = player.boss_attacks_fired,
				.boss_attacks_reversed = player.boss_attacks_reversed,
				.boss_panics_fired = player.boss_panics_fired,
			};
		}
		const auto stock_mode =
			state.game_mode == game_mode_story || state.practice_uses_stock;
		const auto match_complete =
			state.players[0].rounds_won >= 2U ||
			state.players[1].rounds_won >= 2U;
		const auto summary_active =
			state.defeat_flag == DefeatFlag::banner &&
			(stock_mode ? (state.winner == 0) : match_complete);
		observer->main_result_state(MainResultState{
			.context = main_phase_context(state),
			.defeated_player = defeated_player,
			.winner = winner,
			.summary_active = summary_active,
			.story_stage = state.story_stage,
			.story_lives = state.story_lives,
			.rounds_won = {
				state.players[0].rounds_won,
				state.players[1].rounds_won,
			},
			.winner_stats = winner_stats,
		});
	}
	if(result_control_update(state)) {
		return;
	}
	if(observer != nullptr) {
		observer->main_gauge_state(MainGaugeState{
			.context = main_phase_context(state),
			.players = {
				MainGaugePlayerState{
					.gauge_charged = state.players[0].gauge_charged,
					.gauge_avail = state.players[0].gauge_avail,
					.hyper_active = state.players[0].hyper_active,
				},
				MainGaugePlayerState{
					.gauge_charged = state.players[1].gauge_charged,
					.gauge_avail = state.players[1].gauge_avail,
					.hyper_active = state.players[1].hyper_active,
				},
			},
		});
	}
	if(observer != nullptr) {
		observer->main_score_state(MainScoreState{
			.context = main_phase_context(state),
			.digits = state.score,
		});
	}
	story_extend_update(state, observer);
	warning_portrait_update(state, observer);
	if(observer != nullptr) {
		observer->main_hud_start_state(MainHudStartState{
			.round_id = state.round_id,
			.story_mode = state.game_mode == game_mode_story,
			.story_stage = state.story_stage,
			.state_after_prefix = state.hud_start_state,
			.frame_after_prefix = state.hud_start_frame,
		});
		observer->main_combo_state(MainComboState{
			.context = main_phase_context(state),
			.players = {
				MainComboPlayerState{
					.time = state.combos[0].time,
					.hits_highest = state.combos[0].hits_highest,
					.bonus_total = state.combos[0].bonus_total,
				},
				MainComboPlayerState{
					.time = state.combos[1].time,
					.hits_highest = state.combos[1].hits_highest,
					.bonus_total = state.combos[1].bonus_total,
				},
			},
		});
	}
	combos_update(state);
	if(observer != nullptr) {
		observer->main_result_transition_state(MainResultTransitionState{
			.context = main_phase_context(state),
			.phase = static_cast<MainResultTransitionPhase>(state.result_phase),
			.frame_before = state.result_phase_frame,
		});
	}
	result_transition_update(state);
	round_mode_update(state);
	if(observer != nullptr) {
		observer->main_frame_footer_reached(MainFrameFooterReached{});
	}

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
