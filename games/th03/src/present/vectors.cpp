#include "th03/present/vectors.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace th03::present {
namespace {

constexpr std::uint8_t out_left = 0x01U;
constexpr std::uint8_t out_right = 0x02U;
constexpr std::uint8_t out_bottom = 0x04U;
constexpr std::uint8_t out_top = 0x08U;
constexpr std::uint8_t out_horizontal = out_left | out_right;
constexpr std::uint16_t word_fraction = 0x8000U;
constexpr std::uint32_t word_modulus = 0x10000U;
constexpr std::uint32_t word_mask = word_modulus - 1U;
constexpr unsigned int word_fraction_bits = 16U;
constexpr std::int16_t zoomed_raster_height = static_cast<std::int16_t>(
	graphics_height / 2U
);

[[nodiscard]] constexpr std::int16_t wrap_word(std::int32_t value) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(value) & word_mask
	);
	return (bits <= static_cast<std::uint16_t>(
		std::numeric_limits<std::int16_t>::max()
	)) ?
		static_cast<std::int16_t>(bits) :
		static_cast<std::int16_t>(
			static_cast<std::int32_t>(bits) -
			static_cast<std::int32_t>(word_modulus)
		);
}

[[nodiscard]] constexpr std::int16_t add_word(
	std::int16_t left,
	std::int32_t right
) noexcept
{
	return wrap_word(static_cast<std::int32_t>(left) + right);
}

[[nodiscard]] constexpr std::int16_t subtract_word(
	std::int16_t left,
	std::int16_t right
) noexcept
{
	return wrap_word(
		static_cast<std::int32_t>(left) - static_cast<std::int32_t>(right)
	);
}

[[nodiscard]] constexpr std::int16_t active_height(
	RasterMode mode
) noexcept
{
	return (mode == RasterMode::lines_400) ?
		static_cast<std::int16_t>(graphics_height) : zoomed_raster_height;
}

[[nodiscard]] constexpr bool valid_clip(
	GrcgClipRect clip,
	RasterMode mode
) noexcept
{
	return clip.left >= 0 && clip.top >= 0 && clip.left <= clip.right &&
		clip.top <= clip.bottom && clip.right < graphics_width &&
		clip.bottom < active_height(mode);
}

[[nodiscard]] constexpr std::uint8_t outcode(
	GrcgPoint point,
	GrcgClipRect clip
) noexcept
{
	std::uint8_t result = 0U;
	if(point.x < clip.left) {
		result = static_cast<std::uint8_t>(result | out_left);
	} else if(point.x > clip.right) {
		result = static_cast<std::uint8_t>(result | out_right);
	}
	if(point.y < clip.top) {
		result = static_cast<std::uint8_t>(result | out_top);
	} else if(point.y > clip.bottom) {
		result = static_cast<std::uint8_t>(result | out_bottom);
	}
	return result;
}

[[nodiscard]] std::optional<std::int16_t> divided_word(
	std::int32_t numerator,
	std::int16_t denominator
) noexcept
{
	if(denominator == 0) {
		return std::nullopt;
	}
	const auto quotient = numerator / static_cast<std::int32_t>(denominator);
	if(quotient < std::numeric_limits<std::int16_t>::min() ||
		quotient > std::numeric_limits<std::int16_t>::max()) {
		return std::nullopt;
	}
	return static_cast<std::int16_t>(quotient);
}

