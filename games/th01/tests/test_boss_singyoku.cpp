#include "th01/core/boss.hpp"
#include "th01/core/state.hpp"

#include <cstdlib>
#include <iostream>

namespace {

int failures{};

void check(bool value, const char *message)
{
	if(!value) {
		++failures;
		std::cerr << message << '\n';
	}
}

[[nodiscard]] th01::core::State singyoku_fixture()
{
	using namespace th01::core;
	State state{};
	state.rank = 1;
	state.boss.id = BossId::singyoku;
	boss_setup(state);
	return state;
}

void enter_combat(th01::core::State &state)
{
	using namespace th01::core;
	check(boss_update(state) == BossUpdateResult::running,
		"SinGyoku collapses its pure blocking entrance as a running boss update");
}

void setup_and_entrance_tests()
{
	using namespace th01::core;
	auto state = singyoku_fixture();
	check(state.boss.active && state.boss.hp == 8 && state.boss.phase == 0 &&
		state.boss.singyoku.left == 640 && state.boss.singyoku.top == 64 &&
		state.boss.singyoku.sphere_image == 0,
		"SinGyoku setup retains the source loader state");
	enter_combat(state);
	check(state.boss.phase == 1 && state.boss.phase_frame == 0 &&
		state.boss.singyoku.left == 272 && state.boss.singyoku.top == 107 &&
		state.boss.singyoku.sphere_image == 7 &&
		state.boss.singyoku.pattern_value == 50,
		"Collapsed entrance retains all gameplay-visible source endpoint state");

	for(int frame = 0; frame < 100; ++frame) {
		check(boss_update(state) == BossUpdateResult::running,
			"SinGyoku half-circle remains a running combat update");
	}
	check(state.boss.phase_frame == 100 &&
		state.projectiles.pellet_alive_count == 1 &&
		state.projectiles.pellets[0].moving &&
		state.projectiles.pellets[0].velocity_x == 56 &&
		state.projectiles.pellets[0].velocity_y == 0,
		"Normal half-circle begins on frame 100 with source rank-adjusted Q12.4 vector");
	for(int frame = 100; frame < 160; ++frame) {
		static_cast<void>(boss_update(state));
	}
	check(state.boss.phase_frame == 0 && state.boss.singyoku.pattern_cur == 1 &&
		state.projectiles.pellet_alive_count == 15,
		"Half-circle fires fifteen normal pellets then yields to the slam");
}

void hit_and_phase_tests()
{
	using namespace th01::core;
	auto shot_state = singyoku_fixture();
	enter_combat(shot_state);
	shot_state.shots.slots[0] = {288, 139, 0, true, 0};
	static_cast<void>(boss_update(shot_state));
	check(shot_state.boss.hp == 8 && !shot_state.shots.slots[0].moving &&
		shot_state.shots.slots[0].decay_frame == 1,
		"SinGyoku shot hitbox consumes shots without reducing boss HP");

	auto state = singyoku_fixture();
	enter_combat(state);
	state.orb.prev_left = 272;
	state.orb.prev_top = 107;
	state.orb.horizontal.velocity = OrbVelocityX::left8;
	static_cast<void>(boss_update(state));
	check(state.boss.hp == 7 && state.boss.singyoku.invincible &&
		state.boss.singyoku.invincibility_frame == 0 &&
		state.orb.horizontal.velocity == OrbVelocityX::right4 && state.score == 3000,
		"Orb collision uses the source hitbox, score, and 8-to-4 reflection");
	state.orb.prev_left = 0;
	state.orb.prev_top = 0;
	for(int frame = 0; frame < 40; ++frame) {
		static_cast<void>(boss_update(state));
	}
	check(state.boss.singyoku.invincible &&
		state.boss.singyoku.invincibility_frame == 40,
		"Boss invincibility remains set exactly through frame forty");
	static_cast<void>(boss_update(state));
	check(!state.boss.singyoku.invincible &&
		state.boss.singyoku.invincibility_frame == 0,
		"Boss hit helper retains the source frame-forty-one reset");

	auto transition = singyoku_fixture();
	enter_combat(transition);
	transition.boss.hp = 6;
	static_cast<void>(boss_update(transition));
	check(transition.boss.phase == 2 && transition.boss.phase_frame == 0 &&
		transition.boss.singyoku.pattern_cur == 0,
		"Phase one only enters phase two after a non-slam hit update");
}

void defeat_and_route_tests()
{
	using namespace th01::core;
	auto state = singyoku_fixture();
	enter_combat(state);
	state.boss.phase = 2;
	state.boss.phase_frame = 0;
	state.boss.singyoku.pattern_cur = 0;
	state.boss.hp = 0;
	pellets_add_single(state, 100, 100, 0, 50);
	check(boss_update(state) == BossUpdateResult::running && state.boss.phase == 8,
		"Phase-two defeat enters phase eight before its route modal");
	check(boss_update(state) == BossUpdateResult::route_selection &&
		state.projectiles.pellet_alive_count == 0,
		"Phase eight clears non-cloud pellets before route selection");
	check(singyoku_route_select(state, BossRoute::jigoku) ==
		BossUpdateResult::stage_cleared && state.route == 1 && state.stage_cleared &&
		state.input.player_is_hit && !state.boss.active,
		"Jigoku route selection writes stored route_t and source stage boundary");
	check(singyoku_route_select(state, BossRoute::makai) == BossUpdateResult::unsupported,
		"Route selection cannot be submitted twice after its source boundary");
}

} // namespace

int main()
{
	setup_and_entrance_tests();
	hit_and_phase_tests();
	defeat_and_route_tests();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
