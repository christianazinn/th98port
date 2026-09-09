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
	stage1_fights_setup(s);
	if (s.midboss.frames_until != 2500 || s.midboss.hp != 1000) return EXIT_FAILURE;
	s.page_back = 1;
	if (stage1_tick(s, 0x1000, false, p) != StageResult::pause || s.stage_frame != 0)
		return EXIT_FAILURE;
	if (stage1_tick(s, 0, false, p) != StageResult::dialog || !s.dialog_pending ||
		s.stage_frame != 0 || s.boss_active) return EXIT_FAILURE;
	if (!stage1_dialog_complete(s) || s.dialog_pending || !s.boss_active ||
		stage1_dialog_complete(s)) return EXIT_FAILURE;
	s.stage_clear_requested = true;
	if (stage1_tick(s, 0, false, p) != StageResult::cleared || s.stage_frame != 0)
		return EXIT_FAILURE;
	s.stage_clear_requested = false;
	s.gameover_pending = true;
	if (stage1_tick(s, 0, false, p) != StageResult::gameover || s.stage_frame != 0)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
