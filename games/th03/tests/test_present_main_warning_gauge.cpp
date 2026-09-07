#include "th03/present/main_warning_gauge.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

#include "th03/present/main_static.hpp"

namespace {

using th03::GaugeAttackFinished;
using th03::GaugeBossAttackKind;
using th03::MainGaugePlayerState;
using th03::MainGaugeState;
using th03::MainPhaseContext;
using th03::PlayerId;
using th03::WarningFrameLifecycle;
using th03::WarningInterstitialSample;
using th03::WarningInterstitialStarted;
using th03::WarningPlayerLifecycle;
using th03::WarningPortraitTransition;
using th03::assets::MrsImage;
using th03::assets::MrsOrientation;
using th03::present::Index4;
using th03::present::MainPaletteState;
using th03::present::MainWarningGaugePresenter;
using th03::present::MainWarningGaugeStatus;
using th03::present::MainWarningMrsAssets;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::Rgb8;
using th03::present::Sprite16Staging;
using th03::present::Tram;
using th03::present::TramAttribute;
using th03::present::TramPoint;
using th03::present::gaiji_cell_code;
using th03::present::tram_black;
using th03::present::tram_cyan;
using th03::present::tram_white;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(
			stderr,
			"FAIL: %.*s\n",
			static_cast<int>(message.size()),
			message.data()
		);
	}
}

MrsImage make_mrs(Index4 color, MrsOrientation orientation)
{
	constexpr std::uint16_t width = 288U;
	constexpr std::uint16_t height = 184U;
	constexpr std::size_t plane_size = (width / 8U) * height;
	MrsImage image{};
	image.cel.pixel_width = width;
	image.cel.pixel_height = height;
	image.cel.bitplane_size = plane_size;
	image.cel.offset_at_bottom_left = 183 * 80;
	image.cel.vram_dword_width = width / 32U;
	image.cel.row_order = th03::assets::PlanarRowOrder::bottom_to_top;
	image.cel.bit_order =
		th03::assets::PlanarBitOrder::most_significant_bit_left;
	image.cel.alpha = std::vector<std::uint8_t>(plane_size, 0xFFU);
	for(std::size_t plane = 0U; plane < image.cel.color_planes.size(); ++plane) {
		image.cel.color_planes[plane] = std::vector<std::uint8_t>(
			plane_size,
			((color & (1U << plane)) != 0U) ? 0xFFU : 0x00U
		);
	}
	image.orientation = orientation;
	return image;
}

void prepare_raster(Raster &raster, Sprite16Staging &staging)
{
	for(std::int32_t y = 0; y < 8; ++y) {
		for(std::int32_t x = 0; x < 288; ++x) {
			expect(
				raster.plot(
					{320 + x, 392 + y},
					static_cast<Index4>((x / 16) & 0x0F)
				),
				"seed hidden gauge atlas"
			);
		}
	}
	expect(staging.commit(raster), "commit hidden atlas");
}

void prepare_palette(Raster &raster, MainPaletteState &palette)
{
	expect(
		raster.set_pending_palette_entry_rgb8(0U, {16U, 32U, 48U}),
		"seed P1 baseline"
	);
	expect(
		raster.set_pending_palette_entry_rgb8(1U, {64U, 80U, 96U}),
		"seed P2 baseline"
	);
	expect(palette.capture_baseline(raster), "capture process palette baseline");
	palette.show_now(raster);
}

void expect_gaiji(
	const Tram &tram,
	TramPoint point,
	std::uint8_t glyph,
	TramAttribute attribute,
	std::string_view message
)
{
	const auto cell = tram.cell(point);
	expect(
		cell.has_value() &&
		cell->code == gaiji_cell_code(glyph, th03::present::GaijiHalf::left) &&
		cell->attribute == attribute,
		message
	);
}

WarningFrameLifecycle idle_lifecycle() noexcept
{
	return {
		.players = {{
			{0U, 0U, GaugeBossAttackKind::none, GaugeBossAttackKind::none},
			{0U, 0U, GaugeBossAttackKind::none, GaugeBossAttackKind::none},
		}},
		.gauge_levels = {1U, 1U},
		.boss_level = 0U,
	};
}

