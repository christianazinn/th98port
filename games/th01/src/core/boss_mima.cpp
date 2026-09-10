#include "th01/core/boss_mima.hpp"

#include "th01/core/boss.hpp"
#include "th01/core/projectiles.hpp"
#include "th01/core/state.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/random.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace th01::core {
namespace {

constexpr std::int16_t playfield_left = 0;
constexpr std::int16_t playfield_top = 64;
constexpr std::int16_t playfield_right = 640;
constexpr std::int16_t playfield_bottom = 400;
constexpr std::int16_t playfield_center_x = 320;
constexpr std::int16_t player_width = 32;
constexpr std::int16_t player_height = 32;
constexpr std::int16_t player_top = playfield_bottom - player_height;
constexpr std::int16_t pellet_width = 8;
constexpr std::int16_t pellet_height = 8;
constexpr std::int16_t missile_width = 16;
constexpr std::int16_t mima_width = 128;
constexpr std::int16_t mima_height = 160;
constexpr std::int16_t mima_base_center_y =
	playfield_top + ((playfield_bottom - playfield_top) / 42) * 17;
constexpr std::int16_t mima_base_left = playfield_center_x - (mima_width / 2);
constexpr std::int16_t mima_base_top = mima_base_center_y - (mima_height / 2);
constexpr std::int16_t seal_radius = 80;
constexpr std::int16_t seal_circumsquare_radius = 112;
constexpr std::int16_t square_interval = 8;
constexpr std::int16_t square_radius_step = 8;
constexpr std::int16_t hp_total = 12;
constexpr std::int16_t hp_phase_1_end = 6;
constexpr std::int16_t hp_phase_3_end = 0;
constexpr std::int16_t mima_pillar_count = 8;
constexpr std::int16_t pillar_time_none = -1;
constexpr std::int16_t pillar_time_circles = 128;
constexpr std::int16_t pillar_time_pillars = 32;
constexpr std::int16_t pillar_time_done = 16;
constexpr std::int16_t pillar_width = 32;

[[nodiscard]] constexpr std::int16_t word(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr std::int16_t q12_4_to_pixel(std::int16_t value) noexcept
{
	return static_cast<std::int16_t>((value >= 0) ? (value / 16) :
		-(((-value) + 15) / 16));
}

[[nodiscard]] constexpr std::int16_t rank_value(
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

[[nodiscard]] constexpr std::int16_t mima_center_x(const MimaState &mima) noexcept
{
	return word(static_cast<std::int32_t>(mima.left) + (mima_width / 2));
}

[[nodiscard]] constexpr std::int16_t mima_center_y(const MimaState &mima) noexcept
{
	return word(static_cast<std::int32_t>(mima.top) + (mima_height / 2));
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

[[nodiscard]] std::int16_t polar_x(
	std::int16_t center,
	std::int16_t radius,
	std::uint8_t angle
) noexcept
{
	return word(static_cast<std::int32_t>(center) +
		pc98::masterlib::vector2(angle, radius).x);
}

[[nodiscard]] std::int16_t polar_y(
	std::int16_t center,
	std::int16_t radius,
	std::uint8_t angle
) noexcept
{
	return word(static_cast<std::int32_t>(center) +
		pc98::masterlib::vector2(angle, radius).y);
}

struct MimaSquareCorners {
	std::array<std::int16_t, 4U> x{};
	std::array<std::int16_t, 4U> y{};
	std::int16_t center_x{};
	std::int16_t center_y{};
};

void square_corners_set(
	MimaSquareCorners &corners,
	const MimaState &mima,
	std::int16_t radius,
	std::uint8_t angle
) noexcept
{
	corners.center_x = mima_center_x(mima);
	corners.center_y = mima_center_y(mima);
	for(std::size_t i = 0U; i < corners.x.size(); ++i) {
		corners.x[i] = polar_x(corners.center_x, radius, angle);
		corners.y[i] = polar_y(corners.center_y, radius, angle);
		angle = static_cast<std::uint8_t>(angle + 0x40U);
	}
}

void square_init(MimaSquareState &square) noexcept
{
	square.radius = static_cast<std::int16_t>((seal_radius * 4) / 10);
	square.angle = 0U;
}

void mima_frame_common(State &state) noexcept
{
	state.boss.phase_frame = word(static_cast<std::int32_t>(state.boss.phase_frame) + 1);
	state.boss.mima.hit_invincibility_frame = word(
		static_cast<std::int32_t>(state.boss.mima.hit_invincibility_frame) + 1
	);
}

[[nodiscard]] bool mima_orb_hittest(const State &state) noexcept
{
	const auto &mima = state.boss.mima;
	if(mima.orb_hitbox_inactive) return false;
	// CBossEntity::hitbox_orb_set(16, 16, 112, 144), followed by its
	// centered 16x16 Orb hittest against orb_prev_*.
	return state.orb.prev_left >= word(static_cast<std::int32_t>(mima.left) + 8) &&
		state.orb.prev_left <= word(static_cast<std::int32_t>(mima.left) + 88) &&
		state.orb.prev_top >= word(static_cast<std::int32_t>(mima.top) + 8) &&
		state.orb.prev_top <= word(static_cast<std::int32_t>(mima.top) + 120);
}

void mima_hit_update(State &state) noexcept
{
	auto &mima = state.boss.mima;
	static_cast<void>(boss_hit_update(
		state,
		mima.hit_invincibility_frame,
		mima.hit_invincible,
		mima_orb_hittest(state),
		word(static_cast<std::int32_t>(mima.left) + 16),
		word(static_cast<std::int32_t>(mima.top) + 48),
		80,
		32,
		5000
	));
}

void phase_spreadin(State &state, std::int16_t final_left, std::int16_t final_top) noexcept
{
	auto &boss = state.boss;
	auto &mima = boss.mima;
	if(boss.phase_frame < 10) return;
	if(boss.phase_frame == 10) {
		mima.left = final_left;
		mima.top = final_top;
		return;
	}
	if((boss.phase_frame % mima.spreadin_interval) != 0) return;
	const auto bottom = static_cast<std::int16_t>(
		(static_cast<std::int16_t>(mima.spreadin_speed / static_cast<std::int8_t>(
			mima.spreadin_interval
		)) * (boss.phase_frame - 10))
	);
	if(((mima_height / 2) - bottom) < 0) {
		boss.phase_frame = 0;
	}
}

void mima_pattern_aimed_then_static_pellets(State &state) noexcept
{
	auto &boss = state.boss;
	auto &mima = boss.mima;
	auto &square = mima.square_aimed_pellets;
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame == 100) {
		square_init(square);
		mima.pattern_state = rank_value(state, 64, 72, 80, 88);
	}
	if((boss.phase_frame % square_interval) == 0) {
		MimaSquareCorners corners{};
		square_corners_set(corners, mima, square.radius, square.angle);
		square.angle = static_cast<std::uint8_t>(square.angle +
			((boss.phase_frame < 260) ? 0x0CU : static_cast<std::uint8_t>(0U - 0x0CU)));
		if(square.radius < seal_circumsquare_radius) {
			square.radius = word(static_cast<std::int32_t>(square.radius) + square_radius_step);
		} else if(boss.phase_frame > 280) {
			for(std::size_t i = 0U; i < corners.x.size(); ++i) {
				pellets_add_single(
					state,
					corners.x[i],
					corners.y[i],
					angle_between(corners.center_x, corners.center_y, corners.x[i], corners.y[i]),
					mima.pattern_state
				);
			}
		} else {
			for(std::size_t i = 0U; i < corners.x.size(); ++i) {
				const auto angle = angle_between(
					corners.x[i], corners.y[i],
					word(static_cast<std::int32_t>(state.player.left) + 12),
					static_cast<std::int16_t>(player_top + (player_height / 2))
				);
				pellets_add_single(state, corners.x[i], corners.y[i], angle,
					static_cast<std::int16_t>(mima.pattern_state / 2));
			}
		}
	}
	if(boss.phase_frame > 360) boss.phase_frame = 0;
}

void mima_pattern_aimed_missiles(State &state) noexcept
{
	auto &boss = state.boss;
	auto &mima = boss.mima;
	auto &square = mima.square_aimed_missiles;
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame == 100) {
		square_init(square);
		mima.pattern_state = rank_value(state, 96, 104, 112, 118);
	}
	if((boss.phase_frame % square_interval) == 0) {
		MimaSquareCorners corners{};
		square_corners_set(corners, mima, square.radius, square.angle);
		square.angle = static_cast<std::uint8_t>(square.angle - 0x0CU);
		if(square.radius < seal_radius) {
			square.radius = word(static_cast<std::int32_t>(square.radius) + square_radius_step);
		} else if(boss.phase_frame == 224) {
			mima.target_left = state.player.left;
		} else if(boss.phase_frame > 240) {
			const auto velocity = pc98::masterlib::vector2(
				angle_between(
					corners.center_x,
					corners.center_y,
					word(static_cast<std::int32_t>(mima.target_left) +
						(player_width / 2) - (missile_width / 2)),
					static_cast<std::int16_t>(player_top + (player_height / 2))
				),
				mima.pattern_state
			);
			for(std::size_t i = 0U; i < corners.x.size(); ++i) {
				missiles_add(
					state,
					corners.x[i],
					corners.y[i],
					pellet_pixels_to_subpixels(q12_4_to_pixel(velocity.x)),
					pellet_pixels_to_subpixels(q12_4_to_pixel(velocity.y))
				);
			}
		}
	}
	if(boss.phase_frame > 320) boss.phase_frame = 0;
}

void mima_pattern_two_rotating_squares(State &state) noexcept
{
	auto &boss = state.boss;
	auto &mima = boss.mima;
	auto &square = mima.square_two_pellets;
	if(boss.phase_frame == 50) mima.meteor_active = false;
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame == 100) {
		square_init(square);
		mima.pattern_state = rank_value(state, 64, 72, 80, 88);
	}
	if((boss.phase_frame % square_interval) == 0) {
		MimaSquareCorners counterclockwise{};
		MimaSquareCorners clockwise{};
		square_corners_set(counterclockwise, mima, square.radius, square.angle);
		square_corners_set(clockwise, mima, square.radius,
			static_cast<std::uint8_t>(0U - square.angle));
		square.angle = static_cast<std::uint8_t>(square.angle - 0x06U);
		state.projectiles.pellet_spawn_with_cloud = true;
		if(square.radius < seal_circumsquare_radius) {
			square.radius = word(static_cast<std::int32_t>(square.radius) + square_radius_step);
		} else {
			for(std::size_t i = 0U; i < counterclockwise.x.size(); ++i) {
				pellets_add_single(
					state,
					counterclockwise.x[i],
					counterclockwise.y[i],
					angle_between(
						counterclockwise.center_x,
						counterclockwise.center_y,
						counterclockwise.x[i],
						counterclockwise.y[i]
					),
					mima.pattern_state
				);
				pellets_add_single(
					state,
					clockwise.x[i],
					clockwise.y[i],
					angle_between(
						clockwise.center_x,
						clockwise.center_y,
						clockwise.x[i],
						clockwise.y[i]
					),
					mima.pattern_state
				);
			}
		}
		state.projectiles.pellet_spawn_with_cloud = false;
	}
	if(boss.phase_frame > 320) {
		boss.phase_frame = 0;
		mima.meteor_active = true;
	}
}

void mima_pattern_hop_and_chase(State &state, bool initialize) noexcept
{
	auto &boss = state.boss;
	auto &mima = boss.mima;
	if(initialize) {
		mima.hop = 0xFFU;
		mima.spreadin_interval = 4U;
		mima.hop_direction = static_cast<std::int8_t>(pc98::irand(state.random_seed) % 2U);
		mima.spreadin_speed = 16;
		return;
	}
	if(mima.hop == 0xFFU) {
		mima.meteor_active = false;
		mima.hop = 0U;
		boss.phase_frame = 1;
	}
	if(mima.hop == 0U) {
		mima.orb_hitbox_inactive = true;
		if((boss.phase_frame >= 10) && ((boss.phase_frame % 4) == 0)) {
			if(static_cast<std::int16_t>((boss.phase_frame - 10) * 2) >=
				(mima_height / 2)) {
				boss.phase_frame = 0;
			}
		}
	} else {
		if(boss.phase_frame == 4) {
			mima.left = (mima.hop_direction == 0)
				? static_cast<std::int16_t>(playfield_left +
					(static_cast<std::int16_t>(mima.hop) - 1) * (640 / 5))
				: static_cast<std::int16_t>(playfield_right -
					static_cast<std::int16_t>(mima.hop) * (640 / 5));
			mima.top = static_cast<std::int16_t>(mima_base_top - ((playfield_bottom - playfield_top) / 14));
		}
		mima.orb_hitbox_inactive = boss.phase_frame < 12;
		phase_spreadin(state, mima.left, mima.top);
	}
	if(boss.phase_frame != 0) return;

	if(mima.hop != 0U) {
		const auto speed = rank_value(state, 30, 35, 40, 45);
		const auto pellet_count = rank_value(state, 5, 8, 10, 12);
		std::uint8_t angle{};
		for(std::int16_t i = 0; i < pellet_count; ++i) {
			pellets_add_single(
				state,
				mima_center_x(mima),
				mima_center_y(mima),
				angle,
				speed,
				PelletMotion::chase,
				speed
			);
			angle = static_cast<std::uint8_t>(angle +
				static_cast<std::uint8_t>(0x100 / pellet_count));
		}
	}
	if(mima.hop >= 4U) {
		mima.hop = 0xFFU;
		mima.hop_direction = static_cast<std::int8_t>(1 - mima.hop_direction);
		mima.meteor_active = true;
		boss.phase_frame = 0;
	} else {
		mima.hop = static_cast<std::uint8_t>(mima.hop + 1U);
		boss.phase_frame = 1;
	}
}

[[nodiscard]] std::int16_t mima_pillar_random_x(
	State &state,
	std::int16_t left,
	std::int16_t width
) noexcept
{
	return word(static_cast<std::int32_t>(left) +
		static_cast<std::int16_t>(pc98::irand(state.random_seed) %
			static_cast<std::uint16_t>(width)));
}

void mima_pattern_pillars_and_aimed_spreads(State &state) noexcept
{
	auto &boss = state.boss;
	auto &mima = boss.mima;
	if(boss.phase_frame == 50) mima.meteor_active = false;
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame == 100) {
		for(auto &pillar : mima.pillars) {
			pillar.time = pillar_time_circles;
			pillar.bottom = playfield_bottom;
		}
		mima.pillars[0].center_x = mima_pillar_random_x(state, 16, 48);
		mima.pillars[1].center_x = word(static_cast<std::int32_t>(
			(playfield_right - pillar_width) - mima_pillar_random_x(state, 0, 48) +
			(pillar_width / 2)
		));
		mima.pillars[2].center_x = mima_pillar_random_x(state, 64, 224);
		mima.pillars[3].center_x = mima_pillar_random_x(state, 336, 224);
		mima.pillars[4].center_x = mima_pillar_random_x(state, 128, 160);
		mima.pillars[5].center_x = mima_pillar_random_x(state, 336, 160);
		mima.pillars[6].center_x = mima_pillar_random_x(state, 64, 224);
		mima.pillars[7].center_x = mima_pillar_random_x(state, 336, 224);
		mima.pattern_state = rank_value(state, 40, 45, 50, 55);
	}
	if(boss.phase_frame == 240) mima.meteor_active = true;

