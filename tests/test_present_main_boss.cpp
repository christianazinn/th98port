#include "th03/present/main_boss.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>

#include "th03/present/main_static.hpp"

namespace {

using th03::BossActivated;
using th03::BossAnimationStep;
using th03::BossPaletteBaselineRestored;
using th03::MainBossFact;
using th03::MainBossView;
using th03::MainPhaseContext;
using th03::PlayerId;
using th03::RawPoint;
using th03::present::Index4;
using th03::present::MainBossPresenter;
using th03::present::MainBossStatus;
using th03::present::MainPaletteState;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::RasterMode;
using th03::present::Rgb8;
using th03::present::Sprite16AtlasRect;
using th03::present::Sprite16Staging;
using th03::present::StaffFlakeTrigTables;
using th03::present::sprite16_source_top;

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

void expect_pixel(
	const Raster &raster,
	Point point,
	Index4 color,
	std::string_view message
)
{
	expect(raster.pixel(Page::zero, point) == color, message);
}

constexpr std::int8_t paletted(std::uint8_t character) noexcept
{
	return static_cast<std::int8_t>(character * 2U + 1U);
}

constexpr PlayerId player_id(std::uint8_t player) noexcept
{
	return static_cast<PlayerId>(player);
}

constexpr std::array<std::uint16_t, 9U> base_left{{
	96U, 96U, 0U, 0U, 64U, 64U, 48U, 64U, 64U,
}};
constexpr std::array<std::uint16_t, 9U> base_top{{
	8U, 8U, 8U, 24U, 8U, 8U, 8U, 8U, 8U,
}};
constexpr std::array<std::uint16_t, 9U> body_width{{
	96U, 144U, 176U, 64U, 128U, 128U, 96U, 128U, 112U,
}};
constexpr std::array<std::uint16_t, 9U> body_height{{
	48U, 56U, 48U, 48U, 48U, 40U, 48U, 64U, 56U,
}};
constexpr std::array<std::int16_t, 9U> left_radius{{
	48, 72, 88, 32, 64, 64, 48, 64, 56,
}};
constexpr std::array<std::int16_t, 9U> top_radius{{
	32, 40, 32, 32, 32, 24, 32, 48, 40,
}};

struct TestTrig {
	std::array<std::int16_t, 256U> sine{};
	std::array<std::int16_t, 256U> cosine{};

	TestTrig()
	{
		cosine[0U] = 256;
		sine[0x40U] = 256;
		cosine[0x80U] = -256;
		sine[0xC0U] = -256;
	}

	[[nodiscard]] StaffFlakeTrigTables view() const noexcept
	{
		return {sine, cosine};
	}
};

struct Fixture {
	Raster raster;
	Sprite16Staging staging;
	TestTrig trig;
	MainPaletteState palette;
	std::optional<MainBossPresenter> presenter;

	void seed(Sprite16AtlasRect rect, Index4 color)
	{
		expect(
			raster.clear(
				{
					static_cast<std::int32_t>(rect.left),
					static_cast<std::int32_t>(sprite16_source_top + rect.raw_top),
					rect.width,
					rect.raw_height,
				},
				color
			),
			"seed Boss atlas rectangle"
		);
	}

	void prepare(bool commit = true)
	{
		if(commit) {
			expect(staging.commit(raster), "commit Boss atlas");
		}
		expect(
			raster.set_raster_mode(RasterMode::zoomed_200_upper),
			"select MAIN raster mode"
		);
		expect(palette.capture_baseline(raster), "capture MAIN palette baseline");
		presenter.emplace(raster, staging, trig.view(), palette);
	}

	void activate(std::uint8_t character, std::uint8_t owner = 0U)
	{
		presenter->boss_activated({player_id(owner), paletted(character)});
	}

