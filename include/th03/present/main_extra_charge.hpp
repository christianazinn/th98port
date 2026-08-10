#ifndef TH03_PRESENT_MAIN_EXTRA_CHARGE_HPP
#define TH03_PRESENT_MAIN_EXTRA_CHARGE_HPP

#include <array>
#include <cstdint>

#include "th03/core/observer.hpp"
#include "th03/present/sprite16.hpp"
#include "th03/present/staff_flakes.hpp"

namespace th03::present {

enum class MainExtraChargeStatus : std::uint8_t {
	ready,
	invalid_assets,
	invalid_observation,
	staging_not_committed,
	draw_failed,
};

// Process-scoped reconstruction of MAIN's nine Extra Attack and Charge Shot
// render callbacks. Construct this presenter at process start and retain it
// across repeat rounds: Kana's direct Extra allocation renders through a stale
// destination byte whose last state-2/state-3 value is presentation-owned.
// Consequently, attaching a fresh presenter to an already-running process
// cannot reproduce that byte exactly. `staging` must already have been
// committed from and for the injected `raster`; no runtime identity check is
// available. Both objects and the backing storage of both `trig` spans must
// outlive this presenter.
class MainExtraChargePresenter final : public GameplayObserver {
public:
	MainExtraChargePresenter(
		Raster &raster,
		const Sprite16Staging &staging,
		StaffFlakeTrigTables trig
	) noexcept;

	void observe(const MainExtraAttackView &view) noexcept override;
	void observe(const MainChargeShotView &view) noexcept override;

	[[nodiscard]] MainExtraChargeStatus status() const noexcept;
	[[nodiscard]] bool ready() const noexcept;

private:
	Raster *raster_;
	const Sprite16Staging *staging_;
	StaffFlakeTrigTables trig_{};
	std::array<
		std::array<std::uint8_t, 8U>,
		2U
	> kana_destination_cache_{};
	MainExtraChargeStatus status_ = MainExtraChargeStatus::ready;
};

} // namespace th03::present

#endif
