#include "th01/core/boss_yuugenmagan.hpp"

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
constexpr std::int16_t playfield_center_x = 320;
constexpr std::int16_t playfield_center_y = 232;
constexpr std::int16_t player_width = 32;
constexpr std::int16_t player_height = 32;
constexpr std::int16_t player_top = 368;
constexpr std::int16_t eye_width = 64;
constexpr std::int16_t eye_height = 48;
constexpr std::int16_t missile_width = 16;
constexpr std::int16_t missile_height = 16;
constexpr std::int16_t eye_count = 5;
constexpr std::int16_t pentagram_interval = 4;
constexpr std::int16_t pentagram_radius_final = 64;
constexpr std::int16_t pentagram_regular_center_top = 189;
constexpr std::int16_t pentagram_shrink_distance = 160;
constexpr std::int16_t subphase_prepare_frames = 100;
constexpr std::int16_t subphase_timeout_frames = 200;
constexpr std::int16_t eye_toggle_frames = 70;
constexpr std::uint8_t eye_west = 1U << 0U;
constexpr std::uint8_t eye_east = 1U << 1U;
constexpr std::uint8_t eye_southwest = 1U << 2U;
constexpr std::uint8_t eye_southeast = 1U << 3U;
constexpr std::uint8_t eye_north = 1U << 4U;
constexpr std::uint8_t eye_all = static_cast<std::uint8_t>(
	eye_west | eye_east | eye_southwest | eye_southeast | eye_north
);

enum EyeCel : std::uint8_t {
	eye_hidden,
	eye_closed,
	eye_halfopen,
	eye_down,
	eye_left,
	eye_right,
	eye_ahead,
};

enum PentagramPhase : std::uint8_t {
	pentagram_prepare_1,
	pentagram_spin_clockwise_1,
	pentagram_spin_counterclockwise_1,
	pentagram_spin_clockwise_2,
	pentagram_spin_counterclockwise_2,
	pentagram_spin_clockwise_3,
	pentagram_slam_1,
	pentagram_prepare_2,
	pentagram_grow,
	pentagram_slam_2,
};

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

[[nodiscard]] constexpr std::size_t eye_index(std::uint8_t bit) noexcept
{
	switch(bit) {
	case eye_west: return 0U;
	case eye_east: return 1U;
	case eye_southwest: return 2U;
	case eye_southeast: return 3U;
	default: return 4U;
	}
}

[[nodiscard]] YuugenmaganEyeState &eye(YuugenmaganState &yuugen, std::uint8_t bit) noexcept
{
	return yuugen.eyes[eye_index(bit)];
}

[[nodiscard]] const YuugenmaganEyeState &eye(
	const YuugenmaganState &yuugen, std::uint8_t bit
) noexcept
{
	return yuugen.eyes[eye_index(bit)];
}

[[nodiscard]] constexpr std::int16_t eye_center_left(const YuugenmaganEyeState &value) noexcept
{
	return word(static_cast<std::int32_t>(value.left) + (eye_width / 2));
}

[[nodiscard]] constexpr std::int16_t eye_center_top(const YuugenmaganEyeState &value) noexcept
{
	return word(static_cast<std::int32_t>(value.top) + (eye_height / 2));
}

[[nodiscard]] constexpr std::int16_t eye_offcenter_left(const YuugenmaganEyeState &value) noexcept
{
	return word(static_cast<std::int32_t>(eye_center_left(value)) - 4);
}

[[nodiscard]] constexpr std::int16_t eye_iris_top(const YuugenmaganEyeState &value) noexcept
{
	return word(static_cast<std::int32_t>(eye_center_top(value)) + 4);
}

[[nodiscard]] constexpr std::int16_t eye_iris_center_top(const YuugenmaganEyeState &value) noexcept
{
	return word(static_cast<std::int32_t>(eye_iris_top(value)) + 4);
}

[[nodiscard]] constexpr std::int16_t eye_pupil_bottom(const YuugenmaganEyeState &value) noexcept
{
	return word(static_cast<std::int32_t>(eye_center_top(value)) + 12);
}

void eyes_set_image(YuugenmaganState &yuugen, std::uint8_t bits, EyeCel image) noexcept
{
	for(const auto bit : {eye_west, eye_east, eye_southwest, eye_southeast, eye_north}) {
		if((bits & bit) != 0U) eye(yuugen, bit).image = image;
	}
}

void eyes_set_orb_hitbox(YuugenmaganState &yuugen, std::uint8_t bits, bool active) noexcept
{
	for(const auto bit : {eye_west, eye_east, eye_southwest, eye_southeast, eye_north}) {
		if((bits & bit) != 0U) eye(yuugen, bit).orb_hitbox_active = active;
	}
}

void eye_track_player(const State &state, YuugenmaganEyeState &value) noexcept
{
	if((eye_center_left(value) - state.player.left) > (eye_width / 2)) {
		value.image = eye_left;
	} else if((state.player.left - eye_center_left(value)) > (eye_width / 2)) {
		value.image = eye_right;
	} else {
		value.image = eye_down;
	}
}

void eyes_track_player(State &state, std::uint8_t bits) noexcept
{
	auto &yuugen = state.boss.yuugenmagan;
	for(const auto bit : {eye_west, eye_east, eye_southwest, eye_southeast, eye_north}) {
		if((bits & bit) != 0U) eye_track_player(state, eye(yuugen, bit));
	}
}

[[nodiscard]] bool eye_orb_hittest(const State &state, const YuugenmaganEyeState &value) noexcept
{
	if(!value.orb_hitbox_active || value.image == eye_hidden) return false;
	// CBossEntitySized::hitbox_orb_set(-4, -4, EYE_W + 4, EYE_H), tested
	// with the 16x16 Orb centred at its previous coordinate.
	return state.orb.prev_left >= word(static_cast<std::int32_t>(value.left) - 28) &&
		state.orb.prev_left <= word(static_cast<std::int32_t>(value.left) + 60) &&
		state.orb.prev_top >= word(static_cast<std::int32_t>(value.top) - 28) &&
		state.orb.prev_top <= word(static_cast<std::int32_t>(value.top) + 40);
}

