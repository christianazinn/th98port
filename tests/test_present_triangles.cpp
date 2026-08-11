#include "th03/present/triangles.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>

namespace {

using th03::present::GrcgClipRect;
using th03::present::GrcgPoint;
using th03::present::GrcgTriangleFill;
using th03::present::Index4;
using th03::present::Page;
using th03::present::Raster;
using th03::present::RasterMode;

constexpr GrcgClipRect full_400_line_clip{0, 0, 639, 399};
constexpr GrcgClipRect full_200_line_clip{0, 0, 639, 199};
constexpr Index4 triangle_color = 12U;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(
			stderr,
			"FAIL: %.*s\n",
			static_cast<int>(message.size()),
			message.data()
		);
	}
}

template<std::size_t Count>
void expect_exact_pixels(
	const Raster &raster,
	Page page,
	const std::array<GrcgPoint, Count> &expected,
	std::string_view message,
	Index4 expected_color = triangle_color
)
{
	for(const auto point : expected) {
		expect(
			raster.pixel(page, {point.x, point.y}) == expected_color,
			message
		);
	}
	std::size_t actual_count = 0U;
	for(const auto pixel : raster.graphics(page)) {
		actual_count += (pixel == expected_color) ? 1U : 0U;
	}
	expect(actual_count == Count, message);
}

void test_flat_top_sort_and_inclusive_scanlines()
{
	Raster raster;
	expect(
		th03::present::grcg_triangle(
			raster,
			{2, 1},
			{6, 1},
			{4, 4},
			full_400_line_clip,
			{triangle_color, 0xFFFFU}
		),
		"flat-top triangle draws"
	);
	// grcg_triangle.asm swaps equal-Y input vertices. The two source DDAs then
	// cover the bottom vertex and retain their distinct intermediate rounding.
	constexpr std::array<GrcgPoint, 12U> expected{{
		{2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1},
		{3, 2}, {4, 2}, {5, 2},
		{3, 3}, {4, 3}, {5, 3},
		{4, 4},
	}};
	expect_exact_pixels(
		raster,
		Page::zero,
		expected,
		"equal-Y sort, Q0.16 stepping, and endpoints are source-exact"
	);
}

void test_split_triangle_and_top_clip_progression()
{
	{
		Raster raster;
		expect(
			th03::present::grcg_triangle(
				raster,
				{2, 1},
				{5, 3},
				{3, 5},
				full_400_line_clip,
				{triangle_color, 0xFFFFU}
			),
			"non-horizontal triangle draws"
		);
		constexpr std::array<GrcgPoint, 10U> expected{{
			{2, 1},
			{2, 2}, {3, 2}, {4, 2},
			{3, 3}, {4, 3}, {5, 3},
			{3, 4}, {4, 4},
			{3, 5},
		}};
		expect_exact_pixels(
			raster,
			Page::zero,
			expected,
			"middle vertex belongs only to the second trapezoid"
		);
	}
	{
		Raster raster;
		constexpr GrcgClipRect clip{0, 0, 10, 10};
		expect(
			th03::present::grcg_triangle(
				raster,
				{2, -1},
				{6, 1},
				{4, 4},
				clip,
				{triangle_color, 0xFFFFU}
			),
			"top-clipped triangle draws"
		);
		constexpr std::array<GrcgPoint, 13U> expected{{
			{2, 0}, {3, 0}, {4, 0},
			{3, 1}, {4, 1}, {5, 1}, {6, 1},
			{3, 2}, {4, 2}, {5, 2},
			{4, 3}, {5, 3},
			{4, 4},
		}};
		expect_exact_pixels(
			raster,
			Page::zero,
			expected,
			"rows above ClipYT advance DDA state before the first visible scanline"
		);
	}
}

void test_source_horizontal_mask()
{
	for(const auto mask : {std::uint16_t{0x5555U}, std::uint16_t{0xAAAAU}}) {
		Raster raster;
		expect(
			raster.clear({10, 0, 10U, 1U}, 5U),
			"seed masked triangle span"
		);
		expect(
			th03::present::grcg_triangle(
				raster,
				{10, 0},
				{19, 0},
				{10, 2},
				full_400_line_clip,
				{triangle_color, mask}
			),
			"masked triangle draws"
		);
		for(std::int32_t x = 10; x <= 19; ++x) {
			const auto expected = ((mask & static_cast<std::uint16_t>(
				0x8000U >> static_cast<unsigned int>(x & 15)
			)) != 0U) ? triangle_color : 5U;
			expect(
				raster.pixel(Page::zero, {x, 0}) == expected,
				"trapezoid_hmask is MSB-first per physical 16-pixel word and preserves zero bits"
			);
		}
	}
}

