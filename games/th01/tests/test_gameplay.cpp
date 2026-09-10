#include "th01/core/gameplay.hpp"

#include <cstdlib>
#include <iostream>

namespace {
int failures{};
void check(bool condition, const char *message)
{
	if(!condition) { std::cerr << message << '\n'; ++failures; }
}
th01::core::State card_stage()
{
	th01::core::State s{};
	s.stage.scene[0].cards[0][0] = 1;
	check(th01::core::gameplay_begin_stage(s, false) == th01::core::StageSetupResult::completed,
		"One-card source layout initializes");
	return s;
}
}

int main()
{
	using namespace th01::core;
	auto s = card_stage();
	check(s.stage.timer == 1000 && s.orb.horizontal.left == 600 && s.orb.top == 280,
		"Stage entry initializes timer and Orb");
	check(gameplay_tick(s, {}) == TickResult::running && s.frame_rand == 1 &&
		s.orb.horizontal.left == 596 && s.orb.top == 272,
		"Connected first tick advances source clocks before Orb acceleration");
	for(int i = 0; i < 3; ++i) (void)gameplay_tick(s, {});
	check(s.stage.timer == 998 && s.frame_since_start_of_binary == 4,
		"Timer decrements only on every fourth gameplay tick");

	s = card_stage();
	s.score = 400000;
	check(gameplay_tick(s, {.group0 = 1}) == TickResult::paused,
		"Escape suspends after Orb update");
	check(s.frame_rand == 1 && s.rem_lives == 4,
		"Pause retains pending extend suffix");
	check(gameplay_tick(s, {}) == TickResult::paused && s.frame_rand == 1,
		"Modal polling cannot advance gameplay clocks");
	check(gameplay_resume(s) == TickResult::running && s.rem_lives == 5 &&
		s.extend_next == 2 && s.projectiles.pellet_speed_x40 == 1,
		"Resume completes the deferred integer-overload extend once");
	check(gameplay_resume(s) == TickResult::unsupported_numeric_state && s.rem_lives == 5,
		"Repeated resume cannot award another extend");

	s = card_stage();
	s.input.player_is_hit = true;
	s.player.left = 303;
	s.frame_rand = 987;
	s.shots.slots[0].decay_frame = 3;
	check(gameplay_restart_life(s) && s.rem_lives == 3 && s.rem_bombs == 2 &&
		s.player.left == 323 && s.player.invincibility_time == 150 &&
		s.shots.slots[0].decay_frame == 0 && s.random_seed == 987,
		"Miss preserves truncated return movement, clears shot decay and reseeds at life entry");
	check(s.stage.timer == 1200 && s.projectiles.pellet_speed_x40 == -2,
		"Miss adds timer and lowers pellet speed");

	s = card_stage();
	s.stage.timer = 100;
	s.cardcombo_max = 2;
	s.rem_lives = 4;
	s.rem_bombs = 1;
	check(gameplay_stage_bonus(s, false) == 14000 && s.score == 14000,
		"Card bonus combines four source metrics");
	s = card_stage();
	s.stage_id = 4;
	s.stage.timer = 100;
	s.cardcombo_max = 2;
	check(gameplay_stage_bonus(s, true) == 81000,
		"Boss TOTLE uses its separate metric factors");

	s = card_stage();
	check(gameplay_begin_stage(s, true) == StageSetupResult::completed && s.waiting_for_shot,
		"First card stage exposes its Shot gate");
	check(!gameplay_poll_start(s, {.group7 = 0x10}) && s.player.left == 308 && s.frame_rand == 0,
		"Initial Shot gate allows movement without advancing random clock");
	check(gameplay_poll_start(s, {.group5 = 2}) && !s.waiting_for_shot && !s.input.shot,
		"Initial Shot starts play and clears consumed latch");
	s.stage_cleared = s.input.player_is_hit = s.test_damage = true;
	s.stage_id = 1;
	s.stage.scene[1].cards[0][0] = 1;
	check(gameplay_begin_stage(s, false) == StageSetupResult::completed &&
		!s.stage_cleared && !s.input.player_is_hit && !s.test_damage &&
		gameplay_tick(s, {}) == TickResult::running,
		"Next card-stage entry clears the completed life/stage flags on the same State");

	s = {};
	s.stage_id = 4;
	check(gameplay_begin_stage(s, true) == StageSetupResult::completed,
		"Boss entry does not require card-stage objects");
	s.boss.phase = 8;
	const auto left_before_choice = s.orb.horizontal.left;
	check(gameplay_tick(s, {}) == TickResult::route_selection && s.frame_rand == 1 &&
		s.orb.horizontal.left == left_before_choice,
		"Route modal suspends between boss and Orb, not after the whole frame");
	check(gameplay_tick(s, {}) == TickResult::route_selection && s.frame_rand == 1,
		"Route polling does not repeat boss prefix");
	check(gameplay_choose_route(s, BossRoute::jigoku) == TickResult::stage_cleared &&
		s.route == 1 && s.frame_rand == 1 && s.orb.horizontal.left == left_before_choice - 4,
		"Explicit route choice resumes the same frame's Orb and score suffix");
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
