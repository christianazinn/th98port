#include "th03/present/sprite16.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace th03::present {
namespace {

constexpr std::uint16_t sprite16_column_width = 16U;
constexpr std::uint16_t raw_row_bytes = graphics_width / 8U;
constexpr std::uint32_t vram_plane_bytes = 0x10000U;
constexpr std::uint32_t modeled_plane_bytes =
	static_cast<std::uint32_t>(raw_row_bytes) * graphics_height;

struct HorizontalDraw {
	std::int32_t left = 0;
	std::uint16_t source_skip = 0U;
	std::uint16_t width = 0U;
};

[[nodiscard]] constexpr bool in_signed_word(std::int32_t value) noexcept
{
	return (value >= std::numeric_limits<std::int16_t>::min()) &&
		(value <= std::numeric_limits<std::int16_t>::max());
}

[[nodiscard]] constexpr bool valid_source(
	Sprite16AtlasRect source
) noexcept
{
	return (source.width != 0U) && (source.raw_height != 0U) &&
		((source.left % sprite16_column_width) == 0U) &&
		((source.width % sprite16_column_width) == 0U) &&
		(static_cast<std::uint32_t>(source.left) + source.width <=
			sprite16_source_width) &&
		(static_cast<std::uint32_t>(source.raw_top) + source.raw_height <=
			sprite16_source_height);
}

[[nodiscard]] constexpr bool valid_style(Sprite16DrawStyle style) noexcept
{
	if((style.overlap != Sprite16Overlap::clear) &&
		(style.overlap != Sprite16Overlap::or_planes)) {
		return false;
	}
	return !style.monochrome_color.has_value() ||
		(*style.monochrome_color <= 0x0FU);
}

[[nodiscard]] constexpr bool valid_position(Point position) noexcept
{
	return in_signed_word(position.x) && in_signed_word(position.y);
}

[[nodiscard]] constexpr bool valid_clip(
	Sprite16HorizontalClip clip
) noexcept
{
	return in_signed_word(clip.left) && in_signed_word(clip.right) &&
		(clip.left < clip.right);
}

[[nodiscard]] HorizontalDraw clip_horizontal_columns(
	Sprite16AtlasRect source,
	std::int32_t left,
	Sprite16HorizontalClip clip
) noexcept
{
	HorizontalDraw result{left, 0U, source.width};
	auto right = left + source.width;
	if(right < clip.right) {
		if(left >= clip.left) {
			return result;
		}
		if(right < clip.left) {
			result.width = 0U;
			return result;
		}
		do {
			result.left += sprite16_column_width;
			result.source_skip = static_cast<std::uint16_t>(
				result.source_skip + sprite16_column_width
			);
			result.width = static_cast<std::uint16_t>(
				result.width - sprite16_column_width
			);
			if(result.width == 0U) {
				return result;
			}
		} while(result.left < clip.left);
		return result;
	}
	if(left >= clip.right) {
		result.width = 0U;
		return result;
	}
	do {
		right -= sprite16_column_width;
		result.width = static_cast<std::uint16_t>(
			result.width - sprite16_column_width
		);
		if(result.width == 0U) {
			return result;
		}
	} while(right >= clip.right);
	return result;
}

[[nodiscard]] constexpr std::int32_t raw_top_from_screen(
	std::int32_t screen_top
) noexcept
{
	const auto top = static_cast<std::int16_t>(screen_top);
	auto raw_top = static_cast<std::int32_t>(top) / 2;
	if((top < 0) && ((top % 2) != 0)) {
		--raw_top;
	}
	return raw_top;
}

[[nodiscard]] bool source_bit(
	BitSource source,
	std::size_t x,
	std::size_t y
) noexcept
{
	const auto value = source.bits[(y * source.stride) + (x / 8U)];
	return ((value >> (7U - (x & 7U))) & 1U) != 0U;
}

[[nodiscard]] constexpr bool write_mask_bit(
	std::uint16_t mask,
	std::uint32_t shifted_x
) noexcept
{
	const auto bit_in_word = shifted_x & 0x0FU;
	const auto bit = static_cast<unsigned int>(
		7U - (bit_in_word & 7U) + ((bit_in_word / 8U) * 8U)
	);
	return ((mask >> bit) & 1U) != 0U;
}

[[nodiscard]] bool draw_raw_tile(
	Raster &raster,
	const Sprite16Staging &staging,
	Sprite16AtlasRect source,
	HorizontalDraw horizontal,
	std::int32_t raw_top,
	Sprite16DrawStyle style
) noexcept
{
	auto source_row_skip = std::int32_t{0};
	auto draw_height = static_cast<std::int32_t>(source.raw_height);
	if(raw_top < 0) {
		if((raw_top + draw_height) <= 0) {
			return true;
		}
		source_row_skip = -raw_top;
		draw_height -= source_row_skip;
		raw_top = 0;
	} else if(raw_top >= sprite16_source_height) {
		return true;
	}
	if((raw_top + draw_height) > sprite16_source_height) {
		draw_height = sprite16_source_height - raw_top;
	}

	const auto positive = staging.positive_alpha();
	if(!positive.has_value()) {
		return false;
	}
	const auto source_page = raster.graphics(raster.access_page());
	if(source_page.size() != graphics_page_pixels) {
		return false;
	}

	const auto left_word = static_cast<std::uint16_t>(
		static_cast<std::int16_t>(horizontal.left)
	);
	const auto destination_word_byte = static_cast<std::uint16_t>(
		(left_word >> 3U) & 0xFFFEU
	);
	const auto destination_base = static_cast<std::uint16_t>(
		(static_cast<std::uint32_t>(raw_top) * raw_row_bytes) +
		destination_word_byte
	);
	const auto destination_shift = static_cast<std::uint32_t>(left_word & 0x0FU);
	const auto source_left = static_cast<std::size_t>(
		source.left + horizontal.source_skip
	);
	const auto source_top = static_cast<std::size_t>(
		source.raw_top + source_row_skip
	);

	for(std::int32_t y = 0; y < draw_height; ++y) {
		const auto source_y = source_top + static_cast<std::size_t>(y);
		for(std::uint16_t x = 0U; x < horizontal.width; ++x) {
			const auto shifted_x = destination_shift + x;
			if(!write_mask_bit(style.write_mask, shifted_x)) {
				continue;
			}
			const auto source_x = source_left + x;
			const auto opaque = source_bit(*positive, source_x, source_y);
			const auto color = source_page[
				((source_y + sprite16_source_top) * graphics_width) + source_x
			];
			if(style.monochrome_color.has_value()) {
				if(!opaque) {
					continue;
				}
			} else if((style.overlap == Sprite16Overlap::clear) && !opaque) {
				continue;
			} else if((style.overlap == Sprite16Overlap::or_planes) &&
				(color == 0U)) {
				continue;
			}

			const auto destination_byte = static_cast<std::uint16_t>(
				(static_cast<std::uint32_t>(destination_base) +
				(static_cast<std::uint32_t>(y) * raw_row_bytes) +
				(shifted_x / 8U)) % vram_plane_bytes
			);
			if(destination_byte >= modeled_plane_bytes) {
				continue;
			}
			const auto destination = Point{
				static_cast<std::int32_t>(
					(destination_byte % raw_row_bytes) * 8U + (shifted_x & 7U)
				),
				static_cast<std::int32_t>(destination_byte / raw_row_bytes),
			};
			Index4 output = color;
			if(style.monochrome_color.has_value()) {
				output = *style.monochrome_color;
			} else if(style.overlap == Sprite16Overlap::or_planes) {
				const auto destination_color = raster.pixel(
					raster.access_page(),
					destination
				);
				if(!destination_color.has_value()) {
					return false;
				}
				output = static_cast<Index4>(*destination_color | color);
			}
			if(!raster.plot(destination, output)) {
				return false;
			}
		}
	}
	return true;
}

[[nodiscard]] bool valid_common(
	Sprite16AtlasRect source,
	Point position,
	Sprite16DrawStyle style
) noexcept
{
	return valid_source(source) && valid_position(position) && valid_style(style);
}

} // namespace

