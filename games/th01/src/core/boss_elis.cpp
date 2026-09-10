#include "th01/core/boss_elis.hpp"

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
constexpr std::int16_t playfield_width = 640;
constexpr std::int16_t playfield_height = 336;
constexpr std::int16_t player_width = 32;
constexpr std::int16_t player_height = 32;
constexpr std::int16_t player_top = 368;
constexpr std::int16_t girl_width = 128;
constexpr std::int16_t girl_height = 96;
constexpr std::int16_t bat_width = 48;
constexpr std::int16_t bat_height = 32;
constexpr std::int16_t pellet_width = 8;
constexpr std::int16_t pellet_height = 8;
constexpr std::int16_t circle_radius = 128;
constexpr std::int16_t choose_new = 0;

enum ElisForm : std::uint8_t { girl, bat };
enum StarPatternState : std::uint8_t { star_summon, pattern_active };
enum PhaseFiveSubphase : std::int16_t { phase_five_pattern, phase_five_transform };

[[nodiscard]] constexpr std::int16_t word(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr std::uint8_t byte(std::int32_t value) noexcept
{
	return static_cast<std::uint8_t>(static_cast<std::uint32_t>(value));
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

[[nodiscard]] constexpr std::int16_t girl_center_left(const ElisState &elis) noexcept
{
	return word(static_cast<std::int32_t>(elis.girl.left) + (girl_width / 2));
}

[[nodiscard]] constexpr std::int16_t girl_center_top(const ElisState &elis) noexcept
{
	return word(static_cast<std::int32_t>(elis.girl.top) + (girl_height / 2));
}

[[nodiscard]] constexpr std::int16_t bat_center_left(const ElisState &elis) noexcept
{
	return word(static_cast<std::int32_t>(elis.bat.left) + (bat_width / 2));
}

[[nodiscard]] constexpr std::int16_t bat_center_top(const ElisState &elis) noexcept
{
	return word(static_cast<std::int32_t>(elis.bat.top) + (bat_height / 2));
}

[[nodiscard]] constexpr std::int16_t form_center_left(const ElisState &elis) noexcept
{
	return (elis.form == girl) ? girl_center_left(elis) : bat_center_left(elis);
}

[[nodiscard]] constexpr std::int16_t form_center_top(const ElisState &elis) noexcept
{
	return (elis.form == girl) ? girl_center_top(elis) : bat_center_top(elis);
}

[[nodiscard]] std::int16_t polar_x(
	std::int16_t center, std::int16_t radius, std::uint8_t angle
) noexcept
{
	return word(static_cast<std::int32_t>(center) + pc98::masterlib::vector2(angle, radius).x);
}

[[nodiscard]] std::int16_t polar_y(
	std::int16_t center, std::int16_t radius, std::uint8_t angle
) noexcept
{
	return word(static_cast<std::int32_t>(center) + pc98::masterlib::vector2(angle, radius).y);
}

[[nodiscard]] std::int16_t random_girl_left(State &state) noexcept
{
	return static_cast<std::int16_t>(pc98::irand(state.random_seed) %
		static_cast<std::uint16_t>(playfield_width - girl_width));
}

[[nodiscard]] std::int16_t random_girl_top(State &state) noexcept
{
	// playfield_rand_y(0.0f, 17 / 42.0f), with playfield height 336.
	return word(static_cast<std::int32_t>(playfield_top) +
		static_cast<std::int16_t>(pc98::irand(state.random_seed) % 136U));
}

[[nodiscard]] std::int16_t surround_left(State &state, const ElisTopleftState &entity) noexcept
{
	return word(static_cast<std::int32_t>(entity.left) +
		static_cast<std::int16_t>(pc98::irand(state.random_seed) % 192U) - 32);
}

[[nodiscard]] std::int16_t surround_top(
	State &state, const ElisTopleftState &entity, bool is_bat
) noexcept
{
	const auto offset = is_bat ? 48 : 32;
	return word(static_cast<std::int32_t>(entity.top) +
		static_cast<std::int16_t>(pc98::irand(state.random_seed) % 128U) - offset);
}

void phase_frame_common(State &state, std::int8_t phase) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	boss.phase_frame = word(static_cast<std::int32_t>(boss.phase_frame) + 1);
	elis.invincibility_frame = word(static_cast<std::int32_t>(elis.invincibility_frame) + 1);
	if(phase != 1) elis.attack = elis.girl;
}

[[nodiscard]] bool girl_orb_hittest(const State &state) noexcept
{
	const auto &elis = state.boss.elis;
	if(elis.girl_orb_hitbox_inactive) return false;
	// hitbox_orb_set(16, -4, 112, 80): bounds after the donor's Orb offsets.
	return state.orb.prev_left >= word(static_cast<std::int32_t>(elis.girl.left) + 8) &&
		state.orb.prev_left <= word(static_cast<std::int32_t>(elis.girl.left) + 88) &&
		state.orb.prev_top >= word(static_cast<std::int32_t>(elis.girl.top) - 12) &&
		state.orb.prev_top <= word(static_cast<std::int32_t>(elis.girl.top) + 68);
}

[[nodiscard]] bool bat_orb_hittest(const State &state) noexcept
{
	const auto &bat_state = state.boss.elis.bat;
	// hitbox_orb_set(-8, -8, BAT_W + 8, BAT_H + 8).
	return state.orb.prev_left >= word(static_cast<std::int32_t>(bat_state.left) - 16) &&
		state.orb.prev_left <= word(static_cast<std::int32_t>(bat_state.left) + 24) &&
		state.orb.prev_top >= word(static_cast<std::int32_t>(bat_state.top) - 16) &&
		state.orb.prev_top <= word(static_cast<std::int32_t>(bat_state.top) + 16);
}

void hit_update(State &state) noexcept
{
	auto &elis = state.boss.elis;
	const auto is_girl = elis.form == girl;
	const auto shot_left = is_girl ? word(static_cast<std::int32_t>(elis.girl.left) + 32) : elis.bat.left;
	// b15m's bat branch deliberately passes cur_left for the hitbox top.
	const auto shot_top = is_girl ? word(static_cast<std::int32_t>(elis.girl.top) + 32) : elis.bat.left;
	static_cast<void>(boss_hit_update(
		state, elis.invincibility_frame, elis.invincible,
		is_girl ? girl_orb_hittest(state) : bat_orb_hittest(state),
		shot_left, shot_top, is_girl ? 64 : 32, is_girl ? 48 : 32, 7000
	));
}

void fire_from_form(State &state, PelletGroup group, std::int16_t speed) noexcept
{
	const auto &elis = state.boss.elis;
	pellets_add_group(state,
		word(static_cast<std::int32_t>(form_center_left(elis)) - 4),
		word(static_cast<std::int32_t>(form_center_top(elis)) - 4), group, speed);
}

void rifts_update(
	State &state,
	std::array<ElisTopleftState, 5U> &rifts,
	const ElisTopleftState &entity,
	bool entity_is_bat,
	std::int16_t start_frame,
	std::int16_t end_frame
) noexcept
{
	const auto frame = state.boss.phase_frame;
	if(frame < start_frame || frame > end_frame || (frame % 4) != 0) return;
	for(std::size_t i = 0U; i < rifts.size(); ++i) {
		if((frame % 16) == static_cast<std::int16_t>((i * 4U) % 16U) || frame == start_frame) {
			rifts[i].left = surround_left(state, entity);
			rifts[i].top = surround_top(state, entity, entity_is_bat);
		}
		if(frame < end_frame) static_cast<void>(pc98::irand(state.random_seed) % 2U);
	}
}

[[nodiscard]] bool wave_teleport(
	State &state, std::int16_t target_left, std::int16_t target_top
) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	elis.attack = elis.girl;
	if(boss.phase_frame == 20 || boss.phase_frame == 28 || boss.phase_frame == 36 ||
		boss.phase_frame == 44 || boss.phase_frame == 52 || boss.phase_frame == 60 ||
		boss.phase_frame == 68) {
		elis.girl_orb_hitbox_inactive = true;
	}
	if(boss.phase_frame == 52) elis.girl = {target_left, target_top};
	if(boss.phase_frame == 76) elis.girl_orb_hitbox_inactive = false;
	if(boss.phase_frame > 80) {
		boss.phase_frame = 0;
		return true;
	}
	if((boss.phase_frame % 4) != 0) return false;
	for(auto &star : elis.teleport_stars) {
		if(boss.phase_frame < 40 || boss.phase_frame > 52) {
			star.left = surround_left(state, elis.girl);
			star.top = surround_top(state, elis.girl, false);
		} else {
			const auto next_left = surround_left(state, elis.girl);
			const auto next_top = surround_top(state, elis.girl, false);
			star.left = word(static_cast<std::int32_t>(star.left) + ((next_left - star.left) / 3));
			star.top = word(static_cast<std::int32_t>(star.top) + ((next_top - star.top) / 3));
		}
	}
	return false;
}

