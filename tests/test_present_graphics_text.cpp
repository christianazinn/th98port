#include "th03/present/graphics_text.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using th03::present::AnkGlyph;
using th03::present::AnkGlyphProvider;
using th03::present::GraphicsTextErrorCode;
using th03::present::GraphicsTextStyle;
using th03::present::GraphicsTextWeight;
using th03::present::JisGlyph;
using th03::present::Page;
using th03::present::Point;
using th03::present::PreparedGraphicsText8x16;
using th03::present::Raster;

int checks = 0;
int failures = 0;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

class RecordingFont final : public AnkGlyphProvider {
public:
	[[nodiscard]] std::optional<AnkGlyph> glyph(
		std::uint8_t
	) const override {
		return std::nullopt;
	}

	[[nodiscard]] std::optional<JisGlyph> jis_glyph(
		std::uint16_t canonical_jis
	) const override {
		queries.push_back(canonical_jis);
		if(missing && *missing == canonical_jis) {
			return std::nullopt;
		}
		return glyph_value;
	}

	JisGlyph glyph_value{};
	std::optional<std::uint16_t> missing{};
	mutable std::vector<std::uint16_t> queries{};
};

[[nodiscard]] JisGlyph glyph_with_first_row(
	std::uint8_t left,
	std::uint8_t right = 0U
) {
	JisGlyph result{};
	result.row_halves[0U] = left;
	result.row_halves[1U] = right;
	return result;
}

[[nodiscard]] std::vector<th03::present::Index4> page_copy(
	const Raster &raster,
	Page page = Page::zero
) {
	const auto source = raster.graphics(page);
	return {source.begin(), source.end()};
}

void expect_pixel(
	const Raster &raster,
	Page page,
	Point point,
	std::uint8_t expected,
	std::string_view message
) {
	expect(raster.pixel(page, point) == expected, message);
}

void test_shift_jis_mapping_and_widths()
{
	RecordingFont font;
	const std::array<std::uint8_t, 9U> source{
		0x85U, 0x40U,
		0x86U, 0x9EU,
		0x86U, 0x9FU,
		'A', 0xA1U, 0x01U,
	};
	const auto prepared = th03::present::prepare_graphics_text_8x16(
		source, font
	);
	expect(prepared.has_value(), "mixed source Shift-JIS preparation succeeds");
	if(!prepared) {
		return;
	}
	const std::array<std::uint16_t, 6U> expected_queries{
		0x2921U, 0x2B7EU, 0x2C21U, 0x2941U, 0x2A21U, 0x2B21U,
	};
	expect(
		font.queries == std::vector<std::uint16_t>(
			expected_queries.begin(), expected_queries.end()
		),
		"special halfwidth, ASCII, kana, and control bytes query exact FONT rows"
	);
	expect(
		prepared->glyph_count() == 6U && prepared->pixel_width() == 56U,
		"8540 and 869E are halfwidth while 869F is fullwidth"
	);

	RecordingFont boundaries;
	const std::array<std::uint8_t, 16U> boundary_source{
		0x81U, 0x40U, 0x81U, 0x7EU, 0x81U, 0x80U, 0x81U, 0x9EU,
		0x81U, 0x9FU, 0x9FU, 0xFCU, 0xE0U, 0x40U, 0xFCU, 0xFCU,
	};
	const auto boundary_text = th03::present::prepare_graphics_text_8x16(
		boundary_source, boundaries
	);
	const std::array<std::uint16_t, 8U> boundary_codes{
		0x2121U, 0x215FU, 0x2160U, 0x217EU,
		0x2221U, 0x5E7EU, 0x5F21U, 0x987EU,
	};
	expect(
		boundary_text.has_value() && boundaries.queries ==
			std::vector<std::uint16_t>(boundary_codes.begin(), boundary_codes.end()),
		"Shift-JIS lead/trail boundaries convert to canonical JIS exactly"
	);
}

