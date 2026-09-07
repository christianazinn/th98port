#include "th03/present/main_players_bullets.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using th03::Bullet16ConstructionKind;
using th03::Bullet16Spawned;
using th03::BulletFact;
using th03::BulletFlagFact;
using th03::BulletSlot;
using th03::MainPhaseContext;
using th03::PlayerDefeatStep;
using th03::PlayerFact;
using th03::PlayerId;
using th03::PlayerPoseCommitted;
using th03::RawPoint;
using th03::assets::BfntConsumerPolicy;
using th03::assets::BfntGlyph;
using th03::assets::BfntSheet;
using th03::assets::PlanarBitOrder;
using th03::assets::PlanarBitmap4;
using th03::assets::PlanarRowOrder;
using th03::present::Index4;
using th03::present::MainBulletFacts;
using th03::present::MainBulletTrails;
using th03::present::MainPalettedCharacter;
using th03::present::MainPlayerFacts;
using th03::present::MainPlayerOverlayAssets;
using th03::present::MainPlayersBulletsAtlasLayout;
using th03::present::MainPlayersBulletsPresenter;
using th03::present::MainPlayersBulletsStatus;
using th03::present::PackedRowOrder;
using th03::present::Page;
using th03::present::PelletMasks;
using th03::present::Point;
using th03::present::Raster;
using th03::present::Sprite16AtlasRect;
using th03::present::Sprite16Staging;
using th03::present::StaffFlakeTrigTables;
using th03::present::authentic_main_players_bullets_atlas_layout;
using th03::present::pellet_mask_bytes;
using th03::present::pellet_preshift_count;
using th03::present::pellet_variant_bytes;
using th03::present::sprite16_source_top;

int failures = 0;

void expect(bool condition, std::string_view message)
{
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
)
{
	const auto actual = raster.pixel(page, point);
	expect(actual.has_value() && *actual == expected, message);
}

constexpr bool rect_equal(
	Sprite16AtlasRect left,
	Sprite16AtlasRect right
) noexcept
{
	return left.left == right.left && left.raw_top == right.raw_top &&
		left.width == right.width && left.raw_height == right.raw_height;
}

void put_source_marker(
	Raster &raster,
	Sprite16AtlasRect rect,
	Index4 color
)
{
	expect(
		raster.plot(
			{
				static_cast<std::int32_t>(rect.left),
				static_cast<std::int32_t>(sprite16_source_top + rect.raw_top),
			},
			color
		),
		"seed a synthetic player/bullet atlas marker"
	);
}

MainPlayersBulletsAtlasLayout seed_atlas(Raster &raster)
{
	auto layout = authentic_main_players_bullets_atlas_layout();
	for(std::size_t player = 0U; player < layout.player_pose_cels.size();
		++player) {
		for(std::size_t pose = 0U; pose < layout.player_pose_cels[player].size();
			++pose) {
			put_source_marker(
				raster,
				layout.player_pose_cels[player][pose],
				static_cast<Index4>(1U + (player * 3U) + pose)
			);
		}
	}
	put_source_marker(raster, layout.defeat_ring, 14U);
	for(std::size_t player = 0U; player < 2U; ++player) {
		for(std::size_t cel = 0U; cel < 4U; ++cel) {
			put_source_marker(
				raster,
				layout.default_bullet16_cels[player][cel],
				static_cast<Index4>(1U + (player * 4U) + cel)
			);
			put_source_marker(
				raster,
				layout.kana_bullet16_cels[player][cel],
				static_cast<Index4>(
					1U + ((8U + (player * 4U) + cel) % 15U)
				)
			);
		}
	}
	for(const auto rect : layout.pellet_cloud_cels) {
		put_source_marker(raster, rect, 6U);
	}
	return layout;
}

