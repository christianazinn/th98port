#include "th03/present/cels.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace th03::present {
namespace {

constexpr std::uint16_t pc98_vram_row_bytes = 80U;
constexpr std::uint16_t pixels_per_byte = 8U;
constexpr std::uint16_t pixels_per_dword = 32U;
constexpr std::uint16_t mrs_width = 288U;
constexpr std::uint16_t mrs_height = 184U;
constexpr Index4 color_without_intensity = 0x07U;

constexpr std::array<std::array<std::uint16_t, 4U>, 8U> dissolve_patterns{{
	{{0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU}},
	{{0x2222U, 0x5555U, 0x8888U, 0x5555U}},
	{{0xAAAAU, 0x5555U, 0xAAAAU, 0x5555U}},
	{{0xEEEEU, 0x5555U, 0xBBBBU, 0x5555U}},
	{{0xEEEEU, 0x7777U, 0xBBBBU, 0xDDDDU}},
	{{0xFFFFU, 0x5555U, 0xFFFFU, 0x5555U}},
	{{0xFFFFU, 0xEEEEU, 0xFFFFU, 0xBBBBU}},
	{{0xFFFFU, 0x7777U, 0xFFFFU, 0xFFFFU}},
}};

[[nodiscard]] bool valid_cel_geometry(
	const assets::PlanarCel4 &cel
) noexcept {
	if(cel.pixel_width == 0U || cel.pixel_height == 0U ||
		(cel.pixel_width % pixels_per_dword) != 0U ||
		cel.row_order != assets::PlanarRowOrder::bottom_to_top ||
		cel.bit_order != assets::PlanarBitOrder::most_significant_bit_left) {
		return false;
	}

	const auto byte_width = static_cast<std::size_t>(cel.pixel_width) /
		pixels_per_byte;
	const auto plane_size = byte_width * cel.pixel_height;
	const auto bottom_offset = static_cast<std::size_t>(cel.pixel_height - 1U) *
		pc98_vram_row_bytes;
	return plane_size <= std::numeric_limits<std::uint16_t>::max() &&
		bottom_offset <= static_cast<std::size_t>(
			std::numeric_limits<std::int16_t>::max()
		) &&
		cel.bitplane_size == plane_size &&
		cel.offset_at_bottom_left == static_cast<std::int16_t>(bottom_offset) &&
		cel.vram_dword_width == (cel.pixel_width / pixels_per_dword);
}

[[nodiscard]] bool valid_cel_planes(
	const assets::PlanarCel4 &cel,
	bool require_alpha
) noexcept {
	if(!valid_cel_geometry(cel)) {
		return false;
	}
	const auto plane_size = static_cast<std::size_t>(cel.bitplane_size);
	for(const auto &plane : cel.color_planes) {
		if(plane.size() != plane_size) {
			return false;
		}
	}
	if(require_alpha) {
		return cel.alpha && cel.alpha->size() == plane_size;
	}
	return !cel.alpha || cel.alpha->size() == plane_size;
}

[[nodiscard]] bool valid_mrs(const assets::MrsImage &image) noexcept {
	return image.cel.pixel_width == mrs_width &&
		image.cel.pixel_height == mrs_height &&
		valid_cel_planes(image.cel, true);
}

[[nodiscard]] std::optional<std::int32_t> align_left_8(
	std::int64_t left
) noexcept {
	auto remainder = left % pixels_per_byte;
	if(remainder < 0) {
		remainder += pixels_per_byte;
	}
	const auto aligned = left - remainder;
	if(aligned < std::numeric_limits<std::int32_t>::min() ||
		aligned > std::numeric_limits<std::int32_t>::max()) {
		return std::nullopt;
	}
	return static_cast<std::int32_t>(aligned);
}

[[nodiscard]] std::optional<std::int32_t> checked_i32(
	std::int64_t value
) noexcept {
	if(value < std::numeric_limits<std::int32_t>::min() ||
		value > std::numeric_limits<std::int32_t>::max()) {
		return std::nullopt;
	}
	return static_cast<std::int32_t>(value);
}

[[nodiscard]] PackedRowOrder row_order(
	assets::PlanarRowOrder order
) noexcept {
	return (order == assets::PlanarRowOrder::bottom_to_top) ?
		PackedRowOrder::bottom_to_top : PackedRowOrder::top_to_bottom;
}

[[nodiscard]] PlanarSource planar_source(
	const assets::PlanarCel4 &cel
) noexcept {
	PlanarSource source{};
	for(std::size_t plane = 0U; plane < planar_plane_count; ++plane) {
		source.planes[plane] = cel.color_planes[plane];
	}
	source.width = cel.pixel_width;
	source.height = cel.pixel_height;
	source.stride = static_cast<std::size_t>(cel.pixel_width) / pixels_per_byte;
	source.row_order = row_order(cel.row_order);
	return source;
}

[[nodiscard]] BitSource alpha_source(
	const assets::PlanarCel4 &cel
) noexcept {
	return BitSource{
		*cel.alpha,
		cel.pixel_width,
		cel.pixel_height,
		static_cast<std::size_t>(cel.pixel_width) / pixels_per_byte,
		row_order(cel.row_order),
	};
}

[[nodiscard]] bool put_alpha(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	PlanarSource colors,
	BitSource alpha
) {
	const auto aligned_left = align_left_8(left);
	if(!aligned_left) {
		return false;
	}
	const Point destination{*aligned_left, top};
	return raster.clear_planar_masked(alpha, destination) &&
		raster.blit_planar(colors, destination, PlanarWriteMode::or_planes);
}

[[nodiscard]] bool put_noalpha(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const assets::PlanarCel4 &cel
) {
	const auto aligned_left = align_left_8(left);
	return aligned_left && raster.blit_planar(
		planar_source(cel),
		{*aligned_left, top},
		PlanarWriteMode::replace
	);
}

[[nodiscard]] bool cel_bit(
	std::span<const std::uint8_t> plane,
	const assets::PlanarCel4 &cel,
	std::size_t x,
	std::size_t y
) noexcept {
	const auto stride = static_cast<std::size_t>(cel.pixel_width) / pixels_per_byte;
	const auto stored_y = static_cast<std::size_t>(cel.pixel_height) - 1U - y;
	const auto byte = plane[(stored_y * stride) + (x / pixels_per_byte)];
	const auto shift = static_cast<unsigned int>(7U - (x & 7U));
	return ((byte >> shift) & 1U) != 0U;
}

[[nodiscard]] Index4 cel_color(
	const assets::PlanarCel4 &cel,
	std::size_t x,
	std::size_t y
) noexcept {
	Index4 color = 0U;
	for(std::size_t plane = 0U; plane < planar_plane_count; ++plane) {
		if(cel_bit(cel.color_planes[plane], cel, x, y)) {
			color = static_cast<Index4>(color | (1U << plane));
		}
	}
	return color;
}

[[nodiscard]] bool composite_pixel(
	Raster &raster,
	std::int64_t destination_x,
	std::int64_t destination_y,
	bool clear,
	Index4 color
) {
	if(destination_x < 0 || destination_y < 0 ||
		destination_x >= graphics_width || destination_y >= graphics_height) {
		return true;
	}
	const Point destination{
		static_cast<std::int32_t>(destination_x),
		static_cast<std::int32_t>(destination_y),
	};
	const auto current = raster.pixel(raster.access_page(), destination);
	return current && raster.plot(
		destination,
		static_cast<Index4>((clear ? 0U : *current) | color)
	);
}

[[nodiscard]] bool valid_alpha_mode(CdgAlphaMode mode) noexcept {
	return mode == CdgAlphaMode::no_alpha || mode == CdgAlphaMode::alpha;
}

[[nodiscard]] bool valid_color_mode(MrsColorMode mode) noexcept {
	return mode == MrsColorMode::regular || mode == MrsColorMode::altered;
}

[[nodiscard]] bool dissolve_bit(
	std::uint16_t pattern,
	std::size_t x
) noexcept {
	const auto byte = ((x & 8U) == 0U) ?
		static_cast<std::uint8_t>(pattern & 0x00FFU) :
		static_cast<std::uint8_t>(pattern >> 8U);
	const auto bit = static_cast<unsigned int>(7U - (x & 7U));
	return ((byte >> bit) & 1U) != 0U;
}

void clear_intensity_pixel(Raster &raster, std::int64_t x, std::int64_t y) {
	if(x < 0 || y < 0 || x >= graphics_width || y >= graphics_height) {
		return;
	}
	const Point point{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
	const auto pixel = raster.pixel(raster.access_page(), point);
	if(pixel) {
		(void)raster.plot(
			point,
			static_cast<Index4>(*pixel & color_without_intensity)
		);
	}
}

} // namespace

bool cdg_put_8(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const assets::PlanarCel4 &cel
) {
	return valid_cel_planes(cel, true) && put_alpha(
		raster,
		left,
		top,
		planar_source(cel),
		alpha_source(cel)
	);
}

bool cdg_put_noalpha_8(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const assets::PlanarCel4 &cel
) {
	return valid_cel_planes(cel, false) && put_noalpha(raster, left, top, cel);
}

bool cdg_put_hflip_8(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const assets::PlanarCel4 &cel
) {
	if(!valid_cel_planes(cel, true)) {
		return false;
	}
	const auto aligned_left = align_left_8(left);
	if(!aligned_left) {
		return false;
	}
	for(std::size_t y = 0U; y < cel.pixel_height; ++y) {
		for(std::size_t x = 0U; x < cel.pixel_width; ++x) {
			const auto source_x = static_cast<std::size_t>(cel.pixel_width) - 1U - x;
			if(!composite_pixel(
				raster,
				static_cast<std::int64_t>(*aligned_left) +
					static_cast<std::int64_t>(x),
				static_cast<std::int64_t>(top) + static_cast<std::int64_t>(y),
				cel_bit(*cel.alpha, cel, source_x, y),
				cel_color(cel, source_x, y)
			)) {
				return false;
			}
		}
	}
	return true;
}

bool cdg_put_dissolve_e_8(
	Raster &raster,
	std::int32_t center_x,
	std::int32_t center_y,
	const assets::PlanarCel4 &cel,
	std::uint8_t strength,
	CdgAlphaMode alpha_mode
) {
	if(!valid_alpha_mode(alpha_mode) ||
		!valid_cel_planes(cel, alpha_mode == CdgAlphaMode::alpha)) {
		return false;
	}
	const auto raw_left = static_cast<std::int64_t>(center_x) -
		(cel.pixel_width / 2U);
	const auto raw_top = static_cast<std::int64_t>(center_y) -
		(cel.pixel_height / 2U);
	const auto left = align_left_8(raw_left);
	const auto top = checked_i32(raw_top);
	if(!left || !top) {
		return false;
	}

	const auto put = (alpha_mode == CdgAlphaMode::alpha) ?
		put_alpha(
			raster,
			*left,
			*top,
			planar_source(cel),
			alpha_source(cel)
		) :
		put_noalpha(raster, *left, *top, cel);
	if(!put) {
		return false;
	}

	const auto effective_strength = static_cast<std::size_t>(strength & 7U);
	if(effective_strength == 0U) {
		return true;
	}
	for(std::size_t y = 0U; y < cel.pixel_height; ++y) {
		const auto destination_y = raw_top + static_cast<std::int64_t>(y);
		const auto pattern_row = static_cast<std::size_t>(
			static_cast<std::uint64_t>(destination_y) & 3U
		);
		const auto pattern = dissolve_patterns[effective_strength][pattern_row];
		for(std::size_t x = 0U; x < cel.pixel_width; ++x) {
			if(dissolve_bit(pattern, x)) {
				clear_intensity_pixel(
					raster,
					static_cast<std::int64_t>(*left) +
						static_cast<std::int64_t>(x),
					destination_y
				);
			}
		}
	}
	return true;
}

bool cdg_unput_for_upwards_motion_e_8(
	Raster &raster,
	std::int32_t center_x,
	std::int32_t center_y,
	const assets::PlanarCel4 &cel
) {
	if(!valid_cel_geometry(cel)) {
		return false;
	}
	const auto raw_left = static_cast<std::int64_t>(center_x) -
		(cel.pixel_width / 2U);
	const auto left = align_left_8(raw_left);
	if(!left) {
		return false;
	}
	const auto first_y = static_cast<std::int64_t>(center_y) +
		(cel.pixel_height / 2U) - 2;
	for(std::int64_t row = 0; row < 3; ++row) {
		for(std::size_t x = 0U; x < cel.pixel_width; ++x) {
			clear_intensity_pixel(
				raster,
				static_cast<std::int64_t>(*left) + static_cast<std::int64_t>(x),
				first_y + row
			);
		}
	}
	return true;
}

bool mrs_put_8(
	Raster &raster,
	std::int32_t left,
	std::uint16_t screen_top,
	const assets::MrsImage &image
) {
	if(!valid_mrs(image)) {
		return false;
	}

	// mrs_put_8() skips the four color-plane ORs only when the complete
	// 32-pixel alpha dword is zero. Color bits outside individual alpha pixels
	// therefore still pass through inside a nonempty dword.
	const auto aligned_left = align_left_8(left);
	if(!aligned_left) {
		return false;
	}
	const auto top = static_cast<std::int32_t>(screen_top / 2U);
	for(std::size_t y = 0U; y < image.cel.pixel_height; ++y) {
		for(std::size_t group_x = 0U; group_x < image.cel.pixel_width;
			group_x += pixels_per_dword) {
			bool any_alpha = false;
			for(std::size_t x = group_x; x < group_x + pixels_per_dword; ++x) {
				any_alpha = any_alpha || cel_bit(*image.cel.alpha, image.cel, x, y);
			}
			for(std::size_t x = group_x; x < group_x + pixels_per_dword; ++x) {
				if(!composite_pixel(
					raster,
					static_cast<std::int64_t>(*aligned_left) +
						static_cast<std::int64_t>(x),
					static_cast<std::int64_t>(top) + static_cast<std::int64_t>(y),
					cel_bit(*image.cel.alpha, image.cel, x, y),
					any_alpha ? cel_color(image.cel, x, y) : 0U
				)) {
					return false;
				}
			}
		}
	}
	return true;
}

bool mrs_put_noalpha_8(
	Raster &raster,
	std::int32_t left,
	std::uint16_t screen_top,
	const assets::MrsImage &image,
	MrsColorMode color_mode
) {
	if(!valid_color_mode(color_mode) || !valid_mrs(image)) {
		return false;
	}
	if(color_mode == MrsColorMode::regular) {
		return put_noalpha(
			raster,
			left,
			static_cast<std::int32_t>(screen_top / 2U),
			image.cel
		);
	}

	auto altered_blue = image.cel.color_planes[
		static_cast<std::size_t>(assets::PlanarColorPlane::blue)
	];
	for(std::size_t offset = 0U; offset < altered_blue.size(); ++offset) {
		altered_blue[offset] = static_cast<std::uint8_t>(
			altered_blue[offset] |
			static_cast<std::uint8_t>(~(*image.cel.alpha)[offset])
		);
	}
	PlanarSource source = planar_source(image.cel);
	source.planes[static_cast<std::size_t>(PlanarPlane::blue)] = altered_blue;
	const auto aligned_left = align_left_8(left);
	return aligned_left && raster.blit_planar(
		source,
		{*aligned_left, static_cast<std::int32_t>(screen_top / 2U)},
		PlanarWriteMode::replace
	);
}

} // namespace th03::present
