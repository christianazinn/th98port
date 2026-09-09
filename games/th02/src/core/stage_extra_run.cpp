#include "th02/core/stage_extra_run.hpp"

namespace th02::core {
namespace {
Stage1RunWait frame_result(StageState &state, StageExtraRunState &run,
	StageExtraRunPresentation &presentation) noexcept
{
	switch (run.frame.phase) {
	case Stage1FramePhase::complete: run.phase = Stage1RunPhase::ready; break;
	case Stage1FramePhase::paused: run.phase = Stage1RunPhase::paused; break;
	case Stage1FramePhase::cleared: run.phase = Stage1RunPhase::cleared; break;
	case Stage1FramePhase::quit: run.phase = Stage1RunPhase::quit; break;
	case Stage1FramePhase::boss_init_pending:
		run.phase = Stage1RunPhase::intro;
		if (boss_intro_begin(state, run.intro, presentation.intro, BossIntroKind::sigma) == Stage1IntroWait::refused)
			run.phase = Stage1RunPhase::refused;
		break;
	case Stage1FramePhase::stage_end_pending:
		run.phase = Stage1RunPhase::ending;
		if (stage_extra_end_begin(state, run.ending, presentation.ending) == Stage1EndWait::refused)
			run.phase = Stage1RunPhase::refused;
		break;
	default: run.phase = Stage1RunPhase::refused; break;
	}
	return stage_extra_run_wait(run);
}

Stage1RunWait lifecycle_result(StageState &state, StageExtraRunState &run,
	StageExtraRunPresentation &presentation) noexcept
{
	if (run.phase == Stage1RunPhase::intro) {
		if (run.intro.phase == BossIntroPhase::refused) run.phase = Stage1RunPhase::refused;
		else if (run.intro.phase == BossIntroPhase::complete) {
			(void)stage1_frame_after_boss_init(state, run.frame, presentation.frame);
			return frame_result(state, run, presentation);
		}
	} else if (run.phase == Stage1RunPhase::ending) {
		if (run.ending.phase == Stage1EndPhase::refused) run.phase = Stage1RunPhase::refused;
		else if (run.ending.phase == Stage1EndPhase::complete) {
			run.phase = run.ending.maine ? Stage1RunPhase::cleared : Stage1RunPhase::refused;
		}
	}
	return stage_extra_run_wait(run);
}
} // namespace

Stage1RunWait stage_extra_run_wait(const StageExtraRunState &run) noexcept
{
	switch (run.phase) {
	case Stage1RunPhase::not_started:
	case Stage1RunPhase::ready: return Stage1RunWait::ready_for_frame;
	case Stage1RunPhase::input: return Stage1RunWait::input_sample;
	case Stage1RunPhase::paused: return Stage1RunWait::paused;
	case Stage1RunPhase::cleared: return Stage1RunWait::maine;
	case Stage1RunPhase::quit: return Stage1RunWait::quit;
	case Stage1RunPhase::refused: return Stage1RunWait::refused;
	case Stage1RunPhase::intro:
		switch (boss_intro_wait(run.intro)) {
		case Stage1IntroWait::input_sample: return Stage1RunWait::input_sample;
		case Stage1IntroWait::delay_one_frame: return Stage1RunWait::delay_one_frame;
		case Stage1IntroWait::delay_two_frames: return Stage1RunWait::delay_two_frames;
		case Stage1IntroWait::acknowledgement_first_sample: return Stage1RunWait::acknowledgement_first_sample;
		case Stage1IntroWait::acknowledgement_second_sample: return Stage1RunWait::acknowledgement_second_sample;
		default: return Stage1RunWait::refused;
		}
	case Stage1RunPhase::ending:
		switch (stage1_end_wait(run.ending)) {
		case Stage1EndWait::input_sample: return Stage1RunWait::input_sample;
		case Stage1EndWait::delay_one_frame: return Stage1RunWait::delay_one_frame;
		case Stage1EndWait::delay_two_frames: return Stage1RunWait::delay_two_frames;
		case Stage1EndWait::acknowledgement_first_sample: return Stage1RunWait::acknowledgement_first_sample;
		case Stage1EndWait::acknowledgement_second_sample: return Stage1RunWait::acknowledgement_second_sample;
		default: return Stage1RunWait::refused;
		}
	}
	return Stage1RunWait::refused;
}

Stage1RunWait stage_extra_run_prepare(StageState &state, StageExtraRunState &run,
	StageExtraRunPresentation &presentation) noexcept
{
	if (run.phase != Stage1RunPhase::not_started && run.phase != Stage1RunPhase::ready)
		return stage_extra_run_wait(run);
	if (run.phase == Stage1RunPhase::not_started) state.scroll_line_on_page = {0, 0};
	if (state.quit) {
		score_delta_commit(state.score);
		state.quit = false;
		run.phase = Stage1RunPhase::quit;
		return Stage1RunWait::quit;
	}
	stage1_notifications(state, run.stage_title_enabled, run.demo, presentation.notifications);
	run.preparation = stage_extra_prepare(state, presentation.invalidation, presentation.preparation);
	run.phase = run.preparation == Stage1PreparationResult::input_pending ?
		Stage1RunPhase::input : Stage1RunPhase::refused;
	return stage_extra_run_wait(run);
}

Stage1RunWait stage_extra_run_input(StageState &state, StageExtraRunState &run,
	std::uint16_t input, StageExtraRunPresentation &presentation) noexcept
{
	if (run.phase == Stage1RunPhase::intro) {
		(void)boss_intro_input(state, run.intro, input, presentation.intro);
		return lifecycle_result(state, run, presentation);
	}
	if (run.phase == Stage1RunPhase::ending) {
		(void)stage1_end_input(state, run.ending, input, presentation.ending);
		return lifecycle_result(state, run, presentation);
	}
	if (run.phase != Stage1RunPhase::input) return stage_extra_run_wait(run);
	state.input = input;
	run.player = player_move_and_bomb_update(state, presentation.bomb_animation, presentation.bomb);
	if (!run.player.bomb_update_called ||
		(run.player.bomb_result != BombUpdateResult::inactive &&
		 run.player.bomb_result != BombUpdateResult::running &&
		 run.player.bomb_result != BombUpdateResult::completed)) {
		run.phase = Stage1RunPhase::refused;
		return Stage1RunWait::refused;
	}
	run.entities = stage_extra_entities_update(state, presentation.entities);
	if (run.entities.result != Stage1EntitiesResult::completed) {
		run.phase = Stage1RunPhase::refused;
		return Stage1RunWait::refused;
	}
	run.frame.phase = Stage1FramePhase::entities_complete;
	(void)stage1_frame_finish(state, run.frame, presentation.frame);
	return frame_result(state, run, presentation);
}

Stage1RunWait stage_extra_run_after_delay(StageState &state, StageExtraRunState &run,
	StageExtraRunPresentation &presentation) noexcept
{
	if (run.phase == Stage1RunPhase::intro)
		(void)boss_intro_after_delay(state, run.intro, presentation.intro);
	else if (run.phase == Stage1RunPhase::ending)
		(void)stage1_end_after_delay(state, run.ending, presentation.ending);
	return lifecycle_result(state, run, presentation);
}

Stage1RunWait stage_extra_run_acknowledgement(StageState &state, StageExtraRunState &run,
	bool pressed, StageExtraRunPresentation &presentation) noexcept
{
	if (run.phase == Stage1RunPhase::intro)
		(void)boss_intro_acknowledgement(state, run.intro, pressed, presentation.intro);
	else if (run.phase == Stage1RunPhase::ending)
		(void)stage1_end_acknowledgement(state, run.ending, pressed, presentation.ending);
	return lifecycle_result(state, run, presentation);
}

Stage1RunWait stage_extra_run_after_pause(StageState &state, StageExtraRunState &run,
	bool quit_selected, StageExtraRunPresentation &presentation) noexcept
{
	if (run.phase != Stage1RunPhase::paused) return stage_extra_run_wait(run);
	(void)stage1_frame_resume(state, run.frame, quit_selected, presentation.frame);
	return frame_result(state, run, presentation);
}
} // namespace th02::core
