#include "th03/present/raster.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace th03::present {
namespace {

struct ClippedMapping {
	std::int64_t x_begin = 0;
	std::int64_t x_end = 0;
	std::int64_t y_begin = 0;
	std::int64_t y_end = 0;
};

[[nodiscard]] constexpr std::size_t packed_row_bytes(
	std::uint16_t width
) noexcept {
	return (static_cast<std::size_t>(width) + 7U) / 8U;
}

[[nodiscard]] bool valid_packed_size(
	std::size_t size,
	std::uint16_t width,
	std::uint16_t height,
	std::size_t stride
) noexcept {
	if((width == 0U) || (height == 0U)) {
		return true;
	}
	const auto row_bytes = packed_row_bytes(width);
	if(stride < row_bytes) {
		return false;
	}
	const auto rows_before_last = static_cast<std::size_t>(height - 1U);
	if((rows_before_last != 0U) &&
		(stride > ((std::numeric_limits<std::size_t>::max() - row_bytes) /
		 rows_before_last))) {
		return false;
	}
	return size >= ((rows_before_last * stride) + row_bytes);
}

[[nodiscard]] bool valid_source(BitSource source) noexcept {
	return ((source.row_order == PackedRowOrder::top_to_bottom) ||
		(source.row_order == PackedRowOrder::bottom_to_top)) &&
		valid_packed_size(
		source.bits.size(),
		source.width,
		source.height,
		source.stride
	);
}

[[nodiscard]] bool valid_source(PlanarSource source) noexcept {
	if((source.row_order != PackedRowOrder::top_to_bottom) &&
		(source.row_order != PackedRowOrder::bottom_to_top)) {
		return false;
	}
	for(const auto plane : source.planes) {
		if(!valid_packed_size(
			plane.size(),
			source.width,
			source.height,
			source.stride
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr bool valid_mode(PlanarWriteMode mode) noexcept {
	return (mode == PlanarWriteMode::replace) ||
		(mode == PlanarWriteMode::or_planes);
}

[[nodiscard]] constexpr bool valid_mode(GrcgWriteMode mode) noexcept {
	return (mode == GrcgWriteMode::tile_direct) ||
		(mode == GrcgWriteMode::read_modify_write);
}

[[nodiscard]] bool page_index(Page page, std::size_t &index) noexcept {
	switch(page) {
	case Page::zero:
		index = 0U;
		return true;
	case Page::one:
		index = 1U;
		return true;
	}
	return false;
}

[[nodiscard]] constexpr bool plane_index(
	PlanarPlane plane,
	std::size_t &index
) noexcept {
	switch(plane) {
	case PlanarPlane::blue:
		index = 0U;
		return true;
	case PlanarPlane::red:
		index = 1U;
		return true;
	case PlanarPlane::green:
		index = 2U;
		return true;
	case PlanarPlane::intensity:
		index = 3U;
		return true;
	}
	return false;
}

[[nodiscard]] ClippedMapping clipped_mapping(
	std::uint16_t source_width,
	std::uint16_t source_height,
	Point destination
) noexcept {
	const auto destination_x = static_cast<std::int64_t>(destination.x);
	const auto destination_y = static_cast<std::int64_t>(destination.y);
	ClippedMapping result{};
	result.x_begin = std::max(std::int64_t{0}, -destination_x);
	result.x_end = std::min(
		static_cast<std::int64_t>(source_width),
		static_cast<std::int64_t>(graphics_width) - destination_x
	);
	result.y_begin = std::max(std::int64_t{0}, -destination_y);
	result.y_end = std::min(
		static_cast<std::int64_t>(source_height),
		static_cast<std::int64_t>(graphics_height) - destination_y
	);
	return result;
}

[[nodiscard]] bool source_bit(
	std::span<const std::uint8_t> bits,
	std::size_t stride,
	std::size_t x,
	std::size_t y,
	std::uint16_t height,
	PackedRowOrder row_order
) noexcept {
	const auto stored_y = (row_order == PackedRowOrder::top_to_bottom) ?
		y : (static_cast<std::size_t>(height) - 1U - y);
	const auto value = bits[(stored_y * stride) + (x / 8U)];
	const auto shift = static_cast<unsigned int>(7U - (x & 7U));
	return ((value >> shift) & 1U) != 0U;
}

[[nodiscard]] Index4 planar_pixel(
	PlanarSource source,
	std::size_t x,
	std::size_t y
) noexcept {
	Index4 color = 0U;
	for(std::size_t plane = 0U; plane < planar_plane_count; ++plane) {
		if(source_bit(
			source.planes[plane],
			source.stride,
			x,
			y,
			source.height,
			source.row_order
		)) {
			color = static_cast<Index4>(color | (1U << plane));
		}
	}
	return color;
}

[[nodiscard]] Index4 tile_pixel(GrcgTile tile, std::size_t x) noexcept {
	Index4 color = 0U;
	for(std::size_t plane = 0U; plane < planar_plane_count; ++plane) {
		const auto shift = static_cast<unsigned int>(7U - (x & 7U));
		if(((tile.planes[plane] >> shift) & 1U) != 0U) {
			color = static_cast<Index4>(color | (1U << plane));
		}
	}
	return color;
}

[[nodiscard]] std::size_t page_offset(
	std::int64_t x,
	std::int64_t y
) noexcept {
	return (static_cast<std::size_t>(y) * graphics_width) +
		static_cast<std::size_t>(x);
}

} // namespace

std::optional<GrcgTile> GrcgTile::solid(std::uint8_t color_index) noexcept {
	if(color_index > 0x0FU) {
		return std::nullopt;
	}
	GrcgTile result{};
	for(std::size_t plane = 0U; plane < planar_plane_count; ++plane) {
		result.planes[plane] =
			(((color_index >> plane) & 1U) != 0U) ? 0xFFU : 0x00U;
	}
	return result;
}

bool Raster::blit_planar(
	PlanarSource source,
	Point destination,
	PlanarWriteMode mode
) noexcept {
	if(!valid_source(source) || !valid_mode(mode)) {
		return false;
	}
	std::size_t access = 0U;
	if(!page_index(access_page_, access)) {
		return false;
	}

	const auto region = clipped_mapping(source.width, source.height, destination);
	for(auto y = region.y_begin; y < region.y_end; ++y) {
		const auto source_y = static_cast<std::size_t>(y);
		const auto destination_y = static_cast<std::int64_t>(destination.y) + y;
		for(auto x = region.x_begin; x < region.x_end; ++x) {
			const auto source_x = static_cast<std::size_t>(x);
			const auto destination_x =
				static_cast<std::int64_t>(destination.x) + x;
			const auto source_color = planar_pixel(source, source_x, source_y);
			auto &destination_color =
				pages_[access][page_offset(destination_x, destination_y)];
			if(mode == PlanarWriteMode::replace) {
				destination_color = source_color;
			} else {
				destination_color = static_cast<Index4>(
					destination_color | source_color
				);
			}
		}
	}
	return true;
}

bool Raster::write_plane(
	PlanarPlane plane,
	BitSource source,
	Point destination
) noexcept {
	std::size_t plane_id = 0U;
	if(!valid_source(source) || !plane_index(plane, plane_id)) {
		return false;
	}
	std::size_t access = 0U;
	if(!page_index(access_page_, access)) {
		return false;
	}

	const auto region = clipped_mapping(
		source.width, source.height, destination
	);
	const auto plane_mask = static_cast<Index4>(1U << plane_id);
	const auto keep_mask = static_cast<Index4>(0x0FU ^ plane_mask);
	for(auto y = region.y_begin; y < region.y_end; ++y) {
		const auto source_y = static_cast<std::size_t>(y);
		const auto destination_y = static_cast<std::int64_t>(destination.y) + y;
		for(auto x = region.x_begin; x < region.x_end; ++x) {
			const auto source_x = static_cast<std::size_t>(x);
			const auto destination_x =
				static_cast<std::int64_t>(destination.x) + x;
			auto &destination_color =
				pages_[access][page_offset(destination_x, destination_y)];
			destination_color = static_cast<Index4>(
				(destination_color & keep_mask) |
				(source_bit(
					source.bits,
					source.stride,
					source_x,
					source_y,
					source.height,
					source.row_order
				) ? plane_mask : 0U)
			);
		}
	}
	return true;
}

bool Raster::blit_planar_masked(
	PlanarSource source,
	BitSource alpha,
	Point destination
) noexcept {
	if(!valid_source(source) || !valid_source(alpha) ||
		(source.width != alpha.width) || (source.height != alpha.height)) {
		return false;
	}
	std::size_t access = 0U;
	if(!page_index(access_page_, access)) {
		return false;
	}

	const auto region = clipped_mapping(source.width, source.height, destination);
	for(auto y = region.y_begin; y < region.y_end; ++y) {
		const auto source_y = static_cast<std::size_t>(y);
		const auto destination_y = static_cast<std::int64_t>(destination.y) + y;
		for(auto x = region.x_begin; x < region.x_end; ++x) {
			const auto source_x = static_cast<std::size_t>(x);
			if(!source_bit(
				alpha.bits,
				alpha.stride,
				source_x,
				source_y,
				alpha.height,
				alpha.row_order
			)) {
				continue;
			}
			const auto destination_x =
				static_cast<std::int64_t>(destination.x) + x;
			pages_[access][page_offset(destination_x, destination_y)] =
				planar_pixel(source, source_x, source_y);
		}
	}
	return true;
}

bool Raster::clear_planar_masked(
	BitSource mask,
	Point destination
) noexcept {
	if(!valid_source(mask)) {
		return false;
	}
	std::size_t access = 0U;
	if(!page_index(access_page_, access)) {
		return false;
	}

	const auto region = clipped_mapping(mask.width, mask.height, destination);
	for(auto y = region.y_begin; y < region.y_end; ++y) {
		const auto source_y = static_cast<std::size_t>(y);
		const auto destination_y = static_cast<std::int64_t>(destination.y) + y;
		for(auto x = region.x_begin; x < region.x_end; ++x) {
			const auto source_x = static_cast<std::size_t>(x);
			if(!source_bit(
				mask.bits,
				mask.stride,
				source_x,
				source_y,
				mask.height,
				mask.row_order
			)) {
				continue;
			}
			const auto destination_x =
				static_cast<std::int64_t>(destination.x) + x;
			pages_[access][page_offset(destination_x, destination_y)] = 0U;
		}
	}
	return true;
}

bool Raster::grcg_write(
	BitSource cpu_source,
	Point destination,
	GrcgWriteMode mode,
	GrcgTile tile
) noexcept {
	if(!valid_source(cpu_source) || !valid_mode(mode) ||
		((cpu_source.width % 8U) != 0U) || ((destination.x % 8) != 0)) {
		return false;
	}
	std::size_t access = 0U;
	if(!page_index(access_page_, access)) {
		return false;
	}

	const auto region = clipped_mapping(
		cpu_source.width,
		cpu_source.height,
		destination
	);
	for(auto y = region.y_begin; y < region.y_end; ++y) {
		const auto source_y = static_cast<std::size_t>(y);
		const auto destination_y = static_cast<std::int64_t>(destination.y) + y;
		for(auto x = region.x_begin; x < region.x_end; ++x) {
			const auto source_x = static_cast<std::size_t>(x);
			if((mode == GrcgWriteMode::read_modify_write) &&
				!source_bit(
					cpu_source.bits,
					cpu_source.stride,
					source_x,
					source_y,
					cpu_source.height,
					cpu_source.row_order
				)) {
				continue;
			}
			const auto destination_x =
				static_cast<std::int64_t>(destination.x) + x;
			pages_[access][page_offset(destination_x, destination_y)] =
				tile_pixel(tile, source_x);
		}
	}
	return true;
}

std::optional<std::uint8_t> Raster::planar_byte(
	Page page,
	PlanarPlane plane,
	std::uint16_t byte_x,
	std::uint16_t y
) const noexcept {
	std::size_t page_id = 0U;
	std::size_t plane_id = 0U;
	if(!page_index(page, page_id) || !plane_index(plane, plane_id) ||
		(byte_x >= (graphics_width / 8U)) || (y >= graphics_height)) {
		return std::nullopt;
	}

	std::uint8_t result = 0U;
	const auto first_x = static_cast<std::size_t>(byte_x) * 8U;
	for(std::size_t bit = 0U; bit < 8U; ++bit) {
		const auto color = pages_[page_id][
			(static_cast<std::size_t>(y) * graphics_width) + first_x + bit
		];
		if(((color >> plane_id) & 1U) != 0U) {
			result = static_cast<std::uint8_t>(result | (0x80U >> bit));
		}
	}
	return result;
}

} // namespace th03::present
