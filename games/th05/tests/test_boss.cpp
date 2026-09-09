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

[[nodiscard]] bool setup_and_midboss() noexcept
{
	auto state = source_state();
	Trace trace{};
	state.midboss_active = true;
	state.midboss.phase = 3U;
	state.midboss.phase_frame = 99;
	stage1_fights_setup(state);
	if(
		state.midboss_active || state.boss_active ||
		state.midboss.frames_until != 2500U ||
		state.midboss.pos.cur != Point{3072, 1536} ||
		state.midboss.hp != 1000 || state.midboss.sprite != 200U ||
		state.boss.phase != 0U || state.boss.pos.cur != Point{3072, 1024} ||
		state.boss.sprite != 180U || state.boss.hitbox_radius != Point{384, 384} ||
		state.boss.sprite_left != 186U || state.boss.sprite_right != 184U ||
		state.boss.sprite_stay != 180U
	) return false;

	state.stage_frame = 2499U;
	stage1_midboss_activate(state);
	if(state.midboss_active) return false;
	state.stage_frame = 2500U;
	stage1_midboss_activate(state);
	if(!state.midboss_active || state.midboss.phase != 0U) return false;
	for(std::uint16_t frame = 0U; frame < 256U; ++frame) {
		if(!stage1_fights_update(state, trace)) return false;
	}
	if(
		state.midboss.phase != 1U || state.midboss.phase_frame != 0 ||
		state.midboss.sprite != 204U || trace.hp != 1000U || trace.hp_total != 1000U ||
		state.shots.homing_target != state.midboss.pos.cur
	) return false;

	state.midboss.phase = 3U;
	state.midboss.phase_frame = 15;
	state.midboss.hp = 500;
	state.boss.angle = 0U;
	if(!stage1_fights_update(state, trace)) return false;
	// The firing step advances the shared boss angle before m1_move() samples
	// it, so the half-health orbit is at 0x14 rather than the seeded zero.
	return bullet_count(state) == 5U && state.midboss.pos.prev == Point{3072, 1536} &&
		state.midboss.pos.cur == Point{3524, 1778} && state.boss.angle == 18U;
}

[[nodiscard]] bool sara_entrance_and_patterns() noexcept
{
	{
		auto state = source_state();
		Trace trace{};
		stage1_fights_setup(state);
		stage1_boss_activate(state);
		for(std::uint16_t frame = 0U; frame < 288U; ++frame) {
			if(!stage1_fights_update(state, trace)) return false;
		}
		if(
			state.boss.phase != 2U || state.boss.phase_frame != 0 ||
			state.boss.mode != 0U || state.boss.phase_state != 0U ||
			state.boss.statebytes[9] != 64U || state.boss.statebytes[10] != 0xFFU ||
			trace.hp != 4650U || trace.hp_total != 4650U
		) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		stage1_fights_setup(state);
		stage1_boss_activate(state);
		state.boss.phase = 2U;
		state.boss.mode = 3U;
		state.boss.selected_pattern = 3U;
		state.boss.phase_frame = 31;
		state.boss.hp = 4650;
		state.boss.phase_end_hp = 2550;
		if(!stage1_fights_update(state, trace)) return false;
		if(
			state.bullets.bullet_template.group != static_cast<std::uint8_t>(
				BulletGroup::spread_stack_aimed
			) ||
			state.bullets.bullet_template.stack != 5U ||
			state.bullets.bullet_template.stack_speed_delta != 7U ||
			state.bullets.bullet_template.speed != 16U || bullet_count(state) != 25U ||
			trace.sounds != std::vector<std::uint8_t>{15U}
		) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		stage1_fights_setup(state);
		stage1_boss_activate(state);
		state.boss.phase = 3U;
		state.boss.mode = 1U;
		state.boss.selected_pattern = 1U;
		state.boss.phase_frame = 39;
		state.boss.statebytes[9] = 80U;
		state.boss.hp = 2550;
		state.boss.phase_end_hp = 450;
		state.random.ring[0] = 1U;
		state.random.ring[1] = 0U;
		state.random.ring[2] = 2U;
		state.random.ring[3] = 0U;
		state.random.ring[4] = 3U;
		if(!stage1_fights_update(state, trace)) return false;
		if(
			state.random.cursor != 4U || state.bullets.bullet_template.spawn_type !=
				bullet_spawn_type_cloud_backwards ||
			state.bullets.bullet_template.patnum != 44U || bullet_count(state) != 4U ||
			trace.sounds != std::vector<std::uint8_t>{3U}
		) return false;
	}
	return true;
}

[[nodiscard]] bool sara_lasers_and_defeat_boundary() noexcept
{
	auto state = source_state();
	Trace trace{};
	stage1_fights_setup(state);
	stage1_boss_activate(state);
	state.boss.phase = 4U;
	state.boss.phase_frame = 31;
	state.boss.hp = 450;
	state.boss.phase_end_hp = 0;
	if(!stage1_fights_update(state, trace)) return false;
	if(
		state.lasers.pool[0].coords.angle != 0xE0U ||
		state.lasers.pool[1].coords.angle != 0xA0U ||
		state.lasers.pool[0].color != 8U || state.lasers.pool[1].coords.width != 1U ||
		state.lasers.pool[0].grow_to_width != 8U || state.lasers.pool[1].grow_to_width != 8U ||
		state.boss.statebytes[13] != 1U || state.boss.statebytes[14] != 1U
	) return false;

	state.boss.phase = 253U;
	state.boss.phase_frame = 31;
	state.boss.phase_state = 1U;
	state.random.cursor = 0U;
	if(!stage1_fights_update(state, trace)) return false;
	if(
		state.boss.phase != 254U || state.boss.phase_frame != 0 ||
		!state.boss_explosions.big.alive || !state.bullets.zap.active() ||
		state.score.score_delta != 10000U || trace.pointnums.size() != 10U ||
		state.player_invincibility_time != 255U
	) return false;
	for(std::uint8_t frame = 0U; frame < 64U; ++frame) {
		if(!stage1_fights_update(state, trace)) return false;
	}
	if(state.boss.phase != 255U || state.boss.phase_frame != 0 || !state.bombing_disabled) {
		return false;
	}
	if(!stage1_fights_update(state, trace)) return false;
	if(!state.dialog_pending || !state.stage_clear_requested || state.palette_tone != 60U) {
		return false;
	}

	state.boss_explosions.big.radius_delta = {1, 1};
	state.midboss_active = true;
	state.midboss.phase = 3U;
	state.midboss.damage_this_frame = 1U;
	stage1_fights_render_tail(state, trace);
	return state.boss_explosions.big_frame == 1 && state.palette_tone == 150U &&
		state.midboss.damage_this_frame == 0U;
}

} // namespace

int main()
{
	if(!setup_and_midboss()) {
		std::cerr << "TH05 Stage 1 setup/midboss test failure\n";
		return EXIT_FAILURE;
	}
	if(!sara_entrance_and_patterns()) {
		std::cerr << "TH05 Sara entrance/pattern test failure\n";
		return EXIT_FAILURE;
	}
	if(!sara_lasers_and_defeat_boundary()) {
		std::cerr << "TH05 Sara laser/defeat test failure\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
