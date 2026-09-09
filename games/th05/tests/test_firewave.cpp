#include "th05/core/state.hpp"
#include "th05/core/firewave.hpp"

using namespace th05::core;
int main()
{
	State s;
	firewaves_add(s, 128, false);
	firewaves_add(s, 64, true);
	const auto full = s.firewaves;
	firewaves_add(s, 1, false);
	if (s.firewaves != full || s.firewaves[0].bottom != 16 ||
		!s.firewaves[1].is_right) return 1;
	for (unsigned frame = 0; frame < 147; ++frame) firewaves_update(s);
	if (!s.firewaves[0].alive || s.firewaves[0].bottom != 604) return 2;
	firewaves_update(s);
	if (s.firewaves[0].alive || s.firewaves[0].bottom != 608) return 3;
	// bottom80: row16 uses angle32, sin=181/256; 128px amplitude gives
	// x90+16. Left hittest is strictly x<74, with y in [16,32).
	s.firewaves = {};
	s.firewaves[0] = {true, false, 80, 128};
	s.player_pos.cur = {73 * 16, 16 * 16};
	firewaves_render_hittest(s);
	if (!s.player_is_hit) return 4;
	s.player_is_hit = false;
	s.player_pos.cur.x = 74 * 16;
	firewaves_render_hittest(s);
	if (s.player_is_hit) return 5;
	s.player_pos.cur = {0, 15 * 16};
	firewaves_render_hittest(s);
	if (s.player_is_hit) return 6;
	// Right side: x416-90+16-32=310; equality remains safe.
	s.firewaves[0].is_right = true;
	s.player_pos.cur = {310 * 16, 16 * 16};
	firewaves_render_hittest(s);
	if (s.player_is_hit) return 7;
	s.player_pos.cur.x = 311 * 16;
	firewaves_render_hittest(s);
	if (!s.player_is_hit) return 8;
	return 0;
}
