#include "th02/core/stage5_end.hpp"
#include "th02/core/stage_state.hpp"
#include <algorithm>
#include <array>
#include <bit>

namespace th02::core {
namespace {
constexpr std::array<std::uint8_t, 5> continued{105, 105, 6, 105, 57};
constexpr std::array<std::uint8_t, 4> not_continued{108, 6, 108, 12};

void finish(StageState &s, Stage5EndState &e, Stage5EndPresentation &p,
	std::uint32_t bonus) noexcept
{
	s.score.score = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(s.score.score) + bonus);
	s.bomb_animation.palette_tone = 0; p.palette_show();
	s.resident_score_highest = std::max(s.resident_score_highest, s.score.score);
	e.resident.score_highest = s.resident_score_highest;
	e.resident.score_with_continues = std::bit_cast<std::int32_t>(
		std::bit_cast<std::uint32_t>(s.score.score) * 10U + s.continues_used);
	e.resident.stage = 127; e.resident.lives = s.score.lives; e.resident.bombs = s.score.bombs;
	s.item_lifecycle.item_skill = std::clamp<std::int16_t>(s.item_lifecycle.item_skill, 0, 25);
	const auto skill = s.rank * 20 + (10 - s.continues_used * 3) +
		std::max(0, 50 - s.player_miss.total_miss_count * 3 - s.bomb.total_bombs_used) + s.item_lifecycle.item_skill;
	e.resident.skill = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(skill));
	e.resident.cleared = !e.first_form && !s.continues_used;
	e.phase = Stage5EndPhase::maine;
}
void box(StageState &s, Stage5EndState &e, Stage5EndPresentation &p) noexcept
{
	e.phase = Stage5EndPhase::boxes;
	const auto face = e.first_form ? (s.continues_used ? continued[e.sequence_index] : not_continued[e.sequence_index]) : 105;
	dialog_box_begin(s.dialog, e.box, static_cast<std::uint8_t>(face), p);
}
void flash(StageState &s, Stage5EndState &e) noexcept
{
	palette_white_fade_begin(e.fade, PaletteWhiteFadeDirection::out, 0, s.bomb_animation.palette_tone);
	e.phase = Stage5EndPhase::flash_out;
}
void wait(Stage5EndState &e, std::uint16_t frames) noexcept
{ e.remaining = frames; e.phase = Stage5EndPhase::wait; }

