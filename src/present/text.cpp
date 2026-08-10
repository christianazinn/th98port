#include "th03/present/text.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th03::present {
namespace {

constexpr TramAttribute attribute_display = 0x0001U;
constexpr TramAttribute attribute_color_mask = 0x00E0U;
constexpr TramAttribute supported_attribute_bits =
	attribute_display | attribute_color_mask | tram_reverse | tram_blink |
	tram_underline;

enum class JisHalf : std::uint8_t {
	left,
	right,
};

struct JisCellReference {
	std::uint16_t canonical_jis = 0U;
	std::size_t left_cell_offset = 0U;
	JisHalf half = JisHalf::left;
};

struct TextPreparation {
	std::array<std::optional<AnkGlyph>, 256U> ank_cache{};
	std::array<bool, 256U> ank_queried{};
	std::array<std::optional<JisGlyph>, tram_cell_count> jis_cache{};
};

[[nodiscard]] TextComposeError cell_error(
	TextComposeErrorCode code,
	std::size_t cell_offset,
	TramCell cell
) noexcept {
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
) noexcept {
	return static_cast<TextColor>((attribute & attribute_color_mask) >> 5U);
}

[[nodiscard]] constexpr std::size_t pixel_offset(
	std::size_t x,
	std::size_t y
) noexcept {
	return (y * graphics_width) + x;
}

[[nodiscard]] std::optional<JisCellReference> decode_paired_jis_cell(
	std::span<const TramCell> cells,
	std::size_t offset
) noexcept {
	if(offset >= cells.size()) {
		return std::nullopt;
	}
	const auto code = cells[offset].code;
	if((code & 0xFF00U) == 0U) {
		return std::nullopt;
	}

	const bool is_right = (code & 0x0080U) != 0U;
	const auto cell_x = offset % tram_width;
	if((is_right && cell_x == 0U) ||
		(!is_right && (cell_x + 1U >= tram_width))) {
		return std::nullopt;
	}
	const auto left_offset = is_right ?
		((offset == 0U) ? cells.size() : offset - 1U) : offset;
	if(left_offset >= cells.size() || left_offset + 1U >= cells.size()) {
		return std::nullopt;
	}
	const auto left = cells[left_offset];
	const auto right = cells[left_offset + 1U];
	if((left.code & 0xFF00U) == 0U || (left.code & 0x0080U) != 0U ||
		right.code != static_cast<TramCode>(left.code | 0x0080U)) {
		return std::nullopt;
	}

	const auto first = static_cast<std::uint16_t>(
		(left.code & 0x007FU) + 0x0020U
	);
	const auto second = static_cast<std::uint16_t>(left.code >> 8U);
	return JisCellReference{
		static_cast<std::uint16_t>((first << 8U) | second),
		left_offset,
		is_right ? JisHalf::right : JisHalf::left,
	};
}

[[nodiscard]] std::optional<TextComposeError> prepare_text(
	const Tram &tram,
	const AnkGlyphProvider &ank,
	TextPreparation &prepared
) {
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
			return cell_error(
				TextComposeErrorCode::attribute_not_supported,
				cell_offset,
				cell
			);
		}
		if((cell.attribute & tram_blink) != 0U) {
			return cell_error(
				TextComposeErrorCode::blink_not_supported,
				cell_offset,
				cell
			);
		}
		if((cell.attribute & tram_underline) != 0U) {
			return cell_error(
				TextComposeErrorCode::underline_not_supported,
				cell_offset,
				cell
			);
		}

		if(decode_paired_gaiji_cell(cells, cell_offset).has_value()) {
			continue;
		}
		if((cell.code & 0xFF00U) == 0U) {
			const auto ank_code = static_cast<std::uint8_t>(cell.code);
			if(!prepared.ank_queried[ank_code]) {
				prepared.ank_cache[ank_code] = ank.glyph(ank_code);
				prepared.ank_queried[ank_code] = true;
			}
			if(!prepared.ank_cache[ank_code].has_value()) {
				return cell_error(
					TextComposeErrorCode::missing_ank_glyph,
					cell_offset,
					cell
				);
			}
			continue;
		}

