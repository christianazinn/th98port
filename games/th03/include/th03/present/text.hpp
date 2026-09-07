#ifndef TH03_PRESENT_TEXT_HPP
#define TH03_PRESENT_TEXT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

#include "th03/present/raster.hpp"
#include "th03/present/tram.hpp"

namespace th03::present {

inline constexpr std::uint8_t ank_width = 8U;
inline constexpr std::uint8_t ank_height = 16U;
inline constexpr std::uint8_t jis_width = 16U;
inline constexpr std::uint8_t jis_height = 16U;
inline constexpr std::size_t jis_bytes_per_row = 2U;
inline constexpr std::size_t jis_glyph_bytes =
	jis_height * jis_bytes_per_row;

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

// Host-supplied fullwidth PC-98 glyph. Every row stores its left byte followed
// by its right byte, and bit 7 is the leftmost pixel within either byte.
struct JisGlyph {
	std::array<std::uint8_t, jis_glyph_bytes> row_halves{};

	[[nodiscard]] friend constexpr bool operator==(
		const JisGlyph &,
		const JisGlyph &
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

	// The canonical word stores the first JIS byte in the high byte and the
	// second in the low byte. Native TRAM stores the ordered pair as
	// (second << 8) | (first - 0x20), followed by that word | 0x0080. The
	// default keeps existing ANK-only providers source-compatible while making
	// missing machine font data explicit.
	[[nodiscard]] virtual std::optional<JisGlyph> jis_glyph(
		std::uint16_t canonical_jis
	) const;
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

enum class TextComposeErrorCode : std::uint8_t {
	graphics_size_mismatch,
	output_size_mismatch,
	graphics_index_out_of_range,
	missing_ank_glyph,
	jis_glyph_not_supported,
	malformed_jis_pair,
	missing_jis_glyph,
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

// Composes the visible 80x25 TRAM into a caller-owned 640x400 output span over
// a caller-resolved graphics image of the same size. The graphics span must
// already reflect page selection, raster mode, and scrolling; this function
// owns only the source-proven final text overlay and performs no allocation.
//
// Native fullwidth text is retained as an ordered pair of adjacent TRAM cells.
// The left cell maps to the left eight glyph pixels and the right cell to the
// right eight; hardware applies each cell's attributes independently. Hardware
// blink cadence and underline rasterization remain explicit errors.
[[nodiscard]] std::expected<void, TextComposeError>
compose_text_layer(
	std::span<ComposedPixel> output,
	std::span<const Index4> graphics,
	const Tram &tram,
	const AnkGlyphProvider &ank
);

} // namespace th03::present

#endif
