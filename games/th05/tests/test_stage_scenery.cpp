#include "th05/core/state.hpp"
#include <cstdlib>
int main()
{
	using namespace th05::core;
	State s;
	Presentation p;
	s.tile_ring.fill(123);
	s.stage_frame = 256;
	if (!stage2_scenery_update(s, p) || s.scroll_active || s.stage2_scenery.flash_tone != 2 ||
		s.palette_tone != 100 || s.tile_ring[11] != 20552 || s.tile_ring[12] != 20552 ||
		s.tile_ring[10] != 123) return 1;
	for (s.stage_frame = 257; s.stage_frame < 330; ++s.stage_frame)
		if (!stage2_scenery_update(s, p)) return 2;
	if (s.stage2_scenery.flash_tone != 0 || !s.scroll_active || s.tile_ring[0] != 72 ||
		s.tile_ring[24] != 123 || s.stage2_scenery.spawned != 0) return 3;
	s.random.seed = 318;
	if (!stage2_scenery_update(s, p) || s.stage2_scenery.spawned != 1 ||
		s.random.cursor != 1 || s.random.seed != 318 || s.stage2_scenery.particles[0].zoom != 0) return 4;
	s.stage_frame = 331;
	s.stage2_scenery.particles[0].pos.cur = {-16000, -16000};
	if (!stage2_scenery_update(s, p) || s.random.cursor != 2 || s.random.seed == 318 ||
		s.stage2_scenery.particles[0].zoom != 8 || s.stage2_scenery.particles[0].pos.cur.y != 0) return 5;
	const auto cursor = s.random.cursor;
	s.boss.phase = 2;
	s.stage_frame = 332;
	if (!stage2_scenery_update(s, p) || s.random.cursor != cursor || s.stage2_scenery.spawned != 1) return 6;
	return EXIT_SUCCESS;
}
