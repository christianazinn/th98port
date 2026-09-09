#include "th05/core/state.hpp"

#include "pc98/masterlib_vector.hpp"
#include "th05/core/laser.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace th05::core {
namespace {

constexpr std::int16_t laser_distance_step = 256;
constexpr std::uint8_t laser_shootout_decay_width_max = 28U;
constexpr std::int16_t playfield_screen_left = 32;
constexpr std::int16_t playfield_screen_top = 16;
constexpr std::int16_t playfield_screen_right = 415;
constexpr std::int16_t playfield_screen_bottom = 383;

[[nodiscard]] constexpr std::int16_t word_add(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) + static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::int16_t word_sub(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) - static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::uint8_t byte_add(
	std::uint8_t left, int right
) noexcept
{
	return static_cast<std::uint8_t>(static_cast<unsigned>(left) +
		static_cast<unsigned>(right));
}

[[nodiscard]] constexpr std::int8_t signed_width(std::uint8_t width) noexcept
{
	return std::bit_cast<std::int8_t>(width);
}

[[nodiscard]] constexpr bool overlap_laser_axis(
	std::int16_t sample, std::int16_t player_axis
) noexcept
{
	// th01/math/overlap.hpp::overlap_1d_fast() with the source's Q12.4
	// 12-pixel extent. Its unsigned comparison is lower-inclusive and
	// upper-exclusive, unlike the bullet hitbox helper.
	const auto delta = word_sub(word_add(sample, 96), player_axis);
	return static_cast<std::uint16_t>(delta) < 192U;
}

[[nodiscard]] constexpr LaserFlag next_flag(LaserFlag flag) noexcept
{
	return static_cast<LaserFlag>(byte_add(static_cast<std::uint8_t>(flag), 1));
}

[[nodiscard]] constexpr bool is_free(LaserFlag flag) noexcept
{
	return flag == LaserFlag::free;
}

[[nodiscard]] bool laser_hittest(State &state, const Laser &laser) noexcept
{
	// laser_rh.cpp starts at [end] and includes [start]. The ordinary range is
	// finite; a signed-word wrap after a reached sample is the source's host-
	// runaway landmine and is reported as an explicit refusal after its prefix.
	auto distance = laser.coords.ends_at_distance;
	for(;;) {
		const auto offset = pc98::masterlib::vector2(laser.coords.angle, distance);
		const auto sample_y = word_add(laser.coords.origin.y, offset.y);
		if(overlap_laser_axis(sample_y, state.player_pos.cur.y)) {
			const auto sample_x = word_add(laser.coords.origin.x, offset.x);
			if(overlap_laser_axis(sample_x, state.player_pos.cur.x)) {
				state.player_is_hit = true;
				return true;
			}
		}

		const auto next_distance = word_sub(distance, laser_distance_step);
		if(next_distance > distance) return false;
		distance = next_distance;
		if(distance < laser.coords.starts_at_distance) return true;
	}
}

void shootout_update(Laser &laser) noexcept
{
	if(laser.coords.starts_at_distance < laser_distance_max) {
		laser.coords.ends_at_distance = word_add(
			laser.coords.ends_at_distance, laser.shootout_speed
		);
	}
	if(laser.age >= laser.active_at_age) {
		laser.coords.starts_at_distance = word_add(
			laser.coords.starts_at_distance, laser.shootout_speed
		);
	}
}

struct ScreenPoint {
	std::int16_t x{}, y{};
};

[[nodiscard]] constexpr std::int16_t q12_4_to_pixel(std::int16_t value) noexcept
{
	// The original uses a signed SAR 4, including floor-toward-negative-
	// infinity behavior for points left/above the visible screen.
	if(value >= 0) return static_cast<std::int16_t>(value / 16);
	return static_cast<std::int16_t>(-
		((static_cast<std::int32_t>(-value) + 15) / 16)
	);
}

[[nodiscard]] constexpr std::int64_t cross(
	ScreenPoint origin, ScreenPoint first, ScreenPoint second
) noexcept
{
	return static_cast<std::int64_t>(first.x - origin.x) * (second.y - origin.y) -
		static_cast<std::int64_t>(first.y - origin.y) * (second.x - origin.x);
}

[[nodiscard]] constexpr bool point_in_screen(ScreenPoint point) noexcept
{
	return point.x >= playfield_screen_left && point.x <= playfield_screen_right &&
		point.y >= playfield_screen_top && point.y <= playfield_screen_bottom;
}

[[nodiscard]] constexpr bool point_in_convex_quad(
	ScreenPoint point, const std::array<ScreenPoint, 4U> &quad
) noexcept
{
	bool positive{};
	bool negative{};
	for(std::size_t index{}; index < quad.size(); ++index) {
		const auto area = cross(quad[index], quad[(index + 1U) % quad.size()], point);
		positive = positive || area > 0;
		negative = negative || area < 0;
	}
	return !(positive && negative);
}

[[nodiscard]] constexpr bool between(std::int16_t value, std::int16_t a,
	std::int16_t b) noexcept
{
	return value >= (a < b ? a : b) && value <= (a < b ? b : a);
}

[[nodiscard]] constexpr bool point_on_segment(
	ScreenPoint point, ScreenPoint first, ScreenPoint second
) noexcept
{
	return cross(first, second, point) == 0 &&
		between(point.x, first.x, second.x) && between(point.y, first.y, second.y);
}

[[nodiscard]] constexpr int sign(std::int64_t value) noexcept
{
	return (value > 0) - (value < 0);
}

[[nodiscard]] constexpr bool segments_intersect(
	ScreenPoint a, ScreenPoint b, ScreenPoint c, ScreenPoint d
) noexcept
{
	const auto ab_c = cross(a, b, c);
	const auto ab_d = cross(a, b, d);
	const auto cd_a = cross(c, d, a);
	const auto cd_b = cross(c, d, b);
	if(sign(ab_c) != sign(ab_d) && sign(cd_a) != sign(cd_b)) return true;
	return (ab_c == 0 && point_on_segment(c, a, b)) ||
		(ab_d == 0 && point_on_segment(d, a, b)) ||
		(cd_a == 0 && point_on_segment(a, c, d)) ||
		(cd_b == 0 && point_on_segment(b, c, d));
}

[[nodiscard]] bool laser_ray_is_offscreen(const LaserCoords &coords) noexcept
{
	// laser_render_ray(): construct the actual four Q12.4 corners, then ask
	// master.lib's inclusive current clip rectangle whether any part
	// intersects it. Stage initialization has set this to the playfield.
	const auto origin_x = word_add(coords.origin.x, playfield_screen_left * 16);
	const auto origin_y = word_add(coords.origin.y, playfield_screen_top * 16);
	const auto radius = static_cast<std::int16_t>(
		static_cast<std::uint16_t>(coords.width) << 3U
	);
	const auto cw = pc98::masterlib::vector2(
		byte_add(coords.angle, 0x40), radius
	);
	const auto ccw = pc98::masterlib::vector2(
		byte_add(coords.angle, -0x40), radius
	);
	const pc98::th0405::SubpixelPoint origin_ccw{
		word_add(origin_x, ccw.x), word_add(origin_y, ccw.y),
	};
	const pc98::th0405::SubpixelPoint ccw_to_cw{
		word_sub(cw.x, ccw.x), word_sub(cw.y, ccw.y),
	};
	const auto start = pc98::masterlib::vector2(
		coords.angle, coords.starts_at_distance
	);
	const auto end = pc98::masterlib::vector2(
		coords.angle, coords.ends_at_distance
	);
	const pc98::th0405::SubpixelPoint start_ccw{
		word_add(origin_ccw.x, start.x), word_add(origin_ccw.y, start.y),
	};
	const pc98::th0405::SubpixelPoint end_ccw{
		word_add(origin_ccw.x, end.x), word_add(origin_ccw.y, end.y),
	};
	const std::array<ScreenPoint, 4U> quad{{
		{q12_4_to_pixel(start_ccw.x), q12_4_to_pixel(start_ccw.y)},
		{q12_4_to_pixel(word_add(start_ccw.x, ccw_to_cw.x)),
			q12_4_to_pixel(word_add(start_ccw.y, ccw_to_cw.y))},
		{q12_4_to_pixel(word_add(end_ccw.x, ccw_to_cw.x)),
			q12_4_to_pixel(word_add(end_ccw.y, ccw_to_cw.y))},
		{q12_4_to_pixel(end_ccw.x), q12_4_to_pixel(end_ccw.y)},
	}};
	for(const auto point : quad) {
		if(point_in_screen(point)) return false;
	}
	constexpr std::array<ScreenPoint, 4U> screen{{
		{playfield_screen_left, playfield_screen_top},
		{playfield_screen_right, playfield_screen_top},
		{playfield_screen_right, playfield_screen_bottom},
		{playfield_screen_left, playfield_screen_bottom},
	}};
	for(const auto point : screen) {
		if(point_in_convex_quad(point, quad)) return false;
	}
	for(std::size_t quad_index{}; quad_index < quad.size(); ++quad_index) {
		for(std::size_t screen_index{}; screen_index < screen.size(); ++screen_index) {
			if(segments_intersect(
				quad[quad_index], quad[(quad_index + 1U) % quad.size()],
				screen[screen_index], screen[(screen_index + 1U) % screen.size()]
			)) return false;
		}
	}
	return true;
}

void fixed_init(Laser &laser, pc98::th0405::SubpixelPoint origin) noexcept
{
	laser.flag = LaserFlag::fixed_wait_to_grow;
	laser.coords.origin = origin;
	laser.coords.starts_at_distance = laser_distance_min;
	laser.coords.ends_at_distance = laser_distance_max;
	laser.age = 0;
}

} // namespace

