#include "th05/core/boss.hpp"
#include "th05/core/state.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

using namespace th05::core;

struct Trace final : Presentation {
	void boss_hp(std::int16_t, std::int16_t) noexcept override {}
};

using FightsSetup = void (*)(State &) noexcept;
using MidbossActivate = void (*)(State &) noexcept;
using FightsUpdate = bool (*)(State &, Presentation &) noexcept;

[[nodiscard]] bool check_lifecycle(
	FightsSetup setup, MidbossActivate activate, FightsUpdate update
) noexcept
{
	State state{};
	Trace trace{};
	state.midboss.phase = 3U;
	state.midboss.phase_frame = 99;
	state.midboss.damage_this_frame = 0x5AU;
	setup(state);
	if(
		state.midboss.phase != 3U || state.midboss.phase_frame != 99 ||
		state.midboss.damage_this_frame != 0x5AU || state.midboss_active ||
		state.midboss_callback_active
	) return false;

	state.stage_frame = static_cast<std::uint16_t>(state.midboss.frames_until - 1U);
	activate(state);
	if(state.midboss_active || state.midboss_callback_active) return false;
	state.stage_frame = state.midboss.frames_until;
	activate(state);
	if(
		!state.midboss_active || !state.midboss_callback_active ||
		state.midboss.phase != 0U || state.midboss.phase_frame != 0 ||
		state.midboss.damage_this_frame != 0x5AU
	) return false;

	state.midboss.phase = 254U;
	state.midboss.phase_frame = 0;
	state.midboss.sprite = 4U;
	state.midboss.hp = 77;
	if(!update(state, trace) || state.midboss_active || !state.midboss_callback_active ||
		state.playfield_shake_anim_time != 10 || state.midboss.phase_frame != 1 ||
		state.midboss.sprite != 4U) return false;
	for(std::uint16_t phase_frame = 2U; phase_frame <= 128U; ++phase_frame) {
		if(!update(state, trace)) return false;
	}
	if(
		state.midboss_active || !state.midboss_callback_active ||
		state.midboss.phase != 255U || state.midboss.phase_frame != 128 ||
		state.midboss.sprite != 12U || state.midboss.hp != 77
	) return false;
	if(!update(state, trace)) return false;
	return !state.midboss_active && !state.midboss_callback_active &&
		state.midboss.hp == 0 && state.midboss.phase == 255U &&
		state.midboss.phase_frame == 129;
}

} // namespace

int main()
{
	if(!check_lifecycle(stage1_fights_setup, stage1_midboss_activate, stage1_fights_update) ||
		!check_lifecycle(stage2_fights_setup, stage2_midboss_activate, stage2_fights_update) ||
		!check_lifecycle(stage3_fights_setup, stage3_midboss_activate, stage3_fights_update) ||
		!check_lifecycle(stage4_fights_setup, stage4_midboss_activate, stage4_fights_update) ||
		!check_lifecycle(stage5_fights_setup, stage5_midboss_activate, stage5_fights_update)) {
		std::cerr << "TH05 midboss callback-lifetime test failure\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
