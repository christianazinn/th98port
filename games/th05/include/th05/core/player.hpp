#pragma once

#include "pc98/th0405_player_movement.hpp"

namespace th05::core {

using Velocity = pc98::th0405::Velocity;
using MoveResult = pc98::th0405::MoveResult;
using Motion = pc98::th0405::Motion;
using pc98::th0405::player_pos_update_and_clamp;

struct MovementSpeeds {
	std::int16_t aligned{};
	std::int16_t diagonal{};
};

// TH05's speeds are runtime state, not TH04's constants. The original owner
// is th05/player.asm: its BH mask ignores bits 12..15 before dispatch. The
// caller's broader INPUT_MOVEMENT mask and Shift slowdown are separate.
[[nodiscard]] constexpr MoveResult player_move(
	Velocity &velocity, std::uint16_t input, MovementSpeeds speeds
) noexcept
{
	return pc98::th0405::player_move(velocity, static_cast<std::uint16_t>(input & 0x0FFFU),
		speeds.aligned, speeds.diagonal);
}

} // namespace th05::core
