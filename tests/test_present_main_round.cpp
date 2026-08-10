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
using th03::MainBackgroundKind;
using th03::MainBackgroundPhase;
using th03::BombCompatibilityReadSite;
using th03::BombFrameStep;
using th03::BombCommonEffectStep;
using th03::Bullet16ConstructionKind;
using th03::Bullet16Spawned;
using th03::BulletSlot;
using th03::HitCircleKind;
using th03::HitCircleSpawned;
using th03::CommonEffectKind;
using th03::CommonEffectSpawned;
using th03::GaugeAttackFinished;
using th03::GaugeBossAttackKind;
using th03::HudStartFrameStep;
using th03::MainComboState;
using th03::MainGaugeState;
using th03::MainHudStartState;
using th03::MainResultState;
using th03::MainResultTransitionPhase;
using th03::MainResultTransitionState;
using th03::MainScoreState;
using th03::PlayerDefeatStep;
using th03::PlayerId;
using th03::WarningFrameLifecycle;
using th03::WarningInterstitialSample;
using th03::WarningInterstitialStarted;
using th03::WarningPortraitTransition;
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
using th03::present::MainBackgroundPresenter;
using th03::present::MainBossPresenter;
using th03::present::MainEffectsPresenter;
using th03::present::MainEntitiesPresenter;
using th03::present::MainExtraChargePresenter;
using th03::present::MainGameMode;
using th03::present::MainHudResultPresenter;
using th03::present::MainHudResultSheets;
using th03::present::MainHudResultTramAssets;
using th03::present::MainHudState;
using th03::present::MainPaletteState;
using th03::present::MainPalettedCharacter;
using th03::present::MainPlayerOverlayAssets;
using th03::present::MainPlayersBulletsPresenter;
using th03::present::MainRoundPresenter;
using th03::present::MainRoundStatus;
using th03::present::MainStaticPresenter;
using th03::present::MainWarningGaugePresenter;
using th03::present::MainWarningMrsAssets;
using th03::present::PackedRowOrder;
using th03::present::Page;
using th03::present::PelletMasks;
using th03::present::PreparedSuperimposeSheet;
using th03::present::Raster;
using th03::present::Rgb8;
using th03::present::ScoreFontMasks;
using th03::present::StaffFlakeTrigTables;
using th03::present::Tram;
using th03::present::TramCell;
using th03::present::authentic_main_bomb_atlas_layout;
using th03::present::authentic_main_background_atlas_layout;
using th03::present::authentic_main_effects_atlas_layout;
using th03::present::authentic_main_entity_atlas_layout;
using th03::present::authentic_main_players_bullets_atlas_layout;
using th03::present::graphics_page_pixels;
using th03::present::pellet_mask_bytes;
using th03::present::prepare_superimpose_sheet;
using th03::present::score_font_mask_bytes;
using th03::present::sprite16_source_top;
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
	BfntSheet lose = make_sheet(64U, 32U, 4U, 2U);
	BfntSheet round = make_sheet(32U, 16U, 22U, 4U);
	BfntSheet zikicw = make_sheet(32U, 16U, 16U, 6U);
	std::array<std::uint8_t, pellet_mask_bytes> pellets{};
	std::array<std::uint8_t, score_font_mask_bytes> score_font{};
	std::array<std::array<TramCell, 32U>, 9U> result_labels{};
	std::array<TramCell, 4U> combo_hit{};
	std::array<TramCell, 8U> combo_bonus{};
	std::array<TramCell, 8U> combo_erase{};
	std::array<std::int16_t, 256U> sine{};
	std::array<std::int16_t, 256U> cosine{};
	MrsImage p1_mrs = make_mrs(2U, MrsOrientation::as_stored);
	MrsImage p2_mrs = make_mrs(3U, MrsOrientation::horizontally_flipped);

	SyntheticAssets()
	{
		cosine.fill(256);
		score_font.fill(0xFFU);
		for(std::size_t label = 0U; label < result_labels.size(); ++label) {
			for(auto &cell : result_labels[label]) {
				cell = TramCell{
					static_cast<std::uint16_t>('A' + label), tram_white
				};
			}
		}
		combo_hit.fill(TramCell{'H', tram_white});
		combo_bonus.fill(TramCell{'B', tram_white});
		combo_erase.fill(TramCell{' ', tram_white});
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

	[[nodiscard]] MainWarningMrsAssets warning_mrs() const noexcept
	{
		return {{&p1_mrs, &p2_mrs}};
	}

	[[nodiscard]] MainHudResultTramAssets hud_result_tram() const noexcept
	{
		MainHudResultTramAssets result{};
		for(std::size_t label = 0U; label < result.result_labels.size(); ++label) {
			const auto size = (label < 2U) ? 32U : 26U;
			result.result_labels[label] = std::span<const TramCell>(
				result_labels[label].data(), size
			);
		}
		result.combo_hit = combo_hit;
		result.combo_bonus = combo_bonus;
		result.combo_erase = combo_erase;
		return result;
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
	std::optional<PreparedSuperimposeSheet> lose;
	std::optional<PreparedSuperimposeSheet> round_sheet;
	std::optional<PreparedSuperimposeSheet> zikicw;
	std::optional<MainEntitiesPresenter> entities;
	std::optional<MainBackgroundPresenter> background;
	std::optional<MainBossPresenter> bosses;
	std::optional<MainEffectsPresenter> effects;
	std::optional<MainExtraChargePresenter> extra_charge;
	std::optional<MainPlayersBulletsPresenter> players_bullets;
	std::optional<MainBombsPresenter> bombs;
	std::optional<MainWarningGaugePresenter> warning_gauge;
	std::optional<MainHudResultPresenter> hud_result;
	std::optional<MainRoundPresenter> round;
	MainHudStartState hud_start_late_{};

	explicit Fixture(
		std::uint16_t background_x_q12_4 = 0U,
		std::uint16_t background_y_q12_4 = 0U,
		MainBackgroundKind background_kind = MainBackgroundKind::type_a
	)
	{
		const auto hud = hud_state();
		expect(raster.set_access_page(Page::one), "access the MAINL atlas page");
		const auto background_layout = authentic_main_background_atlas_layout();
		for(const auto rect : background_layout.type_a_variants) {
			expect(
				raster.clear(
					{
						static_cast<std::int32_t>(rect.left),
						static_cast<std::int32_t>(
							sprite16_source_top + rect.raw_top
						),
						rect.width,
						rect.raw_height,
					},
					8U
				),
				"seed a Type-A background source cel"
			);
		}
		expect(
			raster.clear(
				{
					static_cast<std::int32_t>(background_layout.type_b_static.left),
					static_cast<std::int32_t>(
						sprite16_source_top +
						background_layout.type_b_static.raw_top
					),
					background_layout.type_b_static.width,
					background_layout.type_b_static.raw_height,
				},
				8U
			),
			"seed the Type-B static background source cel"
		);
		for(const auto rect : background_layout.type_b_animation) {
			expect(
				raster.clear(
					{
						static_cast<std::int32_t>(rect.left),
						static_cast<std::int32_t>(
							sprite16_source_top + rect.raw_top
						),
						rect.width,
						rect.raw_height,
					},
					8U
				),
				"seed a Type-B animated background source cel"
			);
		}
		expect(
			raster.clear({64, sprite16_source_top, 16U, 8U}, 9U) &&
			raster.clear({96, sprite16_source_top + 8, 96U, 48U}, 10U) &&
			raster.clear({0, sprite16_source_top + 8, 48U, 24U}, 4U) &&
			raster.clear({320, sprite16_source_top + 56, 192U, 24U}, 5U) &&
			raster.clear({32, sprite16_source_top + 8, 32U, 16U}, 11U) &&
			raster.clear({352, sprite16_source_top + 8, 32U, 16U}, 11U) &&
			raster.clear(
				{32, sprite16_source_top + 80, 32U, 16U}, 7U
			) && raster.clear(
				{128, sprite16_source_top + 80, 48U, 24U}, 6U
			),
			"seed entity, Extra, Charge, and effect cels in the handoff"
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
		background.emplace(raster, staging, background_layout);
		bosses.emplace(raster, staging, assets.trig(), palette);
		entities.emplace(
			raster, staging, authentic_main_entity_atlas_layout()
		);
		effects.emplace(
			raster,
			staging,
			authentic_main_effects_atlas_layout(),
			assets.trig()
		);
		extra_charge.emplace(raster, staging, assets.trig());
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
		warning_gauge.emplace(
			raster,
			tram,
			staging,
			palette,
			assets.warning_mrs()
		);
		auto prepared_lose = prepare_superimpose_sheet(assets.lose);
		auto prepared_round = prepare_superimpose_sheet(assets.round);
		auto prepared_zikicw = prepare_superimpose_sheet(assets.zikicw);
		expect(
			prepared_lose.has_value() && prepared_round.has_value() &&
			prepared_zikicw.has_value(),
			"prepare the synthetic HUD/result sheets"
		);
		if(prepared_lose && prepared_round && prepared_zikicw) {
			lose.emplace(std::move(*prepared_lose));
			round_sheet.emplace(std::move(*prepared_round));
			zikicw.emplace(std::move(*prepared_zikicw));
		} else {
			std::abort();
		}
		hud_result.emplace(
			raster,
			tram,
			ScoreFontMasks{
				assets.score_font, PackedRowOrder::bottom_to_top
			},
			assets.trig(),
			MainHudResultSheets{&*lose, &*round_sheet, &*zikicw},
			assets.hud_result_tram(),
			std::uint8_t{0U}
		);
		round.emplace(
			raster,
			tram,
			static_presenter,
			palette,
			*background,
			*bosses,
			*entities,
			*effects,
			*extra_charge,
			*players_bullets,
			*bombs,
			*warning_gauge,
			*hud_result
		);
		expect(round->ready(), "construct a ready MAIN round composer");
		round->main_background_initialize({.kind = background_kind});
		const auto background_slots = (background_kind ==
			MainBackgroundKind::type_a) ? 48U : 50U;
		for(std::size_t slot = 0U; slot < background_slots; ++slot) {
			round->main_background_random_read({background_x_q12_4});
			round->main_background_random_read({background_y_q12_4});
			if(background_kind == MainBackgroundKind::type_a) {
				round->main_background_random_read({0U});
				round->main_background_random_read({0U});
			}
		}
		expect(round->ready(), "initialize the synthetic MAIN background pool");
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
		state.game_mode = th03::detail::game_mode_vs_1p_2p;
		state.hud_start_flag = 0U;
		state.hud_start_state = 4U;
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

	[[nodiscard]] static WarningFrameLifecycle idle_warning() noexcept
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

	[[nodiscard]] static WarningInterstitialStarted warning_start(
		bool p1 = true,
		bool p2 = false
	) noexcept
	{
		return {
			.flashing_players = {p1, p2},
			.next_attacks = {
				p1 ? GaugeBossAttackKind::gauge_pellet :
					GaugeBossAttackKind::none,
				p2 ? GaugeBossAttackKind::boss : GaugeBossAttackKind::none,
			},
			.gauge_levels = {3U, 7U},
			.boss_level = 9U,
		};
	}

	[[nodiscard]] static MainGaugeState gauge_state(
		const th03::detail::DeterministicState &state
	) noexcept
	{
		return {
			.context = {
				.round_id = state.round_id,
				.round_or_result_frame = state.round_or_result_frame,
				.playfield_fg_shift_x = state.playfield_fg_shift_x,
			},
		};
	}

	[[nodiscard]] static th03::MainPhaseContext phase_context(
		const th03::detail::DeterministicState &state
	) noexcept
	{
		return {
			.round_id = state.round_id,
			.round_or_result_frame = state.round_or_result_frame,
			.playfield_fg_shift_x = state.playfield_fg_shift_x,
		};
	}

	[[nodiscard]] static MainResultState result_state(
		const th03::detail::DeterministicState &state
	) noexcept
	{
		return {
			.context = phase_context(state),
			.story_stage = state.story_stage,
			.story_lives = state.story_lives,
			.rounds_won = {
				state.players[0U].rounds_won,
				state.players[1U].rounds_won,
			},
		};
	}

	[[nodiscard]] static MainScoreState score_state(
		const th03::detail::DeterministicState &state
	) noexcept
	{
		return {.context = phase_context(state), .digits = state.score};
	}

	[[nodiscard]] static MainComboState combo_state(
		const th03::detail::DeterministicState &state
	) noexcept
	{
		return {.context = phase_context(state)};
	}

	[[nodiscard]] static MainResultTransitionState transition_state(
		const th03::detail::DeterministicState &state
	) noexcept
	{
		return {
			.context = phase_context(state),
			.phase = MainResultTransitionPhase::idle,
			.frame_before = 0U,
		};
	}

	void hud_start_prefix(const th03::detail::DeterministicState &state)
	{
		const auto initialize = state.hud_start_flag == 1U;
		auto state_before = initialize ? std::uint8_t{0U} : state.hud_start_state;
		auto frame_before = initialize ? std::uint16_t{0U} : state.hud_start_frame;
		round->hud_start_frame_step(HudStartFrameStep{
			.initialize = initialize,
			.story_mode = state.game_mode == th03::detail::game_mode_story,
			.story_stage = state.story_stage,
			.round_id = state.round_id,
			.state_before = state_before,
			.frame_before = frame_before,
		});
		auto state_after = state_before;
		auto frame_after = frame_before;
		if(state_before == 0U) {
			frame_after++;
			if(frame_after >= 69U) {
				state_after = 1U;
				frame_after = 0U;
			}
		} else if(state_before == 1U) {
			frame_after++;
			if(frame_after > 0x56U) {
				state_after = 2U;
				frame_after = 0U;
			}
		} else if(state_before == 2U) {
			state_after = 3U;
			frame_after = 0U;
		} else if(state_before == 3U) {
			frame_after++;
			if(frame_after >= 0x40U) {
				state_after = 4U;
			}
		}
		hud_start_late_ = MainHudStartState{
			.round_id = state.round_id,
			.story_mode = state.game_mode == th03::detail::game_mode_story,
			.story_stage = state.story_stage,
			.state_after_prefix = state_after,
			.frame_after_prefix = frame_after,
		};
	}

	void render_phases(const th03::detail::DeterministicState &state)
	{
		round->observe(th03::detail::ObserverAccess::main_boss(state));
		round->observe(th03::detail::ObserverAccess::shots_enemies(state));
		round->observe(th03::detail::ObserverAccess::fireballs(state));
		extra_charge_phases(state);
		round->observe(th03::detail::ObserverAccess::players_bullets(state));
		round->main_result_state(result_state(state));
	}

	void extra_charge_phases(const th03::detail::DeterministicState &state)
	{
		round->observe(th03::detail::ObserverAccess::main_extra_attack(
			state, PlayerId::p1
		));
		round->observe(th03::detail::ObserverAccess::main_charge_shot(
			state, PlayerId::p1
		));
		round->observe(th03::detail::ObserverAccess::main_extra_attack(
			state, PlayerId::p2
		));
		round->observe(th03::detail::ObserverAccess::main_charge_shot(
			state, PlayerId::p2
		));
	}

	void background_frame(const th03::detail::DeterministicState &state)
	{
		round->main_background_frame({
			.round_id = state.round_id,
			.round_or_result_frame = state.round_or_result_frame,
			.kind = MainBackgroundKind::type_a,
			.phase = MainBackgroundPhase::initial,
		});
		for(std::size_t slot = 0U; slot < 48U; ++slot) {
			round->main_background_random_read({0U});
		}
	}

	void begin_frame(const th03::detail::DeterministicState &state)
	{
		hud_start_prefix(state);
		round->warning_frame_lifecycle(idle_warning());
		background_frame(state);
	}

	void terminal_phases(const th03::detail::DeterministicState &state)
	{
		begin_frame(state);
		render_phases(state);
	}

	void ordinary_phases(const th03::detail::DeterministicState &state)
	{
		terminal_phases(state);
		round->main_gauge_state(gauge_state(state));
		finish_after_gauge(state);
	}

	void finish_after_gauge(const th03::detail::DeterministicState &state)
	{
		round->main_score_state(score_state(state));
		finish_after_score(state);
	}

	void finish_after_score(const th03::detail::DeterministicState &state)
	{
		round->main_hud_start_state(hud_start_late_);
		round->main_combo_state(combo_state(state));
		round->main_result_transition_state(transition_state(state));
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

void open_extra_p1(
	Fixture &fixture,
	const th03::detail::DeterministicState &state
)
{
	fixture.begin_frame(state);
	fixture.round->observe(th03::detail::ObserverAccess::main_boss(state));
	fixture.round->observe(th03::detail::ObserverAccess::shots_enemies(state));
	fixture.round->observe(th03::detail::ObserverAccess::fireballs(state));
}

void test_contract_and_setup_failure()
{
	static_assert(!std::is_copy_constructible_v<MainRoundPresenter>);
	static_assert(!std::is_move_constructible_v<MainRoundPresenter>);

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
		*fixture.background,
		*fixture.bosses,
		*fixture.entities,
		*fixture.effects,
		*fixture.extra_charge,
		*fixture.players_bullets,
		*fixture.bombs,
		*fixture.warning_gauge,
		*fixture.hud_result
	);
	expect(
		stopped.status() == MainRoundStatus::invalid_setup &&
		fixture.raster.palette_tone() == 50,
		"reject an unstarted process without applying construction tone"
	);
	auto invalid_background_layout = authentic_main_background_atlas_layout();
	invalid_background_layout.type_a_variants[0U].width = 32U;
	MainBackgroundPresenter invalid_background(
		fixture.raster,
		fixture.static_presenter.sprite_staging(),
		invalid_background_layout
	);
	MainRoundPresenter stopped_background(
		fixture.raster,
		fixture.tram,
		fixture.static_presenter,
		fixture.palette,
		invalid_background,
		*fixture.bosses,
		*fixture.entities,
		*fixture.effects,
		*fixture.extra_charge,
		*fixture.players_bullets,
		*fixture.bombs,
		*fixture.warning_gauge,
		*fixture.hud_result
	);
	expect(
		stopped_background.status() == MainRoundStatus::background_failed &&
		fixture.raster.palette_tone() == 50,
		"reject an invalid background child before applying construction tone"
	);
	MainExtraChargePresenter invalid_extra_charge(
		fixture.raster,
		fixture.static_presenter.sprite_staging(),
		StaffFlakeTrigTables{}
	);
	MainRoundPresenter stopped_extra_charge(
		fixture.raster,
		fixture.tram,
		fixture.static_presenter,
		fixture.palette,
		*fixture.background,
		*fixture.bosses,
		*fixture.entities,
		*fixture.effects,
		invalid_extra_charge,
		*fixture.players_bullets,
		*fixture.bombs,
		*fixture.warning_gauge,
		*fixture.hud_result
	);
	expect(
		stopped_extra_charge.status() == MainRoundStatus::extra_charge_failed &&
		fixture.raster.palette_tone() == 50,
		"reject an invalid Extra/Charge child before applying construction tone"
	);
	MainRoundPresenter valid(
		fixture.raster,
		fixture.tram,
		fixture.static_presenter,
		fixture.palette,
		*fixture.background,
		*fixture.bosses,
		*fixture.entities,
		*fixture.effects,
		*fixture.extra_charge,
		*fixture.players_bullets,
		*fixture.bombs,
		*fixture.warning_gauge,
		*fixture.hud_result
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
		auto state = fixture.state();
		fixture.begin_frame(state);
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

void test_extra_charge_order_is_strict()
{
	{
		Fixture fixture;
		auto state = fixture.state();
		open_extra_p1(fixture, state);
		const auto before = page_copy(fixture.raster, Page::zero);
		fixture.round->observe(
			th03::detail::ObserverAccess::players_bullets(state)
		);
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation &&
			std::ranges::equal(before, fixture.raster.graphics(Page::zero)),
			"reject skipping all four Extra/Charge callbacks before Players"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		open_extra_p1(fixture, state);
		fixture.round->observe(
			th03::detail::ObserverAccess::main_charge_shot(state, PlayerId::p1)
		);
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject Charge P1 before Extra P1"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		open_extra_p1(fixture, state);
		fixture.round->observe(
			th03::detail::ObserverAccess::main_extra_attack(state, PlayerId::p1)
		);
		fixture.round->observe(
			th03::detail::ObserverAccess::main_charge_shot(state, PlayerId::p1)
		);
		fixture.round->observe(
			th03::detail::ObserverAccess::main_charge_shot(state, PlayerId::p1)
		);
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject a duplicated Charge P1 after its phase advances"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		open_extra_p1(fixture, state);
		fixture.round->observe(
			th03::detail::ObserverAccess::main_extra_attack(state, PlayerId::p2)
		);
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject P2 Extra at the P1 phase"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		open_extra_p1(fixture, state);
		state.playfield_fg_shift_x[1U] = 3;
		fixture.round->observe(
			th03::detail::ObserverAccess::main_extra_attack(state, PlayerId::p1)
		);
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject Extra when any field of its MAIN phase context is stale"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		open_extra_p1(fixture, state);
		state.players[0U].paletted_character = 0;
		const auto shown = fixture.raster.shown_page();
		fixture.round->observe(
			th03::detail::ObserverAccess::main_extra_attack(state, PlayerId::p1)
		);
		expect(
			fixture.round->status() == MainRoundStatus::extra_charge_failed &&
			!fixture.extra_charge->ready(),
			"surface a malformed forwarded Extra view as the named child failure"
		);
		fixture.round->main_frame_footer_reached({});
		expect(
			fixture.round->status() == MainRoundStatus::extra_charge_failed &&
			fixture.raster.shown_page() == shown,
			"keep an Extra/Charge child failure sticky and suppress publication"
		);
	}
}

void test_extra_then_charge_overlap_order()
{
	Fixture fixture;
	auto state = fixture.state();
	constexpr std::int16_t center_x = 144 * 16;
	constexpr std::int16_t center_y = 336 * 16;
	auto &extra = state.extra_attacks[0U][0U];
	extra.state = 1U;
	extra.frame = 0U;
	extra.center_x = center_x;
	extra.center_y = center_y;
	extra.player_id = 1U;
	state.players[1U].paletted_character = 2;
	auto &charge = state.reimu_charge[1U][0U];
	charge.state = 1U;
	charge.x.fill(center_x);
	charge.y.fill(center_y);

	open_extra_p1(fixture, state);
	fixture.round->observe(
		th03::detail::ObserverAccess::main_extra_attack(state, PlayerId::p1)
	);
	expect(
		fixture.round->ready() &&
		fixture.raster.pixel(Page::zero, {480, 170}) == 4U,
		"draw P1 Extra on P2's playfield before either player's Charge"
	);
	fixture.round->observe(
		th03::detail::ObserverAccess::main_charge_shot(state, PlayerId::p1)
	);
	expect(
		fixture.round->ready() &&
		fixture.raster.pixel(Page::zero, {480, 170}) == 4U,
		"inactive P1 Charge leaves the earlier P1 Extra pixel unchanged"
	);
	fixture.round->observe(
		th03::detail::ObserverAccess::main_extra_attack(state, PlayerId::p2)
	);
	expect(
		fixture.round->ready() &&
		fixture.raster.pixel(Page::zero, {480, 170}) == 4U,
		"inactive P2 Extra leaves the earlier P1 Extra pixel unchanged"
	);
	fixture.round->observe(
		th03::detail::ObserverAccess::main_charge_shot(state, PlayerId::p2)
	);
	expect(
		fixture.round->ready() &&
		fixture.raster.pixel(Page::zero, {480, 170}) == 5U,
		"later P2 Charge overwrites an overlapping P1 Extra pixel"
	);
}

void test_phase_context_includes_foreground_shifts()
{
	Fixture fixture;
	auto state = fixture.state();
	fixture.begin_frame(state);
	fixture.round->observe(th03::detail::ObserverAccess::main_boss(state));
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
	fixture.begin_frame(state);
	fixture.round->observe(th03::detail::ObserverAccess::main_boss(state));
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
	fixture.begin_frame(state);
	fixture.round->observe(th03::detail::ObserverAccess::main_boss(state));
	fixture.round->observe(th03::detail::ObserverAccess::shots_enemies(state));
	fixture.round->observe(th03::detail::ObserverAccess::fireballs(state));
	expect(
		fixture.round->ready() &&
		fixture.raster.pixel(Page::zero, {160, 176}) == 12U,
		"odd-frame hit-circle color overwrites the overlapping Fireball cel"
	);
}

void test_boss_view_precedes_entities_and_draws()
{
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.begin_frame(state);
		fixture.round->observe(
			th03::detail::ObserverAccess::shots_enemies(state)
		);
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation &&
			fixture.bosses->ready() && fixture.entities->ready(),
			"require the source Boss view before Shots and Enemies"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		state.gba_boss_launched_by = 0;
		state.boss.center_x = static_cast<std::int16_t>(144 * 16);
		state.boss.center_y = static_cast<std::int16_t>(80 * 16);
		state.boss.state = 1U;
		fixture.round->boss_palette_baseline_restored({PlayerId::p2});
		fixture.round->boss_activated({
			.owner = PlayerId::p1,
			.paletted_character = static_cast<std::int8_t>(
				MainPalettedCharacter::reimu_primary
			),
		});
		fixture.begin_frame(state);
		fixture.round->observe(th03::detail::ObserverAccess::main_boss(state));
		expect(
			fixture.round->ready() &&
			fixture.raster.pixel(Page::zero, {432, 48}) == 10U,
			"render the active Boss synchronously after background and Bombs"
		);
		fixture.round->observe(
			th03::detail::ObserverAccess::shots_enemies(state)
		);
		expect(
			fixture.round->ready(),
			"accept Shots and Enemies only after the matching Boss context"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.begin_frame(state);
		state.round_or_result_frame++;
		fixture.round->observe(th03::detail::ObserverAccess::main_boss(state));
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation &&
			fixture.bosses->ready(),
			"reject a Boss view that does not match the background frame"
		);
	}
}

void test_boss_lifecycle_order_and_child_failure()
{
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.begin_frame(state);
		fixture.round->boss_palette_baseline_restored({PlayerId::p1});
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation &&
			fixture.bosses->ready(),
			"reject a prefix Boss palette restore after background rendering"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		state.gba_boss_launched_by = 0;
		state.players[0U].paletted_character = 0;
		fixture.begin_frame(state);
		const auto before = page_copy(fixture.raster, Page::zero);
		fixture.round->observe(th03::detail::ObserverAccess::main_boss(state));
		expect(
			fixture.round->status() == MainRoundStatus::bosses_failed &&
			!fixture.bosses->ready() && fixture.entities->ready() &&
			std::ranges::equal(before, fixture.raster.graphics(Page::zero)),
			"surface malformed Boss facts as the named child failure"
		);
	}
}

void test_late_bomb_phase_and_player_order()
{
	{
		Fixture fixture;
		const auto before = page_copy(fixture.raster, Page::zero);
		fixture.round->bomb_frame_step(BombFrameStep{
			.player = PlayerId::p1,
			.paletted_character = static_cast<std::int8_t>(
				MainPalettedCharacter::reimu_primary
			),
			.bomb_frame = 1U,
		});
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation &&
			fixture.bombs->ready() &&
			std::ranges::equal(before, fixture.raster.graphics(Page::zero)),
			"reject a late Bomb callback before warning and background"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.begin_frame(state);
		fixture.round->observe(th03::detail::ObserverAccess::main_boss(state));
		const auto before = page_copy(fixture.raster, Page::zero);
		fixture.round->bomb_frame_step(BombFrameStep{
			.player = PlayerId::p1,
			.paletted_character = static_cast<std::int8_t>(
				MainPalettedCharacter::reimu_primary
			),
			.bomb_frame = 1U,
		});
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation &&
			fixture.bombs->ready() &&
			std::ranges::equal(before, fixture.raster.graphics(Page::zero)),
			"reject Bomb drawing after the Boss view"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.begin_frame(state);
		fixture.round->bomb_frame_step(BombFrameStep{
			.player = PlayerId::p2,
			.paletted_character = static_cast<std::int8_t>(
				MainPalettedCharacter::kana_primary
			),
			.bomb_frame = 1U,
		});
		fixture.round->bomb_frame_step(BombFrameStep{
			.player = PlayerId::p1,
			.paletted_character = static_cast<std::int8_t>(
				MainPalettedCharacter::reimu_primary
			),
			.bomb_frame = 1U,
		});
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject a P1 late Bomb callback after P2"
		);
	}
}

void test_background_is_synchronous_before_late_bombs()
{
	Fixture fixture(
		static_cast<std::uint16_t>(100U * 16U),
		static_cast<std::uint16_t>(100U * 16U)
	);
	auto state = fixture.state();
	fixture.hud_start_prefix(state);
	fixture.round->warning_frame_lifecycle(Fixture::idle_warning());
	fixture.round->main_background_frame({
		.round_id = 3U,
		.round_or_result_frame = 0U,
		.kind = MainBackgroundKind::type_a,
		.phase = MainBackgroundPhase::initial,
	});
	for(std::size_t slot = 0U; slot < 48U; ++slot) {
		fixture.round->main_background_random_read({0U});
	}
	expect(
		fixture.round->ready() &&
		fixture.raster.pixel(Page::zero, {100, 50}) == 8U,
		"background frame facts draw synchronously in their source slot"
	);

	fixture.round->bomb_frame_step(BombFrameStep{
		.player = PlayerId::p1,
		.paletted_character = static_cast<std::int8_t>(
			MainPalettedCharacter::reimu_primary
		),
		.bomb_frame = 1U,
	});
	expect(
		fixture.round->ready() &&
		fixture.raster.pixel(Page::zero, {100, 50}) == 0U,
		"later Reimu Bomb fill overwrites the earlier background pixel"
	);
	fixture.round->bomb_compatibility_read({
		.player = PlayerId::p1,
		.site = BombCompatibilityReadSite::reimu_falling_velocity,
		.bomb_frame = 1U,
		.raw_value = 0U,
	});
	fixture.round->bomb_compatibility_read({
		.player = PlayerId::p1,
		.site = BombCompatibilityReadSite::reimu_falling_x,
		.bomb_frame = 1U,
		.raw_value = 0U,
	});
	expect(
		fixture.round->ready(),
		"complete the late Bomb compatibility-read suffix after the order probe"
	);
}

void test_background_failure_is_sticky()
{
	Fixture fixture;
	const auto shown = fixture.raster.shown_page();
	fixture.round->main_background_random_read({0U});
	expect(
		fixture.round->status() == MainRoundStatus::background_failed &&
		!fixture.background->ready(),
		"surface an invalid background fact as its named child failure"
	);
	fixture.round->main_frame_footer_reached({});
	expect(
		fixture.raster.shown_page() == shown,
		"a sticky background failure suppresses later page publication"
	);
}

void test_warning_lifecycle_precedes_background()
{
	Fixture fixture;
	auto state = fixture.state();
	fixture.hud_start_prefix(state);
	fixture.round->main_background_frame({
		.round_id = 3U,
		.round_or_result_frame = 17U,
		.kind = MainBackgroundKind::type_a,
		.phase = MainBackgroundPhase::initial,
	});
	expect(
		fixture.round->status() == MainRoundStatus::invalid_observation &&
		fixture.background->ready(),
		"reject a background frame before the frame's warning lifecycle"
	);
}

void test_background_precedes_entity_phase()
{
	Fixture fixture;
	auto state = fixture.state();
	fixture.hud_start_prefix(state);
	fixture.round->warning_frame_lifecycle(Fixture::idle_warning());
	fixture.round->observe(th03::detail::ObserverAccess::main_boss(state));
	expect(
		fixture.round->status() == MainRoundStatus::invalid_observation &&
		fixture.background->ready() && fixture.entities->ready(),
		"reject the Boss phase when the source background callback is missing"
	);
}

void test_background_reads_complete_before_entity_phase()
{
	Fixture fixture;
	auto state = fixture.state();
	fixture.hud_start_prefix(state);
	fixture.round->warning_frame_lifecycle(Fixture::idle_warning());
	fixture.round->main_background_frame({
		.round_id = state.round_id,
		.round_or_result_frame = state.round_or_result_frame,
		.kind = MainBackgroundKind::type_a,
		.phase = MainBackgroundPhase::initial,
	});
	fixture.round->observe(th03::detail::ObserverAccess::main_boss(state));
	expect(
		fixture.round->status() == MainRoundStatus::invalid_observation &&
		fixture.background->ready() && fixture.entities->ready(),
		"reject Boss rendering before all source background reads have completed"
	);
}

void test_type_b_background_read_count_and_overread()
{
	{
		auto fixture = std::make_unique<Fixture>(
			std::uint16_t{0U}, std::uint16_t{0U},
			MainBackgroundKind::type_b
		);
		auto state = fixture->state();
		fixture->hud_start_prefix(state);
		fixture->round->warning_frame_lifecycle(Fixture::idle_warning());
		fixture->round->main_background_frame({
			.round_id = state.round_id,
			.round_or_result_frame = state.round_or_result_frame,
			.kind = MainBackgroundKind::type_b,
			.phase = MainBackgroundPhase::initial,
		});
		for(std::size_t read = 0U; read < 40U; ++read) {
			fixture->round->main_background_random_read({0U});
		}
		fixture->round->observe(
			th03::detail::ObserverAccess::main_boss(state)
		);
		fixture->round->observe(
			th03::detail::ObserverAccess::shots_enemies(state)
		);
		expect(
			fixture->round->ready(),
			"accept exactly 40 Type-B initial-frame background reads"
		);
	}
	{
		auto fixture = std::make_unique<Fixture>(
			std::uint16_t{0U}, std::uint16_t{0U},
			MainBackgroundKind::type_b
		);
		auto state = fixture->state();
		fixture->hud_start_prefix(state);
		fixture->round->warning_frame_lifecycle(Fixture::idle_warning());
		fixture->round->main_background_frame({
			.round_id = state.round_id,
			.round_or_result_frame = state.round_or_result_frame,
			.kind = MainBackgroundKind::type_b,
			.phase = MainBackgroundPhase::initial,
		});
		for(std::size_t read = 0U; read < 40U; ++read) {
			fixture->round->main_background_random_read({0U});
		}
		fixture->round->main_background_random_read({0U});
		expect(
			fixture->round->status() == MainRoundStatus::invalid_observation &&
			fixture->background->ready(),
			"reject a 41st Type-B frame read before it reaches the child"
		);
	}
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
	fixture.terminal_phases(state);
	expect(
		fixture.round->ready() && fixture.palette.dirty() &&
		fixture.raster.shown_page() == Page::one &&
		fixture.raster.display_state_hash() == shown_hash,
		"terminal return without footer keeps the prior shown surface"
	);
}

void test_late_gauge_portrait_and_footer_order()
{
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.terminal_phases(state);
		fixture.round->main_frame_footer_reached({});
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject a nonterminal footer before its late gauge callback"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.terminal_phases(state);
		auto gauge = Fixture::gauge_state(state);
		gauge.context.playfield_fg_shift_x[1U] = 4;
		fixture.round->main_gauge_state(gauge);
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject a late gauge whose full MAIN phase context differs"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.terminal_phases(state);
		fixture.round->warning_portrait_transition(
			WarningPortraitTransition{PlayerId::p1}
		);
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject a warning portrait before the late gauge"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		auto lifecycle = Fixture::idle_warning();
		lifecycle.players[0U].warning_step_before = 1U;
		lifecycle.players[0U].warning_step_after = 1U;
		lifecycle.players[0U].next_attack = GaugeBossAttackKind::gauge_pellet;
		lifecycle.players[1U].warning_step_before = 1U;
		lifecycle.players[1U].warning_step_after = 1U;
		lifecycle.players[1U].next_attack = GaugeBossAttackKind::boss;
		fixture.hud_start_prefix(state);
		fixture.round->warning_frame_lifecycle(lifecycle);
		fixture.background_frame(state);
		fixture.render_phases(state);
		fixture.round->main_gauge_state(Fixture::gauge_state(state));
		fixture.round->main_score_state(Fixture::score_state(state));
		fixture.round->warning_portrait_transition(
			WarningPortraitTransition{PlayerId::p2}
		);
		fixture.round->warning_portrait_transition(
			WarningPortraitTransition{PlayerId::p1}
		);
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject P1 after a P2 warning portrait"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		auto lifecycle = Fixture::idle_warning();
		lifecycle.players[0U].warning_step_before = 1U;
		lifecycle.players[0U].warning_step_after = 1U;
		lifecycle.players[0U].next_attack = GaugeBossAttackKind::gauge_pellet;
		lifecycle.players[1U].warning_step_before = 1U;
		lifecycle.players[1U].warning_step_after = 1U;
		lifecycle.players[1U].next_attack = GaugeBossAttackKind::boss;
		fixture.hud_start_prefix(state);
		fixture.round->warning_frame_lifecycle(lifecycle);
		fixture.background_frame(state);
		fixture.render_phases(state);
		fixture.round->main_gauge_state(Fixture::gauge_state(state));
		fixture.round->main_score_state(Fixture::score_state(state));
		fixture.round->warning_portrait_transition(
			WarningPortraitTransition{PlayerId::p1}
		);
		fixture.round->warning_portrait_transition(
			WarningPortraitTransition{PlayerId::p2}
		);
		fixture.finish_after_score(state);
		expect(
			fixture.round->ready() &&
			fixture.raster.pixel(Page::zero, {16, 16}) == 2U &&
			fixture.raster.pixel(Page::zero, {336, 16}) == 3U,
			"draw optional warning portraits in source P1-then-P2 order"
		);
		fixture.round->main_frame_footer_reached({});
		expect(
			fixture.round->ready() && fixture.raster.shown_page() == Page::zero,
			"accept the footer only after the nonterminal late-gauge suffix"
		);
	}
}

void test_portrait_expectation_is_strict()
{
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.ordinary_phases(state);
		fixture.round->warning_portrait_transition(
			WarningPortraitTransition{PlayerId::p1}
		);
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject a portrait absent from this frame's warning lifecycle"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		auto lifecycle = Fixture::idle_warning();
		lifecycle.players[0U].warning_step_before = 1U;
		lifecycle.players[0U].warning_step_after = 1U;
		lifecycle.players[0U].next_attack = GaugeBossAttackKind::gauge_pellet;
		fixture.hud_start_prefix(state);
		fixture.round->warning_frame_lifecycle(lifecycle);
		fixture.background_frame(state);
		fixture.render_phases(state);
		fixture.round->main_gauge_state(Fixture::gauge_state(state));
		fixture.round->main_score_state(Fixture::score_state(state));
		fixture.round->main_frame_footer_reached({});
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject a footer before its required warning portrait"
		);
	}
}

