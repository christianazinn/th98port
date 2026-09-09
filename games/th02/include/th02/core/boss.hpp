#pragma once

#include "th02/core/player.hpp"

#include <array>
#include <cstdint>

namespace th02::core {

// Shared source globals used by bosses and midbosses. Page positions are kept
// together for native determinism even though the donor stores left/top as
// separate arrays. Stage-specific state belongs to its own owner.
struct BossState {
	std::array<ScreenPoint, 2U> topleft_on_page{};
	std::int16_t phase_frame{};
	std::int16_t damage{};
	std::uint8_t phase{};
	ScreenPoint target{-1, -1};
	std::int16_t patnum{};
	std::array<std::uint8_t, 5U> rank_param{};
	std::uint8_t explode_angle_offset{};
	bool operator==(const BossState &) const = default;
};

} // namespace th02::core