[[nodiscard]] bool eyes_orb_hittest(const State &state) noexcept
{
	for(const auto &value : state.boss.yuugenmagan.eyes) {
		if(eye_orb_hittest(state, value)) return true;
	}
	return false;
}

void hit_update(State &state) noexcept
{
	auto &yuugen = state.boss.yuugenmagan;
	// b10m leaves these four optional shot bounds at their source defaults.
	static_cast<void>(boss_hit_update(
		state, yuugen.invincibility_frame, yuugen.invincible,
		eyes_orb_hittest(state), 0, 0, 0, 0, 5000
	));
}

void frame_common(State &state) noexcept
{
	auto &boss = state.boss;
	auto &yuugen = boss.yuugenmagan;
	boss.phase_frame = word(static_cast<std::int32_t>(boss.phase_frame) + 1);
	yuugen.invincibility_frame = word(static_cast<std::int32_t>(yuugen.invincibility_frame) + 1);
}

void phase_next(State &state, std::int8_t next) noexcept
{
	auto &boss = state.boss;
	boss.phase = next;
	boss.phase_frame = 0;
	boss.yuugenmagan.invincibility_frame = 0;
}

void fire_from_iris(
	State &state, const YuugenmaganEyeState &value, PelletGroup group, std::int16_t speed
) noexcept
{
	pellets_add_group(
		state,
		word(static_cast<std::int32_t>(eye_center_left(value)) - 4),
		word(static_cast<std::int32_t>(eye_iris_center_top(value)) - 4), group, speed
	);
}

void fire_from_bottom(
	State &state, const YuugenmaganEyeState &value, PelletGroup group, std::int16_t speed
) noexcept
{
	pellets_add_group(
		state,
		word(static_cast<std::int32_t>(eye_center_left(value)) - 4),
		word(static_cast<std::int32_t>(eye_pupil_bottom(value)) - 4), group, speed
	);
}

void fire_single_from_bottom(
	State &state,
	const YuugenmaganEyeState &value,
	bool mirror,
	std::uint8_t angle,
	PelletMotion motion = PelletMotion::regular,
	std::int16_t fixed_speed = 0
) noexcept
{
	if(mirror) angle = static_cast<std::uint8_t>(0x80U - angle);
	pellets_add_single(
		state,
		word(static_cast<std::int32_t>(eye_center_left(value)) - 4),
		word(static_cast<std::int32_t>(eye_pupil_bottom(value)) - 4),
		angle, 64, motion, fixed_speed
	);
}

void phase_zero_eye_open(YuugenmaganEyeState &value, std::int16_t frame, std::int16_t first) noexcept
{
	const auto elapsed = static_cast<std::int16_t>(frame - first);
	value.image = (elapsed < 20) ? eye_closed :
		(elapsed < 40) ? eye_halfopen : (elapsed < 60) ? eye_ahead : eye_down;
}

void phase_zero_open(YuugenmaganState &yuugen, std::int16_t frame) noexcept
{
	if(frame >= 100 && frame <= 160) {
		phase_zero_eye_open(eye(yuugen, eye_west), frame, 100);
		phase_zero_eye_open(eye(yuugen, eye_east), frame, 100);
	}
	if(frame >= 120 && frame <= 180) {
		phase_zero_eye_open(eye(yuugen, eye_southwest), frame, 120);
		phase_zero_eye_open(eye(yuugen, eye_southeast), frame, 120);
	}
	if(frame >= 140 && frame <= 200) phase_zero_eye_open(eye(yuugen, eye_north), frame, 140);
}

void phase_zero_close(YuugenmaganState &yuugen, std::int16_t frame) noexcept
{
	eyes_set_image(yuugen, eye_all, (frame == 240) ? eye_halfopen :
		(frame == 260) ? eye_closed : eye_hidden);
}

void phase_zero_lasers(State &state) noexcept
{
	const auto &yuugen = state.boss.yuugenmagan;
	const auto frame = state.boss.phase_frame;
	if(frame <= 160) return;
	auto hit = [&](const YuugenmaganEyeState &value, std::int16_t player_w) noexcept {
		return state.player.left > word(static_cast<std::int32_t>(eye_offcenter_left(value)) -
			((eye_width - (eye_width / 4)) / 2)) &&
			state.player.left < word(static_cast<std::int32_t>(eye_offcenter_left(value)) +
				((eye_width - (eye_width / 4)) / 2) - player_w);
	};
	bool player_hit{};
	if(frame < 300) player_hit = hit(eye(yuugen, eye_west), player_width) ||
		hit(eye(yuugen, eye_east), player_width / 4);
	if(frame > 180 && frame < 320) player_hit = player_hit ||
		hit(eye(yuugen, eye_southwest), player_width) ||
		hit(eye(yuugen, eye_southeast), player_width);
	if(frame > 200) player_hit = player_hit || hit(eye(yuugen, eye_north), player_width);
	if(player_hit && !state.player.invincible) state.input.player_is_hit = true;
}

void phase_one_pellets(State &state) noexcept
{
	const auto &yuugen = state.boss.yuugenmagan;
	const auto interval = yuugen.pattern_interval;
	if(interval == 0) return;
	const auto part = static_cast<std::int16_t>(state.boss.phase_frame % interval);
	std::int16_t speed{};
	PelletGroup group{PelletGroup::two_spread_wide_aimed};
	switch(part) {
	case 10: speed = 24; break;
	case 25: speed = 28; break;
	case 40: speed = 32; break;
	case 60: speed = 44; break;
	case 80: speed = 50; break;
	case 110: speed = 68; group = PelletGroup::one_aimed; break;
	default: return;
	}
	fire_from_bottom(state, eye(yuugen, eye_west), group, speed);
	fire_from_bottom(state, eye(yuugen, eye_east), group, speed);
}

