#include "th01/core/boss_konngara.hpp"

#include "th01/core/boss.hpp"
#include "th01/core/projectiles.hpp"
#include "th01/core/state.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/random.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace th01::core {
namespace {

constexpr std::int16_t playfield_left = 0;
constexpr std::int16_t playfield_top = 64;
constexpr std::int16_t playfield_right = 640;
constexpr std::int16_t playfield_bottom = 400;
constexpr std::int16_t player_top = 368;
constexpr std::int16_t player_width = 32;
constexpr std::int16_t player_height = 32;
constexpr std::int16_t pellet_width = 8;
constexpr std::int16_t pellet_height = 8;
constexpr std::int16_t diamond_width = 8;
constexpr std::int16_t diamond_height = 8;
constexpr std::int16_t cup_left = 296;
constexpr std::int16_t cup_top = 188;
constexpr std::int16_t cup_right = 328;
constexpr std::int16_t cup_center_left = 312;
constexpr std::int16_t left_sleeve_left = 290;
constexpr std::int16_t left_sleeve_top = 200;
constexpr std::int16_t eye_center_left = 316;
constexpr std::int16_t eye_bottom = 140;
constexpr std::int16_t sword_center_left = 410;
constexpr std::int16_t sword_center_top = 70;
constexpr std::int16_t slash_four_left = 432;
constexpr std::int16_t slash_four_top = 232;
constexpr std::int16_t slash_five_left = 198;
constexpr std::int16_t slash_five_top = 198;
constexpr std::int16_t choose_new = 99;

[[nodiscard]] std::int16_t word(std::int32_t value) noexcept
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

[[nodiscard]] std::int16_t random_x(State &state) noexcept
{
	return static_cast<std::int16_t>(pc98::irand(state.random_seed) % 640U);
}

[[nodiscard]] std::int16_t random_x_range(
	State &state, std::int16_t minimum, std::int16_t span
) noexcept
{
	return word(static_cast<std::int32_t>(minimum) +
		static_cast<std::int16_t>(pc98::irand(state.random_seed) %
			static_cast<std::uint16_t>(span)));
}

[[nodiscard]] std::int16_t random_y_range(
	State &state, std::int16_t minimum, std::int16_t span
) noexcept
{
	return word(static_cast<std::int32_t>(playfield_top + minimum) +
		static_cast<std::int16_t>(pc98::irand(state.random_seed) %
			static_cast<std::uint16_t>(span)));
}

[[nodiscard]] std::uint8_t angle_between(
	std::int16_t from_left,
	std::int16_t from_top,
	std::int16_t to_left,
	std::int16_t to_top
) noexcept
{
	return pc98::masterlib::iatan2_source(
		word(static_cast<std::int32_t>(to_top) - from_top),
		word(static_cast<std::int32_t>(to_left) - from_left)
	).value_or(0U);
}

void pellets_add_rain(
	State &state,
	std::int16_t left,
	std::int16_t top,
	std::uint8_t angle,
	std::int16_t speed_base_q12_4
) noexcept
{
	pellets_add_single(
		state, left, top, angle, speed_base_q12_4, PelletMotion::gravity, 1
	);
}

[[nodiscard]] bool konngara_orb_hittest(const State &state) noexcept
{
	return state.orb.horizontal.left >= 288 && state.orb.horizontal.left <= 352 &&
		state.orb.top >= 120 && state.orb.top <= 160;
}

void face_direction_set(KonngaraState &konngara, KonngaraFaceDirection next) noexcept
{
	if(konngara.face_direction_can_change && konngara.face_direction != next) {
		konngara.face_direction = next;
	}
}

void face_expression_set(KonngaraState &konngara, KonngaraFaceExpression next) noexcept
{
	konngara.face_expression = next;
}

void phase_next(State &state, std::int8_t next) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	boss.phase = next;
	boss.phase_frame = 0;
	konngara.hit_invincibility_frame = 0;
	konngara.pattern_current = choose_new;
	konngara.patterns_done = 0;
	if((next & 1) != 0) {
		face_expression_set(konngara, KonngaraFaceExpression::neutral);
	}
}

void frame_common(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	boss.phase_frame = word(static_cast<std::int32_t>(boss.phase_frame) + 1);
	konngara.hit_invincibility_frame = word(
		static_cast<std::int32_t>(konngara.hit_invincibility_frame) + 1
	);
	const auto face = state.player.left < 198 ? KonngaraFaceDirection::left :
		(state.player.left > 396 ? KonngaraFaceDirection::right :
			KonngaraFaceDirection::center);
	face_direction_set(konngara, face);
}

void hit_update(State &state) noexcept
{
	auto &konngara = state.boss.konngara;
	static_cast<void>(boss_hit_update(
		state,
		konngara.hit_invincibility_frame,
		konngara.hit_invincible,
		konngara_orb_hittest(state),
		288,
		120,
		96,
		40,
		10000
	));
}