void test_gauge_completion_prefix_order()
{
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.hud_start_prefix(state);
		fixture.round->gauge_attack_finished({PlayerId::p2, PlayerId::p1});
		fixture.round->gauge_attack_finished({PlayerId::p1, PlayerId::p2});
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject a P1 Gauge completion after a P2 completion"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.hud_start_prefix(state);
		fixture.round->warning_frame_lifecycle(Fixture::idle_warning());
		fixture.round->gauge_attack_finished({PlayerId::p1, PlayerId::p2});
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation &&
			fixture.warning_gauge->ready(),
			"reject Gauge completion after the warning/background suffix begins"
		);
	}
}

void test_warning_gauge_child_failure_is_sticky()
{
	Fixture fixture;
	auto state = fixture.state();
	fixture.hud_start_prefix(state);
	const auto shown = fixture.raster.shown_page();
	fixture.round->gauge_attack_finished(GaugeAttackFinished{
		.attacker = PlayerId::p1,
		.target = PlayerId::p1,
	});
	expect(
		fixture.round->status() == MainRoundStatus::warning_gauge_failed &&
		!fixture.warning_gauge->ready(),
		"surface a malformed forwarded gauge completion as a child failure"
	);
	fixture.round->main_frame_footer_reached({});
	expect(
		fixture.raster.shown_page() == shown,
		"a sticky warning/gauge child failure suppresses later publication"
	);
}