	for(std::size_t i = 0U; i < mima.pillars.size(); ++i) {
		auto &pillar = mima.pillars[i];
		const auto first_circle_frame = static_cast<std::int16_t>(100 +
			static_cast<std::int16_t>(i) * 20);
		const auto is_circle_frame =
			((boss.phase_frame % mima_pillar_count) == static_cast<std::int16_t>(i)) &&
			(boss.phase_frame > first_circle_frame);
		if(!is_circle_frame) continue;
		if(pillar.time > pillar_time_pillars) {
			pillar.time = word(static_cast<std::int32_t>(pillar.time) - mima_pillar_count);
		} else if(pillar.time > pillar_time_done) {
			if(!state.player.invincible &&
				state.player.left > word(static_cast<std::int32_t>(pillar.center_x) -
					player_width - (pillar_width / 4)) &&
				state.player.left < word(static_cast<std::int32_t>(pillar.center_x) +
					(pillar_width / 4))) {
				state.input.player_is_hit = true;
			}
			pillar.time = word(static_cast<std::int32_t>(pillar.time) - mima_pillar_count);
		} else if(pillar.time != pillar_time_none) {
			pellets_add_group(
				state,
				mima_center_x(mima),
				mima_center_y(mima),
				PelletGroup::three_spread_wide_aimed,
				36
			);
			pillar.time = pillar_time_none;
		}
	}