void choose_pattern(State &state, std::int16_t frame_min, std::uint16_t first,
	std::uint16_t second) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	if(boss.phase_frame <= frame_min) {
		return;
	}
	boss.phase_frame = 1;
	konngara.pattern_current = static_cast<std::int16_t>(
		pc98::irand(state.random_seed) % first
	);
	if(konngara.pattern_current == konngara.pattern_previous) {
		konngara.pattern_current = static_cast<std::int16_t>(
			pc98::irand(state.random_seed) % second
		);
	}
	konngara.pattern_previous = konngara.pattern_current;
	konngara.patterns_done = word(static_cast<std::int32_t>(konngara.patterns_done) + 1);
}

template <std::size_t Count>
void snakes_spawn(
	State &state,
	KonngaraSnakesState<Count> &snakes,
	std::size_t snake,
	std::int16_t left,
	std::int16_t top
) noexcept
{
	for(std::size_t i = 0U; i < 5U; ++i) {
		snakes.left[snake][i] = left;
		snakes.top[snake][i] = top;
	}
	const auto angle = static_cast<std::uint8_t>(angle_between(
		left,
		top,
		word(static_cast<std::int32_t>(state.player.left) + 12),
		word(static_cast<std::int32_t>(player_top) + 16)
	) + ((pc98::irand(state.random_seed) % 2U) == 0U ? 0x28U : 0xD8U));
	const auto velocity = pc98::masterlib::vector2(angle, 6);
	snakes.velocity_x[snake] = velocity.x;
	snakes.velocity_y[snake] = velocity.y;
}

template <std::size_t Count>
void snakes_update(State &state, KonngaraSnakesState<Count> &snakes) noexcept
{
	for(std::size_t snake = 0U; snake < Count; ++snake) {
		if(snakes.left[snake][0] == 999) {
			continue;
		}
		if(!snakes.target_locked[snake]) {
			snakes.target_left[snake] = state.player.left;
		}
		const auto angle = static_cast<std::uint8_t>(angle_between(
			snakes.left[snake][0],
			snakes.top[snake][0],
			word(static_cast<std::int32_t>(snakes.target_left[snake]) + 12),
			word(static_cast<std::int32_t>(player_top) + 16)
		) + ((pc98::irand(state.random_seed) % 2U) == 0U ? 0x28U : 0xD8U));
		const auto velocity = pc98::masterlib::vector2(angle, 7);
		snakes.velocity_x[snake] = velocity.x;
		snakes.velocity_y[snake] = velocity.y;
		if(snakes.top[snake][0] > 300) {
			snakes.target_locked[snake] = true;
		}
		for(std::size_t trail = 4U; trail > 0U; --trail) {
			snakes.left[snake][trail] = snakes.left[snake][trail - 1U];
			snakes.top[snake][trail] = snakes.top[snake][trail - 1U];
		}
		snakes.left[snake][0] = word(
			static_cast<std::int32_t>(snakes.left[snake][0]) + snakes.velocity_x[snake]
		);
		snakes.top[snake][0] = word(
			static_cast<std::int32_t>(snakes.top[snake][0]) + snakes.velocity_y[snake]
		);
		if(!state.player.invincible &&
			snakes.left[snake][0] > state.player.left &&
			snakes.left[snake][0] < word(static_cast<std::int32_t>(state.player.left) + 24) &&
			snakes.top[snake][0] > word(static_cast<std::int32_t>(player_top) +
				(state.player.sliding ? 10 : 0)) &&
			snakes.top[snake][0] < word(static_cast<std::int32_t>(player_top) + 24)) {
			state.input.player_is_hit = true;
		}
	}
}

