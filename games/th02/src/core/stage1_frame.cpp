#include "th02/core/stage1_frame.hpp"
#include "th02/core/enemy_lifecycle.hpp"

#include <bit>

namespace th02::core {
namespace {

Stage1FramePhase score_tail(Stage1State &state, Stage1FrameSchedule &schedule,
	Stage1FramePresentation &presentation) noexcept
{
	if ((state.stage_frame % 1800U) == 1500U &&
		state.player_miss.playperf < state.playperf_max) {
		state.player_miss.playperf = std::bit_cast<std::int16_t>(
			static_cast<std::uint16_t>(
				static_cast<std::uint16_t>(state.player_miss.playperf) + 1U));
	}
	const auto events = score_update(state.score, state.reduce_effects);
	if (!events) return schedule.phase = Stage1FramePhase::faulted;
	presentation.score_events(*events);
	if (state.quit) {
		score_delta_commit(state.score);
		state.quit = false;
		return schedule.phase = Stage1FramePhase::quit;
	}
	return schedule.phase = Stage1FramePhase::complete;
}

Stage1FramePhase counters(Stage1State &state, Stage1FrameSchedule &schedule,
	Stage1FramePresentation &presentation) noexcept
{
	state.resident_frame = std::bit_cast<std::int32_t>(
		std::bit_cast<std::uint32_t>(state.resident_frame) + 1U);
	++state.stage_frame;
	// The source calls stage_should_end even when quit is already true.
	if (schedule.stage_end_enabled && state.stage_progression == StageProgression::clear)
		return schedule.phase = Stage1FramePhase::stage_end_pending;
	return score_tail(state, schedule, presentation);
}

Stage1FramePhase scroll_and_flip(Stage1State &state, Stage1FrameSchedule &schedule,
	Stage1FramePresentation &presentation) noexcept
{
	if (scroll_finish_frame(state.scroll, state.tiles, state.stage_data,
		state.tile_sections, state.frame.page_front, state.frame.page_back,
		presentation) == TileScrollResult::invalid_native_reference)
		return schedule.phase = Stage1FramePhase::faulted;
	presentation.wait_for_presentation(state.bomb_animation.slowdown_factor);
	state.frame.page_front = state.frame.page_back;
	presentation.page_show(state.frame.page_back);
	state.frame.page_back ^= 1U;
	presentation.page_access(state.frame.page_back);
	if (schedule.boss_activation_enabled && state.scroll.done) {
		state.stage_progression = StageProgression::boss;
		return schedule.phase = Stage1FramePhase::boss_init_pending;
	}
	return counters(state, schedule, presentation);
}

} // namespace

Stage1FramePhase stage1_frame_finish(Stage1State &state,
	Stage1FrameSchedule &schedule, Stage1FramePresentation &presentation) noexcept
{
	if (schedule.phase != Stage1FramePhase::entities_complete) return schedule.phase;
	if ((state.input & 0x0040U) != 0U)
		return schedule.phase = Stage1FramePhase::paused;
	return scroll_and_flip(state, schedule, presentation);
}

Stage1FramePhase stage1_frame_resume(Stage1State &state,
	Stage1FrameSchedule &schedule, bool pause_quit,
	Stage1FramePresentation &presentation) noexcept
{
	if (schedule.phase != Stage1FramePhase::paused) return schedule.phase;
	if (pause_quit) state.quit = true;
	return scroll_and_flip(state, schedule, presentation);
}

Stage1FramePhase stage1_frame_after_boss_init(Stage1State &state,
	Stage1FrameSchedule &schedule, Stage1FramePresentation &presentation) noexcept
{
	if (schedule.phase != Stage1FramePhase::boss_init_pending) return schedule.phase;
	enemies_remove_all(state.enemies);
	state.enemies_enabled = false;
	schedule.boss_activation_enabled = false;
	state.boss_enabled = true;
	schedule.stage_end_enabled = true;
	state.scroll.cycle = 255U;
	state.bgm_show_timer = 1U;
	state.bgm_title_id = state.boss_bgm_title_id;
	return counters(state, schedule, presentation);
}

Stage1FramePhase stage1_frame_after_stage_end(Stage1State &state,
	Stage1FrameSchedule &schedule, Stage1FramePresentation &presentation) noexcept
{
	if (schedule.phase != Stage1FramePhase::stage_end_pending) return schedule.phase;
	if (!state.quit) return schedule.phase = Stage1FramePhase::cleared;
	return score_tail(state, schedule, presentation);
}

} // namespace th02::core
