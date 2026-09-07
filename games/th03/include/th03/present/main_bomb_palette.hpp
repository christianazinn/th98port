#ifndef TH03_PRESENT_MAIN_BOMB_PALETTE_HPP
#define TH03_PRESENT_MAIN_BOMB_PALETTE_HPP

#include <cstdint>

#include "th03/core/observer.hpp"
#include "th03/present/main_palette.hpp"

namespace th03::present {

enum class MainBombPaletteStatus : std::uint8_t {
	ready,
	invalid_player,
	invalid_paletted_character,
	missing_baseline,
	rejected_kotohime_component,
};

// kotohime_bomb()'s late fade reads a source-local byte before it was
// initialized, doubles it, and stores the result to both red and green before
// storing the computed fade to blue. The deterministic default substitutes
// the adjacent kana_bomb() fade local. An optional research provider can
// instead supply the final doubled R/G byte and fail atomically.
using KotohimeBombFadeComponentProvider = bool (*) (
	void *context,
	const BombFrameStep &step,
	std::uint8_t fade_blue,
	std::uint8_t &red_green_out
) noexcept;

struct KotohimeBombFadeCompatibility {
	KotohimeBombFadeComponentProvider component_provider = nullptr;
	void *context = nullptr;
};

// Presentation-only interpreter for the palette portions of MAIN's nine Bomb
// callbacks. The Game emits BombFrameStep synchronously in source P1-then-P2
// order; this observer deliberately performs no buffering or reordering.
//
// Sources: th03/main/player/ch_shot.cpp::{reimu_bomb,marisa_bomb},
// th03/main/bullet/bullet.cpp::mima_bomb, th03/main/player/chybomb.cpp::{
// chiyuri_bomb,ellen_bomb,kana_bomb,kotohime_bomb}, th03/main_05.cpp::
// rikako_bomb, and th03/main/player/yumemi_bomb.cpp::yumemi_bomb.
class MainBombPaletteInterpreter final : public GameplayObserver {
public:
	MainBombPaletteInterpreter(
		Raster &raster,
		MainPaletteState &palette,
		KotohimeBombFadeCompatibility kotohime_compatibility = {}
	) noexcept;

	void bomb_frame_step(const BombFrameStep &step) noexcept override;
	void bomb_finished(const BombFinished &finished) noexcept override;

	// Status is sticky once a callback reports an error. An invalid step or a
	// rejected research override makes no Raster or MainPaletteState mutation,
	// but does not prevent later independently valid facts from rendering.
	[[nodiscard]] MainBombPaletteStatus status() const noexcept;

private:
	friend class MainPresenterRollbackAccess;
	Raster *raster_;
	MainPaletteState *palette_;
	KotohimeBombFadeCompatibility kotohime_compatibility_{};
	MainBombPaletteStatus status_ = MainBombPaletteStatus::ready;
};

} // namespace th03::present

#endif