void pattern_diamonds(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	if(boss.phase_frame == 10) {
		face_expression_set(konngara, KonngaraFaceExpression::neutral);
	}
	if(boss.phase_frame < 100) {
		return;
	}
	if(boss.phase_frame == 100) {
		const auto group = state.rank == 0 ? PelletGroup::two_spread_narrow_aimed :
			(state.rank == 1 ? PelletGroup::three_spread_narrow_aimed :
				(state.rank == 2 ? PelletGroup::five_spread_wide_aimed :
					PelletGroup::five_spread_narrow_aimed));
		const auto first = pc98::masterlib::vector2(
			angle_between(316, 236, 0, word(static_cast<std::int32_t>(player_top) + 16)), 7
		);
		const auto second = pc98::masterlib::vector2(angle_between(316, 236, 0, 64), 7);
		konngara.diamond_bottomleft_velocity_x = first.x;
		konngara.diamond_bottomleft_velocity_y = first.y;
		konngara.diamond_topleft_velocity_x = second.x;
		konngara.diamond_topleft_velocity_y = second.y;
		konngara.diamond_left.fill(316);
		konngara.diamond_top.fill(236);
		pellets_add_group(state, 312, 188, group, 48);
		konngara.pattern_state = rank_value(state, 18, 16, 14, 12);
		konngara.diamonds_at_edges_frame = 0;
		return;
	}
	if(konngara.diamond_left[0] > 0) {
		konngara.diamond_left[0] = word(static_cast<std::int32_t>(konngara.diamond_left[0]) + konngara.diamond_bottomleft_velocity_x);
		konngara.diamond_top[0] = word(static_cast<std::int32_t>(konngara.diamond_top[0]) + konngara.diamond_bottomleft_velocity_y);
		konngara.diamond_left[1] = word(static_cast<std::int32_t>(konngara.diamond_left[1]) - konngara.diamond_bottomleft_velocity_x);
		konngara.diamond_top[1] = word(static_cast<std::int32_t>(konngara.diamond_top[1]) + konngara.diamond_bottomleft_velocity_y);
		konngara.diamond_left[2] = word(static_cast<std::int32_t>(konngara.diamond_left[2]) + konngara.diamond_topleft_velocity_x);
		konngara.diamond_top[2] = word(static_cast<std::int32_t>(konngara.diamond_top[2]) + konngara.diamond_topleft_velocity_y);
		konngara.diamond_left[3] = word(static_cast<std::int32_t>(konngara.diamond_left[3]) - konngara.diamond_topleft_velocity_x);
		konngara.diamond_top[3] = word(static_cast<std::int32_t>(konngara.diamond_top[3]) + konngara.diamond_topleft_velocity_y);
		if(konngara.diamond_left[0] <= 0) {
			konngara.diamond_left[0] = 0;
			konngara.diamond_left[2] = 0;
			konngara.diamond_left[1] = 632;
			konngara.diamond_left[3] = 632;
		}
		return;
	}
	if(konngara.diamond_top[0] > 64) {
		konngara.diamond_top[0] = word(static_cast<std::int32_t>(konngara.diamond_top[0]) - 3);
		konngara.diamond_top[1] = word(static_cast<std::int32_t>(konngara.diamond_top[1]) - 3);
		konngara.diamond_left[2] = word(static_cast<std::int32_t>(konngara.diamond_left[2]) + 6);
		konngara.diamond_left[3] = word(static_cast<std::int32_t>(konngara.diamond_left[3]) - 6);
		if(konngara.diamond_top[0] <= 64) {
			konngara.diamond_top[0] = 64;
		}
		return;
	}
	if(konngara.diamonds_at_edges_frame < 200) {
		konngara.diamonds_at_edges_frame = word(
			static_cast<std::int32_t>(konngara.diamonds_at_edges_frame) + 1
		);
		if((konngara.diamonds_at_edges_frame % konngara.pattern_state) != 0) {
			return;
		}
		const auto fire = [&state](std::int16_t from_left, std::int16_t from_top,
			std::int16_t to_left) {
			pellets_add_single(state, from_left, from_top,
				angle_between(from_left, from_top, to_left, playfield_bottom), 40);
		};
		// Source draws these in separate statements; argument evaluation order
		// must not reverse the shared RNG stream on another compiler.
		auto from = random_y_range(state, 0, 200);
		auto target = random_x_range(state, 240, 400);
		fire(0, from, target);
		from = random_y_range(state, 0, 200);
		target = random_x_range(state, 0, 400);
		fire(632, from, target);
		from = random_x(state);
		target = random_x(state);
		fire(from, 64, target);
		from = random_x(state);
		target = random_x(state);
		fire(from, 64, target);
		pellets_add_group(state, random_x(state), 64, PelletGroup::one_aimed, 40);
		return;
	}
	boss.phase_frame = 0;
	konngara.diamonds_at_edges_frame = 0;
}

void pattern_cup(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	if(boss.phase_frame == 10) {
		face_expression_set(konngara, KonngaraFaceExpression::closed);
	}
	if(boss.phase_frame < 100) {
		return;
	}
	if(boss.phase_frame == 100) {
		konngara.cup_angle = 0x40U;
		konngara.pattern_state = rank_value(state, 80, 80, 96, 112);
	}
	const auto fire = [&state, &konngara](std::uint8_t angle) {
		pellets_add_single(state, cup_right, cup_top, angle, konngara.pattern_state);
		pellets_add_single(state, cup_left, cup_top, angle, konngara.pattern_state);
	};
	if(boss.phase_frame < 140 && (boss.phase_frame % 8) == 0) {
		fire(0x40U);
		return;
	}
	if(boss.phase_frame < 220 && (boss.phase_frame % 8) == 0) {
		konngara.cup_angle = static_cast<std::uint8_t>(konngara.cup_angle + 0x05U);
		fire(konngara.cup_angle);
		fire(static_cast<std::uint8_t>(0x80U - konngara.cup_angle));
		return;
	}
	if(boss.phase_frame < 300 && (boss.phase_frame % 8) == 0) {
		konngara.cup_angle = static_cast<std::uint8_t>(konngara.cup_angle - 0x0CU);
		fire(konngara.cup_angle);
		fire(static_cast<std::uint8_t>(0x80U - konngara.cup_angle));
	}
	if(boss.phase_frame >= 300) {
		boss.phase_frame = 0;
	}
}

