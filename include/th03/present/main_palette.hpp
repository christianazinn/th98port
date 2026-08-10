#ifndef TH03_PRESENT_MAIN_PALETTE_HPP
#define TH03_PRESENT_MAIN_PALETTE_HPP

#include <cstdint>

#include "th03/present/raster.hpp"

namespace th03::present {

// Process-scoped MAIN palette state. This mirrors the source's saved
// Palette8 snapshot and palette_changed latch while Raster owns the logical
// palette and the applied DAC result.
class MainPaletteState {
public:
	// Mirrors the one process-start SUB_A378(): capture Raster's current logical
	// Palette8 snapshot exactly once. Repeats restore this same snapshot rather
	// than replacing it with an end-of-round effect palette. PaletteTone and the
	// changed latch are untouched.
	[[nodiscard]] bool capture_baseline(const Raster &raster) noexcept;
	[[nodiscard]] bool baseline_ready() const noexcept;

	// Mirrors the deferred Palette8 writes in MAIN. Invalid indices leave both
	// Raster and the changed latch untouched.
	[[nodiscard]] bool set_entry_deferred(
		Raster &raster,
		std::uint8_t index,
		Rgb8 color
	) noexcept;
	[[nodiscard]] bool restore_entry_deferred(
		Raster &raster,
		std::uint8_t index
	) noexcept;
	void set_tone_deferred(Raster &raster, std::int16_t tone) noexcept;

	// Mirrors SUB_A38E(): restore every saved entry and show it immediately.
	// Like the source palette_show() call, this deliberately does not clear an
	// already-pending palette_changed latch.
	[[nodiscard]] bool restore_baseline_and_show(Raster &raster) noexcept;
	void set_tone_and_show_now(Raster &raster, std::int16_t tone) noexcept;
	void show_now(Raster &raster) noexcept;

	// Mirrors the end-of-frame palette_changed footer. Returns whether a show
	// occurred, and clears the latch only after that source-equivalent show.
	[[nodiscard]] bool apply_if_changed(Raster &raster) noexcept;
	[[nodiscard]] bool dirty() const noexcept;

private:
	Palette16Rgb8 baseline_{};
	bool baseline_ready_ = false;
	bool dirty_ = false;
};

} // namespace th03::present

#endif