// Direct translation of master.lib cutline. [point.y] is relative to the top
// clip edge here, matching DI/BP in grcg_line.asm.
[[nodiscard]] bool cut_line_endpoint(
	GrcgPoint &point,
	std::uint8_t &point_outcode,
	GrcgPoint other,
	std::uint8_t other_outcode,
	GrcgClipRect clip
) noexcept
{
	if(point_outcode == 0U) {
		return other_outcode == 0U;
	}

	const auto clip_height = subtract_word(clip.bottom, clip.top);
	if((point_outcode & out_horizontal) != 0U) {
		const auto target_x = ((point_outcode & out_left) != 0U) ?
			clip.left : clip.right;
		const auto delta_x = subtract_word(point.x, other.x);
		if(delta_x == 0) {
			return false;
		}
		const auto delta_y = subtract_word(point.y, other.y);
		const auto target_delta_x = subtract_word(target_x, other.x);
		const auto quotient = divided_word(
			static_cast<std::int32_t>(delta_y) * target_delta_x,
			delta_x
		);
		if(!quotient.has_value()) {
			return false;
		}
		point.x = target_x;
		point.y = add_word(other.y, *quotient);
		if(point.y >= 0 && point.y <= clip_height) {
			point_outcode = 0U;
			return other_outcode == 0U;
		}
	}

	// cutline.asm selects this second boundary from the Y produced by the
	// horizontal cut, not from the endpoint's stale original outcode.
	const auto target_y = (point.y > clip_height) ?
		clip_height : std::int16_t{0};
	const auto delta_y = subtract_word(point.y, other.y);
	if(delta_y == 0) {
		return false;
	}
	const auto delta_x = subtract_word(point.x, other.x);
	const auto target_delta_y = subtract_word(target_y, other.y);
	const auto quotient = divided_word(
		static_cast<std::int32_t>(delta_x) * target_delta_y,
		delta_y
	);
	if(!quotient.has_value()) {
		return false;
	}
	point.y = target_y;
	point.x = add_word(other.x, *quotient);
	if(point.x < clip.left) {
		point_outcode = out_left;
		return false;
	}
	if(point.x > clip.right) {
		point_outcode = out_right;
		return false;
	}
	point_outcode = 0U;
	return other_outcode == 0U;
}

[[nodiscard]] bool clip_line(
	GrcgPoint &first,
	GrcgPoint &second,
	GrcgClipRect clip
) noexcept
{
	// grcg_line.asm subtracts ClipYT with 16-bit wrapping before GETOUTCODE.
	// Classifying the absolute input first changes signed-word edge cases.
	first.y = subtract_word(first.y, clip.top);
	second.y = subtract_word(second.y, clip.top);
	const GrcgClipRect relative_clip{
		.left = clip.left,
		.top = 0,
		.right = clip.right,
		.bottom = subtract_word(clip.bottom, clip.top),
	};
	auto first_outcode = outcode(first, relative_clip);
	auto second_outcode = outcode(second, relative_clip);
	if((first_outcode & second_outcode) != 0U) {
		return false;
	}
	if((first_outcode | second_outcode) == 0U) {
		first.y = add_word(first.y, clip.top);
		second.y = add_word(second.y, clip.top);
		return true;
	}

	if(cut_line_endpoint(
		first, first_outcode, second, second_outcode, clip
	)) {
		first.y = add_word(first.y, clip.top);
		second.y = add_word(second.y, clip.top);
		return true;
	}
	if(first_outcode == 0U && second_outcode == 0U) {
		first.y = add_word(first.y, clip.top);
		second.y = add_word(second.y, clip.top);
		return true;
	}

	auto clipped = second;
	auto clipped_outcode = second_outcode;
	if(!cut_line_endpoint(
		clipped, clipped_outcode, first, first_outcode, clip
	)) {
		return false;
	}
	second = clipped;
	first.y = add_word(first.y, clip.top);
	second.y = add_word(second.y, clip.top);
	return true;
}

[[nodiscard]] bool grcg_plot(
	Raster &raster,
	GrcgPoint point,
	GrcgTile tile
) noexcept
{
	const auto bit = static_cast<unsigned int>(
		static_cast<std::uint16_t>(point.x) & 7U
	);
	const std::array<std::uint8_t, 1U> mask{{
		static_cast<std::uint8_t>(0x80U >> bit),
	}};
	const auto byte_left = static_cast<std::int16_t>(
		static_cast<std::uint16_t>(point.x) & 0xFFF8U
	);
	return raster.grcg_write(
		BitSource{
			std::span<const std::uint8_t>(mask),
			8U,
			1U,
			1U,
			PackedRowOrder::top_to_bottom,
		},
		{byte_left, point.y},
		GrcgWriteMode::read_modify_write,
		tile
	);
}