void test_warning_interstitial_tones_without_footer()
{
	Fixture fixture;
	th03::GameplayObserver &observer = *fixture.round;
	auto state = fixture.state();
	const auto shown = fixture.raster.shown_page();
	const auto access = fixture.raster.access_page();
	expect(
		fixture.palette.set_entry_deferred(
			fixture.raster, 6U, Rgb8{0x7FU, 0x3FU, 0x1FU}
		),
		"stage a dirty palette entry before the blocking warning"
	);
	fixture.hud_start_prefix(state);
	observer.warning_interstitial_started(Fixture::warning_start());
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

	auto lifecycle = Fixture::idle_warning();
	lifecycle.players[0U] = {
		3U, 4U, GaugeBossAttackKind::gauge_pellet, GaugeBossAttackKind::none,
	};
	lifecycle.gauge_levels = {3U, 7U};
	lifecycle.boss_level = 9U;
	fixture.round->warning_frame_lifecycle(lifecycle);
	fixture.background_frame(state);
	fixture.render_phases(state);
	fixture.round->main_gauge_state(Fixture::gauge_state(state));
	fixture.finish_after_gauge(state);
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
		auto state = fixture.state();
		fixture.hud_start_prefix(state);
		fixture.round->warning_interstitial_started(Fixture::warning_start());
		fixture.round->warning_interstitial_sample({1U});
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject a skipped warning mini-loop ordinal"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.hud_start_prefix(state);
		fixture.round->warning_interstitial_started(Fixture::warning_start());
		for(std::uint8_t ordinal = 0U; ordinal < 27U; ++ordinal) {
			fixture.round->warning_interstitial_sample({ordinal});
		}
		fixture.round->warning_interstitial_started(
			Fixture::warning_start(false, true)
		);
		expect(
			fixture.round->status() == MainRoundStatus::invalid_observation,
			"reject another warning start before sample 26's resumed suffix"
		);
	}
	{
		Fixture fixture;
		auto state = fixture.state();
		fixture.hud_start_prefix(state);
		fixture.round->warning_interstitial_started(Fixture::warning_start());
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
	fixture.begin_frame(state);
	fixture.round->observe(th03::detail::ObserverAccess::main_boss(state));
	fixture.round->observe(th03::detail::ObserverAccess::shots_enemies(state));
	fixture.round->observe(th03::detail::ObserverAccess::fireballs(state));
	fixture.extra_charge_phases(state);
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
	fixture.terminal_phases(state);
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
	const auto *extra_charge = std::addressof(*fixture.extra_charge);
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
	auto terminal_warning = Fixture::idle_warning();
	terminal_warning.players[0U].warning_step_before = 1U;
	terminal_warning.players[0U].warning_step_after = 1U;
	terminal_warning.players[0U].next_attack = GaugeBossAttackKind::boss;
	state.players[0U].paletted_character = 11;
	auto &kana = state.extra_attacks[0U][0U];
	kana.state = 3U;
	kana.frame = 0U;
	kana.center_x = 100 << 4;
	kana.center_y = 80 << 4;
	kana.player_id = 1U;
	fixture.hud_start_prefix(state);
	fixture.round->warning_frame_lifecycle(terminal_warning);
	fixture.background_frame(state);
	fixture.round->observe(th03::detail::ObserverAccess::main_boss(state));
	fixture.round->observe(th03::detail::ObserverAccess::shots_enemies(state));
	fixture.round->observe(th03::detail::ObserverAccess::fireballs(state));
	fixture.extra_charge_phases(state);
	state.players[0U].paletted_character = 1;
	fixture.round->observe(
		th03::detail::ObserverAccess::players_bullets(state)
	);
	auto terminal_result = Fixture::result_state(state);
	terminal_result.defeated_player = PlayerId::p1;
	terminal_result.winner = PlayerId::p2;
	fixture.round->main_result_state(terminal_result);
	expect(
		fixture.round->repeat_round_startup(Fixture::hud_state()),
		"terminal route-1 repeat skips a pending source warning portrait"
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
		std::addressof(*fixture.bombs) == bombs &&
		std::addressof(*fixture.extra_charge) == extra_charge,
		"repeat startup preserves process-scoped child presenters and pools"
	);

	state.round_id++;
	state.round_or_result_frame = 0U;
	state.hud_start_flag = 1U;
	state.hud_start_state = 0U;
	state.hud_start_frame = 0U;
	state.players[0U].paletted_character = 11;
	kana.state = 1U;
	kana.frame = 0U;
	kana.player_id = 0U;
	fixture.round->common_effects_advanced();
	open_extra_p1(fixture, state);
	const auto normalized_destination_pixel = fixture.raster.pixel(
		Page::zero, {100, 40}
	);
	fixture.round->observe(
		th03::detail::ObserverAccess::main_extra_attack(state, PlayerId::p1)
	);
	expect(
		fixture.round->ready() && fixture.raster.pixel(
			Page::zero, {100, 40}
		) == normalized_destination_pixel && fixture.raster.pixel(
			Page::zero, {420, 40}
		) == 11U,
		"repeat retains Kana's stale destination cache and draws at the retained rather than normalized coordinate"
	);
	fixture.round->observe(
		th03::detail::ObserverAccess::main_charge_shot(state, PlayerId::p1)
	);
	fixture.round->observe(
		th03::detail::ObserverAccess::main_extra_attack(state, PlayerId::p2)
	);
	fixture.round->observe(
		th03::detail::ObserverAccess::main_charge_shot(state, PlayerId::p2)
	);
	state.players[0U].paletted_character = 1;
	fixture.round->observe(
		th03::detail::ObserverAccess::players_bullets(state)
	);
	expect(
		fixture.raster.pixel(Page::zero, {176, 176}) == 13U,
		"repeat startup preserves the process-scoped common-effect pool"
	);
	fixture.round->main_result_state(Fixture::result_state(state));
	fixture.round->main_gauge_state(Fixture::gauge_state(state));
	fixture.round->main_score_state(Fixture::score_state(state));
	fixture.round->main_hud_start_state(fixture.hud_start_late_);
	fixture.round->main_combo_state(Fixture::combo_state(state));
	auto closing = Fixture::transition_state(state);
	closing.phase = MainResultTransitionPhase::closing;
	fixture.round->main_result_transition_state(closing);
	expect(
		fixture.round->ready(),
		"first repeated round forwards closing frame zero through the coordinator"
	);
	fixture.round->main_frame_footer_reached({});
	expect(
		fixture.round->ready(),
		"first repeated-round closing reaches its source footer"
	);
	expect(
		!fixture.palette.dirty(),
		"first repeated-round footer consumes the carried palette latch"
	);
}

