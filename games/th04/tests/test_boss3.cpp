#include "th04/core/boss.hpp"
#include "th04/core/state.hpp"
#include "th04/core/stage.hpp"

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
	if(!gameplay_init(state, 1U, 0U, 0U, 3U, 3U, false)) std::abort();
	state.random.seed = 318U;
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

[[nodiscard]] std::size_t items_alive(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &item : state.items.items) {
		if(item.flag == item_alive) ++count;
	}
	return count;
}

[[nodiscard]] bool setup_midboss_and_pattern()
{
	auto state = source_state();
	Trace presentation{};
	stage3_fights_setup(state);
	if(state.midboss.frames_until != 1600U ||
		state.midboss.pos.cur != Point{3072, -512} || state.midboss.hp != 850 ||
		state.midboss_active || state.boss_active ||
		state.boss.pos.cur != Point{3072, 1024} || state.boss.sprite != 134U ||
		state.boss.hitbox_radius != Point{384, 384}) return false;
	state.stage_frame = 1599U;
	stage3_midboss_activate(state);
	if(state.midboss_active) return false;
	state.stage_frame = 1600U;
	stage3_midboss_activate(state);
	if(!state.midboss_active || state.midboss.phase != 0U) return false;
	for(unsigned frame = 0U; frame < 20U; ++frame) {
		if(!stage3_fights_update(state, presentation)) return false;
	}
	if(state.midboss.phase != 1U || state.midboss.phase_frame != 0 ||
		state.midboss.pos.cur != Point{3072, 768} ||
		state.midboss.stage3_pattern > 3U) return false;

	state.midboss.phase = 1U;
	state.midboss.phase_frame = 31;
	state.midboss.stage3_pattern = 1U;
	state.midboss.pos.velocity = {};
	presentation.sounds.clear();
	if(!stage3_fights_update(state, presentation)) return false;
	return state.midboss.stage3_pattern == 0xFFU && state.midboss.phase_frame == 0 &&
		bullets_alive(state) == 32U &&
		presentation.sounds == std::vector<std::uint8_t>{9U};
}

[[nodiscard]] bool midboss_twelfth_flight_keeps_running()
{
	auto state = source_state();
	Trace presentation{};
	stage3_fights_setup(state);
	state.midboss_active = true;
	state.midboss.phase = 1U;
	state.midboss.stage3_pattern = 0xFFU;
	state.midboss.stage3_patterns_done = 11U;
	state.midboss.phase_frame = 0;
	state.midboss.pos.cur = {3072, 1536};
	state.midboss.pos.prev = state.midboss.pos.cur;
	state.midboss.pos.velocity = {};
	if(!stage3_fights_update(state, presentation) ||
		state.midboss.phase_frame != 1 || state.midboss.stage3_patterns_done != 12U ||
		state.midboss.sprite != 1U || state.midboss.pos.velocity == Point{}) return false;
	const auto last_flight_velocity = state.midboss.pos.velocity;
	for(unsigned frame = 0U; frame < 5U; ++frame) {
		if(!stage3_fights_update(state, presentation)) return false;
	}
	return state.midboss.stage3_pattern == 0xFFU &&
		state.midboss.stage3_patterns_done == 12U && state.midboss.phase_frame == 6 &&
		state.midboss.pos.velocity == last_flight_velocity;
}

