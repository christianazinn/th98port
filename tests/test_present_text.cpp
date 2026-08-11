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
using th03::present::Index4;
using th03::present::JisGlyph;
using th03::present::TextColor;
using th03::present::TextComposeErrorCode;
using th03::present::Tram;
using th03::present::TramCell;
using th03::present::TramPoint;

using ComposedTextFrame = std::vector<ComposedPixel>;

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

	void set_jis(std::uint16_t code, JisGlyph glyph) {
		jis_code_ = code;
		jis_glyph_ = glyph;
	}

	[[nodiscard]] std::optional<JisGlyph> jis_glyph(
		std::uint16_t code
	) const override {
		++jis_queries_;
		last_jis_query_ = code;
		return (jis_code_.has_value() && *jis_code_ == code) ?
			jis_glyph_ : std::nullopt;
	}

	[[nodiscard]] std::size_t jis_queries() const {
		return jis_queries_;
	}

	[[nodiscard]] std::uint16_t last_jis_query() const {
		return last_jis_query_;
	}

private:
	std::array<std::optional<AnkGlyph>, 256U> glyphs_{};
	std::optional<std::uint16_t> jis_code_{};
	std::optional<JisGlyph> jis_glyph_{};
	mutable std::size_t jis_queries_ = 0U;
	mutable std::uint16_t last_jis_query_ = 0U;
};

[[nodiscard]] constexpr std::array<th03::present::TramCode, 2U> jis_cells(
	std::uint16_t canonical_jis
) {
	const auto first = static_cast<std::uint16_t>(canonical_jis >> 8U);
	const auto second = static_cast<std::uint16_t>(canonical_jis & 0x00FFU);
	const auto left = static_cast<th03::present::TramCode>(
		(second << 8U) | (first - 0x0020U)
	);
	return {left, static_cast<th03::present::TramCode>(left | 0x0080U)};
}

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

[[nodiscard]] std::expected<
	ComposedTextFrame,
	th03::present::TextComposeError
