#include <array>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "th03/assets/types.hpp"
#include "th03/present.hpp"

namespace {

using th03::present::Index4;
using th03::present::IndexedSource;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::Rect;
using th03::assets::Rgb4;

void expect(bool condition, std::string_view message) {
	if(!condition) {
		std::fputs("FAIL: ", stderr);
		std::fwrite(message.data(), 1U, message.size(), stderr);
		std::fputc('\n', stderr);
		std::exit(1);
	}
}

void expect_pixel(
	const Raster &raster,
	Page page,
	Point point,
	Index4 expected,
	std::string_view message
) {
	const auto actual = raster.pixel(page, point);
	expect(actual.has_value(), message);
	expect(*actual == expected, message);
}

void test_selectors_and_clipping() {
	Raster raster;
	expect(raster.set_access_page(Page::one), "select access page one");
	expect(raster.plot({4, 5}, 7U), "plot access page one");
	expect_pixel(raster, Page::zero, {4, 5}, 0U, "page zero untouched");
	expect_pixel(raster, Page::one, {4, 5}, 7U, "page one plot");

	expect(raster.set_shown_page(Page::one), "select shown page one");
	expect(raster.shown_page() == Page::one, "shown page updated");
	expect(raster.access_page() == Page::one, "show does not alter access");
	expect(raster.set_shown_page(Page::zero), "restore shown page zero");
	expect(raster.access_page() == Page::one, "shown page remains independent");

	expect(raster.set_access_page(Page::zero), "select page zero for clipping");
	expect(raster.clear({-2, -1, 4U, 3U}, 6U), "clipped clear");
	expect_pixel(raster, Page::zero, {0, 0}, 6U, "clear starts on page");
	expect_pixel(raster, Page::zero, {1, 1}, 6U, "clear covers clipped extent");
	expect_pixel(raster, Page::zero, {2, 1}, 0U, "clear excludes right edge");
	expect_pixel(raster, Page::zero, {1, 2}, 0U, "clear excludes bottom edge");
	expect(!raster.plot({-1, 0}, 1U), "out-of-bounds plot rejected");
	expect(!raster.plot({0, 0}, 16U), "invalid index rejected");

	const std::array<Index4, 9U> source_pixels{
		1U, 2U, 3U,
		4U, 5U, 6U,
		7U, 8U, 9U,
	};
	const IndexedSource source{source_pixels, 3U, 3U, 3U};
	expect(raster.blit(source, {-1, 3}), "clipped opaque blit");
	expect_pixel(raster, Page::zero, {0, 3}, 2U, "left clip skips source x zero");
	expect_pixel(raster, Page::zero, {1, 3}, 3U, "left clip copies source x two");
	expect_pixel(raster, Page::zero, {0, 5}, 8U, "blit preserves source row");
}

void test_copy_page_overlap_and_selectors() {
	Raster raster;
	const std::array<Index4, 8U> source_pixels{
		1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
	};
	const IndexedSource source{source_pixels, 8U, 1U, 8U};
	expect(raster.blit(source, {0, 0}), "seed same-page copy source");
	expect(
		raster.copy_page(Page::zero, {0, 0, 7U, 1U}, Page::zero, {1, 0}),
		"same-page copy succeeds"
	);
	const std::array<Index4, 8U> expected_overlap{
		1U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
	};
	for(std::int32_t x = 0; x < 8; ++x) {
		expect_pixel(
			raster,
			Page::zero,
			{x, 0},
			expected_overlap[static_cast<std::size_t>(x)],
			"same-page copy has snapshot semantics"
		);
	}

	expect(raster.set_access_page(Page::one), "select page one before copy");
	expect(raster.set_shown_page(Page::zero), "select page zero before copy");
	expect(
		raster.copy_page(Page::zero, {2, 0, 3U, 1U}, Page::one, {10, 2}),
		"cross-page copy succeeds"
	);
	expect(raster.access_page() == Page::one, "copy does not alter access page");
	expect(raster.shown_page() == Page::zero, "copy does not alter shown page");
	expect_pixel(raster, Page::one, {10, 2}, 2U, "cross-page first pixel");
	expect_pixel(raster, Page::one, {12, 2}, 4U, "cross-page final pixel");
}

void test_transparent_masked_and_malformed_blits() {
	Raster raster;
	expect(raster.clear({0, 0, 40U, 40U}, 9U), "seed transparent background");
	const std::array<Index4, 6U> source_pixels{
		0U, 2U, 3U,
		4U, 0U, 6U,
	};
	const IndexedSource source{source_pixels, 3U, 2U, 3U};
	expect(
		raster.blit_transparent(source, {10, 10}, 0U),
		"transparent blit succeeds"
	);
	expect_pixel(raster, Page::zero, {10, 10}, 9U, "transparent pixel preserved");
	expect_pixel(raster, Page::zero, {11, 10}, 2U, "opaque transparent blit pixel");
	expect_pixel(raster, Page::zero, {12, 11}, 6U, "transparent blit lower row");

	const std::array<std::uint8_t, 6U> mask{
		1U, 0U, 3U,
		0U, 1U, 0U,
	};
	expect(raster.clear({20, 20, 3U, 2U}, 8U), "seed mask destination");
	expect(
		raster.blit_masked(source, mask, 3U, {20, 20}),
		"masked blit succeeds"
	);
	expect_pixel(raster, Page::zero, {20, 20}, 0U, "nonzero mask copies zero index");
	expect_pixel(raster, Page::zero, {21, 20}, 8U, "zero mask preserves destination");
	expect_pixel(raster, Page::zero, {22, 20}, 3U, "nonzero non-one mask copies");
	expect_pixel(raster, Page::zero, {21, 21}, 0U, "masked lower row copies");

	const auto before_bad_blit = raster.display_state_hash();
	const std::array<Index4, 2U> invalid_pixels{1U, 16U};
	const IndexedSource invalid_source{invalid_pixels, 2U, 1U, 2U};
	expect(!raster.blit(invalid_source, {0, 0}), "invalid source index rejected");
	expect(
		raster.display_state_hash() == before_bad_blit,
		"invalid source performs no partial write"
	);
	expect(
		!raster.blit_masked(source, mask, 2U, {20, 20}),
		"short mask stride rejected"
	);
}

void test_palette_scroll_and_hash() {
	Raster raster;
	const auto initial_hash = raster.display_state_hash();
	expect(
		raster.set_pending_palette_entry(1U, {10U, 11U, 12U}),
		"set valid pending palette entry"
	);
	expect(
		raster.pending_palette().colors[1U] == Rgb4{10U, 11U, 12U},
		"pending palette retains raw RGB4"
	);
	expect(
		raster.applied_palette().colors[1U] == Rgb4{},
		"pending edit is not applied immediately"
	);
	expect(
		raster.display_state_hash() == initial_hash,
		"pending palette is excluded from visible hash"
	);
	raster.apply_pending_palette();
	const auto applied_hash = raster.display_state_hash();
	expect(applied_hash != initial_hash, "applied palette changes visible hash");
	expect(
		!raster.set_pending_palette_entry(1U, {16U, 0U, 0U}),
		"invalid raw RGB4 rejected"
	);
	expect(
		raster.pending_palette().colors[1U] == Rgb4{10U, 11U, 12U},
		"invalid palette write leaves pending state intact"
	);

	const auto before_scroll_pixel = raster.pixel(Page::zero, {0, 0});
	raster.set_scroll_y(4U);
	expect(raster.scroll_y() == 4U, "scroll state retained");
	expect(
		raster.display_state_hash() != applied_hash,
		"scroll participates in visible hash"
	);
	expect(
		raster.pixel(Page::zero, {0, 0}) == before_scroll_pixel,
		"scroll does not modify graphics page"
	);

	Raster identical_pages;
	const auto page_zero_hash = identical_pages.display_state_hash();
	expect(identical_pages.set_shown_page(Page::one), "select identical shown page");
	expect(
		identical_pages.display_state_hash() != page_zero_hash,
		"shown page identity participates in hash"
	);
}

void configure_identically(Raster &raster) {
	const std::array<Index4, 4U> pixels{1U, 2U, 3U, 4U};
	const IndexedSource source{pixels, 2U, 2U, 2U};
	expect(raster.set_access_page(Page::one), "determinism access page");
	expect(raster.blit(source, {12, 34}), "determinism blit");
	expect(raster.set_pending_palette_entry(3U, {4U, 5U, 6U}), "determinism palette");
	raster.apply_pending_palette();
	raster.set_scroll_y(8U);
	expect(raster.set_shown_page(Page::one), "determinism shown page");
}

void test_deterministic_hash() {
	Raster first;
	Raster second;
	configure_identically(first);
	configure_identically(second);
	expect(
		first.display_state_hash() == second.display_state_hash(),
		"identical command streams hash identically"
	);
}

} // namespace

int main() {
	test_selectors_and_clipping();
	test_copy_page_overlap_and_selectors();
	test_transparent_masked_and_malformed_blits();
	test_palette_scroll_and_hash();
	test_deterministic_hash();
	std::fputs("th03-present tests passed\n", stdout);
	return 0;
}
