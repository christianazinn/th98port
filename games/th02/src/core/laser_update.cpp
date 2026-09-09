#include "th02/core/laser_update.hpp"

#include <bit>
#include <cstdint>

namespace th02::core {
namespace {

constexpr std::int16_t screen_height = 400;
constexpr std::int16_t playfield_bottom = 384;
constexpr std::int16_t charge_offset = 8;
constexpr std::int16_t beam_cel_height = 16;
constexpr std::int16_t beam_cel_first_offset = 8;
constexpr std::int16_t laser_charge_patnum = 30;
constexpr std::int16_t laser_head_patnum = 91;
constexpr std::int16_t hitbox_offset_left = -24;
constexpr std::int16_t hitbox_offset_right = 8;

[[nodiscard]] std::int16_t add_word(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t scrolled_once(
	std::int16_t top, std::int16_t scroll_line
) noexcept
{
	top = add_word(top, scroll_line);
	if(top >= screen_height) top = add_word(top, -screen_height);
	return top;
}

} // namespace

void laser_render(const Laser &laser, const PlayerFrameState &player_frame,
	PlayerHitState &player_hit, std::int16_t scroll_line,
	LaserUpdateCallbacks &callbacks
) noexcept
{
	std::int16_t vram_top = scrolled_once(laser.origin.y, scroll_line);
	std::int16_t y = laser.origin.y;
	if(laser.phase < laser_phase_shrink) {
		callbacks.render_laser({LaserPresentationKind::head,
			{laser.origin.x, vram_top}, static_cast<std::int16_t>(
				laser_head_patnum + static_cast<std::int16_t>(laser.phase)
			)});
	}
	vram_top = scrolled_once(vram_top, beam_cel_first_offset);
	y = add_word(y, beam_cel_first_offset);
	const auto patnum = static_cast<std::int16_t>(
		static_cast<std::int16_t>(laser.patnum_base) +
		static_cast<std::int16_t>(laser.phase)
	);
	while(y < playfield_bottom) {
		callbacks.render_laser({LaserPresentationKind::beam,
			{laser.origin.x, vram_top}, patnum});
		vram_top = scrolled_once(vram_top, beam_cel_height);
		y = add_word(y, beam_cel_height);
	}
	if(laser.phase == laser_phase_active &&
		add_word(laser.origin.x, hitbox_offset_left) < player_frame.player.current.x &&
		add_word(laser.origin.x, hitbox_offset_right) > player_frame.player.current.x &&
		player_frame.player.current.y >= laser.origin.y && player_hit.raw_hit == 0U) {
		player_hit.raw_hit = 1U;
	}
}

void lasers_update_and_render(LaserPoolState &state,
	const PlayerFrameState &player_frame, PlayerHitState &player_hit,
	std::uint32_t stage_frame, std::int16_t scroll_line,
	LaserUpdateCallbacks &callbacks
) noexcept
{
	for(auto &laser : state.lasers) {
		if(laser.flag != laser_alive) continue;
		if(laser.charge_cel < laser_charge_cels) {
			callbacks.render_laser({LaserPresentationKind::charge,
				{add_word(laser.origin.x, -charge_offset),
					scrolled_once(add_word(laser.origin.y, -charge_offset), scroll_line)},
				static_cast<std::int16_t>(laser_charge_patnum +
					static_cast<std::int16_t>(laser.charge_cel))});
			laser.wait_frames = add_word(laser.wait_frames, -1);
			continue;
		}

		if(laser.phase == laser_phase_wait) {
			laser.wait_frames = add_word(laser.wait_frames, -1);
			if(laser.wait_frames <= 0) {
				laser.phase = static_cast<std::uint8_t>(laser.phase + 1U);
			}
		} else if(laser.phase < laser_phase_active) {
			if((stage_frame & 7U) == 0U) {
				laser.phase = static_cast<std::uint8_t>(laser.phase + 1U);
			}
			if(laser.phase == laser_phase_active) {
				callbacks.sound_effect(7U);
			}
		} else if(laser.phase == laser_phase_active) {
			laser.active_frames = add_word(laser.active_frames, -1);
			if(laser.active_frames == 0) {
				laser.phase = static_cast<std::uint8_t>(laser.phase + 1U);
			}
		} else {
			if((stage_frame & 3U) == 0U) {
				laser.phase = static_cast<std::uint8_t>(laser.phase + 1U);
			}
			if(laser.phase == laser_phase_done) {
				laser.flag = laser_remove;
				continue;
			}
		}
		laser_render(laser, player_frame, player_hit, scroll_line, callbacks);
	}
}

} // namespace th02::core