BfntSheet make_sheet(
	std::uint16_t width,
	std::uint16_t height,
	std::uint16_t glyph_count,
	std::uint8_t color_bias
)
{
	BfntSheet sheet{};
	sheet.consumer = BfntConsumerPolicy::superimpose;
	sheet.header.pixel_width = width;
	sheet.header.pixel_height = height;
	sheet.header.first_code = 0U;
	sheet.header.last_code = static_cast<std::uint16_t>(glyph_count - 1U);
	sheet.transparent_color = 0U;
	const auto plane_size = static_cast<std::size_t>(width / 8U) * height;
	for(std::uint16_t code = 0U; code < glyph_count; ++code) {
		const auto color = static_cast<std::uint8_t>(
			1U + ((color_bias + code) % 15U)
		);
		PlanarBitmap4 pixels{};
		pixels.row_order = PlanarRowOrder::top_to_bottom;
		pixels.bit_order = PlanarBitOrder::most_significant_bit_left;
		for(std::size_t plane = 0U; plane < pixels.color_planes.size(); ++plane) {
			pixels.color_planes[plane].assign(
				plane_size,
				((color & (1U << plane)) != 0U) ? 0xFFU : 0x00U
			);
		}
		sheet.glyphs.push_back(BfntGlyph{code, std::move(pixels)});
	}
	return sheet;
}

struct SyntheticAssets {
	BfntSheet player_one = make_sheet(32U, 32U, 9U, 0U);
	BfntSheet player_two = make_sheet(32U, 32U, 9U, 5U);
	BfntSheet gauge = make_sheet(32U, 16U, 16U, 9U);
	std::array<std::uint8_t, pellet_mask_bytes> pellets{};
	std::array<std::int16_t, 256U> sine{};
	std::array<std::int16_t, 256U> cosine{};

	SyntheticAssets()
	{
		cosine.fill(256);
		for(std::size_t shift = 0U; shift < pellet_preshift_count; ++shift) {
			const auto normal = shift * pellet_variant_bytes;
			const auto transfer =
				(pellet_preshift_count + shift) * pellet_variant_bytes;
			for(std::size_t byte = 0U; byte < pellet_variant_bytes; ++byte) {
				pellets[normal + byte] = 0xFFU;
			}
			pellets[transfer] = 0x80U;
		}
	}

	[[nodiscard]] MainPlayerOverlayAssets overlays() const noexcept
	{
		return {
			.character_sheets = {{&player_one, &player_two}},
			.loaded_characters = {{
				MainPalettedCharacter::reimu_primary,
				MainPalettedCharacter::kana_primary,
			}},
			.gauge_sheet = &gauge,
		};
	}

	[[nodiscard]] PelletMasks pellet_masks() const noexcept
	{
		return {pellets, PackedRowOrder::top_to_bottom};
	}

	[[nodiscard]] StaffFlakeTrigTables trig() const noexcept
	{
		return {sine, cosine};
	}
};

void commit(Sprite16Staging &staging, Raster &raster)
{
	expect(staging.commit(raster), "commit the synthetic player/bullet atlas");
}

constexpr std::int16_t raw_q12_4(std::int32_t pixels) noexcept
{
	return static_cast<std::int16_t>(pixels * 16);
}

MainPlayerFacts ordinary_players()
{
	MainPlayerFacts players{};
	players[0U] = {
		.player_id = PlayerId::p1,
		.center_x = raw_q12_4(100),
		.center_y = raw_q12_4(100),
		.paletted_character = static_cast<std::int8_t>(
			MainPalettedCharacter::reimu_primary
		),
	};
	players[1U] = {
		.player_id = PlayerId::p2,
		.center_x = raw_q12_4(100),
		.center_y = raw_q12_4(100),
		.paletted_character = static_cast<std::int8_t>(
			MainPalettedCharacter::kana_primary
		),
	};
	return players;
}

MainBulletFacts empty_bullets()
{
	MainBulletFacts bullets{};
	for(std::size_t slot = 0U; slot < bullets.size(); ++slot) {
		bullets[slot].slot = static_cast<BulletSlot>(slot);
	}
	return bullets;
}

