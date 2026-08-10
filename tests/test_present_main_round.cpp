#include "th03/present/main_round.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "game_internal.hpp"

namespace {

using th03::MainFrameFooterReached;
using th03::BombCommonEffectStep;
using th03::Bullet16ConstructionKind;
using th03::Bullet16Spawned;
using th03::BulletSlot;
using th03::HitCircleKind;
using th03::HitCircleSpawned;
using th03::CommonEffectKind;
using th03::CommonEffectSpawned;
using th03::PlayerDefeatStep;
using th03::PlayerId;
using th03::WarningInterstitialSample;
using th03::WarningInterstitialStarted;
using th03::assets::BfntConsumerPolicy;
using th03::assets::BfntGlyph;
using th03::assets::BfntSheet;
using th03::assets::MrsImage;
using th03::assets::MrsOrientation;
using th03::assets::PlanarBitmap4;
using th03::assets::PlanarBitOrder;
using th03::assets::PlanarRowOrder;
using th03::present::AnkGlyph;
using th03::present::AnkGlyphProvider;
using th03::present::ComposedPixel;
using th03::present::Index4;
using th03::present::MainBombMrsAssets;
using th03::present::MainBombsPresenter;
using th03::present::MainEffectsPresenter;
using th03::present::MainEntitiesPresenter;
using th03::present::MainGameMode;
using th03::present::MainHudState;
using th03::present::MainPaletteState;
using th03::present::MainPalettedCharacter;
using th03::present::MainPlayerOverlayAssets;
using th03::present::MainPlayersBulletsPresenter;
using th03::present::MainRoundMissingPhase;
using th03::present::MainRoundPresenter;
using th03::present::MainRoundStatus;
using th03::present::MainStaticPresenter;
using th03::present::PackedRowOrder;
using th03::present::Page;
using th03::present::PelletMasks;
using th03::present::Raster;
using th03::present::Rgb8;
using th03::present::StaffFlakeTrigTables;
using th03::present::Tram;
using th03::present::authentic_main_bomb_atlas_layout;
using th03::present::authentic_main_effects_atlas_layout;
using th03::present::authentic_main_entity_atlas_layout;
using th03::present::authentic_main_players_bullets_atlas_layout;
using th03::present::graphics_page_pixels;
using th03::present::main_round_missing_phases;
using th03::present::pellet_mask_bytes;
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

MrsImage make_mrs(Index4 color, MrsOrientation orientation)
{
	constexpr std::size_t plane_size = (288U / 8U) * 184U;
	MrsImage image{};
	auto &cel = image.cel;
	cel.bitplane_size = static_cast<std::uint16_t>(plane_size);
	cel.pixel_width = 288U;
	cel.pixel_height = 184U;
	cel.offset_at_bottom_left = 183 * 80;
	cel.vram_dword_width = 9U;
	cel.row_order = PlanarRowOrder::bottom_to_top;
	cel.bit_order = PlanarBitOrder::most_significant_bit_left;
	cel.alpha = std::vector<std::uint8_t>(plane_size, 0xFFU);
	for(std::size_t plane = 0U; plane < cel.color_planes.size(); ++plane) {
		cel.color_planes[plane].assign(
			plane_size,
			((color & (1U << plane)) != 0U) ? 0xFFU : 0x00U
		);
	}
	image.orientation = orientation;
	return image;
}

struct SyntheticAssets {
	BfntSheet player_one = make_sheet(32U, 32U, 9U, 0U);
	BfntSheet player_two = make_sheet(32U, 32U, 9U, 5U);
	BfntSheet gauge = make_sheet(32U, 16U, 16U, 9U);
	std::array<std::uint8_t, pellet_mask_bytes> pellets{};
	std::array<std::int16_t, 256U> sine{};
	std::array<std::int16_t, 256U> cosine{};
	MrsImage p1_mrs = make_mrs(2U, MrsOrientation::as_stored);
	MrsImage p2_mrs = make_mrs(3U, MrsOrientation::horizontally_flipped);