[[nodiscard]] std::uint8_t missile_angle(
	const YuugenmaganEyeState &value, std::int16_t target_left
) noexcept
{
	return pc98::masterlib::iatan2_source(
		word(static_cast<std::int32_t>(player_top + player_height) - value.top + 28),
		word(static_cast<std::int32_t>(target_left) - value.left + 28)
	).value_or(0U);
}

void fire_missile_pair(State &state) noexcept
{
	auto &yuugen = state.boss.yuugenmagan;
	for(const auto bit : {eye_southwest, eye_southeast}) {
		const auto velocity = pc98::masterlib::vector2(
			(bit == eye_southwest) ? yuugen.missile_angle_southwest : yuugen.missile_angle_southeast,
			8
		);
		const auto &value = eye(yuugen, bit);
		missiles_add(
			state,
			word(static_cast<std::int32_t>(value.left) + 28),
			word(static_cast<std::int32_t>(value.top) + 28),
			pellet_pixels_to_subpixels(velocity.x), pellet_pixels_to_subpixels(velocity.y)
		);
	}
}

void pattern_missiles(State &state, std::int16_t variations, bool clock) noexcept
{
	auto &boss = state.boss;
	auto &yuugen = boss.yuugenmagan;
	eyes_track_player(state, static_cast<std::uint8_t>(eye_southwest | eye_southeast));
	if(boss.phase_frame >= 90 && (yuugen.phase_u2 % 4) == 0) {
		eyes_set_image(yuugen, static_cast<std::uint8_t>(eye_southwest | eye_southeast), eye_closed);
	}
	const auto subphase = yuugen.phase_u2;
	const auto modulo = static_cast<std::int16_t>(subphase % 4);
	if(boss.phase_frame == 100 || modulo == 1) {
		if(subphase == 0) {
			yuugen.phase_u2 = word(static_cast<std::int32_t>(pc98::irand(state.random_seed) %
				static_cast<std::uint16_t>(variations)) * 4 + 1);
			yuugen.target_left = word(static_cast<std::int32_t>(state.player.left) - (missile_width / 2));
			yuugen.missile_angle_southwest = static_cast<std::uint8_t>(
				missile_angle(eye(yuugen, eye_southwest), yuugen.target_left) - 0x10U
			);
			yuugen.missile_angle_southeast = static_cast<std::uint8_t>(
				missile_angle(eye(yuugen, eye_southeast), yuugen.target_left) + 0x10U
			);
		}
		if((boss.phase_frame % 8) == 0) fire_missile_pair(state);
		if(boss.phase_frame == 200) {
			yuugen.phase_u2 = word(static_cast<std::int32_t>(yuugen.phase_u2) + 1);
			yuugen.phase_u1 = 0;
		}
	}
	if((yuugen.phase_u2 % 4) == 2 && (boss.phase_frame % 8) == 1) {
		yuugen.phase_u1 = word(static_cast<std::int32_t>(yuugen.phase_u1) + 1);
		if(clock) {
			if((yuugen.phase_u2 / 4) == 0) {
				yuugen.missile_angle_southwest = static_cast<std::uint8_t>(yuugen.missile_angle_southwest - 2U);
				yuugen.missile_angle_southeast = static_cast<std::uint8_t>(yuugen.missile_angle_southeast - 2U);
			} else {
				yuugen.missile_angle_southwest = static_cast<std::uint8_t>(yuugen.missile_angle_southwest + 2U);
				yuugen.missile_angle_southeast = static_cast<std::uint8_t>(yuugen.missile_angle_southeast + 2U);
			}
		} else {
			yuugen.missile_angle_southwest = static_cast<std::uint8_t>(yuugen.missile_angle_southwest - 2U);
			yuugen.missile_angle_southeast = static_cast<std::uint8_t>(yuugen.missile_angle_southeast + 2U);
		}
		fire_missile_pair(state);
		if(yuugen.phase_u1 > 10) {
			yuugen.phase_u2 = word(static_cast<std::int32_t>(yuugen.phase_u2) + 1);
			yuugen.phase_u1 = 0;
		}
	} else if((yuugen.phase_u2 % 4) == 3 && (boss.phase_frame % 8) == 1) {
		yuugen.phase_u1 = word(static_cast<std::int32_t>(yuugen.phase_u1) + 1);
		const auto variation = static_cast<std::int16_t>(yuugen.phase_u2 / 4);
		if(clock) {
			const auto step = (variation == 0) ? 3 : -3;
			yuugen.missile_angle_southwest = byte(static_cast<std::int32_t>(yuugen.missile_angle_southwest) + step);
			yuugen.missile_angle_southeast = byte(static_cast<std::int32_t>(yuugen.missile_angle_southeast) + step);
		} else if(variation == 0) {
			yuugen.missile_angle_southwest = static_cast<std::uint8_t>(yuugen.missile_angle_southwest + 4U);
		} else if(variation == 1) {
			yuugen.missile_angle_southeast = static_cast<std::uint8_t>(yuugen.missile_angle_southeast - 4U);
		} else {
			yuugen.missile_angle_southwest = static_cast<std::uint8_t>(yuugen.missile_angle_southwest + 2U);
			yuugen.missile_angle_southeast = static_cast<std::uint8_t>(yuugen.missile_angle_southeast - 2U);
		}
		fire_missile_pair(state);
		if(yuugen.phase_u1 > 10) {
			yuugen.phase_u2 = 0;
			yuugen.phase_u1 = 0;
			boss.phase_frame = 0;
			yuugen.phase_u3 = static_cast<std::uint8_t>(yuugen.phase_u3 + 1U);
		}
	}
}

[[nodiscard]] constexpr std::int16_t laser_right(std::int16_t edge, bool leftward) noexcept
{
	return leftward ? word(static_cast<std::int32_t>(playfield_right) - edge) : edge;
}

