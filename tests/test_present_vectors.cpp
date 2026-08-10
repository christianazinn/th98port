#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

#include "th03/present/vectors.hpp"

namespace {

using th03::present::GrcgClipRect;
using th03::present::GrcgPoint;
using th03::present::Index4;
using th03::present::Page;
using th03::present::Raster;
using th03::present::RasterMode;

constexpr GrcgClipRect full_400_line_clip{0, 0, 639, 399};
constexpr GrcgClipRect full_200_line_clip{0, 0, 639, 199};
constexpr Index4 vector_color = 13U;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

template<std::size_t Count>
void expect_exact_pixels(
	const Raster &raster,
	Page page,
	const std::array<GrcgPoint, Count> &expected,
	std::string_view message
)
{
	for(const auto point : expected) {
		expect(
			raster.pixel(page, {point.x, point.y}) == vector_color,
			message
		);
	}
	std::size_t actual_count = 0U;
	for(const auto pixel : raster.graphics(page)) {
		actual_count += (pixel == vector_color) ? 1U : 0U;
	}
	expect(actual_count == Count, message);
}

void test_axis_and_diagonal_lines()
{
	{
		Raster raster;
		expect(raster.clear({0, 0, 16U, 8U}, 4U), "seed horizontal background");
		expect(
			th03::present::grcg_line(
				raster, {6, 3}, {2, 3}, full_400_line_clip, vector_color
			),
			"reversed horizontal line draws"
		);
		constexpr std::array<GrcgPoint, 5U> expected{{
			{2, 3}, {3, 3}, {4, 3}, {5, 3}, {6, 3},
		}};
		expect_exact_pixels(raster, Page::zero, expected, "horizontal endpoints inclusive");
		expect(
			raster.pixel(Page::zero, {1, 3}) == 4U &&
			raster.pixel(Page::zero, {7, 3}) == 4U,
			"solid RMW line preserves unselected neighboring pixels"
		);
	}
	{
		Raster raster;
		expect(
			th03::present::grcg_line(
				raster, {5, 6}, {5, 2}, full_400_line_clip, vector_color
			),
			"reversed vertical line draws"
		);
		constexpr std::array<GrcgPoint, 5U> expected{{
			{5, 2}, {5, 3}, {5, 4}, {5, 5}, {5, 6},
		}};
		expect_exact_pixels(raster, Page::zero, expected, "vertical endpoints inclusive");
	}
	{
		Raster raster;
		expect(
			th03::present::grcg_line(
				raster, {1, 4}, {4, 1}, full_400_line_clip, vector_color
			),
			"ascending 45-degree line draws"
		);
		constexpr std::array<GrcgPoint, 4U> expected{{
			{1, 4}, {2, 3}, {3, 2}, {4, 1},
		}};
		expect_exact_pixels(raster, Page::zero, expected, "45-degree path is exact");
	}
	{
		Raster raster;
		expect(
			th03::present::grcg_line(
				raster, {9, 9}, {9, 9}, full_400_line_clip, vector_color
			),
			"single-dot line draws without division"
		);
		constexpr std::array<GrcgPoint, 1U> expected{{{9, 9}}};
		expect_exact_pixels(
			raster, Page::zero, expected, "single-dot line writes one endpoint"
		);
	}
}

void test_fixed_fraction_slopes()
{
	{
		Raster raster;
		expect(
			th03::present::grcg_line(
				raster, {1, 1}, {8, 4}, full_400_line_clip, vector_color
			),
			"shallow line draws"
		);
		constexpr std::array<GrcgPoint, 8U> expected{{
			{1, 1}, {2, 1}, {3, 2}, {4, 2},
			{5, 3}, {6, 3}, {7, 4}, {8, 4},
		}};
		expect_exact_pixels(
			raster, Page::zero, expected, "shallow Q0.16 stepping retains half bias"
		);
	}
	{
		Raster raster;
		expect(
			th03::present::grcg_line(
				raster, {2, 1}, {5, 8}, full_400_line_clip, vector_color
			),
			"steep line draws"
		);
		constexpr std::array<GrcgPoint, 8U> expected{{
			{2, 1}, {2, 2}, {3, 3}, {3, 4},
			{4, 5}, {4, 6}, {5, 7}, {5, 8},
		}};
		expect_exact_pixels(
			raster, Page::zero, expected, "steep Q0.16 stepping retains half bias"
		);
	}
}

void test_line_clipping_and_access_page()
{
	Raster raster;
	expect(raster.set_access_page(Page::one), "select line destination page");
	constexpr GrcgClipRect clip{2, 2, 7, 7};
	expect(
		th03::present::grcg_line(
			raster, {0, 3}, {9, 6}, clip, vector_color
		),
		"two-ended clipped line draws"
	);
	constexpr std::array<GrcgPoint, 6U> expected{{
		{2, 4}, {3, 4}, {4, 4}, {5, 5}, {6, 5}, {7, 5},
	}};
	expect_exact_pixels(
		raster, Page::one, expected, "cutline truncation and raster stepping are exact"
	);
	for(const auto point : expected) {
		expect(
			raster.pixel(Page::zero, {point.x, point.y}) == 0U,
			"line changes only the access page"
		);
	}

	const std::vector<Index4> before(
		raster.graphics(Page::one).begin(),
		raster.graphics(Page::one).end()
	);
	expect(
		th03::present::grcg_line(raster, {0, 0}, {1, 1}, clip, vector_color),
		"shared-outcode line is accepted as invisible"
	);
	expect(
		std::vector<Index4>(
			raster.graphics(Page::one).begin(),
			raster.graphics(Page::one).end()
		) == before,
		"shared-outcode rejection changes no pixels"
	);
}

void test_horizontal_then_vertical_cutline()
{
	{
		Raster raster;
		constexpr GrcgClipRect clip{2, 2, 7, 7};
		expect(
			th03::present::grcg_line(
				raster, {-10, -10}, {4, 11}, clip, vector_color
			),
			"post-horizontal vertical cut draws"
		);
		constexpr std::array<GrcgPoint, 1U> expected{{{2, 7}}};
		expect_exact_pixels(
			raster, Page::zero, expected,
			"vertical boundary follows the horizontally clipped Y"
		);
	}
	{
		Raster raster;
		constexpr GrcgClipRect clip{16, 8, 303, 191};
		expect(
			th03::present::grcg_line(
				raster, {334, 123}, {287, 228}, clip, vector_color
			),
			"common-effect corner line draws"
		);
		constexpr std::array<GrcgPoint, 1U> expected{{{303, 191}}};
		expect_exact_pixels(
			raster, Page::zero, expected,
			"common-effect clipping retains the source corner pixel"
		);
	}
}

void test_clip_top_subtraction_wraps_before_outcode()
{
	Raster raster;
	constexpr GrcgClipRect clip{2, 2, 7, 7};
	expect(
		th03::present::grcg_line(
			raster, {3, -32768}, {3, 3}, clip, vector_color
		),
		"signed-word pre-outcode normalization draws"
	);
	constexpr std::array<GrcgPoint, 5U> expected{{
		{3, 3}, {3, 4}, {3, 5}, {3, 6}, {3, 7},
	}};
	expect_exact_pixels(
		raster, Page::zero, expected,
		"ClipYT subtraction wraps before endpoint classification"
	);
}

void test_vertical_cutline_order()
{
	Raster raster;
	constexpr GrcgClipRect clip{2, 2, 7, 7};
	expect(
		th03::present::grcg_line(
			raster, {3, 0}, {6, 9}, clip, vector_color
		),
		"top-and-bottom clipped line draws"
	);
	constexpr std::array<GrcgPoint, 6U> expected{{
		{4, 2}, {4, 3}, {4, 4}, {5, 5}, {5, 6}, {5, 7},
	}};
	expect_exact_pixels(
		raster, Page::zero, expected,
		"vertical cutline IDIV truncation precedes steep raster stepping"
	);
}

void test_circle_400_line_midpoint_path()
{
	Raster raster;
	expect(
		th03::present::grcg_circle(
			raster, {10, 10}, 3U, full_400_line_clip, vector_color
		),
		"400-line circle draws"
	);
	constexpr std::array<GrcgPoint, 16U> expected{{
		{7, 10}, {13, 10},
		{10, 7}, {10, 13},
		{7, 9}, {13, 9}, {7, 11}, {13, 11},
		{9, 7}, {11, 7}, {9, 13}, {11, 13},
		{8, 8}, {12, 8}, {8, 12}, {12, 12},
	}};
	expect_exact_pixels(
		raster, Page::zero, expected, "radius-3 midpoint circle pixels are exact"
	);
}

void test_zoomed_and_clipped_circle_quirks()
{
	{
		Raster raster;
		expect(
			raster.set_raster_mode(RasterMode::zoomed_200_upper),
			"select MAIN 200-line raster"
		);
		expect(
			th03::present::grcg_circle(
				raster, {20, 20}, 3U, full_200_line_clip, vector_color
			),
			"zoomed circle draws"
		);
		constexpr std::array<GrcgPoint, 12U> expected{{
			{17, 20}, {23, 20},
			{20, 19}, {20, 21},
			{19, 19}, {21, 19}, {19, 21}, {21, 21},
			{18, 19}, {22, 19}, {18, 21}, {22, 21},
		}};
		expect_exact_pixels(
			raster, Page::zero, expected,
			"graph_VramZoom halves only circle Y deltas"
		);
	}
	{
		Raster raster;
		constexpr GrcgClipRect clip{0, 0, 10, 10};
		expect(
			th03::present::grcg_circle(
				raster, {5, 9}, 3U, clip, vector_color
			),
			"bottom-clipped circle draws"
		);
		constexpr std::array<GrcgPoint, 9U> expected{{
			{2, 9}, {8, 9}, {2, 8}, {8, 8},
			{5, 6}, {4, 6}, {6, 6}, {3, 7}, {7, 7},
		}};
		expect_exact_pixels(
			raster, Page::zero, expected,
			"clipped PSET4 omits its lower edge at ClipYB"
		);
		for(std::int32_t x = 0; x <= 10; ++x) {
			expect(
				raster.pixel(Page::zero, {x, 10}) == 0U,
				"no clipped-circle pixel lands exactly on the bottom edge"
			);
		}
	}
	{
		Raster raster;
		expect(
			raster.set_raster_mode(RasterMode::zoomed_200_upper),
			"select clipped MAIN circle raster"
		);
		constexpr GrcgClipRect clip{0, 0, 10, 10};
		expect(
			th03::present::grcg_circle(
				raster, {5, 9}, 3U, clip, vector_color
			),
			"zoomed bottom-clipped circle draws"
		);
		constexpr std::array<GrcgPoint, 7U> expected{{
			{2, 9}, {8, 9}, {5, 8}, {4, 8},
			{6, 8}, {3, 8}, {7, 8},
		}};
		expect_exact_pixels(
			raster, Page::zero, expected,
			"zoom scaling precedes the clipped PSET4 bottom-edge test"
		);
	}
	{
		Raster raster;
		constexpr GrcgClipRect clip{0, 0, 10, 10};
		expect(
			th03::present::grcg_circle(
				raster, {5, 7}, 3U, clip, vector_color
			),
			"all-in tangent circle draws"
		);
		expect(
			raster.pixel(Page::zero, {4, 10}) == vector_color &&
			raster.pixel(Page::zero, {6, 10}) == vector_color,
			"all-in grcg_circle_x path retains pixels on ClipYB"
		);
	}
}

void test_zero_and_invalid_requests_are_atomic()
{
	Raster raster;
	expect(raster.clear({0, 0, 32U, 32U}, 4U), "seed atomic vector page");
	const auto before = raster.display_state_hash();
	expect(
		th03::present::grcg_circle(
			raster, {10, 10}, 0U, full_400_line_clip, vector_color
		),
		"zero-radius source circle is a successful no-op"
	);
	expect(raster.display_state_hash() == before, "zero-radius circle is atomic");
	expect(
		!th03::present::grcg_line(
			raster, {1, 1}, {2, 2}, {5, 0, 4, 10}, vector_color
		),
		"reversed clip is rejected"
	);
	expect(
		!th03::present::grcg_circle(
			raster, {10, 10}, 3U, full_400_line_clip, 16U
		),
		"invalid solid GRCG color is rejected"
	);
	expect(
		raster.display_state_hash() == before,
		"invalid vector requests leave the page unchanged"
	);
}

} // namespace

int main()
{
	test_axis_and_diagonal_lines();
	test_fixed_fraction_slopes();
	test_line_clipping_and_access_page();
	test_horizontal_then_vertical_cutline();
	test_clip_top_subtraction_wraps_before_outcode();
	test_vertical_cutline_order();
	test_circle_400_line_midpoint_path();
	test_zoomed_and_clipped_circle_quirks();
	test_zero_and_invalid_requests_are_atomic();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return (failures == 0U) ? EXIT_SUCCESS : EXIT_FAILURE;
}
