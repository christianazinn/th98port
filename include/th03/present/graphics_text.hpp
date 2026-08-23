#ifndef TH03_PRESENT_GRAPHICS_TEXT_HPP
#define TH03_PRESENT_GRAPHICS_TEXT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "th03/present/raster.hpp"
#include "th03/present/text.hpp"

namespace th03::present {

enum class GraphicsTextWeight : std::uint8_t {
	normal = 0U,
	heavy,
	bold,
	black,
};

struct GraphicsTextStyle {
	Index4 color = 0U;
	GraphicsTextWeight weight = GraphicsTextWeight::normal;
	// Additional source cursor advance after every glyph. The final advance has
	// no raster extent, matching graph_putsa_fx().
	std::uint8_t spacing = 0U;
};

enum class GraphicsTextErrorCode : std::uint8_t {
	malformed_shift_jis,
	missing_jis_glyph,
};

struct GraphicsTextError {
	GraphicsTextErrorCode code =
		GraphicsTextErrorCode::malformed_shift_jis;
	std::size_t byte_offset = 0U;
	std::uint16_t canonical_jis = 0U;

	[[nodiscard]] friend constexpr bool operator==(
		const GraphicsTextError &,
		const GraphicsTextError &
	) = default;
};

struct GraphicsTextGlyph8x16 final {
	std::array<std::uint16_t, jis_height> rows{};
	std::uint8_t width = 0U;
};

// Prepared 8x16 graphics-page text. Preparation copies every required FONT.ROM
// row, so neither the input bytes nor the glyph provider need to outlive this
// object. Allocation occurs only during preparation; drawing is allocation-free.
class PreparedGraphicsText8x16 final {
public:
	PreparedGraphicsText8x16(PreparedGraphicsText8x16 &&) noexcept = default;
	PreparedGraphicsText8x16 &operator=(
		PreparedGraphicsText8x16 &&
	) noexcept = default;

	PreparedGraphicsText8x16(const PreparedGraphicsText8x16 &) = delete;
	PreparedGraphicsText8x16 &operator=(
		const PreparedGraphicsText8x16 &
	) = delete;

	// Ink extent at spacing 0. A nonzero style spacing adds pixels only between
	// glyph ink rectangles; the source's final cursor advance is not ink.
	[[nodiscard]] std::uint32_t pixel_width() const noexcept;
	[[nodiscard]] std::size_t glyph_count() const noexcept;

private:
	struct Glyph {
		std::array<std::uint16_t, jis_height> rows{};
		std::uint8_t width = 0U;
	};

	friend std::expected<PreparedGraphicsText8x16, GraphicsTextError>
	prepare_graphics_text_8x16(
		std::span<const std::uint8_t>,
		const AnkGlyphProvider &
	);
	friend std::expected<PreparedGraphicsText8x16, GraphicsTextError>
	prepare_graphics_glyphs_8x16(std::span<const GraphicsTextGlyph8x16>);
	friend bool graphics_text_put_8x16(
		Raster &,
		Point,
		const PreparedGraphicsText8x16 &,
		GraphicsTextStyle
	) noexcept;
	friend bool graphics_text_put_scaled_8x16(
		Raster &,
		Point,
		const PreparedGraphicsText8x16 &,
		GraphicsTextStyle,
		std::uint8_t
	) noexcept;

	PreparedGraphicsText8x16(
		std::vector<Glyph> glyphs,
		std::uint32_t pixel_width
	) noexcept;

	std::vector<Glyph> glyphs_{};
	std::uint32_t pixel_width_ = 0U;
};

// Parses one exact, non-NUL-terminated Shift-JIS byte span. Embedded NULs,
// dangling/invalid pairs, and unavailable machine-font glyphs are rejected
// before a prepared object is published.
[[nodiscard]] std::expected<PreparedGraphicsText8x16, GraphicsTextError>
prepare_graphics_text_8x16(
	std::span<const std::uint8_t> shift_jis,
	const AnkGlyphProvider &font
);

// Builds prepared graphics text from already-resolved deterministic glyph
// rows. This is the source-independent seam used by New Classic's UTF-8
// fallback; Classic continues through the exact Shift-JIS/FONT.ROM parser.
[[nodiscard]] std::expected<PreparedGraphicsText8x16, GraphicsTextError>
prepare_graphics_glyphs_8x16(std::span<const GraphicsTextGlyph8x16> glyphs);

// Portable graph_putsa_fx() 8x16 graphics-page path. The complete request is
// validated before the first current-access-page pixel changes. Stock-invalid
// off-page requests therefore fail atomically instead of wrapping VRAM.
[[nodiscard]] bool graphics_text_put_8x16(
	Raster &raster,
	Point position,
	const PreparedGraphicsText8x16 &text,
	GraphicsTextStyle style
) noexcept;

// New Classic image-text path. Each source pixel and source spacing unit is
// expanded by an integer scale from 1 through 4. The complete destination is
// validated before the first pixel changes, matching the atomic failure
// contract of graphics_text_put_8x16().
[[nodiscard]] bool graphics_text_put_scaled_8x16(
	Raster &raster,
	Point position,
	const PreparedGraphicsText8x16 &text,
	GraphicsTextStyle style,
	std::uint8_t scale
) noexcept;

} // namespace th03::present

#endif