Stage1IntroWait advance(StageState &s, Stage5EndState &e, Stage5EndPresentation &p) noexcept
{
	if (e.phase == Stage5EndPhase::pre_slide && e.slide.phase == DialogSlidePhase::complete) {
		if (!s.continues_used) s.dialog.box_current += 5;
		box(s, e, p);
	} else if (e.phase == Stage5EndPhase::boxes && e.box.phase == DialogBoxPhase::complete) {
		++e.sequence_index;
		if (e.first_form) {
			if (e.sequence_index != (s.continues_used ? continued.size() : not_continued.size())) box(s, e, p);
			else if (s.continues_used) {
				p.music_fade(10); palette_white_fade_begin(e.fade, PaletteWhiteFadeDirection::out, 10, s.bomb_animation.palette_tone);
				e.phase = Stage5EndPhase::continue_fade;
			} else {
				p.load_second_form(); (void)dialog_post_begin(s, e.slide, p); e.phase = Stage5EndPhase::post_slide;
			}
		} else if (e.sequence_index == 1) { e.script_step = 0; flash(s, e); }
		else if (e.sequence_index == 2) { e.script_step = 1; wait(e, 30); }
		else if (e.sequence_index == 3) { e.script_step = 3; wait(e, 20); }
		else { e.script_step = 4; flash(s, e); }
	} else if (e.phase == Stage5EndPhase::post_slide && e.slide.phase == DialogSlidePhase::complete) {
		p.page_access(s.frame.page_front); p.scene(BossIntroScene::clear_page, -1, s);
		p.page_access(s.frame.page_back); p.scene(BossIntroScene::clear_page, -1, s);
		s.mima.all_patterns = false; e.phase = Stage5EndPhase::form2_ready;
	} else if (e.phase == Stage5EndPhase::flash_out && e.fade.phase == PaletteWhiteFadePhase::complete) {
		p.sound_effect_force(5); palette_white_fade_begin(e.fade, PaletteWhiteFadeDirection::in, 0, s.bomb_animation.palette_tone);
		e.phase = Stage5EndPhase::flash_in;
	} else if (e.phase == Stage5EndPhase::flash_in && e.fade.phase == PaletteWhiteFadePhase::complete) {
		if (e.script_step == 6) {
			palette_white_fade_begin(e.fade, PaletteWhiteFadeDirection::out, 3, s.bomb_animation.palette_tone);
			e.phase = Stage5EndPhase::final_fade;
		} else wait(e, e.script_step == 0 ? 10 : 20);
	} else if (e.phase == Stage5EndPhase::final_fade && e.fade.phase == PaletteWhiteFadePhase::complete) finish(s, e, p, 100000);
	else if (e.phase == Stage5EndPhase::continue_fade && e.fade.phase == PaletteWhiteFadePhase::complete) {
		e.remaining = 50; e.phase = Stage5EndPhase::continue_wait;
	}
	if (e.slide.phase == DialogSlidePhase::score_refused || e.box.phase == DialogBoxPhase::invalid_text_reference) e.phase = Stage5EndPhase::refused;
	return stage5_end_wait(e);
}
} // namespace
Stage1IntroWait stage5_end_wait(const Stage5EndState &e) noexcept
{
	if (e.phase == Stage5EndPhase::idle) return Stage1IntroWait::idle;
	if (e.phase == Stage5EndPhase::maine || e.phase == Stage5EndPhase::form2_ready) return Stage1IntroWait::complete;
	if (e.phase == Stage5EndPhase::refused) return Stage1IntroWait::refused;
	if (e.phase != Stage5EndPhase::boxes) return Stage1IntroWait::delay_one_frame;
	Stage1IntroState proxy; proxy.phase = Stage1IntroPhase::boxes; proxy.box = e.box;
	return stage1_intro_wait(proxy);
}
Stage1IntroWait stage5_end_begin(StageState &s, Stage5EndState &e, bool first_form,
	Stage5EndPresentation &p, EnemyBulletInvalidationCallbacks &bullets) noexcept
{
	e = {}; e.first_form = first_form;
	if (s.frame.page_front >= 2 || s.frame.page_back >= 2) { e.phase = Stage5EndPhase::refused; return Stage1IntroWait::refused; }
	if (first_form) {
		shots_free_all(s.player_shots);
		if (bullets_clear(s.bullets, s.frame.page_back, s.frame.page_front, bullets) != EnemyBulletLifecycleResult::completed) {
			e.phase = Stage5EndPhase::refused; return Stage1IntroWait::refused;
		}
	}
	// This source blit is on the back page; do not silently redirect it to the
	// shown page during the final dialogue.
	for (std::int16_t part = 0; part < 3; ++part) p.boss_put(
		static_cast<std::int16_t>(s.boss.topleft_on_page[s.frame.page_back].x + part * 48),
		s.boss.topleft_on_page[s.frame.page_back].y, static_cast<std::int16_t>(s.boss.patnum + part));
	if (first_form) { (void)dialog_pre_begin(s, s.dialog, e.slide, p); e.phase = Stage5EndPhase::pre_slide; }
	else { p.overlay_wipe(); p.music_stop(); box(s, e, p); }
	return advance(s, e, p);
}
Stage1IntroWait stage5_end_after_delay(StageState &s, Stage5EndState &e, Stage5EndPresentation &p) noexcept
{
	if (e.phase == Stage5EndPhase::pre_slide || e.phase == Stage5EndPhase::post_slide)
		(void)dialog_slide_after_delay(s, s.dialog, e.slide, p);
	else if (e.phase == Stage5EndPhase::boxes) (void)dialog_box_after_delay(e.box);
	else if (e.phase == Stage5EndPhase::continue_wait) { if (--e.remaining == 0) finish(s, e, p, 50000); }
	else if (e.phase == Stage5EndPhase::wait) {
		if (--e.remaining == 0) {
			switch (e.script_step) {
			case 0: case 2: case 3: box(s, e, p); break;
			case 1: e.script_step = 2; flash(s, e); break;
			case 4: e.script_step = 5; flash(s, e); break;
			case 5: e.script_step = 6; flash(s, e); break;
			default: e.phase = Stage5EndPhase::refused; break;
			}
		}
	} else if (e.phase == Stage5EndPhase::flash_out || e.phase == Stage5EndPhase::flash_in ||
		e.phase == Stage5EndPhase::final_fade || e.phase == Stage5EndPhase::continue_fade)
		palette_white_fade_after_vsync(e.fade, s.bomb_animation.palette_tone, p);
	return advance(s, e, p);
}
Stage1IntroWait stage5_end_input(StageState &s, Stage5EndState &e, std::uint16_t input, Stage5EndPresentation &p) noexcept
{
	if (e.phase == Stage5EndPhase::boxes && e.box.phase == DialogBoxPhase::input_sample) {
		s.input = input; (void)dialog_box_input(s.dialog, e.box, input, p);
	}
	return advance(s, e, p);
}
Stage1IntroWait stage5_end_acknowledgement(StageState &s, Stage5EndState &e, bool pressed, Stage5EndPresentation &p) noexcept
{
	if (e.phase == Stage5EndPhase::boxes) (void)dialog_box_acknowledgement(s.dialog, e.box, pressed);
	return advance(s, e, p);
}
} // namespace th02::core
