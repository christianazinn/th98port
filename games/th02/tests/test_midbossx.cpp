#include "th02/core/stage_state.hpp"
#include <cstdlib>
#include <cstdio>
#include <vector>
using namespace th02::core;
struct Trace final : MidbossxCallbacks {
	struct Tile { int x, y, image; };
	std::vector<Tile> tiles;
	void tile_ring_set_and_put_both_8(std::int16_t x, std::int16_t y, std::int16_t image) noexcept override
	{ tiles.push_back({x, y, image}); }
};
int main()
{
	StageState s; Trace p; s.scroll.line = 50;
	s.frame.page_back = 0; s.frame.page_front = 1;
	const auto random = s.random;
	for (unsigned frame = 1; frame <= 421; ++frame) {
		if (midbossx_update_and_render(s, p) != EnemyBulletAddResult::completed) return EXIT_FAILURE;
		if (frame == 16 && (s.midbossx.bursts[0].cel != 1 || s.midbossx.bursts[0].frame != 1)) return EXIT_FAILURE;
		if (frame == 23 && (p.tiles.size() != 1 || p.tiles[0].image != 1 || p.tiles[0].x != 128 || p.tiles[0].y != 128)) return EXIT_FAILURE;
		if (frame == 63) {
			unsigned alive = 0; for (const auto &b : s.bullets.bullets) if (b.flag == enemy_bullet_alive) ++alive;
			if (alive != 4) return EXIT_FAILURE;
		}
	}
	if (p.tiles.size() != 51 * 6 || s.midbossx.active || s.boss.phase_frame != 0 || s.random != random) return EXIT_FAILURE;
	for (const auto &burst : s.midbossx.bursts) if (burst.cel) return EXIT_FAILURE;
	// The second cross wraps its upward point into the last VRAM rows.
	bool wrapped = false; for (const auto &tile : p.tiles) if (tile.x == 304 && tile.y == 384) wrapped = true;
	if (!wrapped) return EXIT_FAILURE;
	std::puts("Extra midboss queue and timeline passed");
}
