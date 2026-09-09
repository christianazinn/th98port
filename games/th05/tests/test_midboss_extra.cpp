#include "th05/core/midboss_extra.hpp"
#include "th05/core/state.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using namespace th05::core;
using Point = pc98::th0405::SubpixelPoint;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	std::vector<std::uint16_t> pointnums{};
	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
	void pointnum(Point, std::uint16_t value, bool) noexcept override
	{
		pointnums.push_back(value);
	}
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.random.ring.fill(0U);
	state.resources.playperf = 48U;
	state.resources.playperf_max = 48U;
	state.player_pos.cur = {5000, 5000};
	state.playchar = 1U;
	state.stage_id = 6U;
	return state;
}

[[nodiscard]] bool setup_and_activation_preserve_source_unowned_fields()
{
	auto state = source_state();
	auto &midboss = state.midboss;
	midboss.phase = 7U;
	midboss.phase_frame = 91;
	midboss.damage_this_frame = 3U;
	midboss.pos.velocity = {123, -456};
	state.midboss_active = true;
	extra_midboss_setup(state);
	if(
		midboss.frames_until != 5800U || midboss.pos.cur != Point{3072, -256} ||
		midboss.pos.prev != Point{3072, -256} || midboss.pos.velocity != Point{123, -456} ||
		midboss.angle != 0x40U || midboss.hp != 3000 || midboss.sprite != 220U ||
		midboss.phase != 7U || midboss.phase_frame != 91 ||
		midboss.damage_this_frame != 3U || !state.midboss_active
	) return false;

	state.stage_frame = 5799U;
	extra_midboss_activate(state);
	if(midboss.phase != 7U || midboss.phase_frame != 91 || !state.midboss_active) return false;
	state.stage_frame = 5800U;
	extra_midboss_activate(state);
	return midboss.phase == 0U && midboss.phase_frame == 0 && state.midboss_active;
}

[[nodiscard]] bool invincible_entrance_wait_and_transition()
{
	auto state = source_state();
	Trace trace{};
	extra_midboss_setup(state);
	state.stage_frame = 5800U;
	extra_midboss_activate(state);
	for(std::int16_t i{}; i < 112; ++i) {
		if(!extra_midboss_update(state, trace)) return false;
	}
	const auto &midboss = state.midboss;
	return midboss.phase == 1U && midboss.phase_frame == 0 && midboss.angle == 0U &&
		midboss.hp == 3000 && midboss.sprite == 220U &&
		state.boss.statebytes[12U] == 0U && state.boss.statebytes[14U] == 0U &&
		state.boss.statebytes[15U] == 0U && state.random.cursor == 0U &&
		trace.sounds == std::vector<std::uint8_t>{15U};
}

[[nodiscard]] bool phase_one_patterns_preserve_template_and_rng_order()
{
	{
		auto state = source_state();
		Trace trace{};
		state.midboss.phase = 1U;
		state.midboss.phase_frame = 93;
		state.midboss.pos.cur = {3072, 2048};
		state.midboss.pos.prev = state.midboss.pos.cur;
		state.midboss.hp = 3000;
		state.bullets.bullet_template.angle = 0x40U;
		state.random.ring[0] = 1U;
		if(!extra_midboss_update(state, trace)) return false;
		const auto &bullet = state.bullets.bullet_template;
		if(
			state.midboss.phase_frame != 94 || state.midboss.sprite != 221U ||
			state.boss.statebytes[13U] != 1U || state.random.cursor != 1U ||
			bullet.spawn_type != bullet_spawn_type_no_decelerate || bullet.patnum != 47U ||
			bullet.group != static_cast<std::uint8_t>(BulletGroup::ring) ||
			bullet.special_motion != static_cast<std::uint8_t>(BulletSpecialMotion::speedup) ||
			bullet.spread != 18U || bullet.spread_angle_delta != 16U || bullet.speed != 8U ||
			bullet.angle != 0x41U || state.bullets.special_turns_max != 2U ||
			trace.sounds != std::vector<std::uint8_t>{15U}
		) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		state.midboss.phase = 1U;
		state.midboss.phase_frame = 93;
		state.midboss.pos.cur = {3072, 2048};
		state.midboss.hp = 3000;
		state.boss.statebytes[14U] = 1U;
		state.page_back = 1U;
		if(!extra_midboss_update(state, trace)) return false;
		const auto &bullet = state.bullets.bullet_template;
		if(
			state.random.cursor != 24U ||
			bullet.spawn_type != static_cast<std::uint8_t>(
				bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
			) || bullet.patnum != 0U ||
			bullet.group != static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed) ||
			bullet.speed != 16U || bullet.spread != 12U || bullet.spread_angle_delta != 10U ||
			trace.sounds != std::vector<std::uint8_t>{15U}
		) return false;
	}

	auto state = source_state();
	Trace trace{};
	state.midboss.phase = 1U;
	state.midboss.phase_frame = 95;
	state.midboss.pos.cur = {3072, 2048};
	state.midboss.hp = 3000;
	state.boss.statebytes[12U] = 1U;
	if(!extra_midboss_update(state, trace)) return false;
	const auto &bullet = state.bullets.bullet_template;
	return state.midboss.phase_frame == 96 && bullet.patnum == 47U &&
		bullet.spawn_type == static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_backwards | bullet_spawn_type_no_decelerate
		) && bullet.group == static_cast<std::uint8_t>(BulletGroup::spread_stack) &&
		bullet.special_motion == static_cast<std::uint8_t>(
			BulletSpecialMotion::decelerate_then_turn
		) && bullet.spread == 5U && bullet.spread_angle_delta == 8U && bullet.stack == 4U &&
		bullet.stack_speed_delta == 16U && bullet.speed == 24U && bullet.angle == 0U &&
		state.bullets.special_turns_max == 1U &&
		state.bullets.bullet_template_special_angle == 0x38U &&
		trace.sounds == std::vector<std::uint8_t>{3U};
}

