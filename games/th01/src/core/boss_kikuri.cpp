#include "th01/core/boss_kikuri.hpp"

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
constexpr std::int16_t player_width = 32;
constexpr std::int16_t player_height = 32;
constexpr std::int16_t player_top = playfield_bottom - player_height;
constexpr std::int16_t pellet_width = 8;
constexpr std::int16_t pellet_height = 8;
constexpr std::int16_t disc_center_x = 320;
constexpr std::int16_t disc_center_y = 180;
constexpr std::int16_t disc_radius = 90;
constexpr std::int16_t eye_left_center_x = 307;
constexpr std::int16_t eye_right_center_x = 336;
constexpr std::int16_t eye_bottom = 147;
constexpr std::int16_t lightball_center_x = 320;
constexpr std::int16_t lightball_center_y = 224;
constexpr std::int16_t soul_width = 32;
constexpr std::int16_t soul_height = 32;
constexpr std::int16_t tear_width = 16;
constexpr std::int16_t tear_height = 16;
constexpr std::int16_t soul_area_left = playfield_left + ((playfield_right - playfield_left) / 20);
constexpr std::int16_t soul_area_right = playfield_right - ((playfield_right - playfield_left) / 20) - soul_width;
constexpr std::int16_t soul_area_top = playfield_top;
constexpr std::int16_t soul_area_bottom = playfield_bottom;
constexpr std::int16_t tear_top_max = playfield_bottom - tear_height;
constexpr std::int16_t hp_total = 14;
constexpr std::int16_t hp_phase_2_end = 10;
constexpr std::int16_t hp_phase_5_end = 6;
constexpr std::int16_t hp_phase_6_end = 0;
constexpr std::int16_t phase4_soul_activation = 0;
constexpr std::int16_t phase4_pattern = 1;
constexpr std::int16_t phase4_done = 2;

