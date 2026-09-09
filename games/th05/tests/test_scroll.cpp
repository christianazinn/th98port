#include "th05/core/scroll.hpp"
#include "th05/core/state.hpp"
#include <cstdlib>
#include <iostream>

int main()
{
	using namespace th05::core;
	State s;
	Presentation p;
	s.std_data.data.resize(32);
	s.std_data.map_section_offset = 7;
	for (unsigned i = 0; i < 6; ++i) s.std_data.data[3 + i] = static_cast<std::uint8_t>(i * 2);
	s.map_data.resize(6 * 320);
	for (unsigned section = 0; section < 6; ++section) {
		for (unsigned row = 0; row < 5; ++row) {
			for (unsigned x = 0; x < 24; ++x) {
				s.map_data[section * 320 + row * 64 + x * 2] = static_cast<std::uint8_t>(section * 5 + row);
			}
		}
	}
	s.tile_ring.fill(0xABCD);
	if (!tiles_fill_initial(s)) return EXIT_FAILURE;
	for (unsigned row = 0; row < 25; ++row) {
		const unsigned expected = (4 - row / 5) * 5 + row % 5;
		if (s.tile_ring[row * 32] != expected || s.tile_ring[row * 32 + 24] != 0xABCD) {
			std::cerr << "TH05 initial tile order failure\n";
			return EXIT_FAILURE;
		}
	}
	s.std_data.scroll_speed = 16;
	s.std_data.scroll_speed_offset = 20;
	s.std_data.data[21] = 16;
	if (!scroll_update(s, p) || s.scroll_line != 399 || s.scroll_last_delta != 16 ||
		s.std_data.map_section_offset != 8 || s.std_data.tile_row_in_section != 4 ||
		s.tile_ring[24 * 32] != 29) return EXIT_FAILURE;
	s.stage_id = 5;
	s.scroll_active = true;
	if (!scroll_update(s, p) || s.scroll_active) return EXIT_FAILURE;
	s.std_data.tile_row_in_section = 0;
	s.tile_ring_row_filled = -1;
	s.std_data.data[22] = 0;
	if (!scroll_update(s, p) || s.std_data.scroll_speed != 0 || s.scroll_line != 0 ||
		s.scroll_lines_prev_frame != 0) return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
