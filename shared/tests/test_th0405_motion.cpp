#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {
int add_word(int a, int b)
{
	int sum = a + b;
	if(sum < -32768) sum += 65536;
	if(sum > 32767) sum -= 65536;
	return sum;
}

int clamp(int coordinate, int maximum)
{
	if(coordinate < 128) return 128;
	if(coordinate > maximum) return maximum;
	return coordinate;
}
}

int main()
{
	using namespace pc98::th0405;
	for(int coordinate = -32768; coordinate <= 32767; ++coordinate) {
		for(int delta : std::array<int, 13>{-32768, -6016, -129, -128, -16, -1, 0, 1, 16, 127, 128, 6016, 32767}) {
			const SubpixelPoint before{static_cast<std::int16_t>(coordinate), static_cast<std::int16_t>(-coordinate - 1)};
			const SubpixelPoint velocity{static_cast<std::int16_t>(delta), static_cast<std::int16_t>(-delta - 1)};
			Motion motion{before, {12, 34}, velocity};
			const auto expected_x = add_word(before.x, velocity.x);
			const auto expected_y = add_word(before.y, velocity.y);
			const auto result = update_motion(motion);
			if(motion.cur.x != expected_x || motion.cur.y != expected_y ||
				result != motion.cur || motion.prev != before || motion.velocity != velocity) {
				std::cerr << "Word motion or returned/current/previous state differs\n";
				return EXIT_FAILURE;
			}
			motion = {before, {56, 78}, velocity};
			player_pos_update_and_clamp(motion);
			if(motion.cur.x != clamp(expected_x, 6016) || motion.cur.y != clamp(expected_y, 5632) ||
				motion.prev != before || motion.velocity != velocity) {
				std::cerr << "Player clamp must follow word wrap and preserve old position/velocity\n";
				return EXIT_FAILURE;
			}
		}
	}
	std::cout << "Both axes checked across all 65536 coordinates and 13 deltas\n";
	return EXIT_SUCCESS;
}