[[nodiscard]] bool draw_circle_pair(
	Raster &raster,
	GrcgPoint center,
	std::int16_t delta_x,
	std::int16_t delta_y,
	GrcgClipRect clip,
	GrcgTile tile,
	unsigned int vertical_shift,
	bool clipped
) noexcept
{
	const auto y_delta = static_cast<std::int16_t>(
		static_cast<std::uint16_t>(delta_y) >> vertical_shift
	);
	if(!clipped) {
		// grcg_circle_x() writes each left-hand lower/upper pair before the
		// corresponding right-hand pair, including duplicate symmetric points.
		const auto upper = subtract_word(center.y, y_delta);
		const auto lower = add_word(center.y, y_delta);
		const auto left = subtract_word(center.x, delta_x);
		const auto right = add_word(center.x, delta_x);
		return grcg_plot(raster, {left, lower}, tile) &&
			grcg_plot(raster, {left, upper}, tile) &&
			grcg_plot(raster, {right, lower}, tile) &&
			grcg_plot(raster, {right, upper}, tile);
	}
	const auto draw_row = [&, delta_x](std::int16_t y) noexcept {
		if(y < clip.top || y > clip.bottom) {
			return true;
		}
		const auto left = subtract_word(center.x, delta_x);
		if(left >= clip.left && left <= clip.right &&
			!grcg_plot(raster, {left, y}, tile)) {
			return false;
		}
		if(delta_x == 0) {
			return true;
		}
		const auto right = add_word(center.x, delta_x);
		return right < clip.left || right > clip.right ||
			grcg_plot(raster, {right, y}, tile);
	};

	// GC_CIRCL_PSET4 skips the upper half when the zoomed delta is zero.
	if(y_delta != 0) {
		const auto upper = subtract_word(center.y, y_delta);
		if(!draw_row(upper)) {
			return false;
		}
	}
	const auto lower = add_word(center.y, y_delta);
	// GC_CIRCL_PSET4 uses JAE here, omitting pixels exactly on ClipYB. The
	// all-in fast path jumps to grcg_circle_x() and does draw that edge.
	if(clipped && lower >= clip.bottom) {
		return true;
	}
	return draw_row(lower);
}

} // namespace

bool grcg_line(
	Raster &raster,
	GrcgPoint first,
	GrcgPoint second,
	GrcgClipRect clip,
	Index4 color
) noexcept
{
	const auto tile = GrcgTile::solid(color);
	if(!tile.has_value() || !valid_clip(clip, raster.raster_mode())) {
		return false;
	}

	if(!clip_line(first, second, clip)) {
		return true;
	}
	if(second.x < first.x) {
		const auto swap = first;
		first = second;
		second = swap;
	}

	const auto delta_x = static_cast<std::uint16_t>(
		subtract_word(second.x, first.x)
	);
	const auto descending = second.y < first.y;
	const auto delta_y = static_cast<std::uint16_t>(descending ?
		subtract_word(first.y, second.y) :
		subtract_word(second.y, first.y));
	const auto y_step = descending ? -1 : 1;
	auto x = first.x;
	auto y = first.y;

	if(delta_x == delta_y) {
		for(std::uint16_t index = 0U; index <= delta_x; ++index) {
			if(!grcg_plot(raster, {x, y}, *tile)) {
				return false;
			}
			x = add_word(x, 1);
			y = add_word(y, y_step);
		}
		return true;
	}

	std::uint32_t error = word_fraction;
	if(delta_x < delta_y) {
		const auto step = static_cast<std::uint16_t>(
			(static_cast<std::uint32_t>(delta_x) << word_fraction_bits) /
			delta_y
		);
		for(std::uint16_t index = 0U; index <= delta_y; ++index) {
			if(!grcg_plot(raster, {x, y}, *tile)) {
				return false;
			}
			error += step;
			if(error > word_mask) {
				error &= word_mask;
				x = add_word(x, 1);
			}
			y = add_word(y, y_step);
		}
		return true;
	}

	if(delta_y == 0U) {
		for(std::uint16_t index = 0U; index <= delta_x; ++index) {
			if(!grcg_plot(raster, {x, y}, *tile)) {
				return false;
			}
			x = add_word(x, 1);
		}
		return true;
	}
	const auto step = static_cast<std::uint16_t>(
		(static_cast<std::uint32_t>(delta_y) << word_fraction_bits) /
		delta_x
	);
	for(std::uint16_t index = 0U; index <= delta_x; ++index) {
		if(!grcg_plot(raster, {x, y}, *tile)) {
			return false;
		}
		error += step;
		if(error > word_mask) {
			error &= word_mask;
			y = add_word(y, y_step);
		}
		x = add_word(x, 1);
	}
	return true;
}

