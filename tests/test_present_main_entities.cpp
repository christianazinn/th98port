#include "th03/present/main_entities.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

using th03::EfeFlag;
using th03::EnemyFact;
using th03::EnemySlot;
using th03::FireballFact;
using th03::FireballSlot;
using th03::FireballVariant;
using th03::MainPhaseContext;
using th03::PlayerId;
using th03::ShotPairFact;
using th03::ShotPairSlot;
using th03::ShotPairSpawned;
using th03::present::Index4;
using th03::present::MainEnemyFacts;
using th03::present::MainEntitiesPresenter;
using th03::present::MainEntitiesStatus;
using th03::present::MainEntityAtlasLayout;
using th03::present::MainFireballFacts;
using th03::present::MainShotPairFacts;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::Sprite16AtlasRect;
using th03::present::Sprite16Staging;
using th03::present::authentic_main_entity_atlas_layout;
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
	Point point,
	Index4 expected,
	std::string_view message
)
{
	const auto actual = raster.pixel(Page::zero, point);
	expect(actual.has_value() && *actual == expected, message);
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
		"seed a synthetic semantic atlas slot"
	);
}

constexpr bool rect_equal(
	Sprite16AtlasRect left,
	Sprite16AtlasRect right
) noexcept
{
	return left.left == right.left && left.raw_top == right.raw_top &&
		left.width == right.width && left.raw_height == right.raw_height;
}

MainEntityAtlasLayout seed_authentic_layout(Raster &raster)
{
	auto layout = authentic_main_entity_atlas_layout();
	for(std::size_t cel = 0U; cel < layout.shot_cels[0U].size(); ++cel) {
		put_source_marker(
			raster,
			layout.shot_cels[0U][cel],
			static_cast<Index4>(cel + 1U)
		);
		put_source_marker(
			raster,
			layout.shot_cels[1U][cel],
			static_cast<Index4>(cel + 5U)
		);
	}
	for(std::size_t family = 0U;
		family < layout.living_enemy_undamaged.size();
		++family) {
		put_source_marker(
			raster,
			layout.living_enemy_undamaged[family],
			static_cast<Index4>(family + 9U)
		);
		for(std::size_t cel = 0U;
			cel < layout.explosion_cels[family].size();
			++cel) {
			put_source_marker(
				raster,
				layout.explosion_cels[family][cel],
				static_cast<Index4>(cel + 1U)
			);
		}
	}
	// The 16-pixel family's first damaged cel immediately precedes its anchor.
	put_source_marker(raster, {32U, 104U, 16U, 8U}, 13U);
	// Four damage strides before the 32-pixel family anchor cross from raw row
	// 112 to the final 32 pixels of raw row 111.
	put_source_marker(raster, {608U, 111U, 32U, 16U}, 14U);

	for(std::size_t variant = 0U;
		variant < layout.falling_fireball_cels.size();
		++variant) {
		for(std::size_t cel = 0U;
			cel < layout.falling_fireball_cels[variant].size();
			++cel) {
			put_source_marker(
				raster,
				layout.falling_fireball_cels[variant][cel],
				static_cast<Index4>(5U + (variant * 2U) + cel)
			);
			put_source_marker(
				raster,
				layout.transferring_fireball_cels[variant][cel],
				static_cast<Index4>(9U + (variant * 2U) + cel)
			);
		}
	}
	return layout;
}

void commit(Sprite16Staging &staging, Raster &raster)
{
	expect(staging.commit(raster), "commit the synthetic entity atlas");
}

MainShotPairFacts empty_shots()
{
	MainShotPairFacts shots{};
	for(std::size_t slot = 0U; slot < shots.size(); ++slot) {
		shots[slot].slot = static_cast<ShotPairSlot>(slot);
	}
	return shots;
}

MainEnemyFacts empty_enemies()
{
	MainEnemyFacts enemies{};
	for(std::size_t slot = 0U; slot < enemies.size(); ++slot) {
		enemies[slot].slot = static_cast<EnemySlot>(slot);
	}
	return enemies;
}

MainFireballFacts empty_fireballs()
{
	MainFireballFacts fireballs{};
	for(std::size_t slot = 0U; slot < fireballs.size(); ++slot) {
		fireballs[slot].slot = static_cast<FireballSlot>(slot);
	}
	return fireballs;
}

