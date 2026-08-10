#include "th03/present/mono.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>

namespace {

using th03::present::Index4;
using th03::present::PackedRowOrder;
using th03::present::Page;
using th03::present::PelletCel;
using th03::present::PelletMasks;
using th03::present::Point;
using th03::present::Raster;
using th03::present::ScoreFontMasks;
using th03::present::StaffFlakeMasks;

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

std::size_t pellet_offset(
	std::size_t cel,
	std::size_t preshift,
	std::size_t row,
	std::size_t byte
) {
	return (((cel * th03::present::pellet_preshift_count) + preshift) *
		th03::present::pellet_variant_bytes) +
		(row * th03::present::pellet_row_bytes) + byte;
}

std::size_t flake_offset(
	std::size_t cel,
	std::size_t row,
	std::size_t byte
) {
	return (cel * th03::present::staff_flake_cel_bytes) +
		(row * th03::present::staff_flake_row_bytes) + byte;
}

void test_score_row_order_and_seven_row_loop() {
	std::array<std::uint8_t, th03::present::score_font_mask_bytes> rows{};
	constexpr std::size_t digit = 3U;
	const auto base = digit * th03::present::score_digit_storage_rows;
	rows[base] = 0x80U;
	rows[base + 6U] = 0x01U;
	rows[base + 7U] = 0xFFU;
	const ScoreFontMasks font{rows, PackedRowOrder::bottom_to_top};

	Raster raster;
	expect(raster.clear({0, 0, 64U, 64U}, 4U), "seed score destination");
	expect(
		th03::present::score_digit_put(
			raster, 11, 20, font, static_cast<std::uint8_t>(digit), 10U
		),
		"score digit put succeeds"
	);
	expect_pixel(raster, Page::zero, {15, 20}, 10U, "score stored row 6 is top");
	expect_pixel(raster, Page::zero, {8, 26}, 10U, "score stored row 0 is bottom");
	expect_pixel(raster, Page::zero, {8, 19}, 4U, "score stored row 7 is unread");
	expect_pixel(raster, Page::zero, {8, 27}, 4U, "score blit has seven rows");
	expect_pixel(raster, Page::zero, {9, 26}, 4U, "score GRCG mask preserves gaps");

	for(std::size_t selected = 0U;
		selected < th03::present::score_digit_count;
		++selected) {
		rows.fill(0U);
		rows[selected * th03::present::score_digit_storage_rows] = 0x80U;
		Raster selection;
		const ScoreFontMasks selected_font{rows, PackedRowOrder::bottom_to_top};
		expect(
			th03::present::score_digit_put(
				selection,
				16,
				8,
				selected_font,
				static_cast<std::uint8_t>(selected),
				3U
			),
			"every score glyph can be selected"
		);
		expect_pixel(
			selection,
			Page::zero,
			{16, 14},
			3U,
			"score glyph indexing uses eight-byte cells"
		);
	}
}

void test_score_access_page() {
	std::array<std::uint8_t, th03::present::score_font_mask_bytes> rows{};
	rows[0] = 0x80U;
	const ScoreFontMasks font{rows, PackedRowOrder::bottom_to_top};
	Raster raster;
	expect(raster.set_access_page(Page::one), "select score access page one");
	expect(
		th03::present::score_digit_put(raster, 16, 4, font, 0U, 9U),
		"score draw on page one succeeds"
	);
	expect_pixel(raster, Page::one, {16, 10}, 9U, "score changes access page");
	expect_pixel(raster, Page::zero, {16, 10}, 0U, "score preserves other page");
}

void test_pellet_all_preshifts_and_cels() {
	std::array<std::uint8_t, th03::present::pellet_mask_bytes> rows{};
	for(std::size_t shift = 0U;
		shift < th03::present::pellet_preshift_count;
		++shift) {
		rows[pellet_offset(0U, shift, 0U, shift / 8U)] =
			static_cast<std::uint8_t>(0x80U >> (shift & 7U));
	}
	rows[pellet_offset(1U, 0U, 0U, 2U)] = 0x80U;
	const PelletMasks masks{rows, PackedRowOrder::top_to_bottom};

	for(std::size_t shift = 0U;
		shift < th03::present::pellet_preshift_count;
		++shift) {
		Raster raster;
		const auto left = static_cast<std::int32_t>(80U + shift);
		expect(
			th03::present::pellet_put(
				raster, left, 10, masks, PelletCel::normal, 13U
			),
			"every pellet preshift can be selected"
		);
		expect_pixel(
			raster,
			Page::zero,
			{left, 10},
			13U,
			"pellet uses left modulo sixteen"
		);
	}

	Raster transfer;
	expect(
		th03::present::pellet_put(
			transfer, 96, 4, masks, PelletCel::transfer, 7U
		),
		"transfer pellet put succeeds"
	);
	expect_pixel(transfer, Page::zero, {112, 4}, 7U, "transfer cel is selected");
	expect_pixel(transfer, Page::zero, {96, 4}, 0U, "normal cel is not selected");
}

void test_pellet_full_narrow_rows_and_access_page() {
	std::array<std::uint8_t, th03::present::pellet_mask_bytes> rows{};
	rows[pellet_offset(0U, 0U, 0U, 0U)] = 0x80U;
	rows[pellet_offset(0U, 0U, 0U, 1U)] = 0x40U;
	rows[pellet_offset(0U, 0U, 0U, 2U)] = 0x20U;
	rows[pellet_offset(0U, 0U, 0U, 3U)] = 0x10U;
	rows[pellet_offset(0U, 0U, 3U, 0U)] = 0x08U;
	const PelletMasks masks{rows, PackedRowOrder::top_to_bottom};

	Raster narrow;
	expect(
		th03::present::pellet_put(
			narrow, 0, 0, masks, PelletCel::normal, 6U
		),
		"pellet narrow put succeeds"
	);
	expect_pixel(narrow, Page::zero, {0, 0}, 6U, "narrow keeps first byte");
	expect_pixel(narrow, Page::zero, {9, 0}, 6U, "narrow keeps second byte");
	expect_pixel(narrow, Page::zero, {18, 0}, 0U, "narrow drops third byte");
	expect_pixel(narrow, Page::zero, {27, 0}, 0U, "narrow drops fourth byte");

	Raster boundary;
	expect(
		th03::present::pellet_put(
			boundary, 48, 0, masks, PelletCel::normal, 5U
		),
		"last narrow VRAM offset succeeds"
	);
	expect_pixel(boundary, Page::zero, {66, 0}, 0U, "offset six remains narrow");

	Raster wide;
	expect(wide.set_access_page(Page::one), "select pellet access page one");
	expect(
		th03::present::pellet_put(
			wide, 64, 0, masks, PelletCel::normal, 11U
		),
		"pellet full put succeeds at offset eight"
	);
	expect_pixel(wide, Page::one, {64, 0}, 11U, "full keeps first byte");
	expect_pixel(wide, Page::one, {73, 0}, 11U, "full keeps second byte");
	expect_pixel(wide, Page::one, {82, 0}, 11U, "full keeps third byte");
	expect_pixel(wide, Page::one, {91, 0}, 11U, "full keeps fourth byte");
	expect_pixel(wide, Page::one, {68, 3}, 11U, "pellet rows remain top-down");
	expect_pixel(wide, Page::zero, {64, 0}, 0U, "pellet preserves other page");
}

void test_flake_rotations_cels_and_planes() {
	std::array<std::uint8_t, th03::present::staff_flake_mask_bytes> rows{};
	rows[flake_offset(0U, 0U, 0U)] = 0x80U;
	for(std::size_t cel = 1U;
		cel < th03::present::staff_flake_cel_count;
		++cel) {
		rows[flake_offset(cel, 0U, 0U)] =
			static_cast<std::uint8_t>(0x80U >> (cel * 2U));
	}
	rows[flake_offset(1U, 7U, 1U)] = 0x01U;
	const StaffFlakeMasks masks{rows, PackedRowOrder::top_to_bottom};

	for(std::size_t shift = 0U; shift < 8U; ++shift) {
		Raster raster;
		expect(raster.clear({80, 20, 16U, 8U}, 14U), "seed flake plane test");
		const auto left = static_cast<std::int32_t>(80U + shift);
		expect(
			th03::present::staff_flake_put_blue_or(
				raster, left, 20, masks, 0U
			),
			"every flake rotation succeeds"
		);
		expect_pixel(
			raster,
			Page::zero,
			{left, 20},
			15U,
			"flake rotates right and ORs blue"
		);
		expect_pixel(
			raster,
			Page::zero,
			{80 + static_cast<std::int32_t>((shift + 1U) & 7U), 20},
			14U,
			"flake preserves unselected blue bits and other planes"
		);
	}

	for(std::size_t cel = 0U;
		cel < th03::present::staff_flake_cel_count;
		++cel) {
		Raster raster;
		expect(
			th03::present::staff_flake_put_blue_or(
				raster, 128, 30, masks, static_cast<std::uint8_t>(cel)
			),
			"every staff flake cel can be selected"
		);
		expect_pixel(
			raster,
			Page::zero,
			{128 + static_cast<std::int32_t>(cel * 2U), 30},
			1U,
			"staff flake cel indexing is source ordered"
		);
	}

	Raster order;
	expect(order.set_access_page(Page::one), "select flake access page one");
	expect(
		th03::present::staff_flake_put_blue_or(order, 128, 40, masks, 1U),
		"staff flake row-order put succeeds"
	);
	expect_pixel(order, Page::one, {130, 40}, 1U, "flake stored row zero is top");
	expect_pixel(order, Page::one, {143, 47}, 1U, "flake stored row seven is bottom");
	expect_pixel(order, Page::zero, {130, 40}, 0U, "flake preserves other page");
}

void test_malformed_models_are_atomic() {
	std::array<std::uint8_t, th03::present::score_font_mask_bytes> score{};
	std::array<std::uint8_t, th03::present::pellet_mask_bytes> pellets{};
	std::array<std::uint8_t, th03::present::staff_flake_mask_bytes> flakes{};
	Raster raster;
	expect(raster.plot({0, 0}, 5U), "seed malformed-model destination");
	const auto before = raster.display_state_hash();

	const ScoreFontMasks short_score{
		std::span<const std::uint8_t>(score).first(score.size() - 1U),
		PackedRowOrder::bottom_to_top,
	};
	const ScoreFontMasks long_score{
		std::span<const std::uint8_t>(pellets).first(score.size() + 1U),
		PackedRowOrder::bottom_to_top,
	};
	const ScoreFontMasks wrong_score_order{score, PackedRowOrder::top_to_bottom};
	expect(
		!th03::present::score_digit_put(raster, 0, 0, short_score, 0U, 1U),
		"score rejects a short model"
	);
	expect(
		!th03::present::score_digit_put(raster, 0, 0, long_score, 0U, 1U),
		"score rejects an oversized model"
	);
	expect(
		!th03::present::score_digit_put(
			raster, 0, 0, wrong_score_order, 0U, 1U
		),
		"score rejects the wrong row order"
	);
	expect(
		!th03::present::score_digit_put(
			raster,
			0,
			0,
			ScoreFontMasks{score, PackedRowOrder::bottom_to_top},
			10U,
			1U
		),
		"score rejects an invalid digit"
	);

	const PelletMasks short_pellets{
		std::span<const std::uint8_t>(pellets).first(pellets.size() - 1U),
		PackedRowOrder::top_to_bottom,
	};
	const PelletMasks wrong_pellet_order{
		pellets,
		PackedRowOrder::bottom_to_top,
	};
	expect(
		!th03::present::pellet_put(
			raster, 0, 0, short_pellets, PelletCel::normal, 1U
		),
		"pellet rejects a short model"
	);
	expect(
		!th03::present::pellet_put(
			raster, 0, 0, wrong_pellet_order, PelletCel::normal, 1U
		),
		"pellet rejects the wrong row order"
	);
	expect(
		!th03::present::pellet_put(
			raster,
			0,
			0,
			PelletMasks{pellets, PackedRowOrder::top_to_bottom},
			static_cast<PelletCel>(0xFFU),
			1U
		),
		"pellet rejects an invalid cel"
	);

	const StaffFlakeMasks short_flakes{
		std::span<const std::uint8_t>(flakes).first(flakes.size() - 1U),
		PackedRowOrder::top_to_bottom,
	};
	const StaffFlakeMasks wrong_flake_order{
		flakes,
		PackedRowOrder::bottom_to_top,
	};
	expect(
		!th03::present::staff_flake_put_blue_or(
			raster, 0, 0, short_flakes, 0U
		),
		"flake rejects a short model"
	);
	expect(
		!th03::present::staff_flake_put_blue_or(
			raster, 0, 0, wrong_flake_order, 0U
		),
		"flake rejects the wrong row order"
	);
	expect(
		!th03::present::staff_flake_put_blue_or(
			raster,
			0,
			0,
			StaffFlakeMasks{flakes, PackedRowOrder::top_to_bottom},
			4U
		),
		"flake rejects an invalid cel"
	);
	expect(
		!th03::present::score_digit_put(
			raster,
			0,
			0,
			ScoreFontMasks{score, PackedRowOrder::bottom_to_top},
			0U,
			16U
		),
		"score rejects an invalid color"
	);
	expect(
		!th03::present::pellet_put(
			raster,
			0,
			0,
			PelletMasks{pellets, PackedRowOrder::top_to_bottom},
			PelletCel::normal,
			16U
		),
		"pellet rejects an invalid color"
	);
	expect(raster.display_state_hash() == before, "invalid models are atomic");
}

} // namespace

int main() {
	test_score_row_order_and_seven_row_loop();
	test_score_access_page();
	test_pellet_all_preshifts_and_cels();
	test_pellet_full_narrow_rows_and_access_page();
	test_flake_rotations_cels_and_planes();
	test_malformed_models_are_atomic();
	if(failures == 0) {
		std::fputs("th03 monochrome presentation tests passed\n", stdout);
	}
	return failures == 0 ? 0 : 1;
}
