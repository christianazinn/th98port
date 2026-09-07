#pragma once

#include "pc98/th0405_player_movement.hpp"

namespace th04::core {

using Velocity = pc98::th0405::Velocity;
using MoveResult = pc98::th0405::MoveResult;

// TH04's constants from th04/main/player/move.hpp, in Q12.4 units.
// The caller clears velocity and masks input before this original helper.
// Preserve the helper boundary: do not silently perform either operation here.
[[nodiscard]] constexpr MoveResult player_move(
	Velocity &velocity, std::uint16_t input
) noexcept
{
	return pc98::th0405::player_move(velocity, input, 64, 48);
}

} // namespace th04::core