constexpr std::size_t player_index(PlayerId player) noexcept
{
	return (player == PlayerId::p1) ? 0U : 1U;
}

std::int16_t raw_q12_4(std::int32_t pixels)
{
	return static_cast<std::int16_t>(pixels * 16);
}

ShotPairFact shot_at(
	std::size_t slot,
	PlayerId player,
	std::int32_t left,
	std::int32_t logical_top,
	const MainPhaseContext &context
)
{
	const auto player_id = player_index(player);
	return {
		.slot = static_cast<ShotPairSlot>(slot),
		.alive = true,
		.topleft_x = raw_q12_4(
			left - 16 - static_cast<std::int32_t>(player_id * 320U) -
			context.playfield_fg_shift_x[player_id]
		),
		.topleft_y = raw_q12_4(logical_top - 16),
		.player_id = player,
	};
}

EnemyFact enemy_at(
	std::size_t slot,
	std::uint8_t flag,
	std::uint8_t frame,
	PlayerId player,
	std::uint8_t size_pixels,
	std::uint8_t hp,
	std::uint8_t size_words,
	std::int32_t left,
	std::int32_t logical_top,
	const MainPhaseContext &context
)
{
	const auto player_id = player_index(player);
	const auto logical_size = static_cast<std::int32_t>(
		size_pixels + ((flag == 1U) ? 0U : 16U)
	);
	return {
		.slot = static_cast<EnemySlot>(slot),
		.flag = EfeFlag{flag},
		.frame = frame,
		.center_x = raw_q12_4(
			left + (logical_size / 2) - 16 -
			static_cast<std::int32_t>(player_id * 320U) -
			context.playfield_fg_shift_x[player_id]
		),
		.center_y = raw_q12_4(logical_top + (logical_size / 2) - 16),
		.player_id = player,
		.size_pixels = size_pixels,
		.hp = hp,
		.explosion_max_enemy_hits_half = size_words,
	};
}

FireballFact fireball_at(
	std::size_t slot,
	std::uint8_t flag,
	std::uint8_t frame,
	PlayerId player,
	FireballVariant variant,
	std::uint16_t logical_size,
	std::int32_t left,
	std::int32_t logical_top,
	const MainPhaseContext &context
)
{
	const auto player_id = player_index(player);
	return {
		.slot = static_cast<FireballSlot>(slot),
		.flag = EfeFlag{flag},
		.frame = frame,
		.center_x = raw_q12_4(
			left + static_cast<std::int32_t>(logical_size / 2U) - 16 -
			static_cast<std::int32_t>(player_id * 320U) -
			context.playfield_fg_shift_x[player_id]
		),
		.center_y = raw_q12_4(
			logical_top + static_cast<std::int32_t>(logical_size / 2U) - 16
		),
		.player_id = player,
		.variant = variant,
	};
}

void test_authentic_layout_geometry()
{
	const auto layout = authentic_main_entity_atlas_layout();
	expect(
		layout.shot_cels[0U][0U].left == 0U &&
		layout.shot_cels[1U][3U].left == 368U,
		"shot cels retain the two source attack-sheet regions"
	);
	expect(
		rect_equal(
			layout.living_enemy_undamaged[0U],
			{48U, 104U, 16U, 8U}
		) && rect_equal(
			layout.living_enemy_undamaged[3U],
			{192U, 128U, 64U, 32U}
		),
		"living enemy family anchors retain SO_ENEMIES geometry"
	);
	expect(
		rect_equal(
			layout.explosion_cels[2U][3U],
			{432U, 136U, 64U, 32U}
		),
		"fireball explosion family retains SO_EXPLOSIONS_64X64 geometry"
	);
	expect(
		layout.falling_fireball_cels[1U][1U].left == 96U &&
		layout.transferring_fireball_cels[1U][1U].left == 48U,
		"red fireball variants retain source-relative cel order"
	);
}

