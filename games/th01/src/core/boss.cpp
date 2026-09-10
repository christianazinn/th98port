#include "th01/core/boss.hpp"

#include "th01/core/projectiles.hpp"
#include "th01/core/state.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/random.hpp"

#include <bit>
#include <cstdint>
#include <optional>

namespace th01::core {
namespace {

inline constexpr std::int16_t playfield_right = 640;
inline constexpr std::int16_t playfield_top = 64;
inline constexpr std::int16_t playfield_bottom = 400;
inline constexpr std::int16_t player_top = 368;
inline constexpr std::int16_t player_width = 32;
inline constexpr std::int16_t player_height = 32;
inline constexpr std::int16_t singyoku_width = 96;
inline constexpr std::int16_t singyoku_height = 96;
inline constexpr std::int16_t singyoku_base_left = 272;
inline constexpr std::int16_t singyoku_base_top = 107;

[[nodiscard]] std::int16_t word(int value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] std::int16_t rank_value(
	const State &state,
	std::int16_t easy,
	std::int16_t normal,
	std::int16_t hard,
	std::int16_t lunatic
) noexcept
{
	switch(state.rank) {
	case 0: return easy;
	case 1: return normal;
	case 2: return hard;
	case 3: return lunatic;
	default: return normal;
	}
}

[[nodiscard]] bool bomb_deals_damage(const State &state) noexcept
{
	return state.rem_lives == 0 && state.bomb_damaging &&
		(state.frame_rand % 50U) == 0U;
}

void orb_bounce_from_boss(State &state) noexcept
{
	auto &velocity = state.orb.horizontal.velocity;
	switch(velocity) {
	case OrbVelocityX::left4:
	case OrbVelocityX::left8:
		velocity = OrbVelocityX::right4;
		break;
	case OrbVelocityX::right4:
	case OrbVelocityX::right8:
		velocity = OrbVelocityX::left4;
		break;
	case OrbVelocityX::stationary:
		velocity = state.orb.horizontal.left < 304
			? OrbVelocityX::right4
			: OrbVelocityX::left4;
		break;
	}
}

[[nodiscard]] bool singyoku_orb_hittest(const State &state) noexcept
{
	const auto &singyoku = state.boss.singyoku;
	// CBossEntity::hitbox_orb_set(8, 8, 88, 88) and hittest_orb().
	return state.orb.prev_left >= singyoku.left &&
		state.orb.prev_left <= word(static_cast<int>(singyoku.left) + 64) &&
		state.orb.prev_top >= singyoku.top &&
		state.orb.prev_top <= word(static_cast<int>(singyoku.top) + 64);
}

void sphere_rotate(SingyokuState &singyoku, std::int16_t cel_delta) noexcept;

void singyoku_entrance(State &state) noexcept
{
	auto &boss = state.boss;
	auto &singyoku = boss.singyoku;
	singyoku.left = singyoku_base_left;
	singyoku.top = singyoku_base_top;
	boss.phase_frame = 0;
	std::int16_t rotation_interval = 18;
	while(boss.phase_frame < 200) {
		boss.phase_frame = word(static_cast<int>(boss.phase_frame) + 1);
		if((boss.phase_frame % rotation_interval) == 0) {
			sphere_rotate(singyoku, 1);
			rotation_interval = word(static_cast<int>(rotation_interval) - 2);
			if(rotation_interval <= 0) {
				rotation_interval = 1;
			}
		}
	}
	boss.phase = 1;
	singyoku.pattern_cur = 0;
	singyoku.pattern_value = rank_value(state, 70, 50, 30, 10);
	singyoku.invincible = false;
	boss.phase_frame = 0;
	singyoku.initial_hp_rendered = false;
	singyoku.invincibility_frame = 0;
}

void singyoku_hit_update(State &state) noexcept
{
	const auto &singyoku = state.boss.singyoku;
	static_cast<void>(boss_hit_update(
		state,
		state.boss.singyoku.invincibility_frame,
		state.boss.singyoku.invincible,
		singyoku_orb_hittest(state),
		word(static_cast<int>(singyoku.left) + 16),
		word(static_cast<int>(singyoku.top) + 32),
		96,
		48,
		3000
	));
}

} // namespace

[[nodiscard]] bool boss_hit_update(
	State &state,
	std::int16_t &invincibility_frame,
	bool &is_invincible,
	bool colliding_with_orb,
	std::int16_t shot_hitbox_left,
	std::int16_t shot_hitbox_top,
	std::int16_t shot_hitbox_width_minus_shot,
	std::int16_t shot_hitbox_height_minus_shot,
	std::int16_t hit_score
) noexcept
{
	if(state.test_damage) {
		state.boss.hp = word(static_cast<int>(state.boss.hp) - 1);
		state.test_damage = false;
	}
	static_cast<void>(hit_boss(
		state.shots,
		shot_hitbox_left,
		shot_hitbox_top,
		shot_hitbox_width_minus_shot,
		shot_hitbox_height_minus_shot
	));

	if((colliding_with_orb && !is_invincible) || bomb_deals_damage(state)) {
		invincibility_frame = 0;
		state.boss.hp = word(static_cast<int>(state.boss.hp) - 1);
		is_invincible = true;
		orb_bounce_from_boss(state);
		state.score += static_cast<decltype(state.score)>(hit_score);
		return true;
	}

	if(is_invincible && invincibility_frame < 40) {
		return false;
	}
	if(invincibility_frame > 40) {
		invincibility_frame = 0;
		is_invincible = false;
	}
	return false;
}

namespace {

void sphere_rotate(SingyokuState &singyoku, std::int16_t cel_delta) noexcept
{
	auto image = word(static_cast<int>(singyoku.sphere_image) + cel_delta);
	if(image > 7) {
		image = 0;
	} else if(image < 0) {
		image = 7;
	}
	singyoku.sphere_image = static_cast<std::uint8_t>(image);
}

void sphere_accelerate_rotation(State &state, std::int16_t cel_delta) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame < 50) {
		boss.singyoku.sphere_image = 0;
		return;
	}
	if((boss.phase_frame < 100) && ((boss.phase_frame % 4) == 0)) {
		if((boss.phase_frame == 50) || (boss.phase_frame == 60) ||
			(boss.phase_frame == 68) || (boss.phase_frame == 74) ||
			(boss.phase_frame == 78) || (boss.phase_frame == 82) ||
			(boss.phase_frame > 82)) {
			sphere_rotate(boss.singyoku, cel_delta);
		}
	}
}

