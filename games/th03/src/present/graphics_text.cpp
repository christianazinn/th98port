#include "th03/present/graphics_text.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace th03::present {
namespace {

constexpr std::uint8_t graphics_text_spacing_max = 7U;
constexpr std::uint8_t graphics_text_scale_max = 4U;
constexpr std::uint16_t alternate_ascii_row = 0x2900U;
constexpr std::uint16_t alternate_kana_base = 0x2980U;
constexpr std::uint16_t alternate_blank = 0x2B21U;

[[nodiscard]] constexpr bool is_shift_jis_lead(std::uint8_t byte) noexcept
{
	return ((byte >= 0x81U) && (byte <= 0x9FU)) ||
		((byte >= 0xE0U) && (byte <= 0xFCU));
}

[[nodiscard]] constexpr bool is_shift_jis_trail(std::uint8_t byte) noexcept
{
	return ((byte >= 0x40U) && (byte <= 0x7EU)) ||
		((byte >= 0x80U) && (byte <= 0xFCU));
}

[[nodiscard]] constexpr bool is_graphical_ascii(std::uint8_t byte) noexcept
{
	return byte >= 0x21U && byte <= 0x7EU;
}

[[nodiscard]] constexpr bool is_halfwidth_kana(std::uint8_t byte) noexcept
{
	return byte >= 0xA1U && byte <= 0xDFU;
}

[[nodiscard]] constexpr std::uint16_t shift_jis_to_canonical_jis(
	std::uint8_t lead,
	std::uint8_t trail
) noexcept {
	auto first = static_cast<std::uint16_t>(
		((lead <= 0x9FU) ? (lead - 0x81U) : (lead - 0xC1U)) * 2U +
		0x21U
	);
	std::uint16_t second = 0U;
	if(trail >= 0x9FU) {
		++first;
		second = static_cast<std::uint16_t>(trail - 0x7EU);
	} else {
		second = static_cast<std::uint16_t>(
			trail - ((trail >= 0x80U) ? 0x20U : 0x1FU)
		);
	}
	return static_cast<std::uint16_t>((first << 8U) | second);
}

[[nodiscard]] constexpr bool is_alternate_halfwidth_shift_jis(
	std::uint8_t lead,
	std::uint8_t trail
) noexcept {
	const auto source_word = static_cast<std::uint16_t>(
		(static_cast<std::uint16_t>(lead) << 8U) | trail
	);
	return source_word >= 0x8540U && source_word <= 0x869EU;
}

[[nodiscard]] GraphicsTextError text_error(
	GraphicsTextErrorCode code,
	std::size_t byte_offset,
	std::uint16_t canonical_jis = 0U
) noexcept {
	return {code, byte_offset, canonical_jis};
}

[[nodiscard]] constexpr bool valid_weight(GraphicsTextWeight weight) noexcept
{
	switch(weight) {
	case GraphicsTextWeight::normal:
	case GraphicsTextWeight::heavy:
	case GraphicsTextWeight::bold:
	case GraphicsTextWeight::black:
		return true;
	}
	return false;
}

[[nodiscard]] constexpr std::uint16_t weighted_row(
	std::uint16_t source,
	GraphicsTextWeight weight
) noexcept {
	auto row = source;
	switch(weight) {
	case GraphicsTextWeight::normal:
		break;
	case GraphicsTextWeight::heavy:
		row = static_cast<std::uint16_t>(row | (row << 1U));
		break;
	case GraphicsTextWeight::black:
		row = static_cast<std::uint16_t>(row | (row << 1U));
		[[fallthrough]];
	case GraphicsTextWeight::bold: {
		auto temporary = row;
		row = static_cast<std::uint16_t>(row | (row << 1U));
		temporary = static_cast<std::uint16_t>(temporary ^ row);
		row = static_cast<std::uint16_t>(
			row & static_cast<std::uint16_t>(~(temporary << 1U))
		);
		break;
	}}
	return row;
}

[[nodiscard]] std::array<std::uint8_t, 3U> shifted_mask(
	std::uint16_t row,
	std::uint8_t first_bit
) noexcept {
	std::array<std::uint8_t, 3U> result{};
	for(std::uint8_t source_x = 0U; source_x < jis_width; ++source_x) {
		const auto source_mask = static_cast<std::uint16_t>(
			0x8000U >> source_x
		);
		if((row & source_mask) == 0U) {
			continue;
		}
		const auto destination_bit = static_cast<std::uint8_t>(
			first_bit + source_x
		);
		result[destination_bit / 8U] = static_cast<std::uint8_t>(
			result[destination_bit / 8U] |
			static_cast<std::uint8_t>(0x80U >> (destination_bit & 7U))
		);
	}
	return result;
}

} // namespace

