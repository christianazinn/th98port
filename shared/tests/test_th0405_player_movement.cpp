#include "pc98/th0405_player_movement.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {
using namespace pc98::th0405;
struct InputDirection { unsigned input; int x, y; };
constexpr std::array<InputDirection, 12> directions{{
	{1, 0, -1}, {2, 0, 1}, {4, -1, 0}, {8, 1, 0},
	{5, -1, -1}, {6, -1, 1}, {9, 1, -1}, {10, 1, 1},
	{256, -1, -1}, {512, 1, -1}, {1024, -1, 1}, {2048, 1, 1},
}};

// Numerical expectation uses range normalization, not the implementation's
// unsigned-word narrowing/bit_cast. Table lookup is separate from its switch.
std::int16_t signed_product(int speed, int direction)
{
	int value = speed * direction;
	if(value > 32767) value -= 65536;
	return static_cast<std::int16_t>(value);
}

bool check(unsigned input, int aligned, int diagonal, Velocity before)
{
	auto expected = before;
	MoveResult expected_result = input == 0 ? MoveResult::no_input : MoveResult::invalid;
	for(const auto &direction : directions) {
		if(direction.input != input) continue;
		const int speed = direction.x && direction.y ? diagonal : aligned;
		if(direction.x) expected.x = signed_product(speed, direction.x);
		if(direction.y) expected.y = signed_product(speed, direction.y);
		expected_result = MoveResult::valid;
		break;
	}
	auto actual = before;
	const auto result = player_move(actual, static_cast<std::uint16_t>(input),
		static_cast<std::int16_t>(aligned), static_cast<std::int16_t>(diagonal));
	if(result != expected_result || actual != expected) {
		std::cerr << "Movement differs: input=" << input << " aligned=" << aligned
			<< " diagonal=" << diagonal << '\n';
		return false;
	}
	return true;
}
}

int main()
{
	static_assert(static_cast<int>(MoveResult::invalid) == 0);
	static_assert(static_cast<int>(MoveResult::valid) == 1);
	static_assert(static_cast<int>(MoveResult::no_input) == 2);
	for(unsigned input = 0; input < 65536U; ++input) {
		if(!check(input, 64, 48, {}) || !check(input, -91, 37, {13, -27}) ||
			!check(input, -32768, 32767, {-32768, 32767}) || !check(input, 0, 0, {7, 11})) {
			return EXIT_FAILURE;
		}
	}
	for(int speed = -32768; speed <= 32767; ++speed) {
		for(const auto &direction : directions) {
			if(!check(direction.input, speed, -speed - 1, {23, -31})) return EXIT_FAILURE;
		}
		if(!check(0, speed, -speed - 1, {5, 7}) ||
			!check(3, speed, -speed - 1, {-1, 2})) return EXIT_FAILURE;
	}
	std::cout << "All 65536 input masks and all signed speed values checked\n";
	return EXIT_SUCCESS;
}
