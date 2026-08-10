#include "th03/assets/decode.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace th03::assets {
namespace {

constexpr std::size_t bfnt_header_size = 32U;
constexpr std::size_t bfnt_palette_size = 48U;
constexpr std::uint8_t bfnt_palette_flag = 0x80U;
constexpr std::uint8_t bfnt_color_count_mask = 0x7FU;
constexpr std::uint8_t bfnt_superimpose_color_value = 3U;
constexpr std::uint8_t bfnt_transparent_color_record = 0x10U;
constexpr std::size_t bfnt_superimpose_plane_count = 4U;
constexpr std::size_t master_max_pattern_count = 512U;

AssetError error(
	AssetErrorCode code,
	std::string message,
	std::size_t offset
)
{
	return AssetError{code, std::move(message), offset};
}

std::uint8_t u8(std::span<const std::byte> bytes, std::size_t offset)
{
	return std::to_integer<std::uint8_t>(bytes[offset]);
}

std::uint16_t le16(std::span<const std::byte> bytes, std::size_t offset)
{
	return static_cast<std::uint16_t>(
		u8(bytes, offset) |
		(static_cast<std::uint16_t>(u8(bytes, offset + 1U)) << 8U)
	);
}

std::uint32_t le32(std::span<const std::byte> bytes, std::size_t offset)
{
	return static_cast<std::uint32_t>(u8(bytes, offset)) |
		(static_cast<std::uint32_t>(u8(bytes, offset + 1U)) << 8U) |
		(static_cast<std::uint32_t>(u8(bytes, offset + 2U)) << 16U) |
		(static_cast<std::uint32_t>(u8(bytes, offset + 3U)) << 24U);
}

std::expected<BfntHeader, AssetError> parse_header(
	std::span<const std::byte> bytes
)
{
	if(bytes.size() < bfnt_header_size) {
		return std::unexpected(error(
			AssetErrorCode::truncated,
			"BFNT data is shorter than its 32-byte header",
			bytes.size()
		));
	}

	constexpr std::array<std::byte, 5U> identifier{
		std::byte{'B'}, std::byte{'F'}, std::byte{'N'}, std::byte{'T'},
		std::byte{0x1AU},
	};
	for(std::size_t index = 0U; index < identifier.size(); ++index) {
		if(bytes[index] != identifier[index]) {
			return std::unexpected(error(
				AssetErrorCode::bad_magic,
				"BFNT identifier is not 'BFNT' followed by 0x1A",
				index
			));
		}
	}

	BfntHeader header{};
	header.identifier = identifier;
	header.color_flags = u8(bytes, 5U);
	header.version = u8(bytes, 6U);
	header.reserved = u8(bytes, 7U);
	header.pixel_width = le16(bytes, 8U);
	header.pixel_height = le16(bytes, 10U);
	header.first_code = le16(bytes, 12U);
	header.last_code = le16(bytes, 14U);
	for(std::size_t index = 0U; index < header.font_name.size(); ++index) {
		header.font_name[index] = bytes[16U + index];
	}
	header.timestamp = le32(bytes, 24U);
	header.extended_header_size = le16(bytes, 28U);
	header.first_extended_record_size = le16(bytes, 30U);

	if(header.pixel_width == 0U || (header.pixel_width % 8U) != 0U) {
		return std::unexpected(error(
			AssetErrorCode::unsupported_layout,
			"BFNT width must be a positive multiple of 8 pixels",
			8U
		));
	}
	if(header.pixel_height == 0U) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"BFNT height must be positive",
			10U
		));
	}
	if((header.pixel_width / 8U) >
		static_cast<std::uint16_t>(std::numeric_limits<std::uint8_t>::max()) ||
		header.pixel_height >
		static_cast<std::uint16_t>(std::numeric_limits<std::uint8_t>::max())) {
		return std::unexpected(error(
			AssetErrorCode::unsupported_layout,
			"BFNT dimensions exceed master.lib's packed pattern size",
			8U
		));
	}
	if(header.last_code < header.first_code) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"BFNT glyph range ends before it starts",
			14U
		));
	}
	return header;
}