PreparedGraphicsText8x16::PreparedGraphicsText8x16(
	std::vector<Glyph> glyphs,
	std::uint32_t pixel_width
) noexcept :
	glyphs_(std::move(glyphs)),
	pixel_width_(pixel_width)
{}

std::uint32_t PreparedGraphicsText8x16::pixel_width() const noexcept
{
	return pixel_width_;
}

std::size_t PreparedGraphicsText8x16::glyph_count() const noexcept
{
	return glyphs_.size();
}

std::expected<PreparedGraphicsText8x16, GraphicsTextError>
prepare_graphics_text_8x16(
	std::span<const std::uint8_t> shift_jis,
	const AnkGlyphProvider &font
) {
	std::vector<PreparedGraphicsText8x16::Glyph> glyphs;
	glyphs.reserve(shift_jis.size());
	std::uint64_t width = 0U;

	std::size_t offset = 0U;
	while(offset < shift_jis.size()) {
		const auto first = shift_jis[offset];
		if(first == 0U) {
			return std::unexpected(text_error(
				GraphicsTextErrorCode::malformed_shift_jis,
				offset
			));
		}

		std::uint16_t canonical_jis = 0U;
		bool halfwidth = true;
		std::size_t source_bytes = 1U;
		if(is_shift_jis_lead(first)) {
			if((offset + 1U) >= shift_jis.size()) {
				return std::unexpected(text_error(
					GraphicsTextErrorCode::malformed_shift_jis,
					offset
				));
			}
			const auto second = shift_jis[offset + 1U];
			if(!is_shift_jis_trail(second)) {
				return std::unexpected(text_error(
					GraphicsTextErrorCode::malformed_shift_jis,
					offset + 1U
				));
			}
			canonical_jis = shift_jis_to_canonical_jis(first, second);
			halfwidth = is_alternate_halfwidth_shift_jis(first, second);
			source_bytes = 2U;
		} else if(is_halfwidth_kana(first)) {
			canonical_jis = static_cast<std::uint16_t>(
				alternate_kana_base + first
			);
		} else if(is_graphical_ascii(first)) {
			canonical_jis = static_cast<std::uint16_t>(
				alternate_ascii_row + first
			);
		} else {
			canonical_jis = alternate_blank;
		}

		const auto source = font.jis_glyph(canonical_jis);
		if(!source) {
			return std::unexpected(text_error(
				GraphicsTextErrorCode::missing_jis_glyph,
				offset,
				canonical_jis
			));
		}
		PreparedGraphicsText8x16::Glyph glyph{};
		glyph.width = halfwidth ? ank_width : jis_width;
		for(std::size_t row = 0U; row < jis_height; ++row) {
			const auto left = source->row_halves[row * jis_bytes_per_row];
			const auto right = source->row_halves[
				(row * jis_bytes_per_row) + 1U
			];
			glyph.rows[row] = halfwidth ?
				static_cast<std::uint16_t>(left) << 8U :
				static_cast<std::uint16_t>(
					(static_cast<std::uint16_t>(left) << 8U) | right
				);
		}
		width += glyph.width;
		if(width > std::numeric_limits<std::uint32_t>::max()) {
			return std::unexpected(text_error(
				GraphicsTextErrorCode::malformed_shift_jis,
				offset,
				canonical_jis
			));
		}
		glyphs.push_back(glyph);
		offset += source_bytes;
	}

	return PreparedGraphicsText8x16(
		std::move(glyphs),
		static_cast<std::uint32_t>(width)
	);
}

std::expected<PreparedGraphicsText8x16, GraphicsTextError>
prepare_graphics_glyphs_8x16(std::span<const GraphicsTextGlyph8x16> source)
{
	std::vector<PreparedGraphicsText8x16::Glyph> glyphs;
	glyphs.reserve(source.size());
	std::uint64_t width = 0U;
	for(std::size_t index = 0U; index < source.size(); ++index) {
		if(source[index].width != ank_width &&
			source[index].width != jis_width) {
			return std::unexpected(text_error(
				GraphicsTextErrorCode::malformed_shift_jis, index
			));
		}
		glyphs.push_back({source[index].rows, source[index].width});
		width += source[index].width;
		if(width > std::numeric_limits<std::uint32_t>::max()) {
			return std::unexpected(text_error(
				GraphicsTextErrorCode::malformed_shift_jis, index
			));
		}
	}
	return PreparedGraphicsText8x16(
		std::move(glyphs), static_cast<std::uint32_t>(width)
	);
}