void sphere_move_rotate(State &state, std::int16_t delta_x, std::int16_t delta_y) noexcept
{
	auto &boss = state.boss;
	auto &singyoku = boss.singyoku;
	const auto moved_left = word(static_cast<int>(singyoku.left) + delta_x);
	singyoku.left = moved_left < 0 ? 0 :
		(moved_left > (playfield_right - singyoku_width)
			? static_cast<std::int16_t>(playfield_right - singyoku_width)
			: moved_left);
	singyoku.top = word(static_cast<int>(singyoku.top) + delta_y);
	if((boss.phase_frame % 2) == 0) {
		sphere_rotate(singyoku, 1);
	}
}

void pattern_halfcircle_spray_downwards(State &state) noexcept
{
	auto &boss = state.boss;
	auto &singyoku = boss.singyoku;
	if(boss.phase_frame == 10) {
		singyoku.halfcircle_direction = (pc98::irand(state.random_seed) % 2U) == 1U
			? 1
			: -1;
	}
	if(boss.phase_frame < 100) {
		sphere_accelerate_rotation(state, singyoku.halfcircle_direction);
		return;
	}
	if(boss.phase_frame == 100) {
		singyoku.pattern_value = rank_value(state, 10, 15, 20, 30);
		singyoku.halfcircle_angle = singyoku.halfcircle_direction == -1 ? 0U : 128U;
	}
	if(boss.phase_frame < 160) {
		sphere_rotate(singyoku, singyoku.halfcircle_direction);
		const auto interval = static_cast<std::int16_t>(60 / singyoku.pattern_value);
		if((boss.phase_frame % interval) == 0) {
			pellets_add_single(
				state,
				word(static_cast<int>(singyoku.left) + 40),
				word(static_cast<int>(singyoku.top) + 40),
				singyoku.halfcircle_angle,
				50
			);
			const auto delta = static_cast<std::int16_t>(
				(static_cast<std::int16_t>(singyoku.halfcircle_direction) * 128) /
				singyoku.pattern_value
			);
			singyoku.halfcircle_angle = static_cast<std::uint8_t>(
				singyoku.halfcircle_angle - delta
			);
		}
		return;
	}
	boss.phase_frame = 0;
}

