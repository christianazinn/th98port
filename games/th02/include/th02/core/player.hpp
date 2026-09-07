#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

struct ScreenPoint {
	std::int16_t x{};
	std::int16_t y{};
	bool operator==(const ScreenPoint &) const = default;
};

struct PlayerPositionState {
	std::array<ScreenPoint, 2> on_page{};
	ScreenPoint current{};
	bool operator==(const PlayerPositionState &) const = default;
};

// th02/main/player/player.cpp::player_move. The caller supplies the page
// established by player_invalidate; this does not copy the front page first.
// An invalid native page is rejected without changing any state.
[[nodiscard]] bool player_move(PlayerPositionState &state, std::size_t back_page,
	std::int16_t delta_x, std::int16_t delta_y) noexcept;

} // namespace th02::core
