#include "th03/present/pi.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace {

using th03::assets::PiImage;
using th03::present::CutscenePiMaskStep;
using th03::present::CutscenePiQuarter;
using th03::present::Index4;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;

int failures = 0;

void expect(bool condition, std::string_view message) {
	if(!condition) {
		std::fprintf(
			stderr,
			"FAIL: %.*s\n",
			static_cast<int>(message.size()),
			message.data()
		);
		++failures;
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

PiImage make_pi(std::uint16_t width, std::uint16_t height) {
	PiImage image{};
	image.image.width = width;
	image.image.height = height;
	image.image.indices.assign(
		static_cast<std::size_t>(width) * height,
		0U
	);
	return image;
}

void fill_row(PiImage &image, std::size_t y, Index4 color) {
	const auto begin = y * image.image.width;
	for(std::size_t x = 0U; x < image.image.width; ++x) {
		image.image.indices[begin + x] = color;
	}
}

void fill_quarter(
	PiImage &image,
	CutscenePiQuarter quarter,
	Index4 color
) {
	const auto quarter_id = static_cast<std::size_t>(quarter);
	const auto left = ((quarter_id & 1U) != 0U) ? 320U : 0U;
	const auto top = ((quarter_id & 2U) != 0U) ? 200U : 0U;
	for(std::size_t y = 0U; y < 200U; ++y) {
		for(std::size_t x = 0U; x < 320U; ++x) {
			image.image.indices[((top + y) * image.image.width) + left + x] =
				color;
		}
	}
}

void test_pi_put_alignment_width_and_wrap() {
	auto image = make_pi(10U, 3U);
	fill_row(image, 0U, 1U);
	fill_row(image, 1U, 2U);
	fill_row(image, 2U, 3U);
	image.image.indices[9U] = 7U;

	Raster raster;
	expect(
		th03::present::pi_put_8(raster, 11, 399, image),
		"PI put succeeds"
	);
	expect_pixel(raster, Page::zero, {8, 399}, 1U, "PI X floors to 8 pixels");
	expect_pixel(raster, Page::zero, {15, 399}, 1U, "PI copies truncated row");
	expect_pixel(raster, Page::zero, {16, 399}, 0U, "PI width truncates to 8 pixels");
	expect_pixel(raster, Page::zero, {8, 0}, 2U, "PI rows wrap at line 400");
	expect_pixel(raster, Page::zero, {8, 1}, 3U, "PI wrap continues in order");

	expect(raster.set_access_page(Page::one), "select second PI access page");
	expect(
		th03::present::pi_put_8(raster, -1, 10, image),
		"negative PI X uses source floor and clipping"
	);
	expect_pixel(raster, Page::one, {0, 10}, 0U, "negative aligned PI is clipped out");

	const auto before_bad_top = raster.display_state_hash();
	expect(
		!th03::present::pi_put_8(raster, 0, 400, image),
		"PI put rejects an invalid initial VRAM row"
	);
	expect(
		raster.display_state_hash() == before_bad_top,
		"invalid PI top is rejected atomically"
	);
}

void test_cutscene_quarter_put() {
	auto image = make_pi(640U, 400U);
	fill_quarter(image, CutscenePiQuarter::top_left, 1U);
	fill_quarter(image, CutscenePiQuarter::top_right, 2U);
	fill_quarter(image, CutscenePiQuarter::bottom_left, 3U);
	fill_quarter(image, CutscenePiQuarter::bottom_right, 4U);

	Raster raster;
	expect(raster.clear({0, 0, 640U, 400U}, 9U), "seed cutscene page");
	expect(
		th03::present::pi_cutscene_quarter_put_8(
			raster, image, CutscenePiQuarter::bottom_right
		),
		"cutscene quarter put succeeds"
	);
	expect_pixel(raster, Page::zero, {160, 64}, 4U, "quarter top-left destination");
	expect_pixel(raster, Page::zero, {479, 263}, 4U, "quarter bottom-right destination");
	expect_pixel(raster, Page::zero, {159, 64}, 9U, "quarter keeps left neighbor");
	expect_pixel(raster, Page::zero, {480, 263}, 9U, "quarter keeps right neighbor");
	expect_pixel(raster, Page::zero, {160, 264}, 9U, "quarter keeps lower neighbor");
}

void test_cutscene_masked_steps() {
	auto image = make_pi(640U, 400U);
	fill_quarter(image, CutscenePiQuarter::top_right, 6U);

	Raster raster;
	expect(raster.clear({0, 0, 640U, 400U}, 1U), "seed masked cutscene page");
	expect(
		th03::present::pi_cutscene_quarter_put_masked(
			raster,
			image,
			CutscenePiQuarter::top_right,
			CutscenePiMaskStep::first
		),
		"first cutscene mask succeeds"
	);
	expect_pixel(raster, Page::zero, {160, 64}, 1U, "mask 0 keeps row 0");
	expect_pixel(raster, Page::zero, {162, 65}, 1U, "0x1111 keeps unselected dot");
	expect_pixel(raster, Page::zero, {163, 65}, 6U, "0x1111 copies selected dot");
	expect_pixel(raster, Page::zero, {161, 67}, 6U, "0x4444 copies selected dot");

	expect(
		th03::present::pi_cutscene_quarter_put_masked(
			raster,
			image,
			CutscenePiQuarter::top_right,
			CutscenePiMaskStep::fourth
		),
		"fourth cutscene mask succeeds"
	);
	expect_pixel(raster, Page::zero, {160, 64}, 6U, "later mask adds selected pixels");
	expect_pixel(raster, Page::zero, {163, 64}, 1U, "fourth mask is not final copy");
	expect(
		th03::present::pi_cutscene_quarter_put_8(
			raster, image, CutscenePiQuarter::top_right
		),
		"final quarter put succeeds"
	);
	expect_pixel(raster, Page::zero, {163, 64}, 6U, "final put completes reveal");
}

void test_validation_is_atomic() {
	auto image = make_pi(640U, 400U);
	Raster raster;
	expect(raster.plot({160, 64}, 5U), "seed validation destination");
	const auto before = raster.display_state_hash();

	image.image.indices.back() = 16U;
	expect(
		!th03::present::pi_cutscene_quarter_put_masked(
			raster,
			image,
			CutscenePiQuarter::top_left,
			CutscenePiMaskStep::first
		),
		"invalid PI pixel is rejected"
	);
	expect(
		raster.display_state_hash() == before,
		"invalid PI performs no partial write"
	);

	image.image.indices.back() = 0U;
	image.image.width = 320U;
	expect(
		!th03::present::pi_cutscene_quarter_put_8(
			raster, image, CutscenePiQuarter::top_left
		),
		"cutscene operation rejects non-640x400 PI"
	);
	expect(
		raster.display_state_hash() == before,
		"invalid cutscene geometry performs no write"
	);

	image = make_pi(640U, 400U);
	expect(
		!th03::present::pi_cutscene_quarter_put_8(
			raster,
			image,
			static_cast<CutscenePiQuarter>(0xFFU)
		),
		"invalid cutscene quarter is rejected"
	);
	expect(
		raster.display_state_hash() == before,
		"invalid cutscene quarter performs no write"
	);
}

} // namespace

int main() {
	test_pi_put_alignment_width_and_wrap();
	test_cutscene_quarter_put();
	test_cutscene_masked_steps();
	test_validation_is_atomic();
	if(failures == 0) {
		std::fputs("th03 PI presentation tests passed\n", stdout);
	}
	return failures == 0 ? 0 : 1;
}