[[nodiscard]] constexpr std::int16_t word(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
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

void kikuri_frame_common(State &state) noexcept
{
	state.boss.phase_frame = word(static_cast<std::int32_t>(state.boss.phase_frame) + 1);
	state.boss.kikuri.hit_invincibility_frame = word(
		static_cast<std::int32_t>(state.boss.kikuri.hit_invincibility_frame) + 1
	);
}

[[nodiscard]] bool kikuri_orb_hittest(const State &state) noexcept
{
	// b15j.cpp's vertical bounds both point upward.  Keep that source bug:
	// all Orb positions above y=132 can hit, including positions above the
	// playfield's visible disc.
	return state.orb.horizontal.left > 256 && state.orb.horizontal.left < 352 &&
		state.orb.top < 132 && state.orb.top < 180;
}

void kikuri_hit_update(State &state) noexcept
{
	auto &kikuri = state.boss.kikuri;
	// b15j.cpp intentionally leaves CShots::hittest_boss() at its default
	// zero rectangle, unlike the other boss owners.
	static_cast<void>(boss_hit_update(
		state,
		kikuri.hit_invincibility_frame,
		kikuri.hit_invincible,
		kikuri_orb_hittest(state),
		0,
		0,
		0,
		0,
		7000
	));
}

void kikuri_soul_pos_set(KikuriSoulState &soul, std::int16_t left, std::int16_t top) noexcept
{
	soul.left = left;
	soul.top = top;
}

void kikuri_tear_pos_set(KikuriTearState &tear, std::int16_t left, std::int16_t top) noexcept
{
	tear.left = left;
	tear.top = top;
}

void kikuri_soul_locked_move(KikuriSoulState &soul, std::int16_t dx, std::int16_t dy) noexcept
{
	if(soul.lock_frame == 0) {
		soul.previous_delta_x = dx;
		soul.previous_delta_y = dy;
		soul.previous_left = soul.left;
		soul.previous_top = soul.top;
		soul.left = word(static_cast<std::int32_t>(soul.left) + dx);
		soul.top = word(static_cast<std::int32_t>(soul.top) + dy);
		if(soul.left < soul_area_left) soul.left = soul_area_left;
		if(soul.left > soul_area_right) soul.left = soul_area_right;
		if(soul.top < soul_area_top) soul.top = soul_area_top;
		if(soul.top > soul_area_bottom) soul.top = soul_area_bottom;
		soul.lock_frame = 1;
	} else if(soul.lock_frame >= 1) {
		soul.lock_frame = 0;
	} else {
		soul.lock_frame = word(static_cast<std::int32_t>(soul.lock_frame) + 1);
	}
}

void kikuri_tear_locked_move(KikuriTearState &tear, std::int16_t dx, std::int16_t dy) noexcept
{
	if(tear.lock_frame == 0) {
		tear.previous_delta_x = dx;
		tear.previous_delta_y = dy;
		tear.previous_left = tear.left;
		tear.previous_top = tear.top;
		tear.left = word(static_cast<std::int32_t>(tear.left) + dx);
		tear.top = word(static_cast<std::int32_t>(tear.top) + dy);
		if(tear.left < soul_area_left) tear.left = soul_area_left;
		if(tear.left > soul_area_right) tear.left = soul_area_right;
		if(tear.top < soul_area_top) tear.top = soul_area_top;
		if(tear.top > soul_area_bottom) tear.top = soul_area_bottom;
		tear.lock_frame = 1;
	} else if(tear.lock_frame >= 1) {
		tear.lock_frame = 0;
	} else {
		tear.lock_frame = word(static_cast<std::int32_t>(tear.lock_frame) + 1);
	}
}

void soul_move_and_render(State &state, std::size_t i, std::int16_t dx, std::int16_t dy) noexcept
{
	auto &soul = state.boss.kikuri.souls[i];
	kikuri_soul_locked_move(soul, dx, dy);
	if((state.boss.phase_frame % 12) == 0) {
		soul.image = soul.image >= 2U ? 0U : static_cast<std::uint8_t>(soul.image + 1U);
	}
}

void tears_add(State &state, std::int16_t left, std::int16_t top) noexcept
{
	auto &kikuri = state.boss.kikuri;
	for(std::size_t i = 0U; i < kikuri.tear_animation_frame.size(); ++i) {
		if(kikuri.tear_animation_frame[i] != 0) continue;
		kikuri_tear_pos_set(kikuri.tears[i], left, top);
		kikuri.tears[i].image = 0U;
		kikuri.tear_animation_frame[i] = 1;
		return;
	}
}

[[nodiscard]] bool tear_ripple_hittest(
	State &state,
	std::int16_t left,
	std::int16_t extra_width
) noexcept
{
	if(state.player.invincible) return false;
	if(state.player.left >= word(static_cast<std::int32_t>(left) - 16) &&
		state.player.left <= word(static_cast<std::int32_t>(left) + extra_width)) {
		state.input.player_is_hit = true;
		return true;
	}
	return false;
}

void ripple_update_and_render(State &state, std::int16_t tear_left, std::int8_t &frame) noexcept
{
	frame = static_cast<std::int8_t>(frame + 1);
	if(frame < 10) {
		static_cast<void>(tear_ripple_hittest(state, tear_left, 0));
	} else if(frame < 20) {
		static_cast<void>(tear_ripple_hittest(state,
			word(static_cast<std::int32_t>(tear_left) - tear_width), 32));
	} else if(frame < 60) {
		static_cast<void>(tear_ripple_hittest(state,
			word(static_cast<std::int32_t>(tear_left) - (tear_width * 2)), 64));
	} else if(frame >= 75) {
		frame = 0;
	}
}

void tears_update_and_render(State &state) noexcept
{
	auto &kikuri = state.boss.kikuri;
	for(std::size_t i = 0U; i < kikuri.tear_animation_frame.size(); ++i) {
		if(kikuri.tear_animation_frame[i] == 0) continue;
		if(kikuri.tears[i].top <= tear_top_max) {
			kikuri_tear_locked_move(kikuri.tears[i], 0, 8);
		} else {
			ripple_update_and_render(
				state,
				kikuri.tears[i].left,
				kikuri.tear_animation_frame[i]
			);
		}
	}
}

void souls_move_diagonally_and_tears_update(
	State &state,
	std::int16_t velocity_base_x,
	std::int16_t velocity_base_y
) noexcept
{
	const auto phase_angle = static_cast<std::uint8_t>(state.boss.phase_frame);
	const auto velocity_x = pc98::masterlib::vector2(phase_angle, velocity_base_x).x;
	const auto velocity_y = pc98::masterlib::vector2(phase_angle, velocity_base_y).x;
	tears_update_and_render(state);
	soul_move_and_render(state, 0U, velocity_x, velocity_y);
	soul_move_and_render(state, 1U, static_cast<std::int16_t>(-velocity_x), velocity_y);
}

void pattern_symmetric_spiral_from_disc(State &state) noexcept
{
	auto &boss = state.boss;
	auto &kikuri = boss.kikuri;
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame == 100) {
		kikuri.phase_2_angle = 0U;
		kikuri.phase_2_drift = 0U;
		kikuri.pattern_state = rank_value(state, 4, 3, 2, 1);
		kikuri.phase_2_distance = rank_value(state, disc_radius, disc_radius,
			disc_radius, static_cast<std::int16_t>((disc_radius / 2) + 5));
	}
	if((boss.phase_frame % kikuri.pattern_state) == 0) {
		for(const auto offset : {std::uint8_t{0U}, std::uint8_t{0x80U}}) {
			const auto spawn_angle = static_cast<std::uint8_t>(kikuri.phase_2_angle + offset);
			pellets_add_single(
				state,
				polar_x(disc_center_x, kikuri.phase_2_distance, spawn_angle),
				polar_y(disc_center_y, kikuri.phase_2_distance, spawn_angle),
				static_cast<std::uint8_t>(spawn_angle + kikuri.phase_2_drift),
				50
			);
		}
		if(boss.phase_frame >= 900) boss.phase_frame = 0;
		kikuri.phase_2_angle = static_cast<std::uint8_t>(kikuri.phase_2_angle - 0x08U);
		kikuri.phase_2_drift = static_cast<std::uint8_t>(kikuri.phase_2_drift + 1U);
	}
}

