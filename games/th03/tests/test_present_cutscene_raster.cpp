#include "th03/present/cutscene_raster.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using th03::assets::PiImage;
using th03::present::AnkGlyph;
using th03::present::AnkGlyphProvider;
using th03::present::CutsceneApplyBackgroundPalette;
using th03::present::CutsceneAudioFade;
using th03::present::CutsceneBgm;
using th03::present::CutsceneClearPages;
using th03::present::CutsceneClearPicture;
using th03::present::CutsceneCrossfadePicture;
using th03::present::CutsceneCrossfadeStep;
using th03::present::CutsceneDrawGaiji;
using th03::present::CutsceneDrawGlyph;
using th03::present::CutsceneFadeColor;
using th03::present::CutsceneFadeDirection;
using th03::present::CutsceneFinish;
using th03::present::CutsceneFreeBackground;
using th03::present::CutsceneLoadBackground;
using th03::present::CutscenePaletteFade;
using th03::present::CutscenePlaySoundEffect;
using th03::present::CutscenePresentBackground;
using th03::present::CutsceneRasterDispatch;
using th03::present::CutsceneRasterErrorCode;
using th03::present::CutsceneRasterPresenter;
using th03::present::CutsceneReleaseBox;
using th03::present::CutsceneRestoreBox;
using th03::present::CutsceneSetTextColor;
using th03::present::CutsceneSetTextInterval;
using th03::present::CutsceneSetTextWeight;
using th03::present::CutsceneSetTone;
using th03::present::CutsceneShake;
using th03::present::CutsceneShowPage;
using th03::present::CutsceneShowPictureQuarter;
using th03::present::CutsceneSnapshotBox;
using th03::present::CutsceneWait;
using th03::present::GraphicsTextErrorCode;
using th03::present::GraphicsTextWeight;
using th03::present::JisGlyph;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::RasterMode;
using th03::present::Tram;

int checks = 0;
int failures = 0;