void laser_line(State &state, std::int16_t edge, bool leftward) noexcept
{
	const auto &north = eye(state.boss.yuugenmagan, eye_north);
	const auto right = laser_right(edge, leftward);
	const auto previous = word(static_cast<std::int32_t>(right) + (leftward ? 16 : -16));
	linebullet_unput(state, eye_offcenter_left(north), eye_iris_top(north), previous, playfield_bottom);
	linebullet_put_patterned_and_hittest(
		state, eye_offcenter_left(north), eye_iris_top(north), right, playfield_bottom, 15U, 0xE0U
	);
}

void pattern_lasers(State &state) noexcept
{
	auto &boss = state.boss;
	auto &yuugen = boss.yuugenmagan;
	const auto lsp = yuugen.phase_u2;
	if(boss.phase_frame >= 30 && (lsp == 0 || lsp == 2 || lsp == 4)) {
		eye(yuugen, eye_north).image = eye_closed;
	}
	if(lsp == 0 && boss.phase_frame == 40) yuugen.phase_u2 = 1;
	if(lsp == 2 && boss.phase_frame == 40) yuugen.phase_u2 = 3;
	if(lsp == 4 && boss.phase_frame == 40) yuugen.phase_u2 = 5;
	if((boss.phase_frame % 4) != 1) return;
	if(yuugen.phase_u2 == 1 || yuugen.phase_u2 == 3) {
		const auto leftward = yuugen.phase_u2 == 3;
		const auto edge = yuugen.phase_u1;
		auto &north = eye(yuugen, eye_north);
		north.image = (edge <= (playfield_center_x - eye_width)) ?
			(leftward ? eye_right : eye_left) : (edge >= (playfield_center_x + eye_width)) ?
				(leftward ? eye_left : eye_right) : eye_down;
		laser_line(state, edge, leftward);
		if(edge >= (playfield_right - (player_width + (player_width / 2)))) {
			linebullet_unput(state, eye_offcenter_left(north), eye_iris_top(north), laser_right(edge, leftward), playfield_bottom);
			yuugen.phase_u2 = word(static_cast<std::int32_t>(yuugen.phase_u2) + 1);
			boss.phase_frame = 0;
			yuugen.phase_u1 = 0;
			if(state.rank > 1) fire_from_iris(state, north, PelletGroup::three_spread_narrow_aimed, 3);
		}
		yuugen.phase_u1 = word(static_cast<std::int32_t>(yuugen.phase_u1) + 16);
		return;
	}
	if(yuugen.phase_u2 == 5) {
		auto &north = eye(yuugen, eye_north);
		north.image = eye_ahead;
		laser_line(state, yuugen.phase_u1, false);
		laser_line(state, yuugen.phase_u1, true);
		if(yuugen.phase_u1 >= (playfield_center_x - player_width)) {
			linebullet_unput(state, eye_offcenter_left(north), eye_iris_top(north), laser_right(yuugen.phase_u1, false), playfield_bottom);
			linebullet_unput(state, eye_offcenter_left(north), eye_iris_top(north), laser_right(yuugen.phase_u1, true), playfield_bottom);
			yuugen.phase_u2 = 6;
			boss.phase_frame = 0;
			yuugen.phase_u1 = 0;
			if(state.rank == 2) fire_from_iris(state, north, PelletGroup::three_spread_narrow_aimed, 3);
			if(state.rank == 3) fire_from_iris(state, north, PelletGroup::five_spread_narrow_aimed, 4);
		}
		yuugen.phase_u1 = word(static_cast<std::int32_t>(yuugen.phase_u1) + 16);
	}
}

[[nodiscard]] std::int16_t polar_x(std::int16_t center, std::int16_t radius, std::uint8_t angle) noexcept
{
	return word(static_cast<std::int32_t>(center) + pc98::masterlib::vector2(angle, radius).x);
}

[[nodiscard]] std::int16_t polar_y(std::int16_t center, std::int16_t radius, std::uint8_t angle) noexcept
{
	return word(static_cast<std::int32_t>(center) + pc98::masterlib::vector2(angle, radius).y);
}

void pentagram_set_regular(YuugenmaganPentagramState &pentagram, std::uint8_t angle) noexcept
{
	for(std::size_t i = 0U; i < pentagram.left.size(); ++i) {
		const auto point_angle = static_cast<std::uint8_t>(angle + static_cast<std::uint8_t>(i * 0x33U));
		pentagram.left[i] = polar_x(pentagram.center_left, pentagram.radius, point_angle);
		pentagram.top[i] = polar_y(pentagram.center_top, pentagram.radius, point_angle);
	}
}

void pentagram_unput(State &state, const YuugenmaganPentagramState &pentagram) noexcept
{
	for(const auto pair : std::array<std::array<std::size_t, 2U>, 5U>{{{{0U, 2U}}, {{2U, 4U}}, {{4U, 1U}}, {{1U, 3U}}, {{3U, 0U}}}}) {
		linebullet_unput(state, pentagram.left[pair[0]], pentagram.top[pair[0]], pentagram.left[pair[1]], pentagram.top[pair[1]]);
	}
}

void pentagram_put_and_hittest(State &state, const YuugenmaganPentagramState &pentagram) noexcept
{
	for(const auto pair : std::array<std::array<std::size_t, 2U>, 5U>{{{{0U, 2U}}, {{2U, 4U}}, {{4U, 1U}}, {{1U, 3U}}, {{3U, 0U}}}}) {
		linebullet_put_and_hittest(state, pentagram.left[pair[0]], pentagram.top[pair[0]], pentagram.left[pair[1]], pentagram.top[pair[1]], 15U);
	}
}

void pentagram_regular_update(State &state) noexcept
{
	auto &pentagram = state.boss.yuugenmagan.pentagram;
	pentagram_unput(state, pentagram);
	pentagram_set_regular(pentagram, pentagram.angle);
	pentagram_put_and_hittest(state, pentagram);
}

