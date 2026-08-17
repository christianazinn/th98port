#ifndef TH03_PRESENT_MAIN_ROLLBACK_HPP
#define TH03_PRESENT_MAIN_ROLLBACK_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "th03/core/observer.hpp"
#include "th03/present/main_background.hpp"
#include "th03/present/main_bomb_palette.hpp"
#include "th03/present/main_bombs.hpp"
#include "th03/present/main_boss.hpp"
#include "th03/present/main_effects.hpp"
#include "th03/present/main_entities.hpp"
#include "th03/present/main_extra_charge.hpp"
#include "th03/present/main_hud_result.hpp"
#include "th03/present/main_palette.hpp"
#include "th03/present/main_players_bullets.hpp"
#include "th03/present/main_round.hpp"
#include "th03/present/main_static.hpp"
#include "th03/present/main_warning_gauge.hpp"

namespace th03::present {

struct MainRollbackBackgroundRecord final {
	std::int16_t x_q12_4 = 0;
	std::int16_t y_q12_4 = 0;
	std::int16_t velocity_x_q12_4 = 0;
	std::int16_t velocity_y_q12_4 = 0;
	std::uint8_t angle = 0U;
	std::uint8_t sprite_variant = 0U;
	friend bool operator==(
		const MainRollbackBackgroundRecord &,
		const MainRollbackBackgroundRecord &
	) = default;
};

struct MainRollbackBossActivation final {
	bool active = false;
	PlayerId owner = PlayerId::p1;
	std::int8_t paletted_character = 0;
	std::uint8_t character = 0U;
	friend bool operator==(
		const MainRollbackBossActivation &,
		const MainRollbackBossActivation &
	) = default;
};

struct MainRollbackHitCircle final {
	std::uint8_t age = 0U;
	PlayerId player = PlayerId::p1;
	std::int16_t left = 0;
	std::int16_t top = 0;
	friend bool operator==(
		const MainRollbackHitCircle &,
		const MainRollbackHitCircle &
	) = default;
};

struct MainRollbackCommonEffect final {
	std::uint8_t type = 0U;
	std::uint8_t age = 0U;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	std::uint8_t radius = 0U;
	std::int8_t radius_delta = 0;
	std::uint8_t angle = 0U;
	friend bool operator==(
		const MainRollbackCommonEffect &,
		const MainRollbackCommonEffect &
	) = default;
};

struct MainRollbackDefeat final {
	std::int16_t anchor_left = 0;
	std::int16_t anchor_top = 0;
	std::int16_t radius = 0;
	friend bool operator==(
		const MainRollbackDefeat &,
		const MainRollbackDefeat &
	) = default;
};

struct MainRollbackBulletStyle final {
	PlayerId source_player = PlayerId::p1;
	Bullet16ConstructionKind construction =
		Bullet16ConstructionKind::default_attack;
	bool animated = true;
	bool observed = false;
	friend bool operator==(
		const MainRollbackBulletStyle &,
		const MainRollbackBulletStyle &
	) = default;
};

struct MainRollbackBombActivation final {
	bool active = false;
	std::int8_t paletted_character = 0;
	std::uint8_t character = 0U;
	std::uint8_t frame = 0U;
	std::int16_t entry_shift = 0;
	friend bool operator==(
		const MainRollbackBombActivation &,
		const MainRollbackBombActivation &
	) = default;
};

struct MainRollbackBombPending final {
	std::uint8_t kind = 0U;
	std::uint8_t player = 0U;
	std::uint8_t frame = 0U;
	std::uint8_t received = 0U;
	friend bool operator==(
		const MainRollbackBombPending &,
		const MainRollbackBombPending &
	) = default;
};

struct MainRollbackReimuRecord final {
	bool active = false;
	std::int16_t x = 0;
	std::int16_t y = 0;
	std::int16_t velocity_y = 0;
	friend bool operator==(
		const MainRollbackReimuRecord &,
		const MainRollbackReimuRecord &
	) = default;
};

struct MainRollbackReimuRead final {
	std::uint8_t phase = 0U;
	std::uint8_t frame = 0U;
	std::int16_t velocity_y = 0;
	friend bool operator==(
		const MainRollbackReimuRead &,
		const MainRollbackReimuRead &
	) = default;
};

struct MainRollbackMimaColumn final {
	bool active = false;
	std::int16_t x = 0;
	friend bool operator==(
		const MainRollbackMimaColumn &,
		const MainRollbackMimaColumn &
	) = default;
};

struct MainRollbackEllenVector final {
	std::int16_t x = 19999;
	std::int16_t y = 0;
	std::int16_t velocity_x = 0;
	std::int16_t velocity_y = 0;
	friend bool operator==(
		const MainRollbackEllenVector &,
		const MainRollbackEllenVector &
	) = default;
};

struct MainRollbackEllenPrefix final {
	bool active = false;
	std::uint8_t frame = 0U;
	std::uint8_t next_slot = 0U;
	friend bool operator==(
		const MainRollbackEllenPrefix &,
		const MainRollbackEllenPrefix &
	) = default;
};

struct MainRollbackMarisaPoint final {
	bool initialized = false;
	std::int16_t screen_x = 0;
	std::int16_t screen_y = 0;
	friend bool operator==(
		const MainRollbackMarisaPoint &,
		const MainRollbackMarisaPoint &
	) = default;
};

struct MainRollbackHudBanner final {
	std::int16_t x = 0;
	std::int16_t y = 0;
	std::int16_t velocity = 0;
	std::uint8_t active = 0U;
	friend bool operator==(
		const MainRollbackHudBanner &,
		const MainRollbackHudBanner &
	) = default;
};

struct MainRollbackHudParticle final {
	std::int16_t x_q4 = 0;
	std::int16_t y_q4 = 0;
	std::uint8_t angle = 0U;
	std::int8_t angle_delta_first = 0;
	std::int8_t angle_delta_second = 0;
	std::uint8_t active = 0U;
	friend bool operator==(
		const MainRollbackHudParticle &,
		const MainRollbackHudParticle &
	) = default;
};

// Fixed-capacity typed values only. No member is an asset, pointer, span,
// vector, host-size-dependent counter, or object image. A later enclosing
// MainProcess image serializes these fields canonically.
struct MainPresenterRollbackState final {
	Palette16Rgb8 palette_baseline{};
	bool palette_baseline_ready = false;
	bool palette_dirty = false;
	MainPageRoles page_roles{};
	bool static_started = false;
	bool accelerator_roles_crossed = false;

