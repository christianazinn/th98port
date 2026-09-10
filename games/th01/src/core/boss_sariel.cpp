#include "th01/core/boss_sariel.hpp"

#include "th01/core/boss.hpp"
#include "th01/core/projectiles.hpp"
#include "th01/core/state.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/random.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace th01::core {
namespace {

constexpr std::int16_t playfield_left = 0;
constexpr std::int16_t playfield_top = 64;
constexpr std::int16_t playfield_right = 640;
constexpr std::int16_t playfield_bottom = 400;
constexpr std::int16_t playfield_width = playfield_right - playfield_left;
constexpr std::int16_t playfield_height = playfield_bottom - playfield_top;
constexpr std::int16_t playfield_center_x = 320;
constexpr std::int16_t player_top = 368;
constexpr std::int16_t player_width = 32;
constexpr std::int16_t player_height = 32;
constexpr std::int16_t pellet_width = 8;
constexpr std::int16_t pellet_height = 8;
constexpr std::int16_t orb_width = 32;
constexpr std::int16_t orb_height = 32;

constexpr std::int16_t shield_left = 304;
constexpr std::int16_t shield_top = 144;
constexpr std::int16_t shield_center_x = 320;
constexpr std::int16_t shield_center_y = 164;
constexpr std::int16_t seal_center_x = 320;
constexpr std::int16_t seal_center_y = 185;
constexpr std::int16_t seal_radius = 140;
constexpr std::int16_t wand_emit_left = 340;
constexpr std::int16_t wand_emit_top = 64;
constexpr std::int16_t face_left = 314;
constexpr std::int16_t face_top = 104;
constexpr std::int16_t face_width = 64;
constexpr std::int16_t face_height = 64;
constexpr std::int16_t face_center_y = face_top + (face_height / 4);
constexpr std::int16_t a2_orb_left = 64;
constexpr std::int16_t a2_orb_right = 568;
constexpr std::int16_t a2_orb_top = 160;

constexpr std::int16_t phase_form1_defeated = 99;
constexpr std::int16_t phase_form2 = 100;
constexpr std::int16_t phase_form2_defeated = 101;
constexpr std::int16_t snowflake_reset = -1;
constexpr std::int16_t snowflake_wand_raise = 0;
constexpr std::int16_t snowflake_spawn = 1;
constexpr std::int16_t snowflake_moving = 2;
constexpr std::int16_t snowflake_detonation_start = 3;
constexpr std::int16_t snowflake_detonation_active = 5;
constexpr std::int16_t leaf_free = 0;
constexpr std::int16_t leaf_spark = 1;
constexpr std::int16_t leaf_splash = 2;
constexpr std::int16_t leaf_splash_done = 5;
constexpr std::int16_t leaf = 6;

[[nodiscard]] constexpr std::int16_t word(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr std::int16_t q12_4_pixel(std::int16_t value) noexcept
{
	return value >= 0 ? static_cast<std::int16_t>(value / 16) :
		static_cast<std::int16_t>(-(((-value) + 15) / 16));
}

[[nodiscard]] constexpr std::int16_t q12_4_pixels(std::int16_t value) noexcept
{
	return word(static_cast<std::int32_t>(value) * 16);
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

[[nodiscard]] std::int16_t random_x(State &state) noexcept
{
	return static_cast<std::int16_t>(pc98::irand(state.random_seed) % 640U);
}

[[nodiscard]] std::int16_t random_x_range(
	State &state, std::int16_t first, std::int16_t width
) noexcept
{
	return word(static_cast<std::int32_t>(first) +
		static_cast<std::int16_t>(pc98::irand(state.random_seed) %
			static_cast<std::uint16_t>(width)));
}

[[nodiscard]] std::int16_t random_y_range(
	State &state, std::int16_t first, std::int16_t height
) noexcept
{
	return word(static_cast<std::int32_t>(playfield_top) + first +
		static_cast<std::int16_t>(pc98::irand(state.random_seed) %
			static_cast<std::uint16_t>(height)));
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

[[nodiscard]] pc98::masterlib::Vector2 vector_between(
	std::int16_t from_left,
	std::int16_t from_top,
	std::int16_t to_left,
	std::int16_t to_top,
	std::int16_t speed
) noexcept
{
	return pc98::masterlib::vector2(
		angle_between(from_left, from_top, to_left, to_top), speed
	);
}

[[nodiscard]] constexpr std::int16_t polar_x(
	std::int16_t center, std::int16_t radius, std::uint8_t angle
) noexcept
{
	return word(static_cast<std::int32_t>(center) +
		pc98::masterlib::vector2(angle, radius).x);
}

[[nodiscard]] constexpr std::int16_t polar_y(
	std::int16_t center, std::int16_t radius, std::uint8_t angle
) noexcept
{
	return word(static_cast<std::int32_t>(center) +
		pc98::masterlib::vector2(angle, radius).y);
}

void pellets_add_rain(
	State &state,
	std::int16_t left,
	std::int16_t top,
	std::uint8_t angle,
	std::int16_t speed_q12_4
) noexcept
{
	pellets_add_single(
		state, left, top, angle, speed_q12_4, PelletMotion::gravity, 1
	);
}

void sariel_frame_common(State &state, bool form2) noexcept
{
	auto &boss = state.boss;
	auto &sariel = boss.sariel;
	boss.phase_frame = word(static_cast<std::int32_t>(boss.phase_frame) + 1);
	sariel.hit_invincibility_frame = word(
		static_cast<std::int32_t>(sariel.hit_invincibility_frame) + 1
	);
	static_cast<void>(form2);
}

void sariel_phase_next(State &state, std::int16_t next) noexcept
{
	state.boss.phase = static_cast<std::int8_t>(next);
	state.boss.phase_frame = 0;
}

void sariel_pattern_next(
	SarielState &sariel, std::int16_t ring_min, std::int16_t ring_max
) noexcept
{
	if(ring_max == 1) {
		sariel.pattern_current = word(1 - sariel.pattern_current);
	} else {
		sariel.pattern_current = sariel.pattern_current == ring_max
			? ring_min
			: word(static_cast<std::int32_t>(sariel.pattern_current) + 1);
	}
	// b20m.cpp's phase.u1 union is intentionally reused by Form 2's pulse.
	sariel.pattern_u1 = word(static_cast<std::int32_t>(sariel.pattern_u1) + 1);
}

void sariel_phase_form1_next_if_done(State &state, std::int16_t next) noexcept
{
	auto &boss = state.boss;
	auto &sariel = boss.sariel;
	if(boss.hp <= 0) {
		boss.phase = static_cast<std::int8_t>(phase_form1_defeated);
	}
	if(sariel.pattern_u1 >= sariel.patterns_until_next &&
		!sariel.hit_invincible) {
		sariel.pattern_u1 = 0;
		boss.phase = static_cast<std::int8_t>(next);
		sariel.pattern_current = 0;
		boss.phase_frame = 0;
	}
}

[[nodiscard]] bool sariel_shield_orb_hittest(const State &state) noexcept
{
	// CBossEntity::hitbox_orb_set(-16, -16, 64, 64), hittest_orb().
	return state.orb.prev_left >= 280 && state.orb.prev_left <= 344 &&
		state.orb.prev_top >= 120 && state.orb.prev_top <= 184;
}

[[nodiscard]] bool sariel_orb_hittest(const State &state) noexcept
{
	const bool face = state.orb.horizontal.left >= face_left &&
		state.orb.horizontal.left <= face_left + (face_width - orb_width) &&
		state.orb.top >= face_top &&
		state.orb.top <= face_top + (face_height - orb_height);
	return face || sariel_shield_orb_hittest(state);
}

void sariel_hit_update(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	static_cast<void>(boss_hit_update(
		state,
		sariel.hit_invincibility_frame,
		sariel.hit_invincible,
		sariel_orb_hittest(state),
		face_left,
		face_top,
		80,
		48,
		10000
	));
}

void birds_reset(SarielState &sariel) noexcept
{
	for(auto &bird : sariel.birds) {
		bird.alive = false;
	}
}

void birds_spawn(
	SarielState &sariel,
	double left,
	double top,
	double velocity_x,
	double velocity_y
) noexcept
{
	for(auto &bird : sariel.birds) {
		if(bird.alive) {
			continue;
		}
		bird.alive = true;
		bird.left = left;
		bird.top = top;
		bird.velocity_x = velocity_x;
		bird.velocity_y = velocity_y;
		bird.hatch_time = 25;
		bird.hatch_frames = 25;
		bird.unknown = 1;
		return;
	}
}

void birds_fire(State &state, std::int16_t group) noexcept
{
	for(const auto &bird : state.boss.sariel.birds) {
		if(!bird.alive || bird.hatch_time != 0) {
			continue;
		}
		const auto left = static_cast<std::int16_t>(bird.left + 12.0);
		const auto top = static_cast<std::int16_t>(bird.top + 12.0);
		if(group == 0) {
			pellets_add_group(state, left, top, PelletGroup::one_aimed, 48);
		} else if(group == 1) {
			for(std::uint8_t angle = 0U; angle < 6U; ++angle) {
				pellets_add_single(
					state, left, top,
					static_cast<std::uint8_t>(angle * (0x100U / 6U)), 64
				);
			}
		} else {
			pellets_add_rain(
				state, left, top,
				static_cast<std::uint8_t>((pc98::irand(state.random_seed) & 0x7FU) + 0x80U),
				64
			);
		}
	}
}

void birds_update(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	for(auto &bird : sariel.birds) {
		if(!bird.alive) {
			continue;
		}
		if(bird.hatch_time > 0) {
			bird.hatch_time = static_cast<std::int8_t>(bird.hatch_time - 1);
			continue;
		}
		bird.left += bird.velocity_x;
		bird.top += bird.velocity_y;
		if(bird.left < 0.0 || bird.left > 639.0 ||
			bird.top < 0.0 || bird.top > 399.0) {
			bird.alive = false;
			continue;
		}
		if(!state.player.invincible &&
			bird.left > static_cast<double>(state.player.left - 8) &&
			bird.left < static_cast<double>(state.player.left + 8) &&
			bird.top > static_cast<double>(player_top - 16 - 8) &&
			bird.top < static_cast<double>(player_top + 16)) {
			state.input.player_is_hit = true;
			return;
		}
	}
}

void particles_vertical_update(State &state, bool from_bottom) noexcept
{
	auto &particles = state.boss.sariel.particles_vertical;
	const auto frame = state.boss.phase_frame;
	if((frame % 7) == 0) {
		for(auto &particle : particles) {
			if(particle.alive) {
				continue;
			}
			particle.left = static_cast<double>(pc98::irand(state.random_seed) % 640U);
			particle.top = from_bottom ? 398.0 : 0.0;
			const auto speed = static_cast<std::int16_t>(
				pc98::irand(state.random_seed) % 15U
			);
			particle.velocity = from_bottom
				? static_cast<double>(-speed - 8)
				: static_cast<double>(speed + 2);
			particle.alive = true;
			break;
		}
	}
	if((frame % 2) != 0) {
		return;
	}
	for(auto &particle : particles) {
		if(!particle.alive) {
			continue;
		}
		particle.top += particle.velocity;
		if(particle.top < 0.0 || particle.top > 398.0) {
			particle.alive = false;
		}
	}
}

void particles_wavy_update(State &state) noexcept
{
	auto &particles = state.boss.sariel.particles_wavy;
	const auto frame = state.boss.phase_frame;
	if((frame % 7) == 0) {
		for(auto &particle : particles) {
			if(particle.alive) {
				continue;
			}
			particle.left = static_cast<double>(pc98::irand(state.random_seed) % 640U);
			particle.top = 400.0;
			particle.velocity = -1.0;
			particle.age = 0;
			particle.alive = true;
			break;
		}
	}
	if((frame % 2) != 0) {
		return;
	}
	for(auto &particle : particles) {
		if(!particle.alive) {
			continue;
		}
		particle.top += particle.velocity;
		particle.age = word(static_cast<std::int32_t>(particle.age) + 1);
		if(particle.age >= 100) {
			particle.alive = false;
		}
	}
}

void particles_horizontal_update(State &state, std::int16_t frame) noexcept
{
	auto &particles = state.boss.sariel.particles_horizontal;
	if((frame % 7) == 0) {
		for(auto &particle : particles) {
			if(particle.alive) {
				continue;
			}
			particle.left = static_cast<double>(pc98::irand(state.random_seed) % 640U);
			particle.top = static_cast<double>(pc98::irand(state.random_seed) % 400U);
			particle.velocity = (pc98::irand(state.random_seed) % 2U) == 0U
				? -6.0
				: 6.0;
			particle.alive = true;
			break;
		}
	}
	if((frame % 2) != 0) {
		return;
	}
	for(auto &particle : particles) {
		if(!particle.alive) {
			continue;
		}
		particle.left += particle.velocity;
		particle.velocity += particle.velocity < 0.0 ? 0.1 : -0.1;
		if(particle.left >= 640.0 || particle.left < 0.0 ||
			(particle.velocity > -0.5 && particle.velocity < 0.5)) {
			particle.alive = false;
		}
	}
}

void pattern_vortices(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	const auto frame = state.boss.phase_frame;
	if(frame < 50) {
		return;
	}
	if(frame == 50) {
		sariel.vortex_pattern_end = rank_value(state, 140, 145, 150, 155);
	}
	auto fire_down = [&state, &sariel](std::size_t index) {
		pellets_add_group(
			state,
			word(static_cast<std::int32_t>(sariel.vortex_left[index]) + 12),
			word(static_cast<std::int32_t>(sariel.vortex_top[index]) + 12),
			PelletGroup::one,
			112
		);
	};
	auto fire_three = [&state, &sariel](std::size_t index) {
		const auto left = word(static_cast<std::int32_t>(sariel.vortex_left[index]) + 12);
		const auto top = word(static_cast<std::int32_t>(sariel.vortex_top[index]) + 12);
		pellets_add_single(state, left, top, 0x40U, 112);
		const auto angle = pc98::masterlib::iatan2_source(
			word(static_cast<std::int32_t>(playfield_bottom - 12) - sariel.vortex_top[index]),
			word(static_cast<std::int32_t>(sariel.vortex_left[1U - index]) -
				sariel.vortex_left[index])
		).value_or(0U);
		pellets_add_single(state, left, top, angle, 112);
		pellets_add_single(state, left, top, static_cast<std::uint8_t>(0x80U - angle), 112);
	};
	if(frame < 100) {
		for(std::size_t index = 0U; index < 2U; ++index) {
			sariel.vortex_left[index] = index == 0U ? 0 : 608;
			sariel.vortex_top[index] = 200;
			if((frame % 4) == 0) {
				fire_down(index);
			}
		}
	} else if(frame < sariel.vortex_pattern_end) {
		for(std::size_t index = 0U; index < 2U; ++index) {
			sariel.vortex_left[index] = word(static_cast<std::int32_t>(sariel.vortex_left[index]) +
				(index == 0U ? 5 : -5));
			sariel.vortex_top[index] = word(static_cast<std::int32_t>(sariel.vortex_top[index]) - 2);
			if((frame % 4) == 0) {
				fire_down(index);
				const auto from_left = word(static_cast<std::int32_t>(sariel.vortex_left[index]) + 12);
				const auto from_top = word(static_cast<std::int32_t>(sariel.vortex_top[index]) + 12);
				const auto angle = pc98::masterlib::iatan2_source(
					word(static_cast<std::int32_t>(playfield_bottom + 12) - sariel.vortex_top[index]),
					static_cast<std::int16_t>((index * 160U) - 68U)
				).value_or(0U);
				pellets_add_single(state, from_left, from_top, angle, 112);
			}
		}
	} else if(frame < 200) {
		if((frame % 4) == 0) {
			for(std::size_t index = 0U; index < 2U; ++index) {
				fire_three(index);
			}
		}
	} else if(frame < 240) {
		if(frame == 200) {
			sariel.vortex_first_direction = static_cast<std::int16_t>(
				pc98::irand(state.random_seed) % 2U
			);
		}
		for(std::size_t index = 0U; index < 2U; ++index) {
			sariel.vortex_left[index] = word(static_cast<std::int32_t>(sariel.vortex_left[index]) +
				(2 - (sariel.vortex_first_direction * 4)));
			if((frame % 4) == 0) {
				fire_three(index);
			}
		}
	} else if(frame < 320) {
		if(frame == 240) {
			sariel.vortex_second_direction = static_cast<std::int16_t>(
				pc98::irand(state.random_seed) % 2U
			);
		}
		for(std::size_t index = 0U; index < 2U; ++index) {
			const auto delta = sariel.vortex_second_direction != sariel.vortex_first_direction
				? -(2 - (sariel.vortex_second_direction * 4))
				: -(4 - (sariel.vortex_second_direction * 8));
			sariel.vortex_left[index] = word(static_cast<std::int32_t>(sariel.vortex_left[index]) + delta);
			if((frame % 4) == 0) {
				fire_three(index);
			}
		}
	} else if(frame > 300) {
		state.boss.phase_frame = 0;
		sariel.vortex_wand_raised = false;
	}
}

void pattern_random_purple_lasers(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	const auto frame = state.boss.phase_frame;
	if(frame < 50) {
		return;
	}
	if(frame == 50) {
		sariel.purple_laser_speed = rank_value(state, 68, 72, 76, 80);
		for(std::size_t index = 0U; index < 10U; ++index) {
			sariel.purple_laser_left[index] = random_x_range(state, 120, 400);
			sariel.purple_laser_top[index] = random_y_range(state, 36, 100);
		}
	}
	if(frame < 150) {
		if((frame % 10) == 9) {
			const auto index = static_cast<std::size_t>((frame - 50) / 10);
			shootout_laser_spawn(
				state,
				index,
				sariel.purple_laser_left[index],
				sariel.purple_laser_top[index],
				random_x(state),
				playfield_bottom,
				sariel.purple_laser_speed,
				4U,
				25,
				5
			);
		}
	} else if(frame > 200) {
		state.boss.phase_frame = 0;
	}
}

void pattern_birds_on_ellipse_arc(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	const auto frame = state.boss.phase_frame;
	if(frame < 40) {
		return;
	}
	if(frame < 50) {
		return;
	}
	if(frame == 50) {
		sariel.eggs_alive = 0;
		sariel.bird_spawner_left_q12_4 = q12_4_pixels(wand_emit_left);
		sariel.bird_spawner_top_q12_4 = q12_4_pixels(wand_emit_top);
		sariel.bird_spawner_velocity_x_q12_4 = q12_4_pixels(
			(pc98::irand(state.random_seed) % 2U) == 0U ? 4 : -4
		);
		sariel.bird_spawner_velocity_y_q12_4 = q12_4_pixels(
			(pc98::irand(state.random_seed) % 2U) == 0U ? 2 : -2
		);
		sariel.bird_interval = rank_value(state, 20, 15, 10, 8);
		return;
	}
	if(frame < 200) {
		if(sariel.bird_spawner_left_q12_4 < q12_4_pixels(shield_center_x)) {
			sariel.bird_spawner_velocity_x_q12_4 = word(
				static_cast<std::int32_t>(sariel.bird_spawner_velocity_x_q12_4) + 1
			);
		} else {
			sariel.bird_spawner_velocity_x_q12_4 = word(
				static_cast<std::int32_t>(sariel.bird_spawner_velocity_x_q12_4) - 1
			);
		}
		if(sariel.bird_spawner_top_q12_4 < q12_4_pixels(shield_center_y)) {
			sariel.bird_spawner_velocity_y_q12_4 = word(
				static_cast<std::int32_t>(sariel.bird_spawner_velocity_y_q12_4) + 1
			);
		} else {
			sariel.bird_spawner_velocity_y_q12_4 = word(
				static_cast<std::int32_t>(sariel.bird_spawner_velocity_y_q12_4) - 1
			);
		}
		sariel.bird_spawner_left_q12_4 = word(static_cast<std::int32_t>(
			sariel.bird_spawner_left_q12_4) + sariel.bird_spawner_velocity_x_q12_4);
		sariel.bird_spawner_top_q12_4 = word(static_cast<std::int32_t>(
			sariel.bird_spawner_top_q12_4) + sariel.bird_spawner_velocity_y_q12_4);
		if((frame % sariel.bird_interval) == 0 &&
			sariel.eggs_alive < static_cast<std::int16_t>(sariel.eggs.size())) {
			const auto left = q12_4_pixel(sariel.bird_spawner_left_q12_4);
			const auto top = q12_4_pixel(sariel.bird_spawner_top_q12_4);
			if(sariel.bird_spawner_left_q12_4 >= 0 &&
				sariel.bird_spawner_left_q12_4 <= q12_4_pixels(608) &&
				sariel.bird_spawner_top_q12_4 >= 0 &&
				sariel.bird_spawner_top_q12_4 <= q12_4_pixels(368)) {
				sariel.eggs[static_cast<std::size_t>(sariel.eggs_alive)] = {left, top};
				sariel.eggs_alive = word(static_cast<std::int32_t>(sariel.eggs_alive) + 1);
			}
		}
		return;
	}
	if(frame < 400) {
		if(frame == 200) {
			if(sariel.bird_spawner_velocity_y_q12_4 > -4 &&
				sariel.bird_spawner_velocity_y_q12_4 < 4) {
				sariel.bird_spawner_velocity_y_q12_4 =
					sariel.bird_spawner_velocity_y_q12_4 < 0 ? -4 : 4;
			}
			if(sariel.bird_spawner_velocity_x_q12_4 > -4 &&
				sariel.bird_spawner_velocity_x_q12_4 < 4) {
				sariel.bird_spawner_velocity_x_q12_4 =
					sariel.bird_spawner_velocity_x_q12_4 < 0 ? -4 : 4;
			}
			for(std::int16_t index = word(static_cast<std::int32_t>(sariel.eggs_alive) - 1);
				index >= 0;
				--index) {
				const auto &egg = sariel.eggs[static_cast<std::size_t>(index)];
				const auto velocity = pc98::masterlib::vector2(
					static_cast<std::uint8_t>((pc98::irand(state.random_seed) & 0x7FU) + 0x80U), 3
				);
				birds_spawn(sariel, egg.left, egg.top, velocity.x, velocity.y);
			}
			sariel.bird_pellet_group = static_cast<std::int16_t>(
				pc98::irand(state.random_seed) % 2U
			);
		}
		if(((frame % 16) == 0) || ((frame % 16) == 2)) {
			birds_fire(state, sariel.bird_pellet_group);
		}
		return;
	}
	if(frame == 400) {
		state.boss.phase_frame = 0;
		sariel.bird_wand_raised = false;
	}
}

} // namespace

void sariel_setup(State &state) noexcept
{
	auto &boss = state.boss;
	boss.id = BossId::sariel;
	boss.hp = 18;
	boss.phase = 0;
	boss.phase_frame = 0;
	boss.active = true;
	state.projectiles.pellet_interlace = true;
}

namespace {

void pattern_detonating_snowflake(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	const auto frame = state.boss.phase_frame;
	if(frame < 10) {
		sariel.snowflake_phase = snowflake_reset;
		sariel.snowflake_start_frame = 160;
	}
	if((frame % sariel.snowflake_start_frame) == 0) {
		sariel.snowflake_phase = snowflake_wand_raise;
		sariel.snowflake_wand_frame = 0;
	}
	if(sariel.snowflake_phase == snowflake_wand_raise) {
		sariel.snowflake_wand_frame = word(
			static_cast<std::int32_t>(sariel.snowflake_wand_frame) + 1
		);
		if(sariel.snowflake_wand_frame >= 16) {
			sariel.snowflake_phase = snowflake_spawn;
		}
	}
	if(sariel.snowflake_phase == snowflake_spawn) {
		sariel.snowflake_left_q12_4 = q12_4_pixels(wand_emit_left);
		sariel.snowflake_top_q12_4 = q12_4_pixels(wand_emit_top);
		const auto velocity = vector_between(
			wand_emit_left,
			wand_emit_top,
			word(static_cast<std::int32_t>(state.player.left) + 12),
			player_top + 12,
			112
		);
		sariel.snowflake_velocity_x_q12_4 = velocity.x;
		sariel.snowflake_velocity_y_q12_4 = velocity.y;
		sariel.snowflake_phase = snowflake_moving;
	}
	if(sariel.snowflake_phase == snowflake_moving) {
		sariel.snowflake_top_q12_4 = word(static_cast<std::int32_t>(
			sariel.snowflake_top_q12_4) + sariel.snowflake_velocity_y_q12_4);
		if(sariel.snowflake_top_q12_4 >= q12_4_pixels(396)) {
			sariel.snowflake_phase = snowflake_detonation_start;
			return;
		}
		sariel.snowflake_left_q12_4 = word(static_cast<std::int32_t>(
			sariel.snowflake_left_q12_4) + sariel.snowflake_velocity_x_q12_4);
	}
	if(sariel.snowflake_phase == snowflake_detonation_start) {
		sariel.snowflake_outer_radius_1 = 48;
		sariel.snowflake_outer_radius_2 = 16;
		sariel.snowflake_inner_radius = 24;
		// Literal b20m.cpp narrowing of the Q12.4 value into screen_x_t.
		sariel.snowflake_star_left = sariel.snowflake_left_q12_4;
		sariel.snowflake_star_top = word(static_cast<std::int32_t>(playfield_bottom) -
			static_cast<std::int16_t>(pc98::irand(state.random_seed) % 48U));
	}
	if(sariel.snowflake_phase >= snowflake_detonation_start) {
		sariel.snowflake_phase = word(static_cast<std::int32_t>(
			sariel.snowflake_phase) + 1);
	}
	if(sariel.snowflake_phase >= snowflake_detonation_active &&
		(sariel.snowflake_phase % 8) == 0) {
		sariel.snowflake_outer_radius_1 = word(static_cast<std::int32_t>(
			sariel.snowflake_outer_radius_1) - 6);
		sariel.snowflake_outer_radius_2 = word(static_cast<std::int32_t>(
			sariel.snowflake_outer_radius_2) + 6);
		sariel.snowflake_inner_radius = word(static_cast<std::int32_t>(
			sariel.snowflake_inner_radius) + 4);
		const auto left = q12_4_pixel(sariel.snowflake_left_q12_4);
		sariel.snowflake_star_left = word(static_cast<std::int32_t>(left) +
			static_cast<std::int16_t>(pc98::irand(state.random_seed) % 64U) - 28);
		sariel.snowflake_star_top = word(static_cast<std::int32_t>(playfield_bottom) -
			static_cast<std::int16_t>(pc98::irand(state.random_seed) % 48U));
		if(sariel.snowflake_outer_radius_1 <= 8) {
			sariel.snowflake_phase = snowflake_reset;
			return;
		}
	}
	const auto snowflake_left = q12_4_pixel(sariel.snowflake_left_q12_4);
	if(!state.player.invincible &&
		snowflake_left + 32 > state.player.left &&
		snowflake_left - 64 < state.player.left &&
		sariel.snowflake_phase >= snowflake_detonation_start) {
		state.input.player_is_hit = true;
	}
}

void pattern_two_rings_from_a2_orbs(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	const auto frame = state.boss.phase_frame;
	if(frame < 5) {
		sariel.rings_angle = 0U;
		sariel.rings_interval = rank_value(state, 18, 12, 10, 8);
	}
	if((frame % sariel.rings_interval) == 0) {
		state.projectiles.pellet_spawn_with_cloud = true;
		const auto angle = sariel.rings_angle;
		pellets_add_single(state, a2_orb_right, a2_orb_top,
			static_cast<std::uint8_t>(0U - angle), 56);
		pellets_add_single(state, a2_orb_right, a2_orb_top,
			static_cast<std::uint8_t>(0x80U - angle), 56);
		pellets_add_single(state, a2_orb_left, a2_orb_top, angle, 56);
		pellets_add_single(state, a2_orb_left, a2_orb_top,
			static_cast<std::uint8_t>(0x80U + angle), 56);
		sariel.rings_angle = static_cast<std::uint8_t>(
			sariel.rings_angle + static_cast<std::uint8_t>(sariel.rings_interval)
		);
	}
	if(frame > 479) {
		state.boss.phase_frame = 0;
	}
}

void pattern_aimed_sling_clusters(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	const auto frame = state.boss.phase_frame;
	if(frame == 10) {
		sariel.sling_interval = rank_value(state, 4, 3, 2, 2);
	}
	if((frame % 200) >= 150 && (frame % sariel.sling_interval) == 0) {
		const auto angle = static_cast<std::uint8_t>(pc98::irand(state.random_seed));
		const auto left = polar_y(playfield_center_x, 40, angle);
		const auto top = polar_x(180, 40, angle);
		pellets_add_single(
			state, left, top, 0U, 0, PelletMotion::sling_aimed, 72
		);
	}
	if(frame >= 299) {
		state.boss.phase_frame = 0;
	}
}

void pattern_four_aimed_lasers(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	const auto frame = state.boss.phase_frame;
	const auto spawn = [&state, &sariel](std::size_t index,
		std::int16_t origin_left, std::int16_t origin_top, std::int16_t target_left) {
		shootout_laser_spawn(
			state, index, origin_left, origin_top, target_left, playfield_bottom,
			sariel.four_laser_speed, 15U, 50, 8
		);
	};
	const auto init_ray = [&sariel](std::int16_t origin_left,
		std::int16_t origin_top, std::int16_t target_left) {
		sariel.four_laser_origin_left = origin_left;
		sariel.four_laser_origin_top = origin_top;
		const auto velocity = vector_between(
			origin_left, origin_top, target_left, playfield_bottom, 16
		);
		sariel.four_laser_velocity_left = velocity.x;
		sariel.four_laser_velocity_top = velocity.y;
	};
	if(frame < 50) {
		return;
	}
	if(frame == 50) {
		init_ray(460, face_center_y, state.player.left);
		sariel.four_laser_target_first = state.player.left;
		sariel.four_laser_speed = rank_value(state, 60, 64, 68, 72);
		spawn(0U, 460, face_center_y, state.player.left);
	} else if(frame < 100) {
		sariel.four_laser_origin_left = word(static_cast<std::int32_t>(
			sariel.four_laser_origin_left) + sariel.four_laser_velocity_left);
		sariel.four_laser_origin_top = word(static_cast<std::int32_t>(
			sariel.four_laser_origin_top) + sariel.four_laser_velocity_top);
	} else if(frame == 100) {
		const auto target = word(639 - sariel.four_laser_target_first);
		init_ray(180, face_center_y, target);
		spawn(1U, sariel.four_laser_origin_left, sariel.four_laser_origin_top, target);
	} else if(frame < 150) {
		sariel.four_laser_origin_left = word(static_cast<std::int32_t>(
			sariel.four_laser_origin_left) + sariel.four_laser_velocity_left);
		sariel.four_laser_origin_top = word(static_cast<std::int32_t>(
			sariel.four_laser_origin_top) + sariel.four_laser_velocity_top);
	} else if(frame == 150) {
		init_ray(608, shield_center_y, state.player.left);
		spawn(2U, sariel.four_laser_origin_left, sariel.four_laser_origin_top, state.player.left);
	} else if(frame < 200) {
		sariel.four_laser_origin_left = word(static_cast<std::int32_t>(
			sariel.four_laser_origin_left) + sariel.four_laser_velocity_left);
		sariel.four_laser_origin_top = word(static_cast<std::int32_t>(
			sariel.four_laser_origin_top) + sariel.four_laser_velocity_top);
	} else if(frame == 200) {
		init_ray(24, shield_center_y, state.player.left);
		spawn(3U, sariel.four_laser_origin_left, sariel.four_laser_origin_top, state.player.left);
	} else if(frame > 300) {
		state.boss.phase_frame = 0;
	}
}

void pattern_rain_from_top(State &state) noexcept
{
	if((state.boss.phase_frame % 8) != 0) {
		return;
	}
	const auto group = state.rank == 3
		? PelletGroup::one_random_wide
		: PelletGroup::one;
	pellets_add_group(state, random_x(state), playfield_top, group, 44);
}

void pattern_radial_stacks_and_lasers(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	const auto frame = state.boss.phase_frame;
	if(frame < 150) {
		return;
	}
	if(frame == 150) {
		sariel.radial_angle = 0U;
		sariel.radial_angle_velocity = 1U;
		sariel.radial_laser_speed = rank_value(state, 60, 65, 70, 75);
	} else if(frame < 200) {
		sariel.radial_angle = static_cast<std::uint8_t>(
			sariel.radial_angle + sariel.radial_angle_velocity
		);
		sariel.radial_angle_velocity = static_cast<std::uint8_t>(
			sariel.radial_angle_velocity + 1U
		);
	} else if(frame == 200) {
		sariel.radial_angle = 0U;
	} else if(frame <= 400) {
		if((frame % 5) == 0) {
			const auto target_left = polar_x(shield_center_x, 600, sariel.radial_angle);
			const auto target_top = polar_y(shield_center_y - 4, 600, sariel.radial_angle);
			if((frame % 15) == 0) {
				shootout_laser_spawn_safe(
					state,
					static_cast<std::size_t>((frame - 215) / 15),
					shield_center_x,
					shield_center_y - 4,
					target_left,
					target_top,
					sariel.radial_laser_speed,
					15U,
					20,
					4
				);
			} else {
				pellets_add_single(state, shield_center_x, shield_center_y - 4,
					sariel.radial_angle, 100);
			}
			pellets_add_single(state, shield_center_x, shield_center_y - 4,
				static_cast<std::uint8_t>(sariel.radial_angle - 0x04U), 96);
			pellets_add_single(state, shield_center_x, shield_center_y - 4,
				static_cast<std::uint8_t>(sariel.radial_angle + 0x04U), 96);
			if(state.rank >= 2) {
				pellets_add_single(state, shield_center_x, shield_center_y - 4,
					static_cast<std::uint8_t>(sariel.radial_angle - 0x04U), 64);
				pellets_add_single(state, shield_center_x, shield_center_y - 4,
					static_cast<std::uint8_t>(sariel.radial_angle + 0x04U), 64);
				if(state.rank == 3) {
					pellets_add_single(state, shield_center_x, shield_center_y - 4,
						sariel.radial_angle, 64);
					pellets_add_single(state, shield_center_x, shield_center_y - 4,
						sariel.radial_angle, 48);
				}
			}
			sariel.radial_angle = static_cast<std::uint8_t>(sariel.radial_angle + 0x0CU);
		}
	} else if(frame == 450) {
		state.boss.phase_frame = 0;
	}
}

void pattern_symmetric_birds_from_bottom(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	const auto frame = state.boss.phase_frame;
	if(frame < 50) {
		return;
	}
	if(frame == 50) {
		sariel.symmetric_interval = rank_value(state, 6, 7, 8, 9);
		sariel.symmetric_ray_left = 2;
		sariel.symmetric_ray_top = 399;
		return;
	}
	if(frame < 100) {
		sariel.symmetric_ray_left = word(static_cast<std::int32_t>(
			sariel.symmetric_ray_left) + 2);
		if((frame % 10) == 0) {
			const auto target = word(static_cast<std::int32_t>(frame - 60) * 10 +
				sariel.symmetric_ray_left);
			const auto velocity = vector_between(
				sariel.symmetric_ray_left,
				sariel.symmetric_ray_top - 32,
				target,
				0,
				4
			);
			birds_spawn(sariel, sariel.symmetric_ray_left,
				sariel.symmetric_ray_top - 32, velocity.x, velocity.y);
			birds_spawn(sariel, word(639 - sariel.symmetric_ray_left),
				sariel.symmetric_ray_top - 32, -velocity.x, velocity.y);
		}
		if((frame % 6) == 0) {
			birds_fire(state, 2);
		}
		return;
	}
	if((frame % 10) == 0) {
		birds_fire(state, 2);
	}
	if(frame >= 300) {
		state.boss.phase_frame = 0;
	}
}

void pattern_four_semicircle_spreads(State &state) noexcept
{
	const auto frame = state.boss.phase_frame;
	if(frame == 50) {
		std::uint8_t angle = 0U;
		for(std::int16_t index = 0; index < 20; ++index) {
			pellets_add_single(state, shield_center_x, shield_center_y - 4, angle, 32);
			angle = static_cast<std::uint8_t>(angle + 0x07U);
		}
	} else if(frame == 60) {
		std::uint8_t angle = 0x04U;
		for(std::int16_t index = 0; index < 10; ++index) {
			pellets_add_single(state, shield_center_x, shield_center_y - 4, angle, 48);
			angle = static_cast<std::uint8_t>(angle + 0x0CU);
		}
	} else if(frame == 70) {
		std::uint8_t angle = 0x04U;
		for(std::int16_t index = 0; index < 8; ++index) {
			pellets_add_single(state, shield_center_x, shield_center_y - 4, angle, 64);
			angle = static_cast<std::uint8_t>(angle + 0x0EU);
		}
	} else if(frame == 80) {
		std::uint8_t angle = 0x0EU;
		for(std::int16_t index = 0; index < 10; ++index) {
			pellets_add_single(state, shield_center_x, shield_center_y - 4, angle, 48);
			angle = static_cast<std::uint8_t>(angle + 0x0AU);
		}
	} else if(frame > 150) {
		state.boss.phase_frame = 0;
	}
}

void pattern_vertical_stacks_from_bottom_then_rain(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	const auto frame = state.boss.phase_frame;
	if(frame < 50) {
		return;
	}
	if(frame == 50) {
		sariel.vertical_interval = rank_value(state, 16, 13, 10, 8);
		sariel.vertical_ray_left = 2;
		sariel.vertical_ray_top = 399;
	} else if(frame < 130) {
		sariel.vertical_ray_left = word(static_cast<std::int32_t>(sariel.vertical_ray_left) + 8);
	} else if(frame == 130) {
		sariel.vertical_ray_left = playfield_left;
	} else if(frame > 150 && frame < 350) {
		if((frame % 10) == 0) {
			for(std::int16_t speed = 0; speed < 64; speed += 16) {
				if(std::abs((state.player.left + 16) - sariel.vertical_ray_left) > player_width) {
					pellets_add_single(state, sariel.vertical_ray_left,
						sariel.vertical_ray_top - pellet_height, 0xC0U,
						word(static_cast<std::int32_t>(speed) + 48));
				}
				const auto right = word(639 - sariel.vertical_ray_left);
				if(std::abs((state.player.left + 16) - right) > player_width) {
					pellets_add_single(state, right,
						sariel.vertical_ray_top - pellet_height, 0xC0U,
						word(static_cast<std::int32_t>(speed) + 48));
				}
			}
			sariel.vertical_ray_left = word(static_cast<std::int32_t>(sariel.vertical_ray_left) + 32);
		}
	} else if(frame == 350) {
		sariel.vertical_ray_left = playfield_left;
		sariel.vertical_ray_top = playfield_top;
	} else if(frame < 550) {
		if((frame % sariel.vertical_interval) == 0) {
			for(std::int16_t index = 0; index < 2; ++index) {
				const auto angle_left = angle_between(sariel.vertical_ray_left,
					sariel.vertical_ray_top, random_x(state), playfield_bottom);
				pellets_add_rain(state, sariel.vertical_ray_left, sariel.vertical_ray_top,
					angle_left, 48);
				const auto right = word(639 - sariel.vertical_ray_left);
				// fire_random_to_bottom() computes both directions against
				// target_l_x, then fires the right pellet from target_x(X_RIGHT).
				const auto angle_right = pc98::masterlib::iatan2_source(
					word(static_cast<std::int32_t>(playfield_bottom) - sariel.vertical_ray_top),
					word(static_cast<std::int32_t>(random_x(state)) - sariel.vertical_ray_left)
				).value_or(0U);
				pellets_add_rain(state, right, sariel.vertical_ray_top, angle_right, 48);
			}
			sariel.vertical_ray_left = word(static_cast<std::int32_t>(sariel.vertical_ray_left) + 32);
		}
	} else if(frame > 600) {
		state.boss.phase_frame = 0;
	}
}

void leaves_update(State &state, std::int16_t interval_or_reset) noexcept
{
	auto &sariel = state.boss.sariel;
	const auto frame = state.boss.phase_frame;
	if(interval_or_reset == 999) {
		for(auto &leaf_state : sariel.leaves) {
			leaf_state.flag = leaf_free;
		}
		return;
	}
	if((frame % interval_or_reset) == 0) {
		for(auto &leaf_state : sariel.leaves) {
			if(leaf_state.flag != leaf_free) {
				continue;
			}
			leaf_state.left_tenths = static_cast<std::int16_t>(
				pc98::irand(state.random_seed) % 6400U
			);
			leaf_state.top_tenths = word(static_cast<std::int32_t>(face_center_y * 10) +
				static_cast<std::int16_t>(pc98::irand(state.random_seed) % 1000U));
			const auto velocity = vector_between(
				static_cast<std::int16_t>(leaf_state.left_tenths / 10),
				static_cast<std::int16_t>(leaf_state.top_tenths / 10),
				random_x(state),
				playfield_top,
				40
			);
			leaf_state.velocity_x_tenths = velocity.x;
			leaf_state.velocity_y_tenths = velocity.y;
			leaf_state.flag = leaf_spark;
			break;
		}
	}
	if((frame % 2) != 0) {
		return;
	}
	for(auto &leaf_state : sariel.leaves) {
		if(leaf_state.flag == leaf_free) {
			continue;
		}
		if(leaf_state.flag == leaf_spark) {
			leaf_state.left_tenths = word(static_cast<std::int32_t>(
				leaf_state.left_tenths) + leaf_state.velocity_x_tenths);
			leaf_state.top_tenths = word(static_cast<std::int32_t>(
				leaf_state.top_tenths) + leaf_state.velocity_y_tenths);
			if(leaf_state.top_tenths < playfield_top * 10) {
				leaf_state.flag = leaf_splash;
			}
		} else if(leaf_state.flag <= leaf_splash_done) {
			if(leaf_state.flag == leaf_splash_done) {
				leaf_state.flag = leaf;
				leaf_state.velocity_y_tenths = word(3 +
					static_cast<std::int16_t>(pc98::irand(state.random_seed) % 2U));
				leaf_state.velocity_x_tenths = 2;
			}
		} else if(leaf_state.flag == leaf) {
			leaf_state.left_tenths = word(static_cast<std::int32_t>(
				leaf_state.left_tenths) + leaf_state.velocity_x_tenths);
			leaf_state.top_tenths = word(static_cast<std::int32_t>(
				leaf_state.top_tenths) + leaf_state.velocity_y_tenths);
			leaf_state.velocity_y_tenths = word(static_cast<std::int32_t>(
				leaf_state.velocity_y_tenths) - 1);
			if(leaf_state.velocity_y_tenths < -1) {
				leaf_state.velocity_y_tenths = word(30 +
					static_cast<std::int16_t>(pc98::irand(state.random_seed) % 20U));
				leaf_state.velocity_x_tenths = leaf_state.velocity_x_tenths < 0
					? 20
					: -20;
			}
			if((leaf_state.top_tenths / 10) > playfield_bottom) {
				leaf_state.flag = leaf_free;
			}
		}
	}
	for(auto &leaf_state : sariel.leaves) {
		if(leaf_state.flag == leaf_free) {
			continue;
		}
		if(leaf_state.flag >= leaf_splash && leaf_state.flag <= leaf_splash_done &&
			(frame % 4) == 0) {
			leaf_state.flag = word(static_cast<std::int32_t>(leaf_state.flag) + 1);
		}
		if(leaf_state.flag == leaf && !state.player.invincible &&
			leaf_state.top_tenths > player_top * 10 &&
			leaf_state.top_tenths < (playfield_bottom - 8) * 10 &&
			(leaf_state.left_tenths / 10) > state.player.left &&
			(leaf_state.left_tenths / 10) < state.player.left + 24) {
			state.input.player_is_hit = true;
		}
	}
}

void curved_spray_init(State &state, SarielCurvedSprayState &spray) noexcept
{
	spray.target_left = playfield_left;
	spray.target_top = playfield_bottom;
	spray.speed = 112;
	spray.subpattern_id = 0;
	state.boss.sariel.pattern_state = rank_value(state, 32, 24, 18, 16);
}

void curved_spray_fire(
	State &state, SarielCurvedSprayState &spray, bool right
) noexcept
{
	spray.angle = angle_between(
		seal_center_x, seal_center_y, spray.target_left, spray.target_top
	);
	pellets_add_single(
		state, seal_center_x, seal_center_y, spray.angle, spray.speed
	);
	spray.speed = word(static_cast<std::int32_t>(spray.speed) - 4);
	spray.target_left = word(static_cast<std::int32_t>(spray.target_left) +
		(right ? state.boss.sariel.pattern_state : -state.boss.sariel.pattern_state));
}

void pattern_curved_spray_once(State &state) noexcept
{
	auto &spray = state.boss.sariel.curved_once;
	auto &frame = state.boss.phase_frame;
	if(frame < 50) {
		return;
	}
	if(frame < 100) {
		return;
	}
	if(frame == 100) {
		curved_spray_init(state, spray);
	}
	if(spray.subpattern_id == 0) {
		curved_spray_fire(state, spray, true);
		if(spray.target_left >= playfield_right) {
			spray.subpattern_id = 1;
			spray.speed = 112;
			spray.target_left = playfield_right;
		}
	} else {
		curved_spray_fire(state, spray, false);
		if(spray.target_left < playfield_left) {
			frame = 0;
		}
	}
}

void pattern_rain_from_seal_center(State &state) noexcept
{
	auto &sariel = state.boss.sariel;
	auto &frame = state.boss.phase_frame;
	if(frame < 50) {
		return;
	}
	if(frame == 50) {
		sariel.seal_rain_target_left = playfield_left;
		sariel.seal_rain_target_top = playfield_bottom - 1;
		sariel.seal_rain_pellet_count = rank_value(state, 30, 35, 40, 45);
	}
	if(sariel.seal_rain_target_top > seal_center_y) {
		if((frame % 2) == 1) {
			sariel.seal_rain_target_top = word(static_cast<std::int32_t>(
				sariel.seal_rain_target_top) - 8);
		}
		return;
	}
	for(std::int16_t index = 0; index < sariel.seal_rain_pellet_count; ++index) {
		const auto left = static_cast<std::int16_t>(
			(playfield_width / sariel.seal_rain_pellet_count) * index
		);
		pellets_add_rain(
			state,
			left,
			seal_center_y,
			static_cast<std::uint8_t>(pc98::irand(state.random_seed)),
			16
		);
	}
	frame = 0;
}

void pattern_curved_spray_twice(State &state) noexcept
{
	auto &spray = state.boss.sariel.curved_twice;
	auto &frame = state.boss.phase_frame;
	if(frame < 80) {
		return;
	}
	if(frame == 80) {
		curved_spray_init(state, spray);
	}
	if((spray.subpattern_id % 2) == 0) {
		curved_spray_fire(state, spray, true);
		if(spray.target_left >= playfield_right) {
			spray.subpattern_id = word(static_cast<std::int32_t>(spray.subpattern_id) + 1);
			spray.speed = 112;
			spray.target_left = playfield_right;
		}
	} else {
		curved_spray_fire(state, spray, false);
		if(spray.target_left < playfield_left) {
			spray.subpattern_id = word(static_cast<std::int32_t>(spray.subpattern_id) + 1);
			spray.target_left = playfield_left;
			spray.speed = 112;
			if(spray.subpattern_id == 4) {
				frame = 0;
			}
		}
	}
}

void sariel_bg_transition(State &state) noexcept
{
	auto &boss = state.boss;
	auto &sariel = boss.sariel;
	if(boss.phase_frame == 20) {
		sariel.bg_gust_id = 0;
	}
	if(boss.phase_frame < 50) {
		return;
	}
	// b20m.cpp performs 100 cell draws per gust frame. The graphics writes are
	// presentation-owned, but these IRAND calls are gameplay-visible.
	for(std::int16_t row = 0; row < 100; ++row) {
		static_cast<void>(pc98::irand(state.random_seed));
	}
	if((boss.phase_frame - 50 + 1) == 20) {
		sariel.bg_gust_id = word(static_cast<std::int32_t>(sariel.bg_gust_id) + 1);
		boss.phase_frame = 49;
		if(sariel.bg_gust_id >= 3) {
			boss.phase_frame = 0;
		}
	}
}

void sariel_frame_bg_transition(State &state) noexcept
{
	state.boss.phase_frame = word(static_cast<std::int32_t>(
		state.boss.phase_frame) + 1);
	sariel_bg_transition(state);
}

void sariel_form1_defeated(State &state) noexcept
{
	auto &boss = state.boss;
	auto &sariel = boss.sariel;
	boss.phase_frame = 0;
	sariel.hit_invincibility_frame = 399;
	state.shots = Shots{};
	pellets_unput_and_reset_nonclouds(state);
	shootout_lasers_reset_first(state, 10U);
	state.orb.horizontal.left = 600;
	state.orb.top = 280;
	state.player.left = 304;
	state.orb.vertical.force = -8.0;
	state.orb.vertical.force_frame = 0;
	state.orb.horizontal.velocity = OrbVelocityX::left4;
	state.player.deflecting = false;
	state.bomb_damaging = false;
	player_reset(state);
	sariel.hit_invincible = false;
	state.random_seed = state.frame_rand;

	// The 201-frame form-change loop has no gameplay work between these source
	// endpoints. It is intentionally collapsed without manufacturing ticks.
	boss.phase_frame = 0;
	sariel.hit_invincibility_frame = 0;
	sariel.pattern_current = 0;
	sariel.pattern_u1 = 0;
	boss.phase = static_cast<std::int8_t>(phase_form2);
	state.player.invincibility_time = 0;
	state.player.invincible = false;
	boss.hp = 6;
	sariel.initial_hp_rendered = false;
	leaves_update(state, 999);
}

} // namespace

[[nodiscard]] BossUpdateResult sariel_update(State &state) noexcept
{
	auto &boss = state.boss;
	auto &sariel = boss.sariel;
	if(!boss.active || boss.id != BossId::sariel) {
		return BossUpdateResult::unsupported;
	}
	if(boss.phase == 0) {
		boss.phase_frame = -1;
		sariel.hit_invincibility_frame = 0;
		sariel.hit_invincible = false;
		state.random_seed = state.frame_rand;
		// b20m.cpp's entrance rings only invert graphics and delay. Its exact
		// combat endpoint is the first RNG-consuming phase-1 setup below.
		boss.phase = 1;
		sariel.pattern_current = 0;
		sariel.pattern_u1 = 0;
		sariel.patterns_until_next = word(
			static_cast<std::int16_t>(pc98::irand(state.random_seed) % 6U) + 1
		);
		boss.phase_frame = 0;
		sariel.initial_hp_rendered = false;
		birds_reset(sariel);
		return BossUpdateResult::running;
	}
	if(boss.phase == 1) {
		sariel_frame_common(state, false);
		birds_update(state);
		if(sariel.pattern_current == 0) {
			pattern_random_purple_lasers(state);
		} else if(sariel.pattern_current == 1) {
			pattern_vortices(state);
		} else if(sariel.pattern_current == 2) {
			pattern_birds_on_ellipse_arc(state);
		}
		if(boss.phase_frame == 0) {
			sariel_pattern_next(sariel, 0, 2);
		}
		sariel_hit_update(state);
		sariel_phase_form1_next_if_done(state, 2);
		return BossUpdateResult::running;
	}
	if(boss.phase == 2) {
		sariel_frame_bg_transition(state);
		if(boss.phase_frame == 0) {
			sariel_phase_next(state, 3);
			sariel.patterns_until_next = word(
				static_cast<std::int16_t>(pc98::irand(state.random_seed) % 5U) + 1
			);
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 3) {
		sariel_frame_common(state, false);
		particles_vertical_update(state, false);
		if(sariel.pattern_current == 0) {
			pattern_detonating_snowflake(state);
			pattern_two_rings_from_a2_orbs(state);
		} else if(sariel.pattern_current == 1) {
			pattern_aimed_sling_clusters(state);
			pattern_two_rings_from_a2_orbs(state);
		} else if(boss.phase_frame < 60) {
			boss.phase_frame = 0;
		}
		if(boss.phase_frame == 0) {
			sariel_pattern_next(sariel, 0, 1);
		}
		sariel_hit_update(state);
		sariel_phase_form1_next_if_done(state, 4);
		return BossUpdateResult::running;
	}
	if(boss.phase == 4) {
		sariel_frame_bg_transition(state);
		if(boss.phase_frame == 0) {
			sariel_phase_next(state, 5);
			sariel.patterns_until_next = word(
				static_cast<std::int16_t>(pc98::irand(state.random_seed) % 4U) + 3
			);
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 5) {
		sariel_frame_common(state, false);
		particles_wavy_update(state);
		switch(sariel.pattern_current) {
		case 0: pattern_four_aimed_lasers(state); break;
		case 1: if(boss.phase_frame == 50) boss.phase_frame = 0; break;
		case 2:
			pattern_four_aimed_lasers(state);
			pattern_rain_from_top(state);
			break;
		case 3: pattern_radial_stacks_and_lasers(state); break;
		case 4: if(boss.phase_frame == 50) boss.phase_frame = 0; break;
		case 5:
			pattern_four_aimed_lasers(state);
			pattern_rain_from_top(state);
			break;
		default: break;
		}
		if(boss.phase_frame == 0) {
			sariel_pattern_next(sariel, 10, 5);
		}
		sariel_hit_update(state);
		sariel_phase_form1_next_if_done(state, 6);
		return BossUpdateResult::running;
	}
	if(boss.phase == 6) {
		sariel_frame_bg_transition(state);
		if(boss.phase_frame == 0) {
			sariel_phase_next(state, 7);
			sariel.patterns_until_next = word(
				static_cast<std::int16_t>(pc98::irand(state.random_seed) % 5U) + 2
			);
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 7) {
		sariel_frame_common(state, false);
		particles_vertical_update(state, true);
		birds_update(state);
		if(sariel.pattern_current == 0) {
			pattern_symmetric_birds_from_bottom(state);
		} else if(sariel.pattern_current == 1) {
			pattern_four_semicircle_spreads(state);
		} else if(sariel.pattern_current == 2) {
			pattern_vertical_stacks_from_bottom_then_rain(state);
		}
		if(boss.phase_frame == 0) {
			sariel_pattern_next(sariel, 0, 2);
		}
		sariel_hit_update(state);
		sariel_phase_form1_next_if_done(state, 8);
		return BossUpdateResult::running;
	}
	if(boss.phase == 8) {
		sariel_frame_bg_transition(state);
		if(boss.phase_frame == 0) {
			sariel_phase_next(state, 1);
			sariel.patterns_until_next = word(
				static_cast<std::int16_t>(pc98::irand(state.random_seed) % 6U) + 1
			);
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == phase_form1_defeated) {
		sariel_form1_defeated(state);
		return BossUpdateResult::running;
	}
	if(boss.phase == phase_form2) {
		sariel_frame_common(state, true);
		particles_horizontal_update(state, boss.phase_frame);
		switch(sariel.pattern_current) {
		case 0:
			sariel.form2_leaf_interval = rank_value(state, 56, 32, 24, 20);
			if(boss.phase_frame > 100) boss.phase_frame = 0;
			break;
		case 1: pattern_curved_spray_once(state); break;
		case 2: pattern_rain_from_seal_center(state); break;
		case 3: pattern_curved_spray_twice(state); break;
		case 4:
			leaves_update(state, sariel.form2_leaf_interval);
			if(boss.phase_frame > 300) boss.phase_frame = 0;
			break;
		case 5:
			leaves_update(state, 500);
			if(boss.phase_frame > 200) boss.phase_frame = 0;
			break;
		case 6:
			leaves_update(state, 500);
			pattern_curved_spray_once(state);
			break;
		case 7:
			leaves_update(state, 500);
			pattern_rain_from_seal_center(state);
			break;
		case 8:
			leaves_update(state, 32);
			pattern_curved_spray_twice(state);
			break;
		default: return BossUpdateResult::unsupported;
		}
		if(boss.phase_frame == 0) {
			sariel_pattern_next(sariel, 4, 8);
		}
		sariel_hit_update(state);
		if(boss.hp <= 0) {
			boss.phase = static_cast<std::int8_t>(phase_form2_defeated);
			boss.phase_frame = 0;
			sariel.hit_invincibility_frame = 0;
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == phase_form2_defeated) {
		// b20m.cpp's final 231-frame shake/blackout is presentation-only.
		state.score += 50000U;
		state.game_cleared = true;
		boss.active = false;
		return BossUpdateResult::stage_cleared;
	}
	return BossUpdateResult::unsupported;
}

void sariel_render_tail(State &) noexcept
{
	// b20m.cpp's tail is entirely VRAM/palette work. Gameplay-affecting tails
	// (birds, leaves, spawn timing, and shared IRAND) run at their source sites.
}

} // namespace th01::core
