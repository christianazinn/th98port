#include "th01/core/state.hpp"
#include "pc98/random.hpp"

#include <cstdlib>
#include <iostream>

namespace {
using namespace th01::core;
int failures{};
void check(bool condition, const char *message)
{
	if(!condition) { std::cerr << message << '\n'; ++failures; }
}
State start()
{
	State s{};
	s.stage_id = 19;
	s.route = 1;
	s.orb.horizontal.left = 600;
	s.orb.top = 280;
	shootout_lasers_init(s);
	konngara_setup(s);
	return s;
}
std::size_t pellets(const State &s)
{
	std::size_t count{};
	for(const auto &pellet : s.projectiles.pellets) if(pellet.moving) ++count;
	return count;
}
}

int main()
{
	auto s = start();
	s.frame_rand = 456;
	check(konngara_update(s) == BossUpdateResult::running && s.boss.phase == 1 &&
		s.random_seed == 456 && s.projectiles.pellet_interlace,
		"Konngara enters phase 1 with source RNG seed and interlaced pellets");

	s = start();
	s.boss.phase = 7;
	s.boss.konngara.pattern_current = 2;
	s.boss.phase_frame = 99;
	auto seed = s.random_seed;
	(void)pc98::irand(seed);
	(void)konngara_update(s);
	check(s.boss.konngara.snakes_two.left[1][0] == 999 && s.random_seed == seed,
		"Second snake uses the positive -PIXEL_NONE sentinel and consumes no spawn RNG");
	(void)pc98::irand(seed);
	(void)konngara_update(s);
	check(s.random_seed == seed, "Only active snake consumes homing RNG");

	s = start();
	s.boss.phase = 7;
	s.boss.konngara.pattern_current = 7;
	s.boss.phase_frame = 139;
	(void)konngara_update(s);
	check(s.boss.konngara.slash_rain.left == 409 && s.boss.konngara.slash_rain.top == 229,
		"Slash triangle leg preserves both negative source divisions");
	check(s.boss.konngara.slash_triangular.left == 0 && s.boss.konngara.slash_aimed.left == 0,
		"Three slash routines retain independent static spawners");

	s = start();
	s.boss.phase = 7;
	s.boss.konngara.pattern_current = 10;
	s.boss.phase_frame = 99;
	(void)konngara_update(s);
	check(pellets(s) == 10 && s.projectiles.pellets[1].velocity_x == 30 &&
		s.projectiles.pellets[1].velocity_y == -10,
		"Sleeve rain emits ten pellets across uint8 angle wrap");

	s = start();
	s.boss.phase = 7;
	s.boss.konngara.pattern_current = 6;
	s.boss.phase_frame = 359;
	s.boss.konngara.swordray_end_y = 264;
	s.boss.konngara.pattern_state = 5;
	(void)konngara_update(s);
	check(s.boss.phase_frame == 0 && s.boss.konngara.swordray_end_y == 256 && pellets(s) == 2,
		"Edge rain emits its final pair after resetting phase frame to zero");

	s = start();
	s.boss.phase = 7;
	s.boss.konngara.pattern_current = 0;
	s.boss.phase_frame = 300;
	s.boss.konngara.pattern_state = 16;
	s.boss.konngara.diamonds_at_edges_frame = 15;
	seed = s.random_seed;
	for(int i = 0; i < 9; ++i) (void)pc98::irand(seed);
	(void)konngara_update(s);
	check(s.random_seed == seed && pellets(s) == 5,
		"Diamond edge rain preserves nine ordered source random draws");

	// All twelve final-phase patterns, on all four ranks, must finish from
	// their actual frame-1 entry. This is synthetic reachability, not parity.
	for(std::int8_t rank = 0; rank < 4; ++rank) {
		for(std::int16_t pattern = 0; pattern < 12; ++pattern) {
			s = start();
			s.rank = rank;
			s.boss.phase = 7;
			s.boss.phase_frame = 1;
			s.boss.konngara.pattern_current = pattern;
			bool finished{};
			for(int tick = 0; tick < 2000; ++tick) {
				if(konngara_update(s) != BossUpdateResult::running) break;
				if(s.boss.konngara.pattern_current == 99) { finished = true; break; }
			}
			if(!finished) std::cerr << "rank=" << static_cast<int>(rank) << " pattern=" << pattern << '\n';
			check(finished, "Reachable Konngara pattern must complete");
		}
	}
	s = start();
	s.boss.phase = 7;
	s.boss.konngara.pattern_current = 99;
	s.boss.hp = 0;
	(void)konngara_update(s);
	check(s.game_cleared && s.score == 50000 && s.boss.phase_frame == 65,
		"Final defeat preserves score additions and final shake counter");
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
