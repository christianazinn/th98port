#include "th01/core/projectiles.hpp"
#include "th01/core/state.hpp"

#include <cstdlib>
#include <iostream>

namespace {

using namespace th01::core;

int failures{};

void check(bool value, const char *message)
{
	if(!value) {
		++failures;
		std::cerr << message << '\n';
	}
}

void put_orb_outside_pellet_tests(State &state)
{
	state.orb.horizontal.left = 0;
	state.orb.top = 64;
	state.orb.in_portal = true;
}

void pellet_allocation_speed_and_cloud_lifetime()
{
	State state;
	put_orb_outside_pellet_tests(state);
	pellets_add_group(
		state, 100, 200, PelletGroup::one, pellet_pixels_to_subpixels(0)
	);
	const auto &pellet = state.projectiles.pellets[0];
	check(
		pellet.moving && pellet.velocity_x == 0 &&
		pellet.velocity_y == pellet_pixels_to_subpixels(1) &&
		state.projectiles.pellet_alive_count == 1,
		"Pellet group uses the first free slot and source minimum speed"
	);
	pellet_speed_raise(state, pellet_speed_from_fortieths(30));
	check(state.projectiles.pellet_speed_x40 == 20, "Pellet speed raise clamps at +0.5");
	pellet_speed_lower(state, pellet_speed_from_fortieths(-2), pellet_speed_from_fortieths(-5));
	check(state.projectiles.pellet_speed_x40 == -2, "Pellet speed lower first applies its maximum");
	pellet_speed_lower(state, pellet_speed_from_fortieths(-2), pellet_speed_from_fortieths(-5));
	check(state.projectiles.pellet_speed_x40 == -7, "Pellet speed lower then adds its negative delta");

	State cloud_state;
	put_orb_outside_pellet_tests(cloud_state);
	cloud_state.projectiles.pellet_spawn_with_cloud = true;
	pellets_add_single(
		cloud_state, 100, 200, 0U, pellet_pixels_to_subpixels(2)
	);
	for(int frame = 0; frame < 7; ++frame) {
		check(
			pellets_update_and_shots(cloud_state) == ShotOrbUpdateStatus::complete,
			"Cloud-only pellet keeps the source shot/Orb tail available"
		);
	}
	check(
		!cloud_state.projectiles.pellets[0].moving &&
		cloud_state.projectiles.pellets[0].cloud_frame == 8,
		"Pellet cloud retains its inactive slot through frame 8"
	);
	check(
		pellets_update_and_shots(cloud_state) == ShotOrbUpdateStatus::complete &&
		cloud_state.projectiles.pellets[0].moving &&
		cloud_state.projectiles.pellets[0].cloud_frame == 0,
		"Cloud frame 9 activates the pellet in that update's movement pass"
	);
}

void pellet_shot_decay_and_deflection()
{
	State state;
	put_orb_outside_pellet_tests(state);
	pellets_add_group(
		state, 100, 100, PelletGroup::one, pellet_pixels_to_subpixels(0)
	);
	state.shots.slots[0] = {100, 101, 0x50, true, 0};
	check(
		pellets_update_and_shots(state) == ShotOrbUpdateStatus::complete &&
		!state.projectiles.pellets[0].moving &&
		state.projectiles.pellet_alive_count == 0 &&
		state.shots.slots[0].decay_frame == 2,
		"Pellet collision precedes the source shot update/decay tail"
	);

	State decay_state;
	put_orb_outside_pellet_tests(decay_state);
	pellets_add_group(
		decay_state, 100, 200, PelletGroup::one, pellet_pixels_to_subpixels(0)
	);
	pellets_decay(decay_state);
	for(int frame = 0; frame < 21; ++frame) {
		check(
			pellets_update_and_shots(decay_state) == ShotOrbUpdateStatus::complete,
			"Pellet decay keeps the source shot/Orb tail available"
		);
	}
	check(
		!decay_state.projectiles.pellets[0].moving &&
		decay_state.projectiles.pellets[0].decay_frame == 0 &&
		decay_state.projectiles.pellet_alive_count == 0 &&
		decay_state.projectiles.pellet_destroy_score_delta == 10U,
		"Pellet decay lasts source frames and accrues deferred score"
	);
	pellet_destroy_score_delta_commit(decay_state);
	check(
		decay_state.score == 10U &&
		decay_state.projectiles.pellet_destroy_score_delta == 0U,
		"Pellet destruction commits its separate score delta only at the caller"
	);

	State deflect_state;
	put_orb_outside_pellet_tests(deflect_state);
	deflect_state.player.left = 304;
	deflect_state.player.deflecting = true;
	auto &deflected = deflect_state.projectiles.pellets[0];
	deflected.moving = true;
	deflected.cur_left = pellet_pixels_to_subpixels(305);
	deflected.cur_top = pellet_pixels_to_subpixels(356);
	deflected.from_group = PelletGroup::none;
	check(
		pellets_update_and_shots(deflect_state) == ShotOrbUpdateStatus::complete &&
		deflected.velocity_x == -pellet_pixels_to_subpixels(8) &&
		deflected.velocity_y == 0 && deflected.not_rendered &&
		!deflect_state.input.player_is_hit,
		"Deflection changes velocity and suppresses that frame's pellet draw"
	);
}

void missile_laser_and_line_hittest()
{
	State missile_state;
	missile_state.player.left = 304;
	missiles_add(missile_state, 310, 380, 0, 0);
	missiles_update(missile_state);
	check(missile_state.input.player_is_hit, "Moving missile uses the source player box");

	State laser_state;
	laser_state.player.left = 304;
	shootout_lasers_init(laser_state);
	shootout_laser_spawn(
		laser_state, 0U, 310, 350, 310, 399, 8, 15U, 50, 1
	);
	for(int frame = 0; frame < 40; ++frame) {
		shootout_lasers_update(laser_state);
	}
	check(
		laser_state.projectiles.shootout_lasers[0].id == 0 &&
		laser_state.input.player_is_hit,
		"Shootout laser grows in Q24.8 and hit-tests its byte-boundary segments"
	);

	State line_state;
	line_state.player.left = 304;
	linebullet_put_patterned_and_hittest(
		line_state, 0, 64, 310, 385, 15U, 0xAAAAU
	);
	check(line_state.input.player_is_hit, "Line bullet tests only its right/bottom endpoint");
}

} // namespace

int main()
{
	pellet_allocation_speed_and_cloud_lifetime();
	pellet_shot_decay_and_deflection();
	missile_laser_and_line_hittest();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
