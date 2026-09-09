#include "th05/core/state.hpp"
#include "th05/core/stage_bonus.hpp"
#include <cstdlib>
int main()
{
	using namespace th05::core;
	State s;
	Presentation p;
	if (!gameplay_init(s, 1, 0, 3, 3, 1)) return 1;
	s.boss.phase_state = 1;
	s.items.dream = 10;
	s.bullets.stage_graze = 20;
	s.items.stage_point_items_collected = 2;
	const auto initial = s;
	if (stage_clear_bonus(s, p) != 30800 || s.score.score_delta != 30800 ||
		s.bombs != 3 || s.resources.playperf != 30 || s.palette_tone != 60) return 2;
	s = initial;
	s.miss_count = 1;
	if (stage_clear_bonus(s, p) != 800 || s.bonus_previous_miss_count != 1) return 3;
	// The next tally sees unchanged counters and awards both bonuses again.
	if (stage_clear_bonus(s, p) != 30800) return 4;
	s = initial;
	s.bombs_used = 1;
	if (stage_clear_bonus(s, p) != 15800 || s.bonus_previous_bombs_used != 1) return 5;
	s = initial;
	s.credit_lives = 4;
	s.score.score[0] = 1;
	s.rank = 3;
	if (stage_clear_bonus(s, p) != 24147) return 6;
	s = initial;
	s.items.extend_point_items_collected = 10;
	if (stage_allclear_bonus(s, p) != 108900 || s.items.extends_gained != 10) return 7;
	s = initial;
	s.boss.phase_state = 0;
	if (stage_clear_bonus(s, p) != 0 || s.resources.playperf != 30) return 8;
	return EXIT_SUCCESS;
}