	[[nodiscard]] bool clear_output()
	{
		return raster.clear({0, 0, 640U, 200U}, 0U);
	}
};

[[nodiscard]] Sprite16AtlasRect body_rect(
	std::uint8_t character,
	std::uint8_t owner = 0U
) noexcept
{
	return {
		static_cast<std::uint16_t>(owner * 320U + base_left[character]),
		base_top[character],
		body_width[character],
		body_height[character],
	};
}

[[nodiscard]] MainBossFact fact(
	std::uint8_t character,
	std::uint8_t owner = 0U
) noexcept
{
	return {
		.owner = player_id(owner),
		.paletted_character = paletted(character),
		.center_x = 144 << 4,
		.center_y = 80 << 4,
		.state = 1U,
	};
}

[[nodiscard]] MainPhaseContext context(
	std::int16_t p1_shift = 0,
	std::int16_t p2_shift = 0,
	std::uint16_t frame = 0U
) noexcept
{
	return {
		.round_or_result_frame = frame,
		.playfield_fg_shift_x = {p1_shift, p2_shift},
	};
}

constexpr std::array<RawPoint, MainBossView::reimu_orbit_point_count>
	empty_points{};

void test_all_character_bodies_and_hits()
{
	struct HitSource {
		std::uint16_t left;
		std::uint16_t top;
		std::uint16_t width;
		std::uint16_t height;
		std::int16_t destination_x;
		std::int16_t destination_y;
		bool replaces_body;
	};
	constexpr std::array<HitSource, 9U> hits{{
		{192U, 8U, 96U, 48U, 0, 0, true},
		{240U, 8U, 64U, 40U, 32, 16, false},
		{176U, 8U, 48U, 40U, 64, 16, false},
		{64U, 24U, 64U, 48U, 0, 0, true},
		{192U, 8U, 128U, 48U, 0, 0, true},
		{192U, 8U, 128U, 40U, 0, 0, true},
		{144U, 8U, 96U, 48U, 0, 0, true},
		{0U, 32U, 48U, 40U, 48, 32, false},
		{240U, 8U, 80U, 48U, 16, 0, false},
	}};

	for(std::uint8_t character = 0U; character < 9U; ++character) {
		Fixture fixture;
		fixture.seed(body_rect(character), 2U);
		fixture.seed(
			{
				hits[character].left,
				hits[character].top,
				hits[character].width,
				hits[character].height,
			},
			3U
		);
		fixture.prepare();
		fixture.activate(character);
		auto boss = fact(character);
		const auto left = static_cast<std::int32_t>(
			480 - left_radius[character]
		);
		const auto raw_top = static_cast<std::int32_t>(
			(80 - top_radius[character]) / 2
		);
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw one active Boss body"
		);
		expect_pixel(
			fixture.raster,
			{left, raw_top},
			2U,
			"every character uses its exact body source"
		);
		expect(fixture.clear_output(), "clear between body and hit draw");
		boss.hit = 1U;
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw one hit Boss body"
		);
		expect_pixel(
			fixture.raster,
			{
				left + hits[character].destination_x,
				raw_top + (hits[character].destination_y / 2),
			},
			3U,
			"every character uses its exact hit source"
		);
	}
}

void test_marisa_and_mima_animation_boundaries()
{
	{
		Fixture fixture;
		fixture.seed(body_rect(2U), 2U);
		fixture.seed({224U, 8U, 48U, 40U}, 3U);
		fixture.seed({0U, 56U, 112U, 8U}, 4U);
		fixture.seed({0U, 64U, 112U, 8U}, 5U);
		fixture.prepare();
		fixture.activate(2U);
		auto boss = fact(2U);
		boss.render_state = 1U;
		expect(
			fixture.presenter->present_boss(
				context(0, 0, 0U), false, boss, empty_points
			),
			"draw Marisa first animation half"
		);
		expect_pixel(fixture.raster, {456, 32}, 3U, "Marisa flash cel");
		expect_pixel(fixture.raster, {424, 48}, 4U, "Marisa first lower cel");
		expect(fixture.clear_output(), "clear Marisa animation");
		expect(
			fixture.presenter->present_boss(
				context(0, 0, 2U), false, boss, empty_points
			),
			"draw Marisa second animation half"
		);
		expect_pixel(fixture.raster, {424, 48}, 5U, "Marisa second lower cel");
	}

	{
		Fixture fixture;
		fixture.seed(body_rect(1U), 2U);
		fixture.seed({304U, 8U, 16U, 8U}, 3U);
		fixture.seed({0U, 56U, 32U, 16U}, 4U);
		fixture.seed({32U, 56U, 32U, 16U}, 5U);
		fixture.seed({240U, 48U, 80U, 24U}, 6U);
		fixture.prepare();
		fixture.activate(1U);
		auto boss = fact(1U);
		boss.render_state = 2U;
		for(const auto boundary : std::array<std::uint8_t, 4U>{{15U, 16U, 31U, 32U}}) {
			expect(fixture.clear_output(), "clear Mima boundary frame");
			boss.render_frame = boundary;
			expect(
				fixture.presenter->present_boss(
					context(), false, boss, empty_points
				),
				"draw Mima boundary frame"
			);
			expect_pixel(
				fixture.raster,
				{488, 20},
				(boundary < 16U) ? 4U : 5U,
				"Mima 15/16/31/32 side-cel boundary"
			);
		}
		expect(fixture.clear_output(), "clear Mima state-5 frame");
		boss.render_state = 5U;
		boss.render_frame = 99U;
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw Mima state 5"
		);
		expect_pixel(
			fixture.raster, {488, 20}, 4U,
			"Mima state 5 resets the source frame before drawing"
		);
		expect(fixture.clear_output(), "clear Mima state-3 frame");
		boss.render_state = 3U;
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw Mima state 3"
		);
		expect_pixel(fixture.raster, {440, 28}, 6U, "Mima state-3 full cel");
	}
}

