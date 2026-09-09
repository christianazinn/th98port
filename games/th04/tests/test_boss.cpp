#include "th04/core/boss.hpp"
#include "th04/core/state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
using namespace th04::core;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	std::vector<Point> pointnums{};

	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
	void pointnum(Point point, std::uint16_t, bool) noexcept override {
		pointnums.push_back(point);
	}
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.rank = 1U;
	state.resources.playperf = 16U;
	state.resources.playperf_max = 32U;
	state.player_pos.cur = {3000, 5000};
	state.reimu_a.homing_target = {homing_target_none, homing_target_none};
	return state;
}

[[nodiscard]] std::size_t bullets_alive(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &bullet : state.bullets.pool) {
		if(bullet.flag == bullet_alive) ++count;
	}
	return count;
}

[[nodiscard]] bool setup_activation_and_midboss_entrance()
{
	auto state = source_state();
	Trace presentation{};
	stage1_fights_setup(state);
	if(state.midboss.frames_until != 3100U || state.midboss.pos.cur != Point{3072, 5888} ||
		state.midboss_active || state.boss_active || state.boss.sprite != 128U ||
		state.boss.hitbox_radius != Point{384, 256}) return false;
	state.stage_frame = 3099U;
	stage1_midboss_activate(state);
	if(state.midboss_active) return false;
	state.stage_frame = 3100U;
	stage1_midboss_activate(state);
	if(!state.midboss_active || state.midboss.phase != 0U) return false;
	for(unsigned frame = 0U; frame < 288U; ++frame) {
		if(!stage1_fights_update(state, presentation)) return false;
	}
	return state.midboss.phase == 1U && state.midboss.phase_frame == 0 &&
		state.midboss.pos.cur.y == 1216 && state.midboss.sprite == 136U &&
		state.tile_ring[(6U * 32U) + 12U] == 59U &&
		state.spark_ring_offset == 512U && presentation.sounds == std::vector<std::uint8_t>{9U};
}

[[nodiscard]] bool midboss_fan_and_exit()
{
	auto state = source_state();
	Trace presentation{};
	stage1_fights_setup(state);
	state.midboss_active = true;
	state.midboss.phase = 3U;
	state.midboss.phase_frame = 0;
	state.midboss.hp = 800;
	state.midboss.pos.cur = {3072, 2000};
	for(unsigned frame = 0U; frame < 8U; ++frame) {
		if(!stage1_fights_update(state, presentation)) return false;
	}
	if(bullets_alive(state) != 2U || state.midboss.stage1_fan_angle != 13U ||
		state.bullets.pool[439].from_group != BulletGroup::single ||
		state.bullets.pool[439].angle != 1U || state.bullets.pool[438].angle != 127U) {
		return false;
	}
	state.std_data.scroll_speed = 3U;
	if(!stage1_fights_update(state, presentation)) return false;
	return state.midboss.phase == 254U && state.midboss.sprite == 4U &&
		state.std_data.scroll_speed == 4U && presentation.sounds == std::vector<std::uint8_t>{12U};
}

