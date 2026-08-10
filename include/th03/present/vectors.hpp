#ifndef TH03_PRESENT_VECTORS_HPP
#define TH03_PRESENT_VECTORS_HPP

#include <cstdint>

#include "th03/present/raster.hpp"

namespace th03::present {

// Post-grc_setclip() coordinates. All four edges are inclusive, as in the
// master.lib ClipXL/ClipYT/ClipXR/ClipYB globals.
struct GrcgClipRect {
	std::int16_t left = 0;
	std::int16_t top = 0;
	std::int16_t right = 0;
	std::int16_t bottom = 0;
};

struct GrcgPoint {
	std::int16_t x = 0;
	std::int16_t y = 0;
};

// Source-derived subset used by TH03's common-effect pool. Both functions use
// GRCG read-modify-write with one solid color and write the access page only.
// Circle radii are bytes because cee0_rec_t stores and mutates them as bytes.
[[nodiscard]] bool grcg_line(
	Raster &raster,
	GrcgPoint first,
	GrcgPoint second,
	GrcgClipRect clip,
	Index4 color
) noexcept;

[[nodiscard]] bool grcg_circle(
	Raster &raster,
	GrcgPoint center,
	std::uint8_t radius,
	GrcgClipRect clip,
	Index4 color
) noexcept;

} // namespace th03::present

#endif