BulletFact bullet_at(
	std::size_t slot,
	std::uint8_t flag,
	std::uint8_t age,
	PlayerId player,
	std::int32_t left,
	std::int32_t logical_top,
	std::uint16_t size,
	const MainPhaseContext &context,
	bool has_trail = false
)
{
	const auto player_index = (player == PlayerId::p1) ? 0U : 1U;
	return {
		.slot = static_cast<BulletSlot>(slot),
		.flag = BulletFlagFact{flag},
		.age = age,
		.center_x = raw_q12_4(
			left + static_cast<std::int32_t>(size / 2U) - 16 -
			static_cast<std::int32_t>(player_index * 320U) -
			context.playfield_fg_shift_x[player_index]
		),
		.center_y = raw_q12_4(
			logical_top + static_cast<std::int32_t>(size / 2U) - 16
		),
		.player_id = player,
		.has_trail = has_trail,
	};
}

RawPoint trail_point_at(
	std::int32_t left,
	std::int32_t logical_top,
	PlayerId player,
	const MainPhaseContext &context
)
{
	const auto player_index = (player == PlayerId::p1) ? 0U : 1U;
	return {
		.x = raw_q12_4(
			left + 8 - 16 - static_cast<std::int32_t>(player_index * 320U) -
			context.playfield_fg_shift_x[player_index]
		),
		.y = raw_q12_4(logical_top + 8 - 16),
	};
}

void test_authentic_layout_geometry()
{
	const auto layout = authentic_main_players_bullets_atlas_layout();
	expect(
		rect_equal(layout.player_pose_cels[0U][0U], {544U, 128U, 32U, 32U}) &&
		rect_equal(layout.player_pose_cels[1U][2U], {608U, 160U, 32U, 32U}),
		"player poses retain direct-assignment raw height and P1/P2 rows"
	);
	expect(
		rect_equal(layout.defeat_ring, {416U, 168U, 48U, 24U}),
		"defeat ring retains its direct-assignment 48x24 raw geometry"
	);
	expect(
		rect_equal(
			layout.default_bullet16_cels[1U][3U],
			{432U, 0U, 16U, 8U}
		) && rect_equal(
			layout.kana_bullet16_cels[1U][0U],
			{320U, 72U, 16U, 8U}
		),
		"default and Kana Bullet16 source-player regions are exact"
	);
	expect(
		rect_equal(layout.pellet_cloud_cels[2U], {592U, 80U, 32U, 16U}),
		"cloud cels retain their three adjacent source slots"
	);
}

void test_shared_pose_base_glow_and_gauge()
{
	Raster raster;
	const auto layout = seed_atlas(raster);
	Sprite16Staging staging;
	commit(staging, raster);
	SyntheticAssets assets;
	MainPlayersBulletsPresenter presenter(
		raster,
		staging,
		layout,
		assets.overlays(),
		assets.pellet_masks(),
		assets.trig()
	);
	MainPhaseContext context{};
	auto players = ordinary_players();
	presenter.player_pose_committed({PlayerId::p1, std::int8_t{-16}});
	presenter.player_pose_committed({PlayerId::p2, std::nullopt});
	expect(
		presenter.present_players(context, players),
		"draw both player bases after shared pose commits"
	);
	expect_pixel(raster, Page::zero, {100, 42}, 2U, "P1 uses the left pose cel");
	expect_pixel(
		raster,
		Page::zero,
		{420, 42},
		5U,
		"P2 null pose inherits P1's same-frame shared negative velocity"
	);

	presenter.player_pose_committed({PlayerId::p1, std::int8_t{0}});
	presenter.player_pose_committed({PlayerId::p2, std::nullopt});
	expect(
		raster.clear({90, 35, 360U, 50U}, 0U),
		"clear the player destinations before explicit neutral"
	);
	expect(
		presenter.present_players(context, players),
		"draw after an explicit shared neutral write"
	);
	expect_pixel(raster, Page::zero, {100, 42}, 1U, "engaged zero selects neutral");
	expect_pixel(raster, Page::zero, {420, 42}, 4U, "P2 inherits explicit neutral");

	expect(raster.set_access_page(Page::one), "select source page_back 1");
	players[0U].invincibility_time = 1U;
	players[0U].gauge_charged = 0x0100U;
	players[1U].invincibility_time = 1U;
	players[1U].hyper_active = 1U;
	context.round_or_result_frame = 4U;
	expect(
		presenter.present_players(context, players),
		"draw page-dependent glow and charged-gauge overlays"
	);
	expect_pixel(
		raster,
		Page::one,
		{100, 51},
		2U,
		"P1 invincibility selects source-local glow code 1 below the gauge"
	);
	expect_pixel(
		raster,
		Page::one,
		{420, 42},
		8U,
		"P2 hyper wins over invincibility and selects source-local code 2"
	);
	expect_pixel(
		raster,
		Page::one,
		{100, 35},
		11U,
		"gauge 0x100 uses the first range and frame-selected ZIKICW code 1"
	);
}