void test_reimu_kotohime_kana_material_and_overlays()
{
	{
		Fixture fixture;
		fixture.seed({0U, 8U, 48U, 24U}, 3U);
		fixture.seed({48U, 8U, 48U, 24U}, 4U);
		fixture.prepare();
		fixture.activate(0U);
		auto boss = fact(0U);
		boss.state = 0U;
		boss.pattern_frame = 0U;
		expect(
			fixture.presenter->present_boss(
				context(), true, boss, empty_points
			),
			"skip odd early Reimu material frame"
		);
		expect_pixel(fixture.raster, {456, 36}, 0U, "Reimu parity skip is empty");
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw even Reimu material frame"
		);
		expect(
			fixture.presenter->ready(),
			"Reimu material orbit remains valid"
		);
		expect_pixel(
			fixture.raster, {456, 36}, 3U,
			"Reimu material orbit uses the frame-zero source cel"
		);

		expect(fixture.clear_output(), "clear Reimu material frame");
		fixture.activate(0U);
		boss = fact(0U);
		boss.render_state = 1U;
		boss.pattern_frame = 4U;
		boss.reimu_orbit_count = 1U;
		auto points = empty_points;
		points[0U] = {144 << 4, 80 << 4};
		expect(
			fixture.presenter->present_boss(context(), false, boss, points),
			"draw Reimu retained orbit point"
		);
		expect_pixel(fixture.raster, {456, 36}, 4U, "Reimu quarter-frame cel");
	}

	{
		Fixture fixture;
		fixture.trig.cosine.fill(0);
		fixture.trig.sine.fill(0);
		fixture.trig.cosine[28U] = 64;
		fixture.seed(body_rect(6U), 2U);
		fixture.seed({32U, 8U, 16U, 8U}, 3U);
		fixture.prepare();
		fixture.activate(6U);
		for(std::uint8_t frame = 1U; frame < 8U; ++frame) {
			fixture.presenter->boss_animation_step({
				PlayerId::p1, 6U, frame, 0U
			});
		}
		expect(
			fixture.presenter->present_boss(
				context(), false, fact(6U), empty_points
			),
			"draw Rikako after seven neutral-ramp frames"
		);
		expect_pixel(
			fixture.raster, {496, 44}, 3U,
			"Rikako neutral delta changes only on every eighth frame"
		);
	}

	{
		Fixture fixture;
		fixture.trig.cosine.fill(0);
		fixture.trig.sine.fill(0);
		fixture.trig.cosine[2U] = 64;
		fixture.seed({16U, 16U, 32U, 16U}, 3U);
		fixture.seed(body_rect(4U), 2U);
		fixture.seed({16U, 48U, 32U, 16U}, 4U);
		fixture.prepare();
		fixture.activate(4U);
		auto boss = fact(4U);
		boss.state = 0U;
		boss.pattern_frame = 10U;
		boss.pattern_angle = 0U;
		expect(
			fixture.presenter->present_boss(
				context(), true, boss, empty_points
			),
			"skip odd Kotohime material frame"
		);
		expect_pixel(fixture.raster, {509, 40}, 0U, "Kotohime parity skip");
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw even Kotohime material frame"
		);
		expect_pixel(
			fixture.raster, {509, 40}, 3U,
			"Kotohime material angle is postincremented before use"
		);

		expect(fixture.clear_output(), "clear Kotohime active orbit");
		boss.state = 1U;
		boss.render_state = 2U;
		boss.pattern_frame = 0U;
		boss.kotohime_orbit_count = 1U;
		boss.kotohime_radius = 0;
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw Kotohime flashing orbit"
		);
		expect_pixel(fixture.raster, {464, 40}, 4U, "Kotohime flash parity cel");
	}

	{
		Fixture fixture;
		fixture.trig.cosine.fill(0);
		fixture.trig.sine.fill(0);
		fixture.seed({32U, 8U, 32U, 16U}, 3U);
		fixture.seed(body_rect(5U), 2U);
		for(std::uint16_t cel = 0U; cel < 4U; ++cel) {
			fixture.seed(
				{static_cast<std::uint16_t>(64U + cel * 64U), 48U, 64U, 32U},
				static_cast<Index4>(4U + cel)
			);
		}
		fixture.prepare();
		fixture.activate(5U);
		auto boss = fact(5U);
		boss.state = 0U;
		boss.pattern_frame = 0U;
		expect(
			fixture.presenter->present_boss(
				context(), true, boss, empty_points
			),
			"skip odd early Kana material frame"
		);
		expect_pixel(fixture.raster, {464, 40}, 0U, "Kana parity skip");
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw even Kana material frame"
		);
		expect_pixel(fixture.raster, {464, 40}, 3U, "Kana material cel 0");

		boss.state = 1U;
		boss.render_state = 2U;
		for(const auto frame : std::array<std::uint8_t, 4U>{{0U, 8U, 16U, 24U}}) {
			expect(fixture.clear_output(), "clear Kana active band");
			boss.render_frame = frame;
			expect(
				fixture.presenter->present_boss(
					context(), false, boss, empty_points
				),
				"draw Kana active band"
			);
			expect_pixel(
				fixture.raster,
				{448, 28},
				static_cast<Index4>(4U + frame / 8U),
				"Kana 0/8/16/24 animation bands"
			);
		}
	}
}