bool lasers_update(State &state, Presentation &presentation) noexcept
{
	for(auto &laser : state.lasers.pool) {
		if(is_free(laser.flag)) continue;

		if(state.resources.bullet_clear_time != 0U || state.bullets.zap.active()) {
			if(laser.flag == LaserFlag::shootout) {
				laser.flag = LaserFlag::shootout_decay;
				laser.shootout_speed = static_cast<std::int16_t>(laser.shootout_speed / 2);
			}
		}

		switch(laser.flag) {
		case LaserFlag::shootout:
			shootout_update(laser);
			if(!laser_hittest(state, laser)) return false;
			break;
		case LaserFlag::fixed_wait_to_grow:
			if(laser.active_at_age > 0 && laser.age >= laser.active_at_age) {
				laser.flag = next_flag(laser.flag);
				presentation.sound(6U);
			}
			break;
		case LaserFlag::fixed_grow:
			if((laser.age & 1) != 0) laser.coords.width = byte_add(laser.coords.width, 2);
			if(laser.coords.width >= laser.grow_to_width) laser.flag = next_flag(laser.flag);
			break;
		case LaserFlag::fixed_active:
			if(!laser_hittest(state, laser)) return false;
			if(laser.shrink_at_age > 0 && laser.age >= laser.shrink_at_age) {
				laser.flag = next_flag(laser.flag);
			}
			break;
		case LaserFlag::fixed_shrink:
			if((laser.age & 1) != 0) laser.coords.width = byte_add(laser.coords.width, -2);
			if(signed_width(laser.coords.width) < 1) laser.flag = LaserFlag::free;
			break;
		case LaserFlag::fixed_shrink_and_wait_to_grow:
			if((laser.age & 1) != 0) laser.coords.width = byte_add(laser.coords.width, -2);
			if(signed_width(laser.coords.width) <= 1) {
				laser.flag = LaserFlag::fixed_wait_to_grow;
				laser.age = 0;
			}
			break;
		case LaserFlag::shootout_decay:
			shootout_update(laser);
			laser.coords.width = byte_add(laser.coords.width, 2);
			if(laser.coords.width >= laser_shootout_decay_width_max) {
				laser.flag = LaserFlag::free;
			}
			break;
		default:
			break;
		}
		laser.age = word_add(laser.age, 1);
	}
	return true;
}