void test_shot_animation_reset_coordinates_and_clip()
{
	Raster raster;
	const auto layout = seed_authentic_layout(raster);
	Sprite16Staging staging;
	commit(staging, raster);
	MainEntitiesPresenter presenter(raster, staging, layout);
	MainPhaseContext context{};
	context.playfield_fg_shift_x = {3, -6};
	auto shots = empty_shots();
	const auto enemies = empty_enemies();
	shots[0U] = shot_at(0U, PlayerId::p1, 100, 40, context);
	presenter.shot_pair_spawned({
		static_cast<ShotPairSlot>(0U), PlayerId::p1,
	});
	expect(
		presenter.present_shots_enemies(context, shots, enemies),
		"draw a newly spawned shot pair"
	);
	expect_pixel(raster, {100, 20}, 1U, "new shot starts at cel zero");
	expect_pixel(raster, {116, 20}, 1U, "shot pair draws the second sprite at +16");

	expect(raster.clear({100, 20, 32U, 1U}, 0U), "clear shot destination");
	expect(
		presenter.present_shots_enemies(context, shots, enemies),
		"draw the next shot animation cel"
	);
	expect_pixel(raster, {100, 20}, 2U, "one live phase advances one shot cel");

	presenter.shot_pair_spawned({
		static_cast<ShotPairSlot>(0U), PlayerId::p1,
	});
	expect(raster.clear({100, 20, 32U, 1U}, 0U), "clear reset destination");
	expect(
		presenter.present_shots_enemies(context, shots, enemies),
		"draw a reused shot slot"
	);
	expect_pixel(raster, {100, 20}, 1U, "spawn observation resets reused slot to cel zero");

	presenter.shot_pair_spawned({
		static_cast<ShotPairSlot>(0U), PlayerId::p1,
	});
	shots[0U] = shot_at(0U, PlayerId::p1, -16, 42, context);
	expect(
		presenter.present_shots_enemies(context, shots, enemies),
		"draw a pair whose first sprite clips completely"
	);
	expect_pixel(
		raster,
		{0, 21},
		1U,
		"full MAIN clip removes the first sprite and retains the second"
	);

	shots = empty_shots();
	shots[1U] = shot_at(1U, PlayerId::p2, 350, 44, context);
	presenter.shot_pair_spawned({
		static_cast<ShotPairSlot>(1U), PlayerId::p2,
	});
	expect(
		presenter.present_shots_enemies(context, shots, enemies),
		"draw a P2 shot through the foreground transform"
	);
	expect_pixel(raster, {350, 22}, 5U, "P2 sheet and foreground shift are applied");

	// Move a separate cursor away from zero, reset it on reuse, then omit the
	// killed shot from the next phase. Its first later live view must still use
	// cel 0 rather than advancing while dead.
	shots = empty_shots();
	shots[2U] = shot_at(2U, PlayerId::p1, 240, 46, context);
	presenter.shot_pair_spawned({
		static_cast<ShotPairSlot>(2U), PlayerId::p1,
	});
	expect(
		presenter.present_shots_enemies(context, shots, enemies) &&
		presenter.present_shots_enemies(context, shots, enemies),
		"advance a shot cursor before slot reuse"
	);
	presenter.shot_pair_spawned({
		static_cast<ShotPairSlot>(2U), PlayerId::p1,
	});
	shots[2U].alive = false;
	expect(
		presenter.present_shots_enemies(context, shots, enemies),
		"observe a reused shot killed before its phase view"
	);
	expect(raster.clear({240, 23, 32U, 1U}, 0U), "clear killed-shot destination");
	shots[2U].alive = true;
	expect(
		presenter.present_shots_enemies(context, shots, enemies),
		"draw the reset slot after a dead phase"
	);
	expect_pixel(
		raster,
		{240, 23},
		1U,
		"spawn-reset shot killed before the view does not advance"
	);
	expect(presenter.ready(), "valid shot phases keep presenter ready");
}

