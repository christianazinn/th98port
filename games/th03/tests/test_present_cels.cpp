#include "th03/assets/decode.hpp"
#include "th03/present/cels.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

using th03::assets::MrsImage;
using th03::assets::PlanarCel4;
using th03::assets::PlanarColorPlane;
using th03::present::CdgAlphaMode;
using th03::present::Index4;
using th03::present::MrsColorMode;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;

int failures = 0;

void expect(bool condition, std::string_view message) {
	if(!condition) {
		std::fprintf(stderr, "FAIL: %.*s\n", static_cast<int>(message.size()), message.data());
		failures++;
	}
}

void expect_pixel(
	const Raster &raster,
	Page page,
	Point point,
	Index4 expected,
	std::string_view message
) {
	expect(raster.pixel(page, point) == expected, message);
}

PlanarCel4 make_cel(
	std::uint16_t width,
	std::uint16_t height,
	bool with_alpha = true
) {
	PlanarCel4 cel{};
	const auto stride = static_cast<std::size_t>(width / 8U);
	const auto plane_size = stride * height;
	cel.bitplane_size = static_cast<std::uint16_t>(plane_size);
	cel.pixel_width = width;
	cel.pixel_height = height;
	cel.offset_at_bottom_left = static_cast<std::int16_t>((height - 1U) * 80U);
	cel.vram_dword_width = static_cast<std::uint16_t>(width / 32U);
	if(with_alpha) {
		cel.alpha = std::vector<std::uint8_t>(plane_size, 0U);
	}
	for(auto &plane : cel.color_planes) {
		plane.assign(plane_size, 0U);
	}
	return cel;
}

std::size_t stored_offset(
	const PlanarCel4 &cel,
	std::size_t x,
	std::size_t y
) {
	const auto stride = static_cast<std::size_t>(cel.pixel_width / 8U);
	const auto stored_y = static_cast<std::size_t>(cel.pixel_height) - 1U - y;
	return (stored_y * stride) + (x / 8U);
}

std::uint8_t pixel_mask(std::size_t x) {
	return static_cast<std::uint8_t>(0x80U >> (x & 7U));
}

void set_alpha(PlanarCel4 &cel, std::size_t x, std::size_t y) {
	(*cel.alpha)[stored_offset(cel, x, y)] = static_cast<std::uint8_t>(
		(*cel.alpha)[stored_offset(cel, x, y)] | pixel_mask(x)
	);
}

void set_color(
	PlanarCel4 &cel,
	std::size_t x,
	std::size_t y,
	Index4 color
) {
	const auto offset = stored_offset(cel, x, y);
	for(std::size_t plane = 0U; plane < cel.color_planes.size(); ++plane) {
		if(((color >> plane) & 1U) != 0U) {
			cel.color_planes[plane][offset] = static_cast<std::uint8_t>(
				cel.color_planes[plane][offset] | pixel_mask(x)
			);
		}
	}
}

PlanarCel4 make_solid_cel(
	std::uint16_t width,
	std::uint16_t height,
	Index4 color
) {
	auto cel = make_cel(width, height);
	cel.alpha->assign(cel.bitplane_size, 0xFFU);
	for(std::size_t plane = 0U; plane < cel.color_planes.size(); ++plane) {
		const auto byte = static_cast<std::uint8_t>(
			(((color >> plane) & 1U) != 0U) ? 0xFFU : 0x00U
		);
		cel.color_planes[plane].assign(cel.bitplane_size, byte);
	}
	return cel;
}

MrsImage make_mrs() {
	MrsImage image{};
	image.cel = make_cel(288U, 184U);
	return image;
}