[[nodiscard]] bool circle_summon_step(ElisState &elis, std::int16_t frame, std::int16_t start) noexcept
{
	if(frame < start || (frame % 2) != 0 || elis.circle_frame != 0) return false;
	elis.circle_angle = static_cast<std::uint8_t>(elis.circle_angle + 2U);
	if(elis.circle_angle < 64U) return false;
	elis.circle_frame = 1;
	return true;
}

[[nodiscard]] bool star_of_david(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	if(boss.phase_frame < 5) { elis.circle_frame = 0; elis.circle_angle = 0; }
	if(boss.phase_frame < 10) return false;
	if(boss.phase_frame == 10) { elis.circle_frame = 0; elis.circle_angle = 0; }
	if(circle_summon_step(elis, boss.phase_frame, 10)) return false;
	if(elis.circle_angle < 64U) return false;
	elis.circle_frame = word(static_cast<std::int32_t>(elis.circle_frame) + 1);
	if(elis.circle_frame <= 60) return false;
	boss.phase_frame = 0;
	elis.circle_frame = 0;
	elis.circle_angle = 0;
	return true;
}

[[nodiscard]] std::int16_t phase_one_lasers(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	if(boss.phase_frame == 50) {
		elis.pattern_subphase = static_cast<std::uint8_t>(pc98::irand(state.random_seed) % 2U);
		elis.pattern_value = rank_value(state, 50, 55, 60, 65);
	}
	if(boss.phase_frame >= 70 && (boss.phase_frame % 10) == 0) {
		const auto tenth = static_cast<std::int16_t>((boss.phase_frame - 70) / 10);
		const auto target_left = (elis.pattern_subphase == 0U) ?
			static_cast<std::int16_t>(tenth * 64) : word(static_cast<std::int32_t>(playfield_right) - (tenth * 64));
		shootout_laser_spawn_safe(state, static_cast<std::size_t>(boss.phase_frame / 10),
			word(static_cast<std::int32_t>(elis.girl.left) + 60), word(static_cast<std::int32_t>(elis.girl.top) + 28),
			target_left, playfield_bottom, elis.pattern_value, 15U, 25, 4);
		if(target_left >= playfield_right || target_left <= playfield_left) { boss.phase_frame = 0; return choose_new; }
	}
	return 1;
}

