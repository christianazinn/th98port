#pragma once
#include "th02/core/stage_scenery.hpp"

namespace th02::core {
struct Stage3EffectState {
	std::uint16_t frame{};
	std::int16_t radius{};
	std::uint8_t angle{};
	ScreenPoint center{};
	std::array<std::array<ScreenPoint, 64>, 2> dots{};
};
using Stage3EffectPresentation = StageSceneryCallbacks;
[[nodiscard]] bool stage3_invalidate(const Stage3EffectState &state, std::size_t back,
	StageSceneryCallbacks &presentation) noexcept;
[[nodiscard]] bool stage3_update_and_render(Stage3EffectState &state, std::size_t back,
	std::int16_t scroll, std::uint8_t &slowdown, std::uint16_t &tone,
	Stage3EffectPresentation &presentation) noexcept;
} // namespace th02::core
