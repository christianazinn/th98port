#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>
#include <vector>

#include "th03/present/text.hpp"

namespace {

using th03::present::AnkGlyph;
using th03::present::AnkGlyphProvider;
using th03::present::ComposedPixel;
using th03::present::ComposedPixelLayer;
using th03::present::ComposedTextFrame;
using th03::present::Index4;
using th03::present::TextColor;
using th03::present::TextComposeErrorCode;
using th03::present::Tram;
using th03::present::TramCell;
using th03::present::TramPoint;

void expect(bool condition, std::string_view message) {
	if(!condition) {
		std::fputs("FAIL: ", stderr);
		std::fwrite(message.data(), 1U, message.size(), stderr);
		std::fputc('\n', stderr);
		std::exit(1);
	}
}

class SyntheticAnk final : public AnkGlyphProvider {
public:
	void set(std::uint8_t code, AnkGlyph glyph) {
		glyphs_[code] = glyph;
	}

	[[nodiscard]] std::optional<AnkGlyph> glyph(
		std::uint8_t code
	) const override {
		return glyphs_[code];
	}

private:
	std::array<std::optional<AnkGlyph>, 256U> glyphs_{};
};

[[nodiscard]] std::size_t pixel_offset(std::size_t x, std::size_t y) {
	return (y * th03::present::graphics_width) + x;
}

[[nodiscard]] ComposedPixel pixel(
	const ComposedTextFrame &frame,
	std::size_t x,
	std::size_t y
) {
	return frame[pixel_offset(x, y)];
}

[[nodiscard]] ComposedPixel graphics_pixel(Index4 index) {
	return {ComposedPixelLayer::graphics, index};
}

[[nodiscard]] ComposedPixel text_pixel(TextColor color) {
	return {
		ComposedPixelLayer::text,
		static_cast<std::uint8_t>(color),
	};
}

[[nodiscard]] std::vector<Index4> graphics(Index4 color) {
	return std::vector<Index4>(th03::present::graphics_page_pixels, color);
}

void test_ank_text_over_graphics() {
	SyntheticAnk ank;
	AnkGlyph a{};
	a.rows[0U] = 0xA0U;
	a.rows[1U] = 0x40U;
	ank.set(static_cast<std::uint8_t>('A'), a);
	AnkGlyph v{};
	v.rows[0U] = 0x80U;
	ank.set(static_cast<std::uint8_t>('V'), v);

	Tram tram;
	expect(
		tram.put_cell({2, 3}, {
			static_cast<th03::present::TramCode>('A'),
			th03::present::tram_cyan,
		}),
		"seed ANK cell"
	);
	expect(
		tram.put_cell({4, 3}, {
			static_cast<th03::present::TramCode>('V'),
			th03::present::tram_red,
		}),
		"seed ANK code that aliases a gaiji left code"
	);
	const auto result = th03::present::compose_text_layer(
		graphics(9U),
		tram,
		ank
	);
	expect(result.has_value(), "ANK composition succeeds");
	const auto &frame = *result;
	expect(
		pixel(frame, 16U, 48U) == text_pixel(TextColor::cyan),
		"ANK bit 7 is the leftmost text pixel"
	);
	expect(
		pixel(frame, 17U, 48U) == graphics_pixel(9U),
		"clear ANK bit preserves graphics"
	);
	expect(
		pixel(frame, 18U, 48U) == text_pixel(TextColor::cyan),
		"ANK row retains source bit order"
	);
	expect(
		pixel(frame, 17U, 49U) == text_pixel(TextColor::cyan),
		"ANK rows map to consecutive display rows"
	);
	expect(
		pixel(frame, 0U, 0U) == graphics_pixel(9U),
		"uncovered graphics remain in their own palette domain"
	);
	expect(
		pixel(frame, 32U, 48U) == text_pixel(TextColor::red),
		"unpaired raw 0056 is ANK V rather than gaiji 0"
	);
}

void test_reverse_black_mask() {
	SyntheticAnk ank;
	AnkGlyph space{};
	space.rows[0U] = 0x80U;
	ank.set(static_cast<std::uint8_t>(' '), space);

	Tram tram;
	expect(
		tram.put_cell({0, 0}, {
			static_cast<th03::present::TramCode>(' '),
			static_cast<th03::present::TramAttribute>(
				th03::present::tram_black | th03::present::tram_reverse
			),
		}),
		"seed reverse-black cell"
	);
	const auto result = th03::present::compose_text_layer(
		graphics(12U),
		tram,
		ank
	);
	expect(result.has_value(), "reverse composition succeeds");
	expect(
		pixel(*result, 0U, 0U) == graphics_pixel(12U),
		"reverse turns a set glyph bit into a graphics opening"
	);
	expect(
		pixel(*result, 1U, 0U) == text_pixel(TextColor::black),
		"reverse turns a clear glyph bit into opaque text black"
	);
	expect(
		pixel(*result, 7U, 15U) == text_pixel(TextColor::black),
		"reverse-black blank area masks graphics through the full cell"
	);
}

void test_gaiji_halves_and_color() {
	SyntheticAnk ank;
	Tram tram;
	std::array<std::uint8_t, th03::present::gaiji_glyph_bytes> rows{};
	rows[0U] = 0x81U;
	rows[1U] = 0x42U;
	rows[30U] = 0x18U;
	rows[31U] = 0x24U;
	expect(tram.load_gaiji(0xA5U, rows), "load synthetic gaiji");
	expect(
		tram.put_gaiji({10, 4}, 0xA5U, th03::present::tram_yellow),
		"seed two-cell gaiji"
	);
	const auto result = th03::present::compose_text_layer(
		graphics(3U),
		tram,
		ank
	);
	expect(result.has_value(), "gaiji composition succeeds without ANK lookup");
	expect(
		pixel(*result, 80U, 64U) == text_pixel(TextColor::yellow) &&
		pixel(*result, 87U, 64U) == text_pixel(TextColor::yellow),
		"gaiji left half uses its source row byte"
	);
	expect(
		pixel(*result, 89U, 64U) == text_pixel(TextColor::yellow) &&
		pixel(*result, 94U, 64U) == text_pixel(TextColor::yellow),
		"gaiji right half uses its source row byte"
	);
	expect(
		pixel(*result, 88U, 64U) == graphics_pixel(3U),
		"clear gaiji bit leaves underlying graphics"
	);
	expect(
		pixel(*result, 83U, 79U) == text_pixel(TextColor::yellow) &&
		pixel(*result, 90U, 79U) == text_pixel(TextColor::yellow),
		"gaiji row 15 maps to the final cell row"
	);

	Tram ambiguous;
	std::array<std::uint8_t, th03::present::gaiji_glyph_bytes> gaiji_zero{};
	gaiji_zero[0U] = 0x80U;
	gaiji_zero[1U] = 0x40U;
	expect(ambiguous.load_gaiji(0U, gaiji_zero), "load gaiji zero fixture");
	expect(
		ambiguous.put_gaiji({0, 0}, 0U, th03::present::tram_green),
		"seed gaiji zero pair"
	);
	const auto ambiguous_result = th03::present::compose_text_layer(
		graphics(2U),
		ambiguous,
		ank
	);
	expect(ambiguous_result.has_value(), "paired gaiji zero composition succeeds");
	expect(
		pixel(*ambiguous_result, 0U, 0U) == text_pixel(TextColor::green) &&
		pixel(*ambiguous_result, 9U, 0U) == text_pixel(TextColor::green),
		"ordered 0056/8056 pair resolves as gaiji zero"
	);
}

void test_visibility_and_disabled_cells() {
	SyntheticAnk ank;
	Tram tram;
	expect(
		tram.put_cell({0, 0}, {0x1234U, 0U}),
		"seed disabled unsupported code"
	);
	const auto disabled = th03::present::compose_text_layer(
		graphics(6U),
		tram,
		ank
	);
	expect(
		disabled.has_value() && pixel(*disabled, 0U, 0U) == graphics_pixel(6U),
		"attribute display bit suppresses glyph lookup and composition"
	);

	expect(
		tram.put_cell({0, 0}, {0x1234U, th03::present::tram_white}),
		"enable unsupported code"
	);
	tram.set_visible(false);
	const auto hidden = th03::present::compose_text_layer(
		graphics(7U),
		tram,
		ank
	);
	expect(
		hidden.has_value() && pixel(*hidden, 0U, 0U) == graphics_pixel(7U),
		"hidden TRAM bypasses all glyph lookup"
	);
}

void expect_cell_error(
	const std::expected<ComposedTextFrame, th03::present::TextComposeError> &result,
	TextComposeErrorCode code,
	TramPoint point,
	std::string_view message
) {
	expect(!result.has_value(), message);
	expect(result.error().code == code, message);
	expect(
		result.error().cell.x == point.x && result.error().cell.y == point.y,
		message
	);
}

void test_explicit_deferred_paths() {
	SyntheticAnk ank;
	Tram tram;
	expect(
		tram.put_cell({4, 5}, {static_cast<std::uint16_t>('Z'), th03::present::tram_red}),
		"seed missing ANK code"
	);
	expect_cell_error(
		th03::present::compose_text_layer(graphics(0U), tram, ank),
		TextComposeErrorCode::missing_ank_glyph,
		{4, 5},
		"missing injected ANK bitmap is explicit"
	);

	tram.fill({});
	expect(
		tram.put_cell({6, 7}, {0x2204U, th03::present::tram_white}),
		"seed JIS cell"
	);
	expect_cell_error(
		th03::present::compose_text_layer(graphics(0U), tram, ank),
		TextComposeErrorCode::jis_glyph_not_supported,
		{6, 7},
		"JIS font-ROM path is explicitly deferred"
	);

	tram.fill({});
	expect(
		tram.put_cell({8, 9}, {
			static_cast<std::uint16_t>('B'),
			static_cast<std::uint16_t>(
				th03::present::tram_white | th03::present::tram_blink
			),
		}),
		"seed blink cell"
	);
	expect_cell_error(
		th03::present::compose_text_layer(graphics(0U), tram, ank),
		TextComposeErrorCode::blink_not_supported,
		{8, 9},
		"hardware blink cadence is explicitly deferred"
	);

	tram.fill({});
	expect(
		tram.put_cell({10, 11}, {
			static_cast<std::uint16_t>('C'),
			static_cast<std::uint16_t>(
				th03::present::tram_white | th03::present::tram_underline
			),
		}),
		"seed underline cell"
	);
	expect_cell_error(
		th03::present::compose_text_layer(graphics(0U), tram, ank),
		TextComposeErrorCode::underline_not_supported,
		{10, 11},
		"hardware underline rasterization is explicitly deferred"
	);

	tram.fill({});
	expect(
		tram.put_cell({12, 13}, {
			static_cast<std::uint16_t>('D'),
			static_cast<std::uint16_t>(th03::present::tram_white | 0x0010U),
		}),
		"seed unsupported attribute cell"
	);
	expect_cell_error(
		th03::present::compose_text_layer(graphics(0U), tram, ank),
		TextComposeErrorCode::attribute_not_supported,
		{12, 13},
		"source-unused attribute bit is not guessed"
	);
}

void test_graphics_validation() {
	SyntheticAnk ank;
	Tram tram;
	auto short_graphics = graphics(0U);
	short_graphics.pop_back();
	const auto short_result = th03::present::compose_text_layer(
		short_graphics,
		tram,
		ank
	);
	expect(
		!short_result && short_result.error().code ==
			TextComposeErrorCode::graphics_size_mismatch,
		"composition rejects a non-640x400 graphics input"
	);

	auto invalid_graphics = graphics(0U);
	invalid_graphics[123U] = 16U;
	const auto invalid_result = th03::present::compose_text_layer(
		invalid_graphics,
		tram,
		ank
	);
	expect(
		!invalid_result && invalid_result.error().code ==
			TextComposeErrorCode::graphics_index_out_of_range,
		"composition rejects an invalid graphics palette index"
	);
}

} // namespace

int main() {
	test_ank_text_over_graphics();
	test_reverse_black_mask();
	test_gaiji_halves_and_color();
	test_visibility_and_disabled_cells();
	test_explicit_deferred_paths();
	test_graphics_validation();
	std::fputs("th03-present-text tests passed\n", stdout);
	return 0;
}
