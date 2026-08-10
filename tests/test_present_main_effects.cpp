#include "th03/present/main_effects.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace {

using th03::BombCommonEffectKind;
using th03::BombCommonEffectStep;
using th03::CommonEffectKind;
using th03::CommonEffectSpawned;
using th03::HitCircleKind;
using th03::HitCircleSpawned;
using th03::MainPhaseContext;
using th03::PlayerId;
using th03::present::Index4;
using th03::present::MainEffectsAtlasLayout;
using th03::present::MainEffectsPresenter;
using th03::present::MainEffectsStatus;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::RasterMode;
using th03::present::Sprite16Staging;
using th03::present::StaffFlakeTrigTables;
using th03::present::authentic_main_effects_atlas_layout;
using th03::present::graphics_width;
using th03::present::main_common_effect_count;
using th03::present::main_enemy_hit_circle_count;
using th03::present::main_hit_circle_cel_count;
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
	Index4 expected,
	std::string_view message
)
{
	expect(raster.pixel(Page::zero, point) == expected, message);
}

[[nodiscard]] constexpr std::int16_t raw_q12_4(
	std::int32_t pixels
) noexcept
{
	return static_cast<std::int16_t>(pixels * 16);
}

struct TestTrig {
	std::array<std::int16_t, 256U> sine{};
	std::array<std::int16_t, 256U> cosine{};

	[[nodiscard]] StaffFlakeTrigTables view() const noexcept
	{
		return {sine, cosine};
	}
};

MainEffectsAtlasLayout stage_hit_circle_cels(
	Raster &raster,
	Sprite16Staging &staging
)
{
	auto layout = authentic_main_effects_atlas_layout();
	for(std::size_t cel = 0U; cel < layout.hit_circle_cels.size(); ++cel) {
		const auto rect = layout.hit_circle_cels[cel];
		expect(
			raster.plot(
				{
					static_cast<std::int32_t>(rect.left + cel),
					static_cast<std::int32_t>(
						sprite16_source_top + rect.raw_top
					),
				},
				static_cast<Index4>(cel + 1U)
			),
			"seed a hit-circle atlas cel"
		);
	}
	expect(staging.commit(raster), "commit the hit-circle atlas");
	expect(
		raster.set_raster_mode(RasterMode::zoomed_200_upper),
		"select MAIN's zoomed graphics mode"
	);
	return layout;
}

void clear_visible_page(Raster &raster)
{
	expect(
		raster.clear({0, 0, graphics_width, 200U}, 0U),
		"clear the visible raw MAIN rows"
	);
}

CommonEffectSpawned common_at_screen_x(
	CommonEffectKind kind,
	std::int32_t screen_x,
	PlayerId player = PlayerId::p1,
	std::int16_t shift = 0,
	std::uint8_t initial_angle = 0U
)
{
	const auto player_offset = (player == PlayerId::p1) ? 0 : 320;
	return {
		.kind = kind,
		.player = player,
		.center_x = raw_q12_4(screen_x - 16 - player_offset - shift),
		.center_y = raw_q12_4(184),
		.fg_shift_x_at_spawn = shift,
		.initial_angle = initial_angle,
	};
}

void test_authentic_layout()
{
	const auto layout = authentic_main_effects_atlas_layout();
	for(std::size_t cel = 0U; cel < main_hit_circle_cel_count; ++cel) {
		const auto rect = layout.hit_circle_cels[cel];
		expect(
			rect.left == 128U + (cel * 48U) && rect.raw_top == 80U &&
			rect.width == 48U && rect.raw_height == 24U,
			"hit-circle layout retains the four source cels"
		);
	}
}