void test_direct_trapezoid_reversal_and_degeneracy()
{
	{
		Raster raster;
		expect(
			th03::present::grcg_trapezoid(
				raster,
				5, 5, 7,
				2, 2, 4,
				full_400_line_clip,
				{triangle_color, 0xFFFFU}
			),
			"reversed-Y trapezoid draws"
		);
		constexpr std::array<GrcgPoint, 12U> expected{{
			{2, 2}, {3, 2}, {4, 2},
			{3, 3}, {4, 3}, {5, 3},
			{4, 4}, {5, 4}, {6, 4},
			{5, 5}, {6, 5}, {7, 5},
		}};
		expect_exact_pixels(
			raster, Page::zero, expected,
			"reversed-Y endpoint pairs and inclusive DDA rows are exact"
		);
	}
	{
		Raster raster;
		expect(
			th03::present::grcg_trapezoid(
				raster,
				2, 6, 2,
				2, 20, 24,
				full_400_line_clip,
				{triangle_color, 0xFFFFU}
			),
			"zero-height twisted trapezoid draws"
		);
		constexpr std::array<GrcgPoint, 5U> expected{{
			{2, 2}, {3, 2}, {4, 2}, {5, 2}, {6, 2},
		}};
		expect_exact_pixels(
			raster, Page::zero, expected,
			"zero-height trapezoid uses only its first horizontal edge"
		);
	}
	{
		Raster raster;
		expect(
			th03::present::grcg_trapezoid(
				raster,
				1, 2, 8,
				5, 8, 2,
				full_400_line_clip,
				{triangle_color, 0xFFFFU}
			),
			"crossed-side trapezoid draws"
		);
		constexpr std::array<GrcgPoint, 23U> expected{{
			{2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}, {8, 1},
			{4, 2}, {5, 2}, {6, 2}, {7, 2},
			{5, 3},
			{4, 4}, {5, 4}, {6, 4}, {7, 4},
			{2, 5}, {3, 5}, {4, 5}, {5, 5}, {6, 5}, {7, 5}, {8, 5},
		}};
		expect_exact_pixels(
			raster, Page::zero, expected,
			"crossed-side DDA endpoints are reordered independently on each row"
		);
	}
}

void test_direct_trapezoid_vertical_clip_contract()
{
	{
		Raster raster;
		constexpr GrcgClipRect clip{0, 2, 20, 8};
		const auto before = raster.display_state_hash();
		expect(
			th03::present::grcg_trapezoid(
				raster,
				1, 2, 4,
				5, 6, 8,
				clip,
				{triangle_color, 0xFFFFU}
			) && raster.display_state_hash() == before,
			"direct trapezoid rejects a first edge above ClipYT"
		);
		expect(
			th03::present::grcg_trapezoid(
				raster,
				5, 6, 8,
				1, 2, 4,
				clip,
				{triangle_color, 0xFFFFU}
			) && raster.display_state_hash() == before,
			"direct trapezoid rejects a second edge above ClipYT before reversal"
		);
	}
	{
		Raster raster;
		constexpr GrcgClipRect clip{2, 2, 7, 7};
		const auto before = raster.display_state_hash();
		expect(
			th03::present::grcg_trapezoid(
				raster,
				3, 8, 10,
				6, 10, 12,
				clip,
				{triangle_color, 0xFFFFU}
			) && raster.display_state_hash() == before,
			"fully right-clipped direct trapezoid is a no-op"
		);
	}
	{
		Raster raster;
		constexpr GrcgClipRect clip{0, 0, 12, 4};
		expect(
			th03::present::grcg_trapezoid(
				raster,
				2, 2, 2,
				8, 8, 8,
				clip,
				{triangle_color, 0xFFFFU}
			),
			"bottom-clipped direct trapezoid draws"
		);
		constexpr std::array<GrcgPoint, 3U> expected{{
			{2, 2}, {5, 3}, {8, 4},
		}};
		expect_exact_pixels(
			raster, Page::zero, expected,
			"bottom clamp precedes linework construction"
		);
	}
	{
		Raster raster;
		expect(
			raster.set_raster_mode(RasterMode::zoomed_200_upper),
			"select zoomed mode for direct trapezoid"
		);
		constexpr GrcgClipRect clip{16, 8, 303, 191};
		expect(
			th03::present::grcg_trapezoid(
				raster,
				191, 12, 307,
				191, 12, 307,
				clip,
				{triangle_color, 0xFFFFU}
			),
			"zoomed direct trapezoid clips and draws"
		);
		for(std::int32_t x = 16; x <= 303; ++x) {
			expect(
				raster.pixel(Page::zero, {x, 191}) == triangle_color,
				"zoomed direct trapezoid includes the playfield bottom"
			);
		}
		std::size_t actual_count = 0U;
		for(const auto pixel : raster.graphics(Page::zero)) {
			actual_count += (pixel == triangle_color) ? 1U : 0U;
		}
		expect(
			actual_count == 288U &&
				raster.pixel(Page::zero, {15, 191}) == 0U &&
				raster.pixel(Page::zero, {304, 191}) == 0U,
			"zoomed direct trapezoid changes no clipped neighbor"
		);
	}
}