void pattern_two_snakes(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	if(boss.phase_frame == 10) {
		face_expression_set(konngara, KonngaraFaceExpression::glare);
	}
	if(boss.phase_frame < 100) {
		return;
	}
	if(boss.phase_frame == 100) {
		konngara.snakes_two.target_locked.fill(false);
		snakes_spawn(state, konngara.snakes_two, 0U, cup_center_left, cup_top);
		konngara.snakes_two.left[1][0] = 999;
		return;
	}
	if(boss.phase_frame >= 500) {
		boss.phase_frame = 0;
		return;
	}
	snakes_update(state, konngara.snakes_two);
	if(boss.phase_frame == 150) {
		snakes_spawn(state, konngara.snakes_two, 1U, cup_center_left, cup_top);
	}
	if(boss.phase_frame > (240 - state.rank * 40) && (boss.phase_frame % 40) == 0) {
		auto angle = static_cast<std::uint8_t>(pc98::irand(state.random_seed) % 12U);
		const auto origin_left = (boss.phase_frame % 120) == 0 ? sword_center_left :
			((boss.phase_frame % 120) == 40 ? eye_center_left : cup_center_left);
		const auto origin_top = (boss.phase_frame % 120) == 0 ? sword_center_top :
			((boss.phase_frame % 120) == 40 ? eye_bottom : cup_top);
		for(std::int16_t i = 0; i < 10; ++i) {
			const auto speed = static_cast<std::int16_t>(40 + ((i % 4) == 1 ? 16 :
				((i % 4) == 3 ? 32 : 0)));
			pellets_add_single(state, origin_left, origin_top,
				static_cast<std::uint8_t>(0x80U - angle), speed);
			pellets_add_single(state, origin_left, origin_top, angle, speed);
			angle = static_cast<std::uint8_t>(angle + 12U);
		}
	}
}

void pattern_rows(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	if(boss.phase_frame == 10) {
		face_expression_set(konngara, KonngaraFaceExpression::neutral);
	}
	if(boss.phase_frame < 100) {
		return;
	}
	if(boss.phase_frame == 100) {
		const auto velocity = pc98::masterlib::vector2(angle_between(
			left_sleeve_left, left_sleeve_top, 64, 64), 16);
		konngara.row_velocity_x = velocity.x;
		konngara.row_velocity_y = velocity.y;
		konngara.row_diamond_left = left_sleeve_left;
		konngara.row_diamond_top = left_sleeve_top;
		konngara.row_pellet_speed = 48;
		konngara.row_direction = 99;
		konngara.pattern_state = rank_value(state, 12, 10, 8, 6);
		return;
	}
	if(konngara.row_direction == 99) {
		konngara.row_diamond_left = word(static_cast<std::int32_t>(konngara.row_diamond_left) + konngara.row_velocity_x);
		konngara.row_diamond_top = word(static_cast<std::int32_t>(konngara.row_diamond_top) + konngara.row_velocity_y);
		if(konngara.row_diamond_top <= 64) {
			konngara.row_diamond_left = 64;
			konngara.row_diamond_top = 64;
			konngara.row_direction = 0;
		}
		return;
	}
	konngara.row_diamond_left = word(static_cast<std::int32_t>(konngara.row_diamond_left) +
		(konngara.row_direction == 0 ? 8 : (konngara.row_direction == 1 ? -8 : 0)));
	if(konngara.row_direction >= 2) {
		konngara.row_diamond_top = word(static_cast<std::int32_t>(konngara.row_diamond_top) + 8);
		konngara.row_direction = word(static_cast<std::int32_t>(konngara.row_direction) + 1);
		if(konngara.row_direction >= 10) {
			konngara.row_direction = konngara.row_diamond_left > 320 ? 1 : 0;
		}
	}
	if(konngara.row_diamond_left > 576) {
		konngara.row_direction = 2;
		konngara.row_diamond_left = 576;
	} else if(konngara.row_diamond_left < 64) {
		konngara.row_direction = 2;
		konngara.row_diamond_left = 64;
	}
	if(konngara.row_diamond_top >= 256) {
		boss.phase_frame = 0;
		return;
	}
	if(konngara.row_direction < 2 && (boss.phase_frame % konngara.pattern_state) == 0) {
		pellets_add_group(state, konngara.row_diamond_left, konngara.row_diamond_top,
			PelletGroup::one_aimed, konngara.row_pellet_speed);
		konngara.row_pellet_speed = word(static_cast<std::int32_t>(konngara.row_pellet_speed) + 2);
	}
}

