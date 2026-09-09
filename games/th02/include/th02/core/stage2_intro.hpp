#pragma once

#include "th02/core/stage1_intro.hpp"
#include "th02/core/palette_white_fade.hpp"

namespace th02::core {

enum class Stage2IntroPhase : std::uint8_t {
	idle, pre_slide, entrance_boxes, white_out, white_in, boxes, post_slide, complete, refused,
};
struct Stage2IntroState {
	Stage2IntroPhase phase{};
	DialogSlideState slide{};
	DialogBoxState box{};
	PaletteWhiteFadeState fade{};
	std::uint8_t sequence_index{};
};
class Stage2IntroPresentation : public Stage1IntroPresentation, public PaletteWhiteFadePresentation {
public:
	void palette_show() noexcept override = 0;
	virtual void boss_put(std::int16_t left, std::int16_t vram_y, std::int16_t patnum) noexcept = 0;
};

[[nodiscard]] Stage1IntroWait stage2_intro_wait(const Stage2IntroState &intro) noexcept;
[[nodiscard]] Stage1IntroWait stage2_intro_begin(StageState &state, Stage2IntroState &intro,
	Stage2IntroPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait stage2_intro_after_delay(StageState &state, Stage2IntroState &intro,
	Stage2IntroPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait stage2_intro_input(StageState &state, Stage2IntroState &intro,
	std::uint16_t input, Stage2IntroPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait stage2_intro_acknowledgement(StageState &state, Stage2IntroState &intro,
	bool pressed, Stage2IntroPresentation &presentation) noexcept;

} // namespace th02::core