void test_direct_trapezoid_absolute_mask_and_page()
{
	Raster raster;
	expect(raster.set_access_page(Page::one), "select trapezoid access page");
	expect(
		raster.clear({13, 3, 7U, 1U}, 5U),
		"seed direct trapezoid mask span"
	);
	expect(
		th03::present::grcg_trapezoid(
			raster,
			3, 13, 19,
			3, 13, 19,
			full_400_line_clip,
			{triangle_color, 0x8001U}
		),
		"masked direct trapezoid draws"
	);
	for(std::int32_t x = 13; x <= 19; ++x) {
		const auto expected = (x == 15 || x == 16) ? triangle_color : 5U;
		expect(
			raster.pixel(Page::one, {x, 3}) == expected,
			"direct trapezoid mask crosses an absolute 16-pixel boundary"
		);
		expect(
			raster.pixel(Page::zero, {x, 3}) == 0U,
			"direct trapezoid changes only the access page"
		);
	}
}

void test_clip_bounds_and_endpoint_inclusivity()
{
	Raster raster;
	constexpr GrcgClipRect clip{2, 2, 4, 3};
	expect(
		th03::present::grcg_triangle(
			raster,
			{0, 1},
			{6, 1},
			{3, 4},
			clip,
			{triangle_color, 0xFFFFU}
		),
		"clipped triangle draws"
	);
	constexpr std::array<GrcgPoint, 6U> expected{{
		{2, 2}, {3, 2}, {4, 2},
		{2, 3}, {3, 3}, {4, 3},
	}};
	expect_exact_pixels(
		raster,
		Page::zero,
		expected,
		"clip-relative DDA advance and all four inclusive clip boundaries match draw_trapezoid"
	);
}

void test_access_page_zoom_and_invalid_requests()
{
	{
		Raster raster;
		expect(raster.set_access_page(Page::one), "select triangle access page");
		expect(
			th03::present::grcg_triangle(
				raster,
				{1, 1},
				{3, 1},
				{2, 2},
				full_400_line_clip,
				{triangle_color, 0xFFFFU}
			),
			"page-one triangle draws"
		);
		expect(
			raster.pixel(Page::one, {2, 1}) == triangle_color &&
				raster.pixel(Page::zero, {2, 1}) == 0U,
			"triangle writes Raster's access page only"
		);
	}
	{
		Raster raster;
		expect(
			raster.set_raster_mode(RasterMode::zoomed_200_upper),
			"select 200-line Raster mode"
		);
		expect(
			th03::present::grcg_triangle(
				raster,
				{10, 8},
				{14, 8},
				{12, 10},
				full_200_line_clip,
				{triangle_color, 0xFFFFU}
			),
			"200-line triangle draws"
		);
		expect(
			raster.pixel(Page::zero, {10, 8}) == triangle_color &&
				raster.pixel(Page::zero, {12, 10}) == triangle_color,
			"graph_VramZoom preserves triangle's direct logical scanline Y values"
		);
	}
	{
		Raster raster;
		expect(raster.clear({0, 0, 32U, 32U}, 5U), "seed invalid triangle page");
		const auto before = raster.display_state_hash();
		expect(
			!th03::present::grcg_triangle(
				raster,
				{1, 1},
				{3, 1},
				{2, 2},
				{5, 0, 4, 10},
				{triangle_color, 0xFFFFU}
			),
			"reversed clip is rejected"
		);
		expect(
			!th03::present::grcg_triangle(
				raster,
				{1, 1},
				{3, 1},
				{2, 2},
				full_400_line_clip,
				{16U, 0xFFFFU}
			),
			"invalid solid color is rejected"
		);
		expect(
			!th03::present::grcg_trapezoid(
				raster,
				1, 1, 3,
				2, 1, 3,
				{5, 0, 4, 10},
				{triangle_color, 0xFFFFU}
			),
			"invalid direct trapezoid clip is rejected"
		);
		expect(
			!th03::present::grcg_trapezoid(
				raster,
				1, 1, 3,
				2, 1, 3,
				full_400_line_clip,
				{16U, 0xFFFFU}
			),
			"invalid direct trapezoid color is rejected"
		);
		expect(
			raster.display_state_hash() == before,
			"invalid triangle calls are atomic"
		);
	}
}

