#include "th05/core/std.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

int main()
{
	using namespace th05::core;
	// Three title strings, six map IDs, six scroll speeds, one enemy script,
	// marker and a single timed spawn record followed by the end marker.
	std::vector<std::uint8_t> file{0, 0,
		'S', 0, 'B', 'G', 0, 0,
		6, 0, 2, 4, 6, 8, 10,
		6, 16, 16, 16, 16, 16, 0,
		1, 2, 0xFF, 0,
		0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	file[0] = static_cast<std::uint8_t>(file.size() - 2);
	StdState state;
	state.enemy_scripts[1] = 1234;
	if (!std_load(state, file) || state.data.size() != file.size() - 2 ||
		state.title_offsets != std::array<std::uint16_t, 3>{0, 2, 5} ||
		state.map_section_offset != 11 || state.scroll_speed_offset != 18 ||
		state.scroll_speed != 6 || state.enemy_scripts[0] != 22 ||
		state.enemy_scripts[1] != 1234 || state.ip != 25 || !state.active) {
		std::cerr << "TH05 STD layout failure\n";
		return EXIT_FAILURE;
	}
	const auto saved = state.data;
	for (std::size_t length = 0; length < file.size(); ++length) {
		if (std_load(state, std::span(file).first(length)) || state.data != saved) {
			std::cerr << "TH05 truncated STD mutation\n";
			return EXIT_FAILURE;
		}
	}
	file[22] = 0; // Zero script count would overflow the source table.
	if (std_load(state, file) || state.data != saved) return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
