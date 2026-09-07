#include "th03/present/sprite16.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>

namespace {

using th03::present::Index4;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::Rect;
using th03::present::Sprite16AtlasRect;
using th03::present::Sprite16DrawStyle;
using th03::present::Sprite16HorizontalClip;
using th03::present::Sprite16Overlap;
using th03::present::Sprite16Staging;
using th03::present::sprite16_put;
using th03::present::sprite16_put_downwards_column;
using th03::present::sprite16_put_noclip;
using th03::present::sprite16_source_top;

constexpr Sprite16HorizontalClip full_clip{0, 639};

int failures = 0;

void expect(bool condition, std::string_view message)
{
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
)
{
	const auto actual = raster.pixel(page, point);
	expect(actual.has_value() && (*actual == expected), message);
}

void put_source_pixel(
	Raster &raster,
	Sprite16AtlasRect source,
	std::uint16_t x,
	std::uint16_t raw_y,
	Index4 color
)
{
	expect(
		raster.plot(
			{
				static_cast<std::int32_t>(source.left + x),
				static_cast<std::int32_t>(
					sprite16_source_top + source.raw_top + raw_y
				),
			},
			color
		),
		"seed Sprite16 source pixel"
	);
}

void fill_source(
	Raster &raster,
	Sprite16AtlasRect source,
	Index4 color
)
{
	for(std::uint16_t y = 0U; y < source.raw_height; ++y) {
		for(std::uint16_t x = 0U; x < source.width; ++x) {
			put_source_pixel(raster, source, x, y, color);
		}
	}
}

void commit(Sprite16Staging &staging, Raster &raster)
{
	expect(staging.commit(raster), "commit synthetic Sprite16 atlas");
	expect(staging.committed(), "synthetic Sprite16 atlas is committed");
}

void test_clear_overlap_and_access_page()
{
	constexpr Sprite16AtlasRect source{32U, 10U, 32U, 3U};
	Raster raster;
	Sprite16Staging staging;
	put_source_pixel(raster, source, 0U, 0U, 1U);
	put_source_pixel(raster, source, 15U, 0U, 2U);
	put_source_pixel(raster, source, 16U, 0U, 4U);
	put_source_pixel(raster, source, 31U, 0U, 8U);
	put_source_pixel(raster, source, 0U, 1U, 3U);
	commit(staging, raster);

	expect(raster.set_access_page(Page::one), "select page one for Sprite16 draw");
	expect(
		raster.clear({5, 10, 32U, 3U}, 9U),
		"seed clear-overlap destination"
	);
	expect(
		sprite16_put(raster, staging, source, {5, 20}, full_clip),
		"draw normal clear-overlap sprite"
	);
	expect_pixel(raster, Page::one, {5, 10}, 1U, "opaque pixel replaces destination");
	expect_pixel(raster, Page::one, {6, 10}, 9U, "transparent pixel preserves destination");
	expect_pixel(raster, Page::one, {20, 10}, 2U, "unaligned first column end");
	expect_pixel(raster, Page::one, {21, 10}, 4U, "unaligned second column start");
	expect_pixel(raster, Page::one, {36, 10}, 8U, "unaligned final source pixel");
	expect_pixel(raster, Page::one, {5, 11}, 3U, "source raw row advances once");
	expect_pixel(raster, Page::zero, {5, 10}, 0U, "non-access page stays unchanged");
}

void test_or_overlap()
{
	constexpr Sprite16AtlasRect source{0U, 0U, 16U, 1U};
	Raster raster;
	Sprite16Staging staging;
	put_source_pixel(raster, source, 0U, 0U, 3U);
	commit(staging, raster);
	expect(raster.plot({40, 10}, 5U), "seed OR destination");
	expect(raster.plot({41, 10}, 5U), "seed transparent OR destination");
	const Sprite16DrawStyle style{Sprite16Overlap::or_planes, std::nullopt, 0xFFFFU};
	expect(
		sprite16_put(raster, staging, source, {40, 20}, full_clip, style),
		"draw OR-overlap sprite"
	);
	expect_pixel(raster, Page::zero, {40, 10}, 7U, "source planes OR into destination");
	expect_pixel(raster, Page::zero, {41, 10}, 5U, "zero source preserves OR destination");
}

void test_mono_color_and_write_mask()
{
	constexpr Sprite16AtlasRect source{0U, 0U, 16U, 1U};
	Raster raster;
	Sprite16Staging staging;
	fill_source(raster, source, 3U);
	commit(staging, raster);
	expect(raster.clear({32, 20, 16U, 1U}, 5U), "seed monochrome destination");
	const Sprite16DrawStyle style{
		Sprite16Overlap::or_planes,
		std::optional<Index4>{static_cast<Index4>(10U)},
		0xAAAAU,
	};
	expect(
		sprite16_put(raster, staging, source, {32, 40}, full_clip, style),
		"draw masked monochrome sprite"
	);
	for(std::int32_t x = 0; x < 16; ++x) {
		expect_pixel(
			raster,
			Page::zero,
			{32 + x, 20},
			static_cast<Index4>(((x & 1) == 0) ? 10U : 5U),
			"AAAA mask selects alternating pixels across both bytes"
		);
	}
}

void test_horizontal_column_clipping()
{
	constexpr Sprite16AtlasRect source{0U, 0U, 48U, 1U};
	Raster raster;
	Sprite16Staging staging;
	for(std::uint16_t x = 0U; x < 48U; ++x) {
		put_source_pixel(
			raster,
			source,
			x,
			0U,
			static_cast<Index4>(1U << (x / 16U))
		);
	}
	commit(staging, raster);

	expect(
		sprite16_put(raster, staging, source, {-16, 20}, full_clip),
		"clip one complete source column on the left"
	);
	expect_pixel(raster, Page::zero, {0, 10}, 2U, "left clip advances source one column");
	expect_pixel(raster, Page::zero, {15, 10}, 2U, "left-clipped first column extent");
	expect_pixel(raster, Page::zero, {16, 10}, 4U, "left clip retains final source column");
	expect_pixel(raster, Page::zero, {31, 10}, 4U, "left-clipped final extent");

	expect(
		sprite16_put(raster, staging, source, {70, 22}, {0, 100}),
		"clip complete source columns on the right"
	);
	expect_pixel(raster, Page::zero, {70, 11}, 1U, "right clip retains source first column");
	expect_pixel(raster, Page::zero, {85, 11}, 1U, "right clip retains one full column");
	expect_pixel(raster, Page::zero, {86, 11}, 0U, "right clip discards remaining columns");

	const auto before = raster.display_state_hash();
	expect(
		sprite16_put(raster, staging, source, {-48, 24}, full_clip),
		"edge-touching sprite clips to an empty draw"
	);
	expect(raster.display_state_hash() == before, "empty horizontal clip changes nothing");
}

void test_vertical_raw_row_clipping()
{
	constexpr Sprite16AtlasRect source{0U, 5U, 16U, 3U};
	Raster raster;
	Sprite16Staging staging;
	put_source_pixel(raster, source, 0U, 0U, 1U);
	put_source_pixel(raster, source, 0U, 1U, 2U);
	put_source_pixel(raster, source, 0U, 2U, 4U);
	commit(staging, raster);

	expect(
		sprite16_put(raster, staging, source, {40, -3}, full_clip),
		"clip a negative odd screen Y with signed shift semantics"
	);
	expect_pixel(raster, Page::zero, {40, 0}, 4U, "negative Y skips two source raw rows");
	expect_pixel(raster, Page::zero, {40, 1}, 0U, "negative Y draws one remaining raw row");

	expect(
		sprite16_put(raster, staging, source, {80, 398}, full_clip),
		"clip against raw row 200"
	);
	expect_pixel(raster, Page::zero, {80, 199}, 1U, "bottom clip retains source first row");
}

void test_noclip_address_wrapping()
{
	constexpr Sprite16AtlasRect source{0U, 0U, 16U, 1U};
	Raster raster;
	Sprite16Staging staging;
	fill_source(raster, source, 6U);
	commit(staging, raster);

	expect(
		sprite16_put_noclip(raster, staging, source, {640, 0}),
		"no-clip draw wraps right-edge byte addressing"
	);
	expect_pixel(raster, Page::zero, {0, 1}, 6U, "X 640 maps to the next raw row");
	expect_pixel(raster, Page::zero, {15, 1}, 6U, "wrapped right-edge sprite extent");

	expect(
		sprite16_put_noclip(raster, staging, source, {-16, 0}),
		"no-clip draw preserves unsigned word offset calculation"
	);
	expect_pixel(raster, Page::zero, {240, 102}, 6U, "negative X maps through 16-bit byte offset");
	expect_pixel(raster, Page::zero, {255, 102}, 6U, "negative-X wrapped sprite extent");
}

void test_downwards_column()
{
	constexpr Sprite16AtlasRect source{0U, 0U, 16U, 2U};
	Raster raster;
	Sprite16Staging staging;
	for(std::uint16_t x = 0U; x < source.width; ++x) {
		put_source_pixel(raster, source, x, 0U, 1U);
		put_source_pixel(raster, source, x, 1U, 2U);
	}
	commit(staging, raster);

	expect(
		sprite16_put_downwards_column(
			raster,
			staging,
			source,
			{100, -2},
			full_clip
		),
		"repeat one sprite down to raw row 199"
	);
	expect_pixel(raster, Page::zero, {100, 0}, 2U, "first repeated tile is top-clipped");
	expect_pixel(raster, Page::zero, {100, 1}, 1U, "second repeated tile starts at raw row one");
	expect_pixel(raster, Page::zero, {100, 2}, 2U, "second repeated tile is gapless");
	expect_pixel(raster, Page::zero, {100, 198}, 2U, "penultimate repeated tile reaches row 198");
	expect_pixel(raster, Page::zero, {100, 199}, 1U, "final repeated tile is bottom-clipped");
}

void test_malformed_atomicity_and_uncommitted_input()
{
	constexpr Sprite16AtlasRect source{0U, 0U, 16U, 1U};
	Raster raster;
	Sprite16Staging staging;
	put_source_pixel(raster, source, 0U, 0U, 3U);
	commit(staging, raster);
	expect(raster.plot({10, 10}, 7U), "seed malformed-call sentinel");
	const auto before = raster.display_state_hash();

	const auto expect_rejected = [&](bool result, std::string_view message) {
		expect(!result, message);
		expect(raster.display_state_hash() == before, "rejected draw is atomic");
	};
	expect_rejected(
		sprite16_put(raster, staging, {1U, 0U, 16U, 1U}, {10, 20}, full_clip),
		"reject non-column-aligned atlas X"
	);
	expect_rejected(
		sprite16_put(raster, staging, {0U, 0U, 8U, 1U}, {10, 20}, full_clip),
		"reject non-column source width"
	);
	expect_rejected(
		sprite16_put(raster, staging, {624U, 0U, 32U, 1U}, {10, 20}, full_clip),
		"reject source rectangle beyond atlas"
	);
	expect_rejected(
		sprite16_put(raster, staging, source, {10, 20}, {20, 20}),
		"reject inverted horizontal clip"
	);
	expect_rejected(
		sprite16_put(
			raster,
			staging,
			source,
			{10, 20},
			full_clip,
			{static_cast<Sprite16Overlap>(0xFFU), std::nullopt, 0xFFFFU}
		),
		"reject invalid overlap mode"
	);
	expect_rejected(
		sprite16_put(
			raster,
			staging,
			source,
			{10, 20},
			full_clip,
			{
				Sprite16Overlap::clear,
				std::optional<Index4>{static_cast<Index4>(16U)},
				0xFFFFU,
			}
		),
		"reject invalid monochrome color"
	);
	expect_rejected(
		sprite16_put_noclip(raster, staging, source, {40000, 20}),
		"reject coordinates outside signed source word"
	);

	Raster uncommitted_raster;
	Sprite16Staging uncommitted;
	const auto uncommitted_before = uncommitted_raster.display_state_hash();
	expect(
		!sprite16_put(
			uncommitted_raster,
			uncommitted,
			source,
			{10, 20},
			full_clip
		),
		"reject visible draw before alpha generation"
	);
	expect(
		uncommitted_raster.display_state_hash() == uncommitted_before,
		"uncommitted draw is atomic"
	);
	expect(
		sprite16_put(
			uncommitted_raster,
			uncommitted,
			source,
			{-16, 20},
			{0, 15}
		),
		"fully clipped draw does not require generated alpha"
	);
}

} // namespace

int main()
{
	test_clear_overlap_and_access_page();
	test_or_overlap();
	test_mono_color_and_write_mask();
	test_horizontal_column_clipping();
	test_vertical_raw_row_clipping();
	test_noclip_address_wrapping();
	test_downwards_column();
	test_malformed_atomicity_and_uncommitted_input();
	if(failures != 0) {
		std::fprintf(stderr, "%d Sprite16 test(s) failed\n", failures);
		return EXIT_FAILURE;
	}
	std::puts("Sprite16 presentation tests passed");
	return EXIT_SUCCESS;
}
