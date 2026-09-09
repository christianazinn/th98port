#include "th05/core/boss.hpp"
#include "th05/core/random.hpp"
#include "th05/core/state.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using namespace th05::core;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	std::int16_t hp{};
	std::int16_t hp_total{};

	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
	void boss_hp(std::int16_t value, std::int16_t total) noexcept override
	{
		hp = value;
		hp_total = total;
	}
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	if(!gameplay_init(state, 1U, 0U, 3U, 3U, 2U)) std::abort();
	state.random.seed = 318U;
	randring_fill(state.random);
	state.player_pos.cur = {3072, 5000};
	state.player_pos.prev = state.player_pos.cur;
	state.shots.homing_target = {homing_target_none, homing_target_none};
	return state;
}

[[nodiscard]] std::size_t bullet_count(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &bullet : state.bullets.pool.pellets) {
		if(bullet.flag == bullet_alive) ++count;
	}
	for(const auto &bullet : state.bullets.pool.bullets16) {
		if(bullet.flag == bullet_alive) ++count;
	}
	return count;
}

[[nodiscard]] std::size_t shootout_laser_count(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &laser : state.lasers.pool) {
		if(laser.flag == LaserFlag::shootout) ++count;
	}
	return count;
}

[[nodiscard]] bool midboss_entrance_live_pattern_and_render_tail() noexcept
{
	auto state = source_state();
	Trace trace{};
	stage3_fights_setup(state);
	if(
		state.midboss_active || state.boss_active ||
		state.midboss.frames_until != 5750U ||
		state.midboss.pos.cur != Point{3072, -512} ||
		state.midboss.pos.velocity != Point{0, 8} ||
		state.midboss.hp != 1400 || state.midboss.sprite != 208U ||
		state.boss.pos.cur != Point{3072, 1024} || state.boss.sprite != 180U ||
		state.boss.hitbox_radius != Point{384, 384}
	) return false;

	state.stage_frame = 5749U;
	stage3_midboss_activate(state);
	if(state.midboss_active) return false;
	state.stage_frame = 5750U;
	stage3_midboss_activate(state);
	if(!state.midboss_active || state.midboss.phase != 0U) return false;
	for(std::uint16_t frame{}; frame < 192U; ++frame) {
		if(!stage3_fights_update(state, trace)) return false;
	}
	if(
		state.midboss.phase != 1U || state.midboss.phase_frame != 0U ||
		state.midboss.pos.cur != Point{3072, 1024} || state.midboss.pos.velocity.x != 0 ||
		state.boss.statebytes[13U] != 1U || state.boss.statebytes[14U] != 1U ||
		state.boss.statebytes[15U] != 0U || trace.hp != 1400 || trace.hp_total != 1400
	) return false;

	// The stage-matrix regression: the positive-HP phase-1 hittest is a
	// completed side-effect, not an unsupported operation.
	trace.sounds.clear();
	if(!stage3_fights_update(state, trace) || state.midboss.hp != 1400 ||
		state.midboss.phase != 1U || state.midboss.phase_frame != 1U ||
		trace.sounds != std::vector<std::uint8_t>{8U}) return false;

	trace.sounds.clear();
	state.midboss.phase_frame = 31U;
	if(!stage3_fights_update(state, trace)) return false;
	if(
		state.bullets.bullet_template.group != static_cast<std::uint8_t>(
			BulletGroup::spread_stack
		) || state.bullets.bullet_template.spread != 3U ||
		state.bullets.bullet_template.stack != 4U || bullet_count(state) != 12U ||
		trace.sounds != std::vector<std::uint8_t>{15U}
	) return false;

	state.midboss.hp = 0;
	if(!stage3_fights_update(state, trace)) return false;
	if(
		state.midboss.phase != 254U || !state.bullets.zap.active() ||
		state.midboss.sprite != 4U || trace.sounds.back() != 12U
	) return false;

	state.midboss.phase = 1U;
	state.midboss.pos.cur.y = 0;
	state.midboss.damage_this_frame = 1U;
	stage3_fights_render_tail(state, trace);
	if(state.midboss.damage_this_frame != 0U) return false;
	state.midboss.phase = 254U;
	state.midboss.damage_this_frame = 1U;
	stage3_fights_render_tail(state, trace);
	return state.midboss.damage_this_frame == 1U;
}

