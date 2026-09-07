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
	Motion motion{{130, 5620}, {-1, -1}, {}};
	if(player_move(motion.velocity, 6) != MoveResult::valid) return EXIT_FAILURE;
	player_pos_update_and_clamp(motion);
	if(motion.cur.x != 128 || motion.cur.y != 5632 || motion.prev.x != 130 ||
		motion.prev.y != 5620 || motion.velocity.x != -48 || motion.velocity.y != 48) {
		std::cerr << "TH04 movement/clamp composition differs\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