bool grcg_vline(
	Raster &raster,
	std::int16_t x,
	std::int16_t y1,
	std::int16_t y2,
	GrcgClipRect clip,
	Index4 color
) noexcept
{
	const auto tile = GrcgTile::solid(color);
	if(!tile.has_value() || !valid_clip(clip, raster.raster_mode())) {
		return false;
	}
	if(y1 >= y2) {
		const auto swap = y1;
		y1 = y2;
		y2 = swap;
	}

	// grcg_vline.asm performs both Y subtractions as signed 16-bit words.
	// The lower endpoint is rejected before the upper endpoint is clamped.
	auto lower = subtract_word(y2, clip.top);
	if(lower < 0) {
		return true;
	}
	auto upper = subtract_word(y1, clip.top);
	if(upper < 0) {
		upper = 0;
	}
	const auto clip_height = subtract_word(clip.bottom, clip.top);
	if(upper > clip_height) {
		return true;
	}
	if(lower > clip_height) {
		lower = clip_height;
	}
	if(x < clip.left || x > clip.right) {
		return true;
	}

	for(auto y = upper;; y = add_word(y, 1)) {
		if(!grcg_plot(raster, {x, add_word(y, clip.top)}, *tile)) {
			return false;
		}
		if(y == lower) {
			break;
		}
	}
	return true;
}

bool grcg_hline(
	Raster &raster,
	std::int16_t x1,
	std::int16_t x2,
	std::int16_t y,
	GrcgClipRect clip,
	Index4 color
) noexcept
{
	const auto tile = GrcgTile::solid(color);
	if(!tile.has_value() || !valid_clip(clip, raster.raster_mode())) {
		return false;
	}

	const auto relative_y = subtract_word(y, clip.top);
	const auto clip_height = static_cast<std::uint16_t>(
		subtract_word(clip.bottom, clip.top)
	);
	if(static_cast<std::uint16_t>(relative_y) > clip_height) {
		return true;
	}

	auto first = subtract_word(x1, clip.left);
	auto second = subtract_word(x2, clip.left);
	if((static_cast<std::uint16_t>(first) &
		static_cast<std::uint16_t>(second) & 0x8000U) != 0U) {
		return true;
	}
	if(first > second) {
		const auto swap = first;
		first = second;
		second = swap;
	}
	if(first < 0) {
		first = 0;
	}
	const auto clip_width = subtract_word(clip.right, clip.left);
	if(static_cast<std::uint16_t>(second) >=
		static_cast<std::uint16_t>(clip_width)) {
		second = clip_width;
	}
	if(second < first) {
		return true;
	}

	for(auto x = first;; x = add_word(x, 1)) {
		if(!grcg_plot(
			raster,
			{add_word(clip.left, x), add_word(clip.top, relative_y)},
			*tile
		)) {
			return false;
		}
		if(x == second) {
			break;
		}
	}
	return true;
}