void test_constructor_validation()
{
	Raster raster;
	Tram tram;
	Sprite16Staging staging;
	MainPaletteState palette;
	prepare_raster(raster, staging);
	prepare_palette(raster, palette);
	auto p1 = make_mrs(1U, MrsOrientation::as_stored);
	auto p2 = make_mrs(2U, MrsOrientation::horizontally_flipped);
	MainWarningGaugePresenter ready(
		raster, tram, staging, palette, {{{&p1, &p2}}}
	);
	expect(ready.ready(), "valid warning/gauge assets are accepted");

	Raster bad_raster;
	Tram bad_tram;
	Sprite16Staging bad_staging;
	MainPaletteState bad_palette;
	prepare_raster(bad_raster, bad_staging);
	prepare_palette(bad_raster, bad_palette);
	p2.orientation = MrsOrientation::as_stored;
	MainWarningGaugePresenter bad(
		bad_raster, bad_tram, bad_staging, bad_palette, {{{&p1, &p2}}}
	);
	expect(
		bad.status() == MainWarningGaugeStatus::invalid_assets,
		"P2 EX.MRS must retain its source horizontal flip"
	);
}

void test_warning_text_tones_transitions_and_ramp()
{
	Raster raster;
	Tram tram;
	Sprite16Staging staging;
	MainPaletteState palette;
	prepare_raster(raster, staging);
	prepare_palette(raster, palette);
	auto p1 = make_mrs(1U, MrsOrientation::as_stored);
	auto p2 = make_mrs(2U, MrsOrientation::horizontally_flipped);
	MainWarningGaugePresenter presenter(
		raster, tram, staging, palette, {{{&p1, &p2}}}
	);
	expect(
		palette.set_entry_deferred(raster, 1U, {192U, 176U, 160U}),
		"seed dirty opponent palette"
	);
	const auto access_before = raster.access_page();
	const auto shown_before = raster.shown_page();
	presenter.warning_interstitial_started({
		.flashing_players = {true, false},
		.next_attacks = {
			GaugeBossAttackKind::gauge_pellet,
			GaugeBossAttackKind::none,
		},
		.gauge_levels = {3U, 7U},
		.boss_level = 9U,
	});
	expect(presenter.ready(), "accepted warning start remains ready");
	expect_gaiji(tram, {44, 11}, 0x50U, tram_white, "warning row starts at ID 50");
	expect_gaiji(tram, {50, 15}, 0xC7U, tram_white, "gauge label uses ID C7");
	expect_gaiji(tram, {67, 15}, 0x22U, tram_white, "warning level uses level+1F");
	expect_gaiji(tram, {2, 24}, 0x22U, th03::present::tram_green,
		"warning start redraws P1 static gauge level");
	expect_gaiji(tram, {42, 24}, 0x26U, th03::present::tram_green,
		"warning start redraws P2 static gauge level");

	for(std::uint8_t ordinal = 0U; ordinal < 27U; ++ordinal) {
		presenter.warning_interstitial_sample({ordinal});
		expect(
			raster.palette_tone() ==
				static_cast<std::int16_t>(100 + (50 * (ordinal & 1U))),
			"warning sample immediately applies alternating tone"
		);
	}
	expect(raster.palette_tone() == 100, "warning sample 26 ends at tone 100");
	expect(palette.dirty(), "immediate warning shows preserve deferred dirty latch");
	expect(
		raster.access_page() == access_before && raster.shown_page() == shown_before,
		"blocking warning samples never publish a page"
	);

	auto lifecycle = idle_lifecycle();
	lifecycle.players[0U] = {
		3U, 4U, GaugeBossAttackKind::gauge_pellet, GaugeBossAttackKind::none,
	};
	presenter.warning_frame_lifecycle(lifecycle);
	expect(
		raster.pending_palette().colors[1U] == Rgb8{192U, 176U, 120U},
		"even pellet warning changes only opponent blue to 120"
	);
	expect_gaiji(tram, {44, 11}, 0x50U, tram_black,
		"even warning step redraws text black");

	lifecycle.players[0U] = {
		4U, 5U, GaugeBossAttackKind::gauge_pellet, GaugeBossAttackKind::none,
	};
	presenter.warning_frame_lifecycle(lifecycle);
	expect(
		raster.pending_palette().colors[1U] == Rgb8{64U, 80U, 96U},
		"odd warning step restores the process baseline opponent entry"
	);
	expect_gaiji(tram, {44, 11}, 0x50U, tram_cyan,
		"odd pellet warning step redraws text cyan");

	lifecycle.players[0U] = {
		33U,
		0U,
		GaugeBossAttackKind::gauge_pellet,
		GaugeBossAttackKind::gauge_pellet,
	};
	presenter.warning_frame_lifecycle(lifecycle);
	const auto cleared = tram.cell({44, 11});
	expect(
		cleared.has_value() && cleared->code == ' ' &&
		cleared->attribute == tram_white,
		"warning completion clears all text with white ASCII spaces"
	);
	expect(
		raster.pending_palette().colors[1U] == Rgb8{4U, 80U, 96U},
		"warning completion restores then starts active red ramp at 4"
	);

	lifecycle.players[0U] = {
		0U,
		0U,
		GaugeBossAttackKind::gauge_pellet,
		GaugeBossAttackKind::gauge_pellet,
	};
	for(std::size_t step = 0U; step < 35U; ++step) {
		presenter.warning_frame_lifecycle(lifecycle);
	}
	expect(
		raster.pending_palette().colors[1U].r == 144U,
		"active warning ramp reaches red 144"
	);
	presenter.warning_frame_lifecycle(lifecycle);
	expect(
		raster.pending_palette().colors[1U].r == 140U,
		"active warning ramp reverses after red 144"
	);

	presenter.warning_interstitial_started({
		.flashing_players = {true, false},
		.next_attacks = {
			GaugeBossAttackKind::boss,
			GaugeBossAttackKind::none,
		},
		.gauge_levels = {3U, 7U},
		.boss_level = 9U,
	});
	for(std::uint8_t ordinal = 0U; ordinal < 27U; ++ordinal) {
		presenter.warning_interstitial_sample({ordinal});
	}
	lifecycle = idle_lifecycle();
	lifecycle.players[0U] = {
		3U, 4U, GaugeBossAttackKind::boss,
		GaugeBossAttackKind::gauge_pellet,
	};
	lifecycle.gauge_levels = {3U, 7U};
	lifecycle.boss_level = 10U;
	presenter.warning_frame_lifecycle(lifecycle);
	expect(
		raster.pending_palette().colors[1U].r == 9U,
		"red warning +13 precedes a retained decreasing active ramp"
	);
	expect_gaiji(
		tram, {67, 15}, 0x29U, tram_white,
		"red warning redraw reads the live boss level"
	);
	expect(presenter.ready(), "warning lifecycle remains valid");
}