	SyntheticAssets()
	{
		cosine.fill(256);
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

	[[nodiscard]] MainBombMrsAssets mrs() const noexcept
	{
		return {{&p1_mrs, &p2_mrs}};
	}
};

struct BlankAnk final : AnkGlyphProvider {
	[[nodiscard]] std::optional<AnkGlyph> glyph(
		std::uint8_t
	) const override
	{
		return AnkGlyph{};
	}
};

struct Fixture {
	Raster raster;
	Tram tram;
	MainPaletteState palette;
	MainStaticPresenter static_presenter;
	SyntheticAssets assets;
	std::optional<MainEntitiesPresenter> entities;
	std::optional<MainEffectsPresenter> effects;
	std::optional<MainPlayersBulletsPresenter> players_bullets;
	std::optional<MainBombsPresenter> bombs;
	std::optional<MainRoundPresenter> round;

	Fixture()
	{
		const auto hud = hud_state();
		expect(raster.set_access_page(Page::one), "access the MAINL atlas page");
		expect(
			raster.clear({64, sprite16_source_top, 16U, 8U}, 9U) &&
			raster.clear(
				{32, sprite16_source_top + 80, 32U, 16U}, 7U
			) && raster.clear(
				{128, sprite16_source_top + 80, 48U, 24U}, 6U
			),
			"seed Bullet16, Fireball, and hit-circle cels in the handoff"
		);
		expect(
			static_presenter.round_startup(
				raster,
				tram,
				hud,
				{&assets.player_one, &assets.player_two},
				palette
			),
			"prepare the synthetic MAIN process"
		);
		const auto &staging = static_presenter.sprite_staging();
		entities.emplace(
			raster, staging, authentic_main_entity_atlas_layout()
		);
		effects.emplace(
			raster,
			staging,
			authentic_main_effects_atlas_layout(),
			assets.trig()
		);
		players_bullets.emplace(
			raster,
			staging,
			authentic_main_players_bullets_atlas_layout(),
			assets.overlays(),
			assets.pellet_masks(),
			assets.trig()
		);
		bombs.emplace(
			raster,
			staging,
			authentic_main_bomb_atlas_layout(),
			assets.mrs(),
			assets.trig(),
			palette
		);
		round.emplace(
			raster,
			tram,
			static_presenter,
			palette,
			*entities,
			*effects,
			*players_bullets,
			*bombs
		);
		expect(round->ready(), "construct a ready MAIN round composer");
	}

	[[nodiscard]] static MainHudState hud_state() noexcept
	{
		MainHudState hud{};
		hud.game_mode = MainGameMode::versus;
		hud.players[0U] = {10U, 2U, 0U, 1U};
		hud.players[1U] = {10U, 2U, 0U, 1U};
		return hud;
	}

	[[nodiscard]] th03::detail::DeterministicState state() const
	{
		th03::detail::DeterministicState state{};
		state.round_id = 3U;
		state.round_or_result_frame = 17U;
		for(std::size_t player = 0U; player < state.players.size(); ++player) {
			state.players[player].center_x =
				th03::exact::Subpixel::from_integer_wrapping(144);
			state.players[player].center_y =
				th03::exact::Subpixel::from_integer_wrapping(336);
		}
		state.players[0U].paletted_character = static_cast<std::int8_t>(
			MainPalettedCharacter::reimu_primary
		);
		state.players[1U].paletted_character = static_cast<std::int8_t>(
			MainPalettedCharacter::kana_primary
		);
		return state;
	}

