#ifndef TH03_PRESENT_MAIN_EFFECTS_HPP
#define TH03_PRESENT_MAIN_EFFECTS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "th03/core/observer.hpp"
#include "th03/present/sprite16.hpp"
#include "th03/present/staff_flakes.hpp"

namespace th03::present {

inline constexpr std::size_t main_hit_circle_cel_count = 4U;
inline constexpr std::size_t main_enemy_hit_circle_count = 12U;
inline constexpr std::size_t main_hit_circle_count =
	main_enemy_hit_circle_count + 1U;
inline constexpr std::size_t main_common_effect_count = 12U;

// The four source cels are consecutive animation frames in MAIN's committed
// Sprite16 atlas. Coordinates are injected separately from decoded pixels.
struct MainEffectsAtlasLayout {
	std::array<Sprite16AtlasRect, main_hit_circle_cel_count>
		hit_circle_cels{};
};

[[nodiscard]] MainEffectsAtlasLayout
	authentic_main_effects_atlas_layout() noexcept;

enum class MainEffectsStatus : std::uint8_t {
	ready,
	invalid_layout,
	invalid_assets,
	invalid_observation,
	staging_not_committed,
	draw_failed,
};

// Process-scoped owner of MAIN's two hidden visual pools. Source BSS preserves
// both pools, their increment-before cursors, and the shared Kana/Kotohime Bomb
// values across repeat-round initialization, so round_id changes never reset
// this presenter.
class MainEffectsPresenter final : public GameplayObserver {
public:
	MainEffectsPresenter(
		Raster &raster,
		const Sprite16Staging &staging,
		MainEffectsAtlasLayout layout,
		StaffFlakeTrigTables trig
	) noexcept;

	void hit_circles_advanced() noexcept override;
	void hit_circle_spawned(const HitCircleSpawned &spawn) noexcept override;
	void common_effects_advanced() noexcept override;
	void common_effect_spawned(
		const CommonEffectSpawned &spawn
	) noexcept override;
	void bomb_common_effect_step(
		const BombCommonEffectStep &step
	) noexcept override;

	// These are deliberately not observer phase callbacks. The eventual MAIN
	// composite calls the first pass after Fireballs and the second between
	// MainPlayersBulletsPresenter::present_players() and present_bullets().
	[[nodiscard]] bool present_hit_circles(
		const MainPhaseContext &context
	) noexcept;
	[[nodiscard]] bool present_common_effects(
		const MainPhaseContext &context
	) noexcept;

	[[nodiscard]] MainEffectsStatus status() const noexcept;
	[[nodiscard]] bool ready() const noexcept;

private:
	friend class MainPresenterRollbackAccess;
	struct HitCircleState {
		std::uint8_t age = 0U;
		PlayerId player = PlayerId::p1;
		std::int16_t left = 0;
		std::int16_t top = 0;
	};

	struct CommonEffectState {
		// Exact cee0_rec_t values: 0 is inactive, 1 is a circle, and 2 is
		// a contracting square.
		std::uint8_t type = 0U;
		std::uint8_t age = 0U;
		std::int16_t center_x = 0;
		std::int16_t center_y = 0;
		std::uint8_t radius = 0U;
		std::int8_t radius_delta = 0;
		std::uint8_t angle = 0U;
	};

	void spawn_common_effect(
		CommonEffectKind kind,
		PlayerId player,
		std::int16_t center_x,
		std::int16_t center_y,
		std::int16_t fg_shift_x_at_spawn,
		std::uint8_t initial_angle
	) noexcept;

	Raster *raster_;
	const Sprite16Staging *staging_;
	MainEffectsAtlasLayout layout_{};
	StaffFlakeTrigTables trig_{};
	std::array<HitCircleState, main_hit_circle_count> hit_circles_{};
	std::array<CommonEffectState, main_common_effect_count> common_effects_{};
	std::uint8_t enemy_hit_circle_cursor_ = 0U;
	std::uint8_t common_effect_cursor_ = 0U;
	std::uint8_t kana_bomb_angle_ = 0U;
	std::int16_t kotohime_bomb_y_ = 0;
	MainEffectsStatus status_ = MainEffectsStatus::ready;
};

} // namespace th03::present

#endif
