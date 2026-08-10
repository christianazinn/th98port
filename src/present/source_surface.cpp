#include "th03/present/source_surface.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

namespace th03::present {
namespace {

constexpr TramAttribute attribute_display = 0x0001U;
constexpr TramAttribute attribute_color_mask = 0x00E0U;
constexpr TramAttribute supported_attribute_bits =
	attribute_display | attribute_color_mask | tram_reverse | tram_blink |
	tram_underline;

struct DisplayMapping {
	std::size_t raw_top = 0U;
	std::uint16_t logical_height = 0U;
	std::uint16_t effective_scroll_y = 0U;
	bool vertically_doubled = false;
};

struct TextPreparation {
	std::array<std::optional<AnkGlyph>, 256U> ank_cache{};
	std::array<bool, 256U> ank_queried{};
};

[[nodiscard]] SourceSurfaceError text_error(TextComposeError error) noexcept
{
	return {SourceSurfaceErrorCode::text_composition, error};
}

[[nodiscard]] TextComposeError cell_error(
	TextComposeErrorCode code,
	std::size_t cell_offset,
	TramCell cell
) noexcept
{
	return {
		code,
		{
			static_cast<std::int32_t>(cell_offset % tram_width),
			static_cast<std::int32_t>(cell_offset / tram_width),
		},
		cell.code,
		cell.attribute,
	};
}

[[nodiscard]] constexpr TextColor attribute_color(
	TramAttribute attribute
) noexcept
{
	return static_cast<TextColor>((attribute & attribute_color_mask) >> 5U);
}

[[nodiscard]] constexpr std::size_t pixel_offset(
	std::size_t x,
	std::size_t y
) noexcept
{
	return (y * graphics_width) + x;
}

[[nodiscard]] std::expected<DisplayMapping, SourceSurfaceError>
display_mapping(const Raster &raster) noexcept
{
	DisplayMapping result{};
	switch(raster.raster_mode()) {
	case RasterMode::lines_400:
		result.logical_height = graphics_height;
		break;
	case RasterMode::zoomed_200_upper:
		result.logical_height = graphics_height / 2U;
		result.vertically_doubled = true;
		break;
	case RasterMode::zoomed_200_lower:
		result.raw_top = static_cast<std::size_t>(graphics_height / 2U);
		result.logical_height = graphics_height / 2U;
		result.vertically_doubled = true;
		break;
	default:
		return std::unexpected(SourceSurfaceError{
			SourceSurfaceErrorCode::invalid_raster_mode,
		});
	}

	// graph_scrollup() clamps an out-of-range unsigned line to graph_VramLines.
	// That produces the same complete display as a zero-line scroll because its
	// first region is empty and its second region begins at address zero.
	const auto raw_scroll = raster.scroll_y();
	result.effective_scroll_y = (raw_scroll < result.logical_height) ?
		raw_scroll : 0U;
	return result;
}

[[nodiscard]] constexpr std::size_t source_row(
	std::size_t physical_y,
	DisplayMapping mapping
) noexcept
{
	const auto logical_y = mapping.vertically_doubled ?
		(physical_y / 2U) : physical_y;
	const auto scrolled_y = (logical_y + mapping.effective_scroll_y) %
		mapping.logical_height;
	return mapping.raw_top + scrolled_y;
}

[[nodiscard]] std::optional<SourceSurfaceError> validate_graphics(
	std::span<const Index4> graphics,
	DisplayMapping mapping
) noexcept
{
	if(graphics.size() != graphics_page_pixels) {
		return SourceSurfaceError{
			SourceSurfaceErrorCode::graphics_index_out_of_range,
		};
	}
	for(std::size_t physical_y = 0U; physical_y < graphics_height; ++physical_y) {
		const auto row = source_row(physical_y, mapping);
		const auto row_offset = row * graphics_width;
		for(std::size_t x = 0U; x < graphics_width; ++x) {
			if(graphics[row_offset + x] > 0x0FU) {
				return SourceSurfaceError{
					SourceSurfaceErrorCode::graphics_index_out_of_range,
				};
			}
		}
	}
	return std::nullopt;
}

[[nodiscard]] std::optional<SourceSurfaceError> prepare_text(
	const Tram &tram,
	const AnkGlyphProvider &ank,
	TextPreparation &prepared
)
{
	if(!tram.visible()) {
		return std::nullopt;
	}

	const auto cells = tram.cells();
	for(std::size_t cell_offset = 0U; cell_offset < cells.size(); ++cell_offset) {
		const auto cell = cells[cell_offset];
		if((cell.attribute & attribute_display) == 0U) {
			continue;
		}

		const auto unsupported_bits = static_cast<TramAttribute>(
			cell.attribute & static_cast<TramAttribute>(~supported_attribute_bits)
		);
		if(unsupported_bits != 0U) {
			return text_error(cell_error(
				TextComposeErrorCode::attribute_not_supported,
				cell_offset,
				cell
			));
		}
		if((cell.attribute & tram_blink) != 0U) {
			return text_error(cell_error(
				TextComposeErrorCode::blink_not_supported,
				cell_offset,
				cell
			));
		}
		if((cell.attribute & tram_underline) != 0U) {
			return text_error(cell_error(
				TextComposeErrorCode::underline_not_supported,
				cell_offset,
				cell
			));
		}

		if(decode_paired_gaiji_cell(cells, cell_offset).has_value()) {
			continue;
		}
		if((cell.code & 0xFF00U) != 0U) {
			return text_error(cell_error(
				TextComposeErrorCode::jis_glyph_not_supported,
				cell_offset,
				cell
			));
		}
		const auto ank_code = static_cast<std::uint8_t>(cell.code);
		if(!prepared.ank_queried[ank_code]) {
			prepared.ank_cache[ank_code] = ank.glyph(ank_code);
			prepared.ank_queried[ank_code] = true;
		}
		if(!prepared.ank_cache[ank_code].has_value()) {
			return text_error(cell_error(
				TextComposeErrorCode::missing_ank_glyph,
				cell_offset,
				cell
			));
		}
	}
	return std::nullopt;
}

void resolve_graphics(
	std::span<ComposedPixel, graphics_page_pixels> output,
	std::span<const Index4> graphics,
	DisplayMapping mapping
) noexcept
{
	for(std::size_t physical_y = 0U; physical_y < graphics_height; ++physical_y) {
		const auto source_offset = source_row(physical_y, mapping) * graphics_width;
		const auto destination_offset = physical_y * graphics_width;
		for(std::size_t x = 0U; x < graphics_width; ++x) {
			output[destination_offset + x] = {
				ComposedPixelLayer::graphics,
				graphics[source_offset + x],
			};
		}
	}
}

void compose_glyph_row(
	std::span<ComposedPixel, graphics_page_pixels> output,
	std::size_t cell_x,
	std::size_t cell_y,
	std::size_t glyph_y,
	std::uint8_t glyph_bits,
	TextColor color,
	bool reverse
) noexcept
{
	if(reverse) {
		glyph_bits = static_cast<std::uint8_t>(~glyph_bits);
	}
	const auto destination_y = (cell_y * ank_height) + glyph_y;
	const auto destination_x = cell_x * ank_width;
	for(std::size_t glyph_x = 0U; glyph_x < ank_width; ++glyph_x) {
		const auto mask = static_cast<std::uint8_t>(0x80U >> glyph_x);
		if((glyph_bits & mask) != 0U) {
			output[pixel_offset(destination_x + glyph_x, destination_y)] = {
				ComposedPixelLayer::text,
				static_cast<std::uint8_t>(color),
			};
		}
	}
}

void compose_text(
	std::span<ComposedPixel, graphics_page_pixels> output,
	const Tram &tram,
	const TextPreparation &prepared
) noexcept
{
	if(!tram.visible()) {
		return;
	}

	const auto cells = tram.cells();
	for(std::size_t cell_offset = 0U; cell_offset < cells.size(); ++cell_offset) {
		const auto cell = cells[cell_offset];
		if((cell.attribute & attribute_display) == 0U) {
			continue;
		}

		const auto gaiji = decode_paired_gaiji_cell(cells, cell_offset);
		const auto cell_x = cell_offset % tram_width;
		const auto cell_y = cell_offset / tram_width;
		const auto color = attribute_color(cell.attribute);
		const bool reverse = (cell.attribute & tram_reverse) != 0U;
		if(gaiji.has_value()) {
			const auto &glyph = tram.gaiji(gaiji->glyph_id);
			const auto half_offset =
				(gaiji->half == GaijiHalf::left) ? 0U : 1U;
			for(std::size_t glyph_y = 0U; glyph_y < gaiji_height; ++glyph_y) {
				compose_glyph_row(
					output,
					cell_x,
					cell_y,
					glyph_y,
					glyph.row_halves[(glyph_y * gaiji_bytes_per_row) + half_offset],
					color,
					reverse
				);
			}
			continue;
		}

		const auto ank_code = static_cast<std::uint8_t>(cell.code);
		const auto &glyph = *prepared.ank_cache[ank_code];
		for(std::size_t glyph_y = 0U; glyph_y < ank_height; ++glyph_y) {
			compose_glyph_row(
				output,
				cell_x,
				cell_y,
				glyph_y,
				glyph.rows[glyph_y],
				color,
				reverse
			);
		}
	}
}

} // namespace

std::expected<SourceSurface, SourceSurfaceError>
compose_source_surface(
	std::span<ComposedPixel, graphics_page_pixels> output,
	const Raster &raster,
	const Tram &tram,
	const AnkGlyphProvider &ank
)
{
	const auto mapping = display_mapping(raster);
	if(!mapping.has_value()) {
		return std::unexpected(mapping.error());
	}
	const auto graphics = raster.graphics(raster.shown_page());
	if(const auto graphics_error = validate_graphics(graphics, *mapping);
		graphics_error.has_value()) {
		return std::unexpected(*graphics_error);
	}
	TextPreparation text{};
	if(const auto text_error = prepare_text(tram, ank, text);
		text_error.has_value()) {
		return std::unexpected(*text_error);
	}

	resolve_graphics(output, graphics, *mapping);
	compose_text(output, tram, text);
	return SourceSurface{
		std::span<const ComposedPixel, graphics_page_pixels>(output),
		raster.applied_palette(),
		raster.shown_page(),
		raster.raster_mode(),
		mapping->effective_scroll_y,
	};
}

} // namespace th03::present
