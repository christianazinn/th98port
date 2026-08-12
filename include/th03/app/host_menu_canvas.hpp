#ifndef TH03_APP_HOST_MENU_CANVAS_HPP
#define TH03_APP_HOST_MENU_CANVAS_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "th03/present/source_surface.hpp"

namespace th03::app {

// Small content-independent canvas shared by host-owned product and New
// Classic menus. It intentionally uses its own ASCII glyphs so diagnostics and
// replay management remain available before original FONT.ROM is accepted.
class HostMenuCanvas final {
public:
	HostMenuCanvas();

	void clear(std::uint8_t color = 0U) noexcept;
	void copy_from(present::SourceSurface source) noexcept;
	void fill_rect(
		std::size_t x,
		std::size_t y,
		std::size_t width,
		std::size_t height,
		std::uint8_t color,
		present::ComposedPixelLayer layer =
			present::ComposedPixelLayer::graphics
	) noexcept;
	void draw_text(
		std::string_view text,
		std::size_t x,
		std::size_t y,
		std::size_t scale,
		std::uint8_t color,
		present::ComposedPixelLayer layer =
			present::ComposedPixelLayer::graphics
	) noexcept;
	void draw_text_centered(
		std::string_view text,
		std::size_t center_x,
		std::size_t y,
		std::size_t scale,
		std::uint8_t color,
		present::ComposedPixelLayer layer =
			present::ComposedPixelLayer::graphics
	) noexcept;

	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] const assets::Palette16Rgb4 &palette() const noexcept;
	[[nodiscard]] assets::Palette16Rgb4 &palette() noexcept;

private:
	std::vector<present::ComposedPixel> pixels_;
	assets::Palette16Rgb4 palette_{};
};

} // namespace th03::app

#endif
