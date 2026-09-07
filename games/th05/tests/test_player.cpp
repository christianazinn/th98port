#include "th05/core/player.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
// Independent dispatch model of th05/player.asm's BL/BH checks and jump
// table. This is intentionally not the C++ helper's input switch.
struct Direction { int x, y; };
constexpr std::array<Direction, 20> dispatch{{
	{0, 0}, {0, -1}, {0, 1}, {9, 9}, {-1, 0}, {-1, -1}, {-1, 1}, {9, 9},
	{1, 0}, {1, -1}, {1, 1}, {9, 9}, {-1, -1}, {1, -1}, {9, 9}, {-1, 1},
	{9, 9}, {9, 9}, {9, 9}, {1, 1},
}};

int model(th05::core::Velocity &velocity, unsigned input, int aligned, int diagonal)
{
	unsigned bx = input;
	if((bx % 256U) > 10U) return 0;
	const auto bh = ((bx / 256U) & 15U);
	bx = (bh * 256U) + (bx % 256U);
	if(bh != 0U) {
		if((bx % 256U) != 0U || bh > 8U) return 0;
		bx = bh + 11U;
	}
	const auto direction = dispatch[bx];
	if(direction.x == 9) return 0;
	if(direction.x == 0 && direction.y == 0) return 2;
	const auto speed = (direction.x && direction.y) ? diagonal : aligned;
	auto multiply = [speed](int sign) {
		int value = sign * speed;
		if(value == 32768) value = -32768;
		return static_cast<std::int16_t>(value);
	};
	if(direction.x != 0) velocity.x = multiply(direction.x);
	if(direction.y != 0) velocity.y = multiply(direction.y);
	return 1;
}
}

int main()
{
	using namespace th05::core;
	Velocity velocity{11, 22};
	if(player_move(velocity, 8, {40, 29}) != MoveResult::valid || velocity.x != 40 || velocity.y != 22 ||
		player_move(velocity, 0x0200, {40, 29}) != MoveResult::valid || velocity.x != 29 || velocity.y != -29) {
		std::cerr << "TH05 supplied aligned/diagonal speeds differ\n";
		return EXIT_FAILURE;
	}
	if(player_move(velocity, 5, {-96, -71}) != MoveResult::valid || velocity.x != 71 || velocity.y != 71 ||
		player_move(velocity, 1, {0, 0}) != MoveResult::valid || velocity.x != 71 || velocity.y != 0) {
		std::cerr << "TH05 negative/zero speeds or untouched axis differ\n";
		return EXIT_FAILURE;
	}
	if(player_move(velocity, 4, {std::numeric_limits<std::int16_t>::min(), 1}) != MoveResult::valid ||
		velocity.x != std::numeric_limits<std::int16_t>::min()) {
		std::cerr << "TH05 signed-word negation must wrap\n";
		return EXIT_FAILURE;
	}
	if(player_move(velocity, 0, {64, 48}) != MoveResult::no_input ||
		velocity.x != std::numeric_limits<std::int16_t>::min() || velocity.y != 0 ||
		player_move(velocity, 0x21, {64, 48}) != MoveResult::invalid ||
		velocity.x != std::numeric_limits<std::int16_t>::min() || velocity.y != 0) {
		std::cerr << "TH05 no-input/invalid input must not mutate velocity\n";
		return EXIT_FAILURE;
	}
	if(player_move(velocity, 0xF001, {17, 9}) != MoveResult::valid || velocity.y != -17 ||
		player_move(velocity, 0xF000, {17, 9}) != MoveResult::no_input || velocity.y != -17) {
		std::cerr << "TH05 assembly ignores high input bits before movement dispatch\n";
		return EXIT_FAILURE;
	}
	Motion motion{{6000, 140}, {-1, -1}, {}};
	if(player_move(motion.velocity, 9, {55, 41}) != MoveResult::valid) return EXIT_FAILURE;
	player_pos_update_and_clamp(motion);
	if(motion.cur.x != 6016 || motion.cur.y != 128 || motion.prev.x != 6000 ||
		motion.prev.y != 140 || motion.velocity.x != 41 || motion.velocity.y != -41) {
		std::cerr << "TH05 movement/clamp composition differs\n";
		return EXIT_FAILURE;
	}
	for(unsigned input = 0; input < 65536U; ++input) {
		for(const auto speeds : std::array<MovementSpeeds, 4>{{{64, 48}, {-91, 37}, {0, 0}, {-32768, 32767}}}) {
			velocity = {13, -27};
			auto expected = velocity;
			const auto expected_result = model(expected, input, speeds.aligned, speeds.diagonal);
			const auto result = player_move(velocity, static_cast<std::uint16_t>(input), speeds);
			if(static_cast<int>(result) != expected_result || velocity.x != expected.x || velocity.y != expected.y) {
				std::cerr << "TH05 full-word input dispatch differs at " << input << '\n';
				return EXIT_FAILURE;
			}
		}
	}
	return EXIT_SUCCESS;
}