void expect(bool condition, std::string_view message) {
	++checks;
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

class TestFont final : public AnkGlyphProvider {
public:
	[[nodiscard]] std::optional<AnkGlyph> glyph(
		std::uint8_t
	) const override {
		return std::nullopt;
	}

	[[nodiscard]] std::optional<JisGlyph> jis_glyph(
		std::uint16_t canonical_jis
	) const override {
		last_query = canonical_jis;
		if(missing) {
			return std::nullopt;
		}
		return value;
	}

	JisGlyph value{};
	bool missing = false;
	mutable std::uint16_t last_query = 0U;
};

[[nodiscard]] CutsceneRasterPresenter make_presenter(
	Raster &raster,
	const TestFont &font,
	const Tram &tram
) {
	auto result = CutsceneRasterPresenter::create(raster, font, tram);
	expect(result.has_value(), "cutscene raster presenter creation succeeds");
	return std::move(*result);
}

[[nodiscard]] PiImage make_background(bool with_palette = true) {
	PiImage image{};
	image.image.width = 640U;
	image.image.height = 400U;
	image.image.indices.assign(640U * 400U, 0U);
	for(std::size_t y = 0U; y < 400U; ++y) {
		for(std::size_t x = 0U; x < 640U; ++x) {
			const auto quarter = ((y >= 200U) ? 2U : 0U) +
				((x >= 320U) ? 1U : 0U);
			image.image.indices[(y * 640U) + x] =
				static_cast<std::uint8_t>(quarter + 1U);
		}
	}
	if(with_palette) {
		image.palette.emplace();
		for(std::size_t index = 0U; index < image.palette->colors.size(); ++index) {
			image.palette->colors[index] = {
				static_cast<std::uint8_t>(index),
				static_cast<std::uint8_t>(15U - index),
				static_cast<std::uint8_t>(index / 2U),
			};
		}
	}
	return image;
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

[[nodiscard]] std::vector<std::uint8_t> page_copy(
	const Raster &raster,
	Page page
) {
	const auto source = raster.graphics(page);
	return {source.begin(), source.end()};
}

void test_creation_and_dispatch_contract() {
	TestFont font;
	Tram tram;
	Raster invalid;
	expect(
		invalid.set_raster_mode(RasterMode::zoomed_200_upper),
		"select invalid cutscene raster mode"
	);
	auto rejected = CutsceneRasterPresenter::create(invalid, font, tram);
	expect(
		!rejected && rejected.error().code ==
			CutsceneRasterErrorCode::invalid_raster_mode,
		"presenter rejects a non-400-line raster"
	);

	Raster raster;
	auto presenter = make_presenter(raster, font, tram);
	const std::array<th03::present::CutsceneOperation, 5U> external_ops{
		CutsceneWait{},
		CutsceneAudioFade{},
		CutsceneBgm{},
		CutscenePlaySoundEffect{},
		CutsceneFinish{},
	};
	for(const auto &operation : external_ops) {
		const auto result = presenter.execute(operation);
		expect(
			result && *result == CutsceneRasterDispatch::external,
			"wait/audio/finish operation remains external"
		);
	}

	auto result = presenter.execute(CutsceneLoadBackground{});
	expect(
		result && *result == CutsceneRasterDispatch::background_required,
		"background load is delegated to resource owner"
	);
	result = presenter.execute(CutsceneShake{2U, true});
	expect(
		result && *result == CutsceneRasterDispatch::shake_required,
		"shake is delegated to source-cadence stepping"
	);
	result = presenter.execute(CutsceneCrossfadePicture{0U, 1U, true});
	expect(
		!result && result.error().code ==
			CutsceneRasterErrorCode::missing_background,
		"crossfade validates its PI before dispatch"
	);

	expect(
		presenter.execute(CutsceneSetTextColor{15U}) ==
			CutsceneRasterDispatch::applied,
		"valid text color state is accepted"
	);
	expect(
		presenter.execute(CutsceneSetTextInterval{999U}) ==
			CutsceneRasterDispatch::applied,
		"text interval state remains a visual no-op"
	);
	expect(
		presenter.execute(CutsceneSetTextWeight{GraphicsTextWeight::black}) ==
			CutsceneRasterDispatch::applied,
		"valid text weight state is accepted"
	);
	expect(
		!presenter.execute(CutsceneSetTextColor{16U}),
		"invalid text color is rejected"
	);
	expect(
		!presenter.execute(CutsceneSetTextWeight{
			static_cast<GraphicsTextWeight>(99U)
		}),
		"invalid text weight is rejected"
	);

	expect(
		presenter.execute(CutsceneShowPage{1U}) ==
			CutsceneRasterDispatch::applied &&
			raster.shown_page() == Page::one,
		"show-page operation changes only the shown page"
	);
	expect(
		!presenter.execute(CutsceneShowPage{2U}) &&
			raster.shown_page() == Page::one,
		"invalid shown page is rejected atomically"
	);
	expect(
		presenter.execute(CutsceneSetTone{75U}) ==
			CutsceneRasterDispatch::applied &&
			raster.palette_tone() == 75,
		"set-tone applies immediately"
	);
	expect(
		!presenter.execute(CutsceneSetTone{1000U}) &&
			raster.palette_tone() == 75,
		"tone outside the VM numeric domain is rejected"
	);
}

void test_background_ownership_palette_and_box_snapshot() {
	TestFont font;
	Tram tram;
	Raster raster;
	auto presenter = make_presenter(raster, font, tram);

	auto background = make_background();
	expect(
		presenter.replace_background(background).has_value(),
		"valid background is accepted"
	);
	background.image.indices[0U] = 9U;
	background.palette->colors[1U] = {9U, 9U, 9U};
	expect(
		presenter.execute(CutscenePresentBackground{true}) ==
			CutsceneRasterDispatch::applied,
		"background with palette is presented"
	);
	expect_pixel(raster, Page::zero, {0, 0}, 1U, "presenter owns PI pixels");
	expect_pixel(raster, Page::one, {639, 399}, 4U, "full PI reaches page 1");
	expect_pixel(raster, Page::zero, {639, 399}, 4U, "full PI copies to page 0");
	expect(
		raster.access_page() == Page::zero && presenter.has_box_snapshot(),
		"background presentation leaves page 0 accessed and snapshots box"
	);
	expect(
		raster.applied_palette().colors[1U] == th03::assets::Rgb4{1U, 14U, 0U},
		"presenter owns and applies the original PI palette"
	);

	auto malformed = make_background();
	malformed.image.indices.pop_back();
	expect(
		!presenter.replace_background(std::move(malformed)) &&
			presenter.has_background(),
		"malformed replacement preserves current PI"
	);
	auto bad_pixel = make_background();
	bad_pixel.image.indices.back() = 16U;
	expect(
		!presenter.replace_background(std::move(bad_pixel)),
		"out-of-domain PI index is rejected"
	);
	auto bad_palette = make_background();
	bad_palette.palette->colors[3U].r = 16U;
	expect(
		!presenter.replace_background(std::move(bad_palette)),
		"out-of-domain PI palette is rejected"
	);
	expect(
		presenter.execute(CutsceneShowPictureQuarter{0U}) ==
			CutsceneRasterDispatch::applied,
		"PI remains usable after rejected replacements"
	);
	expect_pixel(raster, Page::zero, {160, 64}, 1U, "retained PI is unchanged");

	auto no_palette = make_background(false);
	expect(
		presenter.replace_background(std::move(no_palette)).has_value(),
		"palette-less PI is accepted for p-at"
	);
	const auto before_missing_palette = page_copy(raster, Page::one);
	expect(
		!presenter.execute(CutscenePresentBackground{true}) &&
			page_copy(raster, Page::one) == before_missing_palette,
		"p-equals rejects a missing palette before raster mutation"
	);
	expect(
		presenter.execute(CutscenePresentBackground{false}) ==
			CutsceneRasterDispatch::applied,
		"p-at presents a palette-less background"
	);
	expect(
		!presenter.execute(CutsceneApplyBackgroundPalette{}),
		"pp reports a missing PI palette"
	);
	expect(
		presenter.execute(CutsceneFreeBackground{}) ==
			CutsceneRasterDispatch::applied && !presenter.has_background(),
		"p-minus releases only the current PI"
	);
	expect(
		!presenter.execute(CutscenePresentBackground{false}) &&
			presenter.has_box_snapshot(),
		"missing PI does not release the independent box snapshot"
	);
}

void test_box_restore_release_and_clear_landmine() {
	TestFont font;
	Tram tram;
	Raster raster;
	auto presenter = make_presenter(raster, font, tram);
	const th03::present::Rect box{80, 320, 480U, 64U};
	expect(raster.clear(box, 3U), "seed page-0 box");
	expect(raster.set_access_page(Page::one), "select page 1 before snapshot");
	expect(raster.clear(box, 4U), "seed page-1 box");
	expect(
		presenter.execute(CutsceneSnapshotBox{}) ==
			CutsceneRasterDispatch::applied &&
			raster.access_page() == Page::zero,
		"snapshot always captures page 0 and leaves it accessed"
	);
	expect(raster.clear(box, 7U), "overwrite page-0 box");
	expect(raster.set_access_page(Page::one), "select page 1 for overwrite");
	expect(raster.clear(box, 8U), "overwrite page-1 box");
	expect(
		presenter.execute(CutsceneRestoreBox{}) ==
			CutsceneRasterDispatch::applied,
		"restore succeeds with a saved box"
	);
	expect_pixel(raster, Page::one, {80, 320}, 3U, "restore writes page 1 first");
	expect_pixel(raster, Page::zero, {559, 383}, 3U, "restore writes complete page 0 box");
	expect(
		raster.access_page() == Page::zero,
		"restore leaves page 0 accessed"
	);

	expect(raster.set_access_page(Page::one), "seed both pages before clear");
	expect(raster.plot({0, 0}, 9U), "seed page 1 before clear");
	expect(raster.set_access_page(Page::zero), "seed page 0 before clear");
	expect(raster.plot({0, 0}, 9U), "seed page 0 pixel before clear");
	expect(
		presenter.execute(CutsceneClearPages{false}) ==
			CutsceneRasterDispatch::applied,
		"TH03 clear-pages operation succeeds"
	);
	expect_pixel(raster, Page::zero, {0, 0}, 0U, "clear erases page 0");
	expect_pixel(raster, Page::one, {0, 0}, 0U, "clear erases page 1");
	expect(
		presenter.has_box_snapshot() &&
		presenter.execute(CutsceneRestoreBox{}) ==
			CutsceneRasterDispatch::applied,
		"clear preserves the stale TH03 box snapshot"
	);
	expect_pixel(raster, Page::zero, {80, 320}, 3U, "stale box is restorable");
	const auto page_before_bad_clear = page_copy(raster, Page::zero);
	expect(
		!presenter.execute(CutsceneClearPages{true}) &&
			page_copy(raster, Page::zero) == page_before_bad_clear,
		"non-TH03 resnapshot request is rejected atomically"
	);

	expect(
		presenter.execute(CutsceneReleaseBox{}) ==
			CutsceneRasterDispatch::applied && !presenter.has_box_snapshot(),
		"release discards the box snapshot"
	);
	const auto before_missing = page_copy(raster, Page::zero);
	expect(
		!presenter.execute(CutsceneRestoreBox{}) &&
			page_copy(raster, Page::zero) == before_missing,
		"restore without a snapshot fails atomically"
	);
}

void test_two_page_text_and_gaiji() {
	TestFont font;
	font.value.row_halves[0U] = 0x80U;
	font.value.row_halves[1U] = 0x01U;
	Tram tram;
	std::array<std::uint8_t, th03::present::gaiji_glyph_bytes> gaiji{};
	gaiji[0U] = 0x80U;
	gaiji[1U] = 0x01U;
	expect(tram.load_gaiji(31U, gaiji), "load synthetic gaiji");
	Raster raster;
	auto presenter = make_presenter(raster, font, tram);

	const CutsceneDrawGlyph glyph{
		{80, 320}, {0x82U, 0xA0U}, 5U, GraphicsTextWeight::normal,
	};
	expect(
		presenter.execute(glyph) == CutsceneRasterDispatch::applied,
		"fullwidth glyph draws"
	);
	expect_pixel(raster, Page::one, {80, 320}, 5U, "glyph left bit reaches page 1");
	expect_pixel(raster, Page::zero, {95, 320}, 5U, "glyph right bit reaches page 0");
	expect_pixel(raster, Page::one, {81, 320}, 0U, "glyph preserves transparent neighbor");
	expect(
		raster.access_page() == Page::zero && font.last_query == 0x2422U,
		"glyph maps Shift-JIS and leaves page 0 accessed"
	);

	font.missing = true;
	const auto before_missing_zero = page_copy(raster, Page::zero);
	const auto before_missing_one = page_copy(raster, Page::one);
	const auto missing = presenter.execute(CutsceneDrawGlyph{
		{96, 320}, {0x82U, 0xA2U}, 6U, GraphicsTextWeight::bold,
	});
	expect(
		!missing && missing.error().code == CutsceneRasterErrorCode::invalid_text &&
			missing.error().text_error &&
			missing.error().text_error->code ==
				GraphicsTextErrorCode::missing_jis_glyph,
		"missing machine glyph reports the nested text error"
	);
	expect(
		page_copy(raster, Page::zero) == before_missing_zero &&
			page_copy(raster, Page::one) == before_missing_one,
		"missing glyph is rejected before either page changes"
	);
	font.missing = false;
	expect(
		!presenter.execute(CutsceneDrawGlyph{
			{633, 320}, {0x82U, 0xA0U}, 5U, GraphicsTextWeight::normal,
		}),
		"off-page glyph is rejected"
	);

	expect(
		presenter.execute(CutsceneDrawGaiji{{112, 336}, 31U, 6U}) ==
			CutsceneRasterDispatch::applied,
		"gaiji draws to both pages"
	);
	expect_pixel(raster, Page::one, {112, 336}, 6U, "gaiji left bit reaches page 1");
	expect_pixel(raster, Page::zero, {127, 336}, 6U, "gaiji right bit reaches page 0");
	expect(
		raster.access_page() == Page::zero,
		"gaiji leaves page 0 accessed"
	);
	const auto before_bad_gaiji = page_copy(raster, Page::zero);
	expect(
		!presenter.execute(CutsceneDrawGaiji{{112, 336}, 256U, 6U}) &&
			!presenter.execute(CutsceneDrawGaiji{{113, 336}, 31U, 6U}) &&
			page_copy(raster, Page::zero) == before_bad_gaiji,
		"invalid or non-byte-aligned gaiji is rejected atomically"
	);
}

void test_picture_transactions_and_crossfade_steps() {
	TestFont font;
	Tram tram;
	Raster raster;
	auto presenter = make_presenter(raster, font, tram);
	expect(
		presenter.replace_background(make_background()).has_value(),
		"load synthetic quarter sheet"
	);
	const th03::present::Rect picture{160, 64, 320U, 200U};
	expect(raster.clear(picture, 9U), "seed page-0 picture");
	expect(raster.set_access_page(Page::one), "select page 1 for picture seed");
	expect(raster.clear(picture, 8U), "seed page-1 picture");

	expect(
		presenter.execute(CutsceneShowPictureQuarter{2U}) ==
			CutsceneRasterDispatch::applied,
		"immediate picture quarter succeeds"
	);
	expect_pixel(raster, Page::zero, {160, 64}, 3U, "quarter draws page 0");
	expect_pixel(raster, Page::one, {479, 263}, 3U, "quarter copies to page 1");
	expect(
		raster.shown_page() == Page::zero && raster.access_page() == Page::zero,
		"quarter transaction leaves page 0 shown and accessed"
	);
	expect(
		presenter.execute(CutsceneClearPicture{}) ==
			CutsceneRasterDispatch::applied,
		"picture clear succeeds"
	);
	expect_pixel(raster, Page::zero, {160, 64}, 0U, "picture clear erases page 0");
	expect_pixel(raster, Page::one, {479, 263}, 0U, "picture clear copies to page 1");

	expect(raster.clear(picture, 1U), "seed old crossfade picture");
	expect(raster.set_access_page(Page::one), "select page 1 for crossfade seed");
	expect(raster.clear(picture, 1U), "seed matching page-1 crossfade picture");
	const CutsceneCrossfadePicture crossfade{1U, 2U, true};
	expect(
		presenter.execute(crossfade) ==
			CutsceneRasterDispatch::crossfade_required,
		"valid crossfade is delegated for cadence"
	);
	expect(
		presenter.crossfade_step(
			crossfade, CutsceneCrossfadeStep::mask_first
		).has_value(),
		"first crossfade mask succeeds"
	);
	expect_pixel(raster, Page::zero, {160, 64}, 1U, "first mask keeps row-0 pixel");
	expect_pixel(raster, Page::zero, {163, 65}, 2U, "first mask reveals selected pixel");
	expect_pixel(raster, Page::one, {163, 65}, 2U, "masked result copies to page 1");
	expect(
		raster.shown_page() == Page::zero && raster.access_page() == Page::zero,
		"masked transaction restores page state"
	);
	const auto before_bad_step = page_copy(raster, Page::zero);
	expect(
		!presenter.crossfade_step(
			crossfade, static_cast<CutsceneCrossfadeStep>(99U)
		) && page_copy(raster, Page::zero) == before_bad_step,
		"invalid crossfade step is rejected atomically"
	);
	expect(
		presenter.crossfade_step(
			crossfade, CutsceneCrossfadeStep::mask_second
		).has_value() &&
		presenter.crossfade_step(
			crossfade, CutsceneCrossfadeStep::mask_third
		).has_value() &&
		presenter.crossfade_step(
			crossfade, CutsceneCrossfadeStep::mask_fourth
		).has_value() &&
		presenter.crossfade_step(
			crossfade, CutsceneCrossfadeStep::complete
		).has_value(),
		"remaining four source crossfade transactions succeed"
	);
	expect_pixel(raster, Page::zero, {160, 64}, 2U, "final step completes page 0");
	expect_pixel(raster, Page::one, {479, 263}, 2U, "final step completes page 1");
	expect(
		!presenter.execute(CutsceneShowPictureQuarter{4U}) &&
		!presenter.execute(CutsceneCrossfadePicture{4U, 1U, true}),
		"invalid immediate and crossfade quarters are rejected"
	);
}

void test_shake_and_palette_fade_samples() {
	TestFont font;
	Tram tram;
	Raster raster;
	auto presenter = make_presenter(raster, font, tram);
	const CutsceneShake shake{2U, true};
	expect(
		presenter.execute(shake) == CutsceneRasterDispatch::shake_required,
		"shake requires caller cadence"
	);
	expect(
		presenter.shake_step(shake, 0U).has_value() &&
			raster.scroll_y() == 396U,
		"even shake position scrolls to 396"
	);
	expect(
		presenter.shake_step(shake, 1U).has_value() &&
			raster.scroll_y() == 4U,
		"odd shake position scrolls to 4"
	);
	expect(
		presenter.shake_step(shake, 2U).has_value() &&
			raster.scroll_y() == 396U,
		"inclusive final shake position is applied"
	);
	expect(
		!presenter.shake_step(shake, 3U) && raster.scroll_y() == 396U,
		"out-of-range shake position preserves scroll"
	);
	presenter.finish_shake();
	expect(raster.scroll_y() == 0U, "shake finish restores scroll zero");

	expect(
		raster.set_pending_palette_entry_rgb8(0U, {255U, 255U, 255U}),
		"seed fade palette"
	);
	raster.set_palette_tone(100);
	raster.apply_pending_palette();
	const CutscenePaletteFade black_in{
		CutsceneFadeColor::black, CutsceneFadeDirection::in, 2U,
	};
	expect(
		presenter.execute(black_in) ==
			CutsceneRasterDispatch::palette_fade_required &&
			raster.palette_tone() == 0 &&
			raster.applied_palette().colors[0U] ==
				th03::assets::Rgb4{15U, 15U, 15U},
		"fade prelude sets tone before VSync without applying it"
	);
	for(std::uint8_t sample = 0U; sample < 17U; ++sample) {
		expect(
			presenter.palette_fade_sample(black_in, sample).has_value() &&
				raster.palette_tone() ==
					static_cast<std::int16_t>(sample * 6U),
			"black-in loop sample uses exact six-point tone"
		);
	}
	expect(
		presenter.palette_fade_sample(black_in, 17U).has_value() &&
			raster.palette_tone() == 100 &&
			raster.applied_palette().colors[0U] ==
				th03::assets::Rgb4{15U, 15U, 15U},
		"black-in final sample applies exact normal endpoint"
	);
	const auto tone_before_invalid = raster.palette_tone();
	expect(
		!presenter.palette_fade_sample(black_in, 18U) &&
			raster.palette_tone() == tone_before_invalid,
		"invalid fade sample is rejected atomically"
	);

	struct FadeCase {
		CutscenePaletteFade operation;
		std::int16_t start;
		std::int16_t first;
		std::int16_t sixteenth;
		std::int16_t final;
	};
	const std::array<FadeCase, 3U> cases{{
		{{CutsceneFadeColor::black, CutsceneFadeDirection::out, 0U},
			100, 100, 4, 0},
		{{CutsceneFadeColor::white, CutsceneFadeDirection::in, 1U},
			200, 200, 104, 100},
		{{CutsceneFadeColor::white, CutsceneFadeDirection::out, 999U},
			100, 100, 196, 200},
	}};
	for(const auto &fade : cases) {
		expect(
			presenter.execute(fade.operation) ==
				CutsceneRasterDispatch::palette_fade_required &&
				raster.palette_tone() == fade.start,
			"fade prelude selects exact source start"
		);
		expect(
			presenter.palette_fade_sample(fade.operation, 0U).has_value() &&
				raster.palette_tone() == fade.first,
			"fade sample zero applies exact source start"
		);
		expect(
			presenter.palette_fade_sample(fade.operation, 16U).has_value() &&
				raster.palette_tone() == fade.sixteenth,
			"fade sample sixteen applies final loop tone"
		);
		expect(
			presenter.palette_fade_sample(fade.operation, 17U).has_value() &&
				raster.palette_tone() == fade.final,
			"fade sample seventeen applies exact endpoint"
		);
	}
	const auto before_bad_fade = raster.palette_tone();
	expect(
		!presenter.execute(CutscenePaletteFade{
			static_cast<CutsceneFadeColor>(99U),
			CutsceneFadeDirection::in,
			1U,
		}) && raster.palette_tone() == before_bad_fade,
		"invalid fade operation is rejected before the prelude"
	);
}

void test_raster_mode_change_is_detected() {
	TestFont font;
	Tram tram;
	Raster raster;
	auto presenter = make_presenter(raster, font, tram);
	expect(
		raster.set_raster_mode(RasterMode::zoomed_200_lower),
		"change raster mode after presenter creation"
	);
	const auto before = page_copy(raster, Page::zero);
	const auto result = presenter.execute(CutsceneClearPicture{});
	expect(
		!result && result.error().code ==
			CutsceneRasterErrorCode::draw_failed &&
			page_copy(raster, Page::zero) == before,
		"later invalid raster mode is rejected before drawing"
	);
}

} // namespace

int main() {
	test_creation_and_dispatch_contract();
	test_background_ownership_palette_and_box_snapshot();
	test_box_restore_release_and_clear_landmine();
	test_two_page_text_and_gaiji();
	test_picture_transactions_and_crossfade_steps();
	test_shake_and_palette_fade_samples();
	test_raster_mode_change_is_detected();

	if(failures != 0) {
		std::fprintf(stderr, "%d/%d checks failed\n", failures, checks);
		return 1;
	}
	std::printf("all %d cutscene raster checks passed\n", checks);
	return 0;
}