[[nodiscard]] bool pattern_slam_into_player_and_back_up(State &state) noexcept
{
	auto &boss = state.boss;
	auto &singyoku = boss.singyoku;
	if(boss.phase_frame < 100) {
		sphere_accelerate_rotation(state, 1);
		return true;
	}
	if(boss.phase_frame == 100) {
		const auto angle = pc98::masterlib::iatan2_source(
			word(player_top - word(static_cast<int>(singyoku.top) + 32)),
			word(static_cast<int>(state.player.left) -
				word(static_cast<int>(singyoku.left) + 32))
		);
		if(!angle) {
			return false;
		}
		const auto velocity = pc98::masterlib::vector2(
			*angle, rank_value(state, 4, 4, 5, 6)
		);
		singyoku.slam_velocity_x = velocity.x;
		singyoku.slam_velocity_y = velocity.y;
	}

	if(singyoku.slam_velocity_x != -1) {
		sphere_move_rotate(
			state, singyoku.slam_velocity_x, singyoku.slam_velocity_y
		);
		if(singyoku.top > (playfield_bottom - singyoku_height)) {
			singyoku.slam_velocity_x = -1;
			singyoku.slam_velocity_y = -4;
		}
	} else if(singyoku.slam_velocity_y == -4) {
		sphere_move_rotate(state, 0, singyoku.slam_velocity_y);
		if(singyoku.top < singyoku_base_top) {
			singyoku.slam_velocity_y = 0;
		}
	} else {
		boss.phase_frame = 0;
	}

	if(!state.player.invincible && singyoku.left <= state.player.left &&
		word(static_cast<int>(singyoku.left) + (singyoku_width - player_width)) >=
			state.player.left &&
		singyoku.top >= (player_top - singyoku_height)) {
		state.input.player_is_hit = true;
	}
	return true;
}

void transform_to_person_and_back_to_sphere(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame < 100) {
		sphere_accelerate_rotation(state, 1);
		return;
	}
	if(boss.phase_frame == 260) {
		boss.singyoku.sphere_image = 0;
		boss.phase_frame = 0;
	}
}

void fire_chasing_pellets(State &state) noexcept
{
	const auto &singyoku = state.boss.singyoku;
	const auto fixed_speed = rank_value(state, 55, 58, 62, 65);
	pellets_add_single(state, word(static_cast<int>(singyoku.left) + 40),
		word(static_cast<int>(singyoku.top) + 40), 240U, 16,
		PelletMotion::chase, fixed_speed);
	pellets_add_single(state, word(static_cast<int>(singyoku.left) + 40),
		word(static_cast<int>(singyoku.top) + 40), 144U, 16,
		PelletMotion::chase, fixed_speed);
}

void fire_crossing_pellets(State &state) noexcept
{
	const auto &singyoku = state.boss.singyoku;
	const auto speed = rank_value(state, 60, 70, 75, 80);
	pellets_add_single(state, word(static_cast<int>(singyoku.left) + 8),
		word(static_cast<int>(singyoku.top) + 40), 48U, speed);
	pellets_add_single(state, word(static_cast<int>(singyoku.left) + 88),
		word(static_cast<int>(singyoku.top) + 40), 80U, speed);
}

