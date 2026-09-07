#pragma once

#include <bit>
#include <cstdint>

namespace th01::core {

inline constexpr std::int16_t player_left_min = 0;
inline constexpr std::int16_t player_left_max = 608;
inline constexpr std::int16_t player_left_start = 304;

// ReC98 th01/main/player/move.cpp::player_move_and_clamp. Assignment to
// the original 16-bit coordinate precedes the signed boundary comparisons.
[[nodiscard]] constexpr std::int16_t move_player(
	std::int16_t current, std::int16_t delta
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(current) + static_cast<std::uint16_t>(delta)
	);
	const auto moved = std::bit_cast<std::int16_t>(bits);
	return moved < player_left_min ? player_left_min :
		(moved >= player_left_max ? player_left_max : moved);
}

} // namespace th01::core
