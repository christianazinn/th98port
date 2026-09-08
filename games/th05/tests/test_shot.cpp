#include "th05/core/shot.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {
using namespace th05::core;

bool test_free_and_occupied()
{
	for(unsigned index = 0; index < hitshot_count; ++index) {
		for(unsigned age = 0; age < 256; ++age) {
			HitShotState state{};
			for(auto &entry : state.hitshots) entry = {9, 71, {{1, 2}, {3, 4}, {5, 6}}};
			state.next_free_id = static_cast<std::uint16_t>(index);
			state.hitshots[index].age = static_cast<std::uint8_t>(age);
			auto expected = state;
			expected.next_free_id = static_cast<std::uint16_t>((index + 1) % 24);
			Shot shot{77, 81, {{101, -102}, {201, -202}, {-13, 17}}, 20, 3, -7, 99};
			auto shot_expected = shot;
			shot_expected.flag = shot_remove;
			if(age == 0) expected.hitshots[index] = {1, 28, {{101, -102}, {201, -202}, {-2, 2}}};
			if(!hitshot_from(state, shot) || state != expected || shot != shot_expected) return false;
		}
	}
	return true;
}

bool test_sprite_and_division()
{
	for(int value = -32768; value < 32768; ++value) {
		HitShotState state{};
		Shot shot{};
		shot.patnum_base = static_cast<std::uint8_t>(value);
		shot.pos.velocity = {static_cast<std::int16_t>(value), static_cast<std::int16_t>(-1 - value)};
		if(!hitshot_from(state, shot)) return false;
		const auto &result = state.hitshots[0];
		// Independent sign/magnitude model makes rounding toward zero explicit.
		const auto divide = [](int v) { return v < 0 ? -((-v) / 6) : v / 6; };
		if(result.patnum != (shot.patnum_base == 20 ? 28 : 32) ||
			result.pos.velocity.x != divide(value) ||
			result.pos.velocity.y != divide(-1 - value)) return false;
	}
	return true;
}

bool test_invalid_cursor()
{
	for(unsigned cursor : {24U, 25U, 65535U}) {
		HitShotState state{};
		state.next_free_id = static_cast<std::uint16_t>(cursor);
		Shot shot{};
		shot.flag = shot_alive;
		const auto before = state;
		const auto shot_before = shot;
		if(hitshot_from(state, shot) || state != before || shot != shot_before) return false;
	}
	return true;
}
}

int main()
{
	if(!test_free_and_occupied() || !test_sprite_and_division() || !test_invalid_cursor()) {
		std::cerr << "TH05 hitshot test failed\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
