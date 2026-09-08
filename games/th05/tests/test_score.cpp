#include "th05/core/score.hpp"

#include <cstdlib>
#include <iostream>

namespace {
// Independent whole-value comparison. Base 256 intentionally tests unsigned
// byte ordering even outside decimal digits; the source does not sanitize them.
std::uint64_t value(const th05::core::ScoreDigits &digits)
{
	std::uint64_t sum = 0, place = 1;
	for(std::size_t i = 1; i < digits.size(); ++i) {
		sum += static_cast<std::uint64_t>(digits[i]) * place;
		place *= 256;
	}
	return sum;
}

bool check(th05::core::ScoreState state)
{
	const auto before = state;
	auto expected_highest = before.score_highest;
	if(value(before.score) > value(before.score_highest)) {
		expected_highest = before.score;
		expected_highest[0] = before.score_highest[0];
	}
	th05::core::score_highest_update_and_reset(state);
	if(state.score_highest != expected_highest ||
	   state.score[0] != before.score[0] || state.score_delta ||
	   state.score_delta_frame || state.hiscore_popup_shown) return false;
	for(std::size_t i = 1; i < state.score.size(); ++i) {
		if(state.score[i]) return false;
	}
	// Reset is idempotent and must not zero the stored record on the next call.
	const auto once = state;
	th05::core::score_highest_update_and_reset(state);
	return state == once;
}
}

int main()
{
	using namespace th05::core;
	// Every decisive digit and every unsigned byte comparison, with opposed
	// lower digits so they cannot overturn a higher-digit decision.
	for(std::size_t digit = 1; digit < 8; ++digit) {
		for(unsigned a = 0; a < 256; ++a) {
			for(unsigned b = 0; b < 256; ++b) {
				ScoreState state{};
				state.score.fill(137);
				state.score_highest.fill(137);
				for(std::size_t i = 1; i < digit; ++i) {
					state.score[i] = 255;
					state.score_highest[i] = 0;
				}
				state.score[digit] = static_cast<std::uint8_t>(a);
				state.score_highest[digit] = static_cast<std::uint8_t>(b);
				state.score[0] = static_cast<std::uint8_t>(b);
				state.score_highest[0] = static_cast<std::uint8_t>(a);
				state.score_delta = 0xFFFFFFFFU;
				state.score_delta_frame = 0x80000000U;
				state.hiscore_popup_shown = 255;
				if(!check(state)) return EXIT_FAILURE;
				for(std::size_t i = 1; i < digit; ++i) {
					state.score[i] = 0;
					state.score_highest[i] = 255;
				}
				if(!check(state)) return EXIT_FAILURE;
			}
		}
	}
	for(unsigned counter = 0; counter < 256; ++counter) {
		ScoreState tied{};
		tied.score.fill(9);
		tied.score_highest.fill(9);
		tied.score[0] = static_cast<std::uint8_t>(counter);
		tied.score_highest[0] = static_cast<std::uint8_t>(255 - counter);
		if(!check(tied)) return EXIT_FAILURE;
	}
	std::cout << "TH05 score record/reset tests passed\n";
	return EXIT_SUCCESS;
}