	bool all_done = true;
	for(const auto &pillar : mima.pillars) {
		if(pillar.time != pillar_time_none) {
			all_done = false;
			break;
		}
	}
	if(all_done) boss.phase_frame = 0;
}

void mima_pattern_halfcircle_missiles(State &state) noexcept
{
	auto &boss = state.boss;
	auto &mima = boss.mima;
	auto &square = mima.square_halfcircle_missiles;
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame == 100) {
		square_init(square);
		mima.missile_angle = 0U;
		mima.pattern_state = rank_value(state, 4, 4, 5, 5);
	}
	if((boss.phase_frame % square_interval) == 0) {
		MimaSquareCorners corners{};
		square_corners_set(corners, mima, square.radius, square.angle);
		square.angle = static_cast<std::uint8_t>(square.angle - 0x0CU);
		if(square.radius < seal_radius) {
			square.radius = word(static_cast<std::int32_t>(square.radius) + square_radius_step);
		} else if((boss.phase_frame > 180) && ((boss.phase_frame % 16) == 8)) {
			const auto velocity = pc98::masterlib::vector2(mima.missile_angle,
				mima.pattern_state);
			for(std::size_t i = 0U; i < corners.x.size(); ++i) {
				missiles_add(
					state,
					corners.x[i],
					corners.y[i],
					pellet_pixels_to_subpixels(velocity.x),
					pellet_pixels_to_subpixels(velocity.y)
				);
			}
			mima.missile_angle = static_cast<std::uint8_t>(mima.missile_angle + 0x0DU);
		}
	}
	if(boss.phase_frame > 340) boss.phase_frame = 0;
}

