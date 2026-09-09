#include "th05/core/state.hpp"
#include <iostream>

using namespace th05::core;
State start()
{
	State s;
	s.player_pos.cur = s.option_cur = {3072, 5120};
	s.movement_speeds = {64, 48};
	s.resources = {24, 48, 2, 0};
	s.bombs = s.credit_bombs = 3;
	s.power = 1;
	s.items.dream = 100;
	s.random.seed = 18;
	pc98::th0405::randring_fill(s.random);
	s.shots.homing_target.y = homing_target_none;
	return s;
}
bool cadence_and_movement()
{
	Presentation p;
	for (unsigned initial = 0; initial < 256; ++initial) {
		for (const bool held : {false, true}) {
			auto s = start();
			s.shot_spawn.shot_time = static_cast<std::uint8_t>(initial);
			if (!player_update(s, static_cast<std::uint16_t>(held ? 0x24 : 4), true, p)) return false;
			const auto admitted = held && initial == 0 ? 18U : initial;
			const auto expected = admitted > 18 || admitted == 0 ? 0U : admitted - 1;
			if (s.shot_spawn.shot_time != expected || s.player_pos.cur.x != 3040 ||
				s.option_cur.x != 3072) return false;
		}
	}
	return true;
}
bool miss_and_deathbomb()
{
	Presentation p;
	auto s = start();
	s.player_is_hit = true;
	s.player_invincibility_time = 1;
	if (!player_update(s, 0, false, p) || s.miss_time != 0 || s.player_is_hit) return false;
	s.player_is_hit = true;
	if (!player_update(s, 0x10, false, p) || s.miss_time || !s.bombing || s.bombs != 2 ||
		s.miss_move_lock_time || s.player_invincibility_time != 192) return false;
	s = start();
	s.power = 80;
	s.resources.playperf = 48;
	s.player_is_hit = true;
	for (unsigned i = 0; i < 40; ++i) {
		s.stage_frame = static_cast<std::uint16_t>(i);
		if (!player_update(s, 0, false, p)) return false;
	}
	return s.miss_count == 1 && s.power == 64 && s.resources.playperf == 34 &&
		s.resources.rem_lives == 1 && s.items.items_spawned == 5 && s.items.dream == 34 &&
		s.player_pos.cur == Point{3072, 5888} && s.player_pos.velocity == Point{0, -32} &&
		s.resources.bullet_clear_time == 32;
}
bool dream_and_gameover()
{
	Presentation p;
	for (const std::uint8_t phase : {std::uint8_t{0}, std::uint8_t{1}}) {
		for (std::uint16_t frame = 0; frame < 2; ++frame) {
			auto s = start();
			s.boss.phase = phase;
			s.stage_frame = frame;
			s.miss_time = 20;
			if (!player_miss_update(s, p) || s.items.dream != (phase == 0 || frame == 1 ? 98 : 100))
				return false;
		}
	}
	auto s = start();
	s.resources.rem_lives = 1;
	s.miss_time = 1;
	if (!player_miss_update(s, p) || !s.gameover_pending) return false;
	return !player_update(s, 0, false, p);
}
bool score_transfer()
{
	Presentation p;
	auto s = start();
	s.score.score_delta = 12345;
	unsigned ticks = 0;
	while (s.score.score_delta && ticks++ < 1000) score_update(s, p);
	if (s.score.score_delta || s.score.score != ScoreDigits{0,5,4,3,2,1,0,0} ||
		s.resources.rem_lives != 2 || s.items.extends_gained != 0) return false;
	s.score.score = {0,9,9,9,9,9,9,9};
	s.score.score_delta = 1;
	s.score.score_delta_frame = 0;
	score_update(s, p);
	return s.score.score == ScoreDigits{0,0,0,0,0,0,0,10};
}
int main()
{
	State s;
	if (!gameplay_init(s, 0, 2, 3, 2, 0, 1) || s.rank != 3 ||
		s.resources.playperf != 48 || s.movement_speeds.aligned != 72 ||
		s.items.item_point_score_at_full_dream != 20000 || !s.turbo_mode) return 4;
	if (!gameplay_init(s, 0, 3, 3, 2, 6) || s.rank != 4 || s.graze_score != 500 ||
		s.items.dream != 1 || s.movement_speeds.diagonal != 40) return 5;
	if (!cadence_and_movement()) { std::cerr << "cadence/movement\n"; return 1; }
	if (!miss_and_deathbomb()) { std::cerr << "miss/deathbomb\n"; return 2; }
	if (!dream_and_gameover()) { std::cerr << "dream/gameover\n"; return 3; }
	if (!score_transfer()) { std::cerr << "score transfer\n"; return 6; }
	return 0;
}