void test_shots_then_enemies_and_enemy_cels()
{
	Raster raster;
	const auto layout = seed_authentic_layout(raster);
	Sprite16Staging staging;
	commit(staging, raster);
	MainEntitiesPresenter presenter(raster, staging, layout);
	MainPhaseContext context{};
	auto shots = empty_shots();
	auto enemies = empty_enemies();
	shots[0U] = shot_at(0U, PlayerId::p2, 100, 40, context);
	shots[1U] = shot_at(1U, PlayerId::p1, 100, 40, context);
	expect(
		presenter.present_shots_enemies(context, shots, enemies),
		"draw ascending overlapping shot slots"
	);
	expect_pixel(raster, {100, 20}, 1U, "higher shot slot draws later");

	enemies[0U] = enemy_at(
		0U, 1U, 0U, PlayerId::p1, 16U, 0U, 1U, 100, 40, context
	);
	enemies[1U] = enemy_at(
		1U, 1U, 0U, PlayerId::p1, 16U, 1U, 1U, 100, 40, context
	);
	expect(
		presenter.present_shots_enemies(context, shots, enemies),
		"draw enemy slots after shot slots"
	);
	expect_pixel(
		raster,
		{100, 20},
		13U,
		"damaged higher enemy slot is the final overlapping draw"
	);

	auto row_crossing_damage = empty_enemies();
	row_crossing_damage[0U] = enemy_at(
		0U, 1U, 0U, PlayerId::p1, 32U, 4U, 2U, 200, 40, context
	);
	expect(
		presenter.present_shots_enemies(
			context,
			empty_shots(),
			row_crossing_damage
		),
		"draw a damaged-enemy stride crossing an atlas row"
	);
	expect_pixel(
		raster,
		{200, 20},
		14U,
		"enemy HP stride preserves the row-crossing source address"
	);

	auto explosions = empty_enemies();
	for(std::size_t slot = 0U; slot < 4U; ++slot) {
		constexpr std::uint8_t frames[4U]{9U, 10U, 12U, 32U};
		explosions[slot] = enemy_at(
			slot,
			9U,
			frames[slot],
			PlayerId::p1,
			16U,
			0U,
			1U,
			static_cast<std::int32_t>(20U + (slot * 60U)),
			80,
			context
		);
	}
	expect(
		presenter.present_shots_enemies(context, empty_shots(), explosions),
		"draw all enemy explosion cel boundaries"
	);
	for(std::size_t slot = 0U; slot < 4U; ++slot) {
		expect_pixel(
			raster,
			{static_cast<std::int32_t>(20U + (slot * 60U)), 40},
			static_cast<Index4>(slot + 1U),
			"enemy explosion selects the exact source cel"
		);
	}

	auto clipped = empty_enemies();
	clipped[0U] = enemy_at(
		0U, 1U, 0U, PlayerId::p1, 16U, 0U, 1U, 320, 100, context
	);
	expect(
		presenter.present_shots_enemies(context, empty_shots(), clipped),
		"draw an enemy beyond its own playfield clip"
	);
	expect_pixel(raster, {320, 50}, 0U, "P1 enemy does not cross into P2 clip");
}

void test_fireball_variants_keyframes_and_order()
{
	Raster raster;
	const auto layout = seed_authentic_layout(raster);
	Sprite16Staging staging;
	commit(staging, raster);
	MainEntitiesPresenter presenter(raster, staging, layout);
	MainPhaseContext context{};
	auto fireballs = empty_fireballs();
	fireballs[0U] = fireball_at(
		0U, 1U, 0U, PlayerId::p1, FireballVariant::blue, 32U, 0, 40, context
	);
	fireballs[1U] = fireball_at(
		1U, 1U, 4U, PlayerId::p1, FireballVariant::red, 32U, 80, 40, context
	);
	fireballs[2U] = fireball_at(
		2U, 2U, 3U, PlayerId::p1, FireballVariant::red, 16U, 160, 40, context
	);
	fireballs[3U] = fireball_at(
		3U, 2U, 7U, PlayerId::p1, FireballVariant::blue, 16U, 220, 40, context
	);
	constexpr std::uint8_t explosion_frames[4U]{11U, 12U, 16U, 32U};
	for(std::size_t index = 0U; index < 4U; ++index) {
		fireballs[4U + index] = fireball_at(
			4U + index,
			9U,
			explosion_frames[index],
			PlayerId::p1,
			FireballVariant::blue,
			64U,
			static_cast<std::int32_t>(280U + (index * 80U)),
			40,
			context
		);
	}
	expect(
		presenter.present_fireballs(context, fireballs),
		"draw living and exploding fireballs"
	);
	expect_pixel(raster, {0, 20}, 6U, "fall frame 0 selects the second blue cel");
	expect_pixel(raster, {80, 20}, 7U, "fall frame 4 selects the first red cel");
	expect_pixel(raster, {160, 20}, 12U, "transfer frame 3 selects second red cel");
	expect_pixel(raster, {220, 20}, 9U, "transfer frame 7 selects first blue cel");
	constexpr Index4 expected_explosions[4U]{1U, 3U, 2U, 4U};
	for(std::size_t index = 0U; index < 4U; ++index) {
		expect_pixel(
			raster,
			{static_cast<std::int32_t>(280U + (index * 80U)), 20},
			expected_explosions[index],
			"fireball explosion selects its 12/32/36-keyed cel"
		);
	}

	auto overlap = empty_fireballs();
	overlap[8U] = fireball_at(
		8U, 2U, 0U, PlayerId::p1, FireballVariant::blue, 16U, 100, 120, context
	);
	overlap[9U] = fireball_at(
		9U, 2U, 0U, PlayerId::p1, FireballVariant::red, 16U, 100, 120, context
	);
	expect(
		presenter.present_fireballs(context, overlap),
		"draw overlapping fireballs in ascending slots"
	);
	expect_pixel(raster, {100, 60}, 12U, "higher fireball slot draws later");
}

