#include "th02/core/stage5_run.hpp"

namespace th02::core {
namespace {
Stage1RunWait frame_result(StageState &state, Stage5RunState &run,
	Stage5RunPresentation &presentation) noexcept
{
	switch (run.frame.phase) {
	case Stage1FramePhase::complete: run.phase = Stage5RunPhase::ready; break;
	case Stage1FramePhase::paused: run.phase = Stage5RunPhase::paused; break;
	case Stage1FramePhase::quit: run.phase = Stage5RunPhase::quit; break;
	case Stage1FramePhase::boss_init_pending:
		run.phase = Stage5RunPhase::intro;
		if (boss_intro_begin(state, run.intro, presentation.intro, BossIntroKind::mima) == Stage1IntroWait::refused)
			run.phase = Stage5RunPhase::refused;
		break;
	default: run.phase = Stage5RunPhase::refused; break;
	}
	return stage5_run_wait(run);
}

Stage1RunWait lifecycle_result(StageState &state, Stage5RunState &run,
	Stage5RunPresentation &presentation) noexcept
{
	if (run.phase == Stage5RunPhase::intro) {
		if (run.intro.phase == BossIntroPhase::refused) run.phase = Stage5RunPhase::refused;
		else if (run.intro.phase == BossIntroPhase::complete) {
			(void)stage1_frame_after_boss_init(state, run.frame, presentation.frame);
			return frame_result(state, run, presentation);
		}
	} else if (run.phase == Stage5RunPhase::ending) {
		if (run.ending.phase == Stage5EndPhase::refused) run.phase = Stage5RunPhase::refused;
		else if (run.ending.phase == Stage5EndPhase::maine) run.phase = Stage5RunPhase::maine;
		else if (run.ending.phase == Stage5EndPhase::form2_ready) {
			run.entities.result = stage5_entities_after_form1(state, presentation.entities);
			if (run.entities.result != StageEntitiesResult::completed) {
				run.phase = Stage5RunPhase::refused; return Stage1RunWait::refused;
			}
			run.entities.mima = MimaUpdateResult::boss;
			run.frame.phase = Stage1FramePhase::entities_complete;
			(void)stage1_frame_finish(state, run.frame, presentation.frame);
			return frame_result(state, run, presentation);
		}
	}
	return stage5_run_wait(run);
}
} // namespace

Stage1RunWait stage5_run_wait(const Stage5RunState &run) noexcept
{
	switch (run.phase) {
	case Stage5RunPhase::not_started:
	case Stage5RunPhase::ready: return Stage1RunWait::ready_for_frame;
	case Stage5RunPhase::input: return Stage1RunWait::input_sample;
	case Stage5RunPhase::paused: return Stage1RunWait::paused;
	case Stage5RunPhase::maine: return Stage1RunWait::maine;
	case Stage5RunPhase::quit: return Stage1RunWait::quit;
	case Stage5RunPhase::refused: return Stage1RunWait::refused;
	case Stage5RunPhase::intro:
		switch (boss_intro_wait(run.intro)) {
		case Stage1IntroWait::input_sample: return Stage1RunWait::input_sample;
		case Stage1IntroWait::delay_one_frame: return Stage1RunWait::delay_one_frame;
		case Stage1IntroWait::delay_two_frames: return Stage1RunWait::delay_two_frames;
		case Stage1IntroWait::acknowledgement_first_sample: return Stage1RunWait::acknowledgement_first_sample;
		case Stage1IntroWait::acknowledgement_second_sample: return Stage1RunWait::acknowledgement_second_sample;
		default: return Stage1RunWait::refused;
		}
	case Stage5RunPhase::ending:
		switch (stage5_end_wait(run.ending)) {
		case Stage1IntroWait::input_sample: return Stage1RunWait::input_sample;
		case Stage1IntroWait::delay_one_frame: return Stage1RunWait::delay_one_frame;
		case Stage1IntroWait::delay_two_frames: return Stage1RunWait::delay_two_frames;
		case Stage1IntroWait::acknowledgement_first_sample: return Stage1RunWait::acknowledgement_first_sample;
		case Stage1IntroWait::acknowledgement_second_sample: return Stage1RunWait::acknowledgement_second_sample;
		default: return Stage1RunWait::refused;
		}
	}
	return Stage1RunWait::refused;
}

Stage1RunWait stage5_run_prepare(StageState &state, Stage5RunState &run,
	Stage5RunPresentation &presentation) noexcept
{
	if (run.phase != Stage5RunPhase::not_started && run.phase != Stage5RunPhase::ready)
		return stage5_run_wait(run);
	if (run.phase == Stage5RunPhase::not_started) state.scroll_line_on_page = {0, 0};
	if (state.quit) {
		score_delta_commit(state.score);
		state.quit = false;
		run.phase = Stage5RunPhase::quit;
		return Stage1RunWait::quit;
	}
	stage1_notifications(state, run.stage_title_enabled, run.demo, presentation.notifications);
	run.preparation = stage5_prepare(state, presentation.invalidation, presentation.preparation);
	run.phase = run.preparation == Stage1PreparationResult::input_pending ?
		Stage5RunPhase::input : Stage5RunPhase::refused;
	return stage5_run_wait(run);
}

Stage1RunWait stage5_run_input(StageState &state, Stage5RunState &run,
	std::uint16_t input, Stage5RunPresentation &presentation) noexcept
{
	if (run.phase == Stage5RunPhase::intro) {
		(void)boss_intro_input(state, run.intro, input, presentation.intro);
		return lifecycle_result(state, run, presentation);
	}
	if (run.phase == Stage5RunPhase::ending) {
		(void)stage5_end_input(state, run.ending, input, presentation.ending);
		return lifecycle_result(state, run, presentation);
	}
	if (run.phase != Stage5RunPhase::input) return stage5_run_wait(run);
	state.input = input;
	run.player = player_move_and_bomb_update(state, presentation.bomb_animation, presentation.bomb);
	if (!run.player.bomb_update_called ||
		(run.player.bomb_result != BombUpdateResult::inactive &&
		 run.player.bomb_result != BombUpdateResult::running &&
		 run.player.bomb_result != BombUpdateResult::completed)) {
		run.phase = Stage5RunPhase::refused;
		return Stage1RunWait::refused;
	}
	run.entities = stage5_entities_update(state, presentation.entities);
	if (run.entities.mima == MimaUpdateResult::form1_defeat_pending ||
		run.entities.mima == MimaUpdateResult::final_end_pending) {
		run.phase = Stage5RunPhase::ending;
		(void)stage5_end_begin(state, run.ending,
			run.entities.mima == MimaUpdateResult::form1_defeat_pending,
			presentation.ending, presentation.entities.mima);
		return lifecycle_result(state, run, presentation);
	}
	if (run.entities.result != Stage1EntitiesResult::completed) {
		run.phase = Stage5RunPhase::refused;
		return Stage1RunWait::refused;
	}
	run.frame.phase = Stage1FramePhase::entities_complete;
	(void)stage1_frame_finish(state, run.frame, presentation.frame);
	return frame_result(state, run, presentation);
}

Stage1RunWait stage5_run_after_delay(StageState &state, Stage5RunState &run,
	Stage5RunPresentation &presentation) noexcept
{
	if (run.phase == Stage5RunPhase::intro)
		(void)boss_intro_after_delay(state, run.intro, presentation.intro);
	else if (run.phase == Stage5RunPhase::ending)
		(void)stage5_end_after_delay(state, run.ending, presentation.ending);
	return lifecycle_result(state, run, presentation);
}

Stage1RunWait stage5_run_acknowledgement(StageState &state, Stage5RunState &run,
	bool pressed, Stage5RunPresentation &presentation) noexcept
{
	if (run.phase == Stage5RunPhase::intro)
		(void)boss_intro_acknowledgement(state, run.intro, pressed, presentation.intro);
	else if (run.phase == Stage5RunPhase::ending)
		(void)stage5_end_acknowledgement(state, run.ending, pressed, presentation.ending);
	return lifecycle_result(state, run, presentation);
}

Stage1RunWait stage5_run_after_pause(StageState &state, Stage5RunState &run,
	bool quit_selected, Stage5RunPresentation &presentation) noexcept
{
	if (run.phase != Stage5RunPhase::paused) return stage5_run_wait(run);
	(void)stage1_frame_resume(state, run.frame, quit_selected, presentation.frame);
	return frame_result(state, run, presentation);
}
} // namespace th02::core
