#ifndef TH03_PRESENT_MAIN_WARNING_GAUGE_HPP
#define TH03_PRESENT_MAIN_WARNING_GAUGE_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "th03/assets/types.hpp"
#include "th03/core/observer.hpp"
#include "th03/present/main_palette.hpp"
#include "th03/present/sprite_staging.hpp"
#include "th03/present/tram.hpp"

namespace th03::present {

inline constexpr std::size_t main_warning_player_count = 2U;

// NNEX.MRS is loaded once per selected character. The P2 image has already
// undergone the source's persistent mrs_hflip() transformation. Both immutable
// images must outlive MainWarningGaugePresenter.
struct MainWarningMrsAssets {
	std::array<const assets::MrsImage *, main_warning_player_count> images{};
};

enum class MainWarningGaugeStatus : std::uint8_t {
	ready,
	invalid_assets,
	invalid_observation,
	staging_not_committed,
	draw_failed,
	palette_failed,
};

// Process-scoped owner of MAIN's warning text/portraits, warning-driven
// palette changes, gauge-attack completion restores, and late gauge overlay.
// It retains only the two source oscillator bytes/directions and interstitial
// ordinal. No gameplay view, render command, or per-frame allocation survives
// a synchronous callback.
class MainWarningGaugePresenter final : public GameplayObserver {
public:
	MainWarningGaugePresenter(
		Raster &raster,
		Tram &tram,
		const Sprite16Staging &staging,
		MainPaletteState &palette,
		MainWarningMrsAssets portraits
	) noexcept;

	void warning_interstitial_started(
		const WarningInterstitialStarted &started
	) noexcept override;
	void warning_interstitial_sample(
		const WarningInterstitialSample &sample
	) noexcept override;
	void warning_frame_lifecycle(
		const WarningFrameLifecycle &lifecycle
	) noexcept override;
	void gauge_attack_finished(
		const GaugeAttackFinished &finished
	) noexcept override;
	void warning_portrait_transition(
		const WarningPortraitTransition &transition
	) noexcept override;
	void main_gauge_state(const MainGaugeState &state) noexcept override;
	// Arrange keeps source gameplay live while retaining the complete 27-frame
	// portrait/tone presentation on each newly composed gameplay page.
	[[nodiscard]] bool compose_nonblocking_interstitial_frame() noexcept;

	[[nodiscard]] MainWarningGaugeStatus status() const noexcept;
	[[nodiscard]] bool ready() const noexcept;

private:
	friend class MainPresenterRollbackAccess;
	friend class MainRoundPresenter;
	[[nodiscard]] bool fail(MainWarningGaugeStatus status) noexcept;
	[[nodiscard]] bool put_warning_text(
		std::size_t player,
		GaugeBossAttackKind attack,
		std::uint8_t gauge_level,
		std::uint8_t boss_level,
		TramAttribute attribute
	) noexcept;
	[[nodiscard]] bool clear_warning_text(std::size_t player) noexcept;
	[[nodiscard]] bool restore_opponent(std::size_t player) noexcept;
	[[nodiscard]] bool set_warning_partial_palette(
		std::size_t player,
		GaugeBossAttackKind attack
	) noexcept;
	[[nodiscard]] bool apply_active_ramp(
		std::size_t player,
		GaugeBossAttackKind active_attack
	) noexcept;
	[[nodiscard]] bool apply_warning_lifecycle(
		std::size_t player,
		const WarningPlayerLifecycle &lifecycle
	) noexcept;
	[[nodiscard]] bool draw_gauge(const MainGaugeState &state) noexcept;

	Raster *raster_;
	Tram *tram_;
	MainPaletteState *palette_;
	MainWarningMrsAssets portraits_{};
	std::array<std::uint8_t, main_warning_player_count> ramp_value_{};
	std::array<bool, main_warning_player_count> ramp_decreasing_{};
	std::array<std::uint8_t, main_warning_player_count> warning_gauge_level_{};
	std::array<bool, main_warning_player_count> warning_level_valid_{};
	std::uint8_t warning_boss_level_ = 0U;
	std::uint8_t next_interstitial_sample_ = 0U;
	std::uint8_t nonblocking_interstitial_frames_ = 0U;
	std::uint8_t nonblocking_interstitial_player_mask_ = 0U;
	bool interstitial_active_ = false;
	MainWarningGaugeStatus status_ = MainWarningGaugeStatus::ready;
};

} // namespace th03::present

#endif