[[nodiscard]] bool alice_entrance_lasers_and_barrier_revenge() noexcept
{
	{
		auto state = source_state();
		Trace trace{};
		stage3_fights_setup(state);
		stage3_boss_activate(state);
		for(std::uint16_t frame{}; frame < 192U; ++frame) {
			if(!stage3_fights_update(state, trace)) return false;
		}
		if(
			state.boss.phase != 2U || state.boss.phase_frame != 0U ||
			state.boss.mode != 1U || state.boss.phase_state != 0U ||
			state.boss.hp != 9600 || state.boss.phase_end_hp != 7400 ||
			state.boss.statebytes[10U] != 24U || state.boss.statebytes[9U] != 0U ||
			trace.hp != 9600 || trace.hp_total != 9600
		) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		stage3_fights_setup(state);
		stage3_boss_activate(state);
		state.boss.phase = 3U;
		state.boss.mode = 1U;
		state.boss.phase_frame = 63U;
		state.boss.hp = 9600;
		state.boss.phase_end_hp = 7400;
		state.alice.boss_pattern = AlicePattern::lasers_and_random_balls;
		if(!stage3_fights_update(state, trace)) return false;
		if(
			shootout_laser_count(state) != 3U || bullet_count(state) == 0U ||
			state.lasers.laser_template.coords.origin != Point{3072, 896} ||
			state.lasers.laser_template.coords.width != 8U ||
			state.lasers.laser_template.color != 6U ||
			state.lasers.laser_template.active_at_age != 40 ||
			state.lasers.laser_template.shootout_speed != 80 ||
			trace.sounds != std::vector<std::uint8_t>{5U, 5U, 5U}
		) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		stage3_fights_setup(state);
		stage3_boss_activate(state);
		state.boss.phase = 5U;
		state.boss.phase_frame = 79U;
		state.boss.hp = 7400;
		state.boss.phase_end_hp = 5200;
		state.alice.barrier_frame = 79U;
		state.alice.barrier_fire_frames = 160U;
		for(auto &puppet : state.alice.puppets) {
			puppet.flag = 1U;
			puppet.hp = 500;
		}
		state.alice.puppets[0].pos.cur = state.alice.puppets[0].pos.prev = {2048, 2048};
		state.alice.puppets[1].pos.cur = state.alice.puppets[1].pos.prev = {4096, 2048};
		auto &shot = state.shots.shots[0];
		shot.flag = shot_alive;
		shot.damage = 1;
		shot.pos.cur = {3072, 2048};
		state.shots.shots_alive[0] = {{3072, 2048}, 0U};
		state.shots.shots_alive_count = 1U;
		if(!stage3_fights_update(state, trace)) return false;
		if(
			state.alice.barrier_frame != 80U || state.shots.shots_alive[0].pos.x !=
				homing_target_none || bullet_count(state) < 9U ||
			std::find(trace.sounds.begin(), trace.sounds.end(), 9U) == trace.sounds.end() ||
			std::find(trace.sounds.begin(), trace.sounds.end(), 3U) == trace.sounds.end()
		) return false;
	}
	return true;
}

} // namespace

int main()
{
	if(!midboss_entrance_live_pattern_and_render_tail()) {
		std::cerr << "TH05 Stage 3 midboss test failure\n";
		return EXIT_FAILURE;
	}
	if(!alice_entrance_lasers_and_barrier_revenge()) {
		std::cerr << "TH05 Alice test failure\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