void test_hit_circle_age_color_shift_and_fixed_player_slot()
{
	Raster raster;
	Sprite16Staging staging;
	const auto layout = stage_hit_circle_cels(raster, staging);
	TestTrig trig;
	MainEffectsPresenter presenter(raster, staging, layout, trig.view());
	MainPhaseContext context{};
	context.playfield_fg_shift_x = {-30, 30};
	presenter.hit_circle_spawned({
		.kind = HitCircleKind::enemy,
		.player = PlayerId::p1,
		.center_x = raw_q12_4(100),
		.center_y = raw_q12_4(40),
		.fg_shift_x_at_spawn = 10,
	});
	expect(
		presenter.present_hit_circles(context),
		"draw an age-1 enemy hit circle"
	);
	expect_pixel(
		raster, {102, 16}, 11U,
		"hit circle captures spawn shift and starts on cel zero"
	);
	context.round_or_result_frame = 1U;
	expect(
		presenter.present_hit_circles(context),
		"redraw with odd round-frame parity"
	);
	expect_pixel(raster, {102, 16}, 12U, "enemy color alternates 11/12");

	for(std::size_t frame = 0U; frame < 4U; ++frame) {
		presenter.hit_circles_advanced();
	}
	clear_visible_page(raster);
	context.round_or_result_frame = 2U;
	expect(
		presenter.present_hit_circles(context),
		"draw the age-5 hit circle"
	);
	expect_pixel(raster, {103, 16}, 11U, "age 5 selects source cel one");
	for(std::size_t frame = 0U; frame < 12U; ++frame) {
		presenter.hit_circles_advanced();
	}
	clear_visible_page(raster);
	expect(
		presenter.present_hit_circles(context),
		"render after the source expiration boundary"
	);
	expect_pixel(raster, {102, 16}, 0U, "age 17 clears the hit-circle slot");

	MainEffectsPresenter overlap(raster, staging, layout, trig.view());
	const HitCircleSpawned same_place{
		.kind = HitCircleKind::enemy,
		.player = PlayerId::p1,
		.center_x = raw_q12_4(160),
		.center_y = raw_q12_4(80),
		.fg_shift_x_at_spawn = 0,
	};
	overlap.hit_circle_spawned(same_place);
	auto player_hit = same_place;
	player_hit.kind = HitCircleKind::player;
	overlap.hit_circle_spawned(player_hit);
	clear_visible_page(raster);
	expect(
		overlap.present_hit_circles(context),
		"draw overlapping enemy and player circles"
	);
	expect_pixel(
		raster, {152, 36}, 15U,
		"fixed player slot 12 renders after all enemy slots in white"
	);

	MainEffectsPresenter shared_player_slot(
		raster, staging, layout, trig.view()
	);
	shared_player_slot.hit_circle_spawned({
		.kind = HitCircleKind::player,
		.player = PlayerId::p1,
		.center_x = raw_q12_4(40),
		.center_y = raw_q12_4(80),
		.fg_shift_x_at_spawn = 0,
	});
	shared_player_slot.hit_circle_spawned({
		.kind = HitCircleKind::player,
		.player = PlayerId::p2,
		.center_x = raw_q12_4(40),
		.center_y = raw_q12_4(80),
		.fg_shift_x_at_spawn = 0,
	});
	clear_visible_page(raster);
	expect(
		shared_player_slot.present_hit_circles(context),
		"draw the shared player-hit slot after a cross-player overwrite"
	);
	expect_pixel(raster, {32, 36}, 0U, "later P2 hit replaces the P1 player circle");
	expect_pixel(raster, {352, 36}, 15U, "shared slot retains the later P2 circle");
	expect(overlap.ready(), "valid hit-circle observations stay ready");
}

void test_hit_circle_increment_before_cursor_and_round_persistence()
{
	Raster raster;
	Sprite16Staging staging;
	const auto layout = stage_hit_circle_cels(raster, staging);
	TestTrig trig;
	MainEffectsPresenter presenter(raster, staging, layout, trig.view());
	for(std::size_t spawn = 0U;
		spawn < (main_enemy_hit_circle_count + 1U);
		++spawn) {
		const auto left = static_cast<std::int32_t>(20U + (spawn * 10U));
		presenter.hit_circle_spawned({
			.kind = HitCircleKind::enemy,
			.player = PlayerId::p1,
			.center_x = raw_q12_4(left + 8),
			.center_y = raw_q12_4(120),
			.fg_shift_x_at_spawn = 0,
		});
	}
	MainPhaseContext context{};
	expect(
		presenter.present_hit_circles(context),
		"draw the wrapped enemy hit-circle pool"
	);
	expect_pixel(raster, {20, 56}, 0U, "thirteenth spawn overwrites first slot 1");
	expect_pixel(raster, {140, 56}, 11U, "replacement remains visible in slot 1");

	clear_visible_page(raster);
	context.round_id = 8U;
	expect(
		presenter.present_hit_circles(context),
		"draw the same pool after a round-id change"
	);
	expect_pixel(
		raster, {140, 56}, 11U,
		"repeat-round context does not reset process-scoped effect BSS"
	);
}

