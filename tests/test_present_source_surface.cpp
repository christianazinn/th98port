#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>

#include "th03/present/source_surface.hpp"

namespace {

using th03::assets::Rgb4;
using th03::present::AnkGlyph;
using th03::present::AnkGlyphProvider;
using th03::present::ComposedPixel;
using th03::present::ComposedPixelLayer;
using th03::present::Index4;
using th03::present::JisGlyph;
using th03::present::Page;
using th03::present::Raster;
using th03::present::RasterMode;
using th03::present::SourceSurface;
using th03::present::SourceSurfaceErrorCode;
using th03::present::TextColor;
using th03::present::TextComposeErrorCode;
using th03::present::Tram;
using th03::present::TramCell;
using th03::present::TramPoint;

using SurfaceBuffer = std::array<
	ComposedPixel,
	th03::present::graphics_page_pixels
>;

void expect(bool condition, std::string_view message)
{
	if(!condition) {
		std::fprintf(
			stderr,
			"FAIL: %.*s\n",
			static_cast<int>(message.size()),
			message.data()
		);
		std::exit(1);
	}
}

[[nodiscard]] std::size_t offset(std::size_t x, std::size_t y)
{
	return (y * th03::present::graphics_width) + x;
}

[[nodiscard]] ComposedPixel graphics_pixel(Index4 index)
{
	return {ComposedPixelLayer::graphics, index};
}

[[nodiscard]] ComposedPixel text_pixel(TextColor color)
{
	return {ComposedPixelLayer::text, static_cast<std::uint8_t>(color)};
}

class SyntheticAnk final : public AnkGlyphProvider {
public:
	void set(std::uint8_t code, AnkGlyph glyph)
	{
		glyphs_[code] = glyph;
	}

	[[nodiscard]] std::optional<AnkGlyph> glyph(
		std::uint8_t code
	) const override
	{
		return glyphs_[code];
	}

	void set_jis(std::uint16_t code, JisGlyph glyph)
	{
		jis_code_ = code;
		jis_glyph_ = glyph;
	}

