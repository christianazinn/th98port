#include "th04/core/score.hpp"

#include <cstdlib>
#include <iostream>

int main()
{
	using namespace th04::core;
	for(unsigned counter = 0; counter < 256; ++counter) {
		for(unsigned byte = 0; byte < 256; ++byte) {
			ScoreState state{};
			state.score.fill(static_cast<std::uint8_t>(byte));
			state.score[0] = static_cast<std::uint8_t>(counter);
			state.score_delta = 0xFFFFFFFFU;
			state.score_delta_frame = 0x80000000U;
			state.score_unused = static_cast<std::uint8_t>(byte);
			state.extends_gained = static_cast<std::uint8_t>(255 - byte);
			state.hiscore_popup_shown = 255;
			ScoreState expected{};
			expected.score[0] = static_cast<std::uint8_t>(counter);
			score_reset(state);
			if(state != expected) return EXIT_FAILURE;
			score_reset(state);
			if(state != expected) return EXIT_FAILURE;
		}
	}
	std::cout << "TH04 score reset tests passed\n";
	return EXIT_SUCCESS;
}
