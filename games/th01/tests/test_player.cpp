#include "th01/core/state.hpp"
#include "pc98/random.hpp"

#include <cstdlib>
#include <iostream>

namespace {
int failures{};
void check(bool value, const char *what)
{
	if(!value) { std::cerr << what << '\n'; ++failures; }
}
}

int main()
{
	using namespace th01::core;
	State s{};
	player_reset(s);
	s.input.shot = true;
	player_update(s);
	check(s.player.mode == 1 && !s.shots.slots[0].moving,
		"Standing shot arms on the input frame; spawning is next update");
	player_update(s);
	check(s.shots.slots[0].moving && s.shots.slots[0].left == 312 &&
		s.shots.slots[0].top == 368, "Standing shot uses the source center and top");
	player_update(s);
	player_update(s);
	check(!s.input.shot && s.player.mode == 0 && s.bomb_frame == 0,
		"Shot end clears its latch and resets bomb frame");

	s = {};
	player_reset(s);
	s.input.strike = true;
	player_update(s);
	check(s.player.deflecting && s.player.invincible_against_orb && s.player.mode == 2,
		"Standing strike starts deflection and Orb immunity");
	for(int i = 0; i < 16; ++i) player_update(s);
	check(!s.player.deflecting && s.player.invincible_against_orb,
		"Easy swing loses projectile deflection before Orb immunity ends");
	for(int i = 0; i < 8; ++i) player_update(s);
	check(s.player.mode == 0 && !s.player.invincible_against_orb,
		"Swing finishes at the source frame boundary");

	s = {};
	player_reset(s);
	s.input.bomb = true;
	player_update(s);
	check(s.rem_bombs == 1 && s.player.bomb_flag == 2 &&
		s.projectiles.pellet_speed_x40 == 1, "Bomb arming raises speed by one fortieth");
	player_update(s);
	check(s.rem_bombs == 0 && s.player.bombing && s.player.invincible,
		"Bomb consumption happens after the arming update");
	s.bomb_frame = 50;
	player_update(s);
	check(s.bomb_damaging, "Bomb damage starts at 50");
	s.bomb_frame = 140;
	player_update(s);
	check(!s.bomb_damaging && !s.player.bombing && s.player.invincibility_time == 100,
		"Bomb finishes at 140 with its immunity tail");

	s = {};
	s.orb.horizontal.left = s.player.left;
	s.orb.top = 368;
	auto seed = s.random_seed;
	(void)pc98::irand(seed);
	(void)pc98::irand(seed);
	orb_player_hittest(s, 1);
	check(s.random_seed == seed && s.orb.vertical.force == -13.0,
		"Centered repel consumes both independent random checks");
	s.player.invincible = true;
	s.player.invincibility_sprite_frames[0] = 255;
	seed = s.random_seed;
	for(int i = 0; i < 24; ++i) (void)pc98::irand(seed);
	player_invincibility_particles_update(s);
	check(s.random_seed == seed, "Eight invincibility sprites retain three random draws each");
	s.player.invincible = false;
	player_invincibility_particles_update(s);
	check(s.player.invincibility_sprite_frames[0] == 255 && s.random_seed == seed,
		"Disabled invincibility sprites only write their first sentinel");
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