[[nodiscard]] std::int16_t phase_one_missiles(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	if(boss.phase_frame == 50) elis.pattern_value = rank_value(state, 0x0F, 0x15, 0x19, 0x1D);
	if(boss.phase_frame > 60 && (boss.phase_frame % 3) == 0) {
		const auto i = static_cast<std::size_t>(pc98::irand(state.random_seed) % 5U);
		const auto angle = byte(static_cast<std::int32_t>(pc98::irand(state.random_seed) % static_cast<std::uint16_t>(elis.pattern_value)) -
			((elis.pattern_value - 1) / 2) + 0x40);
		const auto velocity = pc98::masterlib::vector2(angle, 7);
		missiles_add(state, elis.phase_1_rifts[i].left, elis.phase_1_rifts[i].top,
			pellet_pixels_to_subpixels(velocity.x), pellet_pixels_to_subpixels(velocity.y));
	}
	rifts_update(state, elis.phase_1_rifts, elis.girl, false, 60, 160);
	if(boss.phase_frame > 170) { boss.phase_frame = 0; return choose_new; }
	return 2;
}

[[nodiscard]] std::int16_t phase_one_circle(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	if(boss.phase_frame < 10) elis.circle_frame = 0;
	if(boss.phase_frame == 50) {
		elis.circle_angle = 0;
		elis.pattern_value = rank_value(state, static_cast<std::int16_t>(PelletGroup::one_aimed), static_cast<std::int16_t>(PelletGroup::one_random_narrow_aimed), static_cast<std::int16_t>(PelletGroup::three_spread_wide_aimed), static_cast<std::int16_t>(PelletGroup::five_spread_narrow_aimed));
	}
	if(circle_summon_step(elis, boss.phase_frame, 60)) return 3;
	if(elis.circle_frame > 0 && elis.circle_frame < 40) {
		elis.circle_frame = word(static_cast<std::int32_t>(elis.circle_frame) + 1);
		return 3;
	}
	if(elis.circle_frame >= 40) {
		std::uint8_t angle{};
		for(std::size_t i = 0U; i < 32U; ++i) {
			pellets_add_group(state, polar_x(word(static_cast<std::int32_t>(elis.girl.left) + 60), circle_radius, angle), polar_y(girl_center_top(elis), circle_radius, angle), static_cast<PelletGroup>(elis.pattern_value), 4);
			angle = static_cast<std::uint8_t>(angle + 8U);
		}
		boss.phase_frame = 0; elis.circle_frame = 0; return choose_new;
	}
	return 3;
}

