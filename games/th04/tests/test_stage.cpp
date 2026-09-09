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
	stage1_fights_setup(s);
	// No stage data is read when a modal boundary blocks admission.
	if (stage1_tick(s, 0x1000, false, p) != StageResult::pause || s.stage_frame != 0) return 2;
	s.page_back = 1;
	if (stage1_tick(s, 0, false, p) != StageResult::dialog || !s.dialog_pending ||
		s.boss_active || s.stage_frame != 0) return 3;
	if (stage1_tick(s, 0x20, false, p) != StageResult::dialog || s.stage_frame != 0) return 4;
	if (!stage1_dialog_complete(s) || !s.boss_active || s.dialog_pending ||
		stage1_dialog_complete(s)) return 5;
	s.gameover_pending = true;
	if (stage1_tick(s, 0, false, p) != StageResult::gameover) return 6;
	s.gameover_pending = false;
	s.stage_clear_requested = true;
	if (stage1_tick(s, 0, false, p) != StageResult::cleared) return 7;
	std::cout << "Stage admission and explicit modal boundaries passed\n";
}
