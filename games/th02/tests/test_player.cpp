#include "th02/core/player.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
int failures{};
void check(bool condition, const char *message)
{
	if(!condition) { ++failures; std::cerr << message << '\n'; }
}

// Signed arithmetic model without narrowing or relying on host signed casts.
int wrapped_add(int value, int delta)
{
	int result = value + delta;
	if(result < -32768) result += 65536;
	if(result > 32767) result -= 65536;
	return result;
}
int clamp(int value, int low, int high)
{
	return value < low ? low : (value > high ? high : value);
}
}

int main()
{
	using namespace th02::core;
	PlayerPositionState state{{{{100, 200}, {300, 250}}}, {-100, -200}};
	const auto before = state;
	check(!player_move(state, 2, 1, 1) && state == before, "Page 2 rejected without mutation");
	check(!player_move(state, std::numeric_limits<std::size_t>::max(), 1, 1) &&
		state == before, "Large page rejected without mutation");
	check(player_move(state, 1, 3, -4) && state.current == ScreenPoint{303, 246} &&
		state.on_page[1] == state.current && state.on_page[0] == before.on_page[0],
		"Reads selected page, not cached/current or front page; updates both copies");
	check(player_move(state, 0, 0, 0) && state.current == ScreenPoint{100, 200},
		"Changing page source is explicit");

	for(std::size_t page = 0; page < 2; ++page) {
		for(const int delta : std::array<int, 11>{
			-32768, -129, -128, -8, -1, 0, 1, 8, 127, 128, 32767}) {
			for(int coordinate = -32768; coordinate <= 32767; ++coordinate) {
				state = {};
				state.on_page[page] = {static_cast<std::int16_t>(coordinate),
					static_cast<std::int16_t>(coordinate)};
				state.on_page[1U - page] = {-23, -42};
				state.current = {1234, 5678};
				const auto expected_x = clamp(wrapped_add(coordinate, delta), 28, 390);
				const auto expected_y = clamp(wrapped_add(coordinate, -delta - 1), 16, 344);
				const auto accepted = player_move(state, page, static_cast<std::int16_t>(delta),
					static_cast<std::int16_t>(-delta - 1));
				if(!accepted || state.current.x != expected_x || state.current.y != expected_y ||
					state.on_page[page] != state.current || state.on_page[1U - page] != ScreenPoint{-23, -42}) {
					check(false, "All 16-bit coordinates: wrap before clamps, unchanged other page");
					return EXIT_FAILURE;
				}
			}
		}
	}
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