bool graphics_text_put_8x16(
	Raster &raster,
	Point position,
	const PreparedGraphicsText8x16 &text,
	GraphicsTextStyle style
) noexcept {
	const auto tile = GrcgTile::solid(style.color);
	if(!tile || !valid_weight(style.weight) ||
		style.spacing > graphics_text_spacing_max) {
		return false;
	}
	if(text.glyphs_.empty()) {
		return text.pixel_width_ == 0U;
	}
	if(position.x < 0 || position.y < 0 ||
		position.y > static_cast<std::int32_t>(graphics_height - jis_height)) {
		return false;
	}

	std::int64_t cursor = position.x;
	std::uint64_t prepared_width = 0U;
	for(const auto &glyph : text.glyphs_) {
		if(glyph.width != ank_width && glyph.width != jis_width) {
			return false;
		}
		prepared_width += glyph.width;
		const auto right = cursor + glyph.width;
		if(cursor < 0 || right > graphics_width) {
			return false;
		}
		cursor = right + style.spacing;
	}
	if(prepared_width != text.pixel_width_) {
		return false;
	}

	cursor = position.x;
	for(const auto &glyph : text.glyphs_) {
		const auto first_bit = static_cast<std::uint8_t>(cursor & 7);
		const auto byte_width = static_cast<std::uint16_t>(
			(first_bit == 0U) ? 16U : 24U
		);
		const auto destination_x = static_cast<std::int32_t>(cursor - first_bit);
		for(std::size_t row = 0U; row < jis_height; ++row) {
			const auto mask_bytes = shifted_mask(
				weighted_row(glyph.rows[row], style.weight),
				first_bit
			);
			const BitSource mask{
				std::span<const std::uint8_t>(mask_bytes).first(byte_width / 8U),
				byte_width,
				1U,
				static_cast<std::size_t>(byte_width / 8U),
				PackedRowOrder::top_to_bottom,
			};
			if(!raster.grcg_write(
				mask,
				{
					destination_x,
					position.y + static_cast<std::int32_t>(row),
				},
				GrcgWriteMode::read_modify_write,
				*tile
			)) {
				return false;
			}
		}
		cursor += static_cast<std::int64_t>(glyph.width) + style.spacing;
	}
	return true;
}

bool graphics_text_put_scaled_8x16(
	Raster &raster,
	Point position,
	const PreparedGraphicsText8x16 &text,
	GraphicsTextStyle style,
	std::uint8_t scale
) noexcept {
	const auto tile = GrcgTile::solid(style.color);
	if(!tile || !valid_weight(style.weight) ||
		style.spacing > graphics_text_spacing_max || scale == 0U ||
		scale > graphics_text_scale_max) {
		return false;
	}
	if(text.glyphs_.empty()) {
		return text.pixel_width_ == 0U;
	}
	const auto scaled_height = static_cast<std::uint32_t>(jis_height) * scale;
	if(position.x < 0 || position.y < 0 ||
		position.y > static_cast<std::int32_t>(
			graphics_height - scaled_height
		)) {
		return false;
	}

	std::int64_t cursor = position.x;
	std::uint64_t prepared_width = 0U;
	for(const auto &glyph : text.glyphs_) {
		if(glyph.width != ank_width && glyph.width != jis_width) {
			return false;
		}
		prepared_width += glyph.width;
		const auto right = cursor +
			(static_cast<std::int64_t>(glyph.width) * scale);
		if(cursor < 0 || right > graphics_width) {
			return false;
		}
		cursor = right +
			(static_cast<std::int64_t>(style.spacing) * scale);
	}
	if(prepared_width != text.pixel_width_) {
		return false;
	}

	cursor = position.x;
	for(const auto &glyph : text.glyphs_) {
		for(std::size_t source_y = 0U; source_y < jis_height; ++source_y) {
			const auto row = weighted_row(glyph.rows[source_y], style.weight);
			for(std::uint8_t source_x = 0U; source_x < glyph.width; ++source_x) {
				if((row & static_cast<std::uint16_t>(0x8000U >> source_x)) == 0U) {
					continue;
				}
				for(std::uint8_t y = 0U; y < scale; ++y) {
					for(std::uint8_t x = 0U; x < scale; ++x) {
						if(!raster.plot({
							static_cast<std::int32_t>(cursor) +
								(static_cast<std::int32_t>(source_x) * scale) + x,
							position.y +
								(static_cast<std::int32_t>(source_y) * scale) + y,
						}, style.color)) {
							return false;
						}
					}
				}
			}
		}
		cursor += (static_cast<std::int64_t>(glyph.width) * scale) +
			(static_cast<std::int64_t>(style.spacing) * scale);
	}
	return true;
}

} // namespace th03::present