void pattern_spinning_aimed_rings(State &state) noexcept
{
	const auto interval = rank_value(state, 80, 60, 55, 50);
	if((state.boss.phase_frame % interval) != 0) return;
	const auto angle = angle_between(
		lightball_center_x,
		lightball_center_y,
		word(static_cast<std::int32_t>(state.player.left) + (player_width / 2) +
			(pellet_width / 2)),
		static_cast<std::int16_t>(player_top + (player_height / 2) + (pellet_height / 2))
	);
	for(std::uint8_t i = 0U; i < 8U; ++i) {
		const auto ring_angle = static_cast<std::uint8_t>(i * 0x20U);
		pellets_add_single(
			state,
			polar_x(static_cast<std::int16_t>(lightball_center_x - (pellet_width / 2)),
				16, ring_angle),
			polar_y(lightball_center_y, 16, ring_angle),
			angle,
			48,
			PelletMotion::spin,
			32,
			static_cast<std::int16_t>(lightball_center_x - pellet_width),
			lightball_center_y
		);
	}
}

[[nodiscard]] std::int16_t phase4_souls_activate(State &state) noexcept
{
	auto &boss = state.boss;
	auto &kikuri = boss.kikuri;
	if(boss.phase_frame == 200) {
		for(auto &soul : kikuri.souls) {
			kikuri_soul_pos_set(soul,
				static_cast<std::int16_t>(lightball_center_x - (soul_width / 2)),
				static_cast<std::int16_t>(lightball_center_y - (soul_height / 2)));
		}
	} else if((boss.phase_frame > 200) && ((boss.phase_frame % 2) == 0)) {
		soul_move_and_render(state, 0U, -4, -3);
		soul_move_and_render(state, 1U, 4, -3);
		if(kikuri.souls[0].top < 96) return phase4_pattern;
	}
	return phase4_soul_activation;
}

[[nodiscard]] std::int16_t pattern_souls_spreads(State &state) noexcept
{
	auto &boss = state.boss;
	auto &kikuri = boss.kikuri;
	soul_move_and_render(state, 0U, 0, 0);
	soul_move_and_render(state, 1U, 0, 0);
	if((boss.phase_frame % 80) == 0) {
		const auto group = [&state]() noexcept {
			switch(state.rank) {
			case 0: return PelletGroup::two_spread_wide_aimed;
			case 1: return PelletGroup::three_spread_wide_aimed;
			case 2: return PelletGroup::five_spread_wide_aimed;
			case 3: return PelletGroup::five_spread_wide;
			default: return PelletGroup::three_spread_wide_aimed;
			}
		}();
		for(const auto &soul : kikuri.souls) {
			pellets_add_group(
				state,
				static_cast<std::int16_t>(soul.left + (soul_width / 2) - (pellet_width / 2)),
				static_cast<std::int16_t>(soul.top + (soul_height / 2) - pellet_height),
				group,
				55
			);
		}
	}
	return boss.phase_frame > 600 ? phase4_done : phase4_pattern;
}

