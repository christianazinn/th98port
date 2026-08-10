#include <array>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "th03/assets/types.hpp"
#include "th03/present.hpp"

namespace {

using th03::present::Index4;
using th03::present::IndexedSource;
using th03::present::BitSource;
using th03::present::GaijiHalf;
using th03::present::GrcgTile;
using th03::present::GrcgWriteMode;
using th03::present::MainGameMode;
using th03::present::MainHudState;
using th03::present::MainPlayerShotSheets;
using th03::present::MainStaticPresenter;
using th03::present::PackedRowOrder;
using th03::present::Page;
using th03::present::PlanarPlane;
using th03::present::PlanarSource;
using th03::present::PlanarWriteMode;
using th03::present::Point;
using th03::present::Raster;
using th03::present::RasterMode;
using th03::present::Rect;
using th03::present::Tram;
using th03::present::TramCell;
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

void expect_cell(
	const Tram &tram,
	th03::present::TramPoint point,
	TramCell expected,
	std::string_view message
) {
	const auto actual = tram.cell(point);
	expect(actual.has_value(), message);
	expect(*actual == expected, message);
}

void expect_gaiji(
	const Tram &tram,
	th03::present::TramPoint left,
	std::uint8_t glyph_id,
	th03::present::TramAttribute attribute,
	std::string_view message
) {
	expect_cell(
		tram,
		left,
		{th03::present::gaiji_cell_code(glyph_id, GaijiHalf::left), attribute},
		message
	);
	expect_cell(
		tram,
		{left.x + 1, left.y},
		{th03::present::gaiji_cell_code(glyph_id, GaijiHalf::right), attribute},
		message
	);
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
	expect(raster.palette_tone() == 100, "palette tone defaults to source 100");
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
		raster.applied_palette().colors[1U] == Rgb4{10U, 11U, 12U},
		"tone 100 applies the unmodified logical palette"
	);
	raster.set_palette_tone(50);
	expect(
		raster.display_state_hash() != applied_hash &&
		raster.applied_palette().colors[1U] == Rgb4{10U, 11U, 12U},
		"raw tone changes state but remains deferred until palette application"
	);
	raster.apply_pending_palette();
	expect(
		raster.applied_palette().colors[1U] == Rgb4{5U, 5U, 6U},
		"tone 50 uses palette_show integer black scaling"
	);
	raster.set_palette_tone(150);
	raster.apply_pending_palette();
	expect(
		raster.applied_palette().colors[1U] == Rgb4{13U, 13U, 14U},
		"tone 150 uses palette_show integer white scaling"
	);
	raster.set_palette_tone(-1);
	raster.apply_pending_palette();
	expect(
		raster.palette_tone() == -1 &&
		raster.applied_palette().colors[1U] == Rgb4{},
		"palette_show clamps locally without replacing a negative raw tone"
	);
	raster.set_palette_tone(201);
	raster.apply_pending_palette();
	expect(
		raster.palette_tone() == 201 &&
		raster.applied_palette().colors[1U] == Rgb4{15U, 15U, 15U},
		"palette_show clamps locally without replacing a high raw tone"
	);
	raster.set_palette_tone(100);
	raster.apply_pending_palette();
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
	Raster raster_mode_only;
	const auto lines_400_hash = raster_mode_only.display_state_hash();
	expect(
		raster_mode_only.set_raster_mode(RasterMode::zoomed_200_upper) &&
		raster_mode_only.display_state_hash() != lines_400_hash,
		"raster mode participates in the display-state hash"
	);
	expect(
		identical_pages.set_access_page(Page::one) &&
		identical_pages.set_raster_mode(RasterMode::zoomed_200_lower) &&
		identical_pages.access_page() == Page::zero &&
		identical_pages.raster_mode() == RasterMode::zoomed_200_lower,
		"graph_200line lower-half mode resets the access page"
	);
	expect(
		!identical_pages.set_raster_mode(static_cast<RasterMode>(0xFFU)),
		"invalid raster mode is rejected"
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

void test_planar_order_projection_and_clipping() {
	Raster raster;
	const std::array<std::uint8_t, 1U> blue{0x81U};
	const std::array<std::uint8_t, 1U> red{0x42U};
	const std::array<std::uint8_t, 1U> green{0x24U};
	const std::array<std::uint8_t, 1U> intensity{0x18U};
	PlanarSource source{};
	source.planes = {blue, red, green, intensity};
	source.width = 8U;
	source.height = 1U;
	source.stride = 1U;
	expect(
		raster.blit_planar(source, {8, 7}),
		"B/R/G/E planar blit succeeds"
	);
	const std::array<Index4, 8U> expected{1U, 2U, 4U, 8U, 8U, 4U, 2U, 1U};
	for(std::int32_t x = 0; x < 8; ++x) {
		expect_pixel(
			raster,
			Page::zero,
			{x + 8, 7},
			expected[static_cast<std::size_t>(x)],
			"planar plane and bit order"
		);
	}
	expect(
		raster.planar_byte(Page::zero, PlanarPlane::blue, 1U, 7U) == blue[0],
		"blue plane projects exactly"
	);
	expect(
		raster.planar_byte(Page::zero, PlanarPlane::red, 1U, 7U) == red[0],
		"red plane projects exactly"
	);
	expect(
		raster.planar_byte(Page::zero, PlanarPlane::green, 1U, 7U) == green[0],
		"green plane projects exactly"
	);
	expect(
		raster.planar_byte(Page::zero, PlanarPlane::intensity, 1U, 7U) ==
			intensity[0],
		"intensity plane projects exactly"
	);

	const std::array<std::uint8_t, 1U> clipped_blue{0x40U};
	PlanarSource clipped{};
	clipped.planes = {clipped_blue, red, green, intensity};
	clipped.width = 8U;
	clipped.height = 1U;
	clipped.stride = 1U;
	expect(raster.blit_planar(clipped, {-1, 20}), "left-clipped planar blit");
	expect_pixel(
		raster,
		Page::zero,
		{0, 20},
		3U,
		"left clipping advances all source planes"
	);

	const auto before_invalid = raster.display_state_hash();
	PlanarSource invalid = source;
	invalid.planes[3U] = {};
	expect(!raster.blit_planar(invalid, {0, 0}), "short planar plane rejected");
	expect(
		raster.display_state_hash() == before_invalid,
		"invalid planar source is atomic"
	);
	expect(
		!raster.planar_byte(
			Page::zero,
			static_cast<PlanarPlane>(99U),
			0U,
			0U
		).has_value(),
		"invalid plane rejected"
	);

	Raster page_raster;
	expect(page_raster.set_access_page(Page::one), "planar select access page one");
	expect(page_raster.blit_planar(source, {0, 0}), "planar write page one");
	expect_pixel(page_raster, Page::zero, {0, 0}, 0U, "planar leaves page zero");
	expect_pixel(page_raster, Page::one, {0, 0}, 1U, "planar writes access page");

	const std::array<std::uint8_t, 2U> bottom_first_blue{0x80U, 0x40U};
	const std::array<std::uint8_t, 2U> empty_rows{0U, 0U};
	PlanarSource bottom_first{};
	bottom_first.planes = {
		bottom_first_blue,
		empty_rows,
		empty_rows,
		empty_rows,
	};
	bottom_first.width = 8U;
	bottom_first.height = 2U;
	bottom_first.stride = 1U;
	bottom_first.row_order = PackedRowOrder::bottom_to_top;
	expect(
		page_raster.blit_planar(bottom_first, {0, 10}),
		"bottom-to-top planar rows succeed"
	);
	expect_pixel(page_raster, Page::one, {1, 10}, 1U, "stored last row maps to top");
	expect_pixel(page_raster, Page::one, {0, 11}, 1U, "stored first row maps to bottom");
}

void test_planar_write_modes() {
	Raster raster;
	expect(raster.clear({0, 30, 16U, 3U}, 15U), "seed planar write modes");
	const std::array<std::uint8_t, 1U> blue{0x80U};
	const std::array<std::uint8_t, 1U> red{0x40U};
	const std::array<std::uint8_t, 1U> empty{0x00U};
	const std::array<std::uint8_t, 1U> alpha_bits{0xC0U};
	PlanarSource source{};
	source.planes = {blue, red, empty, empty};
	source.width = 8U;
	source.height = 1U;
	source.stride = 1U;
	const BitSource alpha{alpha_bits, 8U, 1U, 1U};
	expect(
		raster.blit_planar_masked(source, alpha, {0, 30}),
		"masked planar replace succeeds"
	);
	expect_pixel(raster, Page::zero, {0, 30}, 1U, "mask replaces all planes at x0");
	expect_pixel(raster, Page::zero, {1, 30}, 2U, "mask replaces all planes at x1");
	expect_pixel(raster, Page::zero, {2, 30}, 15U, "zero mask preserves destination");

	expect(raster.clear({0, 31, 8U, 1U}, 1U), "seed planar OR row");
	expect(
		raster.blit_planar(source, {0, 31}, PlanarWriteMode::or_planes),
		"planar OR succeeds"
	);
	expect_pixel(raster, Page::zero, {0, 31}, 1U, "blue OR remains blue");
	expect_pixel(raster, Page::zero, {1, 31}, 3U, "red OR blue produces magenta");
	expect_pixel(raster, Page::zero, {2, 31}, 1U, "zero source leaves OR destination");

	const auto before_bad_alpha = raster.display_state_hash();
	const BitSource short_alpha{{}, 8U, 1U, 1U};
	expect(
		!raster.blit_planar_masked(source, short_alpha, {0, 31}),
		"short alpha plane rejected"
	);
	expect(
		raster.display_state_hash() == before_bad_alpha,
		"invalid alpha is atomic"
	);
}

void test_grcg_write_modes() {
	Raster raster;
	expect(raster.clear({0, 40, 16U, 2U}, 9U), "seed GRCG destination");
	const std::array<std::uint8_t, 1U> cpu_mask{0xA0U};
	const BitSource cpu{cpu_mask, 8U, 1U, 1U};
	const GrcgTile tile{{0xAAU, 0xCCU, 0xF0U, 0x0FU}};
	expect(
		raster.grcg_write(cpu, {0, 40}, GrcgWriteMode::read_modify_write, tile),
		"GRCG RMW succeeds"
	);
	expect_pixel(raster, Page::zero, {0, 40}, 7U, "RMW selected first tile pixel");
	expect_pixel(raster, Page::zero, {1, 40}, 9U, "RMW zero CPU bit preserves");
	expect_pixel(raster, Page::zero, {2, 40}, 5U, "RMW selected third tile pixel");

	const std::array<std::uint8_t, 1U> ignored_cpu{0x00U};
	const BitSource ignored{ignored_cpu, 8U, 1U, 1U};
	expect(
		raster.grcg_write(ignored, {8, 40}, GrcgWriteMode::tile_direct, tile),
		"GRCG TDW succeeds"
	);
	expect_pixel(raster, Page::zero, {8, 40}, 7U, "TDW ignores first CPU bit");
	expect_pixel(raster, Page::zero, {15, 40}, 8U, "TDW writes final tile bit");

	const auto solid = GrcgTile::solid(10U);
	expect(solid.has_value(), "valid solid GRCG tile");
	expect(
		solid->planes == std::array<std::uint8_t, 4U>{0U, 0xFFU, 0U, 0xFFU},
		"solid tile follows B/R/G/E color bits"
	);
	expect(!GrcgTile::solid(16U).has_value(), "invalid solid color rejected");

	const auto before_unaligned = raster.display_state_hash();
	expect(
		!raster.grcg_write(cpu, {1, 40}, GrcgWriteMode::tile_direct, tile),
		"unaligned GRCG destination rejected"
	);
	expect(
		raster.display_state_hash() == before_unaligned,
		"invalid GRCG operation is atomic"
	);
}

void test_tram_and_gaiji_boundary() {
	Tram tram;
	expect(
		th03::present::gaiji_cell_code(0x00U, GaijiHalf::left) == 0x0056U &&
		th03::present::gaiji_cell_code(0x7FU, GaijiHalf::right) == 0xFF56U &&
		th03::present::gaiji_cell_code(0x80U, GaijiHalf::left) == 0x0057U &&
		th03::present::gaiji_cell_code(0xFFU, GaijiHalf::right) == 0xFF57U,
		"gaiji code mapping covers both 128-glyph banks and halves"
	);
	tram.fill({' ', static_cast<th03::present::TramAttribute>(
		th03::present::tram_black | th03::present::tram_reverse
	)});
	expect_cell(tram, {0, 0}, {' ', 0x0005U}, "TRAM fill writes raw code and attribute");
	expect(
		tram.fill_attributes({2, 3, 2U, 2U}, th03::present::tram_white),
		"TRAM attribute rectangle succeeds"
	);
	expect_cell(tram, {2, 3}, {' ', th03::present::tram_white}, "attribute fill keeps code");
	expect_cell(tram, {3, 4}, {' ', th03::present::tram_white}, "attribute fill extent");
	expect_cell(tram, {4, 4}, {' ', 0x0005U}, "attribute fill excludes right edge");

	const auto before_bad_rect = tram.visible_state_hash();
	expect(
		!tram.fill_attributes({79, 24, 2U, 1U}, th03::present::tram_red),
		"out-of-range TRAM rectangle rejected"
	);
	expect(
		tram.visible_state_hash() == before_bad_rect,
		"invalid TRAM rectangle is atomic"
	);

	std::array<std::uint8_t, th03::present::gaiji_glyph_bytes> glyph{};
	for(std::size_t row = 0U; row < th03::present::gaiji_height; ++row) {
		glyph[(row * 2U) + 0U] = static_cast<std::uint8_t>(0x80U >> (row & 7U));
		glyph[(row * 2U) + 1U] = static_cast<std::uint8_t>(0x01U << (row & 7U));
	}
	const auto before_unused_glyph = tram.visible_state_hash();
	expect(tram.load_gaiji(0x80U, glyph), "load exact 32-byte gaiji");
	expect(
		tram.visible_state_hash() == before_unused_glyph,
		"unreferenced gaiji is absent from visible hash"
	);
	expect(
		!tram.load_gaiji(0x80U, std::span<const std::uint8_t>{glyph}.first(31U)),
		"short gaiji rejected"
	);
	expect(
		tram.put_gaiji({10, 5}, 0x80U, th03::present::tram_green),
		"put gaiji pair succeeds"
	);
	expect_cell(tram, {10, 5}, {0x0057U, th03::present::tram_green}, "gaiji left JIS code");
	expect_cell(tram, {11, 5}, {0x8057U, th03::present::tram_green}, "gaiji right JIS code");
	expect(
		th03::present::decode_gaiji_cell(0x0057U) ==
			th03::present::GaijiCellReference{0x80U, GaijiHalf::left},
		"decode left gaiji cell"
	);
	expect(
		th03::present::decode_gaiji_cell(0x8057U) ==
			th03::present::GaijiCellReference{0x80U, GaijiHalf::right},
		"decode right gaiji cell"
	);
	expect(
		!th03::present::decode_gaiji_cell(' ').has_value(),
		"ANK cell is not classified as gaiji"
	);
	expect(tram.gaiji_row_half(0x80U, 0U, GaijiHalf::left) == 0x80U, "gaiji left row");
	expect(tram.gaiji_row_half(0x80U, 0U, GaijiHalf::right) == 0x01U, "gaiji right row");
	expect(
		!tram.gaiji_row_half(0x80U, 16U, GaijiHalf::left).has_value(),
		"gaiji row bounds"
	);
	expect(
		tram.visible_state_hash() != before_unused_glyph,
		"referenced gaiji contents join visible TRAM hash"
	);
	const auto before_referenced_glyph_change = tram.visible_state_hash();
	glyph[0U] ^= 0x01U;
	expect(tram.load_gaiji(0x80U, glyph), "replace referenced gaiji contents");
	expect(
		tram.visible_state_hash() != before_referenced_glyph_change,
		"referenced gaiji byte participates in visible hash"
	);

	const auto before_bad_gaiji = tram.visible_state_hash();
	expect(
		!tram.put_gaiji({79, 5}, 1U, th03::present::tram_white),
		"gaiji pair cannot partially cross TRAM edge"
	);
	expect(
		tram.visible_state_hash() == before_bad_gaiji,
		"invalid gaiji pair is atomic"
	);
	tram.set_visible(false);
	expect(!tram.visible(), "TRAM visibility retained");
	expect(
		tram.visible_state_hash() != before_bad_gaiji,
		"TRAM visibility participates in visible hash"
	);

	Raster raster;
	const auto graphics_hash = raster.display_state_hash();
	expect(tram.put_cell({0, 0}, {'X', th03::present::tram_red}), "mutate TRAM only");
	expect(
		raster.display_state_hash() == graphics_hash,
		"TRAM remains separate from graphics-layer hash"
	);
}

th03::assets::BfntSheet make_character_shot_sheet(std::uint8_t color_base) {
	th03::assets::BfntSheet sheet{};
	sheet.header.pixel_width = 32U;
	sheet.header.pixel_height = 32U;
	sheet.header.first_code = 0U;
	sheet.header.last_code = 8U;
	sheet.consumer = th03::assets::BfntConsumerPolicy::superimpose;
	sheet.transparent_color = 0U;
	constexpr std::size_t plane_size = (32U / 8U) * 32U;
	for(std::uint16_t code = 0U; code < 9U; ++code) {
		th03::assets::PlanarBitmap4 pixels{};
		for(auto &plane : pixels.color_planes) {
			plane.assign(plane_size, 0U);
		}
		if(code == 0U || code == 3U || code == 6U) {
			const auto color = static_cast<std::uint8_t>(
				color_base + (code / 3U)
			);
			for(std::size_t plane = 0U; plane < pixels.color_planes.size(); ++plane) {
				if((color & static_cast<std::uint8_t>(1U << plane)) != 0U) {
					pixels.color_planes[plane][0U] = 0x80U;
				}
			}
		}
		sheet.glyphs.push_back({code, std::move(pixels)});
	}
	return sheet;
}

void test_main_static_hud() {
	Tram tram;
	tram.fill({'X', th03::present::tram_red});
	th03::present::main_hud_wipe(tram);
	expect_cell(
		tram,
		{0, 0},
		{' ', 0x0005U},
		"HUD wipe fills non-playfield TRAM black/reverse"
	);
	expect_cell(
		tram,
		{2, 1},
		{' ', th03::present::tram_white},
		"HUD wipe gives P1 rectangle white attributes"
	);
	expect_cell(
		tram,
		{37, 24},
		{' ', th03::present::tram_white},
		"HUD wipe includes P1 gauge row and right edge"
	);
	expect_cell(
		tram,
		{42, 1},
		{' ', th03::present::tram_white},
		"HUD wipe gives P2 rectangle white attributes"
	);
	expect_cell(
		tram,
		{78, 24},
		{' ', 0x0005U},
		"HUD wipe excludes cell after P2 playfield"
	);

	MainHudState state{};
	state.game_mode = MainGameMode::story;
	state.story_lives = 3U;
	state.players[0] = {7U, 2U, 1U, 4U};
	state.players[1] = {10U, 1U, 2U, 16U};
	state.boss_level = 16U;
	expect(
		th03::present::main_hud_static_put(tram, state),
		"static MAIN HUD accepts source-valid state"
	);
	expect_gaiji(
		tram,
		{15, 1},
		th03::present::gaiji_heart_full,
		th03::present::tram_white,
		"P1 first full heart"
	);
	expect_gaiji(
		tram,
		{21, 1},
		th03::present::gaiji_heart_half,
		th03::present::tram_white,
		"P1 odd halfheart"
	);
	expect_gaiji(
		tram,
		{23, 1},
		th03::present::gaiji_heart_empty,
		th03::present::tram_white,
		"P1 empty heart remainder"
	);
	expect_gaiji(
		tram,
		{36, 23},
		th03::present::gaiji_bomb,
		th03::present::tram_white,
		"P1 bombs begin at right and move left"
	);
	expect_gaiji(
		tram,
		{32, 23},
		th03::present::gaiji_space,
		th03::present::tram_white,
		"P1 bomb loop writes trailing gaiji blank"
	);
	expect_gaiji(
		tram,
		{42, 23},
		th03::present::gaiji_bomb,
		th03::present::tram_white,
		"P2 bombs begin at left and move right"
	);
	expect_gaiji(
		tram,
		{44, 23},
		th03::present::gaiji_space,
		th03::present::tram_white,
		"P2 bomb loop writes trailing gaiji blank"
	);
	expect_gaiji(
		tram,
		{2, 1},
		th03::present::gaiji_thalia,
		th03::present::tram_white,
		"P1 round marker"
	);
	expect_gaiji(
		tram,
		{44, 1},
		th03::present::gaiji_thalia,
		th03::present::tram_white,
		"P2 second round marker"
	);
	expect_gaiji(
		tram,
		{36, 2},
		th03::present::gaiji_yinyang,
		th03::present::tram_white,
		"story lives begin at P1 right"
	);
	expect_gaiji(
		tram,
		{32, 2},
		th03::present::gaiji_yinyang,
		th03::present::tram_white,
		"story lives move left"
	);
	expect_gaiji(
		tram,
		{2, 24},
		0x23U,
		th03::present::tram_green,
		"P1 gauge level uses proportional digit"
	);
	expect_gaiji(
		tram,
		{36, 24},
		0x2FU,
		th03::present::tram_red,
		"capped boss level selects digit 16"
	);
	expect_gaiji(
		tram,
		{42, 24},
		0x2FU,
		th03::present::tram_green,
		"P2 level 16 at left gauge edge"
	);
	expect_gaiji(
		tram,
		{76, 24},
		0x2FU,
		th03::present::tram_red,
		"P2 boss level at right gauge edge"
	);

	Tram invalid_tram;
	th03::present::main_hud_wipe(invalid_tram);
	const auto before_invalid = invalid_tram.visible_state_hash();
	state.players[0].halfhearts = 11U;
	expect(
		!th03::present::main_hud_static_put(invalid_tram, state),
		"invalid halfheart count rejected"
	);
	expect(
		invalid_tram.visible_state_hash() == before_invalid,
		"invalid HUD state performs no partial write"
	);
}

void test_main_shot_sheet_and_startup() {
	auto player_one = make_character_shot_sheet(1U);
	auto player_two = make_character_shot_sheet(4U);
	const MainPlayerShotSheets sheets{&player_one, &player_two};
	Raster sheet_raster;
	for(const auto page : {Page::zero, Page::one}) {
		expect(sheet_raster.set_access_page(page), "select shot-sheet seed page");
		expect(
			sheet_raster.clear({544, 328, 96U, 64U}, 14U),
			"seed shot-sheet transparent background"
		);
	}
	expect(
		th03::present::main_prepare_shot_source_sheet(sheet_raster, sheets),
		"prepare exact sub_E266 source sheet"
	);
	expect(sheet_raster.access_page() == Page::zero, "sub_E266 ends on page zero");
	for(const auto page : {Page::zero, Page::one}) {
		expect_pixel(sheet_raster, page, {544, 328}, 1U, "P1 shot pattern zero");
		expect_pixel(sheet_raster, page, {576, 328}, 2U, "P1 shot pattern three");
		expect_pixel(sheet_raster, page, {608, 328}, 3U, "P1 shot pattern six");
		expect_pixel(sheet_raster, page, {544, 360}, 4U, "P2 shot pattern zero");
		expect_pixel(sheet_raster, page, {576, 360}, 5U, "P2 shot pattern three");
		expect_pixel(sheet_raster, page, {608, 360}, 6U, "P2 shot pattern six");
		expect_pixel(
			sheet_raster,
			page,
			{545, 328},
			14U,
			"BFNT transparent color preserves destination"
		);
	}

	auto invalid_sheet = player_one;
	invalid_sheet.header.pixel_width = 16U;
	const auto before_invalid = sheet_raster.display_state_hash();
	expect(
		!th03::present::main_prepare_shot_source_sheet(
			sheet_raster,
			{&invalid_sheet, &player_two}
		),
		"wrong character-sheet geometry rejected"
	);
	expect(
		sheet_raster.display_state_hash() == before_invalid,
		"invalid character sheet leaves display state unchanged"
	);

	Raster raster;
	expect(raster.set_access_page(Page::zero), "select startup page zero seed");
	expect(raster.plot({100, 100}, 2U), "seed startup page zero");
	expect(raster.set_access_page(Page::one), "select startup page one seed");
	expect(raster.plot({100, 100}, 7U), "seed MAINL handoff page one");
	expect(
		raster.set_pending_palette_entry(3U, {1U, 2U, 3U}),
		"seed applied handoff palette"
	);
	raster.apply_pending_palette();
	expect(
		raster.set_pending_palette_entry(3U, {9U, 8U, 7U}),
		"seed unrelated pending palette"
	);
	Tram tram;
	MainHudState hud{};
	hud.game_mode = MainGameMode::versus;
	hud.players[0] = {10U, 2U, 0U, 1U};
	hud.players[1] = {10U, 2U, 0U, 1U};
	MainStaticPresenter presenter;
	expect(!presenter.started(), "MAIN presenter begins stopped");
	expect(
		presenter.round_startup(raster, tram, hud, sheets),
		"static MAIN startup succeeds"
	);
	expect(presenter.started(), "MAIN presenter records startup");
	expect(
		presenter.page_roles() == th03::present::MainPageRoles{Page::zero, Page::one},
		"startup establishes source page roles"
	);
	expect(raster.access_page() == Page::zero, "startup accesses page zero");
	expect(raster.shown_page() == Page::one, "startup shows page one");
	expect(
		raster.raster_mode() == RasterMode::zoomed_200_upper,
		"startup selects graph_200line upper-half mode"
	);
	expect_pixel(raster, Page::zero, {100, 100}, 7U, "page 1 copied to page 0");
	expect_pixel(raster, Page::one, {100, 100}, 7U, "handoff page 1 preserved");
	expect(
		raster.applied_palette().colors[3U] == Rgb4{1U, 2U, 3U} &&
		raster.pending_palette().colors[3U] == Rgb4{1U, 2U, 3U},
		"startup restores and applies handed-off palette"
	);
	expect_gaiji(
		tram,
		{15, 1},
		th03::present::gaiji_heart_full,
		th03::present::tram_white,
		"startup writes static HUD"
	);

	expect(raster.set_access_page(Page::one), "seed first next-page clear target");
	expect(raster.clear({16, 0, 288U, 185U}, 8U), "seed P1 page one rows");
	expect(raster.clear({336, 0, 288U, 185U}, 9U), "seed P2 page one rows");
	expect(raster.set_access_page(Page::zero), "restore source drawing page zero");
	expect(raster.clear({16, 0, 288U, 184U}, 6U), "seed visible P1 page zero");
	expect(raster.clear({336, 0, 288U, 184U}, 7U), "seed visible P2 page zero");
	expect(presenter.end_frame_swap_and_clear(raster), "first MAIN page swap");
	expect(raster.access_page() == Page::one, "first swap accesses old front");
	expect(raster.shown_page() == Page::zero, "first swap shows old back");
	expect(
		presenter.page_roles() == th03::present::MainPageRoles{Page::one, Page::zero},
		"first swap exchanges source page roles"
	);
	expect_pixel(raster, Page::one, {16, 0}, 0U, "P1 clear begins at x16 y0");
	expect_pixel(raster, Page::one, {303, 183}, 0U, "P1 clear ends x303 y183");
	expect_pixel(raster, Page::one, {15, 100}, 0U, "P1 clear excludes x15");
	expect_pixel(raster, Page::one, {304, 100}, 0U, "P1 clear excludes x304");
	expect_pixel(raster, Page::one, {16, 184}, 8U, "P1 clear excludes y184");
	expect_pixel(raster, Page::one, {336, 0}, 1U, "P2 clear uses color one");
	expect_pixel(raster, Page::one, {623, 183}, 1U, "P2 clear exact far edge");
	expect_pixel(raster, Page::one, {336, 184}, 9U, "P2 clear excludes y184");
	expect_pixel(raster, Page::zero, {16, 100}, 6U, "shown page is not cleared");

	expect(raster.clear({16, 0, 288U, 184U}, 12U), "draw next P1 page");
	expect(raster.clear({336, 0, 288U, 184U}, 13U), "draw next P2 page");
	expect(presenter.end_frame_swap_and_clear(raster), "second MAIN page swap");
	expect(raster.access_page() == Page::zero, "second swap accesses page zero");
	expect(raster.shown_page() == Page::one, "second swap shows page one");
	expect(
		presenter.page_roles() == th03::present::MainPageRoles{Page::zero, Page::one},
		"second swap restores source page roles"
	);
	expect_pixel(raster, Page::zero, {16, 100}, 0U, "second swap clears P1 page zero");
	expect_pixel(raster, Page::zero, {336, 100}, 1U, "second swap clears P2 page zero");
	expect_pixel(raster, Page::one, {16, 100}, 12U, "second shown P1 remains drawn");
	expect_pixel(raster, Page::one, {336, 100}, 13U, "second shown P2 remains drawn");
}

} // namespace

int main() {
	test_selectors_and_clipping();
	test_copy_page_overlap_and_selectors();
	test_transparent_masked_and_malformed_blits();
	test_palette_scroll_and_hash();
	test_deterministic_hash();
	test_planar_order_projection_and_clipping();
	test_planar_write_modes();
	test_grcg_write_modes();
	test_tram_and_gaiji_boundary();
	test_main_static_hud();
	test_main_shot_sheet_and_startup();
	std::fputs("th03-present tests passed\n", stdout);
	return 0;
}
