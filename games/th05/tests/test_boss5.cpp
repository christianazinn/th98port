#include "th05/core/boss.hpp"
#include "th05/core/bullet.hpp"
#include "th05/core/laser.hpp"
#include "th05/core/random.hpp"
#include "th05/core/state.hpp"
#include "th05/core/sword.hpp"

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
	if(!gameplay_init(state, 1U, 0U, 3U, 3U, 4U)) std::abort();
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

[[nodiscard]] std::size_t live_sword_count(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &sword : state.swords.pool) {
		if(sword.flag == sword_alive) ++count;
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

[[nodiscard]] bool m5_setup_live_pattern_and_render_clip() noexcept
{
	auto state = source_state();
	Trace trace{};
	stage5_fights_setup(state);
	if(
		state.midboss_active || state.boss_active ||
		state.midboss.frames_until != 4800U || state.midboss.pos.cur != Point{3072, -512} ||
		state.midboss.pos.velocity != Point{0, 8} || state.midboss.hp != 1550 ||
		state.midboss.sprite != 212U || state.boss.pos.cur != Point{1536, 1024} ||
		state.stage4.yuki.pos.cur != Point{3072, 1024} ||
		state.stage5.interval_phase4 != 10U || state.stage5.interval_phase7 != 32U
	) return false;

	state.stage_frame = 4799U;
	stage5_midboss_activate(state);
	if(state.midboss_active) return false;
	state.stage_frame = 4800U;
	stage5_midboss_activate(state);
	if(!state.midboss_active || state.midboss.phase != 0U) return false;
	for(std::uint16_t frame{}; frame < 192U; ++frame) {
		if(!stage5_fights_update(state, trace)) return false;
	}
	if(
		state.midboss.phase != 1U || state.midboss.phase_frame != 0 ||
		state.midboss.hp != 1550 || state.boss.statebytes[13U] != 0U ||
		state.boss.statebytes[14U] != 1U || state.boss.statebytes[15U] != 0U ||
		state.stage5.midboss_pattern != Stage5MidbossPattern::blue_spreads ||
		trace.hp != 1550 || trace.hp_total != 1550
	) return false;

	// A live phase-1 hittest returns false as an outcome predicate; it must not
	// be confused with an unsupported update while M5 still has positive HP.
	if(!stage5_fights_update(state, trace) || state.midboss.phase != 1U ||
		state.midboss.phase_frame != 1 || state.midboss.hp != 1550) return false;

	state.midboss.phase_frame = 31;
	trace.sounds.clear();
	if(!stage5_fights_update(state, trace)) return false;
	if(
		state.bullets.bullet_template.group != static_cast<std::uint8_t>(BulletGroup::spread) ||
		state.bullets.bullet_template.patnum != 116U ||
		state.bullets.bullet_template.speed != 88U || live_bullet_count(state) != 13U ||
		trace.sounds != std::vector<std::uint8_t>{3U}
	) return false;

	state.midboss.phase = 1U;
	state.midboss.damage_this_frame = 1U;
	state.midboss.pos.cur.y = 255;
	stage5_fights_render_tail(state, trace);
	if(state.midboss.damage_this_frame != 1U) return false;
	state.midboss.pos.cur.y = 256;
	stage5_fights_render_tail(state, trace);
	return state.midboss.damage_this_frame == 0U;
}

[[nodiscard]] bool yumeko_entrance_and_phase_boundaries() noexcept
{
	{
		auto state = source_state();
		Trace trace{};
		stage5_fights_setup(state);
		stage5_boss_activate(state);
		for(std::uint16_t frame{}; frame < 192U; ++frame) {
			if(!stage5_fights_update(state, trace)) return false;
		}
		if(
			state.boss.phase != 2U || state.boss.phase_frame != 17 ||
			state.boss.mode != 1U || state.boss.phase_state != 0U ||
			state.boss.hp != 8300 || state.boss.phase_end_hp != 7500 ||
			state.stage5.yumeko_pattern != Stage5YumekoPattern::phase2_sword_fan ||
			state.stage4.yuki.pos.cur.y != -528 || trace.hp != 8300 || trace.hp_total != 8300
		) {
			std::cerr << "Yumeko entrance boundary mismatch\n";
			return false;
		}
	}

	// Phase 4 tightens both its sword interval and arrowhead window once, then
	// clears bullets for exactly the common 20-frame source duration.
	{
		auto state = source_state();
		Trace trace{};
		stage5_fights_setup(state);
		stage5_boss_activate(state);
		state.boss.phase = 4U;
		state.boss.phase_frame = 15;
		state.boss.hp = 6299;
		state.boss.phase_end_hp = 5700;
		state.stage5.yumeko_pattern = Stage5YumekoPattern::phase4_arrowheads_and_sword;
		if(!stage5_fights_update(state, trace)) return false;
		if(
			state.bullets.bullet_template.patnum != 84U ||
			state.bullets.bullet_template.group != static_cast<std::uint8_t>(BulletGroup::single) ||
			state.swords.sword_template.twirl_time != 32U || state.swords.sword_template.speed != 76U
		) return false;
		state.boss.phase_frame = 31;
		if(!stage5_fights_update(state, trace)) return false;
		if(
			state.bullets.bullet_template.patnum != 84U ||
			state.bullets.bullet_template.group != static_cast<std::uint8_t>(BulletGroup::single) ||
			state.stage5.interval_phase4 != 8U || state.boss.statebytes[12U] != 48U ||
			state.boss.statebytes[13U] != 1U || state.resources.bullet_clear_time != 20U
		) {
			std::cerr << "Yumeko phase-4 pattern mismatch\n";
			return false;
		}
	}

	// Phase 7 retains the pattern's literal shootout template, then couples it
	// with exactly two random swords at the selected interval.
	{
		auto state = source_state();
		Trace trace{};
		stage5_fights_setup(state);
		stage5_boss_activate(state);
		state.boss.phase = 7U;
		state.boss.phase_frame = 15;
		state.boss.hp = 2700;
		state.boss.phase_end_hp = 1200;
		state.stage5.yumeko_pattern = Stage5YumekoPattern::phase7_arrowheads_laser_swords;
		if(!stage5_fights_update(state, trace)) return false;
		if(
			state.lasers.laser_template.age != 24 ||
			state.lasers.laser_template.shootout_speed != 100 ||
			state.lasers.laser_template.coords.width != 6U ||
			state.lasers.laser_template.color != 8U ||
			state.lasers.laser_template.active_at_age != 28
		) return false;
		state.boss.phase_frame = 31;
		if(!stage5_fights_update(state, trace)) return false;
		if(shootout_laser_count(state) != 1U || live_sword_count(state) != 2U) {
			std::cerr << "Yumeko phase-7 pattern mismatch\n";
			return false;
		}
	}

	// Phase 8 deliberately reuses Yuki's leftover position as its marching
	// sword-band coordinate before it turns into accelerated blue spreads.
	{
		auto state = source_state();
		Trace trace{};
		stage5_fights_setup(state);
		stage5_boss_activate(state);
		state.boss.phase = 8U;
		state.boss.phase_frame = 15;
		state.boss.hp = 1200;
		state.boss.phase_end_hp = 0;
		if(!stage5_fights_update(state, trace)) return false;
		const auto band_y = state.stage4.yuki.pos.cur.y;
		if(band_y < 0 || band_y >= 512 || state.boss.statebytes[15U] != 0x1EU) return false;
		state.boss.phase_frame = 31;
		if(!stage5_fights_update(state, trace)) return false;
		if(
			live_sword_count(state) != 1U || state.swords.sword_template.angle != 0U ||
			state.swords.sword_template.origin.x != 256 ||
			state.stage4.yuki.pos.cur.y != static_cast<std::int16_t>(band_y + 480)
		) {
			std::cerr << "Yumeko phase-8 pattern mismatch\n";
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool yumeko_turning_rings_defeat_and_render_tail() noexcept
{
	auto state = source_state();
	Trace trace{};
	stage5_fights_setup(state);
	stage5_boss_activate(state);
	state.boss.phase = 10U;
	state.boss.phase_frame = 15;
	state.boss.hp = 1200;
	state.boss.phase_end_hp = 0;
	if(!stage5_fights_update(state, trace)) return false;
	if(
		state.bullets.bullet_template.special_motion != static_cast<std::uint8_t>(
			BulletSpecialMotion::decelerate_then_turn
		) || state.bullets.special_turns_max != 2U || state.bullets.bullet_template.speed != 38U
	) return false;
	state.boss.phase_frame = 31;
	if(!stage5_fights_update(state, trace)) return false;
	if(
		state.bullets.bullet_template_special_angle != 0xC0U ||
		live_bullet_count(state) != 18U || trace.sounds.back() != 3U
	) return false;

	state.boss.phase = 10U;
	state.boss.phase_frame = 1199;
	state.boss.hp = 1;
	if(!stage5_fights_update(state, trace)) return false;
	if(state.boss.phase != 253U || state.boss.phase_frame != 0) return false;
	state.boss.damage_this_frame = 1U;
	stage5_fights_render_tail(state, trace);
	return state.boss.damage_this_frame == 0U;
}

} // namespace

int main()
{
	if(!m5_setup_live_pattern_and_render_clip()) {
		std::cerr << "TH05 Stage 5 M5 test failure\n";
		return EXIT_FAILURE;
	}
	if(!yumeko_entrance_and_phase_boundaries()) {
		std::cerr << "TH05 Yumeko pattern test failure\n";
		return EXIT_FAILURE;
	}
	if(!yumeko_turning_rings_defeat_and_render_tail()) {
		std::cerr << "TH05 Yumeko final-pattern test failure\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