		const auto jis = decode_paired_jis_cell(cells, cell_offset);
		if(!jis.has_value()) {
			return cell_error(
				TextComposeErrorCode::malformed_jis_pair,
				cell_offset,
				cell
			);
		}
		if(!prepared.jis_cache[jis->left_cell_offset].has_value()) {
			prepared.jis_cache[jis->left_cell_offset] =
				ank.jis_glyph(jis->canonical_jis);
		}
		if(!prepared.jis_cache[jis->left_cell_offset].has_value()) {
			return cell_error(
				TextComposeErrorCode::missing_jis_glyph,
				cell_offset,
				cell
			);
		}
	}
	return std::nullopt;
}

void compose_glyph_row(
	std::span<ComposedPixel> frame,
	std::size_t cell_x,
	std::size_t cell_y,
	std::size_t glyph_y,
	std::uint8_t glyph_bits,
	TextColor color,
	bool reverse
) noexcept {
	if(reverse) {
		glyph_bits = static_cast<std::uint8_t>(~glyph_bits);
	}
	const auto destination_y = (cell_y * ank_height) + glyph_y;
	const auto destination_x = cell_x * ank_width;
	for(std::size_t glyph_x = 0U; glyph_x < ank_width; ++glyph_x) {
		const auto mask = static_cast<std::uint8_t>(0x80U >> glyph_x);
		if((glyph_bits & mask) != 0U) {
			frame[pixel_offset(destination_x + glyph_x, destination_y)] = {
				ComposedPixelLayer::text,
				static_cast<std::uint8_t>(color),
			};
		}
	}
}

} // namespace

std::optional<JisGlyph> AnkGlyphProvider::jis_glyph(
	std::uint16_t
) const {
	return std::nullopt;
}

std::expected<void, TextComposeError> compose_text_layer(
	std::span<ComposedPixel> output,
	std::span<const Index4> graphics,
	const Tram &tram,
	const AnkGlyphProvider &ank
) {
	if(graphics.size() != graphics_page_pixels) {
		return std::unexpected(TextComposeError{
			TextComposeErrorCode::graphics_size_mismatch,
		});
	}
	if(output.size() != graphics_page_pixels) {
		return std::unexpected(TextComposeError{
			TextComposeErrorCode::output_size_mismatch,
		});
	}
	if(std::ranges::any_of(graphics, [](Index4 value) {
		return value > 0x0FU;
	})) {
		return std::unexpected(TextComposeError{
			TextComposeErrorCode::graphics_index_out_of_range,
		});
	}

	TextPreparation prepared{};
	if(const auto error = prepare_text(tram, ank, prepared);
		error.has_value()) {
		return std::unexpected(*error);
	}

	for(std::size_t offset = 0U; offset < graphics.size(); ++offset) {
		output[offset] = {ComposedPixelLayer::graphics, graphics[offset]};
	}
	if(!tram.visible()) {
		return {};
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

		if((cell.code & 0xFF00U) == 0U) {
			const auto ank_code = static_cast<std::uint8_t>(cell.code);
			for(std::size_t glyph_y = 0U; glyph_y < ank_height; ++glyph_y) {
				compose_glyph_row(
					output,
					cell_x,
					cell_y,
					glyph_y,
					prepared.ank_cache[ank_code]->rows[glyph_y],
					color,
					reverse
				);
			}
			continue;
		}

		const auto jis = *decode_paired_jis_cell(cells, cell_offset);
		const auto half_offset = (jis.half == JisHalf::left) ? 0U : 1U;
		const auto &glyph = *prepared.jis_cache[jis.left_cell_offset];
		for(std::size_t glyph_y = 0U; glyph_y < jis_height; ++glyph_y) {
			compose_glyph_row(
				output,
				cell_x,
				cell_y,
				glyph_y,
				glyph.row_halves[
					(glyph_y * jis_bytes_per_row) + half_offset
				],
				color,
				reverse
			);
		}
	}
	return {};
}

} // namespace th03::present
