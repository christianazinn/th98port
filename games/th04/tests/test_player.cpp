#include "th04/core/player.hpp"

#include <cstdlib>
#include <iostream>

int main()
{
	using namespace th04::core;
	Velocity velocity{11, 22};
	if(player_move(velocity, 4) != MoveResult::valid || velocity.x != -64 || velocity.y != 22) {
		std::cerr << "TH04 aligned speed or untouched Y differs\n";
		return EXIT_FAILURE;
	}
	if(player_move(velocity, 0x0200) != MoveResult::valid || velocity.x != 48 || velocity.y != -48) {
		std::cerr << "TH04 dedicated diagonal speed differs\n";
		return EXIT_FAILURE;
	}
	if(player_move(velocity, 0) != MoveResult::no_input || velocity.x != 48 || velocity.y != -48 ||
		player_move(velocity, 0x24) != MoveResult::invalid || velocity.x != 48 || velocity.y != -48) {
		std::cerr << "TH04 helper must not clear velocity or accept unmasked shot input\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
