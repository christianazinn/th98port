#include "th01/core/boss.hpp"
#include "th01/core/boss_mima.hpp"
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

[[nodiscard]] th01::core::State mima_fixture()
{
	using namespace th01::core;
	State state{};
	state.rank = 1;
	state.boss.id = BossId::mima;
	shootout_lasers_init(state);
	mima_setup(state);
	return state;
}

void enter_combat(th01::core::State &state)
{
	using namespace th01::core;
	check(mima_update(state) == BossUpdateResult::running,
		"Mima collapses the source blocking spread-in as a running update");
}

void setup_and_phase_one_pattern_tests()
{
	using namespace th01::core;
	auto state = mima_fixture();
	check(state.boss.active && state.boss.hp == 12 && state.boss.phase == 0 &&
		state.boss.mima.left == 256 && state.boss.mima.top == 64 &&
		state.boss.mima.meteor_active,
		"Mima setup retains the source loader position and BSS defaults");
	enter_combat(state);
	check(state.boss.phase == 1 && state.boss.phase_frame == 0 &&
		state.boss.mima.left == 256 && state.boss.mima.top == 120 &&
		state.boss.mima.hop == 0xFFU && state.boss.mima.spreadin_interval == 4U &&
		state.boss.mima.spreadin_speed == 16,
		"Mima entrance reaches the post-spread-in combat boundary and initializes hop state");

	for(int frame = 0; frame < 184; ++frame) {
		static_cast<void>(mima_update(state));
	}
	check(state.boss.phase_frame == 184 && state.boss.mima.pattern_state == 72 &&
		state.projectiles.pellet_alive_count == 4,
		"Mima's first square emits four Normal aimed pellets from its old corners");
	for(const auto &pellet : state.projectiles.pellets) {
		if(pellet.moving) {
			check(pellet.velocity_x != 0 || pellet.velocity_y != 0,
				"Mima aimed square pellets retain a concrete vector");
		}
	}
}

void missile_pillar_laser_and_defeat_tests()
{
	using namespace th01::core;
	auto missiles = mima_fixture();
	enter_combat(missiles);
	missiles.boss.mima.pattern_cur = 1;
	for(int frame = 0; frame < 248; ++frame) {
		static_cast<void>(mima_update(missiles));
	}
	int missile_count{};
	for(const auto &missile : missiles.projectiles.missiles) {
		if(missile.flag == MissileFlag::moving) ++missile_count;
	}
	check(missiles.boss.mima.target_left == missiles.player.left && missile_count == 4,
		"Mima snapshots player_left at frame 224 then fires four truncated-pixel missiles");

	auto pillars = mima_fixture();
	enter_combat(pillars);
	pillars.boss.phase = 3;
	pillars.boss.phase_frame = 99;
	pillars.boss.mima.pattern_cur = 0;
	static_cast<void>(mima_update(pillars));
	check(pillars.boss.mima.pillars[0].time == 128 &&
		pillars.boss.mima.pillars[0].bottom == 400,
		"Mima pillar setup stores all source circle timers at frame 100");
	for(int frame = 100; frame < 199; ++frame) {
		static_cast<void>(mima_update(pillars));
	}
	pillars.player.left = pillars.boss.mima.pillars[0].center_x;
	static_cast<void>(mima_update(pillars));
	check(pillars.input.player_is_hit,
		"Mima pillar collision uses the source's unaligned center-coordinate band");
	for(int frame = 201; frame <= 216; ++frame) {
		static_cast<void>(mima_update(pillars));
	}
	check(pillars.boss.mima.pillars[0].time == -1 &&
		pillars.projectiles.pellet_alive_count >= 3,
		"Mima replaces a completed pillar with its source three-way aimed spread");

	auto lasers = mima_fixture();
	enter_combat(lasers);
	lasers.boss.phase = 3;
	lasers.boss.phase_frame = 99;
	lasers.boss.mima.pattern_cur = 3;
	for(int frame = 0; frame < 181; ++frame) {
		static_cast<void>(mima_update(lasers));
	}
	check(lasers.projectiles.shootout_lasers[1].alive &&
		lasers.projectiles.shootout_lasers[1].width_cel == 3U,
		"Mima preserves its four stored laser-square corners into the firing frames");

	auto defeat = mima_fixture();
	enter_combat(defeat);
	defeat.boss.phase = 3;
	defeat.boss.mima.pattern_cur = 0;
	defeat.boss.hp = 0;
	pellets_add_single(defeat, 100, 100, 0U, 32);
	shootout_laser_spawn(defeat, 0U, 100, 100, 100, 300, 50, 15U, 20, 4);
	shootout_laser_spawn(defeat, 5U, 120, 100, 120, 300, 50, 15U, 20, 4);
	check(mima_update(defeat) == BossUpdateResult::stage_cleared &&
		defeat.stage_cleared && defeat.input.player_is_hit &&
		defeat.projectiles.pellet_alive_count == 0 &&
		!defeat.projectiles.shootout_lasers[0].alive &&
		defeat.projectiles.shootout_lasers[5].alive,
		"Mima defeat keeps b10j's broken first-five-laser reset bound");
}

} // namespace

int main()
{
	setup_and_phase_one_pattern_tests();
	missile_pillar_laser_and_defeat_tests();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
