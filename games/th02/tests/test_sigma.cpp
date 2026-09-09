#include "th02/core/sigma.hpp"
#include "th02/core/stage_state.hpp"

#include <cstdio>

namespace {

class Trace final : public th02::core::SigmaCallbacks {
public:
	void sound_effect(std::uint8_t) noexcept override { ++sounds; }
	void invalidate_scenery(th02::core::StageSceneryInvalidation) noexcept override {}
	void palette_set(std::uint8_t, th02::core::StageSceneryRgb) noexcept override {}
	void palette_tone_set(std::uint16_t) noexcept override {}
	void palette_show() noexcept override {}
	void grc_set_clip(std::int16_t, std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void egc_off() noexcept override { ++egc_offs; }
	void grcg_set_color(std::uint8_t) noexcept override { ++colors; }
	void dot_square(th02::core::ScreenPoint, std::uint8_t) noexcept override {}
	void grcg_off() noexcept override { ++grcg_offs; }
	void grcg_circle(th02::core::ScreenPoint, std::int16_t) noexcept override { ++circles; }
	void grcg_circle_fill(th02::core::ScreenPoint, std::int16_t) noexcept override { ++circle_fills; }
	void grcg_vline(std::int16_t, std::int16_t, std::int16_t) noexcept override { ++vlines; }
	void grcg_bytebox_fill(std::uint8_t, std::int16_t, std::uint8_t,
		std::int16_t) noexcept override { ++byteboxes; }
	void dot_square_ring(th02::core::ScreenPoint, std::uint8_t,
		std::uint8_t) noexcept override { ++rings; }
	void render_sigma(th02::core::SigmaSprite) noexcept override { ++sigmas; }
	void render_sigma_blast(th02::core::SigmaBlastSprite) noexcept override { ++blast_sprites; }

	int sounds{};
	int egc_offs{};
	int colors{};
	int grcg_offs{};
	int circles{};
	int circle_fills{};
	int vlines{};
	int byteboxes{};
	int rings{};
	int sigmas{};
	int blast_sprites{};
};

[[nodiscard]] bool reset_and_background() noexcept
{
	th02::core::StageState state{};
	Trace trace;
	state.frame.page_back = 0U;
	state.frame.page_front = 1U;
	state.sigma.frame = 77U;
	state.boss.topleft_on_page[1U] = {200, 120};
	state.sigma.blasts[0U].phase = static_cast<std::uint8_t>(
		th02::core::SigmaBlastPhase::activating);
	th02::core::sigma_combat_reset(state);
	if((state.tile_mode != 2U) ||
		(state.boss.topleft_on_page[0U] != th02::core::ScreenPoint{160, 48}) ||
		(state.boss.topleft_on_page[1U] != th02::core::ScreenPoint{160, 48}) ||
		(state.sigma.frame != 77U) || (state.boss.patnum != 128) ||
		(state.sigma.phase != 0U) || (state.sigma.cel_interval_mask != 7U)) return false;
	state.boss.topleft_on_page[1U] = {200, 120};
	const auto result = th02::core::sigma_bg_render(state, trace);
	return (result == th02::core::SigmaUpdateResult::boss) &&
		(state.boss.topleft_on_page[0U] == th02::core::ScreenPoint{200, 120}) &&
		(state.sigma.blasts[0U].phase == static_cast<std::uint8_t>(
			th02::core::SigmaBlastPhase::expanding)) &&
		(trace.egc_offs == 1) && (trace.colors == 1) && (trace.grcg_offs == 1);
}

[[nodiscard]] bool trap_spawn_and_hitbox_state() noexcept
{
	th02::core::StageState state{};
	Trace trace;
	state.frame.page_back = 0U;
	state.frame.page_front = 1U;
	state.boss.topleft_on_page.fill({160, 48});
	th02::core::sigma_combat_reset(state);
	state.sigma.phase = 1U;
	state.sigma.pattern = 1U;
	state.boss.phase_frame = 99;
	state.stage_frame = 1U;
	if(th02::core::sigma_update(state, trace) != th02::core::SigmaUpdateResult::boss) {
		return false;
	}
	const auto &blast = state.sigma.blasts[0U];
	return (state.sigma.cel_interval_mask == 7U) && (state.sigma.blast_hitbox_margin == 4) &&
		(blast.phase == static_cast<std::uint8_t>(th02::core::SigmaBlastPhase::telegraph)) &&
		(blast.center == th02::core::ScreenPoint{128, 320}) && (blast.radius_max == 64) &&
		(trace.sounds == 1) && (trace.sigmas == 2);
}

[[nodiscard]] bool final_defeat_returns_stage_clear() noexcept
{
	th02::core::StageState state{};
	Trace trace;
	state.frame.page_back = 0U;
	state.frame.page_front = 1U;
	state.boss.topleft_on_page.fill({160, 48});
	th02::core::sigma_combat_reset(state);
	state.boss.phase = 1U;
	state.boss.phase_frame = 223;
	state.stage_frame = 1U;
	return th02::core::sigma_update(state, trace) ==
		th02::core::SigmaUpdateResult::stage_clear;
}

} // namespace

int main()
{
	if(!reset_and_background()) { std::fputs("sigma test 1 failed\n", stderr); return 1; }
	if(!trap_spawn_and_hitbox_state()) { std::fputs("sigma test 2 failed\n", stderr); return 1; }
	if(!final_defeat_returns_stage_clear()) { std::fputs("sigma test 3 failed\n", stderr); return 1; }
	std::puts("3 sigma combat checks passed");
	return 0;
}
