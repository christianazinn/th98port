#include "th01/core/boss_elis.hpp"
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
	state.frame_rand = 0x3456U;
	elis_setup(state);
	return state;
}

void setup_and_phase_one_tests()
{
	using namespace th01::core;
	auto state = fixture();
	check(state.boss.active && state.boss.id == BossId::elis && state.boss.hp == 14 &&
		state.boss.phase == 1 && state.boss.elis.girl.left == 256 &&
		state.boss.elis.girl.top == 96 && state.random_seed == state.frame_rand &&
		state.projectiles.pellet_interlace && state.projectiles.pellet_unknown_seven == 7,
		"Elis setup retains the source combat seam and resident pellet settings");
	state.boss.phase_frame = 49;
	static_cast<void>(elis_update(state));
	check(state.boss.elis.pattern_value == 55,
		"Elis phase-one lasers select the Normal source speed on frame 50");
	state.boss.phase_frame = 69;
	static_cast<void>(elis_update(state));
	check(state.projectiles.shootout_lasers[7].alive &&
		(state.projectiles.shootout_lasers[7].target_left == 0 ||
			state.projectiles.shootout_lasers[7].target_left == 640),
		"Elis fires the first source laser toward the selected playfield edge on frame 70");
}

void late_patterns_and_defeat_tests()
{
	using namespace th01::core;
	auto bat_state = fixture();
	bat_state.boss.phase = 5;
	bat_state.boss.phase_frame = 15;
	bat_state.boss.elis.form = 1U;
	bat_state.boss.elis.phase_5_subphase = 0;
	bat_state.boss.elis.phase_5_bat_pattern = 1;
	bat_state.boss.elis.bat = {300, 160};
	static_cast<void>(elis_update(bat_state));
	check(bat_state.projectiles.pellet_alive_count == 5,
		"Elis bat phase retains its Normal five-wide aimed spread timing");

	auto girl_state = fixture();
	girl_state.boss.phase = 5;
	girl_state.boss.phase_frame = 49;
	girl_state.boss.elis.form = 0U;
	girl_state.boss.elis.phase_5_subphase = 0;
	girl_state.boss.elis.phase_5_girl_pattern = 1;
	girl_state.boss.elis.pattern_subphase = 1U;
	static_cast<void>(elis_update(girl_state));
	check(girl_state.projectiles.pellet_alive_count == 8,
		"Elis final girl stack pattern emits four symmetric Normal-speed pairs on frame 50");

	girl_state.boss.hp = 0;
	check(elis_update(girl_state) == BossUpdateResult::stage_cleared &&
		girl_state.stage_cleared && girl_state.input.player_is_hit && !girl_state.boss.active &&
		girl_state.projectiles.pellet_alive_count == 0,
		"Elis final defeat resets pellets, missiles, and all ten shootout lasers");
}

} // namespace

int main()
{
	setup_and_phase_one_tests();
	late_patterns_and_defeat_tests();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
