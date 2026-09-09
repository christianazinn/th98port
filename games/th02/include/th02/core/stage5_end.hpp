#pragma once
#include "th02/core/boss_intro.hpp"
#include "th02/core/bullet_lifecycle.hpp"

namespace th02::core {
enum class Stage5EndPhase : std::uint8_t {
	idle, pre_slide, boxes, post_slide, flash_out, flash_in, wait,
	final_fade, continue_fade, continue_wait, form2_ready, maine, refused,
};
struct Stage5ResidentResult {
	std::int32_t score_highest{};
	std::int32_t score_with_continues{};
	std::int16_t skill{};
	std::uint8_t stage{};
	std::int8_t lives{}, bombs{};
	bool cleared{};
};
struct Stage5EndState {
	Stage5EndPhase phase{};
	bool first_form{};
	DialogSlideState slide{};
	DialogBoxState box{};
	PaletteWhiteFadeState fade{};
	std::uint8_t sequence_index{};
	std::uint8_t script_step{};
	std::uint16_t remaining{};
	Stage5ResidentResult resident{};
};
class Stage5EndPresentation : public BossIntroPresentation {
public:
	virtual void music_fade(std::uint8_t speed) noexcept = 0;
	virtual void sound_effect_force(std::uint8_t sound) noexcept = 0;
	virtual void load_second_form() noexcept = 0;
};
[[nodiscard]] Stage1IntroWait stage5_end_wait(const Stage5EndState &end) noexcept;
// These are blocking call-site continuations inside Mima's update, not a
// stage_clear transaction. Form2 resumes that update; MAINE replaces MAIN.
[[nodiscard]] Stage1IntroWait stage5_end_begin(StageState &state, Stage5EndState &end,
	bool first_form, Stage5EndPresentation &presentation,
	EnemyBulletInvalidationCallbacks &bullets) noexcept;
[[nodiscard]] Stage1IntroWait stage5_end_after_delay(StageState &state, Stage5EndState &end,
	Stage5EndPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait stage5_end_input(StageState &state, Stage5EndState &end,
	std::uint16_t input, Stage5EndPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait stage5_end_acknowledgement(StageState &state, Stage5EndState &end,
	bool pressed, Stage5EndPresentation &presentation) noexcept;
} // namespace th02::core