void mima_pattern_slow_pellet_spray(State &state) noexcept
{
	auto &boss = state.boss;
	auto &mima = boss.mima;
	auto &square = mima.square_slow_spray;
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame == 100) {
		square_init(square);
		mima.pellet_angle = 0x80U;
		mima.pattern_state = rank_value(state, 32, 40, 48, 56);
	}
	if((boss.phase_frame % square_interval) == 0) {
		MimaSquareCorners corners{};
		square_corners_set(corners, mima, square.radius, square.angle);
		square.angle = static_cast<std::uint8_t>(square.angle +
			((boss.phase_frame > 270) ? 0x0CU : static_cast<std::uint8_t>(0U - 0x0CU)));
		if(square.radius < seal_radius) {
			square.radius = word(static_cast<std::int32_t>(square.radius) + square_radius_step);
		} else if(boss.phase_frame > 180) {
			mima.pellet_angle = static_cast<std::uint8_t>(mima.pellet_angle +
				((boss.phase_frame > 270) ? 0x0CU : static_cast<std::uint8_t>(0U - 0x0CU)));
			for(std::size_t i = 0U; i < corners.x.size(); ++i) {
				pellets_add_single(state, corners.x[i], corners.y[i],
					mima.pellet_angle, mima.pattern_state);
			}
		}
	}
	if(boss.phase_frame > 370) boss.phase_frame = 0;
}

