#include "th01/core/boss.hpp"
#include "th01/core/boss_kikuri.hpp"
#include "th01/core/projectiles.hpp"
#include "th01/core/state.hpp"

#include <cstdlib>
#include <iostream>

namespace {

int failures{};

void check(bool condition, const char *message)
{
	if(!condition) {
		++failures;
		std::cerr << message << '\n';
	}
}

[[nodiscard]] th01::core::State kikuri_fixture()
{
	using namespace th01::core;
	State state{};
	state.rank = 1;
	state.boss.id = BossId::kikuri;
	shootout_lasers_init(state);
	kikuri_setup(state);
	return state;
}

void enter_combat(th01::core::State &state)
{
	using namespace th01::core;
	check(kikuri_update(state) == BossUpdateResult::running,
		"Kikuri collapses both presentation-only entrance loops into phase two");
}

void setup_spiral_and_souls_tests()
{
	using namespace th01::core;
	auto state = kikuri_fixture();
	check(state.boss.active && state.boss.hp == 14 && state.boss.phase == 0 &&
		state.projectiles.pellet_interlace && state.projectiles.pellet_unknown_seven == 7,
		"Kikuri loader setup installs the source pellet interlace state");
	enter_combat(state);
	check(state.boss.phase == 2 && state.boss.phase_frame == 0 &&
		!state.boss.kikuri.hit_invincible,
		"Kikuri entrance endpoint restores a non-invincible phase-two boss");
	for(int frame = 0; frame < 102; ++frame) {
		static_cast<void>(kikuri_update(state));
	}
	check(state.boss.kikuri.pattern_state == 3 &&
		state.boss.kikuri.phase_2_distance == 90 &&
		state.boss.kikuri.phase_2_angle == 0xF8U &&
		state.boss.kikuri.phase_2_drift == 1U &&
		state.projectiles.pellet_alive_count == 2,
		"Kikuri phase two initializes at frame 100 then starts both spiral sources on its Normal interval");

	state.boss.hp = 10;
	static_cast<void>(kikuri_update(state));
	check(state.boss.phase == 3 && state.boss.phase_frame == 0 &&
		state.boss.kikuri.patterns_done == 0,
		"Kikuri phase two checks its HP endpoint only after source hit handling");
	for(int frame = 0; frame < 100; ++frame) {
		static_cast<void>(kikuri_update(state));
	}
	check(state.boss.phase == 4 && state.boss.phase_frame == 0 &&
		state.boss.kikuri.phase_u1 == 0,
		"Kikuri retains its 100-frame phase-three palette boundary before souls activate");
	state.boss.phase_frame = 199;
	static_cast<void>(kikuri_update(state));
	check(state.boss.kikuri.souls[0].left == 304 && state.boss.kikuri.souls[0].top == 208 &&
		state.boss.kikuri.souls[1].left == 304 && state.boss.kikuri.souls[1].top == 208,
		"Kikuri creates both souls at the lightball only on activation frame 200");
}

void tears_phase_six_and_defeat_tests()
{
	using namespace th01::core;
	auto ripples = kikuri_fixture();
	enter_combat(ripples);
	ripples.boss.phase = 5;
	ripples.boss.phase_frame = 0;
	ripples.boss.kikuri.souls[0].left = 100;
	ripples.boss.kikuri.souls[0].top = 100;
	ripples.boss.kikuri.tear_animation_frame[0] = 1;
	ripples.boss.kikuri.tears[0].left = 100;
	ripples.boss.kikuri.tears[0].top = 392;
	ripples.player.left = 84;
	static_cast<void>(kikuri_update(ripples));
	check(ripples.input.player_is_hit && ripples.boss.kikuri.tear_animation_frame[0] == 2,
		"Kikuri ripple collision retains the source's unaligned first-band left edge");

	auto phase_six = kikuri_fixture();
	enter_combat(phase_six);
	phase_six.boss.phase = 6;
	phase_six.boss.phase_frame = 99;
	phase_six.boss.kikuri.phase_u1 = 0;
	for(int frame = 0; frame < 5; ++frame) {
		static_cast<void>(kikuri_update(phase_six));
	}
	check(phase_six.boss.kikuri.pattern_state == 8 &&
		phase_six.projectiles.pellet_alive_count == 8,
		"Kikuri phase six initializes at frame 100 then creates its duplicate two-source four-way spiral");

	phase_six.boss.kikuri.phase_u1 = 1;
	phase_six.boss.phase_frame = 249;
	static_cast<void>(kikuri_update(phase_six));
	check(phase_six.projectiles.shootout_lasers[0].alive &&
		phase_six.boss.kikuri.pattern_state == 56,
		"Kikuri's left eye stores Normal laser speed then fires the first randomized slot");

	auto rain = kikuri_fixture();
	enter_combat(rain);
	rain.boss.phase = 6;
	rain.boss.phase_frame = 111;
	rain.boss.kikuri.phase_u1 = 2;
	rain.boss.kikuri.pattern_state = 56;
	rain.boss.kikuri.souls[0].left = 100;
	rain.boss.kikuri.souls[0].top = 100;
	rain.boss.kikuri.souls[1].left = 500;
	rain.boss.kikuri.souls[1].top = 100;
	static_cast<void>(kikuri_update(rain));
	check(rain.projectiles.pellet_alive_count == 12,
		"Kikuri rain reinterprets the preceding laser-speed word as its interval");

	auto vertical = kikuri_fixture();
	enter_combat(vertical);
	vertical.boss.phase = 6;
	vertical.boss.phase_frame = 99;
	vertical.boss.kikuri.phase_u1 = 3;
	static_cast<void>(kikuri_update(vertical));
	check(vertical.projectiles.shootout_lasers[0].alive &&
		vertical.projectiles.shootout_lasers[0].target_left ==
			vertical.projectiles.shootout_lasers[0].origin_left / 256,
		"Kikuri vertical laser zero range preserves its exact same-X endpoint");

	auto defeat = kikuri_fixture();
	enter_combat(defeat);
	defeat.boss.phase = 6;
	defeat.boss.hp = 0;
	defeat.boss.kikuri.phase_u1 = 0;
	pellets_add_single(defeat, 100, 100, 0U, 32);
	shootout_laser_spawn(defeat, 0U, 100, 100, 100, 300, 50, 10U, 20, 4);
	shootout_laser_spawn(defeat, 4U, 120, 100, 120, 300, 50, 10U, 20, 4);
	check(kikuri_update(defeat) == BossUpdateResult::stage_cleared &&
		defeat.stage_cleared && defeat.input.player_is_hit &&
		defeat.projectiles.pellet_alive_count == 0 &&
		!defeat.projectiles.shootout_lasers[0].alive &&
		defeat.projectiles.shootout_lasers[4].alive,
		"Kikuri defeat keeps b15j's broken first-four-laser reset bound");
}

} // namespace

int main()
{
	setup_spiral_and_souls_tests();
	tears_phase_six_and_defeat_tests();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
