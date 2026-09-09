#include "th02/core/mima.hpp"
#include "th02/core/stage_state.hpp"

#include <cstdio>

namespace {

class Trace final : public th02::core::MimaCallbacks {
public:
	void invalidate_scenery(th02::core::StageSceneryInvalidation) noexcept override { ++invalidations; }
	void palette_set(std::uint8_t, th02::core::StageSceneryRgb) noexcept override { ++palette_sets; }
	void palette_tone_set(std::uint16_t) noexcept override { ++palette_tones; }
	void palette_show() noexcept override { ++palette_shows; }
	void grc_set_clip(std::int16_t, std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void egc_off() noexcept override { ++egc_offs; }
	void grcg_set_color(std::uint8_t) noexcept override { ++colors; }
	void dot_square(th02::core::ScreenPoint, std::uint8_t) noexcept override { ++dots; }
	void grcg_off() noexcept override { ++grcg_offs; }
	void invalidate_bullet(th02::core::EnemyBulletInvalidation) noexcept override { ++bullet_invalidations; }
	void grcg_line(th02::core::ScreenPoint, th02::core::ScreenPoint) noexcept override { ++lines; }
	void grcg_circle(th02::core::ScreenPoint, std::int16_t) noexcept override { ++circles; }
	void grcg_circle_fill(th02::core::ScreenPoint, std::int16_t) noexcept override { ++circle_fills; }
	void dot_square_ring(th02::core::ScreenPoint, std::uint8_t, std::uint8_t) noexcept override { ++rings; }
	void render_mima(th02::core::MimaSprite) noexcept override { ++mima_sprites; }
	void render_mima_orb(th02::core::MimaOrbSprite) noexcept override { ++orb_sprites; }

	int invalidations{};
	int palette_sets{};
	int palette_tones{};
	int palette_shows{};
	int egc_offs{};
	int colors{};
	int dots{};
	int grcg_offs{};
	int bullet_invalidations{};
	int lines{};
	int circles{};
	int circle_fills{};
	int rings{};
	int mima_sprites{};
	int orb_sprites{};
};

[[nodiscard]] bool reset_and_background() noexcept
{
	th02::core::StageState state{};
	Trace trace;
	state.rank = 1;
	state.frame.page_back = 0U;
	state.frame.page_front = 1U;
	state.boss.topleft_on_page[1U] = {144, 64};
	th02::core::mima_combat_reset(state);
	if((state.mima.phase != 0) || (state.mima.bg_ring_col_head != 13U) ||
		(state.boss.rank_param[2U] != th02::core::enemy_bullet_group::ring_32)) return false;
	state.mima.orb_flag[0U] = 2;
	state.mima.orb_on_page[0U][0U] = {100, 80};
	state.mima.bg_ring_radius = 64U;
	const auto result = th02::core::mima_bg_render(state, trace);
	return (result == th02::core::MimaUpdateResult::boss) &&
		(state.mima.orb_flag[0U] == 0) &&
		(state.boss.topleft_on_page[0U] == th02::core::ScreenPoint{144, 64}) &&
		(trace.invalidations == 1) && (trace.circle_fills == 1) && (trace.dots != 0);
}

[[nodiscard]] bool phase_and_form_transition() noexcept
{
	th02::core::StageState state{};
	Trace trace;
	state.frame.page_back = 0U;
	state.frame.page_front = 1U;
	state.boss.topleft_on_page.fill({144, 64});
	th02::core::mima_combat_reset(state);
	state.boss.phase_frame = 100;
	if(th02::core::mima_update(state, trace) != th02::core::MimaUpdateResult::boss) return false;
	if((state.mima.phase != 1) || (state.boss.phase_frame != 0) ||
		(state.mima.phase_damage_max != 600) || (trace.palette_shows == 0)) return false;
	state.mima.phase = 8;
	state.boss.phase_frame = 0;
	const auto pending = th02::core::mima_update(state, trace);
	if((pending != th02::core::MimaUpdateResult::form1_defeat_pending) ||
		!state.mima.form1_defeat_pending) return false;
	th02::core::mima_after_form1_defeat(state, trace);
	return (state.mima.phase == 9) && !state.mima.form1_defeat_pending &&
		(state.boss.phase_frame == 0) && (state.mima.phase_damage_max == 1100);
}

[[nodiscard]] bool phase9_spiral_spawns() noexcept
{
	th02::core::StageState state{};
	Trace trace;
	state.frame.page_back = 0U;
	state.frame.page_front = 1U;
	state.boss.topleft_on_page.fill({144, 64});
	th02::core::mima_combat_reset(state);
	state.mima.phase = 9;
	state.mima.pattern = 0;
	state.boss.phase_frame = 50;
	if(th02::core::mima_update(state, trace) != th02::core::MimaUpdateResult::boss) return false;
	return state.bullets.bullets[0U].flag == th02::core::enemy_bullet_alive &&
		(state.mima.spiral_angle == state.boss.rank_param[4U]);
}

} // namespace

int main()
{
	if(!reset_and_background()) { std::fputs("mima test 1 failed\n", stderr); return 1; }
	if(!phase_and_form_transition()) { std::fputs("mima test 2 failed\n", stderr); return 1; }
	if(!phase9_spiral_spawns()) { std::fputs("mima test 3 failed\n", stderr); return 1; }
	std::puts("3 mima combat checks passed");
	return 0;
}