void test_repeat_round_startup_requires_result_banner()
{
	Fixture fixture;
	const auto state = fixture.state();
	fixture.terminal_phases(state);
	expect(
		!fixture.round->repeat_round_startup(Fixture::hud_state()) &&
		fixture.round->status() == MainRoundStatus::invalid_observation,
		"route-1 repeat rejects a result gate without the source defeat banner"
	);
}

} // namespace

int main()
{
	test_contract_and_setup_failure();
	test_source_order_footer_and_surface();
	test_common_effect_fanout_stops_before_bombs();
	test_wrong_order_is_sticky_and_atomic();
	test_extra_charge_order_is_strict();
	test_extra_then_charge_overlap_order();
	test_phase_context_includes_foreground_shifts();
	test_fireballs_precede_hit_circles();
	test_hit_circles_overwrite_fireballs();
	test_boss_view_precedes_entities_and_draws();
	test_boss_lifecycle_order_and_child_failure();
	test_late_bomb_phase_and_player_order();
	test_background_is_synchronous_before_late_bombs();
	test_background_failure_is_sticky();
	test_warning_lifecycle_precedes_background();
	test_background_precedes_entity_phase();
	test_background_reads_complete_before_entity_phase();
	test_type_b_background_read_count_and_overread();
	test_terminal_no_footer_does_not_publish();
	test_late_gauge_portrait_and_footer_order();
	test_portrait_expectation_is_strict();
	test_gauge_completion_prefix_order();
	test_warning_gauge_child_failure_is_sticky();
	test_warning_interstitial_tones_without_footer();
	test_warning_interstitial_order_is_strict();
	test_malformed_late_bullet_is_whole_phase_atomic();
	test_players_common_bullets_insertion_order();
	test_defeat_palette_is_immediate();
	test_repeat_round_startup_preserves_process_children();
	test_repeat_round_startup_requires_result_banner();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return EXIT_FAILURE;
	}
	std::printf("%zu checks passed\n", checks);
	return EXIT_SUCCESS;
}
