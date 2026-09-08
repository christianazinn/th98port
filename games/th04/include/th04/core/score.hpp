#pragma once

#include "pc98/score_digits.hpp"

#include <cstddef>
#include <cstdint>

namespace th04::core {

using ScoreDigits = pc98::ScoreDigits;

struct ScoreState {
	ScoreDigits score{};
	std::uint32_t score_delta{}, score_delta_frame{};
	std::uint8_t score_unused{}, extends_gained{}, hiscore_popup_shown{};
	bool operator==(const ScoreState &) const = default;
};

// th04/main/score_reset.cpp. Unlike TH05 this never commits a high score.
inline void score_reset(ScoreState &state)
{
	for(std::size_t i = 1; i < state.score.size(); ++i) {
		state.score[i] = 0;
	}
	state.score_delta = 0;
	state.score_delta_frame = 0;
	state.score_unused = 0;
	state.extends_gained = 0;
	state.hiscore_popup_shown = 0;
}

} // namespace th04::core