[[nodiscard]] std::int16_t phase_one_pattern(State &state, std::int16_t id) noexcept
{
	if(id == 0) return static_cast<std::int16_t>(pc98::irand(state.random_seed) % 4U);
	if(id == 1) return phase_one_lasers(state);
	if(id == 2) return phase_one_missiles(state);
	if(id == 3) return phase_one_circle(state);
	return choose_new;
}

[[nodiscard]] std::int16_t phase_three_rings(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	if(boss.phase_frame == 10) elis.pattern_value = rank_value(state, 14, 16, 18, 20);
	std::int16_t speed{};
	std::uint8_t angle{};
	switch(boss.phase_frame) {
	case 10: speed = 48; angle = 0; break;
	case 16: speed = 54; angle = 2; break;
	case 24: speed = 64; angle = 4; break;
	case 32: speed = 72; angle = 6; break;
	case 40: speed = 80; angle = 8; break;
	default: break;
	}
	if(speed != 0) for(std::int16_t i = 0; i < elis.pattern_value; ++i) pellets_add_single(state,
		word(static_cast<std::int32_t>(girl_center_left(elis)) - 4), word(static_cast<std::int32_t>(girl_center_top(elis)) - 4),
		static_cast<std::uint8_t>(angle + static_cast<std::uint8_t>((0x100 / elis.pattern_value) * i)), speed);
	if(boss.phase_frame > 60) { boss.phase_frame = 0; return choose_new; }
	return 1;
}

[[nodiscard]] std::int16_t phase_three_spheres(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	if(boss.phase_frame >= 10 && boss.phase_frame <= 100 && (boss.phase_frame % 10) == 0) {
		const auto sphere_i = static_cast<std::size_t>((boss.phase_frame / 10) - 1);
		const auto edge = (elis.girl.left < 256) ? static_cast<std::int16_t>(playfield_right - 32) : playfield_left;
		const auto column = static_cast<std::int16_t>((edge - elis.girl.left) / 10);
		const auto row = static_cast<std::int16_t>(((static_cast<std::int16_t>(pc98::irand(state.random_seed) % 264U) + 264) - elis.girl.top) / 10);
		elis.spheres[sphere_i].left = word(static_cast<std::int32_t>(sphere_i + 1U) * column + girl_center_left(elis) - 16);
		elis.spheres[sphere_i].top = word(static_cast<std::int32_t>(sphere_i + 1U) * row + girl_center_top(elis) - 16);
	}
	if(boss.phase_frame > 100 && boss.phase_frame <= 230 && (boss.phase_frame % 10) == 0) {
		const auto sphere_i = static_cast<std::size_t>((boss.phase_frame - 100) / 10);
		if(sphere_i >= 1U && sphere_i <= 10U) {
			elis.pattern_value = rank_value(state, 5, 7, 9, 13);
			for(std::int16_t i = 0; i < elis.pattern_value; ++i) {
				const auto angle = static_cast<std::uint8_t>(pc98::irand(state.random_seed) & 0x7FU);
				const auto speed = static_cast<std::int16_t>((pc98::irand(state.random_seed) + 24U) & 63U);
				pellets_add_single(state, elis.spheres[sphere_i - 1U].left, elis.spheres[sphere_i - 1U].top, angle, speed);
			}
		}
	}
	if(boss.phase_frame > 250) { boss.phase_frame = 0; return choose_new; }
	return 2;
}

