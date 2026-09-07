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

constexpr std::size_t cdg_header_size = 16U;
constexpr std::size_t cdg_plane_count = 5U;
constexpr std::uint16_t pc98_vram_row_size = 80U;
constexpr std::uint16_t pixels_per_vram_dword = 32U;
constexpr std::uint16_t pixels_per_planar_byte = 8U;
constexpr std::int8_t cdg_colors_and_alpha = 0;

struct ParsedCdg {
	CdgHeader header;
	std::size_t image_count = 0U;
	std::size_t image_size = 0U;
};

AssetError error(
	AssetErrorCode code,
	std::string message,
	std::size_t offset
)
{
	return AssetError{code, std::move(message), offset};
}

std::uint16_t le16(std::span<const std::byte> bytes, std::size_t offset)
{
	return static_cast<std::uint16_t>(
		std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U)
	);
}

std::int16_t le_i16(std::span<const std::byte> bytes, std::size_t offset)
{
	return static_cast<std::int16_t>(le16(bytes, offset));
}

std::expected<ParsedCdg, AssetError> parse_cdg_header(
	std::span<const std::byte> bytes
)
{
	if(bytes.size() < cdg_header_size) {
		return std::unexpected(error(
			AssetErrorCode::truncated,
			"CDG data is shorter than its 16-byte header",
			bytes.size()
		));
	}

	ParsedCdg parsed{};
	auto &header = parsed.header;
	header.bitplane_size = le16(bytes, 0U);
	const auto pixel_width = le_i16(bytes, 2U);
	const auto pixel_height = le_i16(bytes, 4U);
	header.offset_at_bottom_left = le_i16(bytes, 6U);
	header.vram_dword_width = le16(bytes, 8U);
	header.stored_image_count = std::to_integer<std::uint8_t>(bytes[10U]);
	header.plane_layout = static_cast<std::int8_t>(
		std::to_integer<std::uint8_t>(bytes[11U])
	);
	header.reserved_alpha_segment = le16(bytes, 12U);
	header.reserved_color_segment = le16(bytes, 14U);

	if(header.bitplane_size == 0U) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"CDG bitplane size must be nonzero",
			0U
		));
	}
	if(pixel_width <= 0 ||
		(pixel_width % static_cast<std::int16_t>(pixels_per_vram_dword)) != 0) {
		return std::unexpected(error(
			AssetErrorCode::unsupported_layout,
			"CDG width must be a positive multiple of 32 pixels",
			2U
		));
	}
	if(pixel_height <= 0) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"CDG height must be positive",
			4U
		));
	}

	header.pixel_width = static_cast<std::uint16_t>(pixel_width);
	header.pixel_height = static_cast<std::uint16_t>(pixel_height);
	const auto byte_width = static_cast<std::size_t>(header.pixel_width) /
		pixels_per_planar_byte;
	const auto expected_bitplane_size = byte_width * header.pixel_height;
	if(expected_bitplane_size != header.bitplane_size) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"CDG bitplane size does not match its dimensions",
			0U
		));
	}

	const auto expected_bottom_offset =
		static_cast<std::size_t>(pc98_vram_row_size) *
		(static_cast<std::size_t>(header.pixel_height) - 1U);
	if(expected_bottom_offset > static_cast<std::size_t>(
		std::numeric_limits<std::int16_t>::max()
	) || header.offset_at_bottom_left != static_cast<std::int16_t>(
		expected_bottom_offset
	)) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"CDG bottom-left VRAM offset does not match its height",
			6U
		));
	}

	const auto expected_dword_width = static_cast<std::uint16_t>(
		header.pixel_width / pixels_per_vram_dword
	);
	if(header.vram_dword_width != expected_dword_width) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"CDG VRAM dword width does not match its pixel width",
			8U
		));
	}
	if(header.plane_layout != cdg_colors_and_alpha) {
		return std::unexpected(error(
			AssetErrorCode::unsupported_layout,
			"CDG plane layout is not TH03 alpha+B/R/G/E",
			11U
		));
	}
	if(header.reserved_alpha_segment != 0U) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"CDG file header alpha segment must be zero",
			12U
		));
	}
	if(header.reserved_color_segment != 0U) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"CDG file header color segment must be zero",
			14U
		));
	}

	parsed.image_count = (header.stored_image_count == 0U) ?
		1U : static_cast<std::size_t>(header.stored_image_count);
	parsed.image_size = static_cast<std::size_t>(header.bitplane_size) *
		cdg_plane_count;
	if(parsed.image_count > (
		(std::numeric_limits<std::size_t>::max() - cdg_header_size) /
		parsed.image_size
	)) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"CDG payload size overflows the host size type",
			10U
		));
	}
	const auto expected_size = cdg_header_size +
		(parsed.image_count * parsed.image_size);
	if(bytes.size() < expected_size) {
		return std::unexpected(error(
			AssetErrorCode::truncated,
			"CDG payload is truncated",
			bytes.size()
		));
	}
	if(bytes.size() > expected_size) {
		return std::unexpected(error(
			AssetErrorCode::size_mismatch,
			"CDG data has bytes after its declared payload",
			expected_size
		));
	}
	return parsed;
}