void test_ellen_rikako_and_chiyuri_private_state()
{
	{
		Fixture fixture;
		fixture.seed(body_rect(3U), 2U);
		fixture.seed({128U, 62U, 96U, 1U}, 3U);
		fixture.seed({128U, 56U, 96U, 4U}, 4U);
		fixture.prepare();
		fixture.activate(3U);
		auto boss = fact(3U);
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw Ellen phase-zero trail"
		);
		expect_pixel(fixture.raster, {432, 74}, 3U, "Ellen trail index 0");
		expect(fixture.clear_output(), "clear Ellen private phase");
		fixture.presenter->boss_animation_step({PlayerId::p1, 6U, 1U, 0U});
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw Ellen phase advanced by four"
		);
		expect_pixel(
			fixture.raster, {432, 68}, 4U,
			"Ellen states 6..10 add the private extra three"
		);
	}

	{
		Fixture fixture;
		fixture.trig.cosine.fill(0);
		fixture.trig.sine.fill(0);
		fixture.trig.cosine[5U] = 64;
		fixture.seed(body_rect(6U), 2U);
		fixture.seed({32U, 8U, 16U, 8U}, 3U);
		fixture.seed({0U, 72U, 48U, 8U}, 3U);
		fixture.seed({0U, 24U, 48U, 24U}, 4U);
		fixture.seed({0U, 48U, 48U, 24U}, 5U);
		fixture.prepare();
		fixture.activate(6U);
		fixture.presenter->boss_animation_step({PlayerId::p1, 2U, 8U, 0U});
		auto boss = fact(6U);
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw Rikako after positive ramp"
		);
		expect_pixel(
			fixture.raster, {496, 44}, 3U,
			"Rikako orbit consumes incremented +5 delta"
		);

		for(std::uint8_t frame = 16U; frame <= 40U; frame += 8U) {
			fixture.presenter->boss_animation_step({PlayerId::p1, 2U, frame, 0U});
		}
		for(std::uint8_t step = 0U; step < 18U; ++step) {
			fixture.presenter->boss_animation_step({
				PlayerId::p1,
				7U,
				static_cast<std::uint16_t>((step + 1U) * 8U),
				0U,
			});
		}
		for(std::uint8_t step = 0U; step < 11U; ++step) {
			fixture.presenter->boss_animation_step({PlayerId::p1, 6U, step, 0U});
		}
		expect(
			fixture.presenter->ready(),
			"Rikako +9, -9, and convergence-to-+2 regimes remain valid"
		);

		expect(fixture.clear_output(), "clear Rikako material parity");
		fixture.activate(6U);
		boss = fact(6U);
		boss.state = 0U;
		boss.pattern_frame = 1U;
		expect(
			fixture.presenter->present_boss(
				context(0, 0, 1U), false, boss, empty_points
			),
			"skip Rikako odd early material frame"
		);
		expect_pixel(fixture.raster, {456, 36}, 0U, "Rikako material parity skip");
		expect(
			fixture.presenter->present_boss(
				context(0, 0, 2U), false, boss, empty_points
			),
			"draw Rikako even material frame"
		);
		expect_pixel(
			fixture.raster, {456, 36}, 5U,
			"Rikako material orbit uses the odd source cel"
		);
	}

	{
		Fixture fixture;
		fixture.trig.cosine.fill(0);
		fixture.trig.sine.fill(0);
		fixture.trig.cosine[4U] = 64;
		fixture.seed(body_rect(7U), 2U);
		fixture.seed({192U, 32U, 32U, 16U}, 3U);
		expect(
			fixture.raster.set_pending_palette_entry_rgb8(1U, {1U, 2U, 3U}),
			"seed Chiyuri target palette baseline"
		);
		fixture.raster.apply_pending_palette();
		fixture.prepare();
		fixture.activate(7U);
		auto boss = fact(7U);
		boss.state = 0U;
		boss.pattern_frame = 0U;
		expect(
			fixture.presenter->present_boss(
				context(), true, boss, empty_points
			),
			"skip odd Chiyuri material frame"
		);
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw even Chiyuri material frame"
		);
		expect_pixel(
			fixture.raster, {528, 40}, 3U,
			"Chiyuri material phase advances only on rendered frames"
		);

		fixture.presenter->boss_animation_step({PlayerId::p1, 0U, 0x60U, 0U});
		expect(
			fixture.raster.pending_palette().colors[1U] == Rgb8{240U, 240U, 240U},
			"Chiyuri frame 96 ends on deferred gray 240"
		);
		expect(
			fixture.raster.applied_palette().colors[1U] ==
				th03::assets::Rgb4{0U, 0U, 0U},
			"Chiyuri fade never shows the deferred palette immediately"
		);
		for(std::uint8_t step = 0U; step < 15U; ++step) {
			fixture.presenter->boss_animation_step({PlayerId::p1, 1U, step, 0U});
		}
		expect(
			fixture.raster.pending_palette().colors[1U] == Rgb8{16U, 16U, 16U},
			"Chiyuri phase 1 to 0 performs no final black write"
		);
		fixture.presenter->boss_palette_baseline_restored({PlayerId::p2});
		expect(
			fixture.raster.pending_palette().colors[1U] == Rgb8{1U, 2U, 3U} &&
				fixture.palette.dirty(),
			"Boss palette restore is one deferred baseline entry"
		);

		expect(fixture.clear_output(), "clear Chiyuri monochrome body");
		boss = fact(7U);
		boss.render_state = 24U;
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw Chiyuri monochrome threshold"
		);
		expect_pixel(fixture.raster, {416, 16}, 6U, "Chiyuri >=24 color threshold");
	}
}

