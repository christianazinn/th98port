#include "th03/present/triangles.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace th03::present {
namespace {

constexpr std::uint32_t word_modulus = 0x10000U;
constexpr std::uint32_t word_mask = word_modulus - 1U;
constexpr std::int16_t zoomed_raster_height = static_cast<std::int16_t>(
	graphics_height / 2U
);

struct Linework {
	std::int16_t x = 0;
	std::uint16_t dlx = 0U;
	std::uint16_t s = 0x8000U;
	std::int16_t d = 0;
};

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

[[nodiscard]] constexpr bool valid_color(Index4 color) noexcept
{
	return color <= 0x0FU;
}

// Direct make_linework.asm translation. The input vertical delta is normally
// nonnegative after grcg_triangle()'s source sort; preserving the source's
// unsigned zero-to-one correction also covers a horizontal side exactly.
[[nodiscard]] Linework make_linework(
	std::int16_t top_x,
	std::int16_t bottom_x,
	std::int16_t vertical_delta
) noexcept
{
	Linework result{};
	result.x = top_x;
	const auto divisor_bits = static_cast<std::uint16_t>(vertical_delta);
	const auto adjusted_divisor = (divisor_bits == 0U) ?
		std::uint16_t{1U} : divisor_bits;
	const auto signed_divisor = static_cast<std::int16_t>(adjusted_divisor);
	const auto delta = subtract_word(bottom_x, top_x);
	const auto quotient = static_cast<std::int16_t>(
		static_cast<std::int32_t>(delta) /
		static_cast<std::int32_t>(signed_divisor)
	);
	const auto remainder = static_cast<std::int16_t>(
		static_cast<std::int32_t>(delta) %
		static_cast<std::int32_t>(signed_divisor)
	);
	result.d = (remainder < 0) ? add_word(quotient, -1) : quotient;

	// make_linework's `add/xor/div/add/xor` sequence turns the IDIV remainder
	// into the exact unsigned Q0.16 increment. Keeping its negative-remainder
	// path verbatim retains the one-unit asymmetry of negative source slopes.
	const auto sign_mask = (remainder < 0) ? std::uint16_t{0xFFFFU} :
		std::uint16_t{0U};
	auto magnitude = static_cast<std::uint16_t>(remainder);
	magnitude = static_cast<std::uint16_t>(magnitude + sign_mask);
	magnitude = static_cast<std::uint16_t>(magnitude ^ sign_mask);
	auto fraction = static_cast<std::uint16_t>(
		(static_cast<std::uint32_t>(magnitude) << 16U) /
		adjusted_divisor
	);
	fraction = static_cast<std::uint16_t>(fraction + sign_mask);
	result.dlx = static_cast<std::uint16_t>(fraction ^ sign_mask);
	return result;
}

void advance_linework(Linework &line) noexcept
{
	const auto sum = static_cast<std::uint32_t>(line.s) + line.dlx;
	line.s = static_cast<std::uint16_t>(sum);
	const auto carry = static_cast<std::int32_t>(sum >> 16U);
	line.x = add_word(line.x, static_cast<std::int32_t>(line.d) + carry);
}

[[nodiscard]] constexpr bool mask_selects_pixel(
	std::uint16_t mask,
	std::int16_t x
) noexcept
{
	const auto pixel_in_word = static_cast<unsigned int>(
		static_cast<std::uint16_t>(x) & 0x000FU
	);
	return (mask & static_cast<std::uint16_t>(
		0x8000U >> pixel_in_word
	)) != 0U;
}

[[nodiscard]] bool draw_span(
	Raster &raster,
	std::int16_t relative_y,
	std::int16_t endpoint_a,
	std::int16_t endpoint_b,
	GrcgClipRect clip,
	std::int16_t clip_height,
	GrcgTriangleFill fill
) noexcept
{
	// draw_trapezoid compares the VRAM word address against ClipYB_adr before
	// horizontal clipping. For the valid source coordinate domain, this is the
	// same inclusive relative-y predicate, while still letting both DDAs advance
	// through rows above ClipYT.
	if(relative_y < 0 || relative_y > clip_height) {
		return true;
	}

	auto left = subtract_word(endpoint_a, clip.left);
	auto right = subtract_word(endpoint_b, clip.left);
	if((static_cast<std::uint16_t>(left) &
		static_cast<std::uint16_t>(right) & 0x8000U) != 0U) {
		return true;
	}
	if(left > right) {
		const auto temporary = left;
		left = right;
		right = temporary;
	}
	if(left < 0) {
		left = 0;
	}
	const auto clip_width = subtract_word(clip.right, clip.left);
	if(right > clip_width) {
		right = clip_width;
	}
	if(right < left) {
		return true;
	}

	// After source endpoint normalization, [left, right] is an inclusive X
	// span relative to ClipXL.
	for(auto relative_x = left; relative_x <= right; ++relative_x) {
		const auto x = add_word(clip.left, relative_x);
		if(mask_selects_pixel(fill.horizontal_mask, x) &&
			!raster.plot({x, add_word(clip.top, relative_y)}, fill.color)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool draw_blue_span(
	Raster &raster,
	std::int16_t relative_y,
	std::int16_t endpoint_a,
	std::int16_t endpoint_b,
	GrcgClipRect clip,
	std::int16_t clip_height
) noexcept
{
	if(relative_y < 0 || relative_y > clip_height) {
		return true;
	}

	auto left = subtract_word(endpoint_a, clip.left);
	auto right = subtract_word(endpoint_b, clip.left);
	if((static_cast<std::uint16_t>(left) &
		static_cast<std::uint16_t>(right) & 0x8000U) != 0U) {
		return true;
	}
	if(left > right) {
		const auto temporary = left;
		left = right;
		right = temporary;
	}
	if(left < 0) {
		left = 0;
	}
	const auto clip_width = subtract_word(clip.right, clip.left);
	if(right > clip_width) {
		right = clip_width;
	}
	if(right < left) {
		return true;
	}

	const auto absolute_left = add_word(clip.left, left);
	const auto absolute_y = add_word(clip.top, relative_y);
	return raster.set_plane(
		{
			absolute_left,
			absolute_y,
			static_cast<std::uint16_t>(
				static_cast<std::uint16_t>(right - left) + 1U
			),
			1U,
		},
		PlanarPlane::blue
	);
}

[[nodiscard]] bool draw_trapezoid(
	Raster &raster,
	std::int16_t top_y,
	std::int16_t ylen,
	Linework &side_a,
	Linework &side_b,
	GrcgClipRect clip,
	std::int16_t clip_height,
	GrcgTriangleFill fill
) noexcept
{
	auto y = top_y;
	auto remaining = ylen;
	for(;;) {
		if(!draw_span(
			raster,
			y,
			side_a.x,
			side_b.x,
			clip,
			clip_height,
			fill
		)) {
			return false;
		}
		advance_linework(side_a);
		advance_linework(side_b);
		y = add_word(y, 1);
		remaining = add_word(remaining, -1);
		if(remaining < 0) {
			return true;
		}
	}
}

[[nodiscard]] bool draw_blue_trapezoid(
	Raster &raster,
	std::int16_t top_y,
	std::int16_t ylen,
	Linework &side_a,
	Linework &side_b,
	GrcgClipRect clip,
	std::int16_t clip_height
) noexcept
{
	auto y = top_y;
	auto remaining = ylen;
	for(;;) {
		if(!draw_blue_span(
			raster,
			y,
			side_a.x,
			side_b.x,
			clip,
			clip_height
		)) {
			return false;
		}
		advance_linework(side_a);
		advance_linework(side_b);
		y = add_word(y, 1);
		remaining = add_word(remaining, -1);
		if(remaining < 0) {
			return true;
		}
	}
}

[[nodiscard]] std::optional<std::int16_t> top_intersection_x(
	GrcgPoint upper,
	GrcgPoint lower,
	std::int16_t clip_top
) noexcept
{
	const auto numerator = static_cast<std::int32_t>(
		subtract_word(upper.x, lower.x)
	) * static_cast<std::int32_t>(subtract_word(clip_top, upper.y));
	const auto denominator = subtract_word(upper.y, lower.y);
	if(denominator == 0) {
		return std::nullopt;
	}
	const auto quotient = numerator / static_cast<std::int32_t>(denominator);
	if(quotient < std::numeric_limits<std::int16_t>::min() ||
		quotient > std::numeric_limits<std::int16_t>::max()) {
		return std::nullopt;
	}
	return add_word(upper.x, quotient);
}

} // namespace

bool grcg_trapezoid(
	Raster &raster,
	std::int16_t y1,
	std::int16_t x11,
	std::int16_t x12,
	std::int16_t y2,
	std::int16_t x21,
	std::int16_t x22,
	GrcgClipRect clip,
	GrcgTriangleFill fill
) noexcept
{
	if(!valid_color(fill.color) || !valid_clip(clip, raster.raster_mode())) {
		return false;
	}

	y1 = subtract_word(y1, clip.top);
	if(y1 < 0) {
		return true;
	}
	y2 = subtract_word(y2, clip.top);
	if(y2 < 0) {
		return true;
	}
	if(y1 > y2) {
		const auto y_swap = y1;
		y1 = y2;
		y2 = y_swap;
		const auto left_swap = x11;
		x11 = x21;
		x21 = left_swap;
		const auto right_swap = x12;
		x12 = x22;
		x22 = right_swap;
	}

	const auto clip_height = subtract_word(clip.bottom, clip.top);
	if(y1 > clip_height) {
		return true;
	}
	if(y2 > clip_height) {
		y2 = clip_height;
	}
	const auto height = subtract_word(y2, y1);
	if(height < 0) {
		return true;
	}

	auto side_a = make_linework(x11, x21, height);
	auto side_b = make_linework(x12, x22, height);
	return draw_trapezoid(
		raster, y1, height, side_a, side_b, clip, clip_height, fill
	);
}

bool grcg_triangle(
	Raster &raster,
	GrcgPoint first,
	GrcgPoint second,
	GrcgPoint third,
	GrcgClipRect clip,
	GrcgTriangleFill fill
) noexcept
{
	if(!valid_color(fill.color) || !valid_clip(clip, raster.raster_mode())) {
		return false;
	}

	// grcg_triangle.asm's three signed comparisons intentionally swap equal-Y
	// vertices. Do not replace this with a stable sort: flat edges depend on its
	// exact x ordering and the DDA's asymmetric negative slopes.
	if(first.y >= second.y) {
		const auto temporary = first;
		first = second;
		second = temporary;
	}
	if(first.y >= third.y) {
		const auto temporary = first;
		first = third;
		third = temporary;
	}
	if(second.y >= third.y) {
		const auto temporary = second;
		second = third;
		third = temporary;
	}

	const auto clip_height = subtract_word(clip.bottom, clip.top);
	first.y = subtract_word(first.y, clip.top);
	if(first.y > clip_height) {
		return true;
	}
	second.y = subtract_word(second.y, clip.top);
	third.y = subtract_word(third.y, clip.top);
	if(third.y < 0) {
		return true;
	}

	const auto long_height = subtract_word(third.y, first.y);
	auto side_b = make_linework(first.x, third.x, long_height);
	if(second.y == first.y) {
		auto side_a = make_linework(
			second.x,
			third.x,
			subtract_word(third.y, second.y)
		);
		return draw_trapezoid(
			raster,
			first.y,
			long_height,
			side_a,
			side_b,
			clip,
			clip_height,
			fill
		);
	}
	if(third.y == second.y) {
		auto side_a = make_linework(
			first.x,
			second.x,
			subtract_word(second.y, first.y)
		);
		return draw_trapezoid(
			raster,
			first.y,
			long_height,
			side_a,
			side_b,
			clip,
			clip_height,
			fill
		);
	}

	const auto upper_height = subtract_word(second.y, first.y);
	auto side_a = make_linework(first.x, second.x, upper_height);
	if(!draw_trapezoid(
		raster,
		first.y,
		add_word(upper_height, -1),
		side_a,
		side_b,
		clip,
		clip_height,
		fill
	)) {
		return false;
	}
	side_a = make_linework(
		second.x,
		third.x,
		subtract_word(third.y, second.y)
	);
	return draw_trapezoid(
		raster,
		second.y,
		subtract_word(third.y, second.y),
		side_a,
		side_b,
		clip,
		clip_height,
		fill
	);
}

bool grcg_polygon_c_blue(
	Raster &raster,
	std::span<const GrcgPoint> points,
	GrcgClipRect clip
) noexcept
{
	constexpr std::size_t source_point_capacity = 10U;
	if(points.size() < 2U || points.size() > source_point_capacity ||
		!valid_clip(clip, raster.raster_mode())) {
		return false;
	}

	std::size_t left_index = 0U;
	std::size_t right_index = 0U;
	auto top_y = points[0U].y;
	auto bottom_y = points[0U].y;
	auto minimum_x = points[0U].x;
	auto maximum_x = points[0U].x;
	for(std::size_t index = 1U; index < points.size(); ++index) {
		const auto point = points[index];
		if(point.y < top_y) {
			left_index = index;
			right_index = index;
			top_y = point.y;
		} else if(point.y == top_y) {
			right_index = index;
		} else if(point.y > bottom_y) {
			bottom_y = point.y;
		}
		if(point.x <= minimum_x) {
			minimum_x = point.x;
		} else if(point.x > maximum_x) {
			maximum_x = point.x;
		}
	}

	if(clip.right < minimum_x || clip.left > maximum_x ||
		clip.bottom < top_y || clip.top > bottom_y) {
		return true;
	}
	if(bottom_y > clip.bottom) {
		bottom_y = clip.bottom;
	}
	const auto last_index = points.size() - 1U;
	if(left_index == 0U && right_index == last_index) {
		left_index = last_index;
		right_index = 0U;
	}

	const auto decrement_index = [last_index](std::size_t index) noexcept {
		return (index == 0U) ? last_index : (index - 1U);
	};
	const auto increment_index = [last_index](std::size_t index) noexcept {
		return (index == last_index) ? 0U : (index + 1U);
	};

	auto left_upper_index = left_index;
	for(std::size_t skipped = 0U;; ++skipped) {
		// grcg_polygon_c.asm updates CX before every decrement. If several
		// vertices are above ClipYT, the intersection therefore starts at the
		// final skipped vertex adjacent to the first visible one, not at the
		// polygon's original top anchor.
		left_upper_index = left_index;
		left_index = decrement_index(left_index);
		if(points[left_index].y >= clip.top) {
			break;
		}
		if(skipped >= points.size()) {
			return false;
		}
	}
	auto left_upper = points[left_upper_index];
	if(left_upper.y < clip.top) {
		const auto clipped = top_intersection_x(
			left_upper, points[left_index], clip.top
		);
		if(!clipped) {
			return false;
		}
		left_upper.x = *clipped;
		left_upper.y = clip.top;
	}
	top_y = left_upper.y;
	auto left_y = points[left_index].y;
	auto side_a = make_linework(
		left_upper.x,
		points[left_index].x,
		subtract_word(left_y, top_y)
	);

	auto right_upper_index = right_index;
	for(std::size_t skipped = 0U;; ++skipped) {
		// LOOPR likewise updates DI before every increment.
		right_upper_index = right_index;
		right_index = increment_index(right_index);
		if(points[right_index].y >= clip.top) {
			break;
		}
		if(skipped >= points.size()) {
			return false;
		}
	}
	auto right_upper = points[right_upper_index];
	if(right_upper.y < top_y) {
		const auto clipped = top_intersection_x(
			right_upper, points[right_index], clip.top
		);
		if(!clipped) {
			return false;
		}
		right_upper.x = *clipped;
	}
	auto right_y = points[right_index].y;
	auto side_b = make_linework(
		right_upper.x,
		points[right_index].x,
		subtract_word(right_y, top_y)
	);
	const auto clip_height = subtract_word(clip.bottom, clip.top);

	for(std::size_t edge_steps = 0U; edge_steps <= (points.size() * 2U);
		++edge_steps) {
		const auto next_y = (left_y <= right_y) ? left_y : right_y;
		if(bottom_y <= next_y) {
			return draw_blue_trapezoid(
				raster,
				subtract_word(top_y, clip.top),
				subtract_word(bottom_y, top_y),
				side_a,
				side_b,
				clip,
				clip_height
			);
		}
		if(!draw_blue_trapezoid(
			raster,
			subtract_word(top_y, clip.top),
			subtract_word(subtract_word(next_y, 1), top_y),
			side_a,
			side_b,
			clip,
			clip_height
		)) {
			return false;
		}
		top_y = next_y;
		if(top_y == left_y) {
			const auto previous_index = left_index;
			left_index = decrement_index(left_index);
			left_y = points[left_index].y;
			side_a = make_linework(
				points[previous_index].x,
				points[left_index].x,
				subtract_word(left_y, top_y)
			);
		}
		if(top_y == right_y) {
			const auto previous_index = right_index;
			right_index = increment_index(right_index);
			right_y = points[right_index].y;
			side_b = make_linework(
				points[previous_index].x,
				points[right_index].x,
				subtract_word(right_y, top_y)
			);
		}
	}
	return false;
}

} // namespace th03::present
