#include "th02/core/laser.hpp"

#include <bit>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_right = 416;
constexpr std::int16_t playfield_bottom = 384;
constexpr std::int16_t screen_height = 400;
constexpr std::int16_t charge_offset = 8;
constexpr std::int16_t charge_width = 32;
constexpr std::int16_t charge_height = 32;
constexpr std::int16_t beam_width = 16;

[[nodiscard]] std::int16_t word_subtract(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) -
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

void increment_byte(std::uint8_t &value) noexcept
{
	value = static_cast<std::uint8_t>(value + 1U);
}

} // namespace

void lasers_reset(LaserPoolState &state) noexcept
{
	for(auto &laser : state.lasers) {
		laser.flag = laser_free;
	}
	state.wait_frames = laser_wait_frames_default;
}

LaserAddResult lasers_add(LaserPoolState &state, ScreenPoint origin,
	std::int16_t active_frames, std::int16_t patnum_base,
	LaserSpawnCallbacks &callbacks
) noexcept
{
	if((origin.x < playfield_left) || (origin.x >= playfield_right)) {
		return LaserAddResult::ignored;
	}
	for(auto &laser : state.lasers) {
		if(laser.flag != laser_free) continue;
		laser.flag = laser_alive;
		laser.phase = laser_phase_wait;
		laser.origin = origin;
		laser.wait_frames = static_cast<std::int16_t>(state.wait_frames);
		laser.active_frames = active_frames;
		laser.charge_cel = 0U;
		laser.patnum_base = static_cast<std::uint8_t>(patnum_base);
		callbacks.sound_effect(6U);
		return LaserAddResult::spawned;
	}
	return LaserAddResult::ignored;
}

void lasers_invalidate(LaserPoolState &state, std::uint32_t stage_frame,
	LaserInvalidationCallbacks &callbacks
) noexcept
{
	for(auto &laser : state.lasers) {
		if(laser.flag == laser_free) continue;
		if(laser.charge_cel <= laser_charge_cels) {
			callbacks.invalidate_laser({
				{word_subtract(laser.origin.x, charge_offset),
					word_subtract(laser.origin.y, charge_offset)},
				charge_width,
				charge_height,
			});
			if((stage_frame & 3U) == 0U) {
				increment_byte(laser.charge_cel);
			}
		} else {
			callbacks.invalidate_laser({
				laser.origin,
				beam_width,
				word_subtract(screen_height, laser.origin.y),
			});
		}
		if(laser.flag == laser_remove) {
			laser.flag = laser_free;
		}
	}
}

} // namespace th02::core
