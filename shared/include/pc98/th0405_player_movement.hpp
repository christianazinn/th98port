#pragma once

#include "pc98/th0405_motion.hpp"

#include <bit>
#include <cstdint>

namespace pc98::th0405 {

using Velocity = SubpixelPoint;

enum class MoveResult : std::uint8_t { invalid = 0, valid = 1, no_input = 2 };

// th04/main/player/move.cpp. TH05 shares direction application, but its
// th05/player.asm dispatcher masks high input bits in the game wrapper.
// Caller-owned velocity clearing, input masking, and focus are not included.
[[nodiscard]] constexpr MoveResult player_move(Velocity &velocity,
	std::uint16_t input, std::int16_t aligned, std::int16_t diagonal) noexcept
{
	int x = 0;
	int y = 0;
	switch(input) {
	case 0: return MoveResult::no_input;
	case 4: x = -1; break;
	case 6: case 0x0400: x = -1; y = 1; break;
	case 2: y = 1; break;
	case 10: case 0x0800: x = 1; y = 1; break;
	case 8: x = 1; break;
	case 9: case 0x0200: x = 1; y = -1; break;
	case 1: y = -1; break;
	case 5: case 0x0100: x = -1; y = -1; break;
	default: return MoveResult::invalid;
	}
	const auto speed = (x != 0 && y != 0) ? diagonal : aligned;
	const auto scaled = [speed](int sign) {
		return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
			static_cast<std::int32_t>(speed) * sign));
	};
	if(x != 0) velocity.x = scaled(x);
	if(y != 0) velocity.y = scaled(y);
	return MoveResult::valid;
}

} // namespace pc98::th0405
