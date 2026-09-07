#include "th03/assets/decode.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace th03::assets {
namespace {

constexpr std::uint16_t mrs_pixel_width = 288U;
constexpr std::uint16_t mrs_pixel_height = 184U;
constexpr std::size_t mrs_byte_width = mrs_pixel_width / 8U;
constexpr std::size_t mrs_bitplane_size =
	mrs_byte_width * mrs_pixel_height;
constexpr std::size_t mrs_plane_count = 5U;
constexpr std::size_t mrs_file_size = mrs_bitplane_size * mrs_plane_count;
constexpr std::int16_t mrs_offset_at_bottom_left =
	static_cast<std::int16_t>((mrs_pixel_height - 1U) * 80U);
constexpr std::uint16_t mrs_vram_dword_width = mrs_pixel_width / 32U;

AssetError error(
	AssetErrorCode code,
	std::string message,
	std::size_t offset
)
{
	return AssetError{code, std::move(message), offset};
}

std::uint8_t reverse_bits(std::uint8_t value)
{
	value = static_cast<std::uint8_t>((value >> 4U) | (value << 4U));
	value = static_cast<std::uint8_t>(
		((value & 0xCCU) >> 2U) | ((value & 0x33U) << 2U)
	);
	return static_cast<std::uint8_t>(
		((value & 0xAAU) >> 1U) | ((value & 0x55U) << 1U)
	);
}

std::vector<std::uint8_t> decode_plane(
	std::span<const std::byte> bytes
)
{
	std::vector<std::uint8_t> plane(mrs_bitplane_size);
	for(std::size_t index = 0U; index < plane.size(); ++index) {
		plane[index] = std::to_integer<std::uint8_t>(bytes[index]);
	}
	return plane;
}

void horizontal_flip_plane(std::vector<std::uint8_t> &plane)
{
	for(std::size_t row = 0U; row < mrs_pixel_height; ++row) {
		const auto row_offset = row * mrs_byte_width;
		for(std::size_t column = 0U; column < (mrs_byte_width / 2U); ++column) {
			const auto left = row_offset + column;
			const auto right = row_offset + (mrs_byte_width - 1U - column);
			const auto left_value = reverse_bits(plane[left]);
			plane[left] = reverse_bits(plane[right]);
			plane[right] = left_value;
		}
	}
}

} // namespace

std::expected<MrsImage, AssetError> decode_mrs(
	std::span<const std::byte> bytes
)
{
	if(bytes.size() < mrs_file_size) {
		return std::unexpected(error(
			AssetErrorCode::truncated,
			"MRS data is shorter than its fixed five-plane payload",
			bytes.size()
		));
	}
	if(bytes.size() > mrs_file_size) {
		return std::unexpected(error(
			AssetErrorCode::size_mismatch,
			"MRS data has bytes after its fixed five-plane payload",
			mrs_file_size
		));
	}

	MrsImage image{};
	auto &cel = image.cel;
	cel.bitplane_size = static_cast<std::uint16_t>(mrs_bitplane_size);
	cel.pixel_width = mrs_pixel_width;
	cel.pixel_height = mrs_pixel_height;
	cel.offset_at_bottom_left = mrs_offset_at_bottom_left;
	cel.vram_dword_width = mrs_vram_dword_width;
	cel.row_order = PlanarRowOrder::bottom_to_top;
	cel.bit_order = PlanarBitOrder::most_significant_bit_left;

	std::size_t cursor = 0U;
	cel.alpha = decode_plane(bytes.subspan(cursor, mrs_bitplane_size));
	cursor += mrs_bitplane_size;
	for(auto &plane : cel.color_planes) {
		plane = decode_plane(bytes.subspan(cursor, mrs_bitplane_size));
		cursor += mrs_bitplane_size;
	}
	return image;
}

void horizontal_flip(MrsImage &image)
{
	if(image.cel.alpha) {
		horizontal_flip_plane(*image.cel.alpha);
	}
	for(auto &plane : image.cel.color_planes) {
		horizontal_flip_plane(plane);
	}
	image.orientation = (image.orientation == MrsOrientation::as_stored) ?
		MrsOrientation::horizontally_flipped : MrsOrientation::as_stored;
}

} // namespace th03::assets