void test_common_circle_lifecycle_cursor_shift_and_clip()
{
	Raster raster;
	Sprite16Staging staging;
	const auto layout = stage_hit_circle_cels(raster, staging);
	TestTrig trig;
	MainEffectsPresenter presenter(raster, staging, layout, trig.view());
	MainPhaseContext context{};
	presenter.common_effect_spawned(common_at_screen_x(
		CommonEffectKind::expanding_circle, 80
	));
	expect(
		presenter.present_common_effects(context),
		"draw an unadvanced expanding circle"
	);
	expect_pixel(raster, {88, 100}, 13U, "late spawn renders at radius 8");
	presenter.common_effects_advanced();
	clear_visible_page(raster);
	expect(
		presenter.present_common_effects(context),
		"draw an expanding circle after one prefix advance"
	);
	expect_pixel(raster, {96, 100}, 13U, "prefix advance grows radius to 16");
	for(std::size_t frame = 0U; frame < 15U; ++frame) {
		presenter.common_effects_advanced();
	}
	clear_visible_page(raster);
	expect(
		presenter.present_common_effects(context),
		"draw a common circle at age 16"
	);
	expect_pixel(raster, {216, 100}, 13U, "age 16 remains visible at radius 136");
	presenter.common_effects_advanced();
	clear_visible_page(raster);
	expect(
		presenter.present_common_effects(context),
		"render after common-effect expiration"
	);
	expect_pixel(raster, {224, 100}, 0U, "age 17 clears before radius 144 renders");

	MainEffectsPresenter captured(raster, staging, layout, trig.view());
	captured.common_effect_spawned({
		.kind = CommonEffectKind::expanding_circle,
		.player = PlayerId::p2,
		.center_x = raw_q12_4(-320),
		.center_y = raw_q12_4(184),
		.fg_shift_x_at_spawn = 0,
		.initial_angle = 0U,
	});
	clear_visible_page(raster);
	context.playfield_fg_shift_x = {50, 50};
	expect(
		captured.present_common_effects(context),
		"draw a P2 allocation whose stored X lies in P1"
	);
	expect_pixel(
		raster, {24, 100}, 13U,
		"common effect captures X and selects clip from stored screen X"
	);
	expect_pixel(raster, {8, 100}, 0U, "stored-X P1 clip removes the left edge");

	MainEffectsPresenter negative_fractional(
		raster, staging, layout, trig.view()
	);
	negative_fractional.common_effect_spawned({
		.kind = CommonEffectKind::expanding_circle,
		.player = PlayerId::p2,
		.center_x = -1,
		.center_y = raw_q12_4(184),
		.fg_shift_x_at_spawn = 0,
		.initial_angle = 0U,
	});
	clear_visible_page(raster);
	expect(
		negative_fractional.present_common_effects(context),
		"draw a negative fractional Q12.4 common-effect center"
	);
	expect_pixel(
		raster, {343, 100}, 13U,
		"negative fractional Q12.4 uses arithmetic shift before screen placement"
	);
	expect_pixel(
		raster, {344, 100}, 0U,
		"negative fractional Q12.4 does not truncate toward zero"
	);

	MainEffectsPresenter wrapped(raster, staging, layout, trig.view());
	for(std::size_t spawn = 0U;
		spawn < (main_common_effect_count + 1U);
		++spawn) {
		wrapped.common_effect_spawned(common_at_screen_x(
			CommonEffectKind::expanding_circle,
			static_cast<std::int32_t>(30U + (spawn * 20U))
		));
	}
	clear_visible_page(raster);
	expect(
		wrapped.present_common_effects(context),
		"draw the wrapped common-effect pool"
	);
	expect_pixel(raster, {38, 100}, 0U, "thirteenth common spawn overwrites slot 1");
	expect_pixel(raster, {278, 100}, 13U, "replacement common circle remains visible");
	clear_visible_page(raster);
	context.round_id = 9U;
	expect(
		wrapped.present_common_effects(context),
		"draw common effects after a round-id change"
	);
	expect_pixel(
		raster, {278, 100}, 13U,
		"common pool and cursor remain process-scoped across repeat rounds"
	);
}