void test_yumemi_wipe_vectors_and_clips()
{
	Fixture fixture;
	fixture.seed(body_rect(8U), 2U);
	fixture.prepare();
	fixture.activate(8U);
	auto boss = fact(8U);
	boss.state = 0U;

	struct Boundary {
		std::uint16_t frame;
		Point present;
		Point absent;
	};
	constexpr std::array<Boundary, 7U> boundaries{{
		{0U, {328, 20}, {400, 20}},
		{23U, {520, 20}, {600, 20}},
		{24U, {528, 20}, {600, 20}},
		{47U, {424, 20}, {600, 20}},
		{48U, {608, 20}, {560, 20}},
		{95U, {424, 20}, {560, 20}},
		{96U, {424, 20}, {608, 20}},
	}};
	for(const auto &boundary : boundaries) {
		expect(fixture.clear_output(), "clear Yumemi wipe boundary");
		boss.pattern_frame = boundary.frame;
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw Yumemi wipe boundary"
		);
		const auto expected = (boundary.frame == 96U) ? 0U : 2U;
		expect_pixel(
			fixture.raster,
			boundary.present,
			static_cast<Index4>(expected),
			"Yumemi wipe inclusive boundary"
		);
		expect_pixel(
			fixture.raster,
			boundary.absent,
			0U,
			"Yumemi wipe excludes the next boundary region"
		);
	}

	expect(fixture.clear_output(), "clear Yumemi wipe for vector");
	boss = fact(8U);
	boss.yumemi_radius = 8U;
	boss.yumemi_muzzle = {0, 0};
	expect(
		fixture.presenter->present_boss(context(), false, boss, empty_points),
		"draw Yumemi radius-8 circle"
	);
	expect_pixel(fixture.raster, {344, 8}, 10U, "Yumemi circle reaches radius 8");
	expect_pixel(fixture.raster, {335, 8}, 0U, "Yumemi P2 circle obeys inner clip");

	expect(fixture.clear_output(), "clear Yumemi circle for line");
	boss.yumemi_radius = 0U;
	boss.render_frame = 1U;
	boss.yumemi_target = {64 << 4, 64 << 4};
	expect(
		fixture.presenter->present_boss(context(), false, boss, empty_points),
		"draw Yumemi target line"
	);
	expect_pixel(fixture.raster, {336, 8}, 10U, "Yumemi line starts on clipped muzzle");
	expect(fixture.presenter->ready(), "Yumemi circle/line remains ready");
}