void pentagram_aim(State &state) noexcept
{
	auto &pentagram = state.boss.yuugenmagan.pentagram;
	const auto velocity = pc98::masterlib::vector2(
		pc98::masterlib::iatan2_source(
			word(static_cast<std::int32_t>(player_top + player_height) - pentagram.center_top),
			word(static_cast<std::int32_t>(state.player.left) - pentagram.center_left)
		).value_or(0U), 4
	);
	pentagram.velocity_x = velocity.x;
	pentagram.velocity_y = velocity.y;
}

void pentagram_spin(YuugenmaganPentagramState &pentagram, std::int16_t frame, bool clockwise) noexcept
{
	const auto delta = static_cast<std::int16_t>((frame / 64) + 2);
	pentagram.angle = byte(static_cast<std::int32_t>(pentagram.angle) + (clockwise ? delta : -delta));
}

void pentagram_slam(State &state, std::int16_t threshold, bool move_first) noexcept
{
	auto &pentagram = state.boss.yuugenmagan.pentagram;
	if(pentagram.center_top < (playfield_bottom - threshold)) pentagram_aim(state);
	if(move_first) {
		pentagram.center_top = word(static_cast<std::int32_t>(pentagram.center_top) + pentagram.velocity_y);
		pentagram.center_left = word(static_cast<std::int32_t>(pentagram.center_left) + pentagram.velocity_x);
	}
	pentagram_spin(pentagram, state.boss.phase_frame, true);
	if(!move_first) {
		pentagram.center_top = word(static_cast<std::int32_t>(pentagram.center_top) + pentagram.velocity_y);
		pentagram.center_left = word(static_cast<std::int32_t>(pentagram.center_left) + pentagram.velocity_x);
	}
	pentagram_regular_update(state);
}

void phase_toggled_eyes(State &state, std::uint8_t close, std::uint8_t open) noexcept
{
	auto &boss = state.boss;
	auto &yuugen = boss.yuugenmagan;
	const auto frame = word(static_cast<std::int32_t>(boss.phase_frame) + 1);
	boss.phase_frame = frame;
	if(frame == 20) {
		eyes_set_image(yuugen, close, eye_halfopen);
		eyes_set_image(yuugen, open, eye_closed);
		eyes_set_orb_hitbox(yuugen, open, true);
	} else if(frame == 40) {
		eyes_set_image(yuugen, close, eye_closed);
		eyes_set_image(yuugen, open, eye_halfopen);
		eyes_set_orb_hitbox(yuugen, close, false);
	} else if(frame == 60) {
		eyes_set_image(yuugen, close, eye_hidden);
		eyes_set_image(yuugen, open, eye_ahead);
	}
}

void eyes_toggle_step(
	YuugenmaganState &yuugen,
	std::uint8_t close,
	std::uint8_t open,
	std::int16_t &frame
) noexcept
{
	frame = word(static_cast<std::int32_t>(frame) + 1);
	if(frame == 20) {
		eyes_set_image(yuugen, close, eye_halfopen);
		eyes_set_image(yuugen, open, eye_closed);
		eyes_set_orb_hitbox(yuugen, open, true);
	} else if(frame == 40) {
		eyes_set_image(yuugen, close, eye_closed);
		eyes_set_image(yuugen, open, eye_halfopen);
		eyes_set_orb_hitbox(yuugen, close, false);
	} else if(frame == 60) {
		eyes_set_image(yuugen, close, eye_hidden);
		eyes_set_image(yuugen, open, eye_ahead);
	}
}

[[nodiscard]] std::int16_t pentagram_shrink_left(
	const YuugenmaganState &yuugen, std::uint8_t bit, std::int16_t distance
) noexcept
{
	const auto &value = eye(yuugen, bit);
	if(bit == eye_west) return word(static_cast<std::int32_t>(eye_offcenter_left(value)) + distance);
	if(bit == eye_east) return word(static_cast<std::int32_t>(eye_offcenter_left(value)) - distance);
	if(bit == eye_southwest) return word(static_cast<std::int32_t>(eye_offcenter_left(value)) + (distance / 3));
	if(bit == eye_southeast) return word(static_cast<std::int32_t>(eye_offcenter_left(value)) - (distance / 3));
	return eye_offcenter_left(value);
}

[[nodiscard]] std::int16_t pentagram_shrink_top(
	const YuugenmaganState &yuugen, std::uint8_t bit, std::int16_t distance
) noexcept
{
	const auto &value = eye(yuugen, bit);
	if(bit == eye_north) return word(static_cast<std::int32_t>(eye_iris_top(value)) + (distance / 2));
	return word(static_cast<std::int32_t>(eye_iris_top(value)) + (distance / 3));
}

void pentagram_shrink_put(State &state, std::int16_t distance) noexcept
{
	const auto &yuugen = state.boss.yuugenmagan;
	constexpr std::array<std::array<std::uint8_t, 2U>, 5U> segments{{
		{{eye_north, eye_southwest}}, {{eye_southwest, eye_east}},
		{{eye_east, eye_west}}, {{eye_west, eye_southeast}}, {{eye_southeast, eye_north}},
	}};
	for(const auto &segment : segments) {
		linebullet_put_and_hittest(state,
			pentagram_shrink_left(yuugen, segment[0], distance), pentagram_shrink_top(yuugen, segment[0], distance),
			pentagram_shrink_left(yuugen, segment[1], distance), pentagram_shrink_top(yuugen, segment[1], distance), 15U);
	}
}

void pentagram_shrink_unput(State &state, std::int16_t distance) noexcept
{
	const auto &yuugen = state.boss.yuugenmagan;
	constexpr std::array<std::array<std::uint8_t, 2U>, 5U> segments{{
		{{eye_north, eye_southwest}}, {{eye_southwest, eye_east}},
		{{eye_east, eye_west}}, {{eye_west, eye_southeast}}, {{eye_southeast, eye_north}},
	}};
	for(const auto &segment : segments) {
		linebullet_unput(state,
			pentagram_shrink_left(yuugen, segment[0], distance), pentagram_shrink_top(yuugen, segment[0], distance),
			pentagram_shrink_left(yuugen, segment[1], distance), pentagram_shrink_top(yuugen, segment[1], distance));
	}
}

