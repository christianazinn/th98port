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
	if(!gameplay_init(state, 1U, 0U, 3U, 3U, 5U)) std::abort();
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

[[nodiscard]] std::size_t live_b6ball_count(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &ball : state.b6balls.pool) {
		if(ball.flag != B6BallFlag::free) ++count;
	}
	return count;
}

[[nodiscard]] bool shinki_setup_phase23_and_live_hittest() noexcept
{
	auto state = source_state();
	Trace trace{};
	stage6_fights_setup(state);
	if(
		state.midboss_active || state.boss_active || state.midboss.frames_until != 30000U ||
		state.boss.pos.cur != Point{3072, 1024} || state.boss.sprite != 180U ||
		state.boss.hitbox_radius != Point{384, 384}
	) return false;
	stage6_boss_activate(state);
	for(std::uint16_t frame{}; frame < 256U; ++frame) {
		if(!stage6_fights_update(state, trace)) return false;
	}
	if(
		state.boss.phase != 2U || state.boss.phase_frame != 0 || state.boss.mode != 1U ||
		state.boss.hp != 22800 || state.boss.phase_end_hp != 20600 ||
		state.stage6.phase_2_3_pattern != Stage6Phase23Pattern::curved_rings ||
		trace.hp != 22800 || trace.hp_total != 22800
	) return false;

	// An un-hit Phase 2 is the ordinary false hittest predicate, not an
	// unsupported update result.  The following source frame also performs
	// curved-rings setup and keeps Shinki alive at positive HP.
	state.boss.phase_frame = 31;
	trace.sounds.clear();
	if(!stage6_fights_update(state, trace)) return false;
	if(
		state.boss.phase == 2U && state.boss.hp == 22800 &&
		state.bullets.bullet_template.patnum == 45U &&
		state.bullets.bullet_template.group == static_cast<std::uint8_t>(BulletGroup::ring) &&
		state.bullets.bullet_template.special_motion == static_cast<std::uint8_t>(
			BulletSpecialMotion::exact_linear
		) &&
		// Frame 32 both tunes the 2.0 cloud ring and immediately emits its
		// first special ring, leaving the retained source template at 2.5.
		state.bullets.bullet_template.speed == 40U && state.bullets.bullet_template.spread == 16U &&
		trace.sounds == std::vector<std::uint8_t>{15U}
	) {
		return true;
	}
	return false;
}

[[nodiscard]] bool shinki_wings_timeout_and_devil() noexcept
{
	{
		auto state = source_state();
		Trace trace{};
		stage6_fights_setup(state);
		stage6_boss_activate(state);
		state.boss.phase = 5U;
		state.boss.phase_frame = 15;
		state.lasers.laser_template.coords.width = 6U;
		if(!stage6_fights_update(state, trace)) return false;
		if(
			state.boss.sprite != 184U || state.boss.statebytes[14U] != 100U ||
			state.lasers.pool[0U].coords.angle != 0x50U ||
			state.lasers.pool[5U].coords.angle != 0x30U ||
			trace.sounds != std::vector<std::uint8_t>{5U, 5U, 5U, 5U, 5U, 5U, 8U}
		) return false;
		state.lasers.pool[2U].coords.angle = 0x80U;
		state.boss.statebytes[15U] = 8U;
		if(!stage6_fights_update(state, trace)) return false;
		if(
			state.boss.phase != 6U || state.boss.phase_frame != 0 || state.boss.hitbox_radius.x != 800 ||
			state.stage6.wing_pattern != Stage6WingPattern::random_rain_and_spreads ||
			state.boss.statebytes[10U] != 0U
		) return false;

		state.boss.phase = 6U;
		state.boss.phase_frame = 2999;
		state.boss.hp = 14601;
		state.boss.phase_end_hp = 14600;
		state.stage6.wing_pattern = Stage6WingPattern::random_rain_and_spreads;
		if(!stage6_fights_update(state, trace)) return false;
		if(
			state.boss.phase != 7U || state.boss.phase_frame != 0 ||
			state.boss.phase_end_hp != 11600 || state.boss.statebytes[10U] != 1U ||
			state.stage6.wing_pattern != Stage6WingPattern::cheetos_within_spread_walls
		) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		stage6_fights_setup(state);
		stage6_boss_activate(state);
		state.boss.phase = 10U;
		state.boss.phase_frame = 191;
		state.boss.hp = 5600;
		state.boss.phase_end_hp = 2800;
		state.stage6.wing_pattern = Stage6WingPattern::devil;
		if(!stage6_fights_update(state, trace)) return false;
		if(
			state.stage6.devil_laser_grow_delay != 2U || live_bullet_count(state) != 4U ||
			state.lasers.pool[0U].coords.origin.x != 4608 ||
			state.lasers.pool[3U].coords.origin.x != 1536 ||
			state.lasers.pool[0U].coords.width != 1U || state.lasers.pool[0U].grow_to_width != 6U ||
			state.lasers.pool[0U].color != 14U ||
			trace.sounds.empty() || trace.sounds.back() != 3U
		) return false;
	}
	return true;
}

[[nodiscard]] bool shinki_phase12_defeat_and_render_tail() noexcept
{
	{
		auto state = source_state();
		Trace trace{};
		stage6_fights_setup(state);
		stage6_boss_activate(state);
		state.boss.phase = 12U;
		state.boss.phase_frame = 127;
		state.boss.hp = 2800;
		state.boss.phase_end_hp = 0;
		state.b6balls.b6ball_template.speed = 60U; // Retained from Devil, as in b6.cpp.
		if(!stage6_fights_update(state, trace)) return false;
		if(live_b6ball_count(state) != 2U || state.b6balls.b6ball_template.angle != 8U) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		stage6_fights_setup(state);
		stage6_boss_activate(state);
		state.boss.phase = 253U;
		state.boss.phase_frame = 31;
		state.boss.phase_state = 1U;
		if(!stage6_fights_update(state, trace)) return false;
		if(
			state.boss.phase != 254U || state.boss.phase_frame != 0 ||
			state.score.score_delta != 65000U || !state.bullets.zap.active() ||
			state.player_invincibility_time != 255U
		) return false;
		state.boss.damage_this_frame = 1U;
		stage6_fights_render_tail(state, trace);
		if(state.boss.damage_this_frame != 1U) return false;
		state.boss.phase = 253U;
		stage6_fights_render_tail(state, trace);
		if(state.boss.damage_this_frame != 0U) return false;
	}
	return true;
}

} // namespace

int main()
{
	if(!shinki_setup_phase23_and_live_hittest()) {
		std::cerr << "TH05 Stage 6 setup/Phase 2 test failure\n";
		return EXIT_FAILURE;
	}
	if(!shinki_wings_timeout_and_devil()) {
		std::cerr << "TH05 Shinki wings/Devil test failure\n";
		return EXIT_FAILURE;
	}
	if(!shinki_phase12_defeat_and_render_tail()) {
		std::cerr << "TH05 Shinki Phase 12 test failure\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