[[nodiscard]] std::int16_t phase_three_rifts(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	if(boss.phase_frame == 30) elis.pattern_value = rank_value(state, 6, 3, 2, 2);
	if(boss.phase_frame > 60 && boss.phase_frame < 120 && (boss.phase_frame % elis.pattern_value) == 0) {
		const auto i = static_cast<std::size_t>(pc98::irand(state.random_seed) % 5U);
		pellets_add_single(state, elis.phase_3_rifts[i].left, elis.phase_3_rifts[i].top,
			static_cast<std::uint8_t>((pc98::irand(state.random_seed) % 0x15U) + 0x36U), 96);
	} else if(boss.phase_frame >= 120 && boss.phase_frame < 200) {
		const auto i = static_cast<std::size_t>(pc98::irand(state.random_seed) % 5U);
		const auto source_angle = static_cast<std::uint8_t>((pc98::irand(state.random_seed) % 0x15U) + 0x36U);
		const auto offset = (pc98::irand(state.random_seed) & 1U) != 0U ? boss.phase_frame - 120 : 120 - boss.phase_frame;
		const auto angle = byte(static_cast<std::int32_t>(source_angle) * (offset / 2));
		pellets_add_single(state, elis.phase_3_rifts[i].left, elis.phase_3_rifts[i].top, angle, 96);
	}
	rifts_update(state, elis.phase_3_rifts, elis.girl, false, 60, 200);
	if(boss.phase_frame > 220) { boss.phase_frame = 0; return choose_new; }
	return 3;
}

[[nodiscard]] std::int16_t phase_three_pattern(State &state, std::int16_t id) noexcept
{
	auto &elis = state.boss.elis;
	if(id == 0) return static_cast<std::int16_t>(pc98::irand(state.random_seed) % 4U);
	if(elis.phase_3_pattern == star_summon) {
		if(star_of_david(state)) elis.phase_3_pattern = pattern_active;
		return id;
	}
	if(id == 1) return phase_three_rings(state);
	if(id == 2) return phase_three_spheres(state);
	if(id == 3) return phase_three_rifts(state);
	return choose_new;
}

[[nodiscard]] bool transform_girl_to_bat(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	static_cast<void>(pc98::irand(state.random_seed) % 16U);
	rifts_update(state, elis.transform_to_bat_rifts, elis.girl, false, 20, 100);
	if(boss.phase_frame <= 100) return false;
	elis.bat.left = word(static_cast<std::int32_t>(elis.girl.left) + ((girl_width - bat_width) / 2));
	elis.bat.top = word(static_cast<std::int32_t>(elis.girl.top) + ((girl_height - bat_height) / 2));
	boss.phase_frame = 0;
	return true;
}

[[nodiscard]] bool transform_bat_to_girl(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	static_cast<void>(pc98::irand(state.random_seed) % 16U);
	rifts_update(state, elis.transform_to_girl_rifts, elis.bat, true, 20, 100);
	if(boss.phase_frame <= 100) return false;
	auto left = word(static_cast<std::int32_t>(elis.bat.left) + ((bat_width - girl_width) / 2));
	auto top = word(static_cast<std::int32_t>(elis.bat.top) + ((bat_height - girl_height) / 2));
	if(left < playfield_left) left = playfield_left;
	if(left > (playfield_right - girl_width)) left = playfield_right - girl_width;
	if(top < playfield_top) top = playfield_top;
	elis.girl = {left, top};
	boss.phase_frame = 0;
	return true;
}

[[nodiscard]] std::int16_t abs16(std::int16_t value) noexcept
{
	return value < 0 ? word(-static_cast<std::int32_t>(value)) : value;
}

[[nodiscard]] bool bat_fly_random(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	if(boss.phase_frame < 3) { elis.bat_velocity_x = 0; elis.bat_velocity_y = 0; return false; }
	if(boss.phase_frame == 3) {
		do {
			elis.bat_target_left = random_girl_left(state);
			elis.bat_target_top = word(static_cast<std::int32_t>(random_girl_top(state)) + ((girl_height - bat_height) / 2));
		} while(abs16(word(static_cast<std::int32_t>(elis.bat.left) - elis.bat_target_left)) < (playfield_width / 5) &&
			abs16(word(static_cast<std::int32_t>(elis.bat.top) - elis.bat_target_top)) < ((playfield_height * 4) / 21));
		const auto angle = pc98::masterlib::iatan2_source(
			word(static_cast<std::int32_t>(elis.bat_target_top) - elis.bat.top),
			word(static_cast<std::int32_t>(elis.bat_target_left) - elis.bat.left)
		).value_or(0U);
		const auto velocity = pc98::masterlib::vector2(angle, 2);
		elis.bat_velocity_x = velocity.x;
		elis.bat_velocity_y = velocity.y;
		elis.bat_frames_until_target = static_cast<std::int16_t>(abs16(word(static_cast<std::int32_t>(elis.bat_target_left) - elis.bat.left)) / 2);
	}
	// C's truncation means this always adds 0, while still consuming IRAND.
	static_cast<void>(pc98::irand(state.random_seed) % 5U);
	if((boss.phase_frame / 3) >= elis.bat_frames_until_target) {
		elis.bat_velocity_x = 0; elis.bat_velocity_y = 0; boss.phase_frame = 0; return true;
	}
	return false;
}

