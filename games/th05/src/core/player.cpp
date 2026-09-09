#include "th05/core/state.hpp"
#include "th05/core/shot_patterns.hpp"
#include "th05/core/bomb.hpp"

namespace th05::core {
bool gameplay_init(State &s, std::uint8_t rank, std::uint8_t playchar,
	std::uint8_t lives, std::uint8_t bombs, std::uint8_t stage_id,
	std::uint8_t demo_num, bool turbo) noexcept
{
	if (rank > 4 || playchar > 3 || stage_id > 6) return false;
	s.total_graze = 0;
	s.miss_count = s.bombs_used = 0;
	s.playchar = playchar;
	s.bombs = s.credit_bombs = bombs;
	s.resources.rem_lives = lives;
	s.score.score.fill(0);
	s.power = s.items.dream = 1;
	constexpr std::array<MovementSpeeds, 4> speeds{{{56,40}, {64,48}, {72,52}, {56,40}}};
	s.movement_speeds = speeds[playchar];
	s.stage_id = stage_id;
	s.rank = demo_num ? (demo_num < 5 ? 3 : 4) : (stage_id == 6 ? 4 : rank);
	s.resources.playperf = demo_num && demo_num < 5 ? 40 : 32;
	s.turbo_mode = demo_num != 0 || stage_id == 6 || turbo;
	score_highest_update_and_reset(s.score);
	constexpr std::array<std::uint16_t, 5> point{6000,10000,15000,20000,40000};
	constexpr std::array<std::uint16_t, 5> graze{25,50,100,200,500};
	constexpr std::array<std::uint8_t, 5> minimum{16,24,44,48,32};
	constexpr std::array<std::uint8_t, 5> maximum{32,40,54,58,36};
	s.items.item_point_score_at_full_dream = point[s.rank];
	s.graze_score = graze[s.rank];
	s.playperf_min = minimum[s.rank];
	s.resources.playperf_max = maximum[s.rank];
	if (s.rank == 2) s.resources.playperf = 44;
	if (s.rank == 3) s.resources.playperf = 48;
	return true;
}

void player_shot_level_update(State &s, Presentation &p) noexcept
{
	constexpr std::array<std::uint16_t, 9> thresholds{6, 12, 16, 24, 32, 48, 72, 96, 128};
	s.shot_level = 0;
	while (s.shot_level < thresholds.size() && s.power >= thresholds[s.shot_level])
		++s.shot_level;
	p.hud(HudField::power);
}

bool player_miss_update(State &s, Presentation &p) noexcept
{
	--s.miss_time;
	if (s.miss_time > 32) return true;
	if (s.miss_time == 32) {
		s.player_pos.velocity = {};
		s.power_overflow = 0;
		s.miss_explosion_radius = 0;
		if (!items_miss_add(s, p)) return false;
		const auto lost = s.power / 4;
		s.power = static_cast<std::uint8_t>(s.power - (lost > 16 ? 16 : lost));
		player_shot_level_update(s, p);
		p.sound(2);
		if (s.resources.playperf > 38) s.resources.playperf = 38;
		playperf_lower(s, 4);
		++s.miss_count;
	}
	if (s.items.dream > 2) {
		if (s.boss.phase == 0 || (s.stage_frame & 1)) s.items.dream -= 2;
	} else {
		s.items.dream = 1;
	}
	p.hud(HudField::dream);
	s.miss_explosion_radius = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<int>(s.miss_explosion_radius) + 112));
	s.miss_explosion_angle = static_cast<std::uint8_t>(s.miss_explosion_angle + 8);
	if (s.miss_time >= 4) return true;
	if (s.resources.rem_lives > 1) {
		s.palette_tone = (s.miss_time & 1) ? 150 : 100;
		s.palette_changed = true;
	}
	if (s.miss_time != 0) return true;
	s.player_pos.cur = s.player_pos.prev = {3072, 5888};
	s.player_pos.velocity = {0, -32};
	if (s.resources.rem_lives > 1) {
		--s.resources.rem_lives;
		p.hud(HudField::lives);
		s.bombs = s.credit_bombs;
		p.hud(HudField::bombs);
		s.resources.bullet_clear_time = 32;
	} else {
		// Stop at the actual Game Over/continue decision, before later frame work.
		s.gameover_pending = true;
	}
	return true;
}

bool player_update(State &s, std::uint16_t input, bool shift, Presentation &p) noexcept
{
	if (s.gameover_pending || s.playchar > 3) return false;
	// TH05's else matters: the last invincible frame still rejects a hit.
	if (s.player_invincibility_time != 0) {
		--s.player_invincibility_time;
		s.player_is_hit = false;
	} else if (s.player_is_hit) {
		s.miss_time = 40;
		s.player_is_hit = false;
		s.player_invincibility_time = 192;
		s.miss_move_lock_time = 72;
		s.player_pos.velocity = {};
	}
	if (s.miss_move_lock_time == 0) {
		s.player_pos.velocity = {};
		auto movement = static_cast<std::uint16_t>(input & 0x0F0F);
		bool first = true;
		while (player_move(s.player_pos.velocity, movement, s.movement_speeds) == MoveResult::invalid) {
			if (!first || s.previous_movement == movement) break;
			movement = static_cast<std::uint16_t>(movement & ~s.previous_movement);
			first = false;
		}
		if (shift) {
			s.player_pos.velocity.x = static_cast<std::int16_t>(s.player_pos.velocity.x / 2);
			s.player_pos.velocity.y = static_cast<std::int16_t>(s.player_pos.velocity.y / 2);
		}
		player_pos_update_and_clamp(s.player_pos);
		if (first) s.previous_movement = movement;
		if ((input & 0x20) && s.shot_spawn.shot_time == 0) s.shot_spawn.shot_time = 18;
		if (s.shot_spawn.shot_time > 18) s.shot_spawn.shot_time = 0;
		if (s.shot_spawn.shot_time != 0) {
			if (!player_shots_fire(s, p)) return false;
			--s.shot_spawn.shot_time;
		}
	} else {
		pc98::th0405::update_motion(s.player_pos);
		--s.miss_move_lock_time;
	}
	s.option_prev = s.option_cur;
	s.option_cur = s.player_pos.cur;
	s.option_cur.x = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<int>(s.option_cur.x) - s.player_pos.velocity.x));
	s.option_cur.y = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<int>(s.option_cur.y) - s.player_pos.velocity.y));
	if (input & 0x10) player_bomb(s, p);
	return s.miss_time == 0 || player_miss_update(s, p);
}
}