void pattern_souls_drop_tears_and_move_diagonally(State &state) noexcept
{
	auto &boss = state.boss;
	auto &kikuri = boss.kikuri;
	if(boss.phase_frame < 10) {
		kikuri.pattern_state = rank_value(state, 200, 160, 140, 120);
	}
	if((boss.phase_frame % kikuri.pattern_state) == 0) {
		for(const auto &soul : kikuri.souls) {
			tears_add(
				state,
				static_cast<std::int16_t>(soul.left + (soul_width / 2) - (tear_width / 4)),
				static_cast<std::int16_t>(soul.top + (soul_height / 2) - (tear_height / 2))
			);
		}
	}
	souls_move_diagonally_and_tears_update(state, 6, 2);
}

void fire_aimed_eye_laser(
	State &state,
	std::size_t slot,
	std::int16_t origin_left,
	std::int16_t aim_offset_x,
	std::int16_t speed_multiplied_by_8,
	std::int16_t width
) noexcept
{
	shootout_laser_spawn(
		state,
		slot,
		origin_left,
		eye_bottom,
		word(static_cast<std::int32_t>(state.player.left) + aim_offset_x),
		playfield_bottom,
		speed_multiplied_by_8,
		10U,
		20,
		width
	);
}

void fire_random_aimed_eye_laser(
	State &state,
	std::size_t slot,
	std::int16_t origin_left,
	std::int16_t aim_range_x,
	std::int16_t speed_multiplied_by_8,
	std::int16_t width
) noexcept
{
	const auto random_offset = static_cast<std::int16_t>(
		static_cast<std::int32_t>(pc98::irand(state.random_seed) %
			static_cast<std::uint16_t>(aim_range_x * 2)) - aim_range_x
	);
	fire_aimed_eye_laser(state, slot, origin_left, random_offset,
		speed_multiplied_by_8, width);
}

void pattern_two_crossed_eye_lasers(State &state) noexcept
{
	if((state.boss.phase_frame > 200) && ((state.boss.phase_frame % 200) == 0)) {
		fire_aimed_eye_laser(state, 0U, eye_left_center_x, 60, 50, 8);
		fire_aimed_eye_laser(state, 1U, eye_right_center_x, -60, 50, 8);
	}
}

void pattern_souls_single_aimed_pellet_and_move_diagonally(State &state) noexcept
{
	if((state.boss.phase_frame % 200) == 0) {
		for(const auto &soul : state.boss.kikuri.souls) {
			pellets_add_group(
				state,
				static_cast<std::int16_t>(soul.left + (soul_width / 2) - (pellet_width / 2)),
				static_cast<std::int16_t>(soul.top + (soul_height / 2) - (pellet_height / 2)),
				PelletGroup::one_aimed,
				40
			);
		}
	}
	souls_move_diagonally_and_tears_update(state, 4, 2);
}

[[nodiscard]] std::int16_t pattern_4_spiral_along_disc(State &state) noexcept
{
	auto &boss = state.boss;
	auto &kikuri = boss.kikuri;
	if(boss.phase_frame < 100) return 0;
	if(boss.phase_frame == 100) {
		kikuri.phase_6_spiral_angle = 0x20U;
		kikuri.pattern_state = rank_value(state, 14, 8, 6, 4);
	}
	if((boss.phase_frame % kikuri.pattern_state) == 0) {
		for(std::int16_t i = 0; i < 4; i += 2) {
			const auto spawn_angle = static_cast<std::uint8_t>(
				boss.phase_frame * 4 + (0x80 * i)
			);
			const auto left = polar_x(disc_center_x, disc_radius, spawn_angle);
			const auto top = polar_y(disc_center_y, disc_radius, spawn_angle);
			pellets_add_single(state, left, top, kikuri.phase_6_spiral_angle, 36);
			pellets_add_single(state, left, top,
				static_cast<std::uint8_t>(kikuri.phase_6_spiral_angle + 0x40U), 36);
			pellets_add_single(state, left, top,
				static_cast<std::uint8_t>(kikuri.phase_6_spiral_angle + 0x80U), 36);
			pellets_add_single(state, left, top,
				static_cast<std::uint8_t>(kikuri.phase_6_spiral_angle + 0xC0U), 36);
		}
		if(boss.phase_frame <= 600) {
			kikuri.phase_6_spiral_angle = static_cast<std::uint8_t>(
				kikuri.phase_6_spiral_angle - 0x05U
			);
		} else {
			boss.phase_frame = 0;
			return 1;
		}
	}
	return 0;
}