PlanarCel4 read_cdg_cel(
	std::span<const std::byte> bytes,
	const ParsedCdg &parsed,
	std::size_t cel_index,
	CdgAlphaPolicy alpha_policy
)
{
	PlanarCel4 cel{};
	cel.bitplane_size = parsed.header.bitplane_size;
	cel.pixel_width = parsed.header.pixel_width;
	cel.pixel_height = parsed.header.pixel_height;
	cel.offset_at_bottom_left = parsed.header.offset_at_bottom_left;
	cel.vram_dword_width = parsed.header.vram_dword_width;

	const auto plane_size = static_cast<std::size_t>(cel.bitplane_size);
	auto cursor = cdg_header_size + (cel_index * parsed.image_size);
	if(alpha_policy == CdgAlphaPolicy::preserve) {
		cel.alpha.emplace();
		cel.alpha->reserve(plane_size);
		for(std::size_t index = 0U; index < plane_size; ++index) {
			cel.alpha->push_back(std::to_integer<std::uint8_t>(bytes[cursor + index]));
		}
	}
	cursor += plane_size;

	for(auto &plane : cel.color_planes) {
		plane.reserve(plane_size);
		for(std::size_t index = 0U; index < plane_size; ++index) {
			plane.push_back(std::to_integer<std::uint8_t>(bytes[cursor + index]));
		}
		cursor += plane_size;
	}
	return cel;
}

} // namespace

std::expected<CdgFile, AssetError> decode_cdg(
	std::span<const std::byte> bytes,
	CdgAlphaPolicy alpha_policy
)
{
	const auto parsed = parse_cdg_header(bytes);
	if(!parsed) {
		return std::unexpected(parsed.error());
	}

	CdgFile file{};
	file.header = parsed->header;
	file.cels.reserve(parsed->image_count);
	for(std::size_t cel_index = 0U; cel_index < parsed->image_count; ++cel_index) {
		file.cels.push_back(read_cdg_cel(bytes, *parsed, cel_index, alpha_policy));
	}
	return file;
}

std::expected<PlanarCel4, AssetError> decode_cdg_cel(
	std::span<const std::byte> bytes,
	std::size_t cel_index,
	CdgAlphaPolicy alpha_policy
)
{
	const auto parsed = parse_cdg_header(bytes);
	if(!parsed) {
		return std::unexpected(parsed.error());
	}
	if(cel_index >= parsed->image_count) {
		return std::unexpected(error(
			AssetErrorCode::index_out_of_range,
			"CDG cel index is outside the declared image count",
			10U
		));
	}
	return read_cdg_cel(bytes, *parsed, cel_index, alpha_policy);
}

} // namespace th03::assets
