#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "th02/core/stage_state_fwd.hpp"

namespace th02::core {

struct ScoreEvents;

struct DialogState {
	std::array<std::uint8_t, 64U * 2U * 40U> text{};
	std::uint8_t box_current{};
	bool restore_tile_mode_none_at_post{};
};

// The source overwrites only the bytes read, retaining a short file's suffix.
// Oversized input is refused without reproducing the DOS buffer overflow.
[[nodiscard]] bool dialog_load_and_init(DialogState &state,
	std::span<const std::uint8_t> bytes) noexcept;

enum class DialogBoxPhase : std::uint8_t {
	idle, input_sample, delay_one_frame, acknowledgement_first_sample,
	acknowledgement_delay_two_frames, acknowledgement_second_sample,
	complete, invalid_text_reference,
};

// Locals/continuation of dialog_box_animate_and_advance, not gameplay ticks.
// Fast typing can request another input sample without waiting for a frame.
struct DialogBoxState {
	DialogBoxPhase phase{DialogBoxPhase::idle};
	std::int16_t cursor{};
	std::int16_t loop_count{};
	std::int16_t face{};
	std::uint8_t box{};
	bool acknowledgement_released{};
	bool acknowledgement_pair_seen{};
};

class DialogBoxPresentation {
public:
	virtual ~DialogBoxPresentation() = default;
	virtual void box_wipe() noexcept = 0;
	virtual void face_put(std::int16_t face) noexcept = 0;
	// Prefix bytes as supplied to the source's bounded-copy text routine.
	// A renderer still honors embedded NUL and the original character encoding.
	virtual void text_put(std::uint8_t line, std::span<const std::uint8_t> prefix) noexcept = 0;
};

void dialog_box_begin(const DialogState &dialog, DialogBoxState &box,
	std::int16_t face, DialogBoxPresentation &presentation) noexcept;
[[nodiscard]] DialogBoxPhase dialog_box_input(const DialogState &dialog,
	DialogBoxState &box, std::uint16_t input, DialogBoxPresentation &presentation) noexcept;
// Complete exactly the delay requested by the current phase. Other phases
// are unchanged; this does not silently poll input or advance gameplay.
[[nodiscard]] DialogBoxPhase dialog_box_after_delay(DialogBoxState &box) noexcept;
// `pressed` is the OR of the source's three raw keyboard groups, not the
// gameplay mask. Two such samples separated by delay2 form one sense result.
// This deliberately cannot modify the caller's gameplay key_det value.
[[nodiscard]] DialogBoxPhase dialog_box_acknowledgement(DialogState &dialog,
	DialogBoxState &box, bool pressed) noexcept;

enum class DialogSlidePhase : std::uint8_t { idle, delay_one_frame, complete, score_refused };
struct DialogSlideState {
	DialogSlidePhase phase{DialogSlidePhase::idle};
	std::int16_t left{};
	std::int16_t top{};
	std::int16_t bottom{};
	bool post{};
};

class DialogSlidePresentation {
public:
	virtual ~DialogSlidePresentation() = default;
	virtual void overlay_wipe() noexcept = 0;
	virtual void score_events(const ScoreEvents &events) noexcept = 0;
	virtual void graph_scrollup(std::int16_t line) noexcept = 0;
	virtual void palette_show() noexcept = 0;
	virtual void page_access(std::size_t page) noexcept = 0;
	virtual void egc_begin() noexcept = 0;
	virtual void box_area_invalidate() noexcept = 0;
	virtual void tiles_render(const StageState &state) noexcept = 0;
	virtual void egc_end() noexcept = 0;
	virtual void player_put(const StageState &state) noexcept = 0;
	virtual void box_put(std::int16_t left, std::int16_t top, std::int16_t bottom) noexcept = 0;
};

// Actual dialog_pre/dialog_post state and delay schedule. Drawing requests
// are not a pixel-fidelity implementation. Pre grants (but does not clear)
// score_delta; post restores TM_NONE only when pre requested it.
[[nodiscard]] DialogSlidePhase dialog_pre_begin(StageState &state, DialogState &dialog,
	DialogSlideState &slide, DialogSlidePresentation &presentation) noexcept;
[[nodiscard]] DialogSlidePhase dialog_post_begin(const StageState &state,
	DialogSlideState &slide, DialogSlidePresentation &presentation) noexcept;
[[nodiscard]] DialogSlidePhase dialog_slide_after_delay(StageState &state,
	DialogState &dialog, DialogSlideState &slide, DialogSlidePresentation &presentation) noexcept;

} // namespace th02::core