	std::array<MainRollbackBackgroundRecord, main_background_record_count>
		background{};
	MainBackgroundKind background_kind = MainBackgroundKind::type_a;
	std::uint8_t background_read_stream = 0U;
	std::uint32_t background_read_slot = 0U;
	std::uint8_t background_read_field = 0U;
	std::uint16_t background_initial_frames_completed = 0U;
	std::uint8_t background_magnitude = 0U;
	std::uint8_t background_round_id = 0U;
	bool background_initialized = false;
	bool background_have_round_id = false;
	bool background_vector_latch = false;
	bool background_rendering_enabled = true;
	MainBackgroundStatus background_status = MainBackgroundStatus::ready;

	MainRollbackBossActivation boss_activation{};
	std::uint8_t boss_ellen_phase = 0U;
	std::uint8_t boss_rikako_angle = 0U;
	std::int8_t boss_rikako_angle_delta = 0;
	std::uint8_t boss_chiyuri_phase = 0U;
	MainBossStatus boss_status = MainBossStatus::ready;

	std::array<std::uint8_t, ShotsEnemiesView::shot_count> entity_shot_cels{};
	std::array<PlayerId, ShotsEnemiesView::shot_count> entity_shot_players{};
	std::array<bool, ShotsEnemiesView::shot_count> entity_shot_spawn_observed{};
	MainEntitiesStatus entities_status = MainEntitiesStatus::ready;

	std::array<MainRollbackHitCircle, main_hit_circle_count> hit_circles{};
	std::array<MainRollbackCommonEffect, main_common_effect_count> common_effects{};
	std::uint8_t enemy_hit_circle_cursor = 0U;
	std::uint8_t common_effect_cursor = 0U;
	std::uint8_t kana_bomb_angle = 0U;
	std::int16_t kotohime_bomb_y = 0;
	MainEffectsStatus effects_status = MainEffectsStatus::ready;

	std::array<std::array<std::uint8_t, 8U>, 2U> kana_destination_cache{};
	MainExtraChargeStatus extra_charge_status = MainExtraChargeStatus::ready;

	std::int8_t shared_velocity_x_q4_4 = 0;
	std::array<std::uint8_t, PlayersBulletsView::player_count> player_poses{};
	MainRollbackDefeat defeat{};
	std::array<MainRollbackBulletStyle, PlayersBulletsView::bullet_count>
		bullet16_styles{};
	MainPlayersBulletsStatus players_bullets_status =
		MainPlayersBulletsStatus::ready;