> compose_fixture(
	std::span<const Index4> source,
	const Tram &tram,
	const AnkGlyphProvider &ank
) {
	ComposedTextFrame output(th03::present::graphics_page_pixels);
	const auto result = th03::present::compose_text_layer(
		output,
		source,
		tram,
		ank
	);
	if(!result) {
		return std::unexpected(result.error());
	}
	return output;
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
	const auto result = compose_fixture(
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
	const auto result = compose_fixture(
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
	const auto result = compose_fixture(
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
	const auto ambiguous_result = compose_fixture(
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
	expect(
		ank.jis_queries() == 0U,
		"paired gaiji takes priority over every JIS interpretation"
	);

	Tram overlap;
	std::array<std::uint8_t, th03::present::gaiji_glyph_bytes> old_rows{};
	old_rows[0U] = 0x80U;
	std::array<std::uint8_t, th03::present::gaiji_glyph_bytes> new_rows{};
	new_rows[0U] = 0x40U;
	expect(
		overlap.load_gaiji(0x71U, old_rows) &&
		overlap.load_gaiji(0x78U, new_rows) &&
		overlap.put_gaiji({0, 0}, 0x71U, th03::present::tram_red) &&
		overlap.put_gaiji({1, 0}, 0x78U, th03::present::tram_green),
		"seed source-style one-cell overlapping gaiji redraw"
	);
	const auto overlap_result = compose_fixture(graphics(4U), overlap, ank);
	expect(
		overlap_result.has_value(),
		"unpaired non-ambiguous gaiji half composes without a JIS error"
	);
	expect(
		pixel(*overlap_result, 0U, 0U) == text_pixel(TextColor::red) &&
		pixel(*overlap_result, 9U, 0U) == text_pixel(TextColor::green),
		"overlap renders the retained old half and replacement gaiji independently"
	);
}

void test_jis_pair_and_independent_attributes() {
	SyntheticAnk ank;
	AnkGlyph ank_a{};
	ank_a.rows[0U] = 0x80U;
	ank.set(static_cast<std::uint8_t>('A'), ank_a);
	constexpr std::uint16_t canonical = 0x2441U;
	JisGlyph glyph{};
	glyph.row_halves[0U] = 0x80U;
	glyph.row_halves[1U] = 0x01U;
	glyph.row_halves[2U] = 0x40U;
	glyph.row_halves[3U] = 0x80U;
	ank.set_jis(canonical, glyph);
	const auto codes = jis_cells(canonical);

	Tram tram;
	std::array<std::uint8_t, th03::present::gaiji_glyph_bytes> gaiji{};
	gaiji[0U] = 0x80U;
	gaiji[1U] = 0x01U;
	expect(
		tram.load_gaiji(4U, gaiji) &&
		tram.put_cell({0, 0}, {
			static_cast<th03::present::TramCode>('A'),
			th03::present::tram_white,
		}) && tram.put_gaiji({2, 0}, 4U, th03::present::tram_cyan) &&
		tram.put_cell({6, 7}, {codes[0U], th03::present::tram_red}) &&
		tram.put_cell({7, 7}, {
			codes[1U],
			static_cast<th03::present::TramAttribute>(
				th03::present::tram_green | th03::present::tram_reverse
			),
		}),
		"seed JIS pair with independent half attributes"
	);
	const auto result = compose_fixture(graphics(9U), tram, ank);
	expect(result.has_value(), "paired JIS composition succeeds");
	expect(
		pixel(*result, 0U, 0U) == text_pixel(TextColor::white) &&
		pixel(*result, 16U, 0U) == text_pixel(TextColor::cyan) &&
		pixel(*result, 31U, 0U) == text_pixel(TextColor::cyan),
		"ANK, gaiji, and JIS compose in one visible text layer"
	);
	expect(
		ank.jis_queries() == 1U && ank.last_jis_query() == canonical,
		"paired TRAM cells query one canonical JIS word"
	);
	expect(
		pixel(*result, 48U, 112U) == text_pixel(TextColor::red) &&
		pixel(*result, 49U, 112U) == graphics_pixel(9U),
		"JIS left cell selects the glyph's left row byte"
	);
	expect(
		pixel(*result, 56U, 112U) == text_pixel(TextColor::green) &&
		pixel(*result, 63U, 112U) == graphics_pixel(9U),
		"JIS right cell independently applies its color and reverse attribute"
	);
	expect(
		pixel(*result, 49U, 113U) == text_pixel(TextColor::red) &&
		pixel(*result, 63U, 113U) == text_pixel(TextColor::green),
		"JIS rows preserve both stored half-byte boundaries"
	);
}

void test_visibility_and_disabled_cells() {
	SyntheticAnk ank;
	Tram tram;
	expect(
		tram.put_cell({0, 0}, {0x1234U, 0U}),
		"seed disabled unsupported code"
	);
	const auto disabled = compose_fixture(
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
	const auto hidden = compose_fixture(
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
		compose_fixture(graphics(0U), tram, ank),
		TextComposeErrorCode::missing_ank_glyph,
		{4, 5},
		"missing injected ANK bitmap is explicit"
	);

	tram.fill({});
	expect(tram.put_cell({6, 7}, {0x4104U, th03::present::tram_white}),
		"seed unpaired JIS cell");
	expect_cell_error(
		compose_fixture(graphics(0U), tram, ank),
		TextComposeErrorCode::malformed_jis_pair,
		{6, 7},
		"unpaired JIS cell is explicit"
	);

	tram.fill({});
	const auto wrapped_jis = jis_cells(0x2441U);
	expect(
		tram.put_cell({79, 6}, {
			wrapped_jis[0U], th03::present::tram_white,
		}) && tram.put_cell({0, 7}, {
			wrapped_jis[1U], th03::present::tram_white,
		}),
		"seed adjacent-memory JIS codes across a TRAM row boundary"
	);
	expect_cell_error(
		compose_fixture(graphics(0U), tram, ank),
		TextComposeErrorCode::malformed_jis_pair,
		{79, 6},
		"JIS halves cannot pair across TRAM rows"
	);

	tram.fill({});
	const auto missing_jis = jis_cells(0x2441U);
	expect(
		tram.put_cell({6, 7}, {missing_jis[0U], th03::present::tram_white}) &&
		tram.put_cell({7, 7}, {missing_jis[1U], th03::present::tram_white}),
		"seed unavailable JIS pair"
	);
	expect_cell_error(
		compose_fixture(graphics(0U), tram, ank),
		TextComposeErrorCode::missing_jis_glyph,
		{6, 7},
		"missing injected JIS bitmap is explicit"
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
		compose_fixture(graphics(0U), tram, ank),
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
		compose_fixture(graphics(0U), tram, ank),
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
		compose_fixture(graphics(0U), tram, ank),
		TextComposeErrorCode::attribute_not_supported,
		{12, 13},
		"source-unused attribute bit is not guessed"
	);
}

void test_late_text_error_is_atomic() {
	SyntheticAnk ank;
	AnkGlyph early{};
	early.rows[0U] = 0x80U;
	ank.set(static_cast<std::uint8_t>('A'), early);
	Tram tram;
	expect(
		tram.put_cell({0, 0}, {
			static_cast<th03::present::TramCode>('A'),
			th03::present::tram_white,
		}) && tram.put_cell({79, 24}, {0x4104U, th03::present::tram_white}),
		"seed valid early ANK and malformed late JIS"
	);
	auto source = graphics(3U);
	ComposedTextFrame output(
		th03::present::graphics_page_pixels,
		{ComposedPixelLayer::text, 6U}
	);
	const auto before = output;
	const auto result = th03::present::compose_text_layer(
		output,
		source,
		tram,
		ank
	);
	expect(
		!result && result.error().code ==
			TextComposeErrorCode::malformed_jis_pair,
		"late malformed JIS rejects the complete visible layer"
	);
	expect(output == before, "late text failure leaves caller output untouched");
}

void test_graphics_validation() {
	SyntheticAnk ank;
	Tram tram;
	auto valid_graphics = graphics(0U);
	ComposedTextFrame short_output(th03::present::graphics_page_pixels - 1U);
	const auto short_output_result = th03::present::compose_text_layer(
		short_output,
		valid_graphics,
		tram,
		ank
	);
	expect(
		!short_output_result && short_output_result.error().code ==
			TextComposeErrorCode::output_size_mismatch,
		"composition rejects a non-640x400 caller output"
	);

	auto short_graphics = graphics(0U);
	short_graphics.pop_back();
	const auto short_result = compose_fixture(
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
	const auto invalid_result = compose_fixture(
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
	test_jis_pair_and_independent_attributes();
	test_visibility_and_disabled_cells();
	test_explicit_deferred_paths();
	test_graphics_validation();
	test_late_text_error_is_atomic();
	std::fputs("th03-present-text tests passed\n", stdout);
	return 0;
}
