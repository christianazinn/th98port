#include "th02/core/stage_continue.hpp"

#include "th02/core/reset.hpp"
#include "th02/core/stage1_state.hpp"

namespace th02::core {
namespace {

constexpr ScreenPoint player_continue_start{208, 336};
constexpr std::uint8_t continue_invincibility_frames = 200U;
constexpr std::uint16_t palette_tone_100 = 100U;
constexpr std::uint16_t tile_render_height = 16U;

} // namespace

StageContinueResult stage1_continue_resume(
	Stage1State &state, StageContinuePresentationCallbacks &presentation
) noexcept
{
	presentation.hud_put();
	presentation.overlay_stage_leave_animate();

	// The source writes page 1 before page 0 despite the chained assignment's
	// textual left-to-right spelling.
	state.frame.player.on_page[1U] = player_continue_start;
	state.frame.player.on_page[0U] = player_continue_start;
	player_reset(state.player_reset, state.quit, state.player_hit,
		state.player_miss, state.player_shots, state.player_shot_lifetime,
		state.player_stream, state.player_volley, state.player_speeds,
		state.shottype);
	// Player reset writes the source boss-pos globals. BossState is this native
	// aggregate's canonical owner of that pair, so mirror the reset projection
	// before the next volley or entity pass observes it.
	state.boss.target = {
		state.player_volley.boss_position.x,
		state.player_volley.boss_position.y,
	};
	state.player_hit.invincibility_time = continue_invincibility_frames;

	state.tiles.copy_lines_top = 0U;
	state.tiles.copy_lines_h = tile_render_height;
	presentation.tiles_render_all(state.frame.page_front);
	state.tiles.copy_lines_top = 0U;
	state.tiles.copy_lines_h = tile_render_height;
	presentation.tiles_render_all(state.frame.page_back);
	if(bullets_clear(state.bullets, state.frame.page_back, state.frame.page_front,
		presentation) != EnemyBulletLifecycleResult::completed) {
		return StageContinueResult::bullet_invalid_page;
	}

	state.bomb_animation.palette_tone = palette_tone_100;
	presentation.palette_100();
	presentation.overlay_stage_enter_animate();
	return StageContinueResult::completed;
}

} // namespace th02::core
