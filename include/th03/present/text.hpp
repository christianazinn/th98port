#ifndef TH03_PRESENT_TEXT_HPP
#define TH03_PRESENT_TEXT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

#include "th03/present/raster.hpp"
#include "th03/present/tram.hpp"

namespace th03::present {

inline constexpr std::uint8_t ank_width = 8U;
inline constexpr std::uint8_t ank_height = 16U;

// Host-supplied ANK bitmap. The pinned text routines store only the character
// code in TRAM; the corresponding bitmap remains machine font data. As with
// gaiji_write(), bit 7 is the leftmost pixel of a row.
struct AnkGlyph {
	std::array<std::uint8_t, ank_height> rows{};

	[[nodiscard]] friend constexpr bool operator==(
		const AnkGlyph &,
		const AnkGlyph &
	) = default;
};

class AnkGlyphProvider {
public:
	virtual ~AnkGlyphProvider() = default;

	// Implementations must return stable, caller-owned font data by value.
	// Original PC-98 font ROM bytes are deliberately not part of th03_present.
	[[nodiscard]] virtual std::optional<AnkGlyph> glyph(
		std::uint8_t code
	) const = 0;
};

// Text attributes select one of the PC-98 text layer's fixed three-bit colors.
// Keep this domain separate from programmable four-bit graphics palette indices.
enum class TextColor : std::uint8_t {
	black = 0U,
	blue = 1U,
	red = 2U,
	magenta = 3U,
	green = 4U,
	cyan = 5U,
	yellow = 6U,
	white = 7U,
};

enum class ComposedPixelLayer : std::uint8_t {
	graphics,
	text,
};

// [value] is an Index4 when [layer] is graphics, and a TextColor value when it
// is text. Retaining the layer avoids incorrectly applying the graphics palette
// to PC-98 text colors.
struct ComposedPixel {
	ComposedPixelLayer layer = ComposedPixelLayer::graphics;
	std::uint8_t value = 0U;

	[[nodiscard]] friend constexpr bool operator==(
		const ComposedPixel &,
		const ComposedPixel &
	) = default;
};

using ComposedTextFrame = std::vector<ComposedPixel>;

enum class TextComposeErrorCode : std::uint8_t {
	graphics_size_mismatch,
	graphics_index_out_of_range,
	missing_ank_glyph,
	jis_glyph_not_supported,
	blink_not_supported,
	underline_not_supported,
	attribute_not_supported,
};

struct TextComposeError {
	TextComposeErrorCode code = TextComposeErrorCode::graphics_size_mismatch;
	TramPoint cell{-1, -1};
	TramCode tram_code = 0U;
	TramAttribute attribute = 0U;
};

// Composes the visible 80x25 TRAM over a caller-resolved 640x400 graphics
// image. The graphics span must already reflect page selection, raster mode,
// and scrolling; this function owns only the source-proven final text overlay.
//
// Anniversary TH03 uses ANK and gaiji TRAM cells and the reverse attribute.
// JIS glyph lookup, hardware blink cadence, and underline rasterization remain
// explicit errors until their external font/timing behavior is authoritative.
[[nodiscard]] std::expected<ComposedTextFrame, TextComposeError>
compose_text_layer(
	std::span<const Index4> graphics,
	const Tram &tram,
	const AnkGlyphProvider &ank
);

} // namespace th03::present

#endif