void test_common_square_slot_parity_and_render_mutation()
{
	Raster raster;
	Sprite16Staging staging;
	const auto layout = stage_hit_circle_cels(raster, staging);
	TestTrig trig;
	trig.cosine[0U] = 16;
	trig.cosine[8U] = 32;
	MainEffectsPresenter presenter(raster, staging, layout, trig.view());
	presenter.common_effect_spawned(common_at_screen_x(
		CommonEffectKind::contracting_square, 100, PlayerId::p1, 0, 8U
	));
	presenter.common_effect_spawned(common_at_screen_x(
		CommonEffectKind::contracting_square, 200
	));
	MainPhaseContext context{};
	expect(
		presenter.present_common_effects(context),
		"draw two contracting squares"
	);
	expect_pixel(
		raster, {110, 100}, 13U,
		"odd slot subtracts 8 from the captured initial angle"
	);
	expect_pixel(
		raster, {220, 100}, 13U,
		"second allocation occupies even slot 2 and rotates by plus 8"
	);

	clear_visible_page(raster);
	expect(
		presenter.present_common_effects(context),
		"draw the same squares a second time"
	);
	expect_pixel(raster, {110, 100}, 0U, "square angle mutates only on render");
	expect_pixel(raster, {220, 100}, 0U, "even-slot angle also advances per draw");
	expect_pixel(raster, {100, 100}, 13U, "second odd-slot polygon collapses at angle 240");
	expect_pixel(raster, {200, 100}, 13U, "second even-slot polygon collapses at angle 16");
}

void seed_kana_trig(TestTrig &trig)
{
	trig.cosine[0U] = 256;
	trig.cosine[128U] = -256;
	trig.sine[16U] = 256;
	trig.sine[112U] = -256;
}

void test_kana_shared_bomb_state_and_every_active_callback()
{
	Raster raster;
	Sprite16Staging staging;
	const auto layout = stage_hit_circle_cels(raster, staging);
	TestTrig trig;
	seed_kana_trig(trig);
	MainEffectsPresenter simultaneous(raster, staging, layout, trig.view());
	simultaneous.bomb_common_effect_step({
		BombCommonEffectKind::kana, PlayerId::p1, 64U, 0,
	});
	simultaneous.bomb_common_effect_step({
		BombCommonEffectKind::kana, PlayerId::p2, 64U, 0,
	});
	MainPhaseContext context{};
	expect(
		simultaneous.present_common_effects(context),
		"draw simultaneous Kana Bomb callbacks in P1-to-P2 order"
	);
	expect_pixel(raster, {296, 100}, 13U, "P1 Kana starts at shared angle zero");
	expect_pixel(raster, {24, 100}, 13U, "P1 Kana draws the reflected angle");
	expect_pixel(raster, {488, 172}, 13U, "P2 Kana observes P1's angle increment");
	expect_pixel(raster, {488, 28}, 13U, "P2 Kana uses the reflected shared angle");

	clear_visible_page(raster);
	MainEffectsPresenter reset_regression(raster, staging, layout, trig.view());
	reset_regression.bomb_common_effect_step({
		BombCommonEffectKind::kana, PlayerId::p1, 64U, 0,
	});
	// No circle is spawned on frame 63, but the source still writes the shared
	// angle to zero before the following P2 callback.
	reset_regression.bomb_common_effect_step({
		BombCommonEffectKind::kana, PlayerId::p1, 63U, 0,
	});
	reset_regression.bomb_common_effect_step({
		BombCommonEffectKind::kana, PlayerId::p2, 64U, 0,
	});
	expect(
		reset_regression.present_common_effects(context),
		"draw the P1 frame63 to P2 frame64 Kana regression"
	);
	expect_pixel(raster, {616, 100}, 13U, "frame63 reset restores angle zero");
	expect_pixel(raster, {344, 100}, 13U, "reflected reset angle reaches P2 left edge");
	expect_pixel(raster, {488, 172}, 0U, "P2 does not retain the stale angle 16");

	clear_visible_page(raster);
	MainEffectsPresenter early(raster, staging, layout, trig.view());
	early.bomb_common_effect_step({
		BombCommonEffectKind::kana, PlayerId::p1, 56U, 0,
	});
	expect(
		early.present_common_effects(context),
		"draw Kana's early contracting circle"
	);
	expect_pixel(raster, {24, 100}, 13U, "Kana frame56 starts at radius 136");
}

