#include "th04/core/stage.hpp"
#include "th04/core/state.hpp"
#include <iostream>
using namespace th04::core;
int main()
{
	State s;
	Presentation p;
	if (!gameplay_init(s, 1, 0, 0, 3, 3, false)) return 1;
	s.random.seed = 318;
	stage_init(s, p);
	if (!stage_fights_setup(s)) return 8;
	// No stage data is read when a modal boundary blocks admission.
	if (stage_tick(s, 0x1000, false, p) != StageResult::pause || s.stage_frame != 0) return 2;
	s.page_back = 1;
	if (stage_tick(s, 0, false, p) != StageResult::dialog || !s.dialog_pending ||
		s.boss_active || s.stage_frame != 0) return 3;
	if (stage_tick(s, 0x20, false, p) != StageResult::dialog || s.stage_frame != 0) return 4;
	if (!stage_dialog_complete(s) || !s.boss_active || s.dialog_pending ||
		stage_dialog_complete(s)) return 5;
	s.gameover_pending = true;
	if (stage_tick(s, 0, false, p) != StageResult::gameover) return 6;
	s.gameover_pending = false;
	s.stage_clear_requested = true;
	if (stage_tick(s, 0, false, p) != StageResult::cleared) return 7;
	s.stage_id = 1;
	s.boss.kurumi_spawnrays[0].flag = KurumiSpawnrayFlag::grow;
	s.miss_count = 2;
	s.bombs_used = 3;
	stage_init(s, p);
	if (s.boss.kurumi_spawnrays[0].flag != KurumiSpawnrayFlag::free || s.miss_count != 2 || s.bombs_used != 3) return 11;
	if (!stage_fights_setup(s) || s.midboss.frames_until != 2600 || s.midboss.hp != 750) return 9;
	s.stage_id = 2;
	s.boss.elly_boomerang.flag = EllyBoomerangFlag::thrown;
	stage_init(s, p);
	if (s.boss.elly_boomerang.flag != EllyBoomerangFlag::free) return 13;
	if (!stage_fights_setup(s) || s.midboss.frames_until != 1600 || s.midboss.hp != 850) return 12;
	s.stage_id = 3;
	s.boss.reimu_orbs[0].spin_time = 12;
	s.boss.marisa_bits[0].distance = 12;
	s.boss.reimu_pattern8_angle = 42;
	stage_init(s, p);
	if (s.boss.reimu_orbs[0].spin_time != 0 || s.boss.marisa_bits[0].distance != 0 ||
		s.boss.reimu_pattern8_angle != 42) return 14;
	if (!stage_fights_setup(s) || s.midboss.frames_until != 2800 || s.midboss.hp != 1200) return 15;
	s.stage_id = 4;
	stage_init(s, p);
	if (!stage_fights_setup(s) || s.midboss.frames_until != 60000) return 16;
	s.stage_id = 5;
	if (stage_fights_setup(s) || stage_tick(s, 0, false, p) != StageResult::unsupported) return 10;
	std::cout << "Stage admission and explicit modal boundaries passed\n";
}
