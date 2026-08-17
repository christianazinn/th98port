#ifndef TH03_PRESENT_MAIN_BOSS_HPP
#define TH03_PRESENT_MAIN_BOSS_HPP

#include <array>
#include <cstdint>

#include "th03/core/observer.hpp"
#include "th03/present/main_palette.hpp"
#include "th03/present/sprite16.hpp"
#include "th03/present/staff_flakes.hpp"

namespace th03::present {

enum class MainBossStatus : std::uint8_t {
	ready,
	invalid_assets,
	invalid_observation,
	staging_not_committed,
	draw_failed,
	palette_failed,
};

// Process-scoped reconstruction of MAIN's nine Boss Attack render callbacks.
// Gameplay facts stay borrowed; only the source-private Ellen, Rikako, and
// Chiyuri animation bytes are retained here.
class MainBossPresenter final : public GameplayObserver {
public:
	MainBossPresenter(
		Raster &raster,
		const Sprite16Staging &staging,
		StaffFlakeTrigTables trig,
		MainPaletteState &palette
	) noexcept;

	void boss_activated(const BossActivated &activated) noexcept override;
	void boss_animation_step(const BossAnimationStep &step) noexcept override;
	void boss_palette_baseline_restored(
		const BossPaletteBaselineRestored &restored
	) noexcept override;
	void observe(const MainBossView &view) noexcept override;

	// Fixed-capacity entry point for synthetic tests and trace adapters. Reimu's
	// six source orbit records are supplied in their ascending record order.
	[[nodiscard]] bool present_boss(
		const MainPhaseContext &context,
		bool round_frame_is_odd,
		const MainBossFact &boss,
		const std::array<
			RawPoint,
			MainBossView::reimu_orbit_point_count
		> &reimu_orbit_points
	) noexcept;

	[[nodiscard]] MainBossStatus status() const noexcept;
	[[nodiscard]] bool ready() const noexcept;

private:
	friend class MainPresenterRollbackAccess;
	enum class Character : std::uint8_t {
		reimu,
		mima,
		marisa,
		ellen,
		kotohime,
		kana,
		rikako,
		chiyuri,
		yumemi,
	};

	struct Activation {
		bool active = false;
		PlayerId owner = PlayerId::p1;
		std::int8_t paletted_character = 0;
		Character character = Character::reimu;
	};

	[[nodiscard]] bool fail(MainBossStatus status) noexcept;
	[[nodiscard]] bool put(
		Sprite16AtlasRect source,
		Point position,
		Sprite16HorizontalClip clip,
		Sprite16DrawStyle style = {}
	) noexcept;
	[[nodiscard]] bool set_chiyuri_gray(
		std::uint8_t value,
		std::uint8_t target_player
	) noexcept;
	[[nodiscard]] bool render_active(
		const MainPhaseContext &context,
		const MainBossFact &boss,
		Character character,
		std::uint8_t owner,
		std::uint8_t target,
		Sprite16HorizontalClip clip,
		const std::array<
			RawPoint,
			MainBossView::reimu_orbit_point_count
		> &reimu_orbit_points
	) noexcept;
	[[nodiscard]] bool render_materializing(
		const MainPhaseContext &context,
		bool round_frame_is_odd,
		const MainBossFact &boss,
		Character character,
		std::uint8_t owner,
		std::uint8_t target,
		Sprite16HorizontalClip clip
	) noexcept;
	[[nodiscard]] bool render_terminal(
		const MainPhaseContext &context,
		const MainBossFact &boss,
		std::uint8_t target,
		Sprite16HorizontalClip clip
	) noexcept;

	Raster *raster_;
	const Sprite16Staging *staging_;
	StaffFlakeTrigTables trig_{};
	MainPaletteState *palette_;
	Activation activation_{};
	std::uint8_t ellen_phase_ = 0U;
	std::uint8_t rikako_angle_ = 0U;
	std::int8_t rikako_angle_delta_ = 4;
	std::uint8_t chiyuri_phase_ = 0U;
	MainBossStatus status_ = MainBossStatus::ready;
};

} // namespace th03::present

#endif
