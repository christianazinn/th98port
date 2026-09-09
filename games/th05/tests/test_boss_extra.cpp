#include "th05/core/b6ball.hpp"
#include "th05/core/boss.hpp"
#include "th05/core/bullet.hpp"
#include "th05/core/laser.hpp"
#include "th05/core/random.hpp"
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
	if(!gameplay_init(state, 1U, 0U, 3U, 3U, 6U)) std::abort();
	state.random.seed = 318U;
	randring_fill(state.random);
	state.player_pos.cur = {3072, 5000};
	state.player_pos.prev = state.player_pos.cur;
	state.shots.homing_target = {homing_target_none, homing_target_none};
	return state;
}

[[nodiscard]] std::size_t live_bullet_count(const State &state) noexcept
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

[[nodiscard]] bool setup_and_source_retention() noexcept
{
	auto state = source_state();
	state.midboss.phase = 19U;
	state.midboss.phase_frame = 77;
	state.midboss.damage_this_frame = 3U;
	state.extra.pattern_origin_x = 1234;
	state.extra.laser_slot = 7U;
	state.extra.custom_render_active = true;
	state.firewaves[0U] = {true, true, 128, 64};
	extra_fights_setup(state);
	return !state.midboss_active && !state.boss_active &&
		state.midboss.frames_until == 5800U && state.midboss.pos.cur == Point{3072, -256} &&
		state.midboss.angle == 0x40U && state.midboss.hp == 3000 && state.midboss.sprite == 220U &&
		// stagex_setup assigns the combat placement but does not blanket-clear
		// these midboss fields or BX_DATA's initialized pattern words.
		state.midboss.phase == 19U && state.midboss.phase_frame == 77 &&
		state.midboss.damage_this_frame == 3U && state.extra.pattern_origin_x == 1234 &&
		state.extra.laser_slot == 7U && !state.extra.custom_render_active &&
		state.firewaves[0U] == Firewave{true, true, 128, 64} &&
		state.boss.pos.cur == Point{3072, 1024} &&
		state.boss.sprite == 180U && state.boss.hitbox_radius == Point{384, 384};
}

[[nodiscard]] bool live_patterns_and_overlay() noexcept
{
	{
		auto state = source_state();
		Trace trace{};
		extra_fights_setup(state);
		extra_boss_activate(state);
		state.boss.phase = 2U;
		state.boss.mode = 1U;
		state.boss.phase_frame = 63;
		state.boss.hp = 26500;
		state.boss.phase_end_hp = 23800;
		state.extra.pattern = ExtraPattern::spreads_and_firewaves;
		if(!extra_fights_update(state, trace)) return false;
		if(
			// Frame 64 seeds the 0x40 sweep and immediately performs its first
			// -3 source step; the firewave cadence begins later on frame 128.
			state.boss.statebytes[14U] != 0x3DU || state.boss.statebytes[13U] != 0U ||
			state.bullets.pellet_bottom_color != 7U || live_bullet_count(state) == 0U
		) return false;
		state.boss.phase_frame = 127;
		if(!extra_fights_update(state, trace)) return false;
		if(!state.firewaves[0U].alive || trace.sounds.empty() || trace.sounds.back() != 13U) {
			return false;
		}
	}

	{
		auto state = source_state();
		Trace trace{};
		extra_fights_setup(state);
		extra_boss_activate(state);
		state.boss.phase = 2U;
		state.boss.mode = 1U;
		state.boss.phase_frame = 63;
		state.boss.hp = 26500;
		state.boss.phase_end_hp = 23800;
		state.extra.pattern = ExtraPattern::lasers_and_green_rings;
		if(!extra_fights_update(state, trace)) return false;
		if(
			state.extra.laser_slot != 1U || state.lasers.pool[0U].flag != LaserFlag::fixed_wait_to_grow ||
			state.lasers.pool[0U].coords.width != 1U || state.lasers.pool[0U].grow_to_width != 8U ||
			state.lasers.pool[0U].active_at_age != 47 || state.lasers.pool[0U].shrink_at_age != 80 ||
			state.bullets.bullet_template.group != static_cast<std::uint8_t>(BulletGroup::ring) ||
			live_bullet_count(state) == 0U
		) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		extra_fights_setup(state);
		extra_boss_activate(state);
		state.boss.phase = 2U;
		state.boss.mode = 1U;
		// The gather wrapper defers every callback until phase frame 64;
		// 71 becomes its first reached 8-frame Cheeto cadence.
		state.boss.phase_frame = 71;
		state.boss.hp = 26500;
		state.boss.phase_end_hp = 23800;
		state.extra.pattern = ExtraPattern::aimed_cheetos;
		if(!extra_fights_update(state, trace)) return false;
		if(
			state.stage4.cheeto_trails[0U].flag != CheetoFlag::decelerate ||
			state.stage4.cheeto_trails[0U].color != 11 || trace.sounds.empty() ||
			trace.sounds.back() != 15U
		) return false;
	}
	return true;
}