	void ordinary_phases(const th03::detail::DeterministicState &state)
	{
		round->observe(th03::detail::ObserverAccess::shots_enemies(state));
		round->observe(th03::detail::ObserverAccess::fireballs(state));
		round->observe(th03::detail::ObserverAccess::players_bullets(state));
	}
};

[[nodiscard]] std::vector<Index4> page_copy(
	const Raster &raster,
	Page page
)
{
	const auto pixels = raster.graphics(page);
	return {pixels.begin(), pixels.end()};
}

void test_contract_and_setup_failure()
{
	static_assert(!std::is_copy_constructible_v<MainRoundPresenter>);
	static_assert(!std::is_move_constructible_v<MainRoundPresenter>);
	constexpr std::array expected{
		MainRoundMissingPhase::boss,
		MainRoundMissingPhase::extra_attack,
		MainRoundMissingPhase::charge_shot,
		MainRoundMissingPhase::background,
		MainRoundMissingPhase::warning_and_gauge,
		MainRoundMissingPhase::hud,
		MainRoundMissingPhase::result,
	};
	expect(
		main_round_missing_phases == expected,
		"name every unimplemented MAIN phase instead of guessing it"
	);

	Fixture fixture;
	MainStaticPresenter stopped_static;
	fixture.raster.set_palette_tone(50);
	expect(
		fixture.raster.set_pending_palette_entry_rgb8(
			2U, Rgb8{0xA0U, 0x50U, 0x20U}
		),
		"seed a nonblack logical palette for construction-show coverage"
	);
	MainRoundPresenter stopped(
		fixture.raster,
		fixture.tram,
		stopped_static,
		fixture.palette,
		*fixture.entities,
		*fixture.effects,
		*fixture.players_bullets,
		*fixture.bombs
	);
	expect(
		stopped.status() == MainRoundStatus::invalid_setup &&
		fixture.raster.palette_tone() == 50,
		"reject an unstarted process without applying construction tone"
	);
	MainRoundPresenter valid(
		fixture.raster,
		fixture.tram,
		fixture.static_presenter,
		fixture.palette,
		*fixture.entities,
		*fixture.effects,
		*fixture.players_bullets,
		*fixture.bombs
	);
	expect(
		valid.ready() && fixture.raster.palette_tone() == 100,
		"valid construction performs entry.cpp's immediate tone-100 show"
	);
	expect(
		fixture.raster.applied_palette().colors[2U] ==
			th03::assets::Rgb4{10U, 5U, 2U},
		"valid construction applies the logical palette rather than only storing tone"
	);
	const auto before = page_copy(fixture.raster, Page::zero);
	auto state = fixture.state();
	stopped.observe(th03::detail::ObserverAccess::shots_enemies(state));
	stopped.main_frame_footer_reached({});
	expect(
		std::ranges::equal(before, fixture.raster.graphics(Page::zero)),
		"an invalid composer suppresses observation and footer mutation"
	);
}

void test_source_order_footer_and_surface()
{
	Fixture fixture;
	auto state = fixture.state();
	expect(
		fixture.palette.set_entry_deferred(
			fixture.raster, 3U, Rgb8{0xFFU, 0U, 0U}
		),
		"stage one deferred MAIN palette entry"
	);
	expect(fixture.palette.dirty(), "deferred palette starts dirty");
	const auto initially_shown = fixture.raster.shown_page();
	fixture.ordinary_phases(state);
	expect(fixture.round->ready(), "accept shots, fireballs, then final phase");
	expect(
		fixture.raster.shown_page() == initially_shown &&
		fixture.palette.dirty(),
		"ordinary phases neither publish the page nor consume deferred palette"
	);
	fixture.round->main_frame_footer_reached(MainFrameFooterReached{});
	expect(fixture.round->ready(), "accept the source footer after all phases");
	expect(
		fixture.raster.shown_page() == Page::zero &&
		fixture.raster.access_page() == Page::one,
		"footer publishes old back and accesses old front"
	);
	expect(
		!fixture.palette.dirty() &&
		fixture.raster.applied_palette().colors[3U] ==
			th03::assets::Rgb4{15U, 0U, 0U},
		"footer applies deferred palette before completing publication"
	);

	BlankAnk ank;
	std::vector<ComposedPixel> output(graphics_page_pixels);
	auto surface = fixture.round->compose_shown_surface(
		std::span<ComposedPixel, graphics_page_pixels>(
			output.data(), output.size()
		),
		ank
	);
	expect(
		surface.has_value() && surface->pixels.data() == output.data() &&
		surface->shown_page == Page::zero,
		"compose the shown graphics and caller-owned TRAM into caller storage"
	);

	state.round_or_result_frame++;
	fixture.ordinary_phases(state);
	fixture.round->main_frame_footer_reached({});
	expect(
		fixture.round->ready() &&
		fixture.raster.shown_page() == Page::one &&
		fixture.raster.access_page() == Page::zero,
		"footer resets ordinary phase order for the next frame"
	);
}

void test_common_effect_fanout_stops_before_bombs()
{
	{
		Fixture fixture;
		fixture.round->common_effect_spawned(CommonEffectSpawned{
			.kind = CommonEffectKind::expanding_circle,
			.player = static_cast<PlayerId>(2U),
		});
		expect(
			fixture.round->status() == MainRoundStatus::effects_failed &&
			!fixture.effects->ready(),
			"common-effect spawn reaches MainEffects first"
		);
		expect(
			fixture.bombs->ready(),
			"a failed common spawn does not reach Bombs out of source order"
		);
	}
	{
		Fixture fixture;
		fixture.round->bomb_common_effect_step(BombCommonEffectStep{
			.player = static_cast<PlayerId>(2U),
		});
		expect(
			fixture.round->status() == MainRoundStatus::effects_failed &&
			!fixture.effects->ready(),
			"Bomb common-effect step reaches MainEffects first"
		);
		expect(
			fixture.bombs->ready(),
			"a failed Bomb common step does not reach Bombs out of order"
		);
	}
}

void test_wrong_order_is_sticky_and_atomic()
{
	Fixture fixture;
	auto state = fixture.state();
	const auto before = page_copy(fixture.raster, Page::zero);
	fixture.round->observe(th03::detail::ObserverAccess::fireballs(state));
	expect(
		fixture.round->status() == MainRoundStatus::invalid_observation,
		"reject fireballs before shots and enemies"
	);
	fixture.round->observe(th03::detail::ObserverAccess::shots_enemies(state));
	fixture.round->main_frame_footer_reached({});
	expect(
		fixture.round->status() == MainRoundStatus::invalid_observation &&
		fixture.raster.shown_page() == Page::one &&
		std::ranges::equal(before, fixture.raster.graphics(Page::zero)),
		"first order failure is sticky and suppresses all later work"
	);
}

void test_phase_context_includes_foreground_shifts()
{
	Fixture fixture;
	auto state = fixture.state();
	fixture.round->observe(th03::detail::ObserverAccess::shots_enemies(state));
	state.playfield_fg_shift_x[0U] = 4;
	fixture.round->observe(th03::detail::ObserverAccess::fireballs(state));
	expect(
		fixture.round->status() == MainRoundStatus::invalid_observation,
		"reject a phase view whose foreground shifts differ within one frame"
	);
}

void test_fireballs_precede_hit_circles()
{
	Fixture fixture;
	auto state = fixture.state();
	fixture.round->observe(th03::detail::ObserverAccess::shots_enemies(state));
	state.fireballs[0U].flag = th03::detail::FireballFlag::fall;
	state.fireballs[0U].player_id = 2U;
	fixture.round->observe(th03::detail::ObserverAccess::fireballs(state));
	expect(
		fixture.round->status() == MainRoundStatus::entities_failed &&
		!fixture.entities->ready(),
		"the Fireballs entity pass rejects its facts first"
	);
	expect(
		fixture.effects->ready(),
		"failed Fireballs suppresses the following hit-circle pass"
	);
}

void test_hit_circles_overwrite_fireballs()
{
	Fixture fixture;
	auto state = fixture.state();
	fixture.round->hit_circle_spawned(HitCircleSpawned{
		.kind = HitCircleKind::enemy,
		.player = PlayerId::p1,
		.center_x = static_cast<std::int16_t>(144 * 16),
		.center_y = static_cast<std::int16_t>(336 * 16),
	});
	state.fireballs[0U].flag = th03::detail::FireballFlag::fall;
	state.fireballs[0U].center_x = static_cast<std::int16_t>(144 * 16);
	state.fireballs[0U].center_y = static_cast<std::int16_t>(336 * 16);
	state.fireballs[0U].player_id = 0U;
	state.fireballs[0U].variant_as_explosion_target = 2U;
	fixture.round->observe(th03::detail::ObserverAccess::shots_enemies(state));
	fixture.round->observe(th03::detail::ObserverAccess::fireballs(state));
	expect(
		fixture.round->ready() &&
		fixture.raster.pixel(Page::zero, {160, 176}) == 12U,
		"odd-frame hit-circle color overwrites the overlapping Fireball cel"
	);
}

void test_terminal_no_footer_does_not_publish()
{
	Fixture fixture;
	auto state = fixture.state();
	expect(
		fixture.palette.set_entry_deferred(
			fixture.raster, 4U, Rgb8{0U, 0xFFU, 0U}
		),
		"stage a palette change for a terminal frame"
	);
	const auto shown_hash = fixture.raster.display_state_hash();
	fixture.ordinary_phases(state);
	expect(
		fixture.round->ready() && fixture.palette.dirty() &&
		fixture.raster.shown_page() == Page::one &&
		fixture.raster.display_state_hash() == shown_hash,
		"terminal return without footer keeps the prior shown surface"
	);
}

void test_warning_interstitial_tones_without_footer()
{
	Fixture fixture;
	th03::GameplayObserver &observer = *fixture.round;
	const auto shown = fixture.raster.shown_page();
	const auto access = fixture.raster.access_page();
	expect(
		fixture.palette.set_entry_deferred(
			fixture.raster, 6U, Rgb8{0x7FU, 0x3FU, 0x1FU}
		),
		"stage a dirty palette entry before the blocking warning"
	);
	observer.warning_interstitial_started(WarningInterstitialStarted{
		.flashing_players = {true, false},
	});
	for(std::uint8_t ordinal = 0U; ordinal < 27U; ++ordinal) {
		observer.warning_interstitial_sample(WarningInterstitialSample{ordinal});
		expect(
			fixture.round->ready() &&
			fixture.raster.palette_tone() ==
				static_cast<std::int16_t>(100 + (50 * (ordinal & 1U))) &&
			fixture.raster.shown_page() == shown &&
			fixture.raster.access_page() == access && fixture.palette.dirty(),
			"warning sample immediately alternates tone without a page footer"
		);
		if(ordinal == 0U) {
			expect(
				fixture.raster.applied_palette().colors[6U] ==
					th03::assets::Rgb4{7U, 3U, 1U},
				"warning sample performs palette_show while retaining the dirty latch"
			);
		}
	}

	auto state = fixture.state();
	fixture.ordinary_phases(state);
	fixture.round->main_frame_footer_reached({});
	expect(
		fixture.round->ready() && !fixture.palette.dirty() &&
		fixture.raster.shown_page() != shown,
		"sample 26 permits the resumed suffix and its one ordinary footer"
	);
}

void test_warning_interstitial_order_is_strict()
{
	{
		Fixture fixture;
		fixture.round->warning_interstitial_sample({0U});
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject a warning sample without an accepted warning start"
		);
	}
	{
		Fixture fixture;
		fixture.round->warning_interstitial_started({{{true, false}}});
		fixture.round->warning_interstitial_sample({1U});
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject a skipped warning mini-loop ordinal"
		);
	}
	{
		Fixture fixture;
		fixture.round->warning_interstitial_started({{{true, false}}});
		for(std::uint8_t ordinal = 0U; ordinal < 27U; ++ordinal) {
			fixture.round->warning_interstitial_sample({ordinal});
		}
		fixture.round->warning_interstitial_started({{{false, true}}});
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject another warning start before sample 26's resumed suffix"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.round->warning_interstitial_started({{{true, false}}});
		fixture.round->observe(
			th03::detail::ObserverAccess::shots_enemies(state)
		);
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject ordinary phase publication before warning sample 26"
		);
	}
}