[[nodiscard]] bool orange_entrance_and_pattern_matrix()
{
	{
		auto state = source_state();
		Trace presentation{};
		stage1_fights_setup(state);
		stage1_boss_activate(state);
		for(std::uint16_t frame = 0U; frame < 384U; ++frame) {
			state.stage_frame = frame;
			if(!stage1_fights_update(state, presentation)) return false;
		}
		if(state.boss.phase != 2U || state.boss.phase_frame != 0 ||
			state.boss.mode != 0U || bullets_alive(state) != 48U ||
			presentation.sounds != std::vector<std::uint8_t>{8U, 13U, 6U}) return false;
	}

	const std::array expected{4U, 6U, 16U, 16U};
	for(std::uint8_t mode = 1U; mode <= 4U; ++mode) {
		auto state = source_state();
		Trace presentation{};
		stage1_fights_setup(state);
		stage1_boss_activate(state);
		state.boss.phase = 2U;
		state.boss.mode = mode;
		state.boss.phase_frame = 86;
		state.boss.hp = 3050;
		state.boss.phase_end_hp = 1950;
		state.stage_frame = mode == 2U ? 4U : (mode == 1U ? 2U : 8U);
		if(!stage1_fights_update(state, presentation) || bullets_alive(state) != expected[mode - 1U]) return false;
	}

	{
		auto state = source_state();
		Trace presentation{};
		stage1_fights_setup(state);
		stage1_boss_activate(state);
		state.boss.phase = 3U;
		state.boss.mode = 1U;
		state.boss.phase_frame = 1;
		state.boss.hp = 700;
		state.boss.phase_end_hp = 450;
		state.boss.pos.cur = {3072, 1280};
		state.stage_frame = 4U;
		if(!stage1_fights_update(state, presentation) || bullets_alive(state) != 4U) return false;
	}

	{
		auto state = source_state();
		Trace presentation{};
		stage1_fights_setup(state);
		stage1_boss_activate(state);
		state.boss.phase = 4U;
		state.boss.mode = 1U;
		state.boss.phase_frame = 320;
		state.boss.hp = 450;
		state.boss.phase_end_hp = 0;
		state.boss.pos.cur = {3072, 1280};
		state.stage_frame = 4U;
		if(!stage1_fights_update(state, presentation) || bullets_alive(state) != 6U) return false;
	}
	return true;
}

[[nodiscard]] bool terminal_and_render_tail()
{
	auto state = source_state();
	Trace presentation{};
	stage1_fights_setup(state);
	stage1_boss_activate(state);
	state.boss.phase = 5U;
	state.boss.phase_frame = 31;
	state.boss.phase_state = 1U;
	state.boss.pos.cur = {3072, 1280};
	state.pointnum_times_2 = true;
	if(!stage1_fights_update(state, presentation) || state.boss.phase != 254U ||
		!state.bullets.zap.active() || state.score.score_delta != 12800U ||
		presentation.pointnums.size() != 10U || state.pointnum_times_2 ||
		state.player_invincibility_time != 255U) return false;
	for(unsigned frame = 0U; frame < 13U; ++frame) {
		if(!stage1_fights_update(state, presentation)) return false;
	}
	if(state.boss.phase != 254U || state.boss.phase_frame != 13 ||
		state.bullets.slowdown_factor != 2U) return false;
	for(unsigned frame = 13U; frame < 64U; ++frame) {
		if(!stage1_fights_update(state, presentation)) return false;
	}
	if(state.boss.phase != 255U || !state.bombing_disabled) return false;
	state.palette_changed = false;
	if(!stage1_fights_update(state, presentation) || !state.stage_clear_requested ||
		state.palette_tone != 60U || !state.palette_changed) return false;

	state.midboss_active = true;
	state.midboss.phase = 3U;
	state.midboss.damage_this_frame = 1U;
	state.boss_explosions.big.alive = true;
	state.boss_explosions.big.radius_delta = {1, 1};
	stage1_fights_render_tail(state, presentation);
	if(state.midboss.damage_this_frame != 0U || state.boss_explosions.big_frame != 1 ||
		state.palette_tone != 150U) return false;
	state.circles.pool[0] = {circle_alive, 16U, 0, 0, 4, 8};
	stage1_circles_update(state, presentation);
	if(state.circles.pool[0].flag != circle_remove) return false;
	stage1_circles_update(state, presentation);
	return state.circles.pool[0].flag == circle_free;
}

} // namespace

int main()
{
	const std::array checks{
		setup_activation_and_midboss_entrance(), midboss_fan_and_exit(),
		orange_entrance_and_pattern_matrix(), terminal_and_render_tail(),
	};
	for(std::size_t index = 0U; index < checks.size(); ++index) {
		if(!checks[index]) {
			std::cerr << "TH04 Stage 1 boss check failed: " << index << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
