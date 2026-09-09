#include "th04/core/state.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

using namespace th04::core;
struct Trace : Presentation {
	std::vector<unsigned> events;
	unsigned stars{};
	void sound(std::uint8_t id) noexcept override { events.push_back(id); }
	void hud(HudField field) noexcept override { events.push_back(100 + static_cast<unsigned>(field)); }
	void bomb_star(Point, std::uint8_t) noexcept override { ++stars; }
};
State start()
{
	State s;
	s.player_pos.cur = {3072, 5120};
	s.option_cur = s.player_pos.cur;
	s.resources = {12, 32, 2, 0};
	s.credit_bombs = 3;
	s.bombs = 3;
	s.power = 1;
	s.random.seed = 18;
	pc98::th0405::randring_fill(s.random);
	s.random2.seed = 91;
	pc98::th0405::randring_fill(s.random2);
	s.reimu_a.homing_target.y = homing_target_none;
	return s;
}
bool movement_and_cadence()
{
	auto s = start();
	Trace p;
	if (!player_update(s, 0x24, true, p)) return false;
	if (s.player_pos.cur.x != 3040 || s.option_cur.x != 3072 ||
		s.shot_spawn.shot_time != 18 || p.events != std::vector<unsigned>{1}) return false;
	for (int i = 0; i < 12; ++i) {
		if (!player_update(s, 0, false, p)) return false;
		shots_update(s.shots, s.option_cur);
	}
	return p.events == std::vector<unsigned>({1, 1, 1}) && s.shot_spawn.shot_time == 6;
}
bool deathbomb_and_miss()
{
	auto s = start();
	Trace p;
	s.player_is_hit = true;
	if (!player_update(s, 0x30, false, p)) return false;
	if (s.miss_time != 0 || s.miss_move_lock_time != 0 || !s.bombing ||
		s.bombs != 2 || s.player_invincibility_time != 255 || s.shot_spawn.shot_time != 0)
		return false;
	if (p.events != std::vector<unsigned>({102, 13})) return false;
	s = start();
	s.power = 80;
	s.resources.playperf = 30;
	s.player_is_hit = true;
	for (int i = 0; i < 40; ++i)
		if (!player_update(s, 0, false, p)) return false;
	return s.miss_time == 0 && s.miss_count == 1 && s.power == 64 &&
		s.resources.playperf == 17 && s.resources.rem_lives == 1 &&
		s.player_pos.cur == Point{3072, 5888} && s.player_pos.velocity == Point{0, -32} &&
		s.resources.bullet_clear_time == 32 && s.items.items_spawned == 5;
}
bool cadence_all_byte_states()
{
	for (unsigned initial = 0; initial < 256; ++initial) {
		for (const bool held : {false, true}) {
			auto s = start();
			Trace p;
			s.shot_spawn.shot_time = static_cast<std::uint8_t>(initial);
			if (!player_update(s, held ? 0x20 : 0, false, p)) return false;
			const bool immediate = held && initial <= 1;
			const auto expected = immediate ? 18U : (initial == 0 ? 0 : initial - 1);
			const bool fired = immediate || expected == 6 || expected == 12;
			if (s.shot_spawn.shot_time != expected || p.events.size() != (fired ? 1U : 0U) ||
				(s.shots.shots[0].flag == shot_alive) != fired) return false;
		}
	}
	return true;
}
bool invincibility_edge_and_last_life()
{
	auto s = start();
	Trace p;
	s.player_invincibility_time = 1;
	s.player_is_hit = true;
	s.resources.rem_lives = 1;
	if (!player_update(s, 0, false, p) || s.miss_time != 39) return false;
	for (int i = 0; i < 39; ++i)
		if (!player_update(s, 0, false, p)) return false;
	return s.gameover_pending && !player_update(s, 0, false, p);
}
bool bomb_lifetime()
{
	for (std::uint8_t character = 0; character < 2; ++character) {
		auto s = start();
		Trace p;
		s.playchar = character;
		s.palette[14] = {1, 2, 3};
		player_bomb(s, p);
		for (unsigned i = 0; i < 227; ++i) {
			s.stage_frame = static_cast<std::uint16_t>(i);
			if (!bomb_update(s, p)) return false;
		}
		if (s.bombing || s.bomb_frame != 227 || !s.scroll_active ||
			s.items.items_pull_to_player || s.palette_tone != 100 ||
			s.palette[14].r != 1 || s.palette[14].g != 2 || s.palette[14].b != 3 ||
			p.stars != 48 * 128 || s.circles_color != 13) return false;
	}
	return true;
}
bool all_shot_types_and_power_levels()
{
	for (std::uint8_t character = 0; character < 2; ++character) {
	for (std::uint8_t type = 0; type < 2; ++type) {
	for (const auto power : {0, 6, 12, 16, 24, 32, 48, 72, 96, 128}) {
		auto s = start();
		Trace p;
		s.playchar = character;
		s.shottype = type;
		s.power = static_cast<std::uint8_t>(power);
		player_shot_level_update(s, p);
		for (int i = 0; i < 54; ++i) {
			if (!player_update(s, 0x20, false, p)) return false;
			shots_update(s.shots, s.option_cur);
		}
		if (std::none_of(s.shots.shots.begin(), s.shots.shots.end(),
			[](const Shot &shot) { return shot.flag == shot_alive; })) return false;
	}
	}
	}
	return true;
}
bool pickup_changes_next_shot()
{
	auto s = start();
	Trace p;
	s.power = 5;
	player_shot_level_update(s, p);
	items_add(s, p, s.player_pos.cur, ItemType::power);
	if (!items_update(s, p) || s.power != 6 || s.shot_level != 1 ||
		s.score.score_delta != 1) return false;
	const auto before = s.random.cursor;
	if (!player_update(s, 0x20, false, p)) return false;
	return s.random.cursor == static_cast<std::uint8_t>(before + 1) &&
		s.shots.shots[0].flag == shot_alive;
}
int main()
{
	const bool checks[]{movement_and_cadence(), deathbomb_and_miss(),
		invincibility_edge_and_last_life(), bomb_lifetime(), all_shot_types_and_power_levels(),
		cadence_all_byte_states(), pickup_changes_next_shot()};
	for (std::size_t i = 0; i < std::size(checks); ++i) {
		if (!checks[i]) { std::cerr << "gameplay check failed: " << i << '\n'; return 1; }
	}
}