void test_polygon_top_order_and_edge_progression()
{
	{
		Raster raster;
		// The first and final vertices share the top edge. grcg_polygon_c()
		// swaps its left/right ring anchors only for this exact arrangement.
		constexpr std::array<GrcgPoint, 4U> rectangle{{
			{2, 1}, {2, 4}, {6, 4}, {6, 1},
		}};
		expect(
			th03::present::grcg_polygon_c_blue(
				raster, rectangle, full_400_line_clip
			),
			"flat-top ring-wrapped polygon draws"
		);
		constexpr std::array<GrcgPoint, 20U> expected{{
			{2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1},
			{2, 2}, {3, 2}, {4, 2}, {5, 2}, {6, 2},
			{2, 3}, {3, 3}, {4, 3}, {5, 3}, {6, 3},
			{2, 4}, {3, 4}, {4, 4}, {5, 4}, {6, 4},
		}};
		expect_exact_pixels(
			raster,
			Page::zero,
			expected,
			"polygon top-anchor swap and inclusive bottom are exact",
			1U
		);
	}
	{
		Raster raster;
		constexpr std::array<GrcgPoint, 4U> diamond{{
			{4, 1}, {7, 4}, {4, 7}, {1, 4},
		}};
		expect(
			th03::present::grcg_polygon_c_blue(
				raster, diamond, full_400_line_clip
			),
			"multi-edge diamond polygon draws"
		);
		constexpr std::array<GrcgPoint, 25U> expected{{
			{4, 1},
			{3, 2}, {4, 2}, {5, 2},
			{2, 3}, {3, 3}, {4, 3}, {5, 3}, {6, 3},
			{1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}, {6, 4}, {7, 4},
			{2, 5}, {3, 5}, {4, 5}, {5, 5}, {6, 5},
			{3, 6}, {4, 6}, {5, 6},
			{4, 7},
		}};
		expect_exact_pixels(
			raster,
			Page::zero,
			expected,
			"polygon changes both source DDAs at the middle vertices",
			1U
		);
	}
}

