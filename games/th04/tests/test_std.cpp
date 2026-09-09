#include "th04/core/std.hpp"
#include "th04/core/state.hpp"
#include <iostream>

using namespace th04::core;
std::vector<std::uint8_t> fixture()
{
	// Eight map bytes, five scroll bytes, one one-byte enemy script, marker,
	// then a frame-10 spawn and the zero-word end marker.
	std::vector<std::uint8_t> body(8, 0);
	body.insert(body.end(), {5, 16, 16, 16, 16, 16, 1, 1, 0, 0,
		10, 0, 1, 0, 0, 12, 0, 4, 1, 0, 0, 0, 0});
	std::vector<std::uint8_t> file{
		static_cast<std::uint8_t>(body.size() + 1), 0, 8};
	file.insert(file.end(), body.begin(), body.end());
	return file;
}
int main()
{
	StdState state;
	state.enemy_scripts[31] = 444;
	auto file = fixture();
	if (!std_load(state, file) || !state.active || state.map_section_id != 4 ||
		state.scroll_speed != 5 || state.scroll_speed_offset != 13 ||
		state.enemy_scripts[0] != 16 || state.enemy_scripts[31] != 444 ||
		state.ip != 18 || state.data[state.ip] != 10) return 1;
	const auto original = state.data;
	file.pop_back();
	if (std_load(state, file) || state.data != original) return 2;
	file = fixture();
	file[3 + 14] = 33;
	if (std_load(state, file)) return 3;
	file[3 + 14] = 0;
	if (std_load(state, file)) return 4;
	State game;
	Presentation presentation;
	if (!std_load(game.std_data, fixture())) return 5;
	game.midboss_active = true;
	game.stage_frame = 9;
	if (!std_run(game, presentation) || game.std_data.ip != 18) return 6;
	game.stage_frame = 10;
	if (!std_run(game, presentation) || game.std_data.ip != 29 ||
		game.std_data.active) return 7;
	// The source's byte do-while executes 256 skips for a zero count.
	game.std_data.data.assign(2053, 0);
	game.std_data.data[0] = 5;
	game.std_data.ip = 0;
	game.std_data.active = true;
	game.stage_frame = 5;
	if (!std_run(game, presentation) || game.std_data.ip != 2051 ||
		game.std_data.active) return 8;
	std::cout << "STD loading and spawn scheduling passed\n";
}