[[nodiscard]] bool elly_entry_and_boomerang()
{
	{
		auto state = source_state();
		Trace presentation{};
		stage3_fights_setup(state);
		state.boss_active = true;
		state.boss.phase = 1U;
		state.boss.phase_frame = 32;
		if(!stage3_fights_update(state, presentation) ||
			state.boss.elly_boomerang.state != 1U ||
			state.boss.elly_boomerang.state_frame != 0U ||
			state.boss.elly_boomerang.flag != EllyBoomerangFlag::free ||
			state.boss.phase_frame != 34 || !presentation.sounds.empty()) return false;
		if(!stage3_fights_update(state, presentation) ||
			state.boss.elly_boomerang.state_frame != 1U ||
			state.boss.phase_frame != 36 ||
			presentation.sounds != std::vector<std::uint8_t>{8U}) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage3_fights_setup(state);
		state.boss_active = true;
		state.boss.phase = 1U;
		state.boss.phase_frame = 40;
		state.boss.elly_boomerang.state = 1U;
		state.boss.elly_boomerang.state_frame = 56U;
		state.boss.elly_boomerang.angle = 0U;
		state.boss.elly_boomerang.speed = 8U;
		if(!stage3_fights_update(state, presentation) ||
			state.boss.elly_boomerang.flag != EllyBoomerangFlag::thrown ||
			state.boss.elly_boomerang.state_frame != 57U ||
			state.boss.elly_boomerang.pos.cur == state.boss.pos.cur ||
			presentation.sounds != std::vector<std::uint8_t>{9U}) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage3_fights_setup(state);
		state.boss_active = true;
		state.boss.phase = 1U;
		state.boss.elly_boomerang.flag = EllyBoomerangFlag::thrown;
		state.boss.elly_boomerang.state = 7U;
		state.boss.elly_boomerang.pos.cur = state.boss.pos.cur;
		if(!stage3_fights_update(state, presentation) ||
			state.boss.elly_boomerang.state != 8U ||
			state.boss.elly_boomerang.flag != EllyBoomerangFlag::caught) return false;
		if(!stage3_fights_update(state, presentation) ||
			state.boss.elly_boomerang.flag != EllyBoomerangFlag::free ||
			state.boss.elly_boomerang.state_frame != 1U) return false;
	}
	return true;
}

[[nodiscard]] bool elly_fight_sets_and_defeat_tail()
{
	{
		auto state = source_state();
		Trace presentation{};
		stage3_fights_setup(state);
		state.boss_active = true;
		state.boss.phase = 2U;
		state.boss.phase_frame = 31;
		state.boss.hp = 6000;
		state.boss.phase_end_hp = 6000;
		if(!stage3_fights_update(state, presentation) || state.boss.phase != 3U ||
			state.boss.mode != 0U || state.boss.hp != 6000 ||
			state.boss.phase_end_hp != 0 || state.boss.elly_pattern_set != 0U ||
			state.boss.pos.cur != Point{3072, 1536}) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage3_fights_setup(state);
		state.boss_active = true;
		state.boss.phase = 3U;
		state.boss.mode = 7U;
		state.boss.phase_frame = 32;
		state.boss.hp = 6000;
		if(!stage3_fights_update(state, presentation) || bullets_alive(state) != 64U ||
			state.boss.phase_frame != 33 ||
			presentation.sounds != std::vector<std::uint8_t>{9U}) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage3_fights_setup(state);
		state.boss_active = true;
		state.boss.phase = 3U;
		state.boss.mode = 0U;
		state.boss.hp = 4700;
		state.boss.phase_end_hp = 0;
		if(!stage3_fights_update(state, presentation) || items_alive(state) != 5U ||
			state.resources.bullet_clear_time != 20 || state.score.score_delta != 12800U ||
			state.boss.mode != 0xFFU || state.boss.phase_frame != 0 ||
			state.boss.elly_pattern_set != 1U || presentation.pointnums.size() != 10U) {
			return false;
		}
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage3_fights_setup(state);
		state.boss_active = true;
		state.boss.phase = 4U;
		state.boss.phase_frame = 31;
		state.boss.phase_state = 1U;
		state.boss.elly_boomerang.state = 7U;
		state.boss.elly_boomerang.flag = EllyBoomerangFlag::thrown;
		if(!stage3_fights_update(state, presentation) || state.boss.phase != 254U ||
			state.boss.phase_frame != 0 || !state.bullets.zap.active() ||
			state.score.score_delta != 51200U || presentation.pointnums.size() != 40U ||
			state.player_invincibility_time != 255U ||
			state.boss.elly_boomerang.state != 0U ||
			state.boss.elly_boomerang.flag != EllyBoomerangFlag::free ||
			!state.boss_explosions.big.alive ||
			presentation.sounds != std::vector<std::uint8_t>{15U, 12U}) return false;
		stage3_fights_render_tail(state, presentation);
		if(state.boss_explosions.big.age != 1U || state.boss_explosions.big_frame != 1 ||
			state.palette_tone != 150U || !state.palette_changed) return false;
	}
	return true;
}

} // namespace

int main()
{
	const std::array checks{
		setup_midboss_and_pattern(), midboss_twelfth_flight_keeps_running(),
		elly_entry_and_boomerang(),
		elly_fight_sets_and_defeat_tail(),
	};
	for(std::size_t index = 0U; index < checks.size(); ++index) {
		if(!checks[index]) {
			std::cerr << "TH04 Stage 3 boss check failed: " << index << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