std::expected<BfntExtendedHeader, AssetError> parse_extended_header(
	std::span<const std::byte> bytes,
	const BfntHeader &header,
	BfntConsumerPolicy consumer
)
{
	BfntExtendedHeader result{};
	if(header.extended_header_size == 0U) {
		return result;
	}
	const auto extension_size = static_cast<std::size_t>(
		header.extended_header_size
	);
	if(extension_size > (bytes.size() - bfnt_header_size)) {
		return std::unexpected(error(
			AssetErrorCode::truncated,
			"BFNT extended header is truncated",
			bytes.size()
		));
	}
	const auto extension = bytes.subspan(bfnt_header_size, extension_size);
	result.bytes.assign(extension.begin(), extension.end());
	if(consumer == BfntConsumerPolicy::gaiji) {
		// gaiji_entry_bfnt() seeks past this region without analyzing it.
		return result;
	}

	std::size_t cursor = 0U;
	std::size_t record_size = header.first_extended_record_size;
	while(cursor < extension.size()) {
		const auto record_id = u8(extension, cursor);
		if(record_id == bfnt_transparent_color_record) {
			if(record_size < 4U) {
				return std::unexpected(error(
					AssetErrorCode::malformed_stream,
					"BFNT transparent-color extension record is too short",
					bfnt_header_size + cursor
				));
			}
			if(record_size > (extension.size() - cursor)) {
				return std::unexpected(error(
					AssetErrorCode::truncated,
					"BFNT transparent-color extension record is truncated",
					bfnt_header_size + cursor
				));
			}
			result.transparent_color = static_cast<std::uint8_t>(
				u8(extension, cursor + 3U) & 0x0FU
			);
			return result;
		}

		if(record_size < 3U) {
			return std::unexpected(error(
				AssetErrorCode::malformed_stream,
				"BFNT extended-header record is too short",
				bfnt_header_size + cursor
			));
		}
		if(record_size > (extension.size() - cursor)) {
			return std::unexpected(error(
				AssetErrorCode::truncated,
				"BFNT extended-header record exceeds the declared extension",
				bfnt_header_size + cursor
			));
		}
		const auto next_record_size = static_cast<std::size_t>(le16(
			extension,
			cursor + record_size - 2U
		));
		cursor += record_size;
		if(cursor == extension.size() || next_record_size == 0U) {
			return result;
		}
		record_size = next_record_size;
	}
	return result;
}

Palette16Rgb4 parse_palette(std::span<const std::byte> bytes)
{
	Palette16Rgb4 palette{};
	for(std::size_t color = 0U; color < palette.colors.size(); ++color) {
		const auto offset = color * 3U;
		const auto blue = static_cast<std::uint8_t>(u8(bytes, offset) >> 4U);
		const auto red = static_cast<std::uint8_t>(u8(bytes, offset + 1U) >> 4U);
		const auto green = static_cast<std::uint8_t>(u8(bytes, offset + 2U) >> 4U);
		palette.colors[color] = Rgb4{red, green, blue};
	}
	return palette;
}

std::expected<std::size_t, AssetError> validate_consumer(
	const BfntHeader &header,
	BfntConsumerPolicy consumer
)
{
	const auto glyph_count =
		static_cast<std::size_t>(header.last_code - header.first_code) + 1U;
	if(consumer == BfntConsumerPolicy::gaiji) {
		// gaiji_entry_bfnt() describes BFNT 1.6 in its comment but does not
		// inspect the version byte. It checks exactly these four domain fields.
		if(header.color_flags != 0U || header.pixel_width != 16U ||
			header.pixel_height != 16U || header.first_code != 0U ||
			header.last_code != 255U) {
			return std::unexpected(error(
				AssetErrorCode::unsupported_layout,
				"BFNT gaiji consumer requires monochrome 16x16 codes 0 through 255",
				5U
			));
		}
		return glyph_count;
	}

	if((header.color_flags & bfnt_color_count_mask) !=
		bfnt_superimpose_color_value) {
		return std::unexpected(error(
			AssetErrorCode::unsupported_layout,
			"BFNT superimpose consumer requires four-bit indexed color",
			5U
		));
	}
	if(glyph_count > master_max_pattern_count) {
		return std::unexpected(error(
			AssetErrorCode::unsupported_layout,
			"BFNT glyph count exceeds master.lib's pattern capacity",
			12U
		));
	}
	return glyph_count;
}

MonochromeBitmap1 decode_monochrome(
	std::span<const std::byte> bytes
)
{
	MonochromeBitmap1 bitmap{};
	bitmap.bits.reserve(bytes.size());
	for(const auto value : bytes) {
		bitmap.bits.push_back(std::to_integer<std::uint8_t>(value));
	}
	return bitmap;
}

