#include "th04/core/state.hpp"

#include "th04/core/thicklaser.hpp"

#include <bit>
#include <cstdint>

namespace th04::core {
namespace {

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

[[nodiscard]] constexpr std::int16_t word_mul(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(static_cast<std::uint16_t>(left)) *
			static_cast<std::uint32_t>(static_cast<std::uint16_t>(right))
	));
}

[[nodiscard]] constexpr std::int16_t layer_width(std::int16_t radius) noexcept
{
	const auto width = static_cast<std::int16_t>(radius / 4);
	return width > thicklaser_layer_width_max ? thicklaser_layer_width_max : width;
}

[[nodiscard]] constexpr bool is_hittable(ThickLaserFlag flag) noexcept
{
	return static_cast<std::uint8_t>(flag) >
		static_cast<std::uint8_t>(ThickLaserFlag::line);
}

void thicklaser_update(ThickLaser &laser, Presentation &presentation) noexcept
{
	switch(laser.flag) {
	case ThickLaserFlag::line:
		if(laser.cur_flag_frame >= laser.line_frames) {
			laser.flag = ThickLaserFlag::grow;
			laser.cur_flag_frame = 0;
			presentation.sound(6U);
		}
		break;
	case ThickLaserFlag::grow:
		laser.radius_cur = word_add(laser.radius_cur, laser.radius_speed);
		if(laser.radius_cur >= laser.radius_max) {
			laser.flag = ThickLaserFlag::static_;
			laser.cur_flag_frame = 0;
			laser.radius_cur = laser.radius_max;
		}
		break;
	case ThickLaserFlag::static_:
		if(laser.cur_flag_frame >= laser.static_frames) {
			laser.flag = ThickLaserFlag::shrink;
			laser.cur_flag_frame = 0;
		}
		break;
	case ThickLaserFlag::shrink:
		laser.radius_cur = word_sub(laser.radius_cur, laser.radius_speed);
		if(laser.radius_cur <= 1) laser.flag = ThickLaserFlag::free;
		break;
	default:
		break;
	}

	// The source increments this after every reached state, including SHRINK
	// after it freed the slot. A later template pull overwrites that residue.
	laser.cur_flag_frame = word_add(laser.cur_flag_frame, 1);
}

void thicklaser_hittest(State &state, const ThickLaser &laser) noexcept
{
	if(!is_hittable(laser.flag)) return;

	// b4m.cpp first compares the player against the circle's upper half, then
	// horizontally against its white core. There is deliberately no lower Y
	// boundary: the laser body continues down the entire playfield.
	auto hitbox_half_w = word_mul(laser.radius_cur, 8);
	if(word_add(laser.origin.y, hitbox_half_w) > state.player_pos.cur.y) return;

	hitbox_half_w = word_mul(laser.radius_cur, 4);
	if(hitbox_half_w >= word_mul(thicklaser_layer_width_max, 16)) {
		hitbox_half_w = word_mul(thicklaser_layer_width_max, 16);
	}
	hitbox_half_w = word_sub(word_mul(laser.radius_cur, 16), hitbox_half_w);
	if(word_sub(laser.origin.x, hitbox_half_w) > state.player_pos.cur.x) return;
	if(word_add(laser.origin.x, hitbox_half_w) < state.player_pos.cur.x) return;
	state.player_is_hit = true;
}

} // namespace

void thicklasers_reset(State &state) noexcept
{
	for(auto &laser : state.thicklasers.pool) laser.flag = ThickLaserFlag::free;
	state.thicklasers.template_.cur_flag_frame = 0;
	state.thicklasers.template_.flag = ThickLaserFlag::line;
	state.thicklasers.template_.radius_cur = 1;
	state.thicklasers.template_.radius_speed = 1;
}

void thicklaser_add(State &state, Presentation &presentation) noexcept
{
	for(auto &laser : state.thicklasers.pool) {
		if(laser.flag != ThickLaserFlag::free) continue;
		laser = state.thicklasers.template_;
		presentation.sound(5U);
		return;
	}
}

void thicklasers_update_and_hittest(State &state, Presentation &presentation) noexcept
{
	for(auto &laser : state.thicklasers.pool) {
		if(laser.flag == ThickLaserFlag::free) continue;
		thicklaser_update(laser, presentation);
		thicklaser_hittest(state, laser);
	}
}

void thicklasers_render(State &state, Presentation &presentation) noexcept
{
	for(const auto &laser : state.thicklasers.pool) {
		if(laser.flag == ThickLaserFlag::free) continue;
		if(laser.flag == ThickLaserFlag::line) {
			presentation.thicklaser_geometry({
				laser.flag, laser.origin, laser.col_outline, 0, 0,
			});
			continue;
		}
		presentation.thicklaser_geometry({
			laser.flag, laser.origin, laser.col_outline, laser.radius_cur,
			layer_width(laser.radius_cur),
		});
	}
}

} // namespace th04::core
