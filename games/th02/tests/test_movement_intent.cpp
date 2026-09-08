#include "th02/core/player.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
// Independent lookup/bit model, not the implementation's nested switches.
struct Direction { int x, y; };
constexpr std::array<Direction, 16> aligned{{
	{0,0}, {0,-1}, {0,1}, {0,0}, {-1,0}, {-1,-1}, {-1,1}, {0,0},
	{1,0}, {1,-1}, {1,1}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
}};

th02::core::MovementIntent model(unsigned input,
	th02::core::MovementSpeeds speeds, unsigned page)
{
	using namespace th02::core;
	auto direction = aligned[input % 16U];
	const unsigned diagonal = input / 4096U;
	if(diagonal && ((diagonal & (diagonal - 1U)) == 0U)) {
		direction = {(diagonal & 5U) ? -1 : 1, (diagonal & 3U) ? -1 : 1};
	}
	const bool both = direction.x && direction.y;
	int x = direction.x * (both ? speeds.diagonal_x : speeds.aligned_x);
	int y = direction.y * (both ? speeds.diagonal_y : speeds.aligned_y);
	if(page && speeds.aligned_x == 5) {
		x -= (x > 0) - (x < 0);
		y -= (y > 0) - (y < 0);
	}
	return {{static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)},
		direction.x < 0 ? PlayerPose::left :
		(direction.x > 0 ? PlayerPose::right : PlayerPose::still)};
}
}

int main()
{
	using namespace th02::core;
	for(unsigned input = 0; input < 65536; ++input) {
		for(const auto speeds : std::array<MovementSpeeds, 5>{{
			{5,5,3,3}, {4,6,2,3}, {-128,127,-128,127}, {5,-128,0,-1}, {0,0,0,0}}}) {
			for(unsigned page = 0; page < 2; ++page) {
				if(movement_intent(static_cast<std::uint16_t>(input), speeds, page) !=
				   model(input, speeds, page)) return EXIT_FAILURE;
			}
		}
	}
	// Exercise each signed byte value independently on each speed axis.
	for(int speed = -128; speed <= 127; ++speed) {
		for(unsigned input = 0; input < 256; ++input) {
			const unsigned bits = ((input / 16U) * 4096U) + input % 16U;
			for(unsigned axis = 0; axis < 4; ++axis) {
				MovementSpeeds speeds{5,5,3,3};
				const auto byte = static_cast<std::int8_t>(speed);
				if(axis == 0) speeds.aligned_x = byte;
				if(axis == 1) speeds.aligned_y = byte;
				if(axis == 2) speeds.diagonal_x = byte;
				if(axis == 3) speeds.diagonal_y = byte;
				for(unsigned page = 0; page < 2; ++page) {
					if(movement_intent(static_cast<std::uint16_t>(bits), speeds, page) !=
					   model(bits, speeds, page)) return EXIT_FAILURE;
				}
			}
		}
	}
	if(movement_intent(1, {}, 2) ||
	   movement_intent(1, {}, std::numeric_limits<std::size_t>::max())) return EXIT_FAILURE;
	// Compose with the already-ported page-bound clamp. A valid dedicated
	// diagonal overrides contradictory aligned keys.
	const auto intent = movement_intent(0x100AU, {5,5,3,3}, 1);
	PlayerPositionState position{{{{100,200}, {29,17}}}, {999,999}};
	if(!intent || !player_move(position, 1, intent->delta.x, intent->delta.y) ||
	   position.current != ScreenPoint{28,16} || position.on_page[0] != ScreenPoint{100,200})
		return EXIT_FAILURE;
	std::cout << "TH02 movement intent tests passed\n";
	return EXIT_SUCCESS;
}