void test_malformed_and_missing_preparation()
{
	RecordingFont font;
	const std::array<std::uint8_t, 3U> embedded_nul{'A', 0U, 'B'};
	auto result = th03::present::prepare_graphics_text_8x16(
		embedded_nul, font
	);
	expect(
		!result && result.error().code ==
			GraphicsTextErrorCode::malformed_shift_jis &&
			result.error().byte_offset == 1U && font.queries.size() == 1U,
		"late embedded NUL rejects the complete prepared object"
	);

	font.queries.clear();
	const std::array<std::uint8_t, 1U> dangling{0x81U};
	result = th03::present::prepare_graphics_text_8x16(dangling, font);
	expect(
		!result && result.error().byte_offset == 0U && font.queries.empty(),
		"dangling Shift-JIS lead is rejected before a provider query"
	);

	const std::array<std::uint8_t, 2U> bad_trail{0x81U, 0x7FU};
	result = th03::present::prepare_graphics_text_8x16(bad_trail, font);
	expect(
		!result && result.error().byte_offset == 1U,
		"invalid Shift-JIS trail is rejected"
	);

	font.missing = std::uint16_t{0x2B21U};
	const std::array<std::uint8_t, 1U> control{0x01U};
	result = th03::present::prepare_graphics_text_8x16(control, font);
	expect(
		!result && result.error().code ==
			GraphicsTextErrorCode::missing_jis_glyph &&
			result.error().canonical_jis == 0x2B21U,
		"missing alternate blank glyph reports its exact canonical JIS code"
	);
}

PreparedGraphicsText8x16 prepare_halfwidth(
	JisGlyph glyph,
	std::span<const std::uint8_t> source
) {
	RecordingFont font;
	font.glyph_value = glyph;
	auto prepared = th03::present::prepare_graphics_text_8x16(source, font);
	return std::move(*prepared);
}

void test_weight_table_and_left_truncation()
{
	const std::array<std::uint8_t, 1U> source{'A'};
	const std::array weights{
		GraphicsTextWeight::normal,
		GraphicsTextWeight::heavy,
		GraphicsTextWeight::bold,
		GraphicsTextWeight::black,
	};
	const std::array<std::uint8_t, 4U> expected_masks{
		0xA0U, 0xE0U, 0x60U, 0xE0U,
	};
	for(std::size_t index = 0U; index < weights.size(); ++index) {
		auto prepared = prepare_halfwidth(
			glyph_with_first_row(0xA0U), source
		);
		Raster raster;
		expect(
			th03::present::graphics_text_put_8x16(
				raster,
				{8, 10},
				prepared,
				{12U, weights[index], 0U}
			),
			"each source weight renders"
		);
		expect(
			raster.planar_byte(
				Page::zero, th03::present::PlanarPlane::green, 1U, 10U
			) == expected_masks[index] &&
			raster.planar_byte(
				Page::zero, th03::present::PlanarPlane::intensity, 1U, 10U
			) == expected_masks[index],
			"source uint16 weight transform matches the locked row table"
		);
		expect_pixel(
			raster, Page::zero, {7, 10}, 0U,
			"bold pixels generated left of the uint16 row remain truncated"
		);
	}
}