[[nodiscard]] bool phase_one_score_timeout_defeat_and_render_mutation()
{
	{
		auto state = source_state();
		Trace trace{};
		state.midboss.phase = 1U;
		state.midboss.phase_frame = 127;
		state.midboss.pos.cur = {3072, 2048};
		state.midboss.hp = 999;
		if(!extra_midboss_update(state, trace)) return false;
		if(
			state.midboss.phase_frame != 0 || state.midboss.angle != 0x10U ||
			state.boss.statebytes[12U] != 1U || state.boss.statebytes[14U] != 1U ||
			state.boss.statebytes[15U] != 1U || state.resources.bullet_clear_time != 20U ||
			state.score.score_delta != 12800U || state.random.cursor != 20U ||
			trace.sounds != std::vector<std::uint8_t>{15U} ||
			trace.pointnums != std::vector<std::uint16_t>(10U, 1280U)
		) return false;
	}

	auto state = source_state();
	Trace trace{};
	state.midboss.phase = 1U;
	state.midboss.phase_frame = 127;
	state.midboss.pos.cur = {3072, 2048};
	state.midboss.hp = 3000;
	state.boss.statebytes[14U] = 20U;
	state.midboss_active = true;
	if(!extra_midboss_update(state, trace)) return false;
	if(
		state.midboss.phase != 254U || state.midboss.phase_frame != 0 ||
		state.midboss.sprite != 4U || trace.sounds != std::vector<std::uint8_t>{12U}
	) return false;
	if(!extra_midboss_update(state, trace)) return false;
	if(state.midboss_active || state.playfield_shake_anim_time != 10 ||
		state.midboss.phase_frame != 1) return false;
	for(std::int16_t i{}; i < 127; ++i) {
		if(!extra_midboss_update(state, trace)) return false;
	}
	if(state.midboss.phase != 255U || state.midboss.sprite != 12U) return false;
	if(!extra_midboss_update(state, trace) || state.midboss.hp != 0) return false;

	state.midboss.phase = 1U;
	state.midboss.pos.cur.y = 255;
	state.midboss.damage_this_frame = 3U;
	extra_midboss_render_tail(state);
	if(state.midboss.damage_this_frame != 3U) return false;
	state.midboss.pos.cur.y = 256;
	extra_midboss_render_tail(state);
	if(state.midboss.damage_this_frame != 0U) return false;
	state.midboss.phase = 254U;
	state.midboss.damage_this_frame = 4U;
	extra_midboss_render_tail(state);
	return state.midboss.damage_this_frame == 4U;
}

} // namespace

int main()
{
	if(!setup_and_activation_preserve_source_unowned_fields()) {
		std::cerr << "setup/activation\n";
		return EXIT_FAILURE;
	}
	if(!invincible_entrance_wait_and_transition()) {
		std::cerr << "entrance/wait\n";
		return EXIT_FAILURE;
	}
	if(!phase_one_patterns_preserve_template_and_rng_order()) {
		std::cerr << "phase-one patterns\n";
		return EXIT_FAILURE;
	}
	if(!phase_one_score_timeout_defeat_and_render_mutation()) {
		std::cerr << "score/timeout/render\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
