#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <utility>
#include <variant>

#include "th03/assets/types.hpp"
#include "th03/present/superimpose.hpp"

namespace {

using th03::assets::BfntConsumerPolicy;
using th03::assets::BfntGlyph;
using th03::assets::BfntSheet;
using th03::assets::PlanarBitOrder;
using th03::assets::PlanarBitmap4;
using th03::assets::PlanarRowOrder;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;

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

PlanarBitmap4 glyph_pixels(std::array<std::uint8_t, 4U> planes)
{
	PlanarBitmap4 pixels{};
	pixels.row_order = PlanarRowOrder::top_to_bottom;
	pixels.bit_order = PlanarBitOrder::most_significant_bit_left;
	for(std::size_t plane = 0U; plane < planes.size(); ++plane) {
		pixels.color_planes[plane] = {planes[plane]};
	}
	return pixels;
}

BfntSheet synthetic_sheet(std::uint8_t transparent_color = 0U)
{
	BfntSheet sheet{};
	sheet.consumer = BfntConsumerPolicy::superimpose;
	sheet.header.pixel_width = 8U;
	sheet.header.pixel_height = 1U;
	sheet.header.first_code = 10U;
	sheet.header.last_code = 11U;
	sheet.transparent_color = transparent_color;
	sheet.glyphs.push_back(BfntGlyph{
		10U,
		glyph_pixels({0x80U, 0x40U, 0x20U, 0x10U}),
	});
	sheet.glyphs.push_back(BfntGlyph{
		11U,
		glyph_pixels({0xAAU, 0xCCU, 0xF0U, 0x00U}),
	});
	return sheet;
}

void test_zero_transparent_composition()
{
	auto decoded = synthetic_sheet();
	auto prepared = th03::present::prepare_superimpose_sheet(decoded);
	expect(prepared.has_value(), "valid superimpose sheet prepares");
	expect(prepared->first_code() == 10U, "prepared first code retained");
	expect(prepared->last_code() == 11U, "prepared last code retained");

	Raster raster;
	expect(raster.set_access_page(Page::one), "select page one");
	expect(raster.clear({0, 0, 16U, 4U}, 6U), "seed destination");
	expect(
		th03::present::superimpose_put(raster, *prepared, {3, 2}, 10U),
		"put zero-transparent glyph"
	);
	const std::array<std::uint8_t, 8U> expected{{1U, 2U, 4U, 8U, 6U, 6U, 6U, 6U}};
	for(std::size_t x = 0U; x < expected.size(); ++x) {
		expect(
			raster.pixel(Page::one, {
				static_cast<std::int32_t>(x + 3U),
				2,
			}) == expected[x],
			"opaque pixels replace and transparent pixels preserve"
		);
		expect(
			raster.pixel(Page::zero, {
				static_cast<std::int32_t>(x + 3U),
				2,
			}) == 0U,
			"put changes only access page"
		);
	}
}

void test_nonzero_transparent_color()
{
	auto decoded = synthetic_sheet(5U);
	auto &pixels = std::get<PlanarBitmap4>(decoded.glyphs[0].pixels);
	// Pixels: 5, 4, 5, 0, 5, 6, 3, 2.
	pixels.color_planes[0] = {0xAAU};
	pixels.color_planes[1] = {0x07U};
	pixels.color_planes[2] = {0xECU};
	pixels.color_planes[3] = {0x00U};
	auto prepared = th03::present::prepare_superimpose_sheet(decoded);
	expect(prepared.has_value(), "nonzero-transparent sheet prepares");

	Raster raster;
	expect(raster.clear({0, 0, 8U, 1U}, 9U), "seed nonzero destination");
	expect(
		th03::present::superimpose_put(raster, *prepared, {0, 0}, 10U),
		"put nonzero-transparent glyph"
	);
	const std::array<std::uint8_t, 8U> expected{{9U, 4U, 9U, 0U, 9U, 6U, 3U, 2U}};
	for(std::size_t x = 0U; x < expected.size(); ++x) {
		expect(
			raster.pixel(Page::zero, {static_cast<std::int32_t>(x), 0}) ==
				expected[x],
			"nonzero transparent color derives exact opacity"
		);
	}
}

void test_prepared_sheet_owns_source_pixels()
{
	auto decoded = synthetic_sheet();
	auto prepared = th03::present::prepare_superimpose_sheet(decoded);
	expect(prepared.has_value(), "owned superimpose sheet prepares");

	for(auto &glyph : decoded.glyphs) {
		auto &pixels = std::get<PlanarBitmap4>(glyph.pixels);
		for(auto &plane : pixels.color_planes) {
			plane.assign(plane.size(), 0xFFU);
		}
	}
	decoded.glyphs.clear();

	Raster raster;
	expect(raster.clear({0, 0, 8U, 1U}, 6U), "seed owned destination");
	expect(
		th03::present::superimpose_put(raster, *prepared, {0, 0}, 10U),
		"prepared sheet survives source mutation and release"
	);
	const std::array<std::uint8_t, 8U> expected{{1U, 2U, 4U, 8U, 6U, 6U, 6U, 6U}};
	for(std::size_t x = 0U; x < expected.size(); ++x) {
		expect(
			raster.pixel(Page::zero, {static_cast<std::int32_t>(x), 0}) ==
				expected[x],
			"prepared sheet owns immutable color and opacity bytes"
		);
	}
}

void test_rejection_and_atomicity()
{
	for(std::size_t failure = 0U; failure < 9U; ++failure) {
		auto decoded = synthetic_sheet();
		switch(failure) {
		case 0U:
			decoded.consumer = BfntConsumerPolicy::gaiji;
			break;
		case 1U:
			decoded.header.pixel_width = 7U;
			break;
		case 2U:
			decoded.transparent_color = 16U;
			break;
		case 3U:
			decoded.glyphs[0].code = 9U;
			break;
		case 4U:
			std::get<PlanarBitmap4>(decoded.glyphs[0].pixels)
				.color_planes[0].clear();
			break;
		case 5U:
			std::get<PlanarBitmap4>(decoded.glyphs[0].pixels).row_order =
				PlanarRowOrder::bottom_to_top;
			break;
		case 6U:
			decoded.header.pixel_height = 256U;
			break;
		case 7U:
			decoded.header.pixel_width = 2048U;
			break;
		default:
			decoded.header.first_code = 0U;
			decoded.header.last_code = 512U;
			break;
		}
		expect(
			!th03::present::prepare_superimpose_sheet(decoded).has_value(),
			"malformed sheet is rejected"
		);
	}

	auto decoded = synthetic_sheet();
	auto prepared = th03::present::prepare_superimpose_sheet(decoded);
	Raster raster;
	expect(raster.clear({0, 0, 8U, 1U}, 12U), "seed atomicity destination");
	expect(
		!th03::present::superimpose_put(raster, *prepared, {0, 0}, 9U),
		"out-of-range glyph is rejected"
	);
	for(std::size_t x = 0U; x < 8U; ++x) {
		expect(
			raster.pixel(Page::zero, {static_cast<std::int32_t>(x), 0}) == 12U,
			"rejected glyph leaves destination unchanged"
		);
	}
}

} // namespace

int main()
{
	test_zero_transparent_composition();
	test_nonzero_transparent_color();
	test_prepared_sheet_owns_source_pixels();
	test_rejection_and_atomicity();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return (failures == 0U) ? 0 : 1;
}