void fire_random_downwards_pellets(State &state) noexcept
{
	auto &singyoku = state.boss.singyoku;
	singyoku.pattern_value = rank_value(state, 48, 54, 60, 66);
	for(std::int16_t i = 0; i < 10; ++i) {
		pellets_add_single(state, word(static_cast<int>(singyoku.left) + 40),
			word(static_cast<int>(singyoku.top) + 40),
			static_cast<std::uint8_t>(pc98::irand(state.random_seed) % 128U),
			singyoku.pattern_value);
	}
}

void fire_random_sling_pellets(State &state) noexcept
{
	auto &singyoku = state.boss.singyoku;
	singyoku.pattern_value = rank_value(state, 48, 64, 80, 96);
	for(std::int16_t i = 0; i < 10; ++i) {
		const auto offset_x = static_cast<std::int16_t>(
			pc98::irand(state.random_seed) % 96U
		);
		const auto offset_y = static_cast<std::int16_t>(
			pc98::irand(state.random_seed) % 96U
		);
		pellets_add_single(state,
			word(static_cast<int>(singyoku.left) + offset_x),
			word(static_cast<int>(singyoku.top) + offset_y), 0U, 0,
			PelletMotion::sling_aimed, singyoku.pattern_value);
	}
}

void pattern_chasing_pellets(State &state) noexcept
{
	transform_to_person_and_back_to_sphere(state);
	const auto frame = state.boss.phase_frame;
	if(frame > 140 && (frame % 8) == 0 && frame <= 220) {
		fire_chasing_pellets(state);
	}
}

void pattern_crossing_pellets(State &state) noexcept
{
	transform_to_person_and_back_to_sphere(state);
	const auto frame = state.boss.phase_frame;
	if(frame > 140 && (frame % 8) == 0) {
		fire_crossing_pellets(state);
	}
}

void pattern_random_downwards_pellets(State &state) noexcept
{
	transform_to_person_and_back_to_sphere(state);
	const auto frame = state.boss.phase_frame;
	if(frame == 135 || frame == 160 || frame == 185) {
		fire_random_downwards_pellets(state);
	}
}

void pattern_random_sling_pellets(State &state) noexcept
{
	transform_to_person_and_back_to_sphere(state);
	const auto frame = state.boss.phase_frame;
	if(frame == 135 || frame == 160 || frame == 185) {
		fire_random_sling_pellets(state);
	}
}

} // namespace

void singyoku_setup(State &state) noexcept
{
	auto &boss = state.boss;
	boss.id = BossId::singyoku;
	boss.hp = 8;
	boss.phase = 0;
	boss.phase_frame = 0;
	boss.active = true;
	boss.singyoku.left = playfield_right;
	boss.singyoku.top = playfield_top;
	boss.singyoku.sphere_image = 0;
}

