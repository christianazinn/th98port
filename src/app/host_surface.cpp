#include "th03/app/host_surface.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace th03::app {
namespace {

constexpr std::size_t channel_count = 4U;
constexpr std::size_t source_width = present::graphics_width;
constexpr std::size_t source_height = present::graphics_height;
constexpr std::size_t output_row_bytes = source_width * channel_count;
constexpr std::size_t no_pixel = std::numeric_limits<std::size_t>::max();

struct Rgba8 {
	std::uint8_t r = 0U;
	std::uint8_t g = 0U;
	std::uint8_t b = 0U;
	std::uint8_t a = 0xFFU;
};

[[nodiscard]] constexpr bool valid_byte_order(
	HostRgbaByteOrder order
) noexcept
{
	switch(order) {
	case HostRgbaByteOrder::rgba:
	case HostRgbaByteOrder::bgra:
	case HostRgbaByteOrder::argb:
	case HostRgbaByteOrder::abgr:
		return true;
	}
	return false;
}

[[nodiscard]] constexpr bool valid_palette(
	const assets::Palette16Rgb4 &palette
) noexcept
{
	for(const auto color : palette.colors) {
		if(color.r > 0x0FU || color.g > 0x0FU || color.b > 0x0FU) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::expected<void, HostSurfaceError> validate_pixels(
	const present::SourceSurface &source
) noexcept
{
	for(std::size_t offset = 0U; offset < source.pixels.size(); ++offset) {
		const auto pixel = source.pixels[offset];
		switch(pixel.layer) {
		case present::ComposedPixelLayer::graphics:
			if(pixel.value <= 0x0FU) {
				continue;
			}
			break;
		case present::ComposedPixelLayer::text:
			if(pixel.value <= static_cast<std::uint8_t>(present::TextColor::white)) {
				continue;
			}
			break;
		}
		return std::unexpected(HostSurfaceError{
			HostSurfaceErrorCode::invalid_composed_pixel,
			offset,
		});
	}
	return {};
}

[[nodiscard]] constexpr Rgba8 graphics_color(
	assets::Rgb4 color
) noexcept
{
	return {
		static_cast<std::uint8_t>(color.r * 17U),
		static_cast<std::uint8_t>(color.g * 17U),
		static_cast<std::uint8_t>(color.b * 17U),
		0xFFU,
	};
}

[[nodiscard]] constexpr Rgba8 text_color(std::uint8_t value) noexcept
{
	return {
		((value & 0x02U) != 0U) ? std::uint8_t{0xFFU} : std::uint8_t{0U},
		((value & 0x04U) != 0U) ? std::uint8_t{0xFFU} : std::uint8_t{0U},
		((value & 0x01U) != 0U) ? std::uint8_t{0xFFU} : std::uint8_t{0U},
		0xFFU,
	};
}

[[nodiscard]] constexpr std::array<std::uint8_t, channel_count> ordered_bytes(
	Rgba8 color,
	HostRgbaByteOrder order
) noexcept
{
	switch(order) {
	case HostRgbaByteOrder::rgba:
		return {color.r, color.g, color.b, color.a};
	case HostRgbaByteOrder::bgra:
		return {color.b, color.g, color.r, color.a};
	case HostRgbaByteOrder::argb:
		return {color.a, color.r, color.g, color.b};
	case HostRgbaByteOrder::abgr:
		return {color.a, color.b, color.g, color.r};
	}
	return {};
}

} // namespace

std::expected<void, HostSurfaceError> source_surface_to_host_rgba(
	const present::SourceSurface &source,
	HostRgbaTarget target
) noexcept
{
	if(!valid_byte_order(target.byte_order)) {
		return std::unexpected(HostSurfaceError{
			HostSurfaceErrorCode::invalid_byte_order,
			no_pixel,
		});
	}
	if(target.pitch_bytes < output_row_bytes) {
		return std::unexpected(HostSurfaceError{
			HostSurfaceErrorCode::pitch_too_small,
			no_pixel,
		});
	}
	constexpr auto preceding_rows = source_height - 1U;
	if(target.pitch_bytes >
		((std::numeric_limits<std::size_t>::max() - output_row_bytes) /
		 preceding_rows)) {
		return std::unexpected(HostSurfaceError{
			HostSurfaceErrorCode::size_overflow,
			no_pixel,
		});
	}
	const auto required_bytes = (preceding_rows * target.pitch_bytes) +
		output_row_bytes;
	if(target.bytes.size() < required_bytes) {
		return std::unexpected(HostSurfaceError{
			HostSurfaceErrorCode::output_too_small,
			no_pixel,
		});
	}
	if(!valid_palette(source.graphics_palette)) {
		return std::unexpected(HostSurfaceError{
			HostSurfaceErrorCode::invalid_graphics_palette,
			no_pixel,
		});
	}
	if(const auto validation = validate_pixels(source); !validation) {
		return validation;
	}

	for(std::size_t y = 0U; y < source_height; ++y) {
		const auto row_offset = y * target.pitch_bytes;
		const auto source_offset = y * source_width;
		for(std::size_t x = 0U; x < source_width; ++x) {
			const auto pixel = source.pixels[source_offset + x];
			const auto color = (pixel.layer ==
				present::ComposedPixelLayer::graphics) ?
				graphics_color(source.graphics_palette.colors[pixel.value]) :
				text_color(pixel.value);
			const auto bytes = ordered_bytes(color, target.byte_order);
			const auto destination = row_offset + (x * channel_count);
			for(std::size_t channel = 0U; channel < channel_count; ++channel) {
				target.bytes[destination + channel] =
					static_cast<std::byte>(bytes[channel]);
			}
		}
	}
	return {};
}

std::optional<IntegerLetterboxRect> integer_letterbox_8x5(
	std::size_t drawable_width,
	std::size_t drawable_height
) noexcept
{
	const auto scale = std::min(
		drawable_width / source_width,
		drawable_height / source_height
	);
	if(scale == 0U) {
		return std::nullopt;
	}
	const auto width = scale * source_width;
	const auto height = scale * source_height;
	return IntegerLetterboxRect{
		(drawable_width - width) / 2U,
		(drawable_height - height) / 2U,
		width,
		height,
		scale,
	};
}

} // namespace th03::app
