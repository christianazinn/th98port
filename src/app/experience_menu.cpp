#include "th03/app/experience_menu.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace th03::app {
namespace {

using present::ComposedPixel;
using present::ComposedPixelLayer;

constexpr std::size_t width = present::graphics_width;
constexpr std::size_t height = present::graphics_height;

// Compact host-only font. The shell must remain able to identify a
// content failure before a selected experience authenticates FONT.ROM.
constexpr std::array<std::array<std::uint8_t, 7U>, 26U> uppercase_font{{
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

constexpr std::array<std::uint8_t, 7U> lowercase_o{
	0x00U,0x00U,0x0EU,0x11U,0x11U,0x11U,0x0EU
};
constexpr std::array<std::uint8_t, 7U> lowercase_r{
	0x00U,0x00U,0x16U,0x19U,0x10U,0x10U,0x10U
};
constexpr std::array<std::uint8_t, 7U> lowercase_t{
	0x04U,0x04U,0x0EU,0x04U,0x04U,0x05U,0x02U
};

constexpr std::array<std::string_view, 3U> labels{
	"CLASSIC", "NEW CLASSIC", "EXIT"
};
constexpr std::array<std::size_t, 3U> row_y{184U, 248U, 312U};

[[nodiscard]] constexpr std::size_t choice_index(
	ExperienceMenuChoice choice
) noexcept
{
	return static_cast<std::size_t>(choice);
}

void fill_rect(
	std::vector<ComposedPixel> &pixels,
	std::size_t x,
	std::size_t y,
	std::size_t rectangle_width,
	std::size_t rectangle_height,
	std::uint8_t color
) noexcept
{
	for(std::size_t row = y; row < std::min(y + rectangle_height, height); ++row) {
		const auto begin = (row * width) + x;
		const auto end = (row * width) + std::min(x + rectangle_width, width);
		std::fill(
			pixels.begin() + static_cast<std::ptrdiff_t>(begin),
			pixels.begin() + static_cast<std::ptrdiff_t>(end),
			ComposedPixel{ComposedPixelLayer::graphics, color}
		);
	}
}

void draw_text(
	std::vector<ComposedPixel> &pixels,
	std::string_view text,
	std::size_t center_x,
	std::size_t y,
	std::size_t scale,
	std::uint8_t color
) noexcept
{
	const auto glyph_width = 5U * scale;
	const auto advance = 6U * scale;
	const auto text_width = text.empty() ? 0U :
		((text.size() - 1U) * advance) + glyph_width;
	std::size_t x = (center_x >= (text_width / 2U)) ?
		center_x - (text_width / 2U) : 0U;
	for(const auto character : text) {
		const std::array<std::uint8_t, 7U> *glyph = nullptr;
		if(character >= 'A' && character <= 'Z') {
			glyph = &uppercase_font[
				static_cast<std::size_t>(character - 'A')
			];
		} else if(character == 'o') {
			glyph = &lowercase_o;
		} else if(character == 'r') {
			glyph = &lowercase_r;
		} else if(character == 't') {
			glyph = &lowercase_t;
		}
		if(glyph) {
			for(std::size_t row = 0U; row < glyph->size(); ++row) {
				for(std::size_t column = 0U; column < 5U; ++column) {
					if(((*glyph)[row] & (0x10U >> column)) != 0U) {
						fill_rect(
							pixels,
							x + (column * scale), y + (row * scale),
							scale, scale, color
						);
					}
				}
			}
		}
		x += advance;
	}
}

} // namespace

struct ExperienceMenu::Impl final {
	std::vector<ComposedPixel> pixels = std::vector<ComposedPixel>(
		present::graphics_page_pixels,
		ComposedPixel{ComposedPixelLayer::graphics, 0U}
	);
	assets::Palette16Rgb4 palette{};
	ExperienceMenuChoice choice = ExperienceMenuChoice::classic;
	ExperienceMenuStatus status = ExperienceMenuStatus::running;
	bool input_allowed = false;

	Impl()
	{
		palette.colors[0U] = {0U, 0U, 1U};
		palette.colors[1U] = {1U, 1U, 4U};
		palette.colors[2U] = {2U, 3U, 7U};
		palette.colors[3U] = {4U, 6U, 12U};
		palette.colors[4U] = {9U, 3U, 11U};
		palette.colors[14U] = {15U, 13U, 4U};
		palette.colors[15U] = {15U, 15U, 15U};
		draw();
	}

	void draw() noexcept
	{
		std::fill(
			pixels.begin(), pixels.end(),
			ComposedPixel{ComposedPixelLayer::graphics, 0U}
		);
		fill_rect(pixels, 48U, 34U, 544U, 92U, 1U);
		fill_rect(pixels, 52U, 38U, 536U, 84U, 3U);
		draw_text(pixels, "PoDD Port", width / 2U, 68U, 4U, 15U);

		for(std::size_t index = 0U; index < labels.size(); ++index) {
			const bool selected = index == choice_index(choice);
			fill_rect(
				pixels, 160U, row_y[index], 320U, 44U,
				selected ? 4U : 2U
			);
			fill_rect(
				pixels, 164U, row_y[index] + 4U, 312U, 36U,
				selected ? 3U : 1U
			);
			draw_text(
				pixels, labels[index], width / 2U, row_y[index] + 10U,
				3U, selected ? 14U : 15U
			);
		}
	}
};

ExperienceMenu::ExperienceMenu() : impl_(std::make_unique<Impl>()) {}
ExperienceMenu::~ExperienceMenu() = default;
ExperienceMenu::ExperienceMenu(ExperienceMenu &&) noexcept = default;

ExperienceMenuResult ExperienceMenu::consume(ExperienceMenuInput input) noexcept
{
	if(impl_->status != ExperienceMenuStatus::running) {
		return {.status = impl_->status, .choice = impl_->choice};
	}
	if(input.app_quit_requested) {
		impl_->status = ExperienceMenuStatus::quit_requested;
		return {.status = impl_->status, .choice = impl_->choice};
	}
	const bool any = input.up || input.down || input.confirm || input.cancel;
	if(!impl_->input_allowed) {
		if(!any) {
			impl_->input_allowed = true;
		}
		return {.status = impl_->status, .choice = impl_->choice};
	}
	if(!any) {
		return {.status = impl_->status, .choice = impl_->choice};
	}
	impl_->input_allowed = false;
	if(input.cancel) {
		impl_->choice = ExperienceMenuChoice::exit;
		impl_->status = ExperienceMenuStatus::quit_requested;
		return {
			.surface_changed = true,
			.status = impl_->status,
			.choice = impl_->choice,
		};
	}
	if(input.up || input.down) {
		const auto old = choice_index(impl_->choice);
		const auto count = labels.size();
		const auto next = input.up ? ((old + count - 1U) % count) :
			((old + 1U) % count);
		impl_->choice = static_cast<ExperienceMenuChoice>(next);
		impl_->draw();
		return {
			.surface_changed = true,
			.status = impl_->status,
			.choice = impl_->choice,
		};
	}
	if(input.confirm) {
		impl_->status = (impl_->choice == ExperienceMenuChoice::exit) ?
			ExperienceMenuStatus::quit_requested :
			ExperienceMenuStatus::experience_selected;
	}
	return {.status = impl_->status, .choice = impl_->choice};
}

present::SourceSurface ExperienceMenu::surface() const noexcept
{
	return {
		.pixels = std::span<
			const ComposedPixel, present::graphics_page_pixels
		>(impl_->pixels.data(), impl_->pixels.size()),
		.graphics_palette = impl_->palette,
	};
}

ExperienceMenuChoice ExperienceMenu::choice() const noexcept
{
	return impl_->choice;
}

ExperienceMenuStatus ExperienceMenu::status() const noexcept
{
	return impl_->status;
}

} // namespace th03::app
