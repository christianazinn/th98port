#include "th01/core/boss_yuugenmagan.hpp"
#include "th01/core/projectiles.hpp"
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

[[nodiscard]] th01::core::State fixture()
{
	using namespace th01::core;
	State state{};
	state.rank = 1;
	yuugenmagan_setup(state);
	return state;
}

void setup_and_opening_tests()
{
	using namespace th01::core;
	auto state = fixture();
	check(state.boss.active && state.boss.id == BossId::yuugenmagan &&
		state.boss.hp == 16 && state.boss.phase == 0 &&
		state.boss.yuugenmagan.eyes[0].left == 64 &&
		state.boss.yuugenmagan.eyes[4].top == 64,
		"YuugenMagan setup retains all five b10m eye origins");
	for(int frame = 0; frame < 100; ++frame) static_cast<void>(yuugenmagan_update(state));
	check(state.boss.phase_frame == 100 &&
		state.boss.yuugenmagan.eyes[0].orb_hitbox_active &&
		state.boss.yuugenmagan.eyes[1].orb_hitbox_active,
		"YuugenMagan opens and activates the lateral orb hitboxes on frame 100");

	state.rank = 2;
	for(int frame = 100; frame < 240; ++frame) static_cast<void>(yuugenmagan_update(state));
	check(state.projectiles.pellet_alive_count == 15,
		"The phase-zero Hard-only three-eye spread retains its unreachable Lunatic branch");
}

void hit_and_late_phase_tests()
{
	using namespace th01::core;
	auto state = fixture();
	auto &yuugen = state.boss.yuugenmagan;
	state.boss.phase = 1;
	state.boss.phase_frame = 0;
	yuugen.eyes[0].image = 3U;
	yuugen.eyes[0].orb_hitbox_active = true;
	state.orb.prev_left = 72;
	state.orb.prev_top = 116;
	state.orb.horizontal.velocity = OrbVelocityX::left8;
	static_cast<void>(yuugenmagan_update(state));
	check(state.boss.hp == 15 && yuugen.invincible && state.score == 5000 &&
		state.orb.horizontal.velocity == OrbVelocityX::right4,
		"YuugenMagan uses its eye hitbox, source score, and Orb reflection");

	auto late = fixture();
	late.boss.phase = 13;
	late.boss.phase_frame = 99;
	late.boss.yuugenmagan.pentagram.phase = 0U;
	late.boss.yuugenmagan.pentagram.center_left = 320;
	late.boss.yuugenmagan.pentagram.center_top = 189;
	late.boss.yuugenmagan.pentagram.radius = 64;
	static_cast<void>(yuugenmagan_update(late));
	check(late.boss.yuugenmagan.pentagram.phase == 1U && late.boss.phase_frame == 0,
		"YuugenMagan final phase enters its first spinning pentagram continuation at frame 100");

	late.boss.phase = 13;
	late.boss.phase_frame = 3;
	late.boss.yuugenmagan.pentagram.phase = 6U;
	late.boss.yuugenmagan.pentagram.center_top = 420;
	static_cast<void>(yuugenmagan_update(late));
	check(late.boss.phase == 10 && late.boss.yuugenmagan.pentagram.phase == 7U,
		"YuugenMagan first slam loops through phase 10 into the source prepare-two pentagram state");
}

void defeat_test()
{
	using namespace th01::core;
	auto state = fixture();
	state.boss.phase = 13;
	state.boss.hp = 0;
	state.boss.yuugenmagan.pentagram.phase = 0U;
	pellets_add_single(state, 100, 100, 0, 50);
	check(yuugenmagan_update(state) == BossUpdateResult::stage_cleared &&
		state.stage_cleared && state.input.player_is_hit && !state.boss.active &&
		state.projectiles.pellet_alive_count == 0,
		"YuugenMagan final defeat clears its source projectile pools at the stage boundary");
}

} // namespace

int main()
{
	setup_and_opening_tests();
	hit_and_late_phase_tests();
	defeat_test();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