[[nodiscard]] BossUpdateResult singyoku_update(State &state) noexcept
{
	auto &boss = state.boss;
	auto &singyoku = boss.singyoku;
	if(!boss.active) {
		return BossUpdateResult::unsupported;
	}
	if(boss.phase == 0) {
		singyoku_entrance(state);
		return BossUpdateResult::running;
	}
	if(boss.phase == 1) {
		boss.phase_frame = word(static_cast<int>(boss.phase_frame) + 1);
		singyoku.invincibility_frame = word(
			static_cast<int>(singyoku.invincibility_frame) + 1
		);
		if(singyoku.pattern_cur == 0) {
			pattern_halfcircle_spray_downwards(state);
		} else if(singyoku.pattern_cur == 1 &&
			!pattern_slam_into_player_and_back_up(state)) {
			return BossUpdateResult::unsupported;
		}
		if(boss.phase_frame == 0) {
			singyoku.pattern_cur = singyoku.pattern_cur == 1
				? 0
				: word(static_cast<int>(singyoku.pattern_cur) + 1);
		}
		singyoku_hit_update(state);
		if(boss.hp <= 6 && !singyoku.invincible && singyoku.pattern_cur != 1) {
			boss.phase = 2;
			singyoku.pattern_value = 0;
			singyoku.pattern_cur = 0;
			boss.phase_frame = 0;
			singyoku.invincibility_frame = 0;
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 2) {
		boss.phase_frame = word(static_cast<int>(boss.phase_frame) + 1);
		singyoku.invincibility_frame = word(
			static_cast<int>(singyoku.invincibility_frame) + 1
		);
		switch(singyoku.pattern_cur) {
		case 0: pattern_chasing_pellets(state); break;
		case 1: pattern_random_downwards_pellets(state); break;
		case 2: pattern_crossing_pellets(state); break;
		case 3: pattern_random_sling_pellets(state); break;
		case 4:
			if(!pattern_slam_into_player_and_back_up(state)) {
				return BossUpdateResult::unsupported;
			}
			break;
		default: return BossUpdateResult::unsupported;
		}
		if(boss.phase_frame == 0) {
			singyoku.pattern_cur = singyoku.pattern_cur == 4
				? static_cast<std::int16_t>(pc98::irand(state.random_seed) % 4U)
				: 4;
		}
		singyoku_hit_update(state);
		if(boss.hp <= 0) {
			boss.phase = 8;
			boss.phase_frame = 0;
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 8) {
		pellets_unput_and_reset_nonclouds(state);
		return BossUpdateResult::route_selection;
	}
	return BossUpdateResult::unsupported;
}

[[nodiscard]] BossUpdateResult singyoku_route_select(
	State &state, BossRoute route
) noexcept
{
	auto &boss = state.boss;
	if(!boss.active || boss.id != BossId::singyoku || boss.phase != 8) {
		return BossUpdateResult::unsupported;
	}
	state.route = static_cast<std::int8_t>(route);
	state.stage_cleared = true;
	state.input.player_is_hit = true;
	boss.active = false;
	return BossUpdateResult::stage_cleared;
}

void boss_setup(State &state) noexcept
{
	switch(state.boss.id) {
	case BossId::singyoku:
		singyoku_setup(state);
		break;
	case BossId::yuugenmagan:
		yuugenmagan_setup(state);
		break;
	case BossId::mima:
		mima_setup(state);
		break;
	case BossId::kikuri:
		kikuri_setup(state);
		break;
	case BossId::elis:
		elis_setup(state);
		break;
	case BossId::sariel:
		sariel_setup(state);
		break;
	case BossId::konngara:
		konngara_setup(state);
		break;
	default:
		state.boss.active = false;
		break;
	}
}

[[nodiscard]] BossUpdateResult boss_update(State &state) noexcept
{
	switch(state.boss.id) {
	case BossId::singyoku:
		return singyoku_update(state);
	case BossId::yuugenmagan:
		return yuugenmagan_update(state);
	case BossId::mima:
		return mima_update(state);
	case BossId::kikuri:
		return kikuri_update(state);
	case BossId::elis:
		return elis_update(state);
	case BossId::sariel:
		return sariel_update(state);
	case BossId::konngara:
		return konngara_update(state);
	case BossId::none:
		return BossUpdateResult::running;
	default:
		return BossUpdateResult::unsupported;
	}
}

void boss_render_tail(State &state) noexcept
{
	if(state.boss.id == BossId::konngara) {
		konngara_render_tail(state);
	}
	if(state.boss.id == BossId::yuugenmagan) {
		yuugenmagan_render_tail(state);
	}
	if(state.boss.id == BossId::mima) {
		mima_render_tail(state);
	}
	if(state.boss.id == BossId::kikuri) {
		kikuri_render_tail(state);
	}
	if(state.boss.id == BossId::elis) {
		elis_render_tail(state);
	}
	if(state.boss.id == BossId::sariel) {
		sariel_render_tail(state);
	}
	// b05.cpp keeps its represented gameplay mutations in singyoku_update().
}

} // namespace th01::core