void test_cdg_alpha_and_noalpha() {
	auto cel = make_cel(32U, 2U);
	set_alpha(cel, 0U, 0U);
	set_color(cel, 0U, 0U, 2U);
	set_color(cel, 1U, 0U, 1U);
	set_alpha(cel, 31U, 1U);
	set_color(cel, 31U, 1U, 12U);

	Raster alpha;
	expect(alpha.clear({0, 0, 64U, 16U}, 4U), "seed CDG alpha destination");
	expect(
		th03::present::cdg_put_8(alpha, 15, 3, cel),
		"CDG alpha put succeeds"
	);
	expect_pixel(alpha, Page::zero, {8, 3}, 2U, "CDG left floors to 8 pixels");
	expect_pixel(alpha, Page::zero, {39, 4}, 12U, "CDG bottom-up row maps to bottom");
	expect_pixel(
		alpha,
		Page::zero,
		{9, 3},
		5U,
		"CDG color OR remains source-exact outside alpha"
	);
	expect_pixel(alpha, Page::zero, {10, 3}, 4U, "CDG alpha preserves empty background");

	auto noalpha_cel = cel;
	noalpha_cel.alpha.reset();
	Raster noalpha;
	expect(noalpha.clear({0, 0, 64U, 16U}, 9U), "seed CDG no-alpha destination");
	expect(
		th03::present::cdg_put_noalpha_8(noalpha, 15, 3, noalpha_cel),
		"CDG no-alpha put accepts a discarded alpha plane"
	);
	expect_pixel(noalpha, Page::zero, {8, 3}, 2U, "CDG no-alpha copies color");
	expect_pixel(noalpha, Page::zero, {9, 3}, 1U, "CDG no-alpha replaces full colors");
	expect_pixel(noalpha, Page::zero, {10, 3}, 0U, "CDG no-alpha replaces background");

	const auto before = noalpha.display_state_hash();
	expect(
		!th03::present::cdg_put_8(noalpha, 0, 0, noalpha_cel),
		"CDG alpha put rejects a missing alpha plane"
	);
	expect(
		noalpha.display_state_hash() == before,
		"invalid CDG alpha put is atomic"
	);
}

void test_cdg_horizontal_flip() {
	auto cel = make_cel(32U, 2U);
	set_alpha(cel, 0U, 0U);
	set_color(cel, 0U, 0U, 3U);
	set_alpha(cel, 31U, 1U);
	set_color(cel, 31U, 1U, 5U);

	Raster raster;
	expect(raster.clear({0, 0, 64U, 16U}, 6U), "seed CDG flip destination");
	expect(
		th03::present::cdg_put_hflip_8(raster, 11, 7, cel),
		"CDG horizontal flip succeeds"
	);
	expect_pixel(raster, Page::zero, {39, 7}, 3U, "CDG left edge flips to right");
	expect_pixel(raster, Page::zero, {8, 8}, 5U, "CDG right edge flips to left");
	expect_pixel(raster, Page::zero, {8, 7}, 6U, "CDG flip retains alpha transparency");
}

void test_cdg_dissolve() {
	const auto cel = make_solid_cel(32U, 4U, 15U);
	Raster raster;
	expect(
		th03::present::cdg_put_dissolve_e_8(
			raster, 24, 10, cel, 1U, CdgAlphaMode::no_alpha
		),
		"CDG dissolve succeeds"
	);
	expect_pixel(raster, Page::zero, {8, 8}, 15U, "dissolve row 0 keeps pattern zero");
	expect_pixel(raster, Page::zero, {10, 8}, 7U, "dissolve row 0 clears E at bit 5");
	expect_pixel(raster, Page::zero, {9, 9}, 7U, "dissolve row 1 clears alternating E");
	expect_pixel(raster, Page::zero, {8, 10}, 7U, "dissolve row 2 clears leading E");
	expect_pixel(raster, Page::zero, {9, 10}, 15U, "dissolve changes E only where selected");

	Raster modulo;
	expect(
		th03::present::cdg_put_dissolve_e_8(
			modulo, 24, 10, cel, 9U, CdgAlphaMode::no_alpha
		),
		"CDG dissolve strength is reduced modulo 8"
	);
	expect(
		modulo.display_state_hash() == raster.display_state_hash(),
		"CDG dissolve strengths 1 and 9 are identical"
	);

	auto without_alpha = cel;
	without_alpha.alpha.reset();
	const auto before = modulo.display_state_hash();
	expect(
		!th03::present::cdg_put_dissolve_e_8(
			modulo, 24, 10, without_alpha, 1U, CdgAlphaMode::alpha
		),
		"CDG alpha dissolve rejects a discarded alpha plane"
	);
	expect(
		modulo.display_state_hash() == before,
		"invalid CDG dissolve is atomic"
	);
}

