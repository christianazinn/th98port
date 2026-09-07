#ifndef TH03_PRESENT_SPRITE16_HPP
#define TH03_PRESENT_SPRITE16_HPP

#include <cstdint>
#include <optional>

#include "th03/present/raster.hpp"
#include "th03/present/sprite_staging.hpp"

namespace th03::present {

// Semantic coordinates in Sprite16Staging's 640x200 source atlas. Horizontal
// values are pixels; vertical values are raw 200-line VRAM rows.
struct Sprite16AtlasRect {
	std::uint16_t left = 0U;
	std::uint16_t raw_top = 0U;
	std::uint16_t width = 0U;
	std::uint16_t raw_height = 0U;
};

// Exact clipping points consumed by TH03's sprite16_put() wrapper. Clipping
// removes complete 16-pixel source columns. `right` retains the source
// wrapper's strict comparison rather than conventional rectangle semantics.
struct Sprite16HorizontalClip {
	std::int32_t left = 0;
	std::int32_t right = 0;
};

enum class Sprite16Overlap : std::uint8_t {
	clear,
	or_planes,
};

struct Sprite16DrawStyle {
	// SPRITE16 keeps these controls as persistent driver state. The native
	// operation receives their effective values explicitly for each draw,
	// avoiding hidden mutable presentation state.
	Sprite16Overlap overlap = Sprite16Overlap::clear;
	// When present, the generated positive alpha mask selects pixels replaced
	// with this color. The overlap setting is ignored, as in SPRITE16.
	std::optional<Index4> monochrome_color{};
	// Effective value of the source-persistent EGC mask used by TH03's
	// striped boss effects.
	std::uint16_t write_mask = 0xFFFFU;
};

// `screen_position.y` uses TH03's 400-line screen coordinates. Sprite16
// converts it to a raw 200-line row with a signed arithmetic shift.
[[nodiscard]] bool sprite16_put(
	Raster &raster,
	const Sprite16Staging &staging,
	Sprite16AtlasRect source,
	Point screen_position,
	Sprite16HorizontalClip clip,
	Sprite16DrawStyle style = {}
) noexcept;

// Omits TH03's horizontal wrapper clipping. Driver-level vertical clipping is
// retained. Stock-invalid X positions use 16-bit VRAM address wrapping; writes
// into the unmodeled remainder of a 64 KiB plane are safely discarded.
[[nodiscard]] bool sprite16_put_noclip(
	Raster &raster,
	const Sprite16Staging &staging,
	Sprite16AtlasRect source,
	Point screen_position,
	Sprite16DrawStyle style = {}
) noexcept;

// Exact stock-used sprite16_putx(..., SPF_DOWNWARDS_COLUMN) behavior: repeat
// one horizontally clipped source rectangle down to raw row 199.
[[nodiscard]] bool sprite16_put_downwards_column(
	Raster &raster,
	const Sprite16Staging &staging,
	Sprite16AtlasRect source,
	Point screen_position,
	Sprite16HorizontalClip clip,
	Sprite16DrawStyle style = {}
) noexcept;

} // namespace th03::present

#endif
