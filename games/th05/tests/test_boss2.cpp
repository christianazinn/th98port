#include "th05/core/boss.hpp"
#include "th05/core/state.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using namespace th05::core;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	std::uint16_t hp{};
	std::uint16_t hp_total{};
	std::vector<Point> pointnums{};

	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
	void boss_hp(std::int16_t value, std::int16_t total) noexcept override
	{
		hp = static_cast<std::uint16_t>(value);
		hp_total = static_cast<std::uint16_t>(total);
	}
	void pointnum(Point point, std::uint16_t, bool) noexcept override
	{
		pointnums.push_back(point);
	}
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.rank = 1U;
	state.playchar = 0U;
	state.resources.playperf = 16U;
	state.resources.playperf_max = 32U;
	state.player_pos.cur = {3072, 5000};
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

[[nodiscard]] bool midboss_setup_orbit_and_defeat() noexcept
{
	auto state = source_state();
	Trace trace{};
	stage2_fights_setup(state);
	if(
		state.midboss_active || state.boss_active ||
		state.midboss.frames_until != 2750U ||
		state.midboss.pos.cur != Point{3072, -512} ||
		state.midboss.pos.velocity != Point{0, 8} ||
		state.midboss.hp != 1400 || state.midboss.sprite != 202U ||
		state.boss.pos.cur != Point{3072, 1024} || state.boss.sprite != 180U ||
		state.boss.hitbox_radius != Point{384, 384}
	) return false;

	state.stage_frame = 2749U;
	stage2_midboss_activate(state);
	if(state.midboss_active) return false;
	state.stage_frame = 2750U;
	stage2_midboss_activate(state);
	if(!state.midboss_active || state.midboss.phase != 0U) return false;
	for(std::uint16_t frame{}; frame < 256U; ++frame) {
		if(!stage2_fights_update(state, trace)) return false;
	}
	if(
		state.midboss.phase != 1U || state.midboss.phase_frame != 0 ||
		state.midboss.stage2_center != Point{3072, 1536} ||
		state.boss.statebytes[14] != 0x20U || state.boss.statebytes[15] != 0U ||
		state.midboss.pos.velocity.x != 0 || trace.hp != 1400U || trace.hp_total != 1400U
	) return false;

	state.stage_frame = 16U;
	if(!stage2_fights_update(state, trace)) return false;
	if(
		bullet_count(state) != 5U || state.midboss.pos.prev != Point{3072, 1536} ||
		state.midboss.pos.cur != Point{3072, 1536} ||
		state.midboss.stage2_center.x != 3104 || state.midboss.pos.velocity.x != 16 ||
		state.midboss.angle != 254U
	) return false;

	state.midboss.phase = 1U;
	state.midboss.phase_frame = 99;
	state.midboss.hp = 400;
	state.stage_frame = 1U;
	if(!stage2_fights_update(state, trace)) return false;
	if(
		state.midboss.phase != 2U || state.midboss.phase_frame != 0 ||
		state.midboss.sprite != 206U || state.resources.bullet_clear_time != 20U ||
		state.score.score_delta != 5000U || trace.pointnums.size() != 5U ||
		trace.sounds.back() != 15U || state.midboss.pos.velocity == Point{}
	) return false;

	state.midboss.phase = 2U;
	state.midboss.phase_frame = 0;
	state.midboss.hp = 0;
	state.stage_frame = 1U;
	if(!stage2_fights_update(state, trace)) return false;
	if(
		state.midboss.phase != 254U || state.midboss.sprite != 4U ||
		!state.bullets.zap.active() || state.score.score_delta != 20000U ||
		trace.pointnums.size() != 20U || trace.sounds.back() != 12U
	) return false;

	state.midboss.phase = 2U;
	state.midboss.pos.cur.y = 0;
	state.midboss.damage_this_frame = 1U;
	stage2_fights_render_tail(state, trace);
	if(state.midboss.damage_this_frame != 0U) return false;
	state.midboss.phase = 254U;
	state.midboss.damage_this_frame = 1U;
	stage2_fights_render_tail(state, trace);
	return state.midboss.damage_this_frame == 1U;
}

