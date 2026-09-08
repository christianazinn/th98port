#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace th05::core {

// th04/score.h: digit zero aliases the continue counter, not score units.
using ScoreDigits = std::array<std::uint8_t, 8>;

struct ScoreState {
	ScoreDigits score{}, score_highest{};
	std::uint32_t score_delta{}, score_delta_frame{};
	std::uint8_t hiscore_popup_shown{};
	bool operator==(const ScoreState &) const = default;
};

// th05/main/score_highest.cpp. Do not substitute TH04's reset-only policy.
inline void score_highest_update_and_reset(ScoreState &state)
{
	enum class Comparison { lower, equal, higher };
	auto comparison = Comparison::equal;
	for(std::size_t i = state.score.size() - 1; i > 0; --i) {
		if(comparison == Comparison::equal) {
			if(state.score_highest[i] < state.score[i]) {
				comparison = Comparison::higher;
			} else if(state.score_highest[i] > state.score[i]) {
				comparison = Comparison::lower;
			}
		}
		if(comparison == Comparison::higher) {
			state.score_highest[i] = state.score[i];
		}
		state.score[i] = 0;
	}
	state.score_delta = 0;
	state.score_delta_frame = 0;
	state.hiscore_popup_shown = 0;
}

} // namespace th05::core