void test_defeat_shared_anchor_and_radius()
{
	Raster raster;
	const auto layout = seed_atlas(raster);
	Sprite16Staging staging;
	commit(staging, raster);
	SyntheticAssets assets;
	MainPlayersBulletsPresenter presenter(
		raster,
		staging,
		layout,
		assets.overlays(),
		assets.pellet_masks(),
		assets.trig()
	);
	auto players = ordinary_players();
	players[0U].lose_anim_time = 47U;
	MainPhaseContext context{};
	context.playfield_fg_shift_x[0U] = 6;
	presenter.player_defeat_step(PlayerDefeatStep{
		.player = PlayerId::p1,
		.lose_anim_time_before = 48U,
		.center_x = raw_q12_4(100),
		.center_y = raw_q12_4(100),
		.fg_shift_x_before_defeat_shake = 2,
	});
	expect(
		presenter.present_players(context, players),
		"draw the first post-update defeat frame from the captured shared anchor"
	);
	expect_pixel(
		raster,
		Page::zero,
		{100, 46},
		14U,
		"defeat draw adds current shake to the already pre-shifted anchor"
	);

	expect(raster.clear({0, 0, 320U, 200U}, 0U), "clear the P1 defeat field");
	context.playfield_fg_shift_x[0U] = -6;
	players[0U].lose_anim_time = 46U;
	presenter.player_defeat_step(PlayerDefeatStep{
		.player = PlayerId::p1,
		.lose_anim_time_before = 47U,
		.center_x = raw_q12_4(100),
		.center_y = raw_q12_4(100),
		.fg_shift_x_before_defeat_shake = 6,
	});
	expect(
		presenter.present_players(context, players),
		"draw the next defeat frame after the shared radius grows by nine"
	);
	expect_pixel(
		raster,
		Page::zero,
		{97, 46},
		14U,
		"Q8 polar math and current negative shake place the radius-9 ring"
	);
}