void lasers_render(State &state, Presentation &presentation) noexcept
{
	for(auto &laser : state.lasers.pool) {
		if(is_free(laser.flag)) continue;
		const auto width_original = laser.coords.width;
		presentation.laser_geometry({laser.flag, laser.color, laser.coords});

		if(laser.flag != LaserFlag::shootout_decay) {
			if(laser.coords.width >= 3U) {
				laser.coords.width = laser.coords.width >= 5U
					? byte_add(laser.coords.width, -4)
					: byte_add(laser.coords.width, -2);
			}
			if(laser.coords.width > 1U && laser_ray_is_offscreen(laser.coords)) {
				laser.flag = LaserFlag::free;
			}
		}
		laser.coords.width = width_original;
	}
}

bool laser_manual_fixed_spawn(
	State &state, std::uint8_t slot, Presentation &presentation
) noexcept
{
	if(slot >= state.lasers.pool.size()) return false;
	auto &laser = state.lasers.pool[slot];
	if(!is_free(laser.flag)) return true;
	fixed_init(laser, state.lasers.laser_template.coords.origin);
	laser.active_at_age = -1;
	laser.shrink_at_age = -1;
	laser.color = state.lasers.laser_template.color;
	laser.coords.width = 1U;
	laser.grow_to_width = state.lasers.laser_template.coords.width;
	laser.coords.angle = state.lasers.laser_template.coords.angle;
	presentation.sound(5U);
	return true;
}

bool laser_manual_grow(
	State &state, std::uint8_t slot, Presentation &presentation
) noexcept
{
	if(slot >= state.lasers.pool.size()) return false;
	auto &laser = state.lasers.pool[slot];
	if(laser.flag != LaserFlag::fixed_wait_to_grow) return true;
	laser.flag = LaserFlag::fixed_grow;
	presentation.sound(6U);
	return true;
}

bool laser_stop(State &state, std::uint8_t slot) noexcept
{
	if(slot >= state.lasers.pool.size()) return false;
	auto &laser = state.lasers.pool[slot];
	if(laser.flag == LaserFlag::fixed_active) {
		laser.flag = LaserFlag::fixed_shrink;
	} else if(laser.flag == LaserFlag::shootout) {
		laser.flag = LaserFlag::shootout_decay;
	} else if(laser.flag != LaserFlag::fixed_shrink &&
		laser.flag != LaserFlag::shootout_decay) {
		laser.flag = LaserFlag::free;
	}
	return true;
}

} // namespace th05::core
