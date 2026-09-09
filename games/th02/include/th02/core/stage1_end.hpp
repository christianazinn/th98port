#pragma once

#include "th02/core/dialog.hpp"

namespace th02::core {
struct StageClearBonusTally;
enum class StageEndBoss : std::uint8_t { rika, meira, stones, marisa };

enum class Stage1EndPhase : std::uint8_t {
	idle, pre_slide, boxes, bonus_acknowledgement, end_acknowledgement,
	leave_overlay, complete, refused,
};
enum class Stage1EndWait : std::uint8_t {
	idle, delay_one_frame, input_sample, acknowledgement_first_sample,
	delay_two_frames, acknowledgement_second_sample, complete, refused,
};
struct Stage1EndState {
	StageEndBoss boss{StageEndBoss::rika};
	Stage1EndPhase phase{Stage1EndPhase::idle};
	DialogSlideState slide{};
	DialogBoxState box{};
	std::uint8_t sequence_index{};
	Stage1EndWait acknowledgement{Stage1EndWait::acknowledgement_first_sample};
	bool released{};
	bool pair_seen{};
	std::uint8_t overlay_frame{};
};

class Stage1EndPresentation : public DialogSlidePresentation, public DialogBoxPresentation {
public:
	virtual void bonus_title() noexcept = 0;
	virtual void bonus_row(std::uint8_t row, std::int16_t value_x10) noexcept = 0;
	virtual void bonus_total(const StageClearBonusTally &tally, std::int16_t point_items) noexcept = 0;
	virtual void sound_effects_reset() noexcept = 0;
	virtual void sound_effects_update() noexcept = 0;
	// Source TRAM position and fade cel (0..8), rendered in TX_BLACK.
	virtual void leave_cell(std::uint8_t x, std::uint8_t y, std::uint8_t cel) noexcept = 0;
};

// Original Rika ending: no dialog_post. Both independent release/press waits
// and all 21 two-frame overlay steps precede the stage-ID increment.
[[nodiscard]] Stage1EndWait stage1_end_wait(const Stage1EndState &end) noexcept;
[[nodiscard]] Stage1EndWait stage1_end_begin(Stage1State &state,
	Stage1EndState &end, Stage1EndPresentation &presentation) noexcept;
// Meira shares the continuation below, but has 11 boxes and retains flash.frame.
[[nodiscard]] Stage1EndWait stage2_end_begin(StageState &state,
	Stage1EndState &end, Stage1EndPresentation &presentation) noexcept;
[[nodiscard]] Stage1EndWait stage3_end_begin(StageState &state,
	Stage1EndState &end, Stage1EndPresentation &presentation) noexcept;
[[nodiscard]] Stage1EndWait stage4_end_begin(StageState &state,
	Stage1EndState &end, Stage1EndPresentation &presentation) noexcept;
[[nodiscard]] Stage1EndWait stage1_end_after_delay(Stage1State &state,
	Stage1EndState &end, Stage1EndPresentation &presentation) noexcept;
[[nodiscard]] Stage1EndWait stage1_end_input(Stage1State &state,
	Stage1EndState &end, std::uint16_t input, Stage1EndPresentation &presentation) noexcept;
[[nodiscard]] Stage1EndWait stage1_end_acknowledgement(Stage1State &state,
	Stage1EndState &end, bool pressed, Stage1EndPresentation &presentation) noexcept;
} // namespace th02::core