void phase_five_bat_pattern(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	if(elis.phase_5_bat_pattern == choose_new) elis.phase_5_bat_pattern = static_cast<std::int16_t>((pc98::irand(state.random_seed) % 4U) + 1U);
	if(elis.phase_5_bat_pattern == 1 && (boss.phase_frame % 16) == 0) fire_from_form(state,
		static_cast<PelletGroup>(rank_value(state, static_cast<std::int16_t>(PelletGroup::three_spread_wide_aimed), static_cast<std::int16_t>(PelletGroup::five_spread_wide_aimed), static_cast<std::int16_t>(PelletGroup::five_spread_narrow_aimed), static_cast<std::int16_t>(PelletGroup::five_spread_narrow))), 56);
	if(elis.phase_5_bat_pattern == 2 && (boss.phase_frame % 4) == 0) fire_from_form(state,
		(pc98::irand(state.random_seed) & 1U) != 0U ? PelletGroup::two_spread_narrow_aimed : PelletGroup::two_spread_wide_aimed, 88);
	if(elis.phase_5_bat_pattern == 3 && (boss.phase_frame % 4) == 0) pellets_add_single(state,
		word(static_cast<std::int32_t>(bat_center_left(elis)) - 4), word(static_cast<std::int32_t>(bat_center_top(elis)) - 4),
		static_cast<std::uint8_t>(pc98::irand(state.random_seed)), 2, PelletMotion::gravity, 1);
	if(elis.phase_5_bat_pattern == 4) {
		if((boss.phase_frame % 50) < 10 && (boss.phase_frame % 4) == 0) fire_from_form(state, PelletGroup::two_spread_narrow_aimed, 85);
		if((boss.phase_frame % 50) >= 25 && (boss.phase_frame % 50) < 35 && (boss.phase_frame % 4) == 0) fire_from_form(state, PelletGroup::three_spread_narrow_aimed, 85);
	}
}