bool sprite16_put(
	Raster &raster,
	const Sprite16Staging &staging,
	Sprite16AtlasRect source,
	Point screen_position,
	Sprite16HorizontalClip clip,
	Sprite16DrawStyle style
) noexcept
{
	if(!valid_common(source, screen_position, style) || !valid_clip(clip)) {
		return false;
	}
	const auto horizontal = clip_horizontal_columns(
		source,
		screen_position.x,
		clip
	);
	if(horizontal.width == 0U) {
		return true;
	}
	return draw_raw_tile(
		raster,
		staging,
		source,
		horizontal,
		raw_top_from_screen(screen_position.y),
		style
	);
}

bool sprite16_put_noclip(
	Raster &raster,
	const Sprite16Staging &staging,
	Sprite16AtlasRect source,
	Point screen_position,
	Sprite16DrawStyle style
) noexcept
{
	if(!valid_common(source, screen_position, style)) {
		return false;
	}
	return draw_raw_tile(
		raster,
		staging,
		source,
		{screen_position.x, 0U, source.width},
		raw_top_from_screen(screen_position.y),
		style
	);
}

bool sprite16_put_downwards_column(
	Raster &raster,
	const Sprite16Staging &staging,
	Sprite16AtlasRect source,
	Point screen_position,
	Sprite16HorizontalClip clip,
	Sprite16DrawStyle style
) noexcept
{
	if(!valid_common(source, screen_position, style) || !valid_clip(clip)) {
		return false;
	}
	const auto horizontal = clip_horizontal_columns(
		source,
		screen_position.x,
		clip
	);
	if(horizontal.width == 0U) {
		return true;
	}
	auto raw_top = raw_top_from_screen(screen_position.y);
	do {
		if(!draw_raw_tile(raster, staging, source, horizontal, raw_top, style)) {
			return false;
		}
		raw_top += source.raw_height;
	} while(raw_top < sprite16_source_height);
	return true;
}

} // namespace th03::present
