#ifndef TH03_PRESENT_TRIANGLES_HPP
#define TH03_PRESENT_TRIANGLES_HPP

#include <cstdint>

#include "th03/present/vectors.hpp"

namespace th03::present {

// The TH03-only source use of master.lib grcg_triangle(): a GRCG
// read-modify-write solid color and draw_trapezoid's repeated 16-pixel CPU
// mask. The mask's most-significant bit selects the first pixel in every
// 16-pixel word; 0xFFFF is solid, while TH03's transition uses 0x5555 and
// 0xAAAA. Arbitrary GRCG tile modes intentionally remain outside this closed
// presentation primitive.
struct GrcgTriangleFill {
	Index4 color = 0U;
	std::uint16_t horizontal_mask = 0xFFFFU;
};

// Exact direct grcg_trapezoid() geometry. The two horizontal endpoints and
// every generated scanline are inclusive. The source rejects either input Y
// above ClipYT before ordering them, clamps only the lower endpoint, and tiles
// horizontal_mask against absolute 16-pixel VRAM words.
[[nodiscard]] bool grcg_trapezoid(
	Raster &raster,
	std::int16_t y1,
	std::int16_t x11,
	std::int16_t x12,
	std::int16_t y2,
	std::int16_t x21,
	std::int16_t x22,
	GrcgClipRect clip,
	GrcgTriangleFill fill
) noexcept;

// Exact filled grcg_triangle() geometry for the source-used solid GRCG mode.
// Coordinates and intermediate DDA state use signed 16-bit wrapping; all
// clip edges are inclusive. The primitive writes Raster's current access page
// only and returns false before any write for an invalid color or clip.
//
// Sources: libs/master.lib/grcg_triangle.asm, make_linework.asm,
// draw_trapezoid.asm, grcg_triangle[bss].asm, and draw_trapezoid[data].asm;
// TH03 callers: th03/main/player/yumemi_bomb.cpp and th03/main/hitcirc.cpp.
[[nodiscard]] bool grcg_triangle(
	Raster &raster,
	GrcgPoint first,
	GrcgPoint second,
	GrcgPoint third,
	GrcgClipRect clip,
	GrcgTriangleFill fill
) noexcept;

} // namespace th03::present

#endif