PlanarBitmap4 decode_superimpose(
	std::span<const std::byte> bytes,
	std::size_t pixel_count,
	std::size_t bitplane_size
)
{
	PlanarBitmap4 bitmap{};
	for(auto &plane : bitmap.color_planes) {
		plane.assign(bitplane_size, 0U);
	}
	for(std::size_t pixel = 0U; pixel < pixel_count; ++pixel) {
		const auto packed = u8(bytes, pixel / 2U);
		const auto color = static_cast<std::uint8_t>(
			((pixel & 1U) == 0U) ? (packed >> 4U) : (packed & 0x0FU)
		);
		const auto destination_byte = pixel / 8U;
		const auto destination_mask = static_cast<std::uint8_t>(
			0x80U >> (pixel & 7U)
		);
		for(std::size_t plane = 0U; plane < bfnt_superimpose_plane_count; ++plane) {
			if((color & static_cast<std::uint8_t>(1U << plane)) != 0U) {
				bitmap.color_planes[plane][destination_byte] |= destination_mask;
			}
		}
	}
	return bitmap;
}

} // namespace

std::expected<BfntSheet, AssetError> decode_bfnt(
	std::span<const std::byte> bytes,
	BfntConsumerPolicy consumer
)
{
	const auto header = parse_header(bytes);
	if(!header) {
		return std::unexpected(header.error());
	}
	const auto glyph_count = validate_consumer(*header, consumer);
	if(!glyph_count) {
		return std::unexpected(glyph_count.error());
	}
	const auto extended_header = parse_extended_header(bytes, *header, consumer);
	if(!extended_header) {
		return std::unexpected(extended_header.error());
	}

	std::size_t cursor = bfnt_header_size + header->extended_header_size;
	std::optional<Palette16Rgb4> palette;
	if((header->color_flags & bfnt_palette_flag) != 0U) {
		if((bytes.size() - cursor) < bfnt_palette_size) {
			return std::unexpected(error(
				AssetErrorCode::truncated,
				"BFNT palette is truncated",
				bytes.size()
			));
		}
		palette = parse_palette(bytes.subspan(cursor, bfnt_palette_size));
		cursor += bfnt_palette_size;
	}

	const auto byte_width = static_cast<std::size_t>(header->pixel_width / 8U);
	const auto bitplane_size = byte_width * header->pixel_height;
	const auto plane_count = (consumer == BfntConsumerPolicy::gaiji) ?
		1U : bfnt_superimpose_plane_count;
	if(bitplane_size > (std::numeric_limits<std::uint16_t>::max() / plane_count)) {
		return std::unexpected(error(
			AssetErrorCode::unsupported_layout,
			"BFNT glyph payload exceeds master.lib's 16-bit read size",
			8U
		));
	}
	const auto glyph_size = bitplane_size * plane_count;
	if(*glyph_count > (
		(std::numeric_limits<std::size_t>::max() - cursor) / glyph_size
	)) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"BFNT payload size overflows the host size type",
			12U
		));
	}
	const auto expected_size = cursor + (*glyph_count * glyph_size);
	if(bytes.size() < expected_size) {
		return std::unexpected(error(
			AssetErrorCode::truncated,
			"BFNT glyph payload is truncated",
			bytes.size()
		));
	}
	if(bytes.size() > expected_size) {
		return std::unexpected(error(
			AssetErrorCode::size_mismatch,
			"BFNT data has bytes after its declared glyph payload",
			expected_size
		));
	}

	BfntSheet sheet{};
	sheet.header = *header;
	sheet.consumer = consumer;
	sheet.extended_header = *extended_header;
	sheet.palette = palette;
	sheet.transparent_color =
		extended_header->transparent_color.value_or(0U);
	sheet.glyphs.reserve(*glyph_count);
	const auto pixel_count = static_cast<std::size_t>(header->pixel_width) *
		header->pixel_height;
	for(std::size_t glyph_index = 0U; glyph_index < *glyph_count; ++glyph_index) {
		BfntGlyph glyph{};
		glyph.code = static_cast<std::uint16_t>(
			static_cast<std::size_t>(header->first_code) + glyph_index
		);
		const auto glyph_bytes = bytes.subspan(cursor, glyph_size);
		if(consumer == BfntConsumerPolicy::gaiji) {
			glyph.pixels = decode_monochrome(glyph_bytes);
		} else {
			glyph.pixels = decode_superimpose(
				glyph_bytes,
				pixel_count,
				bitplane_size
			);
		}
		sheet.glyphs.push_back(std::move(glyph));
		cursor += glyph_size;
	}
	return sheet;
}

} // namespace th03::assets
