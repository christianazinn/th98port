#include "th01/core/boss.hpp"
#include "th01/core/projectiles.hpp"
#include "th01/core/state.hpp"

#include "pc98/random.hpp"

#include <cstdlib>
#include <iostream>

namespace {

using namespace th01::core;

int failures{};

void check(bool condition, const char *message)
{
	if(!condition) {
		std::cerr << message << '\n';
		++failures;
	}
}

[[nodiscard]] State fixture()
{
	State state{};
	state.rank = 1;
	state.frame_rand = 0x13579BDFU;
	state.orb.horizontal.left = 600;
	state.orb.top = 280;
	state.orb.prev_left = 600;
	state.orb.prev_top = 280;
	shootout_lasers_init(state);
	sariel_setup(state);
	return state;
}

[[nodiscard]] std::size_t moving_pellets(const State &state)
{
	std::size_t count{};
	for(const auto &pellet : state.projectiles.pellets) {
		if(pellet.moving) {
			++count;
		}
	}
	return count;
}

void setup_and_entrance_tests()
{
	auto state = fixture();
	check(state.boss.active && state.boss.id == BossId::sariel &&
		state.boss.hp == 18 && state.boss.phase == 0 &&
		state.projectiles.pellet_interlace,
		"Sariel setup retains b20m HP, phase, active, and interlace state");

	auto expected_seed = state.frame_rand;
	const auto expected_patterns = static_cast<std::int16_t>(
		pc98::irand(expected_seed) % 6U
	) + 1;
	check(sariel_update(state) == BossUpdateResult::running &&
		state.boss.phase == 1 && state.boss.phase_frame == 0 &&
		state.boss.sariel.pattern_current == 0 &&
		state.boss.sariel.patterns_until_next == expected_patterns &&
		state.random_seed == expected_seed,
		"Collapsed Sariel entrance preserves its phase-one IRAND setup boundary");
}

void pattern_and_transition_tests()
{
	auto state = fixture();
	state.boss.phase = 1;
	state.boss.sariel.pattern_current = 0;
	state.boss.sariel.patterns_until_next = 99;
	state.boss.phase_frame = 49;
	auto expected_seed = state.random_seed;
	for(int index = 0; index < 20; ++index) {
		static_cast<void>(pc98::irand(expected_seed));
	}
	static_cast<void>(sariel_update(state));
	check(state.boss.phase_frame == 50 && state.random_seed == expected_seed &&
		state.boss.sariel.purple_laser_speed == 72,
		"Purple-laser setup consumes ten source X/Y pairs at Normal speed");

	state.boss.phase_frame = 58;
	expected_seed = state.random_seed;
	static_cast<void>(pc98::irand(expected_seed));
	static_cast<void>(sariel_update(state));
	check(state.projectiles.shootout_lasers[0].alive &&
		state.random_seed == expected_seed,
		"Purple-laser frame 59 consumes only its destination IRAND before spawning");

	state = fixture();
	state.boss.phase = 2;
	state.boss.phase_frame = 49;
	expected_seed = state.random_seed;
	for(int index = 0; index < 100; ++index) {
		static_cast<void>(pc98::irand(expected_seed));
	}
	static_cast<void>(sariel_update(state));
	check(state.boss.phase_frame == 50 && state.random_seed == expected_seed,
		"Each b20m background-transition gust frame retains all 100 random cell draws");

	state = fixture();
	state.boss.phase = 3;
	state.boss.sariel.pattern_current = 0;
	state.boss.sariel.patterns_until_next = 99;
	for(int frame = 0; frame < 12; ++frame) {
		static_cast<void>(sariel_update(state));
	}
	check(state.projectiles.pellet_alive_count == 4 &&
		state.projectiles.pellets[0].cloud_frame == 1 &&
		state.boss.sariel.rings_angle == 12U,
		"Sariel's two A2 rings emit four cloud pellets and advance by the source interval");
}

void defeat_tests()
{
	auto state = fixture();
	pellets_add_single(state, 100, 100, 0U, 48);
	state.boss.phase = 99;
	static_cast<void>(sariel_update(state));
	check(state.boss.phase == 100 && state.boss.hp == 6 &&
		state.boss.phase_frame == 0 && !state.player.invincible &&
		moving_pellets(state) == 0U,
		"Form-one defeat resets source gameplay state before Form Two without clearing the game");

	state.boss.hp = 0;
	check(sariel_update(state) == BossUpdateResult::running && state.boss.phase == 101,
		"Form-Two hit processing enters b20m's final presentation phase first");
	check(sariel_update(state) == BossUpdateResult::stage_cleared &&
		state.game_cleared && !state.stage_cleared && !state.boss.active &&
		state.score == 50000U,
		"Sariel final defeat grants five source score additions and only sets game_cleared");
}

} // namespace

int main()
{
	setup_and_entrance_tests();
	pattern_and_transition_tests();
	defeat_tests();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