void defeat(State &state, bool reset_missiles_too) noexcept
{
	pellets_unput_and_reset_nonclouds(state);
	if(reset_missiles_too) missiles_reset(state);
	shootout_lasers_reset_first(state, 5U);
	state.stage_cleared = true;
	state.input.player_is_hit = true;
	state.boss.active = false;
}

void phase_thirteen(State &state) noexcept
{
	auto &boss = state.boss;
	auto &yuugen = boss.yuugenmagan;
	auto &pentagram = yuugen.pentagram;
	frame_common(state);
	yuugen.phase_u1 = word(static_cast<std::int32_t>(yuugen.phase_u1) + 1);
	if(yuugen.invincible && yuugen.after_hit_frame == 0) {
		yuugen.phase_u3 = static_cast<std::uint8_t>(yuugen.phase_u3 << 1U);
		if(yuugen.phase_u3 == 32U) yuugen.phase_u3 = eye_west;
		yuugen.after_hit_frame = 1;
	} else if(yuugen.after_hit_frame > 0) {
		const auto close = yuugen.phase_u3 == eye_west ? eye_north : static_cast<std::uint8_t>(yuugen.phase_u3 >> 1U);
		eyes_toggle_step(yuugen, close, yuugen.phase_u3, yuugen.after_hit_frame);
		if(yuugen.after_hit_frame >= eye_toggle_frames) {
			yuugen.after_hit_frame = 0;
			yuugen.phase_u1 = 0;
		}
	} else {
		eyes_track_player(state, yuugen.phase_u3);
	}
	if(yuugen.phase_u2 == 0 && yuugen.pattern_interval != 0 &&
		(boss.phase_frame % yuugen.pattern_interval) == 0 &&
		pentagram.phase != pentagram_slam_1 && pentagram.phase != pentagram_slam_2) {
		for(std::size_t i = 0U; i < pentagram.left.size(); ++i) {
			pellets_add_single(
				state, pentagram.left[i], pentagram.top[i],
				pc98::masterlib::iatan2_source(
					word(static_cast<std::int32_t>(pentagram.top[i]) - pentagram.center_top),
					word(static_cast<std::int32_t>(pentagram.left[i]) - pentagram.center_left)
				).value_or(0U), 48
			);
		}
	}
	if((pentagram.phase == pentagram_prepare_1 || pentagram.phase == pentagram_prepare_2) && boss.phase_frame == 100) {
		pentagram.phase = static_cast<std::uint8_t>(pentagram.phase + 1U);
		boss.phase_frame = 0;
	}
	if(pentagram.phase == pentagram_spin_clockwise_1 || pentagram.phase == pentagram_spin_clockwise_2 ||
		pentagram.phase == pentagram_spin_clockwise_3 || pentagram.phase == pentagram_spin_counterclockwise_1 ||
		pentagram.phase == pentagram_spin_counterclockwise_2) {
		if((boss.phase_frame % pentagram_interval) == 0) {
			const auto clockwise = pentagram.phase == pentagram_spin_clockwise_1 ||
				pentagram.phase == pentagram_spin_clockwise_2 || pentagram.phase == pentagram_spin_clockwise_3;
			pentagram_spin(pentagram, boss.phase_frame, clockwise);
			pentagram_regular_update(state);
		}
		if(boss.phase_frame >= 200) {
			pentagram.phase = static_cast<std::uint8_t>(pentagram.phase + 1U);
			boss.phase_frame = 0;
			if(pentagram.phase == pentagram_slam_1) pentagram_aim(state);
		}
	} else if(pentagram.phase == pentagram_slam_1) {
		if((boss.phase_frame % pentagram_interval) == 0) {
			pentagram_slam(state, player_height + 4 + pentagram_radius_final, false);
		}
		if(pentagram.center_top >= 420) {
			pentagram_unput(state, pentagram);
			boss.phase = 10;
			boss.phase_frame = 0;
			yuugen.invincibility_frame = 0;
			yuugen.phase_u2 = 3;
			pentagram.phase = pentagram_prepare_2;
			yuugen.phase_u1 = 0;
		}
	} else if(pentagram.phase == pentagram_grow) {
		if((boss.phase_frame % pentagram_interval) == 0) {
			pentagram.radius = word(static_cast<std::int32_t>(pentagram.radius) + 1);
			pentagram_spin(pentagram, boss.phase_frame, true);
			pentagram_regular_update(state);
		}
		if(boss.phase_frame >= 200) {
			pentagram.phase = pentagram_slam_2;
			boss.phase_frame = 0;
			pentagram_aim(state);
		}
	} else if(pentagram.phase == pentagram_slam_2) {
		if((boss.phase_frame % pentagram_interval) == 0) {
			pentagram_slam(state, 64 + (player_height - 6), true);
		}
		if(pentagram.center_top >= 420) {
			pentagram_unput(state, pentagram);
			boss.phase = 10;
			boss.phase_frame = 0;
			yuugen.invincibility_frame = 0;
			yuugen.phase_u2 = 3;
			pentagram.phase = pentagram_prepare_1;
			yuugen.phase_u1 = 0;
		}
	}
	hit_update(state);
	if(boss.hp <= 0) defeat(state, true);
}

} // namespace

void yuugenmagan_setup(State &state) noexcept
{
	auto &boss = state.boss;
	auto &yuugen = boss.yuugenmagan;
	yuugen = {};
	boss.id = BossId::yuugenmagan;
	boss.active = true;
	boss.phase = 0;
	boss.phase_frame = 0;
	boss.hp = 16;
	yuugen.eyes[0] = {64, 128, eye_hidden, false};
	yuugen.eyes[1] = {512, 128, eye_hidden, false};
	yuugen.eyes[2] = {192, 176, eye_hidden, false};
	yuugen.eyes[3] = {384, 176, eye_hidden, false};
	yuugen.eyes[4] = {288, 64, eye_hidden, false};
	missiles_reset(state);
}