void test_polygon_top_clip_and_blue_plane_only()
{
	{
		Raster raster;
		constexpr GrcgClipRect clip{0, 0, 8, 5};
		constexpr std::array<GrcgPoint, 4U> diamond{{
			{4, -2}, {8, 2}, {4, 6}, {0, 2},
		}};
		expect(
			th03::present::grcg_polygon_c_blue(raster, diamond, clip),
			"top-and-bottom-clipped polygon draws"
		);
		constexpr std::array<GrcgPoint, 36U> expected{{
			{2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0},
			{1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1},
			{0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2}, {5, 2}, {6, 2}, {7, 2}, {8, 2},
			{1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3}, {6, 3}, {7, 3},
			{2, 4}, {3, 4}, {4, 4}, {5, 4}, {6, 4},
			{3, 5}, {4, 5}, {5, 5},
		}};
		expect_exact_pixels(
			raster,
			Page::zero,
			expected,
			"polygon computes direct signed top intersections before DDA setup",
			1U
		);
	}
	{
		Raster raster;
		constexpr GrcgClipRect clip{0, 0, 20, 10};
		// Both rings contain two consecutive vertices above ClipYT. The
		// master.lib walkers must intersect from the final skipped vertices
		// (5,-2) and (15,-2), rather than drawing chords from (10,-5).
		constexpr std::array<GrcgPoint, 6U> hexagon{{
			{10, -5}, {15, -2}, {16, 3},
			{10, 6}, {4, 3}, {5, -2},
		}};
		expect(
			th03::present::grcg_polygon_c_blue(raster, hexagon, clip),
			"multi-edge top-clipped polygon draws"
		);
		constexpr std::array<GrcgPoint, 7U> spans{{
			{5, 0}, {5, 1}, {4, 2}, {4, 3},
			{6, 4}, {8, 5}, {10, 6},
		}};
		constexpr std::array<std::int16_t, 7U> span_right{{
			15, 15, 16, 16, 14, 12, 10,
		}};
		std::size_t expected_count = 0U;
		for(std::size_t row = 0U; row < spans.size(); ++row) {
			for(auto x = spans[row].x; x <= span_right[row]; ++x) {
				expect(
					raster.pixel(Page::zero, {x, spans[row].y}) == 1U,
					"multi-edge top intersections preserve every source span"
				);
				++expected_count;
			}
		}
		const auto actual_count = static_cast<std::size_t>(std::count(
			raster.graphics(Page::zero).begin(),
			raster.graphics(Page::zero).end(),
			static_cast<Index4>(1U)
		));
		expect(
			actual_count == expected_count,
			"multi-edge top clipping emits no chord pixels"
		);
	}
	{
		Raster raster;
		expect(raster.set_access_page(Page::one), "select polygon access page");
		expect(
			raster.clear({0, 0, 10U, 8U}, 14U),
			"seed R/G/E bits below B-plane polygon"
		);
		constexpr std::array<GrcgPoint, 4U> rectangle{{
			{2, 2}, {7, 2}, {7, 5}, {2, 5},
		}};
		expect(
			th03::present::grcg_polygon_c_blue(
				raster, rectangle, full_400_line_clip
			),
			"B-plane-only polygon draws"
		);
		for(std::int32_t y = 0; y < 8; ++y) {
			for(std::int32_t x = 0; x < 10; ++x) {
				const auto inside = x >= 2 && x <= 7 && y >= 2 && y <= 5;
				expect(
					raster.pixel(Page::one, {x, y}) == (inside ? 15U : 14U),
					"polygon sets B while preserving every R/G/E bit"
				);
				expect(
					raster.pixel(Page::zero, {x, y}) == 0U,
					"polygon changes only the selected access page"
				);
			}
		}
	}
}

void test_polygon_noop_and_invalid_requests()
{
	Raster raster;
	expect(raster.clear({0, 0, 16U, 16U}, 6U), "seed polygon request page");
	const auto before = raster.display_state_hash();
	constexpr std::array<GrcgPoint, 4U> offscreen{{
		{-20, -20}, {-10, -20}, {-10, -10}, {-20, -10},
	}};
	expect(
		th03::present::grcg_polygon_c_blue(
			raster, offscreen, full_400_line_clip
		) && raster.display_state_hash() == before,
		"fully clipped polygon is a successful no-op"
	);
	constexpr std::array<GrcgPoint, 1U> too_few{{{2, 2}}};
	expect(
		!th03::present::grcg_polygon_c_blue(
			raster, too_few, full_400_line_clip
		),
		"single-point polygon is rejected"
	);
	constexpr std::array<GrcgPoint, 11U> too_many{};
	expect(
		!th03::present::grcg_polygon_c_blue(
			raster, too_many, full_400_line_clip
		),
		"polygon beyond source point capacity is rejected"
	);
	expect(
		!th03::present::grcg_polygon_c_blue(
			raster, offscreen, {5, 0, 4, 10}
		),
		"polygon rejects an invalid clip"
	);
	expect(
		raster.display_state_hash() == before,
		"invalid polygon requests are atomic"
	);
}

} // namespace

int main()
{
	test_flat_top_sort_and_inclusive_scanlines();
	test_split_triangle_and_top_clip_progression();
	test_source_horizontal_mask();
	test_direct_trapezoid_reversal_and_degeneracy();
	test_direct_trapezoid_vertical_clip_contract();
	test_direct_trapezoid_absolute_mask_and_page();
	test_clip_bounds_and_endpoint_inclusivity();
	test_access_page_zoom_and_invalid_requests();
	test_polygon_top_order_and_edge_progression();
	test_polygon_top_clip_and_blue_plane_only();
	test_polygon_noop_and_invalid_requests();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return 1;
	}
	std::printf("All %zu GRCG triangle checks passed\n", checks);
	return 0;
}
