#ifndef TH03_PRESENT_MAIN_ROUND_HPP
#define TH03_PRESENT_MAIN_ROUND_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <span>

#include "th03/core/observer.hpp"
#include "th03/present/main_background.hpp"
#include "th03/present/main_bombs.hpp"
#include "th03/present/main_effects.hpp"
#include "th03/present/main_entities.hpp"
#include "th03/present/main_palette.hpp"
#include "th03/present/main_players_bullets.hpp"
#include "th03/present/main_static.hpp"
#include "th03/present/source_surface.hpp"

namespace th03::present {

// Named gaps in this first source-ordered MAIN composition owner. These are
// deliberately explicit rather than approximated from the three ordinary
// phase views.
enum class MainRoundMissingPhase : std::uint8_t {
	boss,
	extra_attack,
	charge_shot,
	warning_and_gauge,
	hud,
	result,
};

inline constexpr std::array<MainRoundMissingPhase, 6U>
	main_round_missing_phases{{
		MainRoundMissingPhase::boss,
		MainRoundMissingPhase::extra_attack,
		MainRoundMissingPhase::charge_shot,
		MainRoundMissingPhase::warning_and_gauge,
		MainRoundMissingPhase::hud,
		MainRoundMissingPhase::result,
	}};

enum class MainRoundStatus : std::uint8_t {
	ready,
	invalid_setup,
	invalid_observation,
	background_failed,
	entities_failed,
	effects_failed,
	players_bullets_failed,
	bombs_failed,
	page_state_mismatch,
	page_transaction_failed,
	repeat_startup_failed,
};

// Process-scoped synchronous coordinator for the currently implemented portion
// of sub_9778()'s presentation schedule. Raster, TRAM, heavy assets, and every
// specialized presenter stay caller-owned at stable addresses for this MAIN
// process; this object preserves their exact call order and one fixed-capacity
// copy of the borrowed final phase. Every injected child must have been built
// over [raster], MainStaticPresenter::sprite_staging(), and [palette] from this
// same process. The caller must not mutate those shared objects outside this
// coordinator's callbacks and lifecycle methods while it is active. The
// decoded BFNT/MRS images and the pellet/trigonometry backing spans referenced
// by those children must likewise retain their documented lifetimes.
//
// Ordinary drawing is published only by MainFrameFooterReached. The footer
// applies the deferred MAIN palette before the static page swap/clear. A
// terminal frame that returns after PlayersBullets has no footer and therefore
// leaves the previously shown page selected, matching the source.
class MainRoundPresenter final : public GameplayObserver {
public:
	MainRoundPresenter(
		Raster &raster,
		Tram &tram,
		MainStaticPresenter &static_presenter,
		MainPaletteState &palette,
		MainBackgroundPresenter &background,
		MainEntitiesPresenter &entities,
		MainEffectsPresenter &effects,
		MainPlayersBulletsPresenter &players_bullets,
		MainBombsPresenter &bombs
	) noexcept;
	MainRoundPresenter(const MainRoundPresenter &) = delete;
	MainRoundPresenter &operator=(const MainRoundPresenter &) = delete;
	MainRoundPresenter(MainRoundPresenter &&) = delete;
	MainRoundPresenter &operator=(MainRoundPresenter &&) = delete;

	void shot_pair_spawned(const ShotPairSpawned &spawn) noexcept override;
	void bullet16_spawned(const Bullet16Spawned &spawn) noexcept override;
	void player_pose_committed(
		const PlayerPoseCommitted &pose
	) noexcept override;
	void player_defeat_step(const PlayerDefeatStep &step) noexcept override;
	void hit_circles_advanced() noexcept override;
	void hit_circle_spawned(const HitCircleSpawned &spawn) noexcept override;
	void common_effects_advanced() noexcept override;
	void common_effect_spawned(
		const CommonEffectSpawned &spawn
	) noexcept override;
	void bomb_compatibility_read(
		const BombCompatibilityRead &read
	) noexcept override;
	void bomb_finished(const BombFinished &finished) noexcept override;
	void bomb_frame_step(const BombFrameStep &step) noexcept override;
	void bomb_common_effect_step(
		const BombCommonEffectStep &step
	) noexcept override;
	void warning_interstitial_started(
		const WarningInterstitialStarted &started
	) noexcept override;
	void warning_interstitial_sample(
		const WarningInterstitialSample &sample
	) noexcept override;
	void main_frame_footer_reached(
		const MainFrameFooterReached &footer
	) noexcept override;
	void main_background_initialize(
		const MainBackgroundInitialize &initialize
	) noexcept override;
	void main_background_frame(
		const MainBackgroundFrame &frame
	) noexcept override;
	void main_background_random_read(
		const MainBackgroundRandomRead &read
	) noexcept override;
	void observe(const ShotsEnemiesView &view) noexcept override;
	void observe(const FireballsView &view) noexcept override;
	void observe(const PlayersBulletsView &view) noexcept override;

	// Called after Game::start_next_round() on the source route-1 path. This
	// performs sub_A21F()'s static reset and entry.cpp's following immediate
	// Tone=100 show while preserving every process-scoped child visual pool.
	[[nodiscard]] bool repeat_round_startup(
		const MainHudState &hud
	) noexcept;

	// Composes the currently shown graphics page and this process's caller-owned
	// TRAM into a caller-owned 640x400 buffer. This never retains [output] or
	// [ank].
	[[nodiscard]] std::expected<SourceSurface, SourceSurfaceError>
	compose_shown_surface(
		std::span<ComposedPixel, graphics_page_pixels> output,
		const AnkGlyphProvider &ank
	) const;

	[[nodiscard]] MainRoundStatus status() const noexcept;
	[[nodiscard]] bool ready() const noexcept;

private:
	enum class Phase : std::uint8_t {
		shots_enemies,
		fireballs,
		players_bullets,
		footer,
	};

	void fail(MainRoundStatus status) noexcept;
	[[nodiscard]] bool source_page_ready() const noexcept;
	[[nodiscard]] bool same_frame(const MainPhaseContext &context) const noexcept;
	[[nodiscard]] static bool present_common_effects(
		void *context,
		const MainPhaseContext &phase
	) noexcept;

	Raster *raster_;
	Tram *tram_;
	MainStaticPresenter *static_;
	MainPaletteState *palette_;
	MainBackgroundPresenter *background_;
	MainEntitiesPresenter *entities_;
	MainEffectsPresenter *effects_;
	MainPlayersBulletsPresenter *players_bullets_;
	MainBombsPresenter *bombs_;
	MainPlayerFacts player_facts_{};
	MainBulletFacts bullet_facts_{};
	MainBulletTrails bullet_trails_{};
	MainPhaseContext frame_context_{};
	Phase phase_ = Phase::shots_enemies;
	std::uint8_t next_warning_sample_ = 0;
	bool warning_interstitial_active_ = false;
	bool warning_suffix_pending_ = false;
	MainRoundStatus status_ = MainRoundStatus::ready;
};

} // namespace th03::present

#endif