void test_malformed_atomicity_and_status()
{
	Raster raster;
	auto layout = seed_authentic_layout(raster);
	Sprite16Staging staging;
	commit(staging, raster);

	auto invalid_layout = layout;
	invalid_layout.shot_cels[0U][0U].width = 32U;
	MainEntitiesPresenter bad_layout(raster, staging, invalid_layout);
	expect(
		bad_layout.status() == MainEntitiesStatus::invalid_layout,
		"constructor rejects malformed semantic layout"
	);

	MainEntitiesPresenter bad_fact(raster, staging, layout);
	MainPhaseContext context{};
	auto shots = empty_shots();
	auto enemies = empty_enemies();
	shots[0U] = shot_at(0U, PlayerId::p1, 100, 40, context);
	enemies.back() = enemy_at(
		enemies.size() - 1U,
		1U,
		0U,
		PlayerId::p1,
		32U,
		0U,
		1U,
		200,
		40,
		context
	);
	const auto before = raster.display_state_hash();
	expect(
		!bad_fact.present_shots_enemies(context, shots, enemies),
		"reject mismatched enemy size_pixels and explosion family"
	);
	expect(
		bad_fact.status() == MainEntitiesStatus::invalid_observation &&
		raster.display_state_hash() == before,
		"whole phase validates atomically before an earlier shot draw"
	);

	Raster uncommitted_raster;
	const auto uncommitted_layout = seed_authentic_layout(uncommitted_raster);
	Sprite16Staging uncommitted;
	MainEntitiesPresenter no_alpha(
		uncommitted_raster,
		uncommitted,
		uncommitted_layout
	);
	auto one_shot = empty_shots();
	one_shot[0U] = shot_at(0U, PlayerId::p1, 40, 40, context);
	const auto uncommitted_before = uncommitted_raster.display_state_hash();
	expect(
		!no_alpha.present_shots_enemies(context, one_shot, empty_enemies()),
		"reject a visible phase before Sprite16 commit"
	);
	expect(
		no_alpha.status() == MainEntitiesStatus::staging_not_committed &&
		uncommitted_raster.display_state_hash() == uncommitted_before,
		"uncommitted phase is atomic"
	);

	MainEntitiesPresenter bad_fireball(raster, staging, layout);
	auto fireballs = empty_fireballs();
	fireballs[23U] = fireball_at(
		23U,
		2U,
		0U,
		PlayerId::p1,
		static_cast<FireballVariant>(0xFFU),
		16U,
		40,
		40,
		context
	);
	const auto before_fireball = raster.display_state_hash();
	expect(
		!bad_fireball.present_fireballs(context, fireballs),
		"reject an invalid living-fireball variant"
	);
	expect(
		bad_fireball.status() == MainEntitiesStatus::invalid_observation &&
		raster.display_state_hash() == before_fireball,
		"malformed fireball phase is atomic"
	);

	MainEntitiesPresenter bad_spawn(raster, staging, layout);
	bad_spawn.shot_pair_spawned({
		static_cast<ShotPairSlot>(255U), PlayerId::p1,
	});
	expect(
		bad_spawn.status() == MainEntitiesStatus::invalid_observation,
		"out-of-range shot spawn is rejected"
	);
}

} // namespace

int main()
{
	test_authentic_layout_geometry();
	test_shot_animation_reset_coordinates_and_clip();
	test_shots_then_enemies_and_enemy_cels();
	test_fireball_variants_keyframes_and_order();
	test_malformed_atomicity_and_status();
	if(failures != 0) {
		std::fprintf(stderr, "%d MAIN entity test(s) failed\n", failures);
		return EXIT_FAILURE;
	}
	std::puts("MAIN entity presentation tests passed");
	return EXIT_SUCCESS;
}