[[nodiscard]] std::int16_t pattern_single_lasers_from_left_eye(State &state) noexcept
{
	auto &boss = state.boss;
	auto &kikuri = boss.kikuri;
	if(boss.phase_frame <= 200) return 1;
	if(boss.phase_frame == 250) {
		kikuri.pattern_state = rank_value(state, 52, 56, 60, 64);
		fire_random_aimed_eye_laser(state, 0U, eye_left_center_x, 60,
			kikuri.pattern_state, 4);
	} else if(boss.phase_frame == 280) {
		fire_random_aimed_eye_laser(state, 1U, eye_left_center_x, 60,
			kikuri.pattern_state, 4);
	} else if(boss.phase_frame == 310) {
		fire_random_aimed_eye_laser(state, 2U, eye_left_center_x, 60,
			kikuri.pattern_state, 4);
	} else if(boss.phase_frame == 340) {
		fire_random_aimed_eye_laser(state, 3U, eye_left_center_x, 60,
			kikuri.pattern_state, 4);
	} else if(boss.phase_frame == 370) {
		fire_random_aimed_eye_laser(state, 4U, eye_left_center_x, 60,
			kikuri.pattern_state, 4);
	} else if(boss.phase_frame == 400) {
		fire_random_aimed_eye_laser(state, 5U, eye_left_center_x, 60,
			kikuri.pattern_state, 4);
	}
	if(boss.phase_frame > 500) {
		boss.phase_frame = 0;
		return 2;
	}
	return 1;
}

void add_rain_pellet(State &state, std::int16_t left, std::int16_t top,
	std::uint8_t angle, std::int16_t speed) noexcept
{
	pellets_add_single(state, left, top, angle, speed, PelletMotion::gravity, 1);
}

[[nodiscard]] std::int16_t pattern_souls_symmetric_rain_lines(State &state) noexcept
{
	auto &boss = state.boss;
	auto &kikuri = boss.kikuri;
	if(boss.phase_frame > 100 && (boss.phase_frame % kikuri.pattern_state) == 0) {
		for(const auto &soul : kikuri.souls) {
			const auto left = static_cast<std::int16_t>(soul.left + (soul_width / 2) - (pellet_width / 2));
			const auto top = static_cast<std::int16_t>(soul.top + (soul_height / 2) - (pellet_height / 2));
			add_rain_pellet(state, left, top, 0x00U, 48);
			add_rain_pellet(state, left, top, 0x00U, 32);
			add_rain_pellet(state, left, top, 0x00U, 16);
			add_rain_pellet(state, left, top, 0x80U, 16);
			add_rain_pellet(state, left, top, 0x80U, 32);
			add_rain_pellet(state, left, top, 0x80U, 48);
		}
	}
	if(boss.phase_frame > 250) {
		boss.phase_frame = 0;
		return 3;
	}
	return 2;
}

[[nodiscard]] std::int16_t pattern_vertical_lasers_from_top(State &state) noexcept
{
	auto &boss = state.boss;
	auto &kikuri = boss.kikuri;
	if(boss.phase_frame < 100) return 3;
	if(boss.phase_frame == 100) {
		kikuri.pattern_state = rank_value(state, 60, 64, 68, 72);
		kikuri.phase_6_random_range_x_half = rank_value(state, 0, 10, 16, 20);
	}
	if((boss.phase_frame % 10) == 0) {
		const auto slot = static_cast<std::size_t>((boss.phase_frame - 100) / 10);
		const auto range = kikuri.phase_6_random_range_x_half;
		const auto random_offset = static_cast<std::int16_t>(
			static_cast<std::int32_t>(pc98::irand(state.random_seed) %
				static_cast<std::uint16_t>((range * 2) + 1)) - range
		);
		const auto left = static_cast<std::int16_t>(playfield_left +
			static_cast<std::int16_t>(slot) * 64 + random_offset);
		shootout_laser_spawn(state, slot, left, playfield_top, left, playfield_bottom,
			kikuri.pattern_state, 10U, 20, 8);
	}
	if(boss.phase_frame >= 190) {
		boss.phase_frame = 0;
		return 0;
	}
	return 3;
}

void kikuri_defeat(State &state) noexcept
{
	pellets_unput_and_reset_nonclouds(state);
	// b15j.cpp carries its off-by-one reset count: slots 0..3 only.
	shootout_lasers_reset_first(state, 4U);
	state.stage_cleared = true;
	state.input.player_is_hit = true;
}

} // namespace