void test_malformed_late_bullet_is_whole_phase_atomic()
{
	Fixture fixture;
	auto state = fixture.state();
	fixture.round->observe(th03::detail::ObserverAccess::shots_enemies(state));
	fixture.round->observe(th03::detail::ObserverAccess::fireballs(state));
	const auto before = page_copy(fixture.raster, Page::zero);

	// The final slot claims a live Bullet16 without its required synchronous
	// construction fact. Whole-phase validation must reject this before player
	// pixels or the common-effect insertion pass can run.
	state.bullets.back().flag = th03::detail::BulletFlag::bullet16;
	state.bullets.back().player_id = 0U;
	fixture.round->observe(
		th03::detail::ObserverAccess::players_bullets(state)
	);
	expect(
		fixture.round->status() == MainRoundStatus::players_bullets_failed,
		"surface a malformed late Bullet16 as a child failure"
	);
	expect(
		std::ranges::equal(before, fixture.raster.graphics(Page::zero)),
		"validate players and every bullet before the first final-phase pixel"
	);
	fixture.round->main_frame_footer_reached({});
	expect(
		fixture.raster.shown_page() == Page::one,
		"failed final phase suppresses the otherwise valid footer"
	);
}

void test_players_common_bullets_insertion_order()
{
	Fixture fixture;
	auto state = fixture.state();
	fixture.round->common_effect_spawned(CommonEffectSpawned{
		.kind = CommonEffectKind::expanding_circle,
		.player = PlayerId::p1,
		.center_x = static_cast<std::int16_t>(144 * 16),
		.center_y = static_cast<std::int16_t>(336 * 16),
	});
	fixture.round->bullet16_spawned(Bullet16Spawned{
		.slot = static_cast<BulletSlot>(0U),
		.source_player = PlayerId::p1,
		.construction = Bullet16ConstructionKind::default_attack,
		.source_template_animation_enabled = false,
	});
	state.bullets[0U].flag = th03::detail::BulletFlag::bullet16;
	state.bullets[0U].center_x = static_cast<std::int16_t>(144 * 16);
	state.bullets[0U].center_y = static_cast<std::int16_t>(314 * 16);
	state.bullets[0U].player_id = 0U;
	fixture.ordinary_phases(state);
	expect(
		fixture.round->ready() &&
		fixture.raster.pixel(Page::zero, {168, 176}) == 13U,
		"common circle overwrites the already-drawn player at its right edge"
	);
	expect(
		fixture.raster.pixel(Page::zero, {160, 168}) == 9U,
		"later Bullet16 overwrites the common circle at its top edge"
	);
}

