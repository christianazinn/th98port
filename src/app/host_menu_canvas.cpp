#include "th03/app/host_menu_canvas.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace th03::app {
namespace {

using Glyph = std::array<std::uint8_t, 7U>;

constexpr Glyph blank{};
constexpr std::array<Glyph, 26U> uppercase_font{{
	{{0x0EU,0x11U,0x11U,0x1FU,0x11U,0x11U,0x11U}},
	{{0x1EU,0x11U,0x11U,0x1EU,0x11U,0x11U,0x1EU}},
	{{0x0EU,0x11U,0x10U,0x10U,0x10U,0x11U,0x0EU}},
	{{0x1EU,0x11U,0x11U,0x11U,0x11U,0x11U,0x1EU}},
	{{0x1FU,0x10U,0x10U,0x1EU,0x10U,0x10U,0x1FU}},
	{{0x1FU,0x10U,0x10U,0x1EU,0x10U,0x10U,0x10U}},
	{{0x0EU,0x11U,0x10U,0x17U,0x11U,0x11U,0x0FU}},
	{{0x11U,0x11U,0x11U,0x1FU,0x11U,0x11U,0x11U}},
	{{0x0EU,0x04U,0x04U,0x04U,0x04U,0x04U,0x0EU}},
	{{0x07U,0x02U,0x02U,0x02U,0x02U,0x12U,0x0CU}},
	{{0x11U,0x12U,0x14U,0x18U,0x14U,0x12U,0x11U}},
	{{0x10U,0x10U,0x10U,0x10U,0x10U,0x10U,0x1FU}},
	{{0x11U,0x1BU,0x15U,0x15U,0x11U,0x11U,0x11U}},
	{{0x11U,0x19U,0x15U,0x13U,0x11U,0x11U,0x11U}},
	{{0x0EU,0x11U,0x11U,0x11U,0x11U,0x11U,0x0EU}},
	{{0x1EU,0x11U,0x11U,0x1EU,0x10U,0x10U,0x10U}},
	{{0x0EU,0x11U,0x11U,0x11U,0x15U,0x12U,0x0DU}},
	{{0x1EU,0x11U,0x11U,0x1EU,0x14U,0x12U,0x11U}},
	{{0x0FU,0x10U,0x10U,0x0EU,0x01U,0x01U,0x1EU}},
	{{0x1FU,0x04U,0x04U,0x04U,0x04U,0x04U,0x04U}},
	{{0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x0EU}},
	{{0x11U,0x11U,0x11U,0x11U,0x11U,0x0AU,0x04U}},
	{{0x11U,0x11U,0x11U,0x15U,0x15U,0x15U,0x0AU}},
	{{0x11U,0x11U,0x0AU,0x04U,0x0AU,0x11U,0x11U}},
	{{0x11U,0x11U,0x0AU,0x04U,0x04U,0x04U,0x04U}},
	{{0x1FU,0x01U,0x02U,0x04U,0x08U,0x10U,0x1FU}},
}};
constexpr std::array<Glyph, 10U> digit_font{{
	{{0x0EU,0x11U,0x13U,0x15U,0x19U,0x11U,0x0EU}},
	{{0x04U,0x0CU,0x04U,0x04U,0x04U,0x04U,0x0EU}},
	{{0x0EU,0x11U,0x01U,0x02U,0x04U,0x08U,0x1FU}},
	{{0x1EU,0x01U,0x01U,0x0EU,0x01U,0x01U,0x1EU}},
	{{0x02U,0x06U,0x0AU,0x12U,0x1FU,0x02U,0x02U}},
	{{0x1FU,0x10U,0x10U,0x1EU,0x01U,0x01U,0x1EU}},
	{{0x06U,0x08U,0x10U,0x1EU,0x11U,0x11U,0x0EU}},
	{{0x1FU,0x01U,0x02U,0x04U,0x08U,0x08U,0x08U}},
	{{0x0EU,0x11U,0x11U,0x0EU,0x11U,0x11U,0x0EU}},
	{{0x0EU,0x11U,0x11U,0x0FU,0x01U,0x02U,0x0CU}},
}};

[[nodiscard]] constexpr Glyph punctuation(char character) noexcept
{
	switch(character) {
	case '-': return {{0U,0U,0U,0x1FU,0U,0U,0U}};
	case ':': return {{0U,0x04U,0x04U,0U,0x04U,0x04U,0U}};
	case '.': return {{0U,0U,0U,0U,0U,0x06U,0x06U}};
	case '/': return {{0x01U,0x02U,0x02U,0x04U,0x08U,0x08U,0x10U}};
	case '?': return {{0x0EU,0x11U,0x01U,0x02U,0x04U,0U,0x04U}};
	case '!': return {{0x04U,0x04U,0x04U,0x04U,0x04U,0U,0x04U}};
	case '<': return {{0x02U,0x04U,0x08U,0x10U,0x08U,0x04U,0x02U}};
	case '>': return {{0x08U,0x04U,0x02U,0x01U,0x02U,0x04U,0x08U}};
	default: return blank;
	}
}

[[nodiscard]] constexpr Glyph glyph_for(char character) noexcept
{
	if(character >= 'a' && character <= 'z') {
		character = static_cast<char>(character - ('a' - 'A'));
	}
	if(character >= 'A' && character <= 'Z') {
		return uppercase_font[static_cast<std::size_t>(character - 'A')];
	}
	if(character >= '0' && character <= '9') {
		return digit_font[static_cast<std::size_t>(character - '0')];
	}
	return punctuation(character);
}

[[nodiscard]] constexpr std::size_t text_width(
	std::string_view text,
	std::size_t scale
) noexcept
{
	return text.empty() ? 0U : (((text.size() - 1U) * 6U) + 5U) * scale;
}

} // namespace