void test_completion_portraits_and_gauge()
{
	Raster raster;
	Tram tram;
	Sprite16Staging staging;
	MainPaletteState palette;
	prepare_raster(raster, staging);
	prepare_palette(raster, palette);
	auto p1 = make_mrs(3U, MrsOrientation::as_stored);
	auto p2 = make_mrs(5U, MrsOrientation::horizontally_flipped);
	MainWarningGaugePresenter presenter(
		raster, tram, staging, palette, {{{&p1, &p2}}}
	);
	expect(
		palette.set_entry_deferred(raster, 1U, {1U, 2U, 3U}),
		"mutate target before gauge completion"
	);
	presenter.gauge_attack_finished({PlayerId::p1, PlayerId::p2});
	expect(
		raster.pending_palette().colors[1U] == Rgb8{64U, 80U, 96U},
		"gauge completion restores the target baseline entry"
	);

	presenter.warning_portrait_transition({PlayerId::p1});
	presenter.warning_portrait_transition({PlayerId::p2});
	expect(raster.pixel(Page::zero, {16, 8}) == 3U,
		"P1 EX.MRS draws at screen (16,16)");
	expect(raster.pixel(Page::zero, {336, 8}) == 5U,
		"P2 flipped EX.MRS draws at screen (336,16)");

	const MainGaugeState state{
		.context = MainPhaseContext{
			.round_id = 2U,
			.round_or_result_frame = 8U,
		},
		.players = {{
			MainGaugePlayerState{63U << 4U, 64U << 4U, 0U},
			MainGaugePlayerState{128U << 4U, 192U << 4U, 1U},
		}},
	};
	presenter.main_gauge_state(state);
	expect(raster.pixel(Page::zero, {16, 192}) == 0U,
		"gauge background copies hidden source first word");
	expect(raster.pixel(Page::zero, {303, 192}) == 1U,
		"gauge background copies hidden source through x607");
	expect(raster.pixel(Page::zero, {608, 192}) == 1U,
		"P2 gauge edge repeats the hidden source's final word");
	expect(raster.pixel(Page::zero, {352, 192}) == 1U,
		"P2 gauge backdrop mirrors every hidden source word");
	expect(raster.pixel(Page::zero, {32, 194}) == 15U,
		"charged width 63 uses white threshold color");
	expect(raster.pixel(Page::zero, {95, 194}) == 9U,
		"uncharged P1 span follows charged span");
	expect(raster.pixel(Page::zero, {352, 194}) == 8U,
		"charged width 128 uses source color 8");
	expect(raster.pixel(Page::zero, {480, 194}) == 15U,
		"even hyper frame draws the uncharged span white");

	auto odd = state;
	odd.context.round_or_result_frame = 9U;
	odd.players[1U].gauge_avail = 255U << 4U;
	presenter.main_gauge_state(odd);
	expect(raster.pixel(Page::zero, {336, 192}) == 6U,
		"odd hyper frame fills its complete strip by gauge tier");
	expect(raster.pixel(Page::zero, {480, 194}) == 9U,
		"odd hyper uncharged span uses color 9 over the strip");
	expect(presenter.ready(), "portrait/gauge transaction remains valid");
}

