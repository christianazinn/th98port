#include "th03/present/triangles.hpp"

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
	std::string_view message
)
{
	for(const auto point : expected) {
		expect(
			raster.pixel(page, {point.x, point.y}) == triangle_color,
			message
		);
	}
	std::size_t actual_count = 0U;
	for(const auto pixel : raster.graphics(page)) {
		actual_count += (pixel == triangle_color) ? 1U : 0U;
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
			raster.display_state_hash() == before,
			"invalid triangle calls are atomic"
		);
	}
}

} // namespace

int main()
{
	test_flat_top_sort_and_inclusive_scanlines();
	test_split_triangle_and_top_clip_progression();
	test_source_horizontal_mask();
	test_clip_bounds_and_endpoint_inclusivity();
	test_access_page_zoom_and_invalid_requests();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return 1;
	}
	std::printf("All %zu GRCG triangle checks passed\n", checks);
	return 0;
}