	[[nodiscard]] std::optional<JisGlyph> jis_glyph(
		std::uint16_t code
	) const override
	{
		return (jis_code_.has_value() && *jis_code_ == code) ?
			jis_glyph_ : std::nullopt;
	}

private:
	std::array<std::optional<AnkGlyph>, 256U> glyphs_{};
	std::optional<std::uint16_t> jis_code_{};
	std::optional<JisGlyph> jis_glyph_{};
};

[[nodiscard]] constexpr std::array<th03::present::TramCode, 2U> jis_cells(
	std::uint16_t canonical_jis
)
{
	const auto first = static_cast<std::uint16_t>(canonical_jis >> 8U);
	const auto second = static_cast<std::uint16_t>(canonical_jis & 0x00FFU);
	const auto left = static_cast<th03::present::TramCode>(
		(second << 8U) | (first - 0x0020U)
	);
	return {left, static_cast<th03::present::TramCode>(left | 0x0080U)};
}

[[nodiscard]] std::optional<SourceSurface> compose(
	SurfaceBuffer &output,
	const Raster &raster,
	const Tram &tram,
	const AnkGlyphProvider &ank
)
{
	const auto result = th03::present::compose_source_surface(
		output,
		raster,
		tram,
		ank
	);
	return result.has_value() ? std::optional<SourceSurface>(*result) :
		std::nullopt;
}

void test_page_palette_and_400_line_identity()
{
	Raster raster;
	Tram tram;
	SyntheticAnk ank;
	static SurfaceBuffer output{};
	expect(raster.set_access_page(Page::zero), "select first source page");
	expect(raster.plot({2, 0}, 3U), "seed first source page");
	expect(raster.plot({2, 399}, 4U), "seed final 400-line row");
	expect(raster.set_access_page(Page::one), "select shown page");
	expect(raster.plot({2, 0}, 7U), "seed shown page");
	expect(raster.set_shown_page(Page::one), "show second page");
	expect(
		raster.set_pending_palette_entry_rgb8(3U, {0xA5U, 0xB7U, 0xC9U}),
		"seed logical palette"
	);
	raster.apply_pending_palette();

	const auto surface = compose(output, raster, tram, ank);
	expect(surface.has_value(), "400-line source surface composes");
	expect(
		surface->pixels.data() == output.data(),
		"surface exposes the caller-owned fixed output buffer without a copy"
	);
	expect(
		surface->pixels[offset(2U, 0U)] == graphics_pixel(7U),
		"shown-page selection resolves before composition"
	);
	expect(
		surface->pixels[offset(2U, 399U)] == graphics_pixel(0U),
		"400-line surface reads the shown page's same physical row"
	);
	expect(
		surface->shown_page == Page::one &&
			surface->raster_mode == RasterMode::lines_400 &&
			surface->effective_scroll_y == 0U,
		"surface retains visible raster selectors"
	);
	expect(
		surface->graphics_palette.colors[3U] == Rgb4{10U, 11U, 12U},
		"surface exports Raster's applied RGB4 palette rather than RGB8 pending values"
	);
}

void test_zoom_windows_and_scroll_wrap()
{
	{
		Raster raster;
		Tram tram;
		SyntheticAnk ank;
		static SurfaceBuffer output{};
		expect(
			raster.set_raster_mode(RasterMode::zoomed_200_upper),
			"select upper 200-line mode"
		);
		expect(raster.plot({3, 0}, 5U), "seed upper logical row zero");
		expect(raster.plot({3, 1}, 6U), "seed upper logical row one");
		const auto surface = compose(output, raster, tram, ank);
		expect(surface.has_value(), "upper 200-line surface composes");
		expect(
			surface->pixels[offset(3U, 0U)] == graphics_pixel(5U) &&
				surface->pixels[offset(3U, 1U)] == graphics_pixel(5U) &&
				surface->pixels[offset(3U, 2U)] == graphics_pixel(6U) &&
				surface->pixels[offset(3U, 3U)] == graphics_pixel(6U),
			"graph_200line upper doubles each logical source scanline"
		);
	}
	{
		Raster raster;
		Tram tram;
		SyntheticAnk ank;
		static SurfaceBuffer output{};
		expect(
			raster.set_raster_mode(RasterMode::zoomed_200_lower),
			"select lower 200-line mode"
		);
		expect(raster.plot({4, 0}, 1U), "seed unrelated upper raw row");
		expect(raster.plot({4, 200}, 9U), "seed lower source window");
		const auto surface = compose(output, raster, tram, ank);
		expect(surface.has_value(), "lower 200-line surface composes");
		expect(
			surface->pixels[offset(4U, 0U)] == graphics_pixel(9U) &&
				surface->pixels[offset(4U, 1U)] == graphics_pixel(9U),
			"graph_200line lower selects raw rows 200 through 399"
		);
	}
	{
		Raster raster;
		Tram tram;
		SyntheticAnk ank;
		static SurfaceBuffer output{};
		expect(raster.plot({5, 0}, 1U), "seed scroll-wrapped first row");
		expect(raster.plot({5, 4}, 2U), "seed scroll first display row");
		expect(raster.plot({5, 399}, 3U), "seed scroll final source row");
		raster.set_scroll_y(4U);
		const auto surface = compose(output, raster, tram, ank);
		expect(surface.has_value(), "scrolling surface composes");
		expect(
			surface->pixels[offset(5U, 0U)] == graphics_pixel(2U) &&
				surface->pixels[offset(5U, 395U)] == graphics_pixel(3U) &&
				surface->pixels[offset(5U, 396U)] == graphics_pixel(1U),
			"graph_scrollup rotates the resolved 400-line graphics page"
		);
	}
	{
		Raster raster;
		Tram tram;
		SyntheticAnk ank;
		static SurfaceBuffer output{};
		expect(raster.plot({6, 0}, 1U), "seed 400-line clamp origin");
		expect(raster.plot({6, 399}, 2U), "seed 400-line clamp boundary");
		raster.set_scroll_y(400U);
		auto surface = compose(output, raster, tram, ank);
		expect(
			surface && surface->effective_scroll_y == 0U &&
				surface->pixels[offset(6U, 0U)] == graphics_pixel(1U),
			"graph_scrollup clamps a 400-line-height scroll to zero"
		);
		raster.set_scroll_y(0xFFFFU);
		surface = compose(output, raster, tram, ank);
		expect(
			surface && surface->effective_scroll_y == 0U &&
				surface->pixels[offset(6U, 399U)] == graphics_pixel(2U),
			"graph_scrollup clamps an out-of-range word scroll to zero"
		);
	}
	for(const auto mode : {
		RasterMode::zoomed_200_upper,
		RasterMode::zoomed_200_lower,
	}) {
		Raster raster;
		Tram tram;
		SyntheticAnk ank;
		static SurfaceBuffer output{};
		expect(raster.set_raster_mode(mode), "select a 200-line scroll window");
		const auto raw_top = (mode == RasterMode::zoomed_200_upper) ? 0 : 200;
		expect(
			raster.plot({7, raw_top}, 3U) &&
				raster.plot({7, raw_top + 199}, 4U),
			"seed both ends of a 200-line source window"
		);
		raster.set_scroll_y(199U);
		auto surface = compose(output, raster, tram, ank);
		expect(
			surface && surface->effective_scroll_y == 199U &&
				surface->pixels[offset(7U, 0U)] == graphics_pixel(4U) &&
				surface->pixels[offset(7U, 2U)] == graphics_pixel(3U),
			"a 199-line scroll wraps within either 200-line window"
		);
		raster.set_scroll_y(200U);
		surface = compose(output, raster, tram, ank);
		expect(
			surface && surface->effective_scroll_y == 0U &&
				surface->pixels[offset(7U, 0U)] == graphics_pixel(3U),
			"a 200-line scroll clamps to zero in either zoomed window"
		);
	}
}

void test_text_last_gaiji_and_visibility()
{
	Raster raster;
	Tram tram;
	SyntheticAnk ank;
	static SurfaceBuffer output{};
	AnkGlyph a{};
	a.rows[0U] = 0x80U;
	ank.set(static_cast<std::uint8_t>('A'), a);
	expect(raster.plot({0, 0}, 11U), "seed graphics beneath ANK");
	expect(
		tram.put_cell({0, 0}, {
			static_cast<std::uint16_t>('A'),
			th03::present::tram_white,
		}),
		"seed visible ANK"
	);
	std::array<std::uint8_t, th03::present::gaiji_glyph_bytes> gaiji{};
	gaiji[0U] = 0x80U;
	gaiji[1U] = 0x01U;
	expect(tram.load_gaiji(3U, gaiji), "load paired gaiji bitmap");
	expect(
		tram.put_gaiji({2, 0}, 3U, th03::present::tram_green),
		"seed paired gaiji"
	);

	const auto surface = compose(output, raster, tram, ank);
	expect(surface.has_value(), "text source surface composes");
	expect(
		surface->pixels[offset(0U, 0U)] == text_pixel(TextColor::white) &&
			surface->pixels[offset(1U, 0U)] == graphics_pixel(0U),
		"visible ANK overlays graphics only at its source bits"
	);
	expect(
		surface->pixels[offset(16U, 0U)] == text_pixel(TextColor::green) &&
			surface->pixels[offset(31U, 0U)] == text_pixel(TextColor::green),
		"ordered gaiji cells compose their left and right source bytes"
	);

	Tram disabled;
	expect(
		disabled.put_cell({0, 0}, {0x2204U, 0U}),
		"seed disabled unsupported text cell"
	);
	const auto disabled_surface = compose(output, raster, disabled, ank);
	expect(disabled_surface.has_value(), "disabled text bypasses glyph lookup");
	expect(
		disabled_surface->pixels[offset(0U, 0U)] == graphics_pixel(11U),
		"disabled text leaves the resolved graphics surface unchanged"
	);
}

void test_text_coordinates_ignore_graphics_mapping()
{
	Raster raster;
	Tram tram;
	SyntheticAnk ank;
	static SurfaceBuffer output{};
	AnkGlyph a{};
	a.rows[0U] = 0x80U;
	ank.set(static_cast<std::uint8_t>('A'), a);
	expect(
		raster.set_raster_mode(RasterMode::zoomed_200_lower),
		"select mapped graphics for physical TRAM test"
	);
	expect(
		raster.plot({0, 399}, 6U) && raster.plot({0, 200}, 7U),
		"seed scrolled lower-window graphics"
	);
	raster.set_scroll_y(199U);
	expect(
		tram.put_cell({0, 0}, {
			static_cast<std::uint16_t>('A'),
			th03::present::tram_cyan,
		}),
		"seed physical top-left TRAM cell"
	);
	const auto surface = compose(output, raster, tram, ank);
	expect(surface.has_value(), "mapped graphics with physical text composes");
	expect(
		surface->pixels[offset(0U, 0U)] == text_pixel(TextColor::cyan) &&
			surface->pixels[offset(0U, 1U)] == graphics_pixel(6U) &&
			surface->pixels[offset(0U, 2U)] == graphics_pixel(7U),
		"TRAM remains physical while graphics scrolls and doubles vertically"
	);
}

void test_jis_source_surface()
{
	Raster raster;
	Tram tram;
	SyntheticAnk font;
	static SurfaceBuffer output{};
	constexpr std::uint16_t canonical = 0x2441U;
	JisGlyph glyph{};
	glyph.row_halves[0U] = 0x80U;
	glyph.row_halves[1U] = 0x01U;
	font.set_jis(canonical, glyph);
	const auto codes = jis_cells(canonical);
	expect(
		tram.put_cell({10, 2}, {codes[0U], th03::present::tram_red}) &&
		tram.put_cell({11, 2}, {codes[1U], th03::present::tram_green}),
		"seed paired JIS source-surface cells"
	);
	const auto result = compose(output, raster, tram, font);
	expect(result.has_value(), "source surface accepts injected JIS glyph");
	expect(
		output[offset(80U, 32U)] == text_pixel(TextColor::red) &&
		output[offset(95U, 32U)] == text_pixel(TextColor::green),
		"source surface composes both JIS halves with per-cell attributes"
	);
}

void test_malformed_text_is_atomic()
{
	Raster raster;
	Tram tram;
	SyntheticAnk missing;
	static SurfaceBuffer output{};
	static SurfaceBuffer preimage{};
	for(std::size_t index = 0U; index < output.size(); ++index) {
		output[index] = (index % 3U == 0U) ?
			text_pixel(TextColor::cyan) :
			graphics_pixel(static_cast<Index4>(index & 0x0FU));
	}
	preimage = output;
	AnkGlyph a{};
	a.rows[0U] = 0x80U;
	missing.set(static_cast<std::uint8_t>('A'), a);
	expect(
		tram.put_cell({0, 0}, {
			static_cast<std::uint16_t>('A'),
			th03::present::tram_white,
		}),
		"seed an early valid injected glyph"
	);
	expect(
		tram.put_cell({79, 24}, {
			static_cast<std::uint16_t>('Z'),
			th03::present::tram_white,
		}),
		"seed a late missing injected glyph"
	);
	const auto missing_result = th03::present::compose_source_surface(
		output,
		raster,
		tram,
		missing
	);
	expect(
		!missing_result &&
			missing_result.error().code == SourceSurfaceErrorCode::text_composition &&
			missing_result.error().text.code == TextComposeErrorCode::missing_ank_glyph,
		"missing provider data is explicit"
	);
	expect(
		output == preimage,
		"a late malformed provider input leaves the complete output untouched"
	);

	tram.fill({});
	expect(
		tram.put_cell({0, 0}, {
			static_cast<std::uint16_t>('A'),
			th03::present::tram_white,
		}),
		"seed early ANK before late missing JIS"
	);
	const auto late_jis = jis_cells(0x2441U);
	expect(
		tram.put_cell({78, 24}, {late_jis[0U], th03::present::tram_white}) &&
		tram.put_cell({79, 24}, {late_jis[1U], th03::present::tram_red}),
		"seed late JIS pair with independently valid attributes"
	);
	const auto jis_result = th03::present::compose_source_surface(
		output,
		raster,
		tram,
		missing
	);
	expect(
		!jis_result && jis_result.error().text.code ==
			TextComposeErrorCode::missing_jis_glyph,
		"late missing JIS provider data is explicit"
	);
	expect(output == preimage, "late missing JIS preserves the complete preimage");

	tram.fill({});
	expect(
		tram.put_cell({0, 0}, {
			static_cast<std::uint16_t>('A'),
			th03::present::tram_white,
		}),
		"seed early ANK before row-wrapped JIS"
	);
	expect(
		tram.put_cell({79, 23}, {late_jis[0U], th03::present::tram_white}) &&
		tram.put_cell({0, 24}, {late_jis[1U], th03::present::tram_red}),
		"seed adjacent-memory JIS codes across a TRAM row boundary"
	);
	const auto wrapped_jis_result = th03::present::compose_source_surface(
		output,
		raster,
		tram,
		missing
	);
	expect(
		!wrapped_jis_result && wrapped_jis_result.error().text.code ==
			TextComposeErrorCode::malformed_jis_pair,
		"source surface rejects a JIS pair that wraps to the next TRAM row"
	);
	expect(output == preimage, "row-wrapped JIS preserves the complete preimage");

	tram.fill({});
	expect(
		tram.put_cell({0, 0}, {
			static_cast<std::uint16_t>('A'),
			th03::present::tram_white,
		}),
		"seed an early valid glyph before a late unsupported attribute"
	);
	expect(
		tram.put_cell({79, 24}, {
			static_cast<std::uint16_t>('A'),
			static_cast<std::uint16_t>(
				th03::present::tram_white | th03::present::tram_blink
			),
		}),
		"seed deferred blink attribute"
	);
	const auto blink_result = th03::present::compose_source_surface(
		output,
		raster,
		tram,
		missing
	);
	expect(
		!blink_result &&
			blink_result.error().text.code == TextComposeErrorCode::blink_not_supported,
		"unsupported source input remains explicit"
	);
	expect(
		output == preimage,
		"every late malformed composition path preserves the complete preimage"
	);
}

} // namespace

int main()
{
	test_page_palette_and_400_line_identity();
	test_zoom_windows_and_scroll_wrap();
	test_text_last_gaiji_and_visibility();
	test_text_coordinates_ignore_graphics_mapping();
	test_jis_source_surface();
	test_malformed_text_is_atomic();
	std::fputs("th03-present-source-surface tests passed\n", stdout);
	return 0;
}