void test_cdg_upward_unput() {
	Raster raster;
	expect(raster.clear({0, 0, 64U, 32U}, 15U), "seed CDG upward unput");
	const auto geometry = make_cel(32U, 4U, false);
	expect(
		th03::present::cdg_unput_for_upwards_motion_e_8(
			raster, 27, 20, geometry
		),
		"CDG upward unput succeeds from metadata alone"
	);
	for(std::int32_t y = 20; y <= 22; ++y) {
		expect_pixel(raster, Page::zero, {8, y}, 7U, "upward unput clears E");
		expect_pixel(raster, Page::zero, {39, y}, 7U, "upward unput covers full width");
	}
	expect_pixel(raster, Page::zero, {7, 20}, 15U, "upward unput keeps left neighbor");
	expect_pixel(raster, Page::zero, {8, 19}, 15U, "upward unput keeps row above");
	expect_pixel(raster, Page::zero, {8, 23}, 15U, "upward unput clears exactly three rows");
}

void test_mrs_put_paths_and_screen_y() {
	auto image = make_mrs();
	set_alpha(image.cel, 0U, 0U);
	set_color(image.cel, 0U, 0U, 2U);
	set_color(image.cel, 1U, 0U, 1U);
	set_color(image.cel, 2U, 0U, 8U);
	set_color(image.cel, 40U, 0U, 2U);

	Raster alpha;
	expect(alpha.clear({0, 0, 320U, 200U}, 4U), "seed MRS alpha destination");
	expect(
		th03::present::mrs_put_8(alpha, 19, 17U, image),
		"MRS alpha put succeeds"
	);
	expect_pixel(alpha, Page::zero, {16, 8}, 2U, "MRS floors X and halves screen Y");
	expect_pixel(
		alpha,
		Page::zero,
		{17, 8},
		5U,
		"MRS nonempty alpha dword ORs all color bits"
	);
	expect_pixel(
		alpha,
		Page::zero,
		{56, 8},
		4U,
		"MRS zero alpha dword skips all color planes"
	);

	Raster regular;
	expect(regular.clear({0, 0, 320U, 200U}, 9U), "seed regular MRS destination");
	expect(
		th03::present::mrs_put_noalpha_8(
			regular, 19, 17U, image, MrsColorMode::regular
		),
		"regular MRS no-alpha put succeeds"
	);
	expect_pixel(regular, Page::zero, {16, 8}, 2U, "regular MRS copies foreground");
	expect_pixel(regular, Page::zero, {17, 8}, 1U, "regular MRS replaces full colors");
	expect_pixel(regular, Page::zero, {19, 8}, 0U, "regular MRS replaces background");

	Raster altered;
	expect(
		th03::present::mrs_put_noalpha_8(
			altered, 19, 17U, image, MrsColorMode::altered
		),
		"altered MRS no-alpha put succeeds"
	);
	expect_pixel(altered, Page::zero, {16, 8}, 2U, "altered MRS keeps opaque color");
	expect_pixel(altered, Page::zero, {17, 8}, 1U, "altered MRS forces B outside alpha");
	expect_pixel(altered, Page::zero, {18, 8}, 9U, "altered MRS preserves other planes");
}

void test_mrs_persistent_horizontal_flip_and_validation() {
	auto image = make_mrs();
	set_alpha(image.cel, 0U, 0U);
	set_color(image.cel, 0U, 0U, 6U);
	th03::assets::horizontal_flip(image);

	Raster raster;
	expect(raster.clear({0, 0, 320U, 200U}, 3U), "seed flipped MRS destination");
	expect(
		th03::present::mrs_put_8(raster, 16, 16U, image),
		"MRS put consumes persistent asset flip"
	);
	expect_pixel(raster, Page::zero, {303, 8}, 6U, "MRS flipped left edge reaches right");
	expect_pixel(raster, Page::zero, {16, 8}, 3U, "MRS flipped alpha leaves old left clear");

	auto invalid = image;
	invalid.cel.pixel_width = 32U;
	const auto before = raster.display_state_hash();
	expect(
		!th03::present::mrs_put_8(raster, 0, 0U, invalid),
		"MRS put rejects non-MRS geometry"
	);
	expect(
		raster.display_state_hash() == before,
		"invalid MRS put is atomic"
	);
}

} // namespace

int main() {
	test_cdg_alpha_and_noalpha();
	test_cdg_horizontal_flip();
	test_cdg_dissolve();
	test_cdg_upward_unput();
	test_mrs_put_paths_and_screen_y();
	test_mrs_persistent_horizontal_flip_and_validation();
	if(failures == 0) {
		std::fputs("th03 presentation cel tests passed\n", stdout);
	}
	return failures == 0 ? 0 : 1;
}
