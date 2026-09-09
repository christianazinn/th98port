#pragma once
#include "th02/core/stage2_intro.hpp"

namespace th02::core {
enum class Stage4IntroPhase : std::uint8_t {
	idle, reset_wait, pre_slide, boxes, entrance_wait, rings, portrait_fade,
	white_out, white_in, complete, refused,
};
enum class BossIntroScene : std::uint8_t {
	clear_page, load_portrait, ring_put, ring_erase, portrait_and_player,
	load_marisa, marisa_and_player, background_rows, copy_and_show,
};
struct Stage4IntroState {
	Stage4IntroPhase phase{};
	DialogSlideState slide{};
	DialogBoxState box{};
	PaletteWhiteFadeState fade{};
	std::uint8_t sequence_index{};
	std::uint16_t remaining{};
	std::int16_t radius{};
};
class Stage4IntroPresentation : public Stage2IntroPresentation {
public:
	virtual void scene(BossIntroScene scene, std::int16_t value, const StageState &state) noexcept = 0;
	virtual void sound_effect(std::uint8_t sound) noexcept = 0;
	virtual void sound_effects_update() noexcept = 0;
};
[[nodiscard]] Stage1IntroWait stage4_intro_wait(const Stage4IntroState &intro) noexcept;
[[nodiscard]] Stage1IntroWait stage4_intro_begin(StageState &state, Stage4IntroState &intro,
	Stage4IntroPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait stage4_intro_after_delay(StageState &state, Stage4IntroState &intro,
	Stage4IntroPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait stage4_intro_input(StageState &state, Stage4IntroState &intro,
	std::uint16_t input, Stage4IntroPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait stage4_intro_acknowledgement(StageState &state, Stage4IntroState &intro,
	bool pressed, Stage4IntroPresentation &presentation) noexcept;
} // namespace th02::core