void test_simultaneous_defeat_shared_record_lifecycle()
{
	Raster raster;
	const auto layout = seed_atlas(raster);
	Sprite16Staging staging;
	commit(staging, raster);
	SyntheticAssets assets;
	MainPlayersBulletsPresenter presenter(
		raster,
		staging,
		layout,
		assets.overlays(),
		assets.pellet_masks(),
		assets.trig()
	);
	auto players = ordinary_players();
	players[0U].lose_anim_time = 47U;
	players[1U].lose_anim_time = 47U;
	MainPhaseContext context{};
	context.playfield_fg_shift_x = {-200, 0};
	presenter.player_defeat_step({
		PlayerId::p1,
		48U,
		raw_q12_4(100),
		raw_q12_4(100),
		0,
	});
	presenter.player_defeat_step({
		PlayerId::p2,
		48U,
		raw_q12_4(100),
		raw_q12_4(100),
		0,
	});
	expect(
		presenter.present_players(context, players),
		"draw simultaneous first defeat frames from the P2-overwritten record"
	);
	expect_pixel(
		raster,
		Page::zero,
		{212, 46},
		14U,
		"P1 uses the source-shared anchor most recently captured by P2"
	);
	expect_pixel(
		raster,
		Page::zero,
		{412, 46},
		14U,
		"P2 uses its last-written source-shared defeat anchor"
	);

	expect(
		raster.clear({0, 0, 640U, 128U}, 0U),
		"clear simultaneous-defeat destinations before continued steps"
	);
	players[0U].lose_anim_time = 46U;
	players[1U].lose_anim_time = 46U;
	presenter.player_defeat_step({
		PlayerId::p1,
		47U,
		raw_q12_4(100),
		raw_q12_4(100),
		0,
	});
	presenter.player_defeat_step({
		PlayerId::p2,
		47U,
		raw_q12_4(100),
		raw_q12_4(100),
		0,
	});
	expect(
		presenter.present_players(context, players),
		"draw both continuing defeats after two shared-radius increments"
	);
	expect_pixel(
		raster,
		Page::zero,
		{230, 46},
		14U,
		"P1 sees the shared radius incremented by both players to 18"
	);
	expect_pixel(
		raster,
		Page::zero,
		{430, 46},
		14U,
		"P2 sees the same twice-incremented shared radius"
	);

	expect(
		raster.clear({0, 0, 640U, 128U}, 0U),
		"clear simultaneous-defeat destinations before shared clear"
	);
	context.playfield_fg_shift_x = {0, -6};
	players[0U].lose_anim_time = 0xFFU;
	players[1U].lose_anim_time = 46U;
	presenter.player_defeat_step({
		PlayerId::p1,
		1U,
		raw_q12_4(100),
		raw_q12_4(100),
		0,
	});
	expect(
		presenter.present_players(context, players),
		"render after P1 decrement-to-zero clears the shared record"
	);
	expect_pixel(
		raster,
		Page::zero,
		{424, 46},
		0U,
		"banner-suppressed P2 clips the cleared zero anchor instead of reusing the old one"
	);

	expect(raster.set_access_page(Page::one), "select page 1 for FF suppression");
	expect(raster.set_shown_page(Page::one), "show page 1 for FF hash coverage");
	expect(
		raster.clear({0, 0, 640U, 128U}, 0U),
		"clear direct-FF suppression destinations"
	);
	players[0U].lose_anim_time = 0xFFU;
	players[1U].lose_anim_time = 0xFFU;
	players[0U].invincibility_time = 1U;
	players[1U].hyper_active = 1U;
	players[0U].gauge_charged = 0x0100U;
	players[1U].gauge_charged = 0x0100U;
	const auto before_ff = raster.display_state_hash();
	expect(
		presenter.present_players(context, players),
		"accept the source's terminal direct-FF defeat state"
	);
	expect(
		raster.display_state_hash() == before_ff,
		"direct lose==0xFF suppresses base, defeat ring, glow, and gauge"
	);
}