void mima_pattern_aimed_lasers(State &state) noexcept
{
	auto &boss = state.boss;
	auto &mima = boss.mima;
	auto &square = mima.square_lasers;
	if(boss.phase_frame == 50) mima.meteor_active = false;
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame == 100) {
		square_init(square);
		mima.pattern_state = rank_value(state, 50, 54, 58, 62);
	}
	if((boss.phase_frame % square_interval) == 0) {
		MimaSquareCorners corners{};
		square_corners_set(corners, mima, square.radius, square.angle);
		square.angle = static_cast<std::uint8_t>(square.angle + 0x03U);
		if(square.radius < seal_radius) {
			square.radius = word(static_cast<std::int32_t>(square.radius) + square_radius_step);
		}
		square_corners_set(corners, mima, square.radius, square.angle);
		mima.laser_corner_x = corners.x;
		mima.laser_corner_y = corners.y;
	}
	if((boss.phase_frame > 180) && (boss.phase_frame < 300)) {
		const auto i = static_cast<std::size_t>(boss.phase_frame % 4);
		shootout_laser_spawn(
			state,
			i,
			mima.laser_corner_x[i],
			mima.laser_corner_y[i],
			word(static_cast<std::int32_t>(state.player.left) + (player_width / 2) - 2),
			static_cast<std::int16_t>(player_top + player_height),
			mima.pattern_state,
			15U,
			20,
			4
		);
	}
	if(boss.phase_frame > 300) boss.phase_frame = 0;
}

