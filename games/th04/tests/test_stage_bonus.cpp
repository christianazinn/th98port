#include "th04/core/state.hpp"
#include "th04/core/stage.hpp"
#include "th04/core/stage_bonus.hpp"
#include <cstdlib>
int main()
{
	using namespace th04::core;
	State s;
	Presentation p;
	if (!gameplay_init(s, 1, 0, 0, 3, 3, false)) return 1;
	s.stage_id = 1;
	s.boss.phase_state = 1;
	s.power = 100;
	s.items.dream_score = 100;
	s.bullets.stage_graze = 20;
	s.items.stage_point_items_collected = 2;
	const auto initial = s;
	if (stage_clear_bonus(s, p) != 1800 || s.score.score_delta != 1800 ||
		s.bombs != 4 || s.resources.playperf != 18 || s.palette_tone != 60) return 2;
	s = initial;
	s.credit_lives = 4;
	s.score.score[0] = 1;
	s.rank = 3;
	if (stage_clear_bonus(s, p) != 1411) return 3;
	s = initial;
	s.boss.phase_state = 0;
	if (stage_clear_bonus(s, p) != 0 || s.bombs != 4) return 4;
	s = initial;
	if (stage_allclear_bonus(s, p) != 7400 || s.score.extends_gained != 10) return 5;
	s = initial;
	s.rank = 4;
	if (stage_allclear_bonus(s, p) != 15400) return 6;
	s = initial;
	s.items.stage_point_items_collected = 0;
	if (stage_clear_bonus(s, p) != 0) return 7;
	return EXIT_SUCCESS;
}
