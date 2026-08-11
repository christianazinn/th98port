#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "th03/assets/types.hpp"
#include "th03/present/cutscene_raster.hpp"
#include "th03/present/cutscene_script.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/text.hpp"
#include "th03/present/tram.hpp"

namespace {

using th03::assets::PiImage;
using th03::assets::Rgb4;
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
using th03::present::CutsceneFilename;
using th03::present::CutsceneFinish;
using th03::present::CutsceneFreeBackground;
using th03::present::CutsceneLoadBackground;
using th03::present::CutsceneOperation;
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
using th03::present::CutsceneWaitKind;
using th03::present::GraphicsTextWeight;
using th03::present::JisGlyph;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::RasterMode;
using th03::present::Rect;
using th03::present::Tram;
using th03::present::graphics_height;
using th03::present::graphics_width;

int failures = 0;
int checks = 0;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

class SyntheticFont final : public AnkGlyphProvider {
public:
	std::optional<AnkGlyph> glyph(std::uint8_t) const override
	{
		return std::nullopt;
	}

	std::optional<JisGlyph> jis_glyph(std::uint16_t) const override
	{
		if(missing) {
			return std::nullopt;
		}
		JisGlyph glyph{};
		for(std::size_t row = 0U; row < 16U; ++row) {
			glyph.row_halves[row * 2U] = 0x80U;
			glyph.row_halves[(row * 2U) + 1U] = 0x01U;
		}
		return glyph;
	}

	bool missing = false;
};

struct Fixture {
	Fixture()
	{
		std::array<std::uint8_t, 32U> gaiji{};
		for(std::size_t row = 0U; row < 16U; ++row) {
			gaiji[row * 2U] = 0x80U;
			gaiji[(row * 2U) + 1U] = 0x01U;
		}
		expect(tram.load_gaiji(26U, gaiji), "fixture loads MIKOFT gaiji");
		auto created = CutsceneRasterPresenter::create(raster, font, tram);
		expect(created.has_value(), "fixture creates cutscene Raster presenter");
		if(created) {
			presenter.emplace(std::move(*created));
		}
	}