void test_bullet16_trails_styles_clouds_and_pellets()
{
	Raster raster;
	const auto layout = seed_atlas(raster);
	Sprite16Staging staging;
	commit(staging, raster);
	SyntheticAssets assets;
	MainPlayersBulletsPresenter presenter(
		raster,
		staging,
		layout,
		assets.overlays(),
		assets.pellet_masks(),
		assets.trig()
	);
	MainPhaseContext context{};
	context.playfield_fg_shift_x = {3, -6};
	auto bullets = empty_bullets();
	MainBulletTrails trails{};

	presenter.bullet16_spawned(Bullet16Spawned{
		.slot = static_cast<BulletSlot>(0U),
		.source_player = PlayerId::p1,
		.construction = Bullet16ConstructionKind::default_attack,
		.source_template_animation_enabled = true,
	});
	bullets[0U] = bullet_at(
		0U, 2U, 2U, PlayerId::p1, 200, 100, 16U, context, true
	);
	trails[0U][5U] = trail_point_at(40, 100, PlayerId::p1, context);
	trails[0U][3U] = trail_point_at(60, 100, PlayerId::p1, context);
	trails[0U][1U] = trail_point_at(80, 100, PlayerId::p1, context);

	presenter.bullet16_spawned(Bullet16Spawned{
		.slot = static_cast<BulletSlot>(1U),
		.source_player = PlayerId::p2,
		.construction = Bullet16ConstructionKind::kana_extra_attack,
		.source_template_animation_enabled = false,
	});
	bullets[1U] = bullet_at(
		1U, 2U, 3U, PlayerId::p1, 240, 100, 16U, context
	);
	bullets[2U] = bullet_at(
		2U, 4U, 8U, PlayerId::p2, 400, 120, 32U, context
	);
	bullets[3U] = bullet_at(
		3U, 1U, 0U, PlayerId::p1, 120, 140, 8U, context
	);
	bullets[4U] = bullet_at(
		4U, 7U, 0U, PlayerId::p1, 160, 140, 8U, context
	);

	presenter.bullet16_spawned({
		static_cast<BulletSlot>(5U),
		PlayerId::p1,
		Bullet16ConstructionKind::default_attack,
		true,
	});
	bullets[5U] = bullet_at(
		5U, 2U, 0U, PlayerId::p1, 280, 100, 16U, context, true
	);
	trails[5U][5U] = trail_point_at(280, 100, PlayerId::p1, context);
	trails[5U][3U] = trails[5U][5U];
	trails[5U][1U] = trails[5U][5U];
	for(std::size_t slot = 6U; slot <= 7U; ++slot) {
		presenter.bullet16_spawned({
			static_cast<BulletSlot>(slot),
			(slot == 6U) ? PlayerId::p1 : PlayerId::p2,
			(slot == 6U) ?
				Bullet16ConstructionKind::default_attack :
				Bullet16ConstructionKind::kana_extra_attack,
			false,
		});
		bullets[slot] = bullet_at(
			slot, 2U, 0U, PlayerId::p1, 320, 100, 16U, context
		);
	}
	presenter.bullet16_spawned({
		static_cast<BulletSlot>(8U),
		PlayerId::p1,
		Bullet16ConstructionKind::default_attack,
		false,
	});
	bullets[8U] = bullet_at(
		8U, 2U, 0U, PlayerId::p2, 400, 120, 16U, context
	);

	expect(
		presenter.present_bullets(context, bullets, trails),
		"draw Bullet16, cloud, and odd-flag pellet passes"
	);
	expect_pixel(raster, Page::zero, {40, 50}, 4U, "trail point 5 uses cel 3");
	expect_pixel(raster, Page::zero, {60, 50}, 3U, "trail point 3 uses cel 2");
	expect_pixel(raster, Page::zero, {80, 50}, 2U, "trail point 1 uses cel 1");
	expect_pixel(
		raster,
		Page::zero,
		{200, 50},
		3U,
		"animated default Bullet16 uses age modulo four"
	);
	expect_pixel(
		raster,
		Page::zero,
		{240, 50},
		13U,
		"static Kana Bullet16 uses source-player P2 cel zero"
	);
	expect_pixel(
		raster,
		Page::zero,
		{400, 60},
		15U,
		"age 8 cloud selects cel 2 and forces white mono"
	);
	expect_pixel(
		raster,
		Page::zero,
		{120, 70},
		15U,
		"exact raw flag 1 selects the ordinary pellet masks"
	);
	expect_pixel(
		raster,
		Page::zero,
		{160, 70},
		15U,
		"a non-stock odd flag retains the transfer-pellet renderer rule"
	);
	expect_pixel(
		raster,
		Page::zero,
		{121, 70},
		15U,
		"the ordinary pellet mask retains its adjacent opaque pixel"
	);
	expect_pixel(
		raster,
		Page::zero,
		{161, 70},
		0U,
		"the transfer pellet mask leaves the adjacent pixel transparent"
	);
	expect_pixel(
		raster,
		Page::zero,
		{280, 50},
		1U,
		"the Bullet16 main cel overwrites overlapping trail cels 3, 2, and 1"
	);
	expect_pixel(
		raster,
		Page::zero,
		{320, 50},
		13U,
		"the higher Bullet16 slot wins when two main cels overlap"
	);
	expect_pixel(
		raster,
		Page::zero,
		{400, 60},
		15U,
		"the later cloud pass wins over a colored Bullet16 at the same pixel"
	);
}