[[nodiscard]] BossUpdateResult yuugenmagan_update(State &state) noexcept
{
	auto &boss = state.boss;
	auto &yuugen = boss.yuugenmagan;
	if(!boss.active || boss.id != BossId::yuugenmagan) return BossUpdateResult::unsupported;
	missiles_update(state);
	if(boss.phase == 0) {
		boss.phase_frame = word(static_cast<std::int32_t>(boss.phase_frame) + 1);
		phase_zero_lasers(state);
		if(boss.phase_frame == 100) {
			phase_zero_open(yuugen, 100); eyes_set_orb_hitbox(yuugen, static_cast<std::uint8_t>(eye_west | eye_east), true);
		} else if(boss.phase_frame == 120) {
			phase_zero_open(yuugen, 120); eyes_set_orb_hitbox(yuugen, static_cast<std::uint8_t>(eye_southwest | eye_southeast), true);
		} else if(boss.phase_frame == 140) {
			phase_zero_open(yuugen, 140); eyes_set_orb_hitbox(yuugen, eye_north, true);
		} else if(boss.phase_frame == 160 || boss.phase_frame == 180 || boss.phase_frame == 200) {
			phase_zero_open(yuugen, boss.phase_frame);
		} else if(boss.phase_frame == 240) {
			phase_zero_close(yuugen, 240); eyes_set_orb_hitbox(yuugen, eye_all, false);
			if(state.rank == 2) {
				fire_from_iris(state, eye(yuugen, eye_north), PelletGroup::five_spread_wide_aimed, 54);
				fire_from_iris(state, eye(yuugen, eye_southeast), PelletGroup::five_spread_wide_aimed, 54);
				fire_from_iris(state, eye(yuugen, eye_southwest), PelletGroup::five_spread_wide_aimed, 54);
			}
		} else if(boss.phase_frame == 260) {
			phase_zero_close(yuugen, 260);
			if(state.rank == 3) for(const auto &value : yuugen.eyes) fire_from_iris(state, value, PelletGroup::five_spread_wide_aimed, 54);
		} else if(boss.phase_frame == 280) {
			phase_zero_close(yuugen, 280);
		} else if(boss.phase_frame == 300) {
			eye(yuugen, eye_west).image = eye_closed; eye(yuugen, eye_east).image = eye_closed;
		} else if(boss.phase_frame == 320) {
			eye(yuugen, eye_west).image = eye_halfopen; eye(yuugen, eye_east).image = eye_halfopen;
			eyes_set_orb_hitbox(yuugen, static_cast<std::uint8_t>(eye_west | eye_east), true);
		} else if(boss.phase_frame > 330) {
			boss.phase = 1; boss.phase_frame = 0; yuugen.invincible = false;
			yuugen.pattern_interval = rank_value(state, 350, 300, 200, 130);
		}
		return BossUpdateResult::running;
	}
	if(boss.phase == 1) {
		frame_common(state); eyes_track_player(state, static_cast<std::uint8_t>(eye_west | eye_east)); phase_one_pellets(state); hit_update(state);
		if(boss.hp <= 15 || boss.phase_frame > 1100) phase_next(state, 2);
	} else if(boss.phase == 2) {
		phase_toggled_eyes(state, static_cast<std::uint8_t>(eye_west | eye_east), static_cast<std::uint8_t>(eye_southwest | eye_southeast));
		if(boss.phase_frame >= eye_toggle_frames) { phase_next(state, 3); yuugen.phase_u1 = 0; yuugen.phase_u2 = 0; yuugen.phase_u3 = 0; }
	} else if(boss.phase == 3) {
		frame_common(state); pattern_missiles(state, 3, false); hit_update(state);
		if(boss.hp <= 12 || yuugen.phase_u3 >= 5U) phase_next(state, 4);
	} else if(boss.phase == 4) {
		phase_toggled_eyes(state, static_cast<std::uint8_t>(eye_southwest | eye_southeast), static_cast<std::uint8_t>(eye_west | eye_east));
		if(boss.phase_frame >= eye_toggle_frames) { phase_next(state, 5); yuugen.phase_u1 = 0; yuugen.phase_u2 = 0; yuugen.phase_u3 = 0; yuugen.pattern_interval = rank_value(state, 12, 8, 4, 2); }
	} else if(boss.phase == 5) {
		frame_common(state); yuugen.phase_u1 = word(static_cast<std::int32_t>(yuugen.phase_u1) + 1); yuugen.missile_angle_southwest = static_cast<std::uint8_t>(yuugen.missile_angle_southwest - 1U); eyes_track_player(state, static_cast<std::uint8_t>(eye_west | eye_east));
		if(boss.phase_frame == 100 && yuugen.phase_u2 == 0) { yuugen.phase_u1 = 0; yuugen.phase_u2 = 1; }
		if(yuugen.phase_u2 == 1) {
			if((yuugen.phase_u1 % yuugen.pattern_interval) == 0) { fire_single_from_bottom(state, eye(yuugen, eye_west), true, yuugen.missile_angle_southwest); fire_single_from_bottom(state, eye(yuugen, eye_east), false, yuugen.missile_angle_southwest); }
			if((yuugen.phase_u1 / 5) >= 36) { yuugen.phase_u2 = 2; boss.phase_frame = 0; }
		}
		if(yuugen.phase_u2 == 2 && boss.phase_frame > 50) { yuugen.phase_u1 = -15; yuugen.phase_u2 = 3; yuugen.missile_angle_southwest = 0x80U; }
		if(yuugen.phase_u2 == 3) {
			if((yuugen.phase_u1 % yuugen.pattern_interval) == 0) { fire_single_from_bottom(state, eye(yuugen, eye_west), true, yuugen.missile_angle_southwest, PelletMotion::fall_straight_from_top_then_regular, 64); fire_single_from_bottom(state, eye(yuugen, eye_east), false, yuugen.missile_angle_southwest, PelletMotion::fall_straight_from_top_then_regular, 64); }
			if((yuugen.phase_u1 / 5) >= 20) { yuugen.phase_u2 = 0; boss.phase_frame = 0; yuugen.phase_u3 = static_cast<std::uint8_t>(yuugen.phase_u3 + 1U); yuugen.missile_angle_southwest = 0; }
		}
		hit_update(state); if(boss.hp <= 10 || yuugen.phase_u3 > 4U) phase_next(state, 6);
	} else if(boss.phase == 6) {
		phase_toggled_eyes(state, static_cast<std::uint8_t>(eye_west | eye_east), static_cast<std::uint8_t>(eye_southwest | eye_southeast));
		if(boss.phase_frame >= eye_toggle_frames) { phase_next(state, 7); yuugen.phase_u1 = 0; yuugen.phase_u2 = 0; yuugen.phase_u3 = 0; }
	} else if(boss.phase == 7) {
		frame_common(state); pattern_missiles(state, 2, true); hit_update(state); if(boss.hp <= 8 || yuugen.phase_u3 > 4U) phase_next(state, 8);
	} else if(boss.phase == 8) {
		phase_toggled_eyes(state, static_cast<std::uint8_t>(eye_southwest | eye_southeast), eye_north);
		if(boss.phase_frame >= eye_toggle_frames) { phase_next(state, 9); yuugen.phase_u1 = 0; yuugen.phase_u2 = 0; }
	} else if(boss.phase == 9) {
		frame_common(state); pattern_lasers(state); hit_update(state);
		if(boss.hp <= 2 || yuugen.phase_u2 == 6) { boss.phase = 10; boss.phase_frame = 0; yuugen.invincibility_frame = 0; yuugen.phase_u2 = 0; yuugen.pentagram.phase = pentagram_prepare_1; }
	} else if(boss.phase == 10) {
		phase_toggled_eyes(state, 0U, eye_all);
		if(boss.phase_frame >= eye_toggle_frames) { phase_next(state, 11); yuugen.phase_u1 = 0; yuugen.phase_u2 = 0; yuugen.phase_u3 = 31U; yuugen.pentagram.angle = 0; }
	} else if(boss.phase == 11) {
		frame_common(state); eyes_track_player(state, eye_all);
		if(boss.phase_frame == 100 || yuugen.phase_u2 == 1) {
			if(yuugen.phase_u2 == 0) yuugen.phase_u2 = 1;
			const auto &north = eye(yuugen, eye_north); const auto &sw = eye(yuugen, eye_southwest); const auto &east = eye(yuugen, eye_east); const auto &west = eye(yuugen, eye_west); const auto &se = eye(yuugen, eye_southeast);
			linebullet_put_and_hittest(state, eye_offcenter_left(north), eye_iris_top(north), eye_offcenter_left(sw), eye_iris_top(sw), 15U); linebullet_put_and_hittest(state, eye_offcenter_left(sw), eye_iris_top(sw), eye_offcenter_left(east), eye_iris_top(east), 15U); linebullet_put_and_hittest(state, eye_offcenter_left(east), eye_iris_top(east), eye_offcenter_left(west), eye_iris_top(west), 15U); linebullet_put_and_hittest(state, eye_offcenter_left(west), eye_iris_top(west), eye_offcenter_left(se), eye_iris_top(se), 15U); linebullet_put_and_hittest(state, eye_offcenter_left(se), eye_iris_top(se), eye_offcenter_left(north), eye_iris_top(north), 15U);
			if(boss.phase_frame == 140) { yuugen.phase_u2 = 2; yuugen.phase_u1 = 0; }
		}
		if(yuugen.phase_u2 == 2 && (boss.phase_frame % 4) == 1) {
			pentagram_shrink_unput(state, yuugen.phase_u1);
			yuugen.phase_u1 = word(static_cast<std::int32_t>(yuugen.phase_u1) + 8);
			pentagram_shrink_put(state, yuugen.phase_u1);
			if(yuugen.phase_u1 > pentagram_shrink_distance) yuugen.phase_u2 = 3;
		}
		hit_update(state); if(boss.hp <= 0) { defeat(state, false); return BossUpdateResult::stage_cleared; }
		if(yuugen.phase_u2 == 3 && !yuugen.invincible) {
			phase_next(state, 12); yuugen.phase_u2 = 0; yuugen.phase_u3 = eye_west;
			yuugen.pentagram.center_left = pentagram_shrink_left(yuugen, eye_north, yuugen.phase_u1);
			yuugen.pentagram.center_top = word(static_cast<std::int32_t>(pentagram_shrink_top(yuugen, eye_north, yuugen.phase_u1)) + pentagram_radius_final);
			yuugen.pentagram.radius = pentagram_radius_final; pentagram_set_regular(yuugen.pentagram, 0);
			yuugen.pattern_interval = rank_value(state, 24, 14, 10, 8);
		}
	} else if(boss.phase == 12) {
		yuugen.invincibility_frame = word(static_cast<std::int32_t>(yuugen.invincibility_frame) + 1); boss.phase_frame = word(static_cast<std::int32_t>(boss.phase_frame) + 1);
		if((yuugen.invincibility_frame % 4) == 0) { yuugen.pentagram.center_top = word(static_cast<std::int32_t>(yuugen.pentagram.center_top) - 3); yuugen.pentagram.angle = 0xC0U; pentagram_regular_update(state); }
		if(boss.phase_frame >= eye_toggle_frames) { phase_next(state, 13); yuugen.phase_u2 = 0; yuugen.phase_u3 = eye_west; yuugen.pentagram.angle = 0; yuugen.after_hit_frame = 0; }
	} else if(boss.phase == 13) {
		phase_thirteen(state); if(!boss.active) return BossUpdateResult::stage_cleared;
	} else {
		return BossUpdateResult::unsupported;
	}
	return BossUpdateResult::running;
}

void yuugenmagan_render_tail(State &) noexcept
{
	// b10m's tail is entity, line, palette, and HUD presentation.  Every
	// source gameplay mutation occurs at the update call sites above.
}

} // namespace th01::core