void pattern_cup_spray(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	if(boss.phase_frame == 10) {
		face_expression_set(konngara, KonngaraFaceExpression::closed);
	}
	if(boss.phase_frame < 100) {
		return;
	}
	if(boss.phase_frame == 100) {
		konngara.spray_offset = 0x20U;
		konngara.spray_delta = -8;
		konngara.pellets_fired_in_direction = 0;
		konngara.pattern_state = rank_value(state, 5, 4, 3, 2);
	}
	if((boss.phase_frame % konngara.pattern_state) == 0) {
		auto angle = static_cast<std::uint8_t>(angle_between(
			278, 184, state.player.left, player_top) + konngara.spray_offset
		);
		konngara.spray_offset = static_cast<std::uint8_t>(
			static_cast<std::int16_t>(konngara.spray_offset) + konngara.spray_delta
		);
		konngara.pellets_fired_in_direction = word(
			static_cast<std::int32_t>(konngara.pellets_fired_in_direction) + 1
		);
		if(konngara.pellets_fired_in_direction > 8) {
			konngara.spray_delta = word(-konngara.spray_delta);
			konngara.pellets_fired_in_direction = 0;
		}
		pellets_add_single(state, cup_center_left, cup_top, angle, 48);
	}
	if(boss.phase_frame >= 700) {
		boss.phase_frame = 0;
	}
}

void pattern_four_snakes(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	if(boss.phase_frame == 10) {
		face_expression_set(konngara, KonngaraFaceExpression::glare);
	}
	if(boss.phase_frame < 100) {
		return;
	}
	if(boss.phase_frame == 100) {
		konngara.snakes_four.target_locked.fill(false);
		snakes_spawn(state, konngara.snakes_four, 0U, cup_center_left, cup_top);
		for(std::size_t i = 1U; i < 4U; ++i) {
			konngara.snakes_four.left[i][0] = 999;
		}
		konngara.pattern_state = rank_value(state, 18, 16, 14, 12);
		return;
	}
	if(boss.phase_frame >= 400) {
		boss.phase_frame = 0;
		return;
	}
	snakes_update(state, konngara.snakes_four);
	if(boss.phase_frame == 150) {
		snakes_spawn(state, konngara.snakes_four, 1U, cup_center_left, cup_top);
	} else if(boss.phase_frame == 200) {
		snakes_spawn(state, konngara.snakes_four, 2U, left_sleeve_left, left_sleeve_top);
	} else if(boss.phase_frame == 250) {
		snakes_spawn(state, konngara.snakes_four, 3U, left_sleeve_left, left_sleeve_top);
	}
}

void pattern_edge_rain(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	if(boss.phase_frame == 10) {
		face_expression_set(konngara, KonngaraFaceExpression::aim);
	}
	if(boss.phase_frame < 100) {
		return;
	}
	if(boss.phase_frame == 100) {
		konngara.swordray_end_x = 0;
		konngara.swordray_end_y = 264;
		konngara.pattern_state = rank_value(state, 5, 3, 2, 2);
	}
	auto &x = konngara.swordray_end_x;
	auto &y = konngara.swordray_end_y;
	if(boss.phase_frame < 125) {
		y = word(static_cast<std::int32_t>(y) - 8);
	} else if(boss.phase_frame == 125) {
		x = 8;
		y = 64;
	} else if(boss.phase_frame < 205) {
		x = word(static_cast<std::int32_t>(x) + 8);
	} else if(boss.phase_frame == 205) {
		x = 639;
		y = 72;
	} else if(boss.phase_frame < 230) {
		y = word(static_cast<std::int32_t>(y) + 8);
	} else if(boss.phase_frame == 230) {
		x = 639;
		y = 256;
	} else if(boss.phase_frame < 255) {
		y = word(static_cast<std::int32_t>(y) - 8);
	} else if(boss.phase_frame == 255) {
		x = 631;
		y = 64;
	} else if(boss.phase_frame < 335) {
		x = word(static_cast<std::int32_t>(x) - 8);
	} else if(boss.phase_frame == 335) {
		x = 0;
		y = 72;
	} else if(boss.phase_frame < 360) {
		y = word(static_cast<std::int32_t>(y) + 8);
	} else if(boss.phase_frame == 360) {
		y = word(static_cast<std::int32_t>(y) - 8);
		boss.phase_frame = 0;
	}
	if((boss.phase_frame % konngara.pattern_state) == 0) {
		pellets_add_rain(state, x, y,
			static_cast<std::uint8_t>(pc98::irand(state.random_seed) & 0x7FU), 32);
		pellets_add_rain(state, x, y,
			static_cast<std::uint8_t>(pc98::irand(state.random_seed) & 0x7FU), 32);
	}
}

void slash_animate(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame > 200) {
		boss.phase_frame = 0;
		state.boss.konngara.face_direction_can_change = true;
	}
}

