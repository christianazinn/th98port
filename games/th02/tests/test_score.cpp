#include "th02/core/score.hpp"

#include <array>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <limits>

namespace {
using namespace th02::core;

bool events_are(const std::optional<ScoreEvents> &result, std::initializer_list<ScoreEvent> expected)
{
	if(!result || result->count != expected.size()) return false;
	std::size_t i = 0;
	for(const auto &event : expected) if(result->entries[i++] != event) return false;
	return true;
}

std::int32_t signed_dword(std::int64_t value)
{
	value %= 4294967296LL;
	if(value < -2147483648LL) value += 4294967296LL;
	if(value > 2147483647LL) value -= 4294967296LL;
	return static_cast<std::int32_t>(value);
}

bool test_transfer_model()
{
	// Independent widened-integer model over all 16-bit previous transfers.
	for(unsigned previous = 0; previous < 65536; ++previous) {
		for(const auto delta : std::array<std::int32_t, 17>{
			-2147483647 - 1, -65537, -65536, -1, 0, 1, 31, 32, 33,
			1111, 3111, 65535, 65536, 50000, 100000, 200000, 2147483647}) {
			for(bool reduced : {false, true}) {
				ScoreState state{700, delta, 1000000, static_cast<std::uint16_t>(previous), 5, 3, 3};
				std::int64_t transfer = previous;
				std::int32_t remainder = 0;
				if(delta == 0) {
					transfer = 0;
				} else {
					const std::int64_t candidate = delta < 32 ? 1 : delta / 32;
					if(reduced) transfer = 3111;
					else if(candidate > transfer) transfer = candidate > 1111 ? 1111 : candidate;
					remainder = signed_dword(static_cast<std::int64_t>(delta) - transfer);
					if(remainder < 0) {
						transfer = (static_cast<std::int64_t>(delta) % 65536 + 65536) % 65536;
						remainder = 0;
					}
				}
				const auto events = score_update(state, reduced);
				if(!events || state.score != 700 + transfer || state.score_delta != remainder ||
					state.score_delta_transferred_prev != transfer || state.hiscore != 1000000 ||
					state.extends_gained != 5 || state.lives != 3 || state.bombs != 3 ||
					events->count != (delta == 0 ? 0U : 1U)) return false;
				if(delta != 0 && events->entries[0] != ScoreEvent{ScoreEventKind::score, state.score}) return false;
			}
		}
	}
	return true;
}

bool test_thresholds_and_bonus()
{
	constexpr std::array<std::int32_t, 6> thresholds{100000, 200000, 300000, 500000, 800000, 99999999};
	for(std::size_t i = 0; i < thresholds.size(); ++i) {
		ScoreState state{thresholds[i] - 1, 100, 0, 0, static_cast<std::uint16_t>(i), 3, 2};
		const auto events = score_update(state, false);
		if(state.score != thresholds[i] + 2 || state.score_delta != 97 ||
			state.extends_gained != i + 1U || state.lives != 4 || state.bombs != 2 ||
			!events_are(events, {{ScoreEventKind::score, thresholds[i] + 2},
				{ScoreEventKind::hiscore, thresholds[i] + 2}, {ScoreEventKind::lives, 4},
				{ScoreEventKind::sound, 8}})) return false;
	}
	for(int bombs : {2, 5}) {
		ScoreState state{99999, 1, 99999, 0, 0, 5, static_cast<std::int8_t>(bombs)};
		const auto events = score_update(state, false);
		if(state.lives != 5 || state.bombs != (bombs == 5 ? 5 : 3) || state.extends_gained != 1) return false;
		if(bombs == 2) {
			if(!events_are(events, {{ScoreEventKind::score, 100000}, {ScoreEventKind::hiscore, 100000},
				{ScoreEventKind::bombs, 3}, {ScoreEventKind::sound, 8}})) return false;
		} else if(!events_are(events, {{ScoreEventKind::score, 100000},
			{ScoreEventKind::hiscore, 100000}, {ScoreEventKind::sound, 8}})) return false;
	}
	ScoreState bonus{99990, 10, 99990, 99, 0, 5, 2};
	if(!events_are(score_grant_current_delta_as_bonus(bonus),
		{{ScoreEventKind::score, 100000}, {ScoreEventKind::hiscore, 100000}}) ||
		bonus.score_delta != 10 || bonus.score_delta_transferred_prev != 99 ||
		bonus.extends_gained != 1 || bonus.lives != 5 || bonus.bombs != 2) return false;
	if(!score_grant_current_delta_as_bonus(bonus) || bonus.score != 100010 || bonus.score_delta != 10) return false;
	// Jumping over several thresholds grants only one on each nonempty call.
	ScoreState jump{0, 800001, 0, 0, 0, 3, 3};
	if(!score_grant_current_delta_as_bonus(jump) || jump.extends_gained != 1 || jump.lives != 4) return false;
	if(!score_grant_current_delta_as_bonus(jump) || jump.extends_gained != 2 || jump.lives != 5) return false;
	return true;
}

bool test_boundaries_and_sequences()
{
	ScoreState overflow{std::numeric_limits<std::int32_t>::max(), 1, 0, 0, 0, 3, 3};
	if(!events_are(score_update(overflow, false), {{ScoreEventKind::score, std::numeric_limits<std::int32_t>::min()}})) return false;
	ScoreState negative{0, -1, 0, 0, 0, 3, 3};
	if(!score_update(negative, false) || negative.score != 65535 || negative.score_delta != 0 ||
		negative.score_delta_transferred_prev != 65535) return false;
	for(auto delta : {50000, 100000}) {
		ScoreState toggle{0, delta, 1000000, 3111, 0, 3, 3};
		if(!score_update(toggle, false) || toggle.score_delta_transferred_prev != (delta == 50000 ? 3111 : 1111)) return false;
	}
	ScoreState invalid{5, 1, 0, 17, 6, 3, 3};
	const auto before = invalid;
	if(score_update(invalid, false) || score_grant_current_delta_as_bonus(invalid) || invalid != before) return false;
	invalid.score_delta = 0;
	if(!events_are(score_update(invalid, false), {}) || invalid.score_delta_transferred_prev != 0 ||
		invalid.score != 5 || invalid.extends_gained != 6) return false;
	ScoreState sequence{1234, 123456, 0, 0, 0, 3, 3};
	unsigned calls = 0;
	while(sequence.score_delta != 0 && calls < 10000) {
		if(!score_update(sequence, false)) return false;
		++calls;
	}
	if(sequence.score != 124690 || sequence.score_delta != 0 || sequence.extends_gained != 1 || sequence.lives != 4) return false;
	return events_are(score_update(sequence, false), {}) && sequence.score_delta_transferred_prev == 0;
}

bool test_lifecycle()
{
	for(const auto score : std::array<std::int32_t, 15>{
		-2147483647 - 1, -1, 0, 99999, 100000, 199999, 200000, 299999,
		300000, 499999, 500000, 799999, 800000, 99999998, 99999999}) {
		ScoreState state{score, 17, 987654, 71, 555, 4, 2, 12};
		auto expected = state;
		if(score >= 99999999) {
			if(score_extend_init(state) || state != expected) return false;
		} else {
			unsigned passed = 0;
			for(auto threshold : {100000, 200000, 300000, 500000, 800000}) {
				if(score >= threshold) ++passed;
			}
			expected.extends_gained = static_cast<std::uint16_t>(passed);
			expected.score_delta = 0;
			if(!score_extend_init(state) || state != expected) return false;
		}
	}
	for(auto score : std::array<std::int32_t, 7>{-2147483647 - 1, -1, 0, 1, 99999, 1234567, 2147483647}) {
		for(auto delta : std::array<std::int32_t, 7>{-2147483647 - 1, -1, 0, 1, 99999, 1234567, 2147483647}) {
			ScoreState state{score, delta, 987654, 71, 555, 4, 2, 12};
			auto expected = state;
			expected.score = signed_dword(static_cast<std::int64_t>(score) + delta);
			expected.score_delta = 0;
			expected.score_delta_transferred_prev = 0;
			score_delta_commit(state);
			if(state != expected) return false;
			expected.score = 0;
			expected.score_reset_unknown_40000 = 40000;
			score_reset(state);
			if(state != expected) return false;
		}
	}
	// Extend initialization does NOT clear the previous frame's transfer.
	ScoreState reused{100000, 99, 0, 2000, 0, 3, 3, 0};
	if(!score_extend_init(reused) || reused.score_delta_transferred_prev != 2000) return false;
	reused.score_delta = 50000;
	if(!score_update(reused, false) || reused.score != 102000 || reused.score_delta != 48000) return false;
	return true;
}
}

int main()
{
	if(!test_transfer_model() || !test_thresholds_and_bonus() || !test_boundaries_and_sequences() || !test_lifecycle()) {
		std::cerr << "TH02 score transfer, bonus, extend, or ordered event mismatch\n";
		return EXIT_FAILURE;
	}
	std::cout << "TH02 score transfer/bonus state and ordered HUD/SFX requests checked\n";
	return EXIT_SUCCESS;
}
