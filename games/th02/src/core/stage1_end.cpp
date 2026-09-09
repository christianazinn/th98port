#include "th02/core/stage1_end.hpp"
#include "th02/core/stage1_state.hpp"
#include "th02/core/stage1_lifecycle.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <span>

namespace th02::core {
namespace {
constexpr std::array<std::uint8_t, 8> faces{99, 99, 57, 96, 3, 99, 9, 96};
constexpr std::array<std::uint8_t, 11> meira_faces{9, 54, 57, 9, 96, 150, 57, 96, 54, 96, 12};
constexpr std::array<std::uint8_t, 3> stones_faces{12, 96, 12};
constexpr std::array<std::uint8_t, 12> marisa_continued_faces{147, 0, 96, 0, 96, 54, 144, 144, 12, 96, 57, 144};
constexpr std::array<std::uint8_t, 9> marisa_faces{147, 0, 96, 0, 96, 96, 57, 96, 57};
constexpr std::array<std::uint8_t, 6> sigma_faces{9, 156, 9, 96, 96, 12};

void acknowledgement_begin(Stage1EndState &end) noexcept
{
	end.acknowledgement = Stage1EndWait::acknowledgement_first_sample;
	end.released = false;
	end.pair_seen = false;
}

void overlay_put(const Stage1EndState &end, Stage1EndPresentation &presentation) noexcept
{
	for (std::uint8_t x = 0; x < 24; ++x) {
		for (std::uint8_t y = 1; y < 24; ++y) {
			const auto cel = std::clamp(static_cast<int>(end.overlay_frame) - (x >> 1), 0, 8);
			presentation.leave_cell(static_cast<std::uint8_t>(4 + x * 2), y,
				static_cast<std::uint8_t>(cel));
		}
	}
}

void bonus_begin(Stage1State &state, Stage1EndState &end,
	Stage1EndPresentation &presentation) noexcept
{
	presentation.overlay_wipe();
	state.bomb_animation.palette_tone = 62;
	presentation.palette_show();
	presentation.bonus_title();
	if (end.boss == StageEndBoss::sigma) stage_extra_clear_bonus_calculate(state.player_miss,
		state.bomb, state.item_lifecycle, state.sigma.frame, state.stage_clear_bonus);
	else stage_clear_bonus_calculate(state.rank, state.player_miss, state.bomb,
		state.item_lifecycle, state.start_lives, state.stage_clear_bonus);
	for (std::uint8_t row = 0; row < (end.boss == StageEndBoss::sigma ? 4 : 6); ++row)
		presentation.bonus_row(row, state.stage_clear_bonus.row_values_x10[row]);
	presentation.bonus_total(state.stage_clear_bonus, state.item_lifecycle.point_items_collected);
	state.score.score_delta = std::bit_cast<std::int32_t>(
		static_cast<std::uint32_t>(state.score.score_delta) +
		static_cast<std::uint32_t>(state.stage_clear_bonus.bonus_delta));
	presentation.sound_effects_reset();
	const auto events = score_grant_current_delta_as_bonus(state.score);
	if (!events) { end.phase = Stage1EndPhase::refused; return; }
	state.stage_clear_bonus.score_events = *events;
	presentation.score_events(*events);
	presentation.sound_effects_update();
	state.input = 1;
	end.phase = Stage1EndPhase::bonus_acknowledgement;
	acknowledgement_begin(end);
}

Stage1EndWait advance(Stage1State &state, Stage1EndState &end,
	Stage1EndPresentation &presentation) noexcept
{
	const std::span<const std::uint8_t> sequence = end.boss == StageEndBoss::sigma ? std::span<const std::uint8_t>(sigma_faces) : end.boss == StageEndBoss::marisa ?
		(state.continues_used != 0 ? std::span<const std::uint8_t>(marisa_continued_faces) : std::span<const std::uint8_t>(marisa_faces)) : end.boss == StageEndBoss::rika ?
		std::span<const std::uint8_t>(faces) : end.boss == StageEndBoss::meira ?
		std::span<const std::uint8_t>(meira_faces) : std::span<const std::uint8_t>(stones_faces);
	if (end.phase == Stage1EndPhase::pre_slide && end.slide.phase == DialogSlidePhase::complete) {
		dialog_box_begin(state.dialog, end.box, sequence[0], presentation);
		end.phase = Stage1EndPhase::boxes;
	} else if (end.phase == Stage1EndPhase::boxes && end.box.phase == DialogBoxPhase::complete) {
		++end.sequence_index;
		if (end.boss == StageEndBoss::marisa) {
			if (end.sequence_index == 4 && state.continues_used == 0)
				state.dialog.box_current = static_cast<std::uint8_t>(state.dialog.box_current + 8);
			if (end.sequence_index == 7 && state.continues_used != 0) {
				const auto offset = static_cast<std::size_t>(state.dialog.box_current) * 80;
				const auto tens = state.continues_used / 10;
				if (offset + 11 >= state.dialog.text.size() || tens >= 10) {
					end.phase = Stage1EndPhase::refused;
					return Stage1EndWait::refused;
				}
				if (tens != 0) {
					state.dialog.text[offset + 8] = 0x82;
					state.dialog.text[offset + 9] = static_cast<std::uint8_t>(0x4F + tens);
				}
				state.dialog.text[offset + 10] = 0x82;
				state.dialog.text[offset + 11] = static_cast<std::uint8_t>(0x4F + state.continues_used % 10);
			}
		}
		if (end.sequence_index < sequence.size())
			dialog_box_begin(state.dialog, end.box, sequence[end.sequence_index], presentation);
		else bonus_begin(state, end, presentation);
	}
	if (end.slide.phase == DialogSlidePhase::score_refused ||
		end.box.phase == DialogBoxPhase::invalid_text_reference) end.phase = Stage1EndPhase::refused;
	return stage1_end_wait(end);
}
} // namespace

Stage1EndWait stage1_end_wait(const Stage1EndState &end) noexcept
{
	switch (end.phase) {
	case Stage1EndPhase::idle: return Stage1EndWait::idle;
	case Stage1EndPhase::complete: return Stage1EndWait::complete;
	case Stage1EndPhase::refused: return Stage1EndWait::refused;
	case Stage1EndPhase::pre_slide: return Stage1EndWait::delay_one_frame;
	case Stage1EndPhase::leave_overlay: return Stage1EndWait::delay_two_frames;
	case Stage1EndPhase::bonus_acknowledgement:
	case Stage1EndPhase::end_acknowledgement: return end.acknowledgement;
	case Stage1EndPhase::boxes:
		switch (end.box.phase) {
		case DialogBoxPhase::input_sample: return Stage1EndWait::input_sample;
		case DialogBoxPhase::delay_one_frame: return Stage1EndWait::delay_one_frame;
		case DialogBoxPhase::acknowledgement_first_sample: return Stage1EndWait::acknowledgement_first_sample;
		case DialogBoxPhase::acknowledgement_delay_two_frames: return Stage1EndWait::delay_two_frames;
		case DialogBoxPhase::acknowledgement_second_sample: return Stage1EndWait::acknowledgement_second_sample;
		default: return Stage1EndWait::refused;
		}
	}
	return Stage1EndWait::refused;
}

Stage1EndWait stage1_end_begin(Stage1State &state, Stage1EndState &end,
	Stage1EndPresentation &presentation) noexcept
{
	end = {};
	end.phase = Stage1EndPhase::pre_slide;
	(void)dialog_pre_begin(state, state.dialog, end.slide, presentation);
	return advance(state, end, presentation);
}

Stage1EndWait stage4_end_begin(StageState &state, Stage1EndState &end,
	Stage1EndPresentation &presentation) noexcept
{
	end = {};
	end.boss = StageEndBoss::marisa;
	end.phase = Stage1EndPhase::pre_slide;
	(void)dialog_pre_begin(state, state.dialog, end.slide, presentation);
	return advance(state, end, presentation);
}

Stage1EndWait stage1_end_after_delay(Stage1State &state, Stage1EndState &end,
	Stage1EndPresentation &presentation) noexcept
{
	if (end.phase == Stage1EndPhase::pre_slide)
		(void)dialog_slide_after_delay(state, state.dialog, end.slide, presentation);
	else if (end.phase == Stage1EndPhase::boxes) (void)dialog_box_after_delay(end.box);
	else if (end.phase == Stage1EndPhase::bonus_acknowledgement || end.phase == Stage1EndPhase::end_acknowledgement) {
		if (end.acknowledgement == Stage1EndWait::delay_two_frames)
			end.acknowledgement = Stage1EndWait::acknowledgement_second_sample;
	} else if (end.phase == Stage1EndPhase::leave_overlay) {
		if (++end.overlay_frame == 21) {
			if (end.boss == StageEndBoss::rika)
				rika_stage_leave_finish(state.stage_id, state.spark_parameters, state.stage_bg_flash);
			else {
				state.stage_id = static_cast<std::uint8_t>(state.stage_id + 1);
				if (end.boss == StageEndBoss::meira) state.spark_parameters.acceleration_x = 0;
			}
			end.phase = Stage1EndPhase::complete;
		} else overlay_put(end, presentation);
	}
	return advance(state, end, presentation);
}

Stage1EndWait stage_extra_end_begin(StageState &state, Stage1EndState &end,
	Stage1EndPresentation &presentation) noexcept
{
	end = {}; end.boss = StageEndBoss::sigma; end.phase = Stage1EndPhase::pre_slide;
	(void)dialog_pre_begin(state, state.dialog, end.slide, presentation);
	return advance(state, end, presentation);
}

Stage1EndWait stage2_end_begin(StageState &state, Stage1EndState &end,
	Stage1EndPresentation &presentation) noexcept
{
	end = {};
	end.boss = StageEndBoss::meira;
	end.phase = Stage1EndPhase::pre_slide;
	(void)dialog_pre_begin(state, state.dialog, end.slide, presentation);
	return advance(state, end, presentation);
}

Stage1EndWait stage3_end_begin(StageState &state, Stage1EndState &end,
	Stage1EndPresentation &presentation) noexcept
{
	end = {};
	end.boss = StageEndBoss::stones;
	end.phase = Stage1EndPhase::pre_slide;
	(void)dialog_pre_begin(state, state.dialog, end.slide, presentation);
	return advance(state, end, presentation);
}

Stage1EndWait stage1_end_input(Stage1State &state, Stage1EndState &end,
	std::uint16_t input, Stage1EndPresentation &presentation) noexcept
{
	if (end.phase == Stage1EndPhase::boxes && end.box.phase == DialogBoxPhase::input_sample) {
		state.input = input;
		(void)dialog_box_input(state.dialog, end.box, input, presentation);
	}
	return advance(state, end, presentation);
}

Stage1EndWait stage1_end_acknowledgement(Stage1State &state, Stage1EndState &end,
	bool pressed, Stage1EndPresentation &presentation) noexcept
{
	if (end.phase == Stage1EndPhase::boxes)
		(void)dialog_box_acknowledgement(state.dialog, end.box, pressed);
	else if (end.phase == Stage1EndPhase::bonus_acknowledgement || end.phase == Stage1EndPhase::end_acknowledgement) {
		if (end.acknowledgement == Stage1EndWait::acknowledgement_first_sample) {
			end.pair_seen = pressed;
			end.acknowledgement = Stage1EndWait::delay_two_frames;
		} else if (end.acknowledgement == Stage1EndWait::acknowledgement_second_sample) {
			const bool seen = end.pair_seen || pressed;
			end.acknowledgement = Stage1EndWait::acknowledgement_first_sample;
			if (end.released && seen) {
				if (end.phase == Stage1EndPhase::bonus_acknowledgement &&
					(end.boss == StageEndBoss::rika || end.boss == StageEndBoss::meira || end.boss == StageEndBoss::sigma)) {
					end.phase = Stage1EndPhase::end_acknowledgement;
					acknowledgement_begin(end);
				} else if (end.boss == StageEndBoss::sigma) {
					end.resident_stage = 127;
					end.resident_score_with_continues = std::bit_cast<std::int32_t>(
						std::bit_cast<std::uint32_t>(state.score.score) * 10U + state.continues_used);
					end.maine = true; end.phase = Stage1EndPhase::complete;
				} else {
					end.phase = Stage1EndPhase::leave_overlay;
					end.overlay_frame = 0;
					overlay_put(end, presentation);
				}
			} else if (!seen) end.released = true;
		}
	}
	return advance(state, end, presentation);
}
} // namespace th02::core
