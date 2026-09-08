#include "th05/core/score.hpp"
#include "th05/core/extend.hpp"

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

bool check_extends()
{
	using namespace th05::core;
	using pc98::th0405::ExtendEvent;
	using pc98::th0405::ExtendEventKind;
	// Independent quotient model, all byte indices and unsigned word counts.
	for(unsigned index = 0; index < 256; ++index) {
		for(unsigned count = 0; count < 65536; ++count) {
			PointExtendState points{static_cast<std::uint16_t>(count), static_cast<std::uint8_t>(index)};
			ExtendState state{7, 16, 2, 1};
			const auto events = point_items_extend_update(points, state);
			const bool award = count / 100U > index;
			if(points.extend_point_items_collected != count ||
				points.extends_gained != ((index + static_cast<unsigned>(award)) % 256U) ||
				state != (award ? ExtendState{11, 16, 3, 20} : ExtendState{7, 16, 2, 1}) ||
				events.count != (award ? 3U : 0U)) return false;
			if(award && (events.entries[0] != ExtendEvent{ExtendEventKind::lives, 3} ||
				events.entries[1] != ExtendEvent{ExtendEventKind::popup, 1} ||
				events.entries[2] != ExtendEvent{ExtendEventKind::sound, 7})) return false;
		}
	}
	for(unsigned lives = 0; lives < 256; ++lives) {
		for(unsigned clear = 0; clear < 256; ++clear) {
			PointExtendState points{100, 0};
			ExtendState state{0, 16, static_cast<std::uint8_t>(lives), static_cast<std::uint8_t>(clear)};
			const auto events = point_items_extend_update(points, state);
			const bool award = lives < 99;
			if(points.extends_gained != 1 || points.extend_point_items_collected != 100 ||
				state.playperf != 4 || state.rem_lives != lives + static_cast<unsigned>(award) ||
				state.bullet_clear_time != (award && clear < 20 ? 20U : clear) ||
				events.count != (award ? 3U : 0U)) return false;
		}
	}
	// Both a blocked life and an awarded life still consume the byte threshold;
	// wrap to zero makes the following invocation target 100 items again.
	for(std::uint8_t lives : {std::uint8_t{98}, std::uint8_t{99}}) {
		PointExtendState points{25600, 255};
		ExtendState state{252, 16, lives, 40};
		const auto events = point_items_extend_update(points, state);
		if(points.extends_gained != 0 || state.playperf != 0 || state.rem_lives != 99 ||
			events.count != (lives == 98 ? 3U : 0U)) return false;
		if(point_items_extend_update(points, state).count != 0 ||
			points.extends_gained != 1 || state.playperf != 4 || state.bullet_clear_time != 40) return false;
	}
	return true;
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
	if(!check_extends()) {
		std::cerr << "TH05 point-extend contract failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "TH05 score record/reset and point-extend tests passed\n";
	return EXIT_SUCCESS;
}
