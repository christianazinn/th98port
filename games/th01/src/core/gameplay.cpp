#include "th01/core/gameplay.hpp"
#include "pc98/random.hpp"

#include <algorithm>
#include <bit>

namespace th01::core {
namespace {
TickResult after_boss(State &) noexcept;
std::int16_t word(int value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

TickResult suffix(State &s) noexcept
{
	score_extend_update(s);
	// Final-boss exit precedes the pending pellet-score addition in main_01.
	if(s.game_cleared) return TickResult::game_cleared;
	if(s.projectiles.pellet_destroy_score_delta) pellet_destroy_score_delta_commit(s);
	if(s.stage_cleared) return TickResult::stage_cleared;
	if(s.input.player_is_hit) return TickResult::life_lost;
	return TickResult::running;
}
} // namespace

void score_extend_update(State &s) noexcept
{
	const auto threshold = static_cast<std::uint32_t>(s.extend_next) * 400000U;
	if(s.score < threshold) return;
	if(s.rem_lives < 6) {
		++s.rem_lives;
		// extend.cpp calls the integer overload, not the float overload.
		pellet_speed_raise(s, 1);
	}
	s.extend_next = word(s.extend_next + 1);
}

TickResult gameplay_tick(State &s, const RawInput &raw) noexcept
{
	if(s.pause_suffix_pending) return TickResult::paused;
	if(s.route_selection_pending) return TickResult::route_selection;
	if(s.waiting_for_shot) return TickResult::waiting_for_shot;
	if(s.game_cleared) return TickResult::game_cleared;
	if(s.stage_cleared) return TickResult::stage_cleared;
	if(s.input.player_is_hit) return TickResult::life_lost;
	++s.frame_rand;
	const int raise_cycle = 1800 - s.rem_lives * 200 - s.rem_bombs * 50;
	if(raise_cycle == 0) return TickResult::unsupported_numeric_state;
	if(s.frame_rand % static_cast<std::uint32_t>(raise_cycle) == 0) pellet_speed_raise(s, 1);
	sense_input(s.input, raw);
	player_invincibility_update(s);
	player_update(s);
	stage_items_update(s);
	++s.frame_since_start_of_binary;
	s.orb.vertical.force_frame = word(s.orb.vertical.force_frame + 1);
	++s.bomb_frame;
	s.input.bomb_doubletap_frame = word(s.input.bomb_doubletap_frame + 1);
	if((s.frame_rand & 3U) == 0) stage_timer_tick(s);
	player_invincibility_particles_update(s);
	const auto boss_result = s.boss.id == BossId::none ? BossUpdateResult::running : boss_update(s);
	if(boss_result == BossUpdateResult::unsupported) return TickResult::unsupported_boss;
	if(boss_result == BossUpdateResult::route_selection) {
		s.route_selection_pending = true;
		return TickResult::route_selection;
	}
	return after_boss(s);
}

namespace {
TickResult after_boss(State &s) noexcept
{
	shootout_lasers_update(s);
	auto &orb = s.orb;
	if(!orb.in_portal) {
		move_orb_x(orb.horizontal, orb.horizontal.velocity);
		const auto delta = update_orb_velocity_y(orb.vertical);
		if(!delta) return TickResult::unsupported_numeric_state;
		orb.top = word(orb.top + *delta);
		orb.frames_outside_portal = word(orb.frames_outside_portal + 1);
		orb.rotation_frame = rotate_orb(orb.rotation_frame, orb.horizontal.velocity);
		if(orb.top > 368) {
			if(!apply_orb_force(orb.vertical, orb.horizontal.velocity, s.random_seed,
				0.78, OrbForce::surface)) return TickResult::unsupported_numeric_state;
			orb.top = 368;
			s.cardcombo_cur = 0;
		}
		if(orb.top < 64) {
			if(!apply_orb_force(orb.vertical, orb.horizontal.velocity, s.random_seed,
				0.0, OrbForce::top)) return TickResult::unsupported_numeric_state;
			orb.top = 64;
		}
	}
	if(pellets_update_and_shots(s) != ShotOrbUpdateStatus::complete)
		return TickResult::unsupported_numeric_state;
	if(s.stage_id % 5 != 4 && !stage_cards_hittest_and_obstacles_update(s))
		return TickResult::unsupported_numeric_state;
	if(!orb.in_portal) orb_player_hittest(s, 0);
	if(s.stage_id % 5 != 4) stage_cards_update(s);
	orb.prev_left = orb.horizontal.left;
	orb.prev_top = orb.top;
	if(s.input.paused) {
		s.pause_suffix_pending = true;
		return TickResult::paused;
	}
	return suffix(s);
}
} // namespace

TickResult gameplay_choose_route(State &s, BossRoute route) noexcept
{
	if(!s.route_selection_pending || (route != BossRoute::makai && route != BossRoute::jigoku))
		return TickResult::unsupported_numeric_state;
	if(singyoku_route_select(s, route) != BossUpdateResult::stage_cleared)
		return TickResult::unsupported_boss;
	s.route_selection_pending = false;
	return after_boss(s);
}

TickResult gameplay_resume(State &s) noexcept
{
	if(!s.pause_suffix_pending) return TickResult::unsupported_numeric_state;
	s.pause_suffix_pending = false;
	s.input.paused = false;
	return suffix(s);
}

bool gameplay_restart_life(State &s) noexcept
{
	if(!s.input.player_is_hit || s.stage_cleared || s.game_cleared || s.rem_lives <= 0)
		return false;
	--s.rem_lives;
	s.test_damage = false;
	s.bomb_frame = 200;
	// The miss image is visual, but selecting it consumes the shared IRAND.
	(void)pc98::irand(s.random_seed);
	for(auto &shot : s.shots.slots) {
		if(shot.moving) shot.moving = false;
		else if(shot.decay_frame) shot.decay_frame = 0;
	}
	pellets_decay(s);
	player_reset(s);
	const int right_target = std::min(static_cast<int>(s.player.left) + 160, 608);
	for(int frame = 0; frame < 30; ++frame) {
		s.player.left = word(s.player.left + (frame < 10 ? 16 : (304 - right_target) / 20));
		if(s.player.left >= 608) s.player.left = 608;
	}
	s.rem_bombs = static_cast<std::int8_t>(std::min(static_cast<int>(s.rem_bombs) + 1, 5));
	stage_timer_extend(s);
	pellet_speed_lower(s, 0, -2);
	s.input.player_is_hit = false;
	s.player.invincibility_time = 150;
	player_reset(s);
	s.input.bomb_doubletap_frame = 60;
	(void)stage_cards_hittest_and_obstacles_update(s, true);
	reset_input(s.input);
	s.player.invincible = true;
	if(s.boss.id == BossId::singyoku || s.boss.id == BossId::mima)
		(void)boss_update(s);
	if(s.boss.id == BossId::kikuri) s.input.strike = false;
	s.input.shot = false;
	s.random_seed = s.frame_rand;
	s.input.bomb_doubletap_frame = 20;
	return true;
}

StageSetupResult gameplay_begin_stage(State &s, bool first_stage_in_scene) noexcept
{
	if(s.stage_id < 0 || s.stage_id >= 20 || s.route < 0 || s.route > 1)
		return StageSetupResult::invalid_stage_id;
	// The preceding source stage boundary clears these before the next stage.
	s.stage_cleared = false;
	s.input.player_is_hit = false;
	s.test_damage = false;
	pellets_unput_and_reset_nonclouds(s);
	for(auto &shot : s.shots.slots) {
		if(shot.moving) shot.moving = false;
		else if(shot.decay_frame) shot.decay_frame = 0;
	}
	s.boss.id = BossId::none;
	s.player.invincible = false;
	s.player.invincibility_time = 0;
	switch(s.stage_id) {
	case 4: s.boss.id = BossId::singyoku; break;
	case 9: s.boss.id = s.route == 0 ? BossId::yuugenmagan : BossId::mima; break;
	case 14: s.boss.id = s.route == 0 ? BossId::elis : BossId::kikuri; break;
	case 19: s.boss.id = s.route == 0 ? BossId::sariel : BossId::konngara; break;
	default: break;
	}
	if(s.boss.id != BossId::none) {
		stage_items_reset(s);
		boss_setup(s);
	}
	const auto setup = stage_setup(s, first_stage_in_scene);
	if(setup != StageSetupResult::completed) return setup;
	if(!stage_timer_init(s)) return StageSetupResult::invalid_stage_id;
	s.cardcombo_max = 0;
	s.orb.in_portal = false;
	s.bomb_frame = 0;
	s.projectiles.pellet_unknown_seven = 7;
	s.frame_since_start_of_binary = 0;
	s.orb.horizontal.left = s.orb.prev_left = 600;
	s.orb.top = s.orb.prev_top = 280;
	if(s.stage_id % 5 == 0 || s.boss.id != BossId::none) s.player.left = 304;
	s.orb.vertical.force = -8.0;
	s.orb.vertical.force_frame = 0;
	s.orb.horizontal.velocity = OrbVelocityX::left4;
	s.player.deflecting = s.bomb_damaging = false;
	s.cardcombo_cur = 0;
	player_reset(s);
	s.input.direction = Direction::none;
	s.input.shot = s.input.ok = s.input.paused = false;
	s.input.bomb_doubletap_frame = 60;
	(void)stage_cards_hittest_and_obstacles_update(s, true);
	reset_input(s.input);
	if(s.boss.id == BossId::singyoku || s.boss.id == BossId::mima)
		(void)boss_update(s);
	if(s.boss.id == BossId::kikuri) s.input.strike = false;
	s.waiting_for_shot = first_stage_in_scene && s.boss.id == BossId::none;
	if(!s.waiting_for_shot) {
		s.input.shot = false;
		s.random_seed = s.frame_rand;
		s.input.bomb_doubletap_frame = 20;
	}
	return StageSetupResult::completed;
}

bool gameplay_poll_start(State &s, const RawInput &raw) noexcept
{
	if(!s.waiting_for_shot) return true;
	sense_input(s.input, raw);
	player_update(s);
	++s.bomb_frame;
	if(!s.input.shot) return false;
	s.waiting_for_shot = false;
	s.input.shot = false;
	s.random_seed = s.frame_rand;
	s.input.bomb_doubletap_frame = 20;
	return true;
}

std::uint32_t gameplay_stage_bonus(State &s, bool boss_stage) noexcept
{
	std::uint32_t bonus{};
	const auto add = [&bonus](std::int32_t value, std::int32_t maximum) {
		bonus += static_cast<std::uint32_t>(std::min(value, maximum)) * 10U;
	};
	if(boss_stage) {
		add(static_cast<std::uint16_t>(s.stage.timer * 5U), 6553);
		add(word(s.cardcombo_max * 200), 6553);
		add(static_cast<std::int32_t>(word(s.rem_lives * 500)) + s.rem_bombs * 200, 6553);
		add(word((s.stage_id + 1) * 1000), 65530);
	} else {
		add(word(s.stage.timer * 3), 6553);
		add(word(s.cardcombo_max * 100), 6553);
		add(word(s.rem_lives * 200 + s.rem_bombs * 100), 6553);
		add(word(s.stage_id * 200), 6553);
	}
	s.score += bonus;
	return bonus;
}
} // namespace th01::core
