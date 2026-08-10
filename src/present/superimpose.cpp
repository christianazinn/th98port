#include "th03/present/superimpose.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <variant>

namespace th03::present {
namespace {

[[nodiscard]] bool valid_sheet(
	const assets::BfntSheet &sheet,
	std::size_t &plane_size
) noexcept
{
	if(sheet.consumer != assets::BfntConsumerPolicy::superimpose ||
		sheet.header.pixel_width == 0U || sheet.header.pixel_height == 0U ||
		(sheet.header.pixel_width % 8U) != 0U ||
		sheet.header.first_code > sheet.header.last_code ||
		sheet.transparent_color > 15U) {
		return false;
	}

	const auto glyph_count = static_cast<std::size_t>(
		static_cast<std::uint32_t>(sheet.header.last_code) -
		static_cast<std::uint32_t>(sheet.header.first_code) + 1U
	);
	const auto stride = static_cast<std::size_t>(
		sheet.header.pixel_width / 8U
	);
	if(stride > std::numeric_limits<std::uint8_t>::max() ||
		sheet.header.pixel_height > std::numeric_limits<std::uint8_t>::max() ||
		glyph_count > 512U ||
		stride > (std::numeric_limits<std::size_t>::max() /
		sheet.header.pixel_height)) {
		return false;
	}
	plane_size = stride * sheet.header.pixel_height;
	if(sheet.glyphs.size() != glyph_count ||
		plane_size > (std::numeric_limits<std::size_t>::max() / glyph_count)) {
		return false;
	}

	for(std::size_t index = 0U; index < glyph_count; ++index) {
		const auto expected_code = static_cast<std::uint16_t>(
			static_cast<std::uint32_t>(sheet.header.first_code) + index
		);
		const auto &glyph = sheet.glyphs[index];
		const auto *pixels = std::get_if<assets::PlanarBitmap4>(&glyph.pixels);
		if(glyph.code != expected_code || pixels == nullptr ||
			pixels->row_order != assets::PlanarRowOrder::top_to_bottom ||
			pixels->bit_order !=
				assets::PlanarBitOrder::most_significant_bit_left) {
			return false;
		}
		for(const auto &plane : pixels->color_planes) {
			if(plane.size() != plane_size) {
				return false;
			}
		}
	}
	return true;
}

} // namespace

PreparedSuperimposeSheet::PreparedSuperimposeSheet(
	std::uint16_t width,
	std::uint16_t height,
	std::uint16_t first_code,
	std::uint16_t last_code,
	std::size_t plane_size,
	std::array<std::vector<std::uint8_t>, planar_plane_count> color_planes,
	std::vector<std::uint8_t> opacity
) noexcept :
	width_(width),
	height_(height),
	first_code_(first_code),
	last_code_(last_code),
	plane_size_(plane_size),
	color_planes_(std::move(color_planes)),
	opacity_(std::move(opacity))
{}

std::uint16_t PreparedSuperimposeSheet::first_code() const noexcept
{
	return first_code_;
}

std::uint16_t PreparedSuperimposeSheet::last_code() const noexcept
{
	return last_code_;
}

std::optional<PreparedSuperimposeSheet> prepare_superimpose_sheet(
	const assets::BfntSheet &sheet
)
{
	std::size_t plane_size = 0U;
	if(!valid_sheet(sheet, plane_size)) {
		return std::nullopt;
	}

	const auto total_plane_size = plane_size * sheet.glyphs.size();
	std::array<
		std::vector<std::uint8_t>,
		planar_plane_count
	> color_planes{};
	for(auto &plane : color_planes) {
		plane.resize(total_plane_size);
	}
	std::vector<std::uint8_t> opacity(total_plane_size, 0U);
	for(std::size_t glyph_index = 0U;
		glyph_index < sheet.glyphs.size();
		++glyph_index) {
		const auto &pixels = std::get<assets::PlanarBitmap4>(
			sheet.glyphs[glyph_index].pixels
		);
		const auto mask_offset = glyph_index * plane_size;
		for(std::size_t plane = 0U; plane < planar_plane_count; ++plane) {
			std::copy(
				pixels.color_planes[plane].begin(),
				pixels.color_planes[plane].end(),
				color_planes[plane].begin() +
					static_cast<std::ptrdiff_t>(mask_offset)
			);
		}
		for(std::size_t offset = 0U; offset < plane_size; ++offset) {
			std::uint8_t opaque = 0U;
			for(std::size_t plane = 0U; plane < planar_plane_count; ++plane) {
				const auto transparent_plane =
					((sheet.transparent_color & (1U << plane)) != 0U) ?
						0xFFU : 0x00U;
				opaque = static_cast<std::uint8_t>(
					opaque |
					(pixels.color_planes[plane][offset] ^ transparent_plane)
				);
			}
			opacity[mask_offset + offset] = opaque;
		}
	}
	return PreparedSuperimposeSheet(
		sheet.header.pixel_width,
		sheet.header.pixel_height,
		sheet.header.first_code,
		sheet.header.last_code,
		plane_size,
		std::move(color_planes),
		std::move(opacity)
	);
}

bool superimpose_put(
	Raster &raster,
	const PreparedSuperimposeSheet &sheet,
	Point position,
	std::uint16_t glyph_code
) noexcept
{
	const auto glyph_count = static_cast<std::size_t>(
		static_cast<std::uint32_t>(sheet.last_code_) - sheet.first_code_ + 1U
	);
	const auto total_plane_size = sheet.plane_size_ * glyph_count;
	if(sheet.plane_size_ == 0U || glyph_code < sheet.first_code() ||
		glyph_code > sheet.last_code() ||
		sheet.opacity_.size() != total_plane_size) {
		return false;
	}
	for(const auto &plane : sheet.color_planes_) {
		if(plane.size() != total_plane_size) {
			return false;
		}
	}
	const auto glyph_index = static_cast<std::size_t>(
		static_cast<std::uint32_t>(glyph_code) - sheet.first_code()
	);
	const auto mask_offset = glyph_index * sheet.plane_size_;
	PlanarSource colors{};
	for(std::size_t plane = 0U; plane < planar_plane_count; ++plane) {
		colors.planes[plane] = std::span<const std::uint8_t>(
			sheet.color_planes_[plane]
		).subspan(mask_offset, sheet.plane_size_);
	}
	colors.width = sheet.width_;
	colors.height = sheet.height_;
	colors.stride = static_cast<std::size_t>(sheet.width_ / 8U);
	colors.row_order = PackedRowOrder::top_to_bottom;
	const BitSource opacity{
		std::span<const std::uint8_t>(sheet.opacity_).subspan(
			mask_offset,
			sheet.plane_size_
		),
		sheet.width_,
		sheet.height_,
		static_cast<std::size_t>(sheet.width_ / 8U),
		PackedRowOrder::top_to_bottom,
	};
	return raster.blit_planar_masked(
		colors,
		opacity,
		position
	);
}

} // namespace th03::present