void test_generic_terminal_frames()
{
	Fixture fixture;
	fixture.seed({384U, 80U, 48U, 24U}, 7U);
	fixture.prepare();
	fixture.activate(0U);
	auto boss = fact(0U);
	boss.state = 0xFFU;
	for(const auto frame : std::array<std::uint16_t, 3U>{{0U, 1U, 25U}}) {
		expect(fixture.clear_output(), "clear generic terminal frame");
		boss.pattern_frame = frame;
		expect(
			fixture.presenter->present_boss(
				context(), false, boss, empty_points
			),
			"draw generic terminal frame 0/1/25"
		);
		expect_pixel(
			fixture.raster, {456, 36}, 7U,
			"generic terminal draws before the frame-25 clear"
		);
	}
}

void test_p1_p2_coordinates_shift_and_clip()
{
	Fixture fixture;
	fixture.seed(body_rect(2U, 0U), 3U);
	fixture.seed(body_rect(2U, 1U), 4U);
	fixture.prepare();
	fixture.activate(2U, 0U);
	auto boss = fact(2U, 0U);
	expect(
		fixture.presenter->present_boss(
			context(-5, 7), false, boss, empty_points
		),
		"draw owner-P1 Boss into shifted P2 field"
	);
	expect_pixel(fixture.raster, {399, 24}, 3U, "P2 target shift and owner source");

	expect(fixture.clear_output(), "clear between Boss owners");
	fixture.activate(2U, 1U);
	boss = fact(2U, 1U);
	expect(
		fixture.presenter->present_boss(
			context(-5, 7), false, boss, empty_points
		),
		"draw owner-P2 Boss into shifted P1 field"
	);
	expect_pixel(fixture.raster, {67, 24}, 4U, "P1 target shift and owner source");

	expect(fixture.clear_output(), "clear before target clip probe");
	fixture.activate(2U, 0U);
	boss = fact(2U, 0U);
	boss.center_x = -300 << 4;
	expect(
		fixture.presenter->present_boss(context(), false, boss, empty_points),
		"fully clipped Boss draw succeeds"
	);
	expect_pixel(fixture.raster, {320, 24}, 0U, "Boss cannot cross target clip");
}