void test_kotohime_shared_bomb_state_and_raw_coordinates()
{
	Raster raster;
	Sprite16Staging staging;
	const auto layout = stage_hit_circle_cels(raster, staging);
	TestTrig trig;
	MainEffectsPresenter presenter(raster, staging, layout, trig.view());
	presenter.bomb_common_effect_step({
		BombCommonEffectKind::kotohime, PlayerId::p1, 0U, 0,
	});
	presenter.bomb_common_effect_step({
		BombCommonEffectKind::kotohime, PlayerId::p1, 56U, 0,
	});
	presenter.bomb_common_effect_step({
		BombCommonEffectKind::kotohime, PlayerId::p2, 64U, 0,
	});
	presenter.bomb_common_effect_step({
		BombCommonEffectKind::kotohime, PlayerId::p1, 72U, 0,
	});
	MainPhaseContext context{};
	expect(
		presenter.present_common_effects(context),
		"draw cross-player Kotohime Bomb callbacks"
	);
	expect_pixel(
		raster, {291, 10}, 13U,
		"Kotohime frame0 subtracts raw 72 rather than 72 pixels"
	);
	expect_pixel(
		raster, {344, 171}, 13U,
		"P2 frame64 reads the y assigned by P1 frame56"
	);
	expect_pixel(
		raster, {24, 148}, 13U,
		"later callback observes the shared raw-y decrement"
	);
}

void test_invalid_inputs_and_staging_contract()
{
	Raster raster;
	Sprite16Staging staging;
	TestTrig trig;
	auto layout = authentic_main_effects_atlas_layout();
	auto invalid_layout = layout;
	invalid_layout.hit_circle_cels[0U].width = 32U;
	MainEffectsPresenter bad_layout(
		raster, staging, invalid_layout, trig.view()
	);
	expect(
		bad_layout.status() == MainEffectsStatus::invalid_layout,
		"invalid hit-circle atlas layout is rejected"
	);

	const std::array<std::int16_t, 1U> short_table{{0}};
	MainEffectsPresenter bad_assets(
		raster,
		staging,
		layout,
		StaffFlakeTrigTables{short_table, short_table}
	);
	expect(
		bad_assets.status() == MainEffectsStatus::invalid_assets,
		"short trigonometry tables are rejected"
	);

	MainEffectsPresenter bad_observation(raster, staging, layout, trig.view());
	bad_observation.hit_circle_spawned({
		.kind = HitCircleKind::enemy,
		.player = static_cast<PlayerId>(2U),
	});
	expect(
		bad_observation.status() == MainEffectsStatus::invalid_observation,
		"invalid observed player is rejected before allocation"
	);
	MainEffectsPresenter bad_common(raster, staging, layout, trig.view());
	bad_common.common_effect_spawned({
		.kind = static_cast<CommonEffectKind>(3U),
		.player = PlayerId::p1,
	});
	expect(
		bad_common.status() == MainEffectsStatus::invalid_observation,
		"invalid concrete common-effect kind is rejected"
	);
	MainEffectsPresenter bad_bomb(raster, staging, layout, trig.view());
	bad_bomb.bomb_common_effect_step({
		.kind = static_cast<BombCommonEffectKind>(2U),
		.player = PlayerId::p1,
	});
	expect(
		bad_bomb.status() == MainEffectsStatus::invalid_observation,
		"invalid exceptional Bomb kind is rejected"
	);

	MainEffectsPresenter uncommitted(raster, staging, layout, trig.view());
	MainPhaseContext context{};
	expect(
		!uncommitted.present_hit_circles(context) &&
		uncommitted.status() == MainEffectsStatus::staging_not_committed,
		"hit-circle pass requires a committed Sprite16 staging area"
	);
}

} // namespace

int main()
{
	test_authentic_layout();
	test_hit_circle_age_color_shift_and_fixed_player_slot();
	test_hit_circle_increment_before_cursor_and_round_persistence();
	test_common_circle_lifecycle_cursor_shift_and_clip();
	test_common_square_slot_parity_and_render_mutation();
	test_kana_shared_bomb_state_and_every_active_callback();
	test_kotohime_shared_bomb_state_and_raw_coordinates();
	test_invalid_inputs_and_staging_contract();

	if(failures != 0U) {
		std::fprintf(
			stderr,
			"%zu of %zu main-effect checks failed\n",
			failures,
			checks
		);
		return 1;
	}
	std::printf("all %zu main-effect checks passed\n", checks);
	return 0;
}
