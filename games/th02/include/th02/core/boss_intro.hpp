#pragma once
#include "th02/core/stage2_intro.hpp"
#include "th02/core/bullet_lifecycle.hpp"

namespace th02::core {
enum class BossIntroKind : std::uint8_t { marisa, mima, sigma };
enum class BossIntroPhase : std::uint8_t {
	idle, reset_wait, pre_slide, boxes, entrance_wait, rings, portrait_fade,
	white_out, white_in, post_slide, complete, refused,
};
enum class BossIntroScene : std::uint8_t {
	clear_page, load_portrait, ring_put, ring_erase, portrait_and_player,
	load_marisa, marisa_and_player, background_rows, copy_and_show,
	load_mima, mima_and_player, mima_side_columns,
	load_sigma, sigma_put,
};
struct BossIntroState {
	BossIntroKind kind{};
	BossIntroPhase phase{};
	DialogSlideState slide{};
	DialogBoxState box{};
	PaletteWhiteFadeState fade{};
	std::uint8_t sequence_index{};
	std::uint16_t remaining{};
	std::int16_t radius{};
};
class BossIntroPresentation : public Stage2IntroPresentation, public EnemyBulletInvalidationCallbacks {
public:
	void invalidate_bullet(EnemyBulletInvalidation) noexcept override {}
	virtual void scene(BossIntroScene scene, std::int16_t value, const StageState &state) noexcept = 0;
	virtual void sound_effect(std::uint8_t sound) noexcept = 0;
	virtual void sound_effects_update() noexcept = 0;
};
[[nodiscard]] Stage1IntroWait boss_intro_wait(const BossIntroState &intro) noexcept;
[[nodiscard]] Stage1IntroWait boss_intro_begin(StageState &state, BossIntroState &intro,
	BossIntroPresentation &presentation, BossIntroKind kind = BossIntroKind::marisa) noexcept;
[[nodiscard]] Stage1IntroWait boss_intro_after_delay(StageState &state, BossIntroState &intro,
	BossIntroPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait boss_intro_input(StageState &state, BossIntroState &intro,
	std::uint16_t input, BossIntroPresentation &presentation) noexcept;
[[nodiscard]] Stage1IntroWait boss_intro_acknowledgement(StageState &state, BossIntroState &intro,
	bool pressed, BossIntroPresentation &presentation) noexcept;
} // namespace th02::core
