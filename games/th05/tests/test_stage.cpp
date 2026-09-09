#include "th05/core/stage.hpp"
#include "th05/core/state.hpp"
#include <cstdlib>

int main()
{
	using namespace th05::core;
	State s;
	Presentation p;
	if (!gameplay_init(s, 1, 0, 3, 3, 0)) return EXIT_FAILURE;
	s.random.seed = 318;
	s.boss.statebytes[0] = 71;
	s.bullets.bullet_template.speed = 23;
	s.items.dream = 77;
	stage_init(s, p);
	if (s.player_pos.cur != Point{3072, 5120} || s.player_invincibility_time != 64 ||
		s.boss.statebytes[0] != 71 || s.bullets.bullet_template.speed != 23 ||
		s.items.dream != 77 || s.random.cursor != 0 || s.shot_spawn.shot_time != 0)
		return EXIT_FAILURE;
	if (!stage_fights_setup(s)) return EXIT_FAILURE;
	if (s.midboss.frames_until != 2500 || s.midboss.hp != 1000) return EXIT_FAILURE;
	s.page_back = 1;
	if (stage_tick(s, 0x1000, false, p) != StageResult::pause || s.stage_frame != 0)
		return EXIT_FAILURE;
	if (stage_tick(s, 0, false, p) != StageResult::dialog || !s.dialog_pending ||
		s.stage_frame != 0 || s.boss_active) return EXIT_FAILURE;
	if (!stage_dialog_complete(s) || s.dialog_pending || !s.boss_active ||
		stage_dialog_complete(s)) return EXIT_FAILURE;
	s.stage_clear_requested = true;
	if (stage_tick(s, 0, false, p) != StageResult::cleared || s.stage_frame != 0)
		return EXIT_FAILURE;
	s.stage_clear_requested = false;
	s.gameover_pending = true;
	if (stage_tick(s, 0, false, p) != StageResult::gameover || s.stage_frame != 0)
		return EXIT_FAILURE;
	s.stage_id = 1;
	s.stage2_scenery.particles[0].flag = 1;
	s.stage2_scenery.flash_tone = 32;
	s.bonus_previous_miss_count = 2;
	s.bonus_previous_bombs_used = 3;
	stage_init(s, p);
	if (s.stage2_scenery.particles[0].flag != 0 ||
		s.stage2_scenery.flash_tone != 32 || s.bonus_previous_miss_count != 2 ||
		s.bonus_previous_bombs_used != 3) return EXIT_FAILURE;
	if (!stage_fights_setup(s) || s.midboss.frames_until != 2750 || s.midboss.hp != 1400)
		return EXIT_FAILURE;
	s.stage_id = 2;
	s.alice.puppets[0].flag = 1;
	s.alice.barrier_frame = 42;
	s.alice.puppet_patterns[0] = PuppetPattern::aimed_ring;
	stage_init(s, p);
	if (s.alice.puppets[0].flag != 0 || s.alice.barrier_frame != 42 ||
		s.alice.puppet_patterns[0] != PuppetPattern::aimed_ring) return EXIT_FAILURE;
	if (!stage_fights_setup(s) || s.midboss.frames_until != 5750 || s.midboss.hp != 1400)
		return EXIT_FAILURE;
	s.stage_id = 3;
	s.stage4.custom.b4balls[0].age = 12;
	s.stage4.cheeto_trails[0].color = 7;
	s.stage4.mai_laser_count = 42;
	s.swords.pool[0].twirl_time = 17;
	s.swords.sword_template.speed = 23;
	stage_init(s, p);
	if (s.stage4.custom.b4balls[0].age != 0 ||
		s.stage4.cheeto_trails[0].color != 7 || s.stage4.mai_laser_count != 42 ||
		s.swords.pool[0].twirl_time != 0 || s.swords.sword_template.speed != 0)
		return EXIT_FAILURE;
	if (!stage_fights_setup(s) || s.midboss.frames_until != 3900 || s.midboss.hp != 1100)
		return EXIT_FAILURE;
	s.stage_id = 4;
	stage_init(s, p);
	if (!stage_fights_setup(s) || s.midboss.frames_until != 4800 || s.midboss.hp != 1550 ||
		s.stage4.yuki.pos.cur != Point{3072, 1024}) return EXIT_FAILURE;
	s.stage_id = 5;
	s.shinki_background.particles[0].age = 17;
	s.stage6.devil_laser_grow_delay = 23;
	stage_init(s, p);
	if (s.shinki_background.particles[0].age != 17 ||
		s.stage6.devil_laser_grow_delay != 23) return EXIT_FAILURE;
	if (!stage_fights_setup(s) || s.midboss.frames_until != 30000 ||
		s.boss.pos.cur != Point{3072, 1024}) return EXIT_FAILURE;
	s.stage_id = 6;
	s.extra.pattern_origin_x = 123;
	stage_init(s, p);
	if (s.extra.pattern_origin_x != 123) return EXIT_FAILURE;
	if (!stage_fights_setup(s) || s.midboss.frames_until != 5800 ||
		s.midboss.hp != 3000) return EXIT_FAILURE;
	s.stage_id = 7;
	if (stage_fights_setup(s) || stage_tick(s, 0, false, p) != StageResult::unsupported)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
