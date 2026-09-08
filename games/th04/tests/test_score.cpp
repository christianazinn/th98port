#include "th04/core/score.hpp"

#include <cstdlib>
#include <iostream>

namespace {
using namespace th04::core;

bool test_extends()
{
	// Independent table predicate: each case requires its own two lower
	// bounds, not a comparison against an integer score. Include non-BCD bytes.
	constexpr std::array<unsigned, 5> lower6{3, 8, 5, 2, 0};
	constexpr std::array<unsigned, 5> lower7{0, 0, 1, 2, 3};
	for(unsigned index = 0; index < 6; ++index) {
		for(unsigned digit6 = 0; digit6 < 256; ++digit6) {
			for(unsigned digit7 = 0; digit7 < 256; ++digit7) {
				ScoreState score{};
				score.score.fill(255);
				score.score[6] = static_cast<std::uint8_t>(digit6);
				score.score[7] = static_cast<std::uint8_t>(digit7);
				score.extends_gained = static_cast<std::uint8_t>(index);
				score.score_delta = 0xFFFFFFFFU;
				const auto before = score;
				ExtendState state{7, 16, 3, 1};
				const auto events = score_extend_update(score, state);
				const bool award = index < 5 && digit6 >= lower6[index] && digit7 >= lower7[index];
				auto expected_score = before;
				if(award) ++expected_score.extends_gained;
				const ExtendState expected_state = award ? ExtendState{11, 16, 4, 20} : ExtendState{7, 16, 3, 1};
				if(score != expected_score || state != expected_state) return false;
				if(events.count != (award ? 3U : 0U)) return false;
				if(award && (events.entries[0] != ScoreEvent{ScoreEventKind::lives, 4} ||
					events.entries[1] != ScoreEvent{ScoreEventKind::popup, 1} ||
					events.entries[2] != ScoreEvent{ScoreEventKind::sound, 7})) return false;
			}
		}
	}
	for(unsigned index = 5; index < 256; ++index) {
		ScoreState score{};
		score.score.fill(255);
		score.extends_gained = static_cast<std::uint8_t>(index);
		const auto before = score;
		ExtendState state{7, 16, 3, 1};
		const auto events = score_extend_update(score, state);
		if(events != ScoreEvents{} || score != before || state != ExtendState{7, 16, 3, 1}) return false;
	}
	for(unsigned performance = 0; performance < 256; ++performance) {
		for(unsigned cap = 0; cap < 256; ++cap) {
			ScoreState score{};
			score.score[6] = 3;
			ExtendState state{static_cast<std::uint8_t>(performance), static_cast<std::uint8_t>(cap), 100, 19};
			const auto events = score_extend_update(score, state);
			const auto raised = (performance + 4U) % 256U;
			if(state.playperf != (raised < cap ? raised : cap) || state.playperf_max != cap ||
				score.extends_gained != 1 || state.rem_lives != 100 ||
				state.bullet_clear_time != 19 || events != ScoreEvents{}) return false;
		}
	}
	for(unsigned lives = 0; lives < 256; ++lives) {
		for(unsigned clear = 0; clear < 256; ++clear) {
			ScoreState score{};
			score.score[6] = 3;
			ExtendState state{0, 16, static_cast<std::uint8_t>(lives), static_cast<std::uint8_t>(clear)};
			const auto events = score_extend_update(score, state);
			const bool awarded = lives < 100;
			if(score.extends_gained != 1 || state.playperf != 4 ||
				state.rem_lives != lives + static_cast<unsigned>(awarded) ||
				state.bullet_clear_time != (awarded && clear < 20 ? 20U : clear) ||
				events.count != (awarded ? 3U : 0U)) return false;
			if(awarded && events.entries[0].value != lives + 1) return false;
		}
	}
	// One threshold per call, and raw digit zero does not affect any threshold.
	ScoreState score{};
	score.score = {255, 0, 0, 0, 0, 0, 8, 3};
	ExtendState state{0, 16, 97, 40};
	for(unsigned i = 1; i <= 5; ++i) {
		const auto events = score_extend_update(score, state);
		if(score.extends_gained != i || state.rem_lives != (i < 3 ? 97U + i : 100U) ||
			events.count != (i <= 3 ? 3U : 0U) || score.score[0] != 255 ||
			state.bullet_clear_time != 40) return false;
	}
	return score_extend_update(score, state) == ScoreEvents{};
}
} // namespace

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
	if(!test_extends()) {
		std::cerr << "TH04 score extend contract failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "TH04 score reset/extend tests passed\n";
	return EXIT_SUCCESS;
}