void slash_rain_fire(State &state, std::int16_t left, std::int16_t top) noexcept
{
	pellets_add_rain(state, left, top,
		static_cast<std::uint8_t>(pc98::irand(state.random_seed) % 0x7FU), 0);
	pellets_add_rain(state, left, top,
		static_cast<std::uint8_t>(pc98::irand(state.random_seed) % 0x7FU), 0);
}

void slash_step_2_to_4(KonngaraSlashSpawner &state, std::int16_t steps) noexcept
{
	state.left = word(static_cast<std::int32_t>(state.left) +
		(22 / (40 / steps)));
	state.top = word(static_cast<std::int32_t>(state.top) +
		(162 / (40 / steps)));
}

void slash_step_4_to_5(KonngaraSlashSpawner &state) noexcept
{
	// Both the source distance and frame interval are negative.
	state.left = word(static_cast<std::int32_t>(state.left) - (-234 / -10));
	state.top = word(static_cast<std::int32_t>(state.top) - (-34 / -10));
}

void slash_step_5_to_6(KonngaraSlashSpawner &state, std::int16_t steps) noexcept
{
	state.left = word(static_cast<std::int32_t>(state.left) -
		(-212 / (20 / steps)));
	state.top = word(static_cast<std::int32_t>(state.top) -
		(128 / (20 / steps)));
}

void pattern_slash_rain(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	auto &spawner = konngara.slash_rain;
	if(boss.phase_frame == 10) {
		face_direction_set(konngara, KonngaraFaceDirection::center);
		face_expression_set(konngara, KonngaraFaceExpression::closed);
		konngara.face_direction_can_change = false;
		spawner.left = sword_center_left;
		spawner.top = sword_center_top;
		konngara.pattern_state = rank_value(state, 5, 3, 2, 1);
	}
	slash_animate(state);
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame < 140 && (boss.phase_frame % konngara.pattern_state) == 0) {
		slash_rain_fire(state, spawner.left, spawner.top);
		slash_step_2_to_4(spawner, konngara.pattern_state);
	}
	if(boss.phase_frame == 140) {
		spawner.left = slash_four_left;
		spawner.top = slash_four_top;
		konngara.pattern_state = rank_value(state, 3, 2, 2, 2);
	}
	if(boss.phase_frame >= 140 && boss.phase_frame < 150) {
		slash_rain_fire(state, spawner.left, spawner.top);
		slash_step_4_to_5(spawner);
	}
	if(boss.phase_frame == 150) {
		spawner.left = slash_five_left;
		spawner.top = slash_five_top;
		konngara.pattern_state = rank_value(state, 3, 2, 1, 1);
	}
	if(boss.phase_frame >= 150 && boss.phase_frame < 170 &&
		(boss.phase_frame % konngara.pattern_state) == 0) {
		slash_rain_fire(state, spawner.left, spawner.top);
		slash_step_5_to_6(spawner, konngara.pattern_state);
	}
}

void slash_triangular_fire(State &state, std::int16_t left, std::int16_t top,
	std::int16_t speed) noexcept
{
	pellets_add_single(state, left, top, 0x20U, speed);
	pellets_add_single(state, left, top, 0x60U, speed);
}

void pattern_slash_triangular(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	auto &spawner = konngara.slash_triangular;
	if(boss.phase_frame == 10) {
		face_direction_set(konngara, KonngaraFaceDirection::center);
		face_expression_set(konngara, KonngaraFaceExpression::aim);
		konngara.face_direction_can_change = false;
		spawner.left = sword_center_left;
		spawner.top = sword_center_top;
		konngara.pattern_state = rank_value(state, 32, 48, 64, 72);
	}
	slash_animate(state);
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame < 140 && (boss.phase_frame % 3) == 0) {
		slash_triangular_fire(state, spawner.left, spawner.top,
			konngara.pattern_state);
		slash_step_2_to_4(spawner, 3);
	}
	if(boss.phase_frame == 140) {
		spawner.left = slash_four_left;
		spawner.top = slash_four_top;
	}
	if(boss.phase_frame >= 140 && boss.phase_frame < 150) {
		slash_triangular_fire(state, spawner.left, spawner.top,
			konngara.pattern_state);
		slash_step_4_to_5(spawner);
	}
	if(boss.phase_frame == 150) {
		spawner.left = slash_five_left;
		spawner.top = slash_five_top;
	}
	if(boss.phase_frame >= 150 && boss.phase_frame < 170 && (boss.phase_frame % 2) == 0) {
		slash_triangular_fire(state, spawner.left, spawner.top,
			konngara.pattern_state);
		slash_step_5_to_6(spawner, 2);
	}
}