void test_defeat_palette_is_immediate()
{
	Fixture fixture;
	expect(
		fixture.palette.set_entry_deferred(
			fixture.raster, 7U, Rgb8{0x80U, 0x40U, 0x20U}
		),
		"stage a logical palette change before the immediate defeat show"
	);
	fixture.round->player_defeat_step(PlayerDefeatStep{
		.player = PlayerId::p1,
		.lose_anim_time_before = 2U,
		.round_or_result_frame_before = 1U,
	});
	expect(
		fixture.round->ready() && fixture.raster.palette_tone() == 130 &&
		fixture.raster.applied_palette().colors[7U] !=
			th03::assets::Rgb4{} && fixture.palette.dirty(),
		"continuing defeat uses the immediate odd-frame tone 130"
	);
	fixture.round->player_defeat_step(PlayerDefeatStep{
		.player = PlayerId::p1,
		.lose_anim_time_before = 1U,
		.round_or_result_frame_before = 1U,
	});
	expect(
		fixture.round->ready() && fixture.raster.palette_tone() == 100,
		"decrement-to-zero defeat immediately restores tone 100"
	);
}

void test_repeat_round_startup_preserves_process_children()
{
	Fixture fixture;
	auto state = fixture.state();
	const auto *effects = std::addressof(*fixture.effects);
	const auto *bombs = std::addressof(*fixture.bombs);
	fixture.round->common_effect_spawned(CommonEffectSpawned{
		.kind = CommonEffectKind::expanding_circle,
		.player = PlayerId::p1,
		.center_x = static_cast<std::int16_t>(144 * 16),
		.center_y = static_cast<std::int16_t>(336 * 16),
	});
	expect(
		fixture.palette.set_entry_deferred(
			fixture.raster, 5U, Rgb8{0U, 0U, 0xFFU}
		),
		"carry a dirty palette latch out of the terminal frame"
	);
	fixture.ordinary_phases(state);
	expect(
		fixture.round->repeat_round_startup(Fixture::hud_state()),
		"perform the explicit route-1 repeat startup"
	);
	expect(
		fixture.round->ready() && fixture.raster.access_page() == Page::zero &&
		fixture.raster.shown_page() == Page::one &&
		fixture.static_presenter.page_roles() ==
			th03::present::MainPageRoles{Page::zero, Page::one},
		"repeat startup restores the source's fixed page roles"
	);
	expect(
		fixture.raster.pixel(Page::zero, {100, 100}) == 2U &&
		fixture.raster.pixel(Page::one, {100, 100}) == 2U,
		"repeat startup clears page 1 then page 0 upper halves to color 2"
	);
	expect(
		fixture.static_presenter.sprite_staging().committed() &&
		fixture.raster.pixel(Page::zero, {64, 200}) == 9U &&
		fixture.raster.pixel(Page::one, {64, 200}) == 9U,
		"repeat startup preserves the committed lower-half Sprite16 atlas"
	);
	expect(
		fixture.palette.dirty() && fixture.raster.palette_tone() == 100,
		"repeat startup retains the dirty latch and immediately shows tone 100"
	);
	expect(
		std::addressof(*fixture.effects) == effects &&
		std::addressof(*fixture.bombs) == bombs,
		"repeat startup preserves process-scoped child presenters and pools"
	);

	state.round_id++;
	state.round_or_result_frame = 0U;
	fixture.round->common_effects_advanced();
	fixture.ordinary_phases(state);
	expect(
		fixture.raster.pixel(Page::zero, {176, 176}) == 13U,
		"repeat startup preserves the process-scoped common-effect pool"
	);
	fixture.round->main_frame_footer_reached({});
	expect(
		fixture.round->ready() && !fixture.palette.dirty(),
		"first repeated-round footer consumes the carried palette latch"
	);
}

} // namespace

int main()
{
	test_contract_and_setup_failure();
	test_source_order_footer_and_surface();
	test_common_effect_fanout_stops_before_bombs();
	test_wrong_order_is_sticky_and_atomic();
	test_phase_context_includes_foreground_shifts();
	test_fireballs_precede_hit_circles();
	test_hit_circles_overwrite_fireballs();
	test_terminal_no_footer_does_not_publish();
	test_warning_interstitial_tones_without_footer();
	test_warning_interstitial_order_is_strict();
	test_malformed_late_bullet_is_whole_phase_atomic();
	test_players_common_bullets_insertion_order();
	test_defeat_palette_is_immediate();
	test_repeat_round_startup_preserves_process_children();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return EXIT_FAILURE;
	}
	std::printf("%zu checks passed\n", checks);
	return EXIT_SUCCESS;
}