void test_invalid_and_uncommitted_are_atomic_and_sticky()
{
	Fixture uncommitted;
	uncommitted.prepare(false);
	expect(
		uncommitted.presenter->status() == MainBossStatus::staging_not_committed,
		"uncommitted Boss atlas is rejected"
	);

	Fixture bad_trig;
	bad_trig.seed(body_rect(0U), 2U);
	bad_trig.trig.sine = {};
	expect(bad_trig.staging.commit(bad_trig.raster), "commit invalid trig fixture");
	expect(
		bad_trig.raster.set_raster_mode(RasterMode::zoomed_200_upper),
		"select invalid trig raster mode"
	);
	expect(bad_trig.palette.capture_baseline(bad_trig.raster), "capture baseline");
	const StaffFlakeTrigTables short_trig{
		std::span<const std::int16_t>(bad_trig.trig.sine).first(255U),
		bad_trig.trig.cosine,
	};
	bad_trig.presenter.emplace(
		bad_trig.raster, bad_trig.staging, short_trig, bad_trig.palette
	);
	expect(
		bad_trig.presenter->status() == MainBossStatus::invalid_assets,
		"short Boss trig table is rejected"
	);

	Fixture no_baseline;
	expect(no_baseline.staging.commit(no_baseline.raster), "commit no-baseline atlas");
	no_baseline.presenter.emplace(
		no_baseline.raster,
		no_baseline.staging,
		no_baseline.trig.view(),
		no_baseline.palette
	);
	expect(
		no_baseline.presenter->status() == MainBossStatus::invalid_assets,
		"Boss presenter rejects an uncaptured process palette baseline"
	);

	Fixture fixture;
	fixture.seed(body_rect(8U), 2U);
	fixture.prepare();
	const auto before = fixture.raster.display_state_hash();
	fixture.presenter->boss_activated({static_cast<PlayerId>(9U), paletted(8U)});
	expect(
		fixture.presenter->status() == MainBossStatus::invalid_observation,
		"invalid Boss activation is rejected"
	);
	expect(
		fixture.raster.display_state_hash() == before,
		"invalid Boss activation is atomic"
	);
	fixture.presenter->boss_activated({PlayerId::p1, paletted(8U)});
	expect(
		fixture.presenter->status() == MainBossStatus::invalid_observation &&
			fixture.raster.display_state_hash() == before,
		"Boss status is sticky"
	);

	Fixture radius;
	radius.seed(body_rect(8U), 2U);
	radius.prepare();
	radius.activate(8U);
	auto boss = fact(8U);
	boss.yumemi_radius = 256U;
	const auto radius_before = radius.raster.display_state_hash();
	expect(
		!radius.presenter->present_boss(
			context(), false, boss, empty_points
		),
		"unreachable Yumemi rendered radius 256 is rejected"
	);
	expect(
		radius.raster.display_state_hash() == radius_before,
		"invalid Boss fact is rejected before drawing"
	);

	Fixture kana;
	kana.seed(body_rect(5U), 2U);
	kana.prepare();
	kana.activate(5U);
	boss = fact(5U);
	boss.render_state = 3U;
	const auto kana_before = kana.raster.display_state_hash();
	expect(
		!kana.presenter->present_boss(context(), false, boss, empty_points) &&
			kana.raster.display_state_hash() == kana_before,
		"Kana rejects the unreachable active render state above 2 atomically"
	);
}

} // namespace

int main()
{
	test_all_character_bodies_and_hits();
	test_marisa_and_mima_animation_boundaries();
	test_reimu_kotohime_kana_material_and_overlays();
	test_ellen_rikako_and_chiyuri_private_state();
	test_yumemi_wipe_vectors_and_clips();
	test_generic_terminal_frames();
	test_p1_p2_coordinates_shift_and_clip();
	test_invalid_and_uncommitted_are_atomic_and_sticky();
	std::printf("%zu checks, %zu failures\n", checks, failures);
	return (failures == 0U) ? 0 : 1;
}
