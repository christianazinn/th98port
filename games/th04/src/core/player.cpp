#include "th04/core/state.hpp"
#include "th04/core/shot_patterns.hpp"

namespace th04::core {
namespace {
std::int16_t subtract_word(std::int16_t a, std::int16_t b) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(static_cast<int>(a) - b));
}
bool fire(State &s) noexcept
{
	s.shot_spawn.player_cur = s.player_pos.cur;
	if (s.shot_level <= 1) {
		const auto pattern = s.playchar == 0 ? pat_shot_reimu : pat_shot_marisa;
		const auto result = s.shot_level == 0 ? shot_l0(s.shot_spawn, s.shots, pattern)
			: shot_l1(s.shot_spawn, s.shots, s.random, pattern);
		return !result.random_refused && (result.allocation.allocated() ||
			result.allocation.status == ShotAddStatus::no_slot);
	}
	if (s.shot_level > 9) return false;
	if (s.playchar == 0 && s.shottype == 1) {
		if (s.shot_level == 2)
			return shot_reimu_b_l2(s.reimu_a, s.shot_spawn, s.shots, s.random).complete();
		using Pattern = ShotPatternResult (*)(ReimuAPatternState&, ShotSpawnState&,
			ShotState&) noexcept;
		constexpr std::array<Pattern, 7> patterns{shot_reimu_b_l3, shot_reimu_b_l4,
			shot_reimu_b_l5, shot_reimu_b_l6, shot_reimu_b_l7, shot_reimu_b_l8,
			shot_reimu_b_l9};
		return patterns[s.shot_level - 3](s.reimu_a, s.shot_spawn, s.shots).complete();
	}
	if (s.playchar == 1 && s.shottype == 0) {
		using Pattern = ShotPatternResult (*)(ShotSpawnState&, ShotState&,
			RandomState&, Point) noexcept;
		constexpr std::array<Pattern, 8> patterns{shot_marisa_a_l2, shot_marisa_a_l3,
			shot_marisa_a_l4, shot_marisa_a_l5, shot_marisa_a_l6, shot_marisa_a_l7,
			shot_marisa_a_l8, shot_marisa_a_l9};
		return patterns[s.shot_level - 2](s.shot_spawn, s.shots, s.random, s.option_cur).complete();
	}
	if (s.playchar == 1) {
		switch (s.shot_level) {
		case 2: return shot_marisa_b_l2(s.shot_spawn, s.shots, s.random).complete();
		case 3: return shot_marisa_b_l3(s.shot_spawn, s.shots).complete();
		case 4: return shot_marisa_b_l4(s.shot_spawn, s.shots, s.random).complete();
		case 5: return shot_marisa_b_l5(s.shot_spawn, s.shots, s.random).complete();
		case 6: return shot_marisa_b_l6(s.shot_spawn, s.shots).complete();
		case 7: return shot_marisa_b_l7(s.shot_spawn, s.shots).complete();
		case 8: return shot_marisa_b_l8(s.shot_spawn, s.shots).complete();
		case 9: return shot_marisa_b_l9(s.shot_spawn, s.shots).complete();
		}
	}
	using Pattern = ReimuAPatternResult (*)(ReimuAPatternState&, ShotSpawnState&,
		ShotState&, RandomState&) noexcept;
	constexpr std::array<Pattern, 8> patterns{shot_reimu_a_l2, shot_reimu_a_l3,
		shot_reimu_a_l4, shot_reimu_a_l5, shot_reimu_a_l6, shot_reimu_a_l7,
		shot_reimu_a_l8, shot_reimu_a_l9};
	return patterns[s.shot_level - 2](s.reimu_a, s.shot_spawn, s.shots, s.random).status
		== ReimuAPatternStatus::complete;
}
} // namespace

void player_shot_level_update(State &s, Presentation &p) noexcept
{
	constexpr std::array<std::uint16_t, 9> thresholds{6, 12, 16, 24, 32, 48, 72, 96, 128};
	s.shot_level = 0;
	while (s.shot_level < thresholds.size() && s.power >= thresholds[s.shot_level])
		++s.shot_level;
	p.hud(HudField::power);
}