HostMenuCanvas::HostMenuCanvas() :
	pixels_(
		present::graphics_page_pixels,
		present::ComposedPixel{present::ComposedPixelLayer::graphics, 0U}
	)
{
	palette_.colors[0U] = {0U, 0U, 1U};
	palette_.colors[1U] = {1U, 1U, 4U};
	palette_.colors[2U] = {2U, 3U, 7U};
	palette_.colors[3U] = {4U, 6U, 12U};
	palette_.colors[4U] = {9U, 3U, 11U};
	palette_.colors[5U] = {1U, 8U, 8U};
	palette_.colors[6U] = {9U, 8U, 2U};
	palette_.colors[14U] = {15U, 13U, 4U};
	palette_.colors[15U] = {15U, 15U, 15U};
}

void HostMenuCanvas::clear(std::uint8_t color) noexcept
{
	std::fill(
		pixels_.begin(), pixels_.end(),
		present::ComposedPixel{present::ComposedPixelLayer::graphics, color}
	);
}

void HostMenuCanvas::fill_rect(
	std::size_t x,
	std::size_t y,
	std::size_t rectangle_width,
	std::size_t rectangle_height,
	std::uint8_t color
) noexcept
{
	constexpr std::size_t width = present::graphics_width;
	constexpr std::size_t height = present::graphics_height;
	if(x >= width || y >= height) {
		return;
	}
	const auto clipped_height = std::min(rectangle_height, height - y);
	const auto clipped_width = std::min(rectangle_width, width - x);
	for(std::size_t row = y; row < y + clipped_height; ++row) {
		const auto begin = (row * width) + x;
		const auto end = begin + clipped_width;
		std::fill(
			pixels_.begin() + static_cast<std::ptrdiff_t>(begin),
			pixels_.begin() + static_cast<std::ptrdiff_t>(end),
			present::ComposedPixel{present::ComposedPixelLayer::graphics, color}
		);
	}
}

void HostMenuCanvas::draw_text(
	std::string_view text,
	std::size_t x,
	std::size_t y,
	std::size_t scale,
	std::uint8_t color
) noexcept
{
	if(scale == 0U) {
		return;
	}
	for(const auto character : text) {
		const auto glyph = glyph_for(character);
		for(std::size_t row = 0U; row < glyph.size(); ++row) {
			for(std::size_t column = 0U; column < 5U; ++column) {
				if((glyph[row] & (0x10U >> column)) != 0U) {
					fill_rect(
						x + (column * scale), y + (row * scale),
						scale, scale, color
					);
				}
			}
		}
		x += 6U * scale;
	}
}

void HostMenuCanvas::draw_text_centered(
	std::string_view text,
	std::size_t center_x,
	std::size_t y,
	std::size_t scale,
	std::uint8_t color
) noexcept
{
	const auto width = text_width(text, scale);
	draw_text(text, center_x >= width / 2U ? center_x - width / 2U : 0U,
		y, scale, color);
}

present::SourceSurface HostMenuCanvas::surface() const noexcept
{
	return {
		std::span<
			const present::ComposedPixel, present::graphics_page_pixels
		>(pixels_.data(), pixels_.size()),
		palette_,
	};
}

const assets::Palette16Rgb4 &HostMenuCanvas::palette() const noexcept
{
	return palette_;
}

assets::Palette16Rgb4 &HostMenuCanvas::palette() noexcept
{
	return palette_;
}

} // namespace th03::app