[[nodiscard]] bool louise_entrance_and_patterns() noexcept
{
	{
		auto state = source_state();
		Trace trace{};
		stage2_fights_setup(state);
		stage2_boss_activate(state);
		for(std::uint16_t frame{}; frame < 192U; ++frame) {
			if(!stage2_fights_update(state, trace)) return false;
		}
		if(
			state.boss.phase != 2U || state.boss.phase_frame != 0 ||
			state.boss.mode != 1U || state.boss.phase_state != 0U ||
			state.boss.hp != 4400 || state.boss.phase_end_hp != 3000 ||
			state.boss.statebytes[10] != 24U || state.boss.statebytes[15] != 0U ||
			trace.hp != 4400U || trace.hp_total != 4400U
		) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		stage2_fights_setup(state);
		stage2_boss_activate(state);
		state.boss.phase = 2U;
		state.boss.mode = 1U;
		state.boss.phase_frame = 31;
		state.boss.hp = 4400;
		state.boss.phase_end_hp = 3000;
		state.boss.statebytes[10] = 24U;
		if(!stage2_fights_update(state, trace)) return false;
		if(
			state.bullets.bullet_template.group != static_cast<std::uint8_t>(BulletGroup::spread) ||
			state.bullets.bullet_template.special_motion != static_cast<std::uint8_t>(
				BulletSpecialMotion::exact_linear
			) ||
			state.boss.statebytes[15] != 1U || bullet_count(state) != 10U ||
			trace.sounds != std::vector<std::uint8_t>{15U}
		) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		stage2_fights_setup(state);
		stage2_boss_activate(state);
		state.boss.phase = 5U;
		state.boss.mode = 1U;
		state.boss.phase_frame = 31;
		state.boss.hp = 1900;
		state.boss.phase_end_hp = 500;
		if(!stage2_fights_update(state, trace)) return false;
		const auto &laser = state.lasers.pool[0];
		if(
			state.lasers.laser_template.coords.origin != Point{3072, 1024} ||
			state.lasers.laser_template.coords.width != 6U ||
			state.lasers.laser_template.color != 8U ||
			state.lasers.laser_template.active_at_age != 30 ||
			state.lasers.laser_template.shootout_speed != 104 ||
			laser.coords.angle != 144U || laser.coords.width != 6U || laser.color != 8U ||
			laser.active_at_age != 30 || trace.sounds != std::vector<std::uint8_t>{5U}
		) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		stage2_fights_setup(state);
		stage2_boss_activate(state);
		state.boss.phase = 5U;
		state.boss.mode = 2U;
		state.boss.phase_frame = 31;
		state.boss.hp = 1900;
		state.boss.phase_end_hp = 500;
		state.boss.statebytes[10] = 20U;
		if(!stage2_fights_update(state, trace)) return false;
		if(
			state.bullets.bullet_template.group != static_cast<std::uint8_t>(
				BulletGroup::ring_stack_aimed
			) || state.bullets.bullet_template.special_motion != static_cast<std::uint8_t>(
				BulletSpecialMotion::decelerate_then_turn
			) || state.bullets.special_turns_max != 1U ||
			state.bullets.bullet_template_special_angle != 0xE0U ||
			bullet_count(state) != 120U || trace.sounds != std::vector<std::uint8_t>{15U}
		) return false;
	}
	return true;
}

[[nodiscard]] bool louise_final_pattern_and_defeat() noexcept
{
	auto state = source_state();
	Trace trace{};
	stage2_fights_setup(state);
	stage2_boss_activate(state);
	state.boss.phase = 7U;
	state.boss.phase_frame = 31;
	state.boss.hp = 500;
	state.boss.phase_end_hp = 0;
	if(!stage2_fights_update(state, trace)) return false;
	if(
		state.bullets.bullet_template.group != static_cast<std::uint8_t>(BulletGroup::spread_aimed) ||
		state.bullets.bullet_template.angle != 0xE1U || bullet_count(state) != 32U ||
		trace.sounds != std::vector<std::uint8_t>{3U}
	) return false;

	state.boss.phase = 253U;
	state.boss.phase_frame = 31;
	state.boss.phase_state = 1U;
	state.random.cursor = 0U;
	if(!stage2_fights_update(state, trace)) return false;
	if(
		state.boss.phase != 254U || state.boss.phase_frame != 0 ||
		!state.boss_explosions.big.alive || !state.bullets.zap.active() ||
		state.score.score_delta != 10000U || trace.pointnums.size() != 10U ||
		state.player_invincibility_time != 255U
	) return false;
	for(std::uint8_t frame{}; frame < 64U; ++frame) {
		if(!stage2_fights_update(state, trace)) return false;
	}
	if(state.boss.phase != 255U || state.boss.phase_frame != 0 || !state.bombing_disabled) {
		return false;
	}
	if(!stage2_fights_update(state, trace)) return false;
	return state.dialog_pending && state.stage_clear_requested && state.palette_tone == 60U;
}

} // namespace

int main()
{
	if(!midboss_setup_orbit_and_defeat()) {
		std::cerr << "TH05 Stage 2 midboss test failure\n";
		return EXIT_FAILURE;
	}
	if(!louise_entrance_and_patterns()) {
		std::cerr << "TH05 Louise entrance/pattern test failure\n";
		return EXIT_FAILURE;
	}
	if(!louise_final_pattern_and_defeat()) {
		std::cerr << "TH05 Louise final-pattern/defeat test failure\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