[[nodiscard]] bool alternating_callback_table() noexcept
{
	struct Row {
		std::uint8_t phase{}, color_phase{};
		ExtraPattern expected{};
	};
	constexpr Row rows[]{
		{2U, 0U, ExtraPattern::rotating_red_rings},
		{6U, 1U, ExtraPattern::rotating_blue_rings},
		{10U, 2U, ExtraPattern::speedup_spreads},
		{14U, 3U, ExtraPattern::aimed_cheetos},
	};
	for(const auto row : rows) {
		auto state = source_state();
		Trace trace{};
		extra_fights_setup(state);
		extra_boss_activate(state);
		state.boss.phase = row.phase;
		state.boss.mode = 0U;
		// The update increments this to 60, so bx_upd.cpp's source flystep
		// sees frame 28 and selects EXALICE_PATTERNS[color][1].
		state.boss.phase_frame = 59;
		state.boss.phase_state = 0U;
		state.boss.statebytes[9U] = row.color_phase;
		state.boss.hp = 26500;
		state.boss.phase_end_hp = 0;
		if(!extra_fights_update(state, trace)) return false;
		if(
			state.boss.phase != row.phase || state.boss.mode != 1U ||
			state.boss.phase_frame != 0 || state.boss.phase_state != 1U ||
			state.extra.pattern != row.expected
		) return false;
	}
	return true;
}

[[nodiscard]] bool final_bonus_and_render_tail() noexcept
{
	{
		auto state = source_state();
		Trace trace{};
		extra_fights_setup(state);
		extra_boss_activate(state);
		state.boss.phase = 17U;
		state.boss.phase_frame = 63;
		state.boss.hp = 3400;
		state.boss.phase_end_hp = 0;
		if(!extra_fights_update(state, trace)) return false;
		if(
			state.boss.sprite != 181U || state.boss.phase_frame != 64 ||
			state.bullets.bullet_template.patnum != 116U ||
			state.bullets.bullet_template.group != static_cast<std::uint8_t>(BulletGroup::spread) ||
			state.bullets.bullet_template.spread != 5U || state.bullets.bullet_template.spread_angle_delta != 21U
		) return false;
		state.boss.phase = 253U;
		state.boss.phase_frame = 31;
		state.boss.phase_state = 1U;
		if(!extra_fights_update(state, trace)) return false;
		if(
			state.boss.phase != 254U || state.boss.phase_frame != 0 ||
			state.score.score_delta != 200000U || !state.bullets.zap.active() ||
			state.player_invincibility_time != 255U
		) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		extra_fights_setup(state);
		extra_boss_activate(state);
		state.boss.damage_this_frame = 1U;
		state.boss.phase = 2U;
		extra_fights_render_tail(state, trace);
		if(state.boss.damage_this_frame != 0U) return false;
		state.boss.damage_this_frame = 1U;
		state.boss.phase = 254U;
		extra_fights_render_tail(state, trace);
		if(state.boss.damage_this_frame != 1U) return false;
	}
	return true;
}

} // namespace

int main()
{
	if(!setup_and_source_retention()) {
		std::cerr << "TH05 Extra setup/retention test failure\n";
		return EXIT_FAILURE;
	}
	if(!live_patterns_and_overlay()) {
		std::cerr << "TH05 Extra live pattern/overlay test failure\n";
		return EXIT_FAILURE;
	}
	if(!alternating_callback_table()) {
		std::cerr << "TH05 Extra alternating callback-table test failure\n";
		return EXIT_FAILURE;
	}
	if(!final_bonus_and_render_tail()) {
		std::cerr << "TH05 Extra final/render-tail test failure\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
