#ifndef TH03_PRESENT_SOURCE_SURFACE_HPP
#define TH03_PRESENT_SOURCE_SURFACE_HPP

#include <cstdint>
#include <expected>
#include <span>

#include "th03/present/text.hpp"

namespace th03::present {

// One complete caller-owned physical display surface. Pixels retain their
// source domain: graphics entries are Index4 values while text entries are
// fixed three-bit TextColor values. The applied graphics palette accompanies
// the view for a future one-texture host uploader; it must not be applied to
// text-domain pixels.
struct SourceSurface {
	std::span<const ComposedPixel, graphics_page_pixels> pixels;
	assets::Palette16Rgb4 graphics_palette{};
	Page shown_page = Page::zero;
	RasterMode raster_mode = RasterMode::lines_400;
	std::uint16_t effective_scroll_y = 0U;
};

enum class SourceSurfaceErrorCode : std::uint8_t {
	invalid_raster_mode,
	graphics_index_out_of_range,
	text_composition,
};

struct SourceSurfaceError {
	SourceSurfaceErrorCode code =
		SourceSurfaceErrorCode::invalid_raster_mode;
	TextComposeError text{};
};

// Resolves the shown page's 400-line or vertically zoomed 200-line GDC window
// and source graph_scrollup() wrap before compositing visible TRAM last.
// [output] is a fixed 640x400 caller-owned buffer and is unchanged on every
// error. No graphics palette is applied to the text layer: ComposedPixel keeps
// the PC-98 fixed text-color domain separate for the host adapter.
//
// Sources: libs/master.lib/graph_200line.asm, graph_400line.asm,
// graph_scrollup.asm, gaiji_write.asm, gaiji_putca.asm, text_fillca.asm, and
// text_boxfilla.asm. Text attributes, ANK lookup, paired gaiji, and deferred
// paths match compose_text_layer() in text.hpp.
[[nodiscard]] std::expected<SourceSurface, SourceSurfaceError>
compose_source_surface(
	std::span<ComposedPixel, graphics_page_pixels> output,
	const Raster &raster,
	const Tram &tram,
	const AnkGlyphProvider &ank
);

} // namespace th03::present

#endif