[[nodiscard]] bool phase_five_girl_pattern(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	if(elis.phase_5_girl_pattern == choose_new) { elis.phase_5_girl_pattern = static_cast<std::int16_t>((pc98::irand(state.random_seed) % 3U) + 1U); return false; }
	if(elis.pattern_subphase == star_summon) {
		if(star_of_david(state)) elis.pattern_subphase = pattern_active;
		return false;
	}
	if(elis.phase_5_girl_pattern == 1) {
		if(boss.phase_frame == 50) elis.pattern_value = rank_value(state, 2, 3, 3, 4);
		std::uint8_t angle{}; std::int16_t base{};
		switch(boss.phase_frame) { case 50: angle = 0x40U; base = elis.pattern_value; break; case 60: angle = 0x30U; base = elis.pattern_value; break; case 70: angle = 0x18U; base = elis.pattern_value; break; case 80: angle = 0; base = static_cast<std::int16_t>(elis.pattern_value + 2); break; default: break; }
		if(base != 0) {
			const auto count = boss.phase_frame == 80 ? 10 : 4;
			for(std::int16_t i = 0; i < count; ++i) {
				const auto right = static_cast<std::uint8_t>(angle + static_cast<std::uint8_t>(boss.phase_frame == 80 ? i * 6 : 0));
				for(const auto xoff : {-4, 4}) pellets_add_single(state,
					word(static_cast<std::int32_t>(girl_center_left(elis)) - 4 + xoff), word(static_cast<std::int32_t>(girl_center_top(elis)) - 8),
					xoff < 0 ? static_cast<std::uint8_t>(0x80U - right) : right, static_cast<std::int16_t>(base + i));
			}
		}
		if(boss.phase_frame >= 120) { boss.phase_frame = 0; return true; }
		return false;
	}
	if(elis.phase_5_girl_pattern == 2) {
		if(boss.phase_frame < 10) elis.circle_frame = 0;
		if(boss.phase_frame == 60) { elis.pattern_value = rank_value(state, 4, 2, 2, 2); elis.circle_angle = 0; elis.circle_target_left = state.player.left; }
		if(circle_summon_step(elis, boss.phase_frame, 60)) return false;
		if(elis.circle_angle >= 64U && elis.circle_frame != 0 && elis.circle_frame < 200) {
			elis.circle_frame = word(static_cast<std::int32_t>(elis.circle_frame) + 1);
			if(!state.player.invincible && (word(static_cast<std::int32_t>(elis.circle_target_left) + 104) <= state.player.left || word(static_cast<std::int32_t>(elis.circle_target_left) - 128) >= state.player.left)) state.input.player_is_hit = true;
			if(state.input.player_is_hit) elis.circle_frame = 200;
			if((boss.phase_frame % elis.pattern_value) == 0) pellets_add_group(state, static_cast<std::int16_t>(pc98::irand(state.random_seed) % 632U), playfield_top, state.rank == 3 ? PelletGroup::one_random_wide : PelletGroup::one, 72);
			if(elis.circle_frame > 180 && (elis.circle_frame % 4) == 0) fire_from_form(state, PelletGroup::one_aimed, 72);
		} else if(elis.circle_frame >= 200) {
			elis.circle_frame = word(static_cast<std::int32_t>(elis.circle_frame) + 1);
			if((elis.circle_frame % 8) == 0) pellets_add_group(state, word(static_cast<std::int32_t>(elis.bat.left) + 60), word(static_cast<std::int32_t>(elis.bat.top) + 44), PelletGroup::one_aimed, 72);
			if(elis.circle_frame > 260) { boss.phase_frame = 0; elis.circle_frame = 0; return true; }
		}
		return false;
	}
	if(elis.phase_5_girl_pattern == 3) {
		if(boss.phase_frame == 50) { elis.circle_angle = 0; elis.circle_frame = 0; elis.pattern_value = rank_value(state, 40, 30, 20, 15); }
		if((boss.phase_frame % elis.pattern_value) == 0) {
			pellets_add_group(state, word(static_cast<std::int32_t>(girl_center_left(elis)) - 42), word(static_cast<std::int32_t>(girl_center_top(elis)) - 8), PelletGroup::five_spread_wide_aimed, 64);
			pellets_add_group(state, word(static_cast<std::int32_t>(girl_center_left(elis)) + 36), word(static_cast<std::int32_t>(girl_center_top(elis)) - 8), PelletGroup::five_spread_wide_aimed, 64);
		}
		if((boss.phase_frame % (elis.pattern_value * 2)) == 0) shootout_laser_spawn_safe(state, static_cast<std::size_t>(boss.phase_frame / 10), word(static_cast<std::int32_t>(girl_center_left(elis)) - 4), word(static_cast<std::int32_t>(elis.girl.top) + 28), state.player.left, player_top + player_height, 50, 15U, 20, 8);
		if(circle_summon_step(elis, boss.phase_frame, 60)) return false;
		if(elis.circle_frame > 0 && elis.circle_frame < 40) {
			elis.circle_frame = word(static_cast<std::int32_t>(elis.circle_frame) + 1);
			return false;
		}
		if(elis.circle_frame >= 40) {
			std::uint8_t angle{}; for(std::size_t i = 0U; i < 32U; ++i) { pellets_add_group(state, polar_x(girl_center_left(elis), circle_radius, angle), polar_y(word(static_cast<std::int32_t>(elis.girl.top) + 28), circle_radius, angle), PelletGroup::one, 64); angle = static_cast<std::uint8_t>(angle + 8U); }
			boss.phase_frame = 0; elis.circle_frame = 0; return true;
		}
	}
	return false;
}

void defeat(State &state) noexcept
{
	pellets_unput_and_reset_nonclouds(state);
	missiles_reset(state);
	shootout_lasers_reset_first(state, 10U);
	state.stage_cleared = true;
	state.input.player_is_hit = true;
	state.boss.active = false;
}

} // namespace

void elis_setup(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	elis = {};
	boss.id = BossId::elis;
	boss.active = true;
	boss.hp = 14;
	// The source's phase-0 entrance is a blocking presentation-only sequence.
	// Its first combat seam is phase 1, pattern 1, with no intervening RNG.
	boss.phase = 1;
	boss.phase_frame = 0;
	elis.girl = {256, 96};
	elis.attack = elis.girl;
	elis.bat = elis.girl;
	elis.form = girl;
	elis.phase_pattern = 1;
	elis.phase_3_pattern = star_summon;
	elis.phase_5_subphase = phase_five_pattern;
	elis.pattern_subphase = star_summon;
	state.random_seed = state.frame_rand;
	state.projectiles.pellet_interlace = true;
	state.projectiles.pellet_unknown_seven = 7;
}

