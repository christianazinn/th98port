#include "th04/core/scroll.hpp"
#include "th04/core/state.hpp"
#include "th04/core/stage.hpp"
#include <iostream>

using namespace th04::core;
struct Trace : Presentation {
	int calls{};
	std::int16_t line{};
	void graph_scroll(std::int16_t value) noexcept override { ++calls; line = value; }
};
int main()
{
	State s;
	Trace p;
	std::vector<std::uint8_t> file(8 + 320, 0);
	file[0] = 64; file[1] = 1;
	for (std::size_t i = 0; i < 160; ++i) file[8 + i * 2] = static_cast<std::uint8_t>(i);
	if (!map_load(s, file)) return 1;
	s.std_data.data.assign(20, 0);
	s.tile_ring.fill(999);
	if (!tiles_fill_initial(s) || s.tile_ring[24 * 32] != 128 ||
		s.tile_ring[20 * 32] != 0 || s.tile_ring[24 * 32 + 24] != 999) return 2;
	s.std_data.map_section_id = 4;
	s.std_data.scroll_speed_offset = 10;
	s.std_data.data[11] = 16;
	s.std_data.scroll_speed = 16;
	s.scroll_active = true;
	if (!scroll_update(s, p) || s.scroll_line != 399 || s.scroll_last_delta != 16 ||
		s.std_data.map_section_id != 5 || s.std_data.tile_row_in_section != 4 ||
		s.scroll_lines_prev_frame != 1 || p.calls != 0) return 3;
	if (!scroll_update(s, p) || p.calls != 1 || p.line != 399 ||
		s.scroll_line_on_page[0] != 399 || s.scroll_line != 398) return 4;
	// Disabled drawing still advances the map and entity scroll delta.
	s.scroll_active = false;
	if (!scroll_update(s, p) || p.calls != 1 || s.scroll_line != 397) return 5;
	s.scroll_subpixel_line = 250;
	s.std_data.scroll_speed = 10;
	if (!scroll_update(s, p) || s.scroll_subpixel_line != 4 || s.scroll_last_delta != 0) return 6;
	s.scroll_line = 0;
	s.tile_ring_row_filled = 0;
	s.std_data.tile_row_in_section = 0;
	s.std_data.scroll_speed = 16;
	s.scroll_subpixel_line = 0;
	if (!scroll_update(s, p) || s.std_data.scroll_speed != 0 || s.scroll_line != 0 ||
		s.scroll_lines_pending != 0 || s.scroll_lines_prev_frame != 0) return 7;
	std::cout << "Map rows and scrolling timeline passed\n";
	State init;
	init.random.seed = 12345;
	if (!gameplay_init(init, 1, 0, 0, 3, 3, false)) return 8;
	init.shot_spawn.shot_time = 12;
	init.shot_spawn.shot_last_id = 19;
	init.player_pos.velocity = {7, 8};
	init.spark_ring_offset = 0x1200;
	auto expected = init.random;
	pc98::th0405::randring_fill(expected);
	const auto drop = static_cast<std::uint8_t>(pc98::irand(expected.seed) & 15);
	for (unsigned i = 0; i < 96; ++i) (void)pc98::irand(expected.seed);
	stage_init(init, p);
	if (init.random != expected || init.items.enemy_drop_ring_p != drop ||
		init.shot_spawn.shot_time != 0 || init.shot_spawn.shot_last_id != 19 ||
		init.spark_ring_offset != 0x1200 || init.player_pos.velocity != Point{7, 8} ||
		init.player_invincibility_time != 64 || init.player_pos.cur != Point{3072, 5120}) return 9;
	if (!gameplay_init(init, 1, 0, 0, 3, 3, true) || init.rank != 2 ||
		init.resources.playperf != 20 || !init.bullets.turbo_mode) return 10;
}