void pattern_lasers(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	if(boss.phase_frame == 10) {
		face_expression_set(konngara, KonngaraFaceExpression::aim);
	}
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame == 100) {
		konngara.lasers_right_to_left = (pc98::irand(state.random_seed) % 2U) != 0U;
		konngara.pattern_state = rank_value(state, 128, 96, 80, 64);
	}
	if((boss.phase_frame % 10) != 0) return;
	const auto index = static_cast<std::size_t>(boss.phase_frame / 10);
	const auto delta = static_cast<std::int16_t>((boss.phase_frame - 100) / 10);
	konngara.laser_target_left = konngara.lasers_right_to_left
		? word(static_cast<std::int32_t>(playfield_right) - delta * konngara.pattern_state)
		: word(static_cast<std::int32_t>(delta) * konngara.pattern_state);
	konngara.laser_target_top = playfield_bottom;
	shootout_laser_spawn_safe(state, index, sword_center_left, sword_center_top,
		konngara.laser_target_left, konngara.laser_target_top, 68, 15, 30, 5);
	pellets_add_group(state, sword_center_left, sword_center_top,
		PelletGroup::three_spread_wide_aimed, 72);
	if((!konngara.lasers_right_to_left && konngara.laser_target_left >= playfield_right) ||
		(konngara.lasers_right_to_left && konngara.laser_target_left <= playfield_left)) {
		boss.phase_frame = 0;
	}
}

void slash_aimed_fire(State &state, std::int16_t left, std::int16_t top,
	std::int16_t speed) noexcept
{
	pellets_add_group(state, left, top, PelletGroup::one_aimed, speed);
}

void pattern_slash_aimed(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	auto &spawner = konngara.slash_aimed;
	if(boss.phase_frame == 10) {
		face_direction_set(konngara, KonngaraFaceDirection::center);
		face_expression_set(konngara, KonngaraFaceExpression::aim);
		konngara.face_direction_can_change = false;
		spawner.left = sword_center_left;
		spawner.top = sword_center_top;
		konngara.pattern_state = rank_value(state, 64, 80, 88, 96);
	}
	slash_animate(state);
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame < 140 && (boss.phase_frame % 3) == 0) {
		slash_aimed_fire(state, spawner.left, spawner.top,
			konngara.pattern_state);
		slash_step_2_to_4(spawner, 3);
	}
	if(boss.phase_frame == 140) {
		spawner.left = slash_four_left;
		spawner.top = slash_four_top;
	}
	if(boss.phase_frame >= 140 && boss.phase_frame < 150) {
		slash_aimed_fire(state, spawner.left, spawner.top,
			konngara.pattern_state);
		slash_step_4_to_5(spawner);
	}
	if(boss.phase_frame == 150) {
		spawner.left = slash_five_left;
		spawner.top = slash_five_top;
	}
	if(boss.phase_frame >= 150 && boss.phase_frame < 170 && (boss.phase_frame % 2) == 0) {
		slash_aimed_fire(state, spawner.left, spawner.top,
			konngara.pattern_state);
		slash_step_5_to_6(spawner, 2);
	}
}

void pattern_sleeve_rain(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 10) {
		face_expression_set(state.boss.konngara, KonngaraFaceExpression::aim);
	}
	if(boss.phase_frame < 100) return;
	if((boss.phase_frame % 20) == 0) {
		std::uint8_t angle{};
		for(int i = 0; i < 10; ++i, angle = static_cast<std::uint8_t>(angle - 12U)) {
			pellets_add_rain(state, left_sleeve_left, left_sleeve_top, angle, 32);
		}
	} else if((boss.phase_frame % 20) == 10) {
		std::uint8_t angle{};
		for(int i = 0; i < 10; ++i, angle = static_cast<std::uint8_t>(angle - 12U)) {
			pellets_add_single(state, left_sleeve_left, left_sleeve_top, angle, 32,
				PelletMotion::fall_straight_from_top_then_regular, 48);
		}
	}
	if(boss.phase_frame >= 300) boss.phase_frame = 0;
}

void phase_flash(State &state, std::int8_t next) noexcept
{
	frame_common(state);
	hit_update(state);
	if(!state.boss.konngara.hit_invincible && state.boss.phase_frame > 120) {
		phase_next(state, next);
	}
}

void final_defeat(State &state) noexcept
{
	// b20j.cpp's long kuji/shake/scroll is presentation-only. Its gameplay
	// endpoint is five 10,000-point additions and the game-clear flag.
	state.boss.phase_frame = 65;
	for(std::int16_t i = 0; i < 5; ++i) {
		state.score += 10000U;
	}
	state.game_cleared = true;
	state.boss.active = false;
}

} // namespace

void konngara_setup(State &state) noexcept
{
	auto &boss = state.boss;
	boss.id = BossId::konngara;
	boss.hp = 18;
	boss.phase = 0;
	boss.phase_frame = 0;
	boss.active = true;
	state.projectiles.pellet_interlace = true;
	boss.konngara.face_direction = KonngaraFaceDirection::center;
	boss.konngara.face_expression = KonngaraFaceExpression::neutral;
	boss.konngara.face_direction_can_change = true;
}