[[nodiscard]] BossUpdateResult elis_update(State &state) noexcept
{
	auto &boss = state.boss;
	auto &elis = boss.elis;
	if(!boss.active || boss.id != BossId::elis) return BossUpdateResult::unsupported;
	missiles_update(state);
	if(boss.phase == 1 || boss.phase == 3) {
		phase_frame_common(state, boss.phase);
		if(elis.phase_pattern != choose_new) {
			elis.phase_pattern = (boss.phase == 1) ? phase_one_pattern(state, elis.phase_pattern) : phase_three_pattern(state, elis.phase_pattern);
		} else if(!elis.teleport_done) {
			const auto left = random_girl_left(state);
			const auto top = random_girl_top(state);
			elis.teleport_done = wave_teleport(state, left, top);
		} else {
			elis.phase_pattern = (boss.phase == 1) ? phase_one_pattern(state, choose_new) : phase_three_pattern(state, choose_new);
			elis.teleport_done = false;
		}
		hit_update(state);
		if(!elis.invincible && elis.teleport_done && boss.hp <= (boss.phase == 1 ? 10 : 6)) {
			boss.phase = (boss.phase == 1) ? 2 : 4;
			boss.phase_frame = 0;
			elis.invincibility_frame = 0;
			elis.phase_pattern = choose_new;
			elis.teleport_done = false;
			if(boss.phase == 4) elis.form = girl;
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 2) {
		boss.phase_frame = word(static_cast<std::int32_t>(boss.phase_frame) + 1);
		elis.teleport_done = wave_teleport(state, 256, 96);
		if(elis.teleport_done) {
			boss.phase = 3; boss.phase_frame = 0; elis.invincibility_frame = 0;
			elis.phase_pattern = 1; elis.phase_3_pattern = star_summon; elis.teleport_done = false;
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 4) {
		boss.phase_frame = word(static_cast<std::int32_t>(boss.phase_frame) + 1);
		if(!elis.teleport_done) elis.teleport_done = wave_teleport(state, 256, 96);
		else if(transform_girl_to_bat(state)) {
			boss.phase = 5; boss.phase_frame = 0; elis.invincibility_frame = 0; elis.phase_pattern = choose_new; elis.teleport_done = false; elis.form = bat;
			elis.phase_5_subphase = phase_five_pattern; elis.phase_5_bat_pattern = choose_new; elis.phase_5_girl_pattern = choose_new; elis.pattern_subphase = star_summon;
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 5) {
		phase_frame_common(state, 5);
		if(elis.form == bat && elis.phase_5_subphase == phase_five_pattern && (boss.phase_frame % 3) == 0) {
			elis.bat.left = word(static_cast<std::int32_t>(elis.bat.left) + elis.bat_velocity_x);
			elis.bat.top = word(static_cast<std::int32_t>(elis.bat.top) + elis.bat_velocity_y);
		}
		if(elis.form == bat) {
			if(elis.phase_5_subphase == phase_five_pattern) {
				if(bat_fly_random(state)) elis.phase_5_subphase = phase_five_transform;
				phase_five_bat_pattern(state);
			} else if(transform_bat_to_girl(state)) {
				elis.form = girl; elis.phase_5_bat_pattern = choose_new; elis.phase_5_subphase = phase_five_pattern;
			}
		} else if(elis.phase_5_subphase == phase_five_pattern) {
			if(phase_five_girl_pattern(state)) { elis.phase_5_girl_pattern = choose_new; elis.pattern_subphase = star_summon; elis.phase_5_subphase = phase_five_transform; }
		} else if(transform_girl_to_bat(state)) {
			elis.form = bat; elis.phase_5_subphase = phase_five_pattern;
		}
		hit_update(state);
		if(boss.hp <= 0) { defeat(state); return BossUpdateResult::stage_cleared; }
		return BossUpdateResult::running;
	}
	return BossUpdateResult::unsupported;
}

void elis_render_tail(State &) noexcept
{
	// b15m's remaining work is GRP/GRC/shape/palette presentation.  Its RNG,
	// collision, pool updates, and all source projectile calls remain in update.
}

} // namespace th01::core