void test_style_retirement_and_malformed_atomicity()
{
	Raster raster;
	const auto layout = seed_atlas(raster);
	Sprite16Staging staging;
	commit(staging, raster);
	SyntheticAssets assets;
	MainPlayersBulletsPresenter presenter(
		raster,
		staging,
		layout,
		assets.overlays(),
		assets.pellet_masks(),
		assets.trig()
	);
	MainPhaseContext context{};
	auto bullets = empty_bullets();
	MainBulletTrails trails{};
	presenter.bullet16_spawned({
		static_cast<BulletSlot>(10U),
		PlayerId::p1,
		Bullet16ConstructionKind::default_attack,
		true,
	});
	expect(
		presenter.present_bullets(context, bullets, trails),
		"a Bullet16 spawn killed before the final view retires without drawing"
	);
	bullets[10U] = bullet_at(
		10U, 2U, 0U, PlayerId::p1, 200, 100, 16U, context
	);
	const auto before_reuse = raster.display_state_hash();
	expect(
		!presenter.present_bullets(context, bullets, trails),
		"later Bullet16 reuse without a fresh spawn fact is rejected"
	);
	expect(
		presenter.status() == MainPlayersBulletsStatus::invalid_observation &&
		raster.display_state_hash() == before_reuse,
		"stale Bullet16 style cannot draw or mutate the page"
	);

	Raster atomic_raster;
	const auto atomic_layout = seed_atlas(atomic_raster);
	Sprite16Staging atomic_staging;
	commit(atomic_staging, atomic_raster);
	MainPlayersBulletsPresenter atomic(
		atomic_raster,
		atomic_staging,
		atomic_layout,
		assets.overlays(),
		assets.pellet_masks(),
		assets.trig()
	);
	auto malformed = empty_bullets();
	atomic.bullet16_spawned({
		static_cast<BulletSlot>(0U),
		PlayerId::p1,
		Bullet16ConstructionKind::default_attack,
		true,
	});
	malformed[0U] = bullet_at(
		0U, 2U, 0U, PlayerId::p1, 100, 80, 16U, context
	);
	malformed.back().slot = static_cast<BulletSlot>(0U);
	const auto atomic_before = atomic_raster.display_state_hash();
	expect(
		!atomic.present_bullets(context, malformed, trails),
		"reject a malformed late bullet after an earlier drawable slot"
	);
	expect(
		atomic_raster.display_state_hash() == atomic_before,
		"the complete bullet pass validates before its first Sprite16 put"
	);
}