	Raster raster{};
	Tram tram{};
	SyntheticFont font{};
	std::optional<CutsceneRasterPresenter> presenter{};
};

bool fill_page(Raster &raster, Page page, std::uint8_t color)
{
	return raster.set_access_page(page) && raster.clear(
		{0, 0, graphics_width, graphics_height}, color
	);
}

std::uint8_t pixel(const Raster &raster, Page page, Point point)
{
	const auto value = raster.pixel(page, point);
	expect(value.has_value(), "pixel probe is in range");
	return value.value_or(0xFFU);
}

PiImage make_background(bool with_palette = true)
{
	PiImage image{};
	image.image.width = graphics_width;
	image.image.height = graphics_height;
	image.image.indices.resize(
		static_cast<std::size_t>(graphics_width) * graphics_height
	);
	for(std::size_t y = 0U; y < graphics_height; ++y) {
		for(std::size_t x = 0U; x < graphics_width; ++x) {
			const auto quarter = static_cast<std::uint8_t>(
				1U + ((x >= 320U) ? 1U : 0U) + ((y >= 200U) ? 2U : 0U)
			);
			image.image.indices[(y * graphics_width) + x] = quarter;
		}
	}
	if(with_palette) {
		image.palette.emplace();
		for(std::size_t index = 0U; index < image.palette->colors.size(); ++index) {
			image.palette->colors[index] = Rgb4{
				static_cast<std::uint8_t>(index),
				static_cast<std::uint8_t>(15U - index),
				static_cast<std::uint8_t>(index / 2U),
			};
		}
	}
	return image;
}

template <class T>
std::optional<CutsceneRasterDispatch> execute(
	CutsceneRasterPresenter &presenter,
	T operation,
	const char *message
)
{
	auto result = presenter.execute(CutsceneOperation{std::move(operation)});
	expect(result.has_value(), message);
	if(!result) {
		return std::nullopt;
	}
	return *result;
}

void test_factory_and_two_page_text_box_lifecycle()
{
	Raster invalid{};
	expect(invalid.set_raster_mode(RasterMode::zoomed_200_upper),
		"invalid-mode fixture selects zoomed mode");
	SyntheticFont font{};
	Tram tram{};
	auto rejected = CutsceneRasterPresenter::create(invalid, font, tram);
	expect(!rejected && rejected.error().code ==
		CutsceneRasterErrorCode::invalid_raster_mode,
		"presenter rejects non-400-line Raster");

	Fixture fixture{};
	expect(fill_page(fixture.raster, Page::zero, 2U) &&
		fill_page(fixture.raster, Page::one, 3U),
		"seed both pages independently");
	auto &presenter = *fixture.presenter;
	expect(execute(presenter, CutsceneShowPage{0U}, "show-page prologue applies") ==
		CutsceneRasterDispatch::applied,
		"show-page prologue is immediate");
	expect(execute(presenter, CutsceneSnapshotBox{}, "box prologue snapshots") ==
		CutsceneRasterDispatch::applied && presenter.has_box_snapshot(),
		"box snapshot is retained");

	const CutsceneDrawGlyph glyph{
		{80, 320}, {0x82U, 0xA0U}, 6U, GraphicsTextWeight::normal,
	};
	expect(execute(presenter, glyph, "regular glyph draws") ==
		CutsceneRasterDispatch::applied,
		"regular glyph dispatch is immediate");
	for(const auto page : {Page::zero, Page::one}) {
		expect(pixel(fixture.raster, page, {80, 320}) == 6U &&
			pixel(fixture.raster, page, {95, 320}) == 6U &&
			pixel(fixture.raster, page, {81, 320}) ==
				((page == Page::zero) ? 2U : 3U),
			"regular glyph uses RMW ink on both source pages");
	}
	expect(fixture.raster.access_page() == Page::zero,
		"regular glyph leaves page zero accessed");

	expect(execute(presenter, CutsceneRestoreBox{}, "box restore applies") ==
		CutsceneRasterDispatch::applied,
		"box restore is immediate");
	for(const auto page : {Page::zero, Page::one}) {
		expect(pixel(fixture.raster, page, {80, 320}) == 2U &&
			pixel(fixture.raster, page, {559, 383}) == 2U,
			"page-zero snapshot restores the complete box to both pages");
	}
	expect(fixture.raster.access_page() == Page::zero,
		"box restore leaves page zero accessed");

	expect(execute(presenter, CutsceneReleaseBox{}, "box release applies") ==
		CutsceneRasterDispatch::applied && !presenter.has_box_snapshot(),
		"box release drops saved storage");
	auto missing = presenter.execute(CutsceneOperation{CutsceneRestoreBox{}});
	expect(!missing && missing.error().code ==
		CutsceneRasterErrorCode::box_not_snapshotted &&
		pixel(fixture.raster, Page::zero, {80, 320}) == 2U &&
		pixel(fixture.raster, Page::one, {80, 320}) == 2U,
		"restore without a snapshot rejects atomically");
}

void test_text_gaiji_validation_and_atomicity()
{
	Fixture fixture{};
	expect(fill_page(fixture.raster, Page::zero, 4U) &&
		fill_page(fixture.raster, Page::one, 5U),
		"seed text atomicity pages");
	fixture.font.missing = true;
	auto missing = fixture.presenter->execute(CutsceneOperation{CutsceneDrawGlyph{
		{80, 320}, {0x82U, 0xA0U}, 7U, GraphicsTextWeight::normal,
	}});
	expect(!missing && missing.error().code ==
		CutsceneRasterErrorCode::invalid_text &&
		missing.error().text_error.has_value() &&
		pixel(fixture.raster, Page::zero, {80, 320}) == 4U &&
		pixel(fixture.raster, Page::one, {80, 320}) == 5U,
		"missing late FONT glyph rejects before either page changes");
	fixture.font.missing = false;

	auto invalid = fixture.presenter->execute(CutsceneOperation{CutsceneDrawGlyph{
		{80, 320}, {0x82U, 0xA0U}, 16U, GraphicsTextWeight::normal,
	}});
	expect(!invalid && invalid.error().code ==
		CutsceneRasterErrorCode::invalid_operation &&
		pixel(fixture.raster, Page::zero, {80, 320}) == 4U,
		"invalid glyph color rejects atomically");

	const CutsceneDrawGaiji gaiji{{96, 336}, 26U, 12U};
	expect(execute(*fixture.presenter, gaiji, "gaiji draws") ==
		CutsceneRasterDispatch::applied,
		"gaiji dispatch is immediate");
	for(const auto page : {Page::zero, Page::one}) {
		expect(pixel(fixture.raster, page, {96, 336}) == 12U &&
			pixel(fixture.raster, page, {111, 336}) == 12U &&
			pixel(fixture.raster, page, {97, 336}) ==
				((page == Page::zero) ? 4U : 5U),
			"MIKOFT gaiji uses identical intended glyph on both pages");
	}
	auto bad_gaiji = fixture.presenter->execute(CutsceneOperation{
		CutsceneDrawGaiji{{97, 336}, 256U, 1U}
	});
	expect(!bad_gaiji && bad_gaiji.error().code ==
		CutsceneRasterErrorCode::invalid_operation,
		"invalid gaiji code/alignment rejects");
}

void test_clear_keeps_stale_box_snapshot()
{
	Fixture fixture{};
	expect(fill_page(fixture.raster, Page::zero, 5U) &&
		fill_page(fixture.raster, Page::one, 6U),
		"seed stale-snapshot pages");
	expect(execute(*fixture.presenter, CutsceneSnapshotBox{}, "snapshot seeded box") ==
		CutsceneRasterDispatch::applied,
		"snapshot before clear applies");
	expect(execute(*fixture.presenter, CutsceneClearPages{false}, "clear both pages") ==
		CutsceneRasterDispatch::applied,
		"TH03 clear-pages operation applies");
	expect(pixel(fixture.raster, Page::zero, {0, 0}) == 0U &&
		pixel(fixture.raster, Page::one, {639, 399}) == 0U,
		"clear-pages clears both complete pages");
	expect(execute(*fixture.presenter, CutsceneRestoreBox{}, "restore stale box") ==
		CutsceneRasterDispatch::applied,
		"stale snapshot remains usable after clear");
	for(const auto page : {Page::zero, Page::one}) {
		expect(pixel(fixture.raster, page, {80, 320}) == 5U &&
			pixel(fixture.raster, page, {559, 383}) == 5U &&
			pixel(fixture.raster, page, {79, 320}) == 0U,
			"clear does not resnapshot the TH03 box background");
	}
	auto invalid = fixture.presenter->execute(CutsceneOperation{
		CutsceneClearPages{true}
	});
	expect(!invalid && invalid.error().code ==
		CutsceneRasterErrorCode::invalid_operation &&
		pixel(fixture.raster, Page::zero, {80, 320}) == 5U,
		"non-TH03 resnapshot request rejects atomically");
}

void test_background_ownership_palette_and_present()
{
	Fixture fixture{};
	CutsceneFilename filename{};
	filename.bytes[0U] = 'D';
	filename.length = 1U;
	expect(execute(
		*fixture.presenter,
		CutsceneLoadBackground{filename},
		"load operation requests external content"
	) == CutsceneRasterDispatch::background_required &&
		!fixture.presenter->has_background(),
		"load operation never performs filesystem I/O");

	auto background = make_background();
	const auto expected_palette = *background.palette;
	auto replaced = fixture.presenter->replace_background(std::move(background));
	expect(replaced.has_value() && fixture.presenter->has_background() &&
		fixture.presenter->has_background_palette(),
		"valid 640x400 PI is owned transactionally");
	expect(execute(
		*fixture.presenter,
		CutscenePresentBackground{true},
		"present background"
	) == CutsceneRasterDispatch::applied,
		"palette background presentation applies");
	for(const auto page : {Page::zero, Page::one}) {
		expect(pixel(fixture.raster, page, {10, 10}) == 1U &&
			pixel(fixture.raster, page, {500, 10}) == 2U &&
			pixel(fixture.raster, page, {10, 300}) == 3U &&
			pixel(fixture.raster, page, {500, 300}) == 4U,
			"full PI is copied from page one to page zero exactly");
	}
	expect(fixture.raster.pending_palette().colors[3U].r ==
		static_cast<std::uint8_t>(expected_palette.colors[3U].r * 0x11U) &&
		fixture.raster.applied_palette() == expected_palette &&
		fixture.raster.access_page() == Page::zero &&
		fixture.presenter->has_box_snapshot(),
		"PI palette is applied before draw and text box is resnapshotted");

	expect(fixture.raster.set_access_page(Page::zero) &&
		fixture.raster.clear({80, 320, 480U, 64U}, 9U),
		"mutate box after background presentation");
	expect(execute(*fixture.presenter, CutsceneRestoreBox{}, "restore PI box") ==
		CutsceneRasterDispatch::applied &&
		pixel(fixture.raster, Page::zero, {80, 320}) == 3U &&
		pixel(fixture.raster, Page::one, {80, 320}) == 3U,
		"background presentation snapshot restores to both pages");

	PiImage malformed = make_background();
	malformed.image.width = 639U;
	auto bad_replace = fixture.presenter->replace_background(std::move(malformed));
	expect(!bad_replace && bad_replace.error().code ==
		CutsceneRasterErrorCode::invalid_background &&
		fixture.presenter->has_background(),
		"malformed replacement preserves prior owned PI");
	expect(execute(
		*fixture.presenter,
		CutsceneApplyBackgroundPalette{},
		"old palette remains usable"
	) == CutsceneRasterDispatch::applied &&
		fixture.raster.applied_palette() == expected_palette,
		"late malformed replacement is atomic");

	expect(execute(*fixture.presenter, CutsceneFreeBackground{}, "free background") ==
		CutsceneRasterDispatch::applied && !fixture.presenter->has_background(),
		"PI release drops owned background only");
	auto missing = fixture.presenter->execute(CutsceneOperation{
		CutscenePresentBackground{true}
	});
	expect(!missing && missing.error().code ==
		CutsceneRasterErrorCode::missing_background,
		"presentation after PI release is rejected");

	auto palette_less = fixture.presenter->replace_background(make_background(false));
	expect(palette_less.has_value() && fixture.presenter->has_background() &&
		!fixture.presenter->has_background_palette(),
		"palette-less PI remains valid for p-at presentation");
	expect(execute(
		*fixture.presenter,
		CutscenePresentBackground{false},
		"palette-less present"
	) == CutsceneRasterDispatch::applied,
		"p-at presents without palette application");
	auto no_palette = fixture.presenter->execute(CutsceneOperation{
		CutsceneApplyBackgroundPalette{}
	});
	expect(!no_palette && no_palette.error().code ==
		CutsceneRasterErrorCode::missing_background_palette,
		"palette-only command rejects palette-less PI");
}

void test_quarter_crossfade_and_page_state()
{
	Fixture fixture{};
	expect(fixture.presenter->replace_background(make_background()).has_value(),
		"quarter fixture owns background");
	expect(fill_page(fixture.raster, Page::zero, 9U) &&
		fill_page(fixture.raster, Page::one, 10U),
		"quarter fixture seeds distinct pages");
	expect(execute(
		*fixture.presenter,
		CutsceneShowPictureQuarter{2U},
		"show bottom-left quarter"
	) == CutsceneRasterDispatch::applied,
		"immediate quarter presentation applies");
	for(const auto page : {Page::zero, Page::one}) {
		expect(pixel(fixture.raster, page, {160, 64}) == 3U &&
			pixel(fixture.raster, page, {479, 263}) == 3U &&
			pixel(fixture.raster, page, {159, 64}) ==
				((page == Page::zero) ? 9U : 10U),
			"quarter draw and inter-page copy touch exact fixed rectangle");
	}
	expect(fixture.raster.shown_page() == Page::zero &&
		fixture.raster.access_page() == Page::zero,
		"immediate quarter restores page zero shown/accessed");
	expect(execute(*fixture.presenter, CutsceneClearPicture{}, "clear picture") ==
		CutsceneRasterDispatch::applied,
		"picture clear applies");
	for(const auto page : {Page::zero, Page::one}) {
		expect(pixel(fixture.raster, page, {160, 64}) == 0U &&
			pixel(fixture.raster, page, {479, 263}) == 0U,
			"picture clear reaches both pages");
	}

	const CutsceneCrossfadePicture crossfade{0U, 2U, true};
	expect(execute(*fixture.presenter, crossfade, "crossfade dispatch") ==
		CutsceneRasterDispatch::crossfade_required,
		"crossfade exposes caller-owned mask timing");
	auto first = fixture.presenter->crossfade_step(
		crossfade, CutsceneCrossfadeStep::mask_first
	);
	expect(first.has_value() &&
		pixel(fixture.raster, Page::zero, {160, 64}) == 0U &&
		pixel(fixture.raster, Page::zero, {163, 65}) == 1U &&
		pixel(fixture.raster, Page::one, {163, 65}) == 1U,
		"first cumulative mask is absolute-MSB phased and copied to page one");
	for(const auto step : {
		CutsceneCrossfadeStep::mask_second,
		CutsceneCrossfadeStep::mask_third,
		CutsceneCrossfadeStep::mask_fourth,
		CutsceneCrossfadeStep::complete,
	}) {
		expect(fixture.presenter->crossfade_step(crossfade, step).has_value(),
			"remaining crossfade step applies");
	}
	for(const auto page : {Page::zero, Page::one}) {
		expect(pixel(fixture.raster, page, {160, 64}) == 1U &&
			pixel(fixture.raster, page, {479, 263}) == 1U,
			"final unmasked crossfade completes both pages");
	}
	expect(fixture.raster.shown_page() == Page::zero &&
		fixture.raster.access_page() == Page::zero,
		"every crossfade step restores page zero shown/accessed");

	const auto before = pixel(fixture.raster, Page::zero, {160, 64});
	auto invalid_step = fixture.presenter->crossfade_step(
		crossfade, static_cast<CutsceneCrossfadeStep>(0xFFU)
	);
	expect(!invalid_step && invalid_step.error().code ==
		CutsceneRasterErrorCode::invalid_crossfade_step &&
		pixel(fixture.raster, Page::zero, {160, 64}) == before,
		"invalid crossfade step rejects before page mutation");
	auto invalid_quarter = fixture.presenter->execute(CutsceneOperation{
		CutsceneCrossfadePicture{4U, 1U, true}
	});
	expect(!invalid_quarter && invalid_quarter.error().code ==
		CutsceneRasterErrorCode::invalid_picture_quarter,
		"bounded port rejects source-invalid crossfade quarter four");
}

void test_shake_fades_and_dispatch_domains()
{
	Fixture fixture{};
	const CutsceneShake shake{2U, true};
	expect(execute(*fixture.presenter, shake, "shake dispatch") ==
		CutsceneRasterDispatch::shake_required,
		"shake exposes caller-owned per-position timing");
	expect(fixture.presenter->shake_step(shake, 0U).has_value() &&
		fixture.raster.scroll_y() == 396U,
		"even shake position uses source 396-line scroll");
	expect(fixture.presenter->shake_step(shake, 1U).has_value() &&
		fixture.raster.scroll_y() == 4U,
		"odd shake position uses source four-line scroll");
	const auto before_scroll = fixture.raster.scroll_y();
	auto invalid_shake = fixture.presenter->shake_step(shake, 3U);
	expect(!invalid_shake && invalid_shake.error().code ==
		CutsceneRasterErrorCode::invalid_shake_step &&
		fixture.raster.scroll_y() == before_scroll,
		"shake rejects an index beyond the inclusive source loop");
	fixture.presenter->finish_shake();
	expect(fixture.raster.scroll_y() == 0U,
		"shake completion resets graph scroll");

	const std::array fades{
		CutscenePaletteFade{CutsceneFadeColor::black, CutsceneFadeDirection::in, 1U},
		CutscenePaletteFade{CutsceneFadeColor::black, CutsceneFadeDirection::out, 2U},
		CutscenePaletteFade{CutsceneFadeColor::white, CutsceneFadeDirection::in, 3U},
		CutscenePaletteFade{CutsceneFadeColor::white, CutsceneFadeDirection::out, 0U},
	};
	const std::array<std::array<std::int16_t, 3U>, 4U> expected{{
		{{0, 96, 100}},
		{{100, 4, 0}},
		{{200, 104, 100}},
		{{100, 196, 200}},
	}};
	for(std::size_t index = 0U; index < fades.size(); ++index) {
		expect(execute(*fixture.presenter, fades[index], "fade dispatch") ==
			CutsceneRasterDispatch::palette_fade_required,
			"palette fade exposes caller-owned alignment/speed waits");
		expect(fixture.presenter->palette_fade_sample(fades[index], 0U).has_value() &&
			fixture.raster.palette_tone() == expected[index][0U],
			"palette fade first loop sample matches master.lib");
		expect(fixture.presenter->palette_fade_sample(fades[index], 16U).has_value() &&
			fixture.raster.palette_tone() == expected[index][1U],
			"palette fade last loop sample matches master.lib");
		expect(fixture.presenter->palette_fade_sample(fades[index], 17U).has_value() &&
			fixture.raster.palette_tone() == expected[index][2U],
			"palette fade exact endpoint matches master.lib");
	}
	const auto before_tone = fixture.raster.palette_tone();
	auto invalid_fade = fixture.presenter->palette_fade_sample(fades[0U], 18U);
	expect(!invalid_fade && invalid_fade.error().code ==
		CutsceneRasterErrorCode::invalid_palette_fade_sample &&
		fixture.raster.palette_tone() == before_tone,
		"out-of-range fade sample rejects atomically");

	expect(execute(
		*fixture.presenter,
		CutsceneWait{CutsceneWaitKind::frames, 1U, 0U},
		"frame wait dispatch"
	) == CutsceneRasterDispatch::external,
		"wait remains a scene-driver operation");
	expect(execute(*fixture.presenter, CutsceneAudioFade{8U}, "audio fade dispatch") ==
		CutsceneRasterDispatch::external &&
		execute(*fixture.presenter, CutsceneBgm{}, "BGM dispatch") ==
			CutsceneRasterDispatch::external &&
		execute(*fixture.presenter, CutscenePlaySoundEffect{3U}, "SE dispatch") ==
			CutsceneRasterDispatch::external &&
		execute(*fixture.presenter, CutsceneFinish{}, "finish dispatch") ==
			CutsceneRasterDispatch::external,
		"audio and route completion remain outside presentation");
	expect(execute(*fixture.presenter, CutsceneSetTextColor{7U}, "color state") ==
		CutsceneRasterDispatch::applied &&
		execute(*fixture.presenter, CutsceneSetTextWeight{GraphicsTextWeight::bold},
			"weight state") == CutsceneRasterDispatch::applied &&
		execute(*fixture.presenter, CutsceneSetTextInterval{16U}, "interval state") ==
			CutsceneRasterDispatch::applied,
		"VM-carried text state operations need no duplicate presenter state");
	expect(execute(*fixture.presenter, CutsceneShowPage{1U}, "show page one") ==
		CutsceneRasterDispatch::applied &&
		fixture.raster.shown_page() == Page::one,
		"stock-unused show-page operation preserves two-page semantics");
	expect(execute(*fixture.presenter, CutsceneSetTone{170U}, "set tone") ==
		CutsceneRasterDispatch::applied &&
		fixture.raster.palette_tone() == 170,
		"tone operation applies pending palette immediately");
	auto bad_page = fixture.presenter->execute(CutsceneOperation{CutsceneShowPage{2U}});
	expect(!bad_page && bad_page.error().code ==
		CutsceneRasterErrorCode::invalid_operation &&
		fixture.raster.shown_page() == Page::one,
		"invalid shown page rejects before mutation");
	const auto invalid_weight = static_cast<GraphicsTextWeight>(0xFFU);
	auto bad_weight = fixture.presenter->execute(CutsceneOperation{
		CutsceneSetTextWeight{invalid_weight}
	});
	expect(!bad_weight && bad_weight.error().code ==
		CutsceneRasterErrorCode::invalid_operation,
		"invalid text-weight enum rejects");
}

} // namespace

int main()
{
	test_factory_and_two_page_text_box_lifecycle();
	test_text_gaiji_validation_and_atomicity();
	test_clear_keeps_stale_box_snapshot();
	test_background_ownership_palette_and_present();
	test_quarter_crossfade_and_page_state();
	test_shake_fades_and_dispatch_domains();

	if(failures != 0) {
		std::cerr << failures << " / " << checks
			<< " cutscene Raster checks failed\n";
		return 1;
	}
	std::cout << "cutscene Raster checks: " << checks << '\n';
	return 0;
}