void test_alignment_spacing_and_access_page()
{
	RecordingFont full_font;
	full_font.glyph_value = glyph_with_first_row(0x80U, 0x01U);
	const std::array<std::uint8_t, 2U> full_source{0x82U, 0xA0U};
	auto full = th03::present::prepare_graphics_text_8x16(
		full_source, full_font
	);
	expect(full.has_value(), "fullwidth alignment fixture prepares");
	if(!full) {
		return;
	}
	Raster unaligned;
	expect(
		unaligned.set_access_page(Page::one) &&
		th03::present::graphics_text_put_8x16(
			unaligned, {7, 20}, *full, {15U, GraphicsTextWeight::normal, 0U}
		),
		"unaligned fullwidth glyph writes its three-byte GRCG mask"
	);
	expect_pixel(unaligned, Page::one, {7, 20}, 15U, "unaligned left bit lands");
	expect_pixel(unaligned, Page::one, {22, 20}, 15U, "unaligned right bit lands");
	expect_pixel(unaligned, Page::one, {6, 20}, 0U, "unaligned mask keeps left neighbor");
	expect_pixel(unaligned, Page::zero, {7, 20}, 0U, "draw changes access page only");

	RecordingFont font;
	font.glyph_value = glyph_with_first_row(0x80U);
	const std::array<std::uint8_t, 2U> two_source{'A', 'B'};
	auto two = th03::present::prepare_graphics_text_8x16(two_source, font);
	expect(two.has_value(), "spacing fixture prepares");
	if(!two) {
		return;
	}
	Raster spaced;
	expect(
		th03::present::graphics_text_put_8x16(
			spaced, {10, 0}, *two, {9U, GraphicsTextWeight::normal, 7U}
		),
		"maximum source spacing renders"
	);
	expect_pixel(spaced, Page::zero, {10, 0}, 9U, "first spaced glyph position");
	expect_pixel(spaced, Page::zero, {25, 0}, 9U, "second spaced glyph position");
	expect_pixel(spaced, Page::zero, {18, 0}, 0U, "spacing gap remains untouched");

	const std::array<std::uint8_t, 1U> one_source{'A'};
	auto one = th03::present::prepare_graphics_text_8x16(one_source, font);
	Raster edge;
	expect(
		one && th03::present::graphics_text_put_8x16(
			edge, {632, 0}, *one, {4U, GraphicsTextWeight::normal, 7U}
		),
		"final glyph ink may fit when its unused trailing spacing passes x=640"
	);
	expect_pixel(edge, Page::zero, {632, 0}, 4U, "right-edge glyph still renders");
}

void test_halfwidth_selects_only_the_font_rows_left_byte()
{
	RecordingFont font;
	font.glyph_value = glyph_with_first_row(0x00U, 0x80U);
	const std::array<std::uint8_t, 1U> half_source{'A'};
	auto half = th03::present::prepare_graphics_text_8x16(half_source, font);
	Raster half_raster;
	expect(
		half && th03::present::graphics_text_put_8x16(
			half_raster,
			{0, 0},
			*half,
			{15U, GraphicsTextWeight::normal, 0U}
		),
		"alternate-row halfwidth glyph renders"
	);
	expect(
		std::all_of(
			half_raster.graphics(Page::zero).begin(),
			half_raster.graphics(Page::zero).end(),
			[](std::uint8_t pixel) { return pixel == 0U; }
		),
		"halfwidth glyph discards the FONT row's right byte"
	);

	font.queries.clear();
	const std::array<std::uint8_t, 2U> full_source{0x82U, 0xA0U};
	auto full = th03::present::prepare_graphics_text_8x16(full_source, font);
	Raster full_raster;
	expect(
		full && th03::present::graphics_text_put_8x16(
			full_raster,
			{0, 0},
			*full,
			{15U, GraphicsTextWeight::normal, 0U}
		),
		"ordinary fullwidth glyph renders both FONT row bytes"
	);
	expect_pixel(
		full_raster, Page::zero, {8, 0}, 15U,
		"fullwidth glyph retains the FONT row's right byte"
	);
}

void expect_atomic_failure(
	const PreparedGraphicsText8x16 &text,
	Point position,
	GraphicsTextStyle style,
	std::string_view message
) {
	Raster raster;
	expect(raster.clear({0, 0, 640U, 400U}, 3U), "seed atomic page");
	const auto before = page_copy(raster);
	expect(
		!th03::present::graphics_text_put_8x16(
			raster, position, text, style
		) && page_copy(raster) == before,
		message
	);
}