bool grcg_boxfill(
	Raster &raster,
	GrcgPoint first,
	GrcgPoint second,
	GrcgClipRect clip,
	GrcgTile tile
) noexcept
{
	if(!valid_clip(clip, raster.raster_mode())) {
		return false;
	}
	if(first.x > second.x) {
		const auto swap = first.x;
		first.x = second.x;
		second.x = swap;
	}

	auto right = subtract_word(second.x, clip.left);
	if(right < 0) {
		return true;
	}
	auto left = subtract_word(first.x, clip.left);
	if(left < 0) {
		left = 0;
	}
	const auto clip_width = subtract_word(clip.right, clip.left);
	if(static_cast<std::uint16_t>(right) >=
		static_cast<std::uint16_t>(clip_width)) {
		right = clip_width;
	}
	if(right < left) {
		return true;
	}
	left = add_word(left, clip.left);
	right = add_word(right, clip.left);

	if(first.y > second.y) {
		const auto swap = first.y;
		first.y = second.y;
		second.y = swap;
	}
	auto lower = subtract_word(second.y, clip.top);
	if(lower < 0) {
		return true;
	}
	auto upper = subtract_word(first.y, clip.top);
	if(upper < 0) {
		upper = 0;
	}
	const auto clip_height = subtract_word(clip.bottom, clip.top);
	if(static_cast<std::uint16_t>(lower) >=
		static_cast<std::uint16_t>(clip_height)) {
		lower = clip_height;
	}
	if(lower < upper) {
		return true;
	}

	// grcg_boxfill.asm starts at the bottom row and walks upward through VRAM.
	for(auto y = lower;; y = add_word(y, -1)) {
		for(auto x = left;; x = add_word(x, 1)) {
			if(!grcg_plot(raster, {x, add_word(clip.top, y)}, tile)) {
				return false;
			}
			if(x == right) {
				break;
			}
		}
		if(y == upper) {
			break;
		}
	}
	return true;
}

bool grcg_circle(
	Raster &raster,
	GrcgPoint center,
	std::uint8_t radius,
	GrcgClipRect clip,
	Index4 color
) noexcept
{
	const auto tile = GrcgTile::solid(color);
	if(!tile.has_value() || !valid_clip(clip, raster.raster_mode())) {
		return false;
	}
	if(radius == 0U) {
		return true;
	}

	const auto signed_radius = static_cast<std::int16_t>(radius);
	if(add_word(center.y, signed_radius) < clip.top ||
		subtract_word(center.y, signed_radius) > clip.bottom ||
		add_word(center.x, signed_radius) < clip.left ||
		subtract_word(center.x, signed_radius) > clip.right) {
		return true;
	}
	const bool clipped =
		subtract_word(center.y, signed_radius) < clip.top ||
		add_word(center.y, signed_radius) > clip.bottom ||
		subtract_word(center.x, signed_radius) < clip.left ||
		add_word(center.x, signed_radius) > clip.right;

	const auto vertical_shift =
		(raster.raster_mode() == RasterMode::lines_400) ? 0U : 1U;
	auto delta_x = signed_radius;
	std::int16_t delta_y = 0;
	auto error = signed_radius;
	while(delta_x >= delta_y) {
		if(!draw_circle_pair(
			raster,
			center,
			delta_x,
			delta_y,
			clip,
			*tile,
			vertical_shift,
			clipped
		)) {
			return false;
		}
		if((!clipped || delta_x != delta_y) && !draw_circle_pair(
			raster,
			center,
			delta_y,
			delta_x,
			clip,
			*tile,
			vertical_shift,
			clipped
		)) {
			return false;
		}

		error = subtract_word(error, add_word(delta_y, delta_y + 1));
		if(error < 0) {
			delta_x = add_word(delta_x, -1);
			error = add_word(error, static_cast<std::int32_t>(delta_x) * 2);
		}
		delta_y = add_word(delta_y, 1);
	}
	return true;
}

} // namespace th03::present