	MainBombPaletteStatus bomb_palette_status = MainBombPaletteStatus::ready;
	std::array<MainRollbackBombActivation, main_bomb_player_count>
		bomb_activations{};
	MainRollbackBombPending bomb_pending{};
	std::array<
		std::array<MainRollbackReimuRecord, main_reimu_bomb_record_count>,
		main_bomb_player_count
	> reimu{};
	std::array<MainRollbackReimuRead, main_bomb_player_count> reimu_reads{};
	std::array<
		std::array<MainRollbackMimaColumn, main_mima_bomb_column_count>,
		main_bomb_player_count
	> mima{};
	std::array<std::uint8_t, main_bomb_player_count> mima_read_frame{};
	std::array<
		std::array<MainRollbackEllenVector, main_ellen_bomb_vector_count>,
		main_bomb_player_count
	> ellen{};
	std::array<MainRollbackEllenPrefix, main_bomb_player_count> ellen_prefix{};
	std::uint8_t ellen_release_cursor = 0U;
	MainRollbackMarisaPoint marisa_point{};
	std::int16_t rikako_radius = 0;
	MainBombsStatus bombs_status = MainBombsStatus::ready;

	std::array<std::uint8_t, main_warning_player_count> warning_ramp_value{};
	std::array<bool, main_warning_player_count> warning_ramp_decreasing{};
	std::array<std::uint8_t, main_warning_player_count> warning_gauge_level{};
	std::array<bool, main_warning_player_count> warning_level_valid{};
	std::uint8_t warning_boss_level = 0U;
	std::uint8_t warning_next_interstitial_sample = 0U;
	bool warning_interstitial_active = false;
	MainWarningGaugeStatus warning_status = MainWarningGaugeStatus::ready;

	std::array<MainRollbackHudBanner, 16U> hud_banner{};
	std::array<MainRollbackHudParticle, 128U> hud_particles{};
	std::uint16_t hud_compatibility_read_count = 0U;
	std::int16_t hud_banner_x = 0;
	std::uint8_t hud_pattern_base = 0U;
	bool hud_particle_reads_pending = false;
	bool hud_completed_batch_waiting = false;
	bool hud_late_state_pending = false;
	bool hud_expected_story_mode = false;
	std::uint8_t hud_expected_story_stage = 0U;
	std::uint8_t hud_expected_round_id = 0U;
	std::uint8_t hud_expected_state_after = 0U;
	std::uint16_t hud_expected_frame_after = 0U;
	MainResultWinnerStats result_snapshot{};
	std::uint16_t result_initial_remainder = 0U;
	std::uint16_t result_initial_quotient = 0U;
	std::uint8_t result_final_lives = 0U;
	std::array<std::int16_t, 8U> transition_coordinates{};
	MainHudResultStatus hud_status = MainHudResultStatus::ready;

	friend bool operator==(
		const MainPresenterRollbackState &,
		const MainPresenterRollbackState &
	) = default;
};

class MainPresenterRollbackAccess final {
public:
	[[nodiscard]] static bool valid(
		const MainPresenterRollbackState &state
	) noexcept;

	[[nodiscard]] static bool boundary_ready(
		const MainPaletteState &palette,
		const MainStaticPresenter &static_presenter,
		const MainBackgroundPresenter &background,
		const MainBossPresenter &bosses,
		const MainEntitiesPresenter &entities,
		const MainEffectsPresenter &effects,
		const MainExtraChargePresenter &extra_charge,
		const MainPlayersBulletsPresenter &players_bullets,
		const MainBombsPresenter &bombs,
		const MainWarningGaugePresenter &warning_gauge,
		const MainHudResultPresenter &hud_result,
		const MainRoundPresenter &round
	) noexcept;

	static void capture(
		const MainPaletteState &palette,
		const MainStaticPresenter &static_presenter,
		const MainBackgroundPresenter &background,
		const MainBossPresenter &bosses,
		const MainEntitiesPresenter &entities,
		const MainEffectsPresenter &effects,
		const MainExtraChargePresenter &extra_charge,
		const MainPlayersBulletsPresenter &players_bullets,
		const MainBombsPresenter &bombs,
		const MainWarningGaugePresenter &warning_gauge,
		const MainHudResultPresenter &hud_result,
		MainPresenterRollbackState &state
	) noexcept;

	static void restore(
		MainPaletteState &palette,
		MainStaticPresenter &static_presenter,
		MainBackgroundPresenter &background,
		MainBossPresenter &bosses,
		MainEntitiesPresenter &entities,
		MainEffectsPresenter &effects,
		MainExtraChargePresenter &extra_charge,
		MainPlayersBulletsPresenter &players_bullets,
		MainBombsPresenter &bombs,
		MainWarningGaugePresenter &warning_gauge,
		MainHudResultPresenter &hud_result,
		const MainPresenterRollbackState &state
	) noexcept;
};

} // namespace th03::present

#endif
