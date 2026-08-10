#include "th03/present/raster.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace th03::present {
namespace {

constexpr std::uint64_t fnv_offset_basis = 14695981039346656037ULL;
constexpr std::uint64_t fnv_prime = 1099511628211ULL;
constexpr std::uint8_t display_state_hash_schema = 2U;

struct ClippedRegion {
	std::int64_t x_begin = 0;
	std::int64_t x_end = 0;
	std::int64_t y_begin = 0;
	std::int64_t y_end = 0;

	[[nodiscard]] bool empty() const noexcept {
		return (x_begin >= x_end) || (y_begin >= y_end);
	}
};

[[nodiscard]] constexpr bool valid_index(Index4 value) noexcept {
	return value <= 0x0FU;
}

[[nodiscard]] constexpr bool valid_rgb4(assets::Rgb4 color) noexcept {
	return valid_index(color.r) && valid_index(color.g) && valid_index(color.b);
}

[[nodiscard]] constexpr std::uint8_t apply_tone_component(
	std::uint8_t component,
	std::int16_t raw_tone
) noexcept {
	const auto tone = std::clamp(static_cast<std::int32_t>(raw_tone), 0, 200);
	if(tone <= 100) {
		return static_cast<std::uint8_t>(
			(static_cast<std::int32_t>(component) * tone) / 100
		);
	}
	return static_cast<std::uint8_t>(15 - (
		(static_cast<std::int32_t>(15U - component) * (200 - tone)) / 100
	));
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

[[nodiscard]] bool valid_source(IndexedSource source) noexcept {
	if((source.width == 0U) || (source.height == 0U)) {
		return true;
	}
	if(source.stride < static_cast<std::size_t>(source.width)) {
		return false;
	}

	const auto rows_before_last = static_cast<std::size_t>(source.height - 1U);
	if((rows_before_last != 0U) &&
		(source.stride >
			((std::numeric_limits<std::size_t>::max() - source.width) /
			 rows_before_last))) {
		return false;
	}
	const auto required = (rows_before_last * source.stride) + source.width;
	if(source.pixels.size() < required) {
		return false;
	}

	for(std::uint16_t y = 0U; y < source.height; ++y) {
		const auto row = static_cast<std::size_t>(y) * source.stride;
		for(std::uint16_t x = 0U; x < source.width; ++x) {
			if(!valid_index(source.pixels[row + x])) {
				return false;
			}
		}
	}
	return true;
}

[[nodiscard]] bool valid_mask(
	std::span<const std::uint8_t> mask,
	std::uint16_t width,
	std::uint16_t height,
	std::size_t stride
) noexcept {
	if((width == 0U) || (height == 0U)) {
		return true;
	}
	if(stride < static_cast<std::size_t>(width)) {
		return false;
	}

	const auto rows_before_last = static_cast<std::size_t>(height - 1U);
	if((rows_before_last != 0U) &&
		(stride >
			((std::numeric_limits<std::size_t>::max() - width) /
			 rows_before_last))) {
		return false;
	}
	const auto required = (rows_before_last * stride) + width;
	return mask.size() >= required;
}

[[nodiscard]] ClippedRegion clipped_mapping(
	std::int64_t source_x,
	std::int64_t source_y,
	std::int64_t source_width,
	std::int64_t source_height,
	std::int64_t destination_x,
	std::int64_t destination_y
) noexcept {
	const auto bound_x = static_cast<std::int64_t>(graphics_width);
	const auto bound_y = static_cast<std::int64_t>(graphics_height);

	ClippedRegion result{};
	result.x_begin = std::max({
		std::int64_t{0},
		-source_x,
		-destination_x,
	});
	result.x_end = std::min({
		source_width,
		bound_x - source_x,
		bound_x - destination_x,
	});
	result.y_begin = std::max({
		std::int64_t{0},
		-source_y,
		-destination_y,
	});
	result.y_end = std::min({
		source_height,
		bound_y - source_y,
		bound_y - destination_y,
	});
	return result;
}

[[nodiscard]] ClippedRegion clipped_rect(Rect rect) noexcept {
	const auto x = static_cast<std::int64_t>(rect.x);
	const auto y = static_cast<std::int64_t>(rect.y);
	const auto width = static_cast<std::int64_t>(rect.width);
	const auto height = static_cast<std::int64_t>(rect.height);

	ClippedRegion result{};
	result.x_begin = std::max(std::int64_t{0}, x);
	result.x_end = std::min(
		static_cast<std::int64_t>(graphics_width),
		x + width
	);
	result.y_begin = std::max(std::int64_t{0}, y);
	result.y_end = std::min(
		static_cast<std::int64_t>(graphics_height),
		y + height
	);
	return result;
}

[[nodiscard]] std::size_t page_offset(
	std::int64_t x,
	std::int64_t y
) noexcept {
	return (static_cast<std::size_t>(y) * graphics_width) +
		static_cast<std::size_t>(x);
}

void hash_byte(std::uint64_t &hash, std::uint8_t value) noexcept {
	hash ^= value;
	hash *= fnv_prime;
}

void hash_u16_le(std::uint64_t &hash, std::uint16_t value) noexcept {
	hash_byte(hash, static_cast<std::uint8_t>(value & 0x00FFU));
	hash_byte(hash, static_cast<std::uint8_t>(value >> 8U));
}

} // namespace

Raster::Raster() {
	for(auto &page : pages_) {
		page.assign(graphics_page_pixels, 0U);
	}
}

bool Raster::set_access_page(Page page) noexcept {
	std::size_t ignored = 0U;
	if(!page_index(page, ignored)) {
		return false;
	}
	access_page_ = page;
	return true;
}

bool Raster::set_shown_page(Page page) noexcept {
	std::size_t ignored = 0U;
	if(!page_index(page, ignored)) {
		return false;
	}
	shown_page_ = page;
	return true;
}

Page Raster::access_page() const noexcept {
	return access_page_;
}

Page Raster::shown_page() const noexcept {
	return shown_page_;
}

bool Raster::set_raster_mode(RasterMode mode) noexcept {
	switch(mode) {
	case RasterMode::lines_400:
		break;
	case RasterMode::zoomed_200_upper:
	case RasterMode::zoomed_200_lower:
		access_page_ = Page::zero;
		break;
	default:
		return false;
	}
	raster_mode_ = mode;
	return true;
}

RasterMode Raster::raster_mode() const noexcept {
	return raster_mode_;
}

bool Raster::clear(Rect rect, Index4 color) noexcept {
	if(!valid_index(color)) {
		return false;
	}
	const auto region = clipped_rect(rect);
	if(region.empty()) {
		return true;
	}

	std::size_t access = 0U;
	const bool page_is_valid = page_index(access_page_, access);
	if(!page_is_valid) {
		return false;
	}
	for(auto y = region.y_begin; y < region.y_end; ++y) {
		const auto first = page_offset(region.x_begin, y);
		const auto last = page_offset(region.x_end, y);
		std::fill(
			pages_[access].begin() + static_cast<std::ptrdiff_t>(first),
			pages_[access].begin() + static_cast<std::ptrdiff_t>(last),
			color
		);
	}
	return true;
}

bool Raster::plot(Point point, Index4 color) noexcept {
	if(!valid_index(color) || (point.x < 0) || (point.y < 0) ||
		(point.x >= graphics_width) || (point.y >= graphics_height)) {
		return false;
	}
	std::size_t access = 0U;
	const bool page_is_valid = page_index(access_page_, access);
	if(!page_is_valid) {
		return false;
	}
	pages_[access][page_offset(point.x, point.y)] = color;
	return true;
}

bool Raster::blit(IndexedSource source, Point destination) noexcept {
	if(!valid_source(source)) {
		return false;
	}

	std::size_t access = 0U;
	const bool page_is_valid = page_index(access_page_, access);
	if(!page_is_valid) {
		return false;
	}
	const auto region = clipped_mapping(
		0,
		0,
		source.width,
		source.height,
		destination.x,
		destination.y
	);
	for(auto y = region.y_begin; y < region.y_end; ++y) {
		const auto source_y = static_cast<std::size_t>(y);
		const auto destination_y = static_cast<std::int64_t>(destination.y) + y;
		for(auto x = region.x_begin; x < region.x_end; ++x) {
			const auto source_x = static_cast<std::size_t>(x);
			const auto destination_x =
				static_cast<std::int64_t>(destination.x) + x;
			pages_[access][page_offset(destination_x, destination_y)] =
				source.pixels[(source_y * source.stride) + source_x];
		}
	}
	return true;
}

bool Raster::blit_transparent(
	IndexedSource source,
	Point destination,
	Index4 transparent_color
) noexcept {
	if(!valid_source(source) || !valid_index(transparent_color)) {
		return false;
	}

	std::size_t access = 0U;
	const bool page_is_valid = page_index(access_page_, access);
	if(!page_is_valid) {
		return false;
	}
	const auto region = clipped_mapping(
		0,
		0,
		source.width,
		source.height,
		destination.x,
		destination.y
	);
	for(auto y = region.y_begin; y < region.y_end; ++y) {
		const auto source_y = static_cast<std::size_t>(y);
		const auto destination_y = static_cast<std::int64_t>(destination.y) + y;
		for(auto x = region.x_begin; x < region.x_end; ++x) {
			const auto source_x = static_cast<std::size_t>(x);
			const auto source_pixel =
				source.pixels[(source_y * source.stride) + source_x];
			if(source_pixel != transparent_color) {
				const auto destination_x =
					static_cast<std::int64_t>(destination.x) + x;
				pages_[access][page_offset(destination_x, destination_y)] =
					source_pixel;
			}
		}
	}
	return true;
}

bool Raster::blit_masked(
	IndexedSource source,
	std::span<const std::uint8_t> mask,
	std::size_t mask_stride,
	Point destination
) noexcept {
	if(!valid_source(source) ||
		!valid_mask(mask, source.width, source.height, mask_stride)) {
		return false;
	}

	std::size_t access = 0U;
	const bool page_is_valid = page_index(access_page_, access);
	if(!page_is_valid) {
		return false;
	}
	const auto region = clipped_mapping(
		0,
		0,
		source.width,
		source.height,
		destination.x,
		destination.y
	);
	for(auto y = region.y_begin; y < region.y_end; ++y) {
		const auto source_y = static_cast<std::size_t>(y);
		const auto destination_y = static_cast<std::int64_t>(destination.y) + y;
		for(auto x = region.x_begin; x < region.x_end; ++x) {
			const auto source_x = static_cast<std::size_t>(x);
			if(mask[(source_y * mask_stride) + source_x] != 0U) {
				const auto destination_x =
					static_cast<std::int64_t>(destination.x) + x;
				pages_[access][page_offset(destination_x, destination_y)] =
					source.pixels[(source_y * source.stride) + source_x];
			}
		}
	}
	return true;
}

bool Raster::copy_page(
	Page source,
	Rect source_rect,
	Page destination,
	Point target
) {
	std::size_t source_page = 0U;
	std::size_t destination_page = 0U;
	if(!page_index(source, source_page) || !page_index(destination, destination_page)) {
		return false;
	}

	const auto region = clipped_mapping(
		source_rect.x,
		source_rect.y,
		source_rect.width,
		source_rect.height,
		target.x,
		target.y
	);
	if(region.empty()) {
		return true;
	}

	const auto copied_width =
		static_cast<std::size_t>(region.x_end - region.x_begin);
	const auto copied_height =
		static_cast<std::size_t>(region.y_end - region.y_begin);
	std::vector<Index4> source_snapshot(copied_width * copied_height);
	for(std::size_t y = 0U; y < copied_height; ++y) {
		const auto logical_y = region.y_begin + static_cast<std::int64_t>(y);
		const auto source_y = static_cast<std::int64_t>(source_rect.y) + logical_y;
		for(std::size_t x = 0U; x < copied_width; ++x) {
			const auto logical_x = region.x_begin + static_cast<std::int64_t>(x);
			const auto source_x = static_cast<std::int64_t>(source_rect.x) + logical_x;
			source_snapshot[(y * copied_width) + x] =
				pages_[source_page][page_offset(source_x, source_y)];
		}
	}

	for(std::size_t y = 0U; y < copied_height; ++y) {
		const auto logical_y = region.y_begin + static_cast<std::int64_t>(y);
		const auto destination_y = static_cast<std::int64_t>(target.y) + logical_y;
		for(std::size_t x = 0U; x < copied_width; ++x) {
			const auto logical_x = region.x_begin + static_cast<std::int64_t>(x);
			const auto destination_x = static_cast<std::int64_t>(target.x) + logical_x;
			pages_[destination_page][page_offset(destination_x, destination_y)] =
				source_snapshot[(y * copied_width) + x];
		}
	}
	return true;
}

bool Raster::set_pending_palette_entry(
	std::uint8_t index,
	assets::Rgb4 color
) noexcept {
	if((index >= pending_palette_.colors.size()) || !valid_rgb4(color)) {
		return false;
	}
	pending_palette_.colors[index] = color;
	return true;
}

const assets::Palette16Rgb4 &Raster::pending_palette() const noexcept {
	return pending_palette_;
}

const assets::Palette16Rgb4 &Raster::applied_palette() const noexcept {
	return applied_palette_;
}

void Raster::set_palette_tone(std::int16_t tone) noexcept {
	palette_tone_ = tone;
}

std::int16_t Raster::palette_tone() const noexcept {
	return palette_tone_;
}

void Raster::apply_pending_palette() noexcept {
	for(std::size_t index = 0U; index < pending_palette_.colors.size(); ++index) {
		const auto source = pending_palette_.colors[index];
		applied_palette_.colors[index] = assets::Rgb4{
			apply_tone_component(source.r, palette_tone_),
			apply_tone_component(source.g, palette_tone_),
			apply_tone_component(source.b, palette_tone_),
		};
	}
}

void Raster::set_scroll_y(std::uint16_t scroll_y) noexcept {
	scroll_y_ = scroll_y;
}

std::uint16_t Raster::scroll_y() const noexcept {
	return scroll_y_;
}

std::optional<Index4> Raster::pixel(Page page, Point point) const noexcept {
	std::size_t page_id = 0U;
	if(!page_index(page, page_id) || (point.x < 0) || (point.y < 0) ||
		(point.x >= graphics_width) || (point.y >= graphics_height)) {
		return std::nullopt;
	}
	return pages_[page_id][page_offset(point.x, point.y)];
}

std::span<const Index4> Raster::graphics(Page page) const noexcept {
	std::size_t page_id = 0U;
	if(!page_index(page, page_id)) {
		return {};
	}
	return pages_[page_id];
}

std::span<const Index4> Raster::shown_graphics() const noexcept {
	return graphics(shown_page_);
}

std::uint64_t Raster::display_state_hash() const noexcept {
	std::uint64_t hash = fnv_offset_basis;
	hash_byte(hash, display_state_hash_schema);
	hash_byte(hash, static_cast<std::uint8_t>(shown_page_));
	hash_byte(hash, static_cast<std::uint8_t>(raster_mode_));
	hash_u16_le(hash, static_cast<std::uint16_t>(palette_tone_));
	hash_u16_le(hash, scroll_y_);
	for(const auto &color : applied_palette_.colors) {
		hash_byte(hash, color.r);
		hash_byte(hash, color.g);
		hash_byte(hash, color.b);
	}
	for(const auto pixel_value : shown_graphics()) {
		hash_byte(hash, pixel_value);
	}
	return hash;
}

} // namespace th03::present
