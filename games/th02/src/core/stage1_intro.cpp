#include "th02/core/stage1_intro.hpp"
#include "th02/core/stage1_lifecycle.hpp"
#include "th02/core/stage1_state.hpp"

#include <array>
#include <span>

namespace th02::core {
namespace {

// th02_main.asm GENERIC_FACES[0], using face.hpp's 3x3 tile top-left IDs.
constexpr std::array<std::uint8_t, 16> faces{0, 96, 51, 96, 9, 96, 0, 96,
	99, 99, 48, 12, 99, 99, 6, 99};
constexpr std::array<std::uint8_t, 8> stones_faces{6, 96, 96, 12, 96, 51, 96, 0};

Stage1IntroWait advance(Stage1State &state, Stage1IntroState &intro,
	Stage1IntroPresentation &presentation) noexcept
{
	const std::span<const std::uint8_t> sequence = intro.stones ?
		std::span<const std::uint8_t>(stones_faces) : std::span<const std::uint8_t>(faces);
	if (intro.phase == Stage1IntroPhase::pre_slide && intro.slide.phase == DialogSlidePhase::complete) {
		intro.sequence_index = 0;
		dialog_box_begin(state.dialog, intro.box, sequence[0], presentation);
		intro.phase = Stage1IntroPhase::boxes;
	} else if (intro.phase == Stage1IntroPhase::boxes && intro.box.phase == DialogBoxPhase::complete) {
		++intro.sequence_index;
		if (intro.sequence_index < sequence.size()) {
			dialog_box_begin(state.dialog, intro.box, sequence[intro.sequence_index], presentation);
		} else {
			(void)dialog_post_begin(state, intro.slide, presentation);
			intro.phase = Stage1IntroPhase::post_slide;
		}
	} else if (intro.phase == Stage1IntroPhase::post_slide && intro.slide.phase == DialogSlidePhase::complete) {
		if (intro.stones) {
			presentation.music_stop();
			presentation.music_load("boss2.m");
			presentation.music_play();
			stones_combat_reset(state.boss, state.stones, state.lasers, state.rank, state.scroll.line);
			enemies_remove_all(state.enemies);
			state.lasers_enabled = true;
		} else rika_combat_setup_after_dialogue(state.boss, state.rika, state.stage_frame,
			state.lasers_enabled, state.scroll.line, state.rank);
		intro.phase = Stage1IntroPhase::complete;
	}
	if (intro.slide.phase == DialogSlidePhase::score_refused ||
		intro.box.phase == DialogBoxPhase::invalid_text_reference) intro.phase = Stage1IntroPhase::refused;
	return stage1_intro_wait(intro);
}

} // namespace

Stage1IntroWait stage1_intro_wait(const Stage1IntroState &intro) noexcept
{
	switch (intro.phase) {
	case Stage1IntroPhase::idle: return Stage1IntroWait::idle;
	case Stage1IntroPhase::complete: return Stage1IntroWait::complete;
	case Stage1IntroPhase::refused: return Stage1IntroWait::refused;
	case Stage1IntroPhase::pre_slide:
	case Stage1IntroPhase::post_slide: return Stage1IntroWait::delay_one_frame;
	case Stage1IntroPhase::boxes:
		switch (intro.box.phase) {
		case DialogBoxPhase::input_sample: return Stage1IntroWait::input_sample;
		case DialogBoxPhase::delay_one_frame: return Stage1IntroWait::delay_one_frame;
		case DialogBoxPhase::acknowledgement_first_sample: return Stage1IntroWait::acknowledgement_first_sample;
		case DialogBoxPhase::acknowledgement_delay_two_frames: return Stage1IntroWait::delay_two_frames;
		case DialogBoxPhase::acknowledgement_second_sample: return Stage1IntroWait::acknowledgement_second_sample;
		default: return Stage1IntroWait::refused;
		}
	}
	return Stage1IntroWait::refused;
}

Stage1IntroWait stage1_intro_begin(Stage1State &state, Stage1IntroState &intro,
	Stage1IntroPresentation &presentation) noexcept
{
	intro = {};
	presentation.music_stop();
	presentation.music_load("boss1.m");
	presentation.music_play();
	intro.phase = Stage1IntroPhase::pre_slide;
	(void)dialog_pre_begin(state, state.dialog, intro.slide, presentation);
	return advance(state, intro, presentation);
}

Stage1IntroWait stage3_intro_begin(StageState &state, Stage1IntroState &intro,
	Stage1IntroPresentation &presentation) noexcept
{
	intro = {};
	intro.stones = true;
	intro.phase = Stage1IntroPhase::pre_slide;
	(void)dialog_pre_begin(state, state.dialog, intro.slide, presentation);
	return advance(state, intro, presentation);
}

Stage1IntroWait stage1_intro_after_delay(Stage1State &state, Stage1IntroState &intro,
	Stage1IntroPresentation &presentation) noexcept
{
	if (intro.phase == Stage1IntroPhase::pre_slide || intro.phase == Stage1IntroPhase::post_slide)
		(void)dialog_slide_after_delay(state, state.dialog, intro.slide, presentation);
	else if (intro.phase == Stage1IntroPhase::boxes) (void)dialog_box_after_delay(intro.box);
	return advance(state, intro, presentation);
}

Stage1IntroWait stage1_intro_input(Stage1State &state, Stage1IntroState &intro,
	std::uint16_t input, Stage1IntroPresentation &presentation) noexcept
{
	if (intro.phase == Stage1IntroPhase::boxes && intro.box.phase == DialogBoxPhase::input_sample) {
		state.input = input;
		(void)dialog_box_input(state.dialog, intro.box, input, presentation);
	}
	return advance(state, intro, presentation);
}

Stage1IntroWait stage1_intro_acknowledgement(Stage1State &state, Stage1IntroState &intro,
	bool pressed, Stage1IntroPresentation &presentation) noexcept
{
	if (intro.phase == Stage1IntroPhase::boxes)
		(void)dialog_box_acknowledgement(state.dialog, intro.box, pressed);
	return advance(state, intro, presentation);
}

} // namespace th02::core