void test_invalid_assets_players_and_uncommitted_staging()
{
	SyntheticAssets assets;
	Raster bad_layout_raster;
	auto bad_layout = seed_atlas(bad_layout_raster);
	bad_layout.player_pose_cels[0U][0U].raw_height = 16U;
	Sprite16Staging bad_layout_staging;
	MainPlayersBulletsPresenter invalid_layout(
		bad_layout_raster,
		bad_layout_staging,
		bad_layout,
		assets.overlays(),
		assets.pellet_masks(),
		assets.trig()
	);
	expect(
		invalid_layout.status() == MainPlayersBulletsStatus::invalid_layout,
		"constructor rejects halved player raw height"
	);

	Raster bad_asset_raster;
	const auto asset_layout = seed_atlas(bad_asset_raster);
	Sprite16Staging bad_asset_staging;
	auto bad_overlays = assets.overlays();
	bad_overlays.loaded_characters[1U] =
		static_cast<MainPalettedCharacter>(0);
	MainPlayersBulletsPresenter invalid_assets(
		bad_asset_raster,
		bad_asset_staging,
		asset_layout,
		bad_overlays,
		assets.pellet_masks(),
		assets.trig()
	);
	expect(
		invalid_assets.status() == MainPlayersBulletsStatus::invalid_assets,
		"constructor rejects a non-character sheet identity"
	);

	Raster non_kana_raster;
	const auto non_kana_layout = seed_atlas(non_kana_raster);
	Sprite16Staging non_kana_staging;
	commit(non_kana_staging, non_kana_raster);
	auto non_kana_overlays = assets.overlays();
	non_kana_overlays.loaded_characters[1U] =
		MainPalettedCharacter::mima_primary;
	MainPlayersBulletsPresenter non_kana(
		non_kana_raster,
		non_kana_staging,
		non_kana_layout,
		non_kana_overlays,
		assets.pellet_masks(),
		assets.trig()
	);
	non_kana.bullet16_spawned({
		static_cast<BulletSlot>(0U),
		PlayerId::p2,
		Bullet16ConstructionKind::kana_extra_attack,
		false,
	});
	expect(
		non_kana.status() == MainPlayersBulletsStatus::invalid_observation,
		"Kana's custom Bullet16 construction rejects a non-Kana source sheet"
	);

	Raster mismatch_raster;
	const auto mismatch_layout = seed_atlas(mismatch_raster);
	Sprite16Staging mismatch_staging;
	commit(mismatch_staging, mismatch_raster);
	MainPlayersBulletsPresenter mismatch(
		mismatch_raster,
		mismatch_staging,
		mismatch_layout,
		assets.overlays(),
		assets.pellet_masks(),
		assets.trig()
	);
	auto players = ordinary_players();
	players[1U].paletted_character = static_cast<std::int8_t>(
		MainPalettedCharacter::marisa_primary
	);
	const auto mismatch_before = mismatch_raster.display_state_hash();
	expect(
		!mismatch.present_players({}, players),
		"reject player facts from a different loaded character sheet"
	);
	expect(
		mismatch_raster.display_state_hash() == mismatch_before,
		"character mismatch is atomic"
	);

	Raster uncommitted_raster;
	const auto uncommitted_layout = seed_atlas(uncommitted_raster);
	Sprite16Staging uncommitted_staging;
	MainPlayersBulletsPresenter uncommitted(
		uncommitted_raster,
		uncommitted_staging,
		uncommitted_layout,
		assets.overlays(),
		assets.pellet_masks(),
		assets.trig()
	);
	const auto uncommitted_before = uncommitted_raster.display_state_hash();
	expect(
		!uncommitted.present_players({}, ordinary_players()),
		"reject a player pass before Sprite16 staging commit"
	);
	expect(
		uncommitted.status() ==
			MainPlayersBulletsStatus::staging_not_committed &&
		uncommitted_raster.display_state_hash() == uncommitted_before,
		"uncommitted staging rejection changes no pixels"
	);
}

} // namespace

int main()
{
	test_authentic_layout_geometry();
	test_shared_pose_base_glow_and_gauge();
	test_defeat_shared_anchor_and_radius();
	test_simultaneous_defeat_shared_record_lifecycle();
	test_bullet16_trails_styles_clouds_and_pellets();
	test_style_retirement_and_malformed_atomicity();
	test_invalid_assets_players_and_uncommitted_staging();
	if(failures != 0) {
		std::fprintf(stderr, "%d player/bullet test(s) failed\n", failures);
		return EXIT_FAILURE;
	}
	std::puts("MAIN player/bullet presentation tests passed");
	return EXIT_SUCCESS;
}
