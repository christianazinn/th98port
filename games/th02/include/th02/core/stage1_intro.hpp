#pragma once

#include "th02/core/dialog.hpp"

#include <string_view>

namespace th02::core {

enum class Stage1IntroPhase : std::uint8_t { idle, pre_slide, boxes, post_slide, complete, refused };
enum class Stage1IntroWait : std::uint8_t {
	idle, delay_one_frame, input_sample, acknowledgement_first_sample,
	delay_two_frames, acknowledgement_second_sample, complete, refused,
};
struct Stage1IntroState {
	bool stones{};
	Stage1IntroPhase phase{Stage1IntroPhase::idle};
	DialogSlideState slide{};
	DialogBoxState box{};
	std::uint8_t sequence_index{};
};

class Stage1IntroPresentation : public DialogSlidePresentation, public DialogBoxPresentation {
public:
	virtual void music_stop() noexcept = 0;
	virtual void music_load(std::string_view filename) noexcept = 0;
	virtual void music_play() noexcept = 0;
};

[[nodiscard]] Stage1IntroWait stage1_intro_wait(const Stage1IntroState &intro) noexcept;
// Actual rika_init sequence: BGM, dialog_pre, all16 generic boxes, dialog_post,
// then the real combat setup suffix. The source dialog cursor is not reset.
[[nodiscard]] Stage1IntroWait stage1_intro_begin(Stage1State &state,
	Stage1IntroState &intro, Stage1IntroPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait stage3_intro_begin(StageState &state,
	Stage1IntroState &intro, Stage1IntroPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait stage1_intro_after_delay(Stage1State &state,
	Stage1IntroState &intro, Stage1IntroPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait stage1_intro_input(Stage1State &state,
	Stage1IntroState &intro, std::uint16_t input, Stage1IntroPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait stage1_intro_acknowledgement(Stage1State &state,
	Stage1IntroState &intro, bool pressed, Stage1IntroPresentation &presentation) noexcept;

} // namespace th02::core