void test_invalid_gauge_is_atomic()
{
	Raster raster;
	Tram tram;
	Sprite16Staging staging;
	MainPaletteState palette;
	prepare_raster(raster, staging);
	prepare_palette(raster, palette);
	auto p1 = make_mrs(1U, MrsOrientation::as_stored);
	auto p2 = make_mrs(2U, MrsOrientation::horizontally_flipped);
	MainWarningGaugePresenter presenter(
		raster, tram, staging, palette, {{{&p1, &p2}}}
	);
	const auto before = raster.display_state_hash();
	presenter.main_gauge_state({
		.context = {},
		.players = {{
			{0x1000U, 0x1000U, 0U},
			{0U, 0x0400U, 0U},
		}},
	});
	expect(
		presenter.status() == MainWarningGaugeStatus::invalid_observation,
		"gauge values outside GAUGE_MAX fail closed"
	);
	expect(
		raster.display_state_hash() == before,
		"invalid gauge fact is raster-atomic"
	);
}

void test_zero_sample_arrange_warning_continues_immediately()
{
	Raster raster;
	Tram tram;
	Sprite16Staging staging;
	MainPaletteState palette;
	prepare_raster(raster, staging);
	prepare_palette(raster, palette);
	auto p1 = make_mrs(1U, MrsOrientation::as_stored);
	auto p2 = make_mrs(2U, MrsOrientation::horizontally_flipped);
	MainWarningGaugePresenter presenter(
		raster, tram, staging, palette, {{{&p1, &p2}}}
	);
	presenter.warning_interstitial_started({
		.flashing_players = {true, false},
		.next_attacks = {
			GaugeBossAttackKind::gauge_bullet,
			GaugeBossAttackKind::none,
		},
		.gauge_levels = {3U, 7U},
		.boss_level = 9U,
		.sample_count = 0U,
	});
	for(std::uint8_t frame = 0U; frame < 5U; ++frame) {
		expect(presenter.compose_nonblocking_interstitial_frame(),
			"zero-hold Arrange cut-in composes over a live gameplay frame");
		expect(raster.palette_tone() ==
			static_cast<std::int16_t>(100 + (50 * (frame & 1U))),
			"zero-hold Arrange cut-in retains the complete alternating tone sequence");
	}
	expect(presenter.compose_nonblocking_interstitial_frame() &&
		raster.palette_tone() == 100,
		"zero-hold Arrange cut-in ends after exactly five visual frames");
	auto lifecycle = idle_lifecycle();
	lifecycle.players[0U] = {
		3U, 4U, GaugeBossAttackKind::gauge_bullet, GaugeBossAttackKind::none,
	};
	lifecycle.gauge_levels = {3U, 7U};
	lifecycle.boss_level = 9U;
	presenter.warning_frame_lifecycle(lifecycle);
	expect(
		presenter.ready(),
		"zero-sample Arrange warning draws the cut-in and accepts the same-frame warning lifecycle"
	);
}

} // namespace

int main()
{
	test_constructor_validation();
	test_warning_text_tones_transitions_and_ramp();
	test_completion_portraits_and_gauge();
	test_invalid_gauge_is_atomic();
	test_zero_sample_arrange_warning_continues_immediately();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return 1;
	}
	std::printf("All %zu MAIN warning/gauge checks passed\n", checks);
	return 0;
}