[[nodiscard]] BossUpdateResult konngara_update(State &state) noexcept
{
	auto &boss = state.boss;
	auto &konngara = boss.konngara;
	if(!boss.active) return BossUpdateResult::unsupported;
	if(boss.phase == 0) {
		boss.phase = 1;
		konngara.pattern_previous = choose_new;
		konngara.pattern_current = choose_new;
		konngara.patterns_done = 0;
		boss.phase_frame = 0;
		konngara.hit_invincibility_frame = 0;
		konngara.hit_invincible = false;
		konngara.initial_hp_rendered = false;
		state.random_seed = state.frame_rand;
		return BossUpdateResult::running;
	}
	if(boss.phase == 1) {
		if(!konngara.initial_hp_rendered && boss.phase_frame == boss.hp - 1)
			konngara.initial_hp_rendered = true;
		frame_common(state);
		switch(konngara.pattern_current) {
		case 0: pattern_diamonds(state); break;
		case 1: pattern_cup(state); break;
		case 2: pattern_two_snakes(state); break;
		case choose_new: choose_pattern(state, 120, 3U, 3U); break;
		default: return BossUpdateResult::unsupported;
		}
		if(boss.phase_frame == 0) {
			face_expression_set(konngara, KonngaraFaceExpression::neutral);
			konngara.pattern_current = choose_new;
		}
		hit_update(state);
		if(!konngara.hit_invincible &&
			(konngara.patterns_done >= 7 || boss.hp < 16) &&
			konngara.pattern_current == choose_new) {
			phase_next(state, 2);
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 2) {
		phase_flash(state, 3);
		return BossUpdateResult::running;
	}
	if(boss.phase == 3) {
		frame_common(state);
		switch(konngara.pattern_current) {
		case 0: pattern_rows(state); break;
		case 1: pattern_cup_spray(state); break;
		case 2: pattern_four_snakes(state); break;
		case 3: pattern_edge_rain(state); break;
		case choose_new: choose_pattern(state, 120, 4U, 4U); break;
		default: return BossUpdateResult::unsupported;
		}
		if(boss.phase_frame == 0) {
			face_expression_set(konngara, KonngaraFaceExpression::neutral);
			konngara.pattern_current = choose_new;
		}
		hit_update(state);
		if(!konngara.hit_invincible &&
			(konngara.patterns_done >= 9 || boss.hp < 13) &&
			konngara.pattern_current == choose_new) {
			phase_next(state, 4);
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 4) {
		phase_flash(state, 5);
		return BossUpdateResult::running;
	}
	if(boss.phase == 5) {
		frame_common(state);
		switch(konngara.pattern_current) {
		case 0: pattern_slash_rain(state); break;
		case 1: pattern_lasers(state); break;
		case 2: pattern_slash_triangular(state); break;
		case choose_new: choose_pattern(state, 120, 3U, 2U); break;
		default: return BossUpdateResult::unsupported;
		}
		if(boss.phase_frame == 0) {
			face_expression_set(konngara, KonngaraFaceExpression::neutral);
			konngara.pattern_current = choose_new;
		}
		hit_update(state);
		if(!konngara.hit_invincible &&
			(konngara.patterns_done >= 6 || boss.hp < 10) &&
			konngara.pattern_current == choose_new) {
			phase_next(state, 6);
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 6) {
		phase_flash(state, 7);
		return BossUpdateResult::running;
	}
	if(boss.phase == 7) {
		frame_common(state);
		switch(konngara.pattern_current) {
		case 0: pattern_diamonds(state); break;
		case 1: pattern_cup(state); break;
		case 2: pattern_two_snakes(state); break;
		case 3: pattern_rows(state); break;
		case 4: pattern_cup_spray(state); break;
		case 5: pattern_four_snakes(state); break;
		case 6: pattern_edge_rain(state); break;
		case 7: pattern_slash_rain(state); break;
		case 8: pattern_lasers(state); break;
		case 9: pattern_slash_triangular(state); break;
		case 10: pattern_sleeve_rain(state); break;
		case 11: pattern_slash_aimed(state); break;
		case choose_new: choose_pattern(state, 5, 12U, 2U); break;
		default: return BossUpdateResult::unsupported;
		}
		if(boss.phase_frame == 0) {
			face_expression_set(konngara, KonngaraFaceExpression::neutral);
			konngara.pattern_current = choose_new;
		}
		hit_update(state);
		if(boss.hp <= 0) final_defeat(state);
		return BossUpdateResult::running;
	}
	return BossUpdateResult::unsupported;
}

void konngara_render_tail(State &) noexcept
{
	// b20j.cpp's remaining calls render face, ray, snake, and slash graphics.
}

} // namespace th01::core
