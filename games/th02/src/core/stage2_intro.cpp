#include "th02/core/stage2_intro.hpp"
#include "th02/core/stage_state.hpp"

#include <array>
#include <bit>

namespace th02::core {
namespace {
// dialog_script_stage2_pre_intro_animate, then GENERIC_FACES[2].
constexpr std::array<std::uint8_t, 24> faces{0, 96,
	102, 102, 57, 60, 102, 102, 60, 9, 150, 54, 102, 102,
	96, 54, 102, 60, 9, 102, 9, 150, 57, 150};

bool in_boxes(const Stage2IntroState &intro) noexcept
{
	return intro.phase == Stage2IntroPhase::entrance_boxes || intro.phase == Stage2IntroPhase::boxes;
}

Stage1IntroWait advance(StageState &state, Stage2IntroState &intro,
	Stage2IntroPresentation &presentation) noexcept
{
	if (intro.phase == Stage2IntroPhase::pre_slide && intro.slide.phase == DialogSlidePhase::complete) {
		dialog_box_begin(state.dialog, intro.box, faces[0], presentation);
		intro.phase = Stage2IntroPhase::entrance_boxes;
	} else if (in_boxes(intro) && intro.box.phase == DialogBoxPhase::complete) {
		++intro.sequence_index;
		if (intro.sequence_index == 2) {
			state.boss.topleft_on_page = {ScreenPoint{192, 48}, ScreenPoint{192, 48}};
			if (state.frame.page_back >= 2) {
				intro.phase = Stage2IntroPhase::refused;
			} else {
				palette_white_fade_begin(intro.fade, PaletteWhiteFadeDirection::out, 1, state.bomb_animation.palette_tone);
				intro.phase = Stage2IntroPhase::white_out;
			}
		} else if (intro.sequence_index < faces.size()) {
			dialog_box_begin(state.dialog, intro.box, faces[intro.sequence_index], presentation);
		} else {
			(void)dialog_post_begin(state, intro.slide, presentation);
			intro.phase = Stage2IntroPhase::post_slide;
		}
	} else if (intro.phase == Stage2IntroPhase::white_out && intro.fade.phase == PaletteWhiteFadePhase::complete) {
		auto y = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(48 + state.scroll.line));
		if (y >= 400) y = static_cast<std::int16_t>(y - 400);
		presentation.boss_put(state.boss.topleft_on_page[state.frame.page_back].x, y, state.boss.patnum);
		presentation.music_stop();
		presentation.music_load("boss4.m");
		presentation.music_play();
		palette_white_fade_begin(intro.fade, PaletteWhiteFadeDirection::in, 1, state.bomb_animation.palette_tone);
		intro.phase = Stage2IntroPhase::white_in;
	} else if (intro.phase == Stage2IntroPhase::white_in && intro.fade.phase == PaletteWhiteFadePhase::complete) {
		dialog_box_begin(state.dialog, intro.box, faces[intro.sequence_index], presentation);
		intro.phase = Stage2IntroPhase::boxes;
	} else if (intro.phase == Stage2IntroPhase::post_slide && intro.slide.phase == DialogSlidePhase::complete) {
		meira_combat_setup_after_dialogue(state.boss, state.meira, state.bullet_special, state.rank);
		intro.phase = Stage2IntroPhase::complete;
	}
	if (intro.slide.phase == DialogSlidePhase::score_refused || intro.box.phase == DialogBoxPhase::invalid_text_reference)
		intro.phase = Stage2IntroPhase::refused;
	return stage2_intro_wait(intro);
}
}

Stage1IntroWait stage2_intro_wait(const Stage2IntroState &intro) noexcept
{
	if (intro.phase == Stage2IntroPhase::idle) return Stage1IntroWait::idle;
	if (intro.phase == Stage2IntroPhase::complete) return Stage1IntroWait::complete;
	if (intro.phase == Stage2IntroPhase::refused) return Stage1IntroWait::refused;
	if (!in_boxes(intro)) return Stage1IntroWait::delay_one_frame;
	// Both bosses use exactly the same dialog-box input/delay contract.
	Stage1IntroState box_wait{};
	box_wait.phase = Stage1IntroPhase::boxes;
	box_wait.box = intro.box;
	return stage1_intro_wait(box_wait);
}

Stage1IntroWait stage2_intro_begin(StageState &state, Stage2IntroState &intro,
	Stage2IntroPresentation &presentation) noexcept
{
	intro = {};
	state.boss.patnum = 141;
	intro.phase = Stage2IntroPhase::pre_slide;
	(void)dialog_pre_begin(state, state.dialog, intro.slide, presentation);
	return advance(state, intro, presentation);
}

Stage1IntroWait stage2_intro_after_delay(StageState &state, Stage2IntroState &intro,
	Stage2IntroPresentation &presentation) noexcept
{
	if (intro.phase == Stage2IntroPhase::pre_slide || intro.phase == Stage2IntroPhase::post_slide)
		(void)dialog_slide_after_delay(state, state.dialog, intro.slide, presentation);
	else if (in_boxes(intro)) (void)dialog_box_after_delay(intro.box);
	else if (intro.phase == Stage2IntroPhase::white_out || intro.phase == Stage2IntroPhase::white_in)
		palette_white_fade_after_vsync(intro.fade, state.bomb_animation.palette_tone, presentation);
	return advance(state, intro, presentation);
}

Stage1IntroWait stage2_intro_input(StageState &state, Stage2IntroState &intro,
	std::uint16_t input, Stage2IntroPresentation &presentation) noexcept
{
	if (in_boxes(intro) && intro.box.phase == DialogBoxPhase::input_sample) {
		state.input = input;
		(void)dialog_box_input(state.dialog, intro.box, input, presentation);
	}
	return advance(state, intro, presentation);
}

Stage1IntroWait stage2_intro_acknowledgement(StageState &state, Stage2IntroState &intro,
	bool pressed, Stage2IntroPresentation &presentation) noexcept
{
	if (in_boxes(intro)) (void)dialog_box_acknowledgement(state.dialog, intro.box, pressed);
	return advance(state, intro, presentation);
}
} // namespace th02::core