void mima_defeat(State &state) noexcept
{
	pellets_unput_and_reset_nonclouds(state);
	missiles_reset(state);
	// b10j.cpp only resets slots 0..4 despite the ten-slot global pool.
	shootout_lasers_reset_first(state, 5U);
	state.stage_cleared = true;
	state.input.player_is_hit = true;
}

} // namespace

void mima_setup(State &state) noexcept
{
	auto &boss = state.boss;
	auto &mima = boss.mima;
	boss.id = BossId::mima;
	boss.active = true;
	boss.phase = 0;
	boss.phase_frame = 0;
	boss.hp = hp_total;
	mima.left = mima_base_left;
	mima.top = playfield_top;
	mima.orb_hitbox_inactive = false;
	missiles_reset(state);
}

[[nodiscard]] BossUpdateResult mima_update(State &state) noexcept
{
	auto &boss = state.boss;
	auto &mima = boss.mima;
	if(!boss.active || boss.id != BossId::mima) return BossUpdateResult::unsupported;

	// mima_main() advances this separate global pool before every branch.
	missiles_update(state);
	if(boss.phase == 0) {
		// The original's blocking spread-in has no gameplay mutation between
		// the setup state and this endpoint beyond the one hop-direction IRAND.
		mima.left = mima_base_left;
		mima.top = mima_base_top;
		mima.orb_hitbox_inactive = false;
		mima.spreadin_interval = 1U;
		mima.spreadin_speed = 2;
		mima.meteor_active = true;
		boss.phase = 1;
		boss.phase_frame = 0;
		mima.pattern_cur = 0;
		mima.initial_hp_rendered = false;
		mima_pattern_hop_and_chase(state, true);
		return BossUpdateResult::running;
	}
	if(boss.phase == 1) {
		mima_frame_common(state);
		switch(mima.pattern_cur) {
		case 0: mima_pattern_aimed_then_static_pellets(state); break;
		case 1: mima_pattern_aimed_missiles(state); break;
		case 2: mima_pattern_two_rotating_squares(state); break;
		case 3: mima_pattern_hop_and_chase(state, false); break;
		default: return BossUpdateResult::unsupported;
		}
		if(boss.phase_frame == 0) {
			mima.pattern_cur = (mima.pattern_cur == 3) ? 0 :
				word(static_cast<std::int32_t>(mima.pattern_cur) + 1);
			if(boss.hp <= hp_phase_1_end) mima.pattern_cur = 99;
		}
		mima_hit_update(state);
		if((mima.pattern_cur == 99) && !mima.hit_invincible) {
			mima.spreadin_interval = 4U;
			mima.spreadin_speed = 8;
			mima.left = mima_base_left;
			mima.top = mima_base_top;
			boss.phase = 2;
			boss.phase_frame = 0;
			mima.hit_invincibility_frame = 0;
			mima.pattern_cur = 0;
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 2) {
		mima_frame_common(state);
		phase_spreadin(state, mima_base_left, mima_base_top);
		if(boss.phase_frame == 0) {
			boss.phase = 3;
			mima.pattern_cur = 0;
			mima.hit_invincibility_frame = 0;
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 3) {
		mima_frame_common(state);
		switch(mima.pattern_cur) {
		case 0: mima_pattern_pillars_and_aimed_spreads(state); break;
		case 1: mima_pattern_halfcircle_missiles(state); break;
		case 2: mima_pattern_slow_pellet_spray(state); break;
		case 3: mima_pattern_aimed_lasers(state); break;
		default: return BossUpdateResult::unsupported;
		}
		if(boss.phase_frame == 0) {
			mima.pattern_cur = (mima.pattern_cur == 3) ? 0 :
				word(static_cast<std::int32_t>(mima.pattern_cur) + 1);
		}
		mima_hit_update(state);
		if(boss.hp <= hp_phase_3_end) {
			mima_defeat(state);
			return BossUpdateResult::stage_cleared;
		}
		return BossUpdateResult::running;
	}
	return BossUpdateResult::unsupported;
}

void mima_render_tail(State &) noexcept
{
	// b10j.cpp's remaining tail work is two-page sprite/line/palette output.
	// Its gameplay mutations (pillars, spawns, collision, pool ordering) stay
	// at their literal source positions in mima_update().
}

} // namespace th01::core