void kikuri_setup(State &state) noexcept
{
	auto &boss = state.boss;
	auto &kikuri = boss.kikuri;
	boss.id = BossId::kikuri;
	boss.active = true;
	boss.phase = 0;
	boss.phase_frame = 0;
	boss.hp = hp_total;
	state.projectiles.pellet_interlace = true;
	state.projectiles.pellet_unknown_seven = 7;
	for(auto &frame : kikuri.tear_animation_frame) frame = 0;
	for(auto &soul : kikuri.souls) kikuri_soul_pos_set(soul, 0, 0);
}

[[nodiscard]] BossUpdateResult kikuri_update(State &state) noexcept
{
	auto &boss = state.boss;
	auto &kikuri = boss.kikuri;
	if(!boss.active || boss.id != BossId::kikuri) return BossUpdateResult::unsupported;
	if(boss.phase == 0) {
		// Both entrance loops block source gameplay until their endpoint.  Their
		// page/palette work is presentation-only; this records the exact combat
		// boundary that follows them.
		kikuri.hit_invincibility_frame = 0;
		kikuri.hit_invincible = false;
		boss.phase = 2;
		boss.phase_frame = 0;
		kikuri.patterns_done = 0;
		kikuri.initial_hp_rendered = false;
		return BossUpdateResult::running;
	}
	if(boss.phase == 2) {
		kikuri_frame_common(state);
		pattern_symmetric_spiral_from_disc(state);
		kikuri_hit_update(state);
		if(boss.phase_frame == 0) {
			kikuri.patterns_done = word(static_cast<std::int32_t>(kikuri.patterns_done) + 1);
		}
		if(!kikuri.hit_invincible &&
			(boss.hp <= hp_phase_2_end || kikuri.patterns_done >= 6)) {
			boss.phase = 3;
			boss.phase_frame = 0;
			kikuri.hit_invincibility_frame = 0;
			kikuri.patterns_done = 0;
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 3) {
		boss.phase_frame = word(static_cast<std::int32_t>(boss.phase_frame) + 1);
		if(boss.phase_frame >= 100) {
			boss.phase = 4;
			boss.phase_frame = 0;
			kikuri.phase_u1 = phase4_soul_activation;
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 4) {
		kikuri_frame_common(state);
		pattern_spinning_aimed_rings(state);
		if(kikuri.phase_u1 == phase4_soul_activation) {
			kikuri.phase_u1 = phase4_souls_activate(state);
		} else {
			kikuri.phase_u1 = pattern_souls_spreads(state);
		}
		kikuri_hit_update(state);
		if(!kikuri.hit_invincible && kikuri.phase_u1 == phase4_done) {
			boss.phase = 5;
			kikuri.phase_u1 = 0;
			boss.phase_frame = 0;
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 5) {
		kikuri_frame_common(state);
		pattern_souls_drop_tears_and_move_diagonally(state);
		pattern_two_crossed_eye_lasers(state);
		kikuri_hit_update(state);
		if(!kikuri.hit_invincible &&
			(boss.hp <= hp_phase_5_end || boss.phase_frame > 1600)) {
			boss.phase = 6;
			boss.phase_frame = 0;
			kikuri.hit_invincibility_frame = 0;
			kikuri.patterns_done = 0;
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 6) {
		kikuri_frame_common(state);
		pattern_souls_single_aimed_pellet_and_move_diagonally(state);
		if(kikuri.phase_u1 == 0) {
			kikuri.phase_u1 = pattern_4_spiral_along_disc(state);
		} else if(kikuri.phase_u1 == 1) {
			kikuri.phase_u1 = pattern_single_lasers_from_left_eye(state);
		} else if(kikuri.phase_u1 == 2) {
			kikuri.phase_u1 = pattern_souls_symmetric_rain_lines(state);
		} else if(kikuri.phase_u1 == 3) {
			kikuri.phase_u1 = pattern_vertical_lasers_from_top(state);
		} else {
			return BossUpdateResult::unsupported;
		}
		if(boss.phase_frame == 0) {
			kikuri.patterns_done = word(static_cast<std::int32_t>(kikuri.patterns_done) + 1);
		}
		if(kikuri.patterns_done > 29) boss.hp = 1;
		kikuri_hit_update(state);
		if(boss.hp <= hp_phase_6_end) {
			kikuri_defeat(state);
			return BossUpdateResult::stage_cleared;
		}
		return BossUpdateResult::running;
	}
	return BossUpdateResult::unsupported;
}

void kikuri_render_tail(State &) noexcept
{
	// The b15j.cpp tail is only two-page tear/ripple, line, and palette work.
	// The source's live ripple collision remains at its original update point.
}

} // namespace th01::core