void player_bomb(State &s, Presentation &p) noexcept
{
	if (s.bombing || s.bombs == 0 || s.bombing_disabled) return;
	if (s.miss_time != 0) {
		if (s.miss_time <= 32) return;
		s.miss_time = 0;
		s.player_is_hit = false;
		s.miss_move_lock_time = 0;
	}
	--s.bombs;
	p.hud(HudField::bombs);
	s.bombing = true;
	s.bomb_frame = 0;
	s.player_invincibility_time = 255;
	s.bomb_background = true;
	s.resources.bullet_clear_time = 192;
	p.sound(13);
	s.items.items_pull_to_player = true;
	++s.bombs_used;
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
		if (s.items.dream_items_collected != 0) --s.items.dream_items_collected;
		if (s.items.dream_items_collected >= dream_score_per_items.size()) return false;
		s.items.dream_score = dream_score_per_items[s.items.dream_items_collected];
		p.hud(HudField::dream);
		player_shot_level_update(s, p);
		p.sound(2);
		if (s.resources.playperf >= 22) s.resources.playperf = 21;
		playperf_lower(s, 4);
		++s.miss_count;
	}
	s.miss_explosion_radius = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<int>(s.miss_explosion_radius) + 112));
	s.miss_explosion_angle = static_cast<std::uint8_t>(s.miss_explosion_angle + 8);
	if (s.miss_time >= 4) return true;
	if (s.resources.rem_lives > 1) {
		s.palette_tone = (s.miss_time & 1) ? 150 : 100;
		s.palette_changed = true;
	}
	if (s.miss_time != 0) return true;
	s.player_pos.cur = {3072, 5888};
	s.player_pos.prev = s.player_pos.cur;
	s.player_pos.velocity = {0, -32};
	if (s.resources.rem_lives > 1) {
		--s.resources.rem_lives;
		p.hud(HudField::lives);
		s.bombs = s.credit_bombs;
		p.hud(HudField::bombs);
		s.resources.bullet_clear_time = 32;
	} else {
		s.gameover_pending = true; // Caller must supply the actual continue decision.
	}
	return true;
}

bool player_update(State &s, std::uint16_t input, bool shift, Presentation &p) noexcept
{
	if (s.gameover_pending || s.playchar > 1 || s.shottype > 1) return false;
	if (s.player_invincibility_time != 0) --s.player_invincibility_time;
	if (s.player_is_hit) {
		if (s.player_invincibility_time == 0) {
			if (s.shots.shot_laser_time > 33) s.shots.shot_laser_time = 33;
			s.miss_time = 40;
			s.player_invincibility_time = 192;
			s.miss_move_lock_time = 72;
			s.player_pos.velocity = {};
		}
		s.player_is_hit = false;
	}
	if (s.miss_move_lock_time == 0) {
		s.player_pos.velocity = {};
		auto movement = static_cast<std::uint16_t>(input & 0x0F0F);
		bool first = true;
		while (player_move(s.player_pos.velocity, movement) == MoveResult::invalid) {
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
		bool shoot = false;
		if ((input & 0x20) != 0 && s.shot_spawn.shot_time <= 1) {
			s.shot_spawn.shot_time = 18;
			shoot = true;
		} else if (s.shot_spawn.shot_time != 0) {
			--s.shot_spawn.shot_time;
			shoot = s.shot_spawn.shot_time == 6 || s.shot_spawn.shot_time == 12;
		}
		if (shoot) {
			if (!fire(s)) return false;
			p.sound(1);
		}
	} else {
		pc98::th0405::update_motion(s.player_pos);
		--s.miss_move_lock_time;
	}
	s.option_prev = s.option_cur;
	s.option_cur = s.player_pos.cur;
	s.option_cur.x = subtract_word(s.option_cur.x, s.player_pos.velocity.x);
	s.option_cur.y = subtract_word(s.option_cur.y, s.player_pos.velocity.y);
	if ((input & 0x10) != 0) player_bomb(s, p);
	return s.miss_time == 0 || player_miss_update(s, p);
}
} // namespace th04::core