void test_invalid_and_late_requests_are_atomic()
{
	RecordingFont font;
	font.glyph_value = glyph_with_first_row(0x80U);
	const std::array<std::uint8_t, 2U> source{'A', 'B'};
	auto prepared = th03::present::prepare_graphics_text_8x16(source, font);
	expect(prepared.has_value(), "atomic fixture prepares");
	if(!prepared) {
		return;
	}
	expect_atomic_failure(
		*prepared, {0, 0}, {16U, GraphicsTextWeight::normal, 0U},
		"invalid graphics color is atomic"
	);
	expect_atomic_failure(
		*prepared,
		{0, 0},
		{1U, static_cast<GraphicsTextWeight>(0xFFU), 0U},
		"invalid weight enum is atomic"
	);
	expect_atomic_failure(
		*prepared, {0, 0}, {1U, GraphicsTextWeight::normal, 8U},
		"spacing outside 0..7 is atomic"
	);
	expect_atomic_failure(
		*prepared, {-1, 0}, {1U, GraphicsTextWeight::normal, 0U},
		"negative origin is atomic"
	);
	expect_atomic_failure(
		*prepared, {0, 385}, {1U, GraphicsTextWeight::normal, 0U},
		"bottom-overflowing glyph is atomic"
	);
	expect_atomic_failure(
		*prepared, {625, 0}, {1U, GraphicsTextWeight::normal, 0U},
		"valid early glyph plus right-overflowing late glyph is atomic"
	);
}

void test_prepared_text_owns_font_rows()
{
	const std::array<std::uint8_t, 1U> source{'A'};
	auto prepared = [&]() {
		RecordingFont temporary;
		temporary.glyph_value = glyph_with_first_row(0x80U);
		return th03::present::prepare_graphics_text_8x16(source, temporary);
	}();
	Raster raster;
	expect(
		prepared && th03::present::graphics_text_put_8x16(
			raster,
			{0, 0},
			*prepared,
			{2U, GraphicsTextWeight::normal, 0U}
		),
		"prepared rows remain usable after the provider lifetime ends"
	);
	expect_pixel(raster, Page::zero, {0, 0}, 2U, "owned prepared row renders");
}

void test_scaled_text_is_integer_exact_and_atomic()
{
	RecordingFont font;
	font.glyph_value = glyph_with_first_row(0xA0U);
	const std::array<std::uint8_t, 2U> source{'A', 'B'};
	auto prepared = th03::present::prepare_graphics_text_8x16(source, font);
	expect(prepared.has_value(), "scaled fixture prepares");
	if(!prepared) {
		return;
	}
	Raster raster;
	expect(
		th03::present::graphics_text_put_scaled_8x16(
			raster, {10, 20}, *prepared,
			{6U, GraphicsTextWeight::normal, 1U}, 2U
		),
		"two-times graphics text renders"
	);
	for(std::int32_t y = 20; y < 22; ++y) {
		for(std::int32_t x = 10; x < 12; ++x) {
			expect_pixel(
				raster, Page::zero, {x, y}, 6U,
				"one source bit expands to a two-by-two block"
			);
		}
	}
	expect_pixel(
		raster, Page::zero, {12, 20}, 0U,
		"scaled zero bit remains transparent"
	);
	expect_pixel(
		raster, Page::zero, {28, 20}, 6U,
		"scaled spacing advances the second glyph"
	);

	Raster invalid;
	expect(invalid.clear({0, 0, 640U, 400U}, 3U), "seed scaled atomic page");
	const auto before = page_copy(invalid);
	expect(
		!th03::present::graphics_text_put_scaled_8x16(
			invalid, {610, 0}, *prepared,
			{6U, GraphicsTextWeight::normal, 0U}, 2U
		) && page_copy(invalid) == before,
		"late scaled overflow is rejected atomically"
	);
	expect(
		!th03::present::graphics_text_put_scaled_8x16(
			invalid, {0, 0}, *prepared,
			{6U, GraphicsTextWeight::normal, 0U}, 0U
		) && page_copy(invalid) == before,
		"zero scale is rejected atomically"
	);
}

} // namespace

int main()
{
	test_shift_jis_mapping_and_widths();
	test_malformed_and_missing_preparation();
	test_weight_table_and_left_truncation();
	test_alignment_spacing_and_access_page();
	test_halfwidth_selects_only_the_font_rows_left_byte();
	test_invalid_and_late_requests_are_atomic();
	test_prepared_text_owns_font_rows();
	test_scaled_text_is_integer_exact_and_atomic();

	if(failures != 0) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << checks << " checks passed\n";
	return 0;
}
