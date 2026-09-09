#include "th02/core/dialog.hpp"
#include "th02/core/stage1_state.hpp"

#include <algorithm>
#include <bit>

namespace th02::core {
namespace {

std::int16_t word_add(std::int16_t value, std::uint16_t amount) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(value) + amount));
}

DialogBoxPhase advance_cursor(DialogBoxState &box, std::int16_t modulus) noexcept
{
	if ((box.loop_count % modulus) == 0) box.cursor = word_add(box.cursor, 2);
	return box.phase = box.cursor > 80 ? DialogBoxPhase::acknowledgement_first_sample :
		DialogBoxPhase::input_sample;
}

std::int16_t scrolled(std::int16_t top, std::int16_t delta) noexcept
{
	top = word_add(top, static_cast<std::uint16_t>(delta));
	if (top >= 400) top = word_add(top, static_cast<std::uint16_t>(-400));
	return top;
}

void slide_begin(const Stage1State &state, DialogSlideState &slide, bool post) noexcept
{
	const auto top = scrolled(320, state.scroll.line);
	slide = {DialogSlidePhase::delay_one_frame,
		static_cast<std::int16_t>(post ? 16 : 384), top, scrolled(32, top), post};
}

} // namespace

bool dialog_load_and_init(DialogState &state, std::span<const std::uint8_t> bytes) noexcept
{
	if (bytes.size() > state.text.size()) return false;
	std::copy(bytes.begin(), bytes.end(), state.text.begin());
	state.box_current = 0;
	return true;
}

void dialog_box_begin(const DialogState &dialog, DialogBoxState &box,
	std::int16_t face, DialogBoxPresentation &presentation) noexcept
{
	presentation.box_wipe();
	box = {DialogBoxPhase::input_sample, 6, 0, face, dialog.box_current, false, false};
}

DialogBoxPhase dialog_box_input(const DialogState &dialog, DialogBoxState &box,
	std::uint16_t input, DialogBoxPresentation &presentation) noexcept
{
	if (box.phase != DialogBoxPhase::input_sample) return box.phase;
	presentation.face_put(box.face);
	if (box.cursor <= 72) {
		const std::uint8_t line = box.cursor <= 36 ? 0U : 1U;
		const auto count = std::max(0, static_cast<int>(box.cursor) - static_cast<int>(line) * 36);
		if (box.box >= 64U) return box.phase = DialogBoxPhase::invalid_text_reference;
		const auto offset = static_cast<std::size_t>(box.box) * 80U + static_cast<std::size_t>(line) * 40U;
		presentation.text_put(line, std::span{dialog.text}.subspan(offset, static_cast<std::size_t>(count)));
	}
	box.loop_count = word_add(box.loop_count, 1);
	if (input == 0U) return box.phase = DialogBoxPhase::delay_one_frame;
	return advance_cursor(box, 1);
}

DialogBoxPhase dialog_box_after_delay(DialogBoxState &box) noexcept
{
	if (box.phase == DialogBoxPhase::delay_one_frame) return advance_cursor(box, 3);
	if (box.phase == DialogBoxPhase::acknowledgement_delay_two_frames)
		box.phase = DialogBoxPhase::acknowledgement_second_sample;
	return box.phase;
}

DialogBoxPhase dialog_box_acknowledgement(DialogState &dialog,
	DialogBoxState &box, bool pressed) noexcept
{
	if (box.phase == DialogBoxPhase::acknowledgement_first_sample) {
		box.acknowledgement_pair_seen = pressed;
		return box.phase = DialogBoxPhase::acknowledgement_delay_two_frames;
	}
	if (box.phase != DialogBoxPhase::acknowledgement_second_sample) return box.phase;
	const bool seen = box.acknowledgement_pair_seen || pressed;
	if (box.acknowledgement_released) {
		if (seen) {
			dialog.box_current = static_cast<std::uint8_t>(dialog.box_current + 1U);
			return box.phase = DialogBoxPhase::complete;
		}
	} else if (!seen) {
		box.acknowledgement_released = true;
	}
	return box.phase = DialogBoxPhase::acknowledgement_first_sample;
}

DialogSlidePhase dialog_pre_begin(Stage1State &state, DialogState &dialog,
	DialogSlideState &slide, DialogSlidePresentation &presentation) noexcept
{
	presentation.overlay_wipe();
	const auto events = score_grant_current_delta_as_bonus(state.score);
	if (!events) return slide.phase = DialogSlidePhase::score_refused;
	presentation.score_events(*events);
	presentation.graph_scrollup(state.scroll.line);
	state.bomb_animation.palette_tone = 100;
	presentation.palette_show();
	presentation.page_access(state.frame.page_front);
	presentation.overlay_wipe();
	dialog.restore_tile_mode_none_at_post = state.tile_mode == 2U;
	if (dialog.restore_tile_mode_none_at_post) state.tile_mode = 0;
	slide_begin(state, slide, false);
	return slide.phase;
}

DialogSlidePhase dialog_post_begin(const Stage1State &state,
	DialogSlideState &slide, DialogSlidePresentation &presentation) noexcept
{
	presentation.overlay_wipe();
	slide_begin(state, slide, true);
	return slide.phase;
}

DialogSlidePhase dialog_slide_after_delay(Stage1State &state, DialogState &dialog,
	DialogSlideState &slide, DialogSlidePresentation &presentation) noexcept
{
	if (slide.phase != DialogSlidePhase::delay_one_frame) return slide.phase;
	presentation.egc_begin();
	presentation.box_area_invalidate();
	presentation.tiles_render(state);
	state.tiles_egc_render_all = false;
	presentation.egc_end();
	presentation.player_put(state);
	presentation.box_put(slide.left, slide.top, slide.bottom);
	const bool again = slide.post ? (word_add(slide.left, 384) >= 48) : (slide.left > 32);
	if (again) {
		slide.left = word_add(slide.left, static_cast<std::uint16_t>(-16));
		return slide.phase;
	}
	if (slide.post) {
		if (dialog.restore_tile_mode_none_at_post) {
			dialog.restore_tile_mode_none_at_post = false;
			state.tile_mode = 2;
		}
		presentation.page_access(state.frame.page_back);
	}
	return slide.phase = DialogSlidePhase::complete;
}

} // namespace th02::core
