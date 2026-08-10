#include "th03/present/main_extra_charge.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <optional>
#include <string_view>

#include "../src/core/game_internal.hpp"
#include "th03/present/main_static.hpp"

namespace {

using th03::PlayerId;
using th03::detail::DeterministicState;
using th03::detail::ObserverAccess;
using th03::present::Index4;
using th03::present::MainExtraChargePresenter;
using th03::present::MainExtraChargeStatus;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::RasterMode;
using th03::present::Sprite16Staging;
using th03::present::StaffFlakeTrigTables;
using th03::present::sprite16_source_height;
using th03::present::sprite16_source_top;
using th03::present::sprite16_source_width;

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

struct SourceMark {
	std::uint16_t left;
	std::uint16_t raw_top;
	Index4 color;
};

struct Fixture {
	Raster raster;
	Sprite16Staging staging;
	TestTrig trig;
	std::optional<MainExtraChargePresenter> presenter;

	void prepare()
	{
		expect(
			raster.clear(
				{
					0,
					static_cast<std::int32_t>(sprite16_source_top),
					sprite16_source_width,
					sprite16_source_height,
				},
				3U
			),
			"seed complete Extra/Charge atlas"
		);
		finish_prepare();
	}

	void prepare_marked(std::initializer_list<SourceMark> marks)
	{
		expect(
			raster.clear(
				{
					0,
					static_cast<std::int32_t>(sprite16_source_top),
					sprite16_source_width,
					sprite16_source_height,
				},
				0U
			),
			"clear marked Extra/Charge atlas"
		);
		for(const auto &mark : marks) {
			expect(
				raster.plot(
					{
						static_cast<std::int32_t>(mark.left),
						static_cast<std::int32_t>(
							sprite16_source_top + mark.raw_top
						),
					},
					mark.color
				),
				"seed marked Extra/Charge atlas pixel"
			);
		}
		finish_prepare();
	}

	void finish_prepare()
	{
		expect(staging.commit(raster), "commit Extra/Charge atlas");
		expect(
			raster.set_raster_mode(RasterMode::zoomed_200_upper),
			"select MAIN raster mode"
		);
		expect(clear_output(), "clear initial output");
		presenter.emplace(raster, staging, trig.view());
		expect(presenter->ready(), "presenter starts ready");
	}

	[[nodiscard]] bool clear_output()
	{
		return raster.clear({0, 0, 640U, 200U}, 0U);
	}
};

void select(DeterministicState &state, std::uint8_t character)
{
	state.players[0U].paletted_character = paletted(character);
}

void observe_extra(Fixture &fixture, DeterministicState &state)
{
	fixture.presenter->observe(
		ObserverAccess::main_extra_attack(state, PlayerId::p1)
	);
}

void observe_charge(Fixture &fixture, DeterministicState &state)
{
	fixture.presenter->observe(
		ObserverAccess::main_charge_shot(state, PlayerId::p1)
	);
}

void reset(DeterministicState &state, std::uint8_t character)
{
	state = {};
	select(state, character);
}

void test_construction_and_atomic_rejection()
{
	Raster raster;
	Sprite16Staging staging;
	TestTrig trig;
	MainExtraChargePresenter uncommitted(raster, staging, trig.view());
	expect(
		uncommitted.status() == MainExtraChargeStatus::staging_not_committed,
		"uncommitted staging is rejected"
	);

	expect(
		raster.clear(
			{0, static_cast<std::int32_t>(sprite16_source_top), 640U, 200U},
			3U
		),
		"seed constructor atlas"
	);
	expect(staging.commit(raster), "commit constructor atlas");
	MainExtraChargePresenter bad_trig(
		raster,
		staging,
		StaffFlakeTrigTables{trig.sine, {}}
	);
	expect(
		bad_trig.status() == MainExtraChargeStatus::invalid_assets,
		"short trig table is rejected"
	);

	MainExtraChargePresenter invalid(raster, staging, trig.view());
	expect(raster.clear({0, 0, 640U, 200U}, 0U), "clear invalid-view output");
	DeterministicState state{};
	state.players[0U].paletted_character = 0;
	invalid.observe(ObserverAccess::main_extra_attack(state, PlayerId::p1));
	expect(
		invalid.status() == MainExtraChargeStatus::invalid_observation,
		"invalid character is rejected"
	);
	expect_pixel(raster, {100, 40}, 0U, "invalid view changes no pixel");
}

void test_extra_dispatch_and_kana_lifetime()
{
	Fixture fixture;
	fixture.prepare();
	DeterministicState state{};

	// Reimu
	reset(state, 0U);
	auto &reimu = state.extra_attacks[0U][0U];
	reimu.state = 1U;
	reimu.center_x = 100 << 4;
	reimu.center_y = 80 << 4;
	reimu.player_id = 1U;
	observe_extra(fixture, state);
	expect_pixel(fixture.raster, {412, 36}, 3U, "Reimu Extra active cel");
	expect(fixture.clear_output(), "clear Reimu Extra");

	// Mima
	reset(state, 1U);
	auto &mima = state.extra_attacks[0U][0U];
	mima.state = 1U;
	mima.frame = 2U;
	mima.center_x = 100 << 4;
	mima.center_y = 80 << 4;
	mima.player_id = 1U;
	observe_extra(fixture, state);
	expect_pixel(fixture.raster, {412, 36}, 3U, "Mima Extra active cel");
	expect(fixture.clear_output(), "clear Mima Extra");

	// Marisa downward column
	reset(state, 2U);
	auto &marisa = state.extra_attacks[0U][0U];
	marisa.state = 1U;
	marisa.center_x = 100 << 4;
	marisa.center_y = 80 << 4;
	marisa.player_id = 1U;
	observe_extra(fixture, state);
	expect_pixel(fixture.raster, {428, 44}, 3U, "Marisa Extra column");
	expect(fixture.clear_output(), "clear Marisa Extra");

	// Ellen trail point 6
	reset(state, 3U);
	auto &ellen = state.extra_attacks[0U][0U];
	ellen.state = 1U;
	ellen.player_id = 1U;
	ellen.ellen_trail_x[6U] = 100 << 4;
	ellen.ellen_trail_y[6U] = 80 << 4;
	observe_extra(fixture, state);
	expect_pixel(fixture.raster, {420, 40}, 3U, "Ellen Extra trail tail");
	expect(fixture.clear_output(), "clear Ellen Extra");

	// Kotohime small variant
	reset(state, 4U);
	auto &kotohime = state.extra_attacks[0U][0U];
	kotohime.state = 1U;
	kotohime.center_x = 100 << 4;
	kotohime.center_y = 80 << 4;
	kotohime.player_id = 1U;
	kotohime.kotohime_target_y = 80 << 4;
	observe_extra(fixture, state);
	expect_pixel(fixture.raster, {420, 40}, 3U, "Kotohime Extra small cel");
	expect(fixture.clear_output(), "clear Kotohime Extra");

	// Kana state 3 writes the retained destination. A later direct state 1
	// deliberately presents through that byte, not the normalized core field.
	reset(state, 5U);
	auto &kana = state.extra_attacks[0U][0U];
	kana.state = 3U;
	kana.center_x = 100 << 4;
	kana.center_y = 80 << 4;
	kana.player_id = 1U;
	observe_extra(fixture, state);
	expect(fixture.clear_output(), "clear Kana transfer");
	kana.state = 1U;
	kana.frame = 0U;
	kana.player_id = 0U;
	observe_extra(fixture, state);
	expect_pixel(fixture.raster, {420, 40}, 3U, "Kana stale destination survives direct spawn");
	expect_pixel(fixture.raster, {100, 40}, 0U, "Kana ignores normalized direct destination");
	expect(fixture.clear_output(), "clear Kana Extra");

	// Rikako
	reset(state, 6U);
	auto &rikako = state.extra_attacks[0U][0U];
	rikako.state = 1U;
	rikako.center_x = 100 << 4;
	rikako.center_y = 80 << 4;
	rikako.player_id = 1U;
	observe_extra(fixture, state);
	expect_pixel(fixture.raster, {412, 36}, 3U, "Rikako Extra active cel");
	expect(fixture.clear_output(), "clear Rikako Extra");

	// Chiyuri beam pair
	reset(state, 7U);
	auto &beam_first = state.extra_attacks[0U][0U];
	auto &beam_second = state.extra_attacks[0U][1U];
	beam_first.state = 1U;
	beam_first.center_x = 100 << 4;
	beam_first.player_id = 1U;
	beam_second.state = 1U;
	beam_second.center_x = 120 << 4;
	beam_second.player_id = 1U;
	observe_extra(fixture, state);
	expect_pixel(fixture.raster, {436, 8}, 15U, "Chiyuri Extra beam begins at first endpoint");
	expect(fixture.clear_output(), "clear Chiyuri Extra");

	// Yumemi scaffold
	reset(state, 8U);
	auto &yumemi = state.extra_attacks[0U][0U];
	yumemi.state = 1U;
	yumemi.frame = 0U;
	yumemi.center_x = 100 << 4;
	yumemi.center_y = 80 << 4;
	yumemi.player_id = 1U;
	yumemi.yumemi_extent = 16U;
	yumemi.yumemi_duration = 32U;
	observe_extra(fixture, state);
	expect_pixel(fixture.raster, {420, 40}, 3U, "Yumemi Extra scaffold");
}

void test_extra_common_clip_and_atomic_validation()
{
	Fixture fixture;
	fixture.prepare();
	DeterministicState state{};

	// Shared transfer/material sprites use the inner 16..623 screen region,
	// unlike the character-specific playfield clips used by Charge Shots.
	reset(state, 0U);
	auto &left = state.extra_attacks[0U][0U];
	left.state = 2U;
	left.frame = 0U;
	left.center_x = 0;
	left.center_y = 40 << 4;
	left.player_id = 0U;
	auto &right = state.extra_attacks[0U][1U];
	right.state = 2U;
	right.frame = 0U;
	right.center_x = 288 << 4;
	right.center_y = 40 << 4;
	right.player_id = 1U;
	auto &right_inside = state.extra_attacks[0U][2U];
	right_inside.state = 2U;
	right_inside.frame = 0U;
	right_inside.center_x = 272 << 4;
	right_inside.center_y = 40 << 4;
	right_inside.player_id = 1U;
	observe_extra(fixture, state);
	expect_pixel(fixture.raster, {15, 20}, 0U, "Extra common clip rejects x=15");
	expect_pixel(fixture.raster, {16, 20}, 3U, "Extra common clip includes x=16");
	expect_pixel(fixture.raster, {607, 20}, 3U, "Extra common clip retains the last full column");
	expect_pixel(fixture.raster, {608, 20}, 0U, "Extra common clip rejects a column ending past x=623");

	Fixture invalid;
	invalid.prepare();
	reset(state, 5U);
	auto &early = state.extra_attacks[0U][0U];
	early.state = 2U;
	early.frame = 0U;
	early.center_x = 100 << 4;
	early.center_y = 80 << 4;
	early.player_id = 1U;
	auto &late = state.extra_attacks[0U][7U];
	late.state = 2U;
	late.frame = 0U;
	late.center_x = 120 << 4;
	late.center_y = 80 << 4;
	late.player_id = 2U;
	observe_extra(invalid, state);
	expect(
		invalid.presenter->status() == MainExtraChargeStatus::invalid_observation,
		"malformed late Extra slot is rejected"
	);
	expect_pixel(
		invalid.raster,
		{420, 80},
		0U,
		"malformed late Extra slot prevents early drawing"
	);

	Fixture invalid_variant;
	invalid_variant.prepare();
	reset(state, 4U);
	auto &variant = state.extra_attacks[0U][0U];
	variant.state = 1U;
	variant.behavior_variant = 2U;
	variant.player_id = 0U;
	observe_extra(invalid_variant, state);
	expect(
		invalid_variant.presenter->status() ==
			MainExtraChargeStatus::invalid_observation,
		"invalid Kotohime variant is rejected"
	);

	Fixture invalid_pair;
	invalid_pair.prepare();
	reset(state, 7U);
	auto &pair_first = state.extra_attacks[0U][0U];
	auto &pair_second = state.extra_attacks[0U][1U];
	pair_first.state = 3U;
	pair_first.player_id = 0U;
	pair_second.state = 0U;
	pair_second.player_id = 2U;
	observe_extra(invalid_pair, state);
	expect(
		invalid_pair.presenter->status() ==
			MainExtraChargeStatus::invalid_observation,
		"invalid stale Chiyuri pair destination is rejected"
	);

	Fixture invalid_destination;
	invalid_destination.prepare();
	reset(state, 0U);
	auto &destination = state.extra_attacks[0U][0U];
	destination.state = 1U;
	destination.player_id = 2U;
	observe_extra(invalid_destination, state);
	expect(
		invalid_destination.presenter->status() ==
			MainExtraChargeStatus::invalid_observation,
		"invalid active Extra destination is rejected"
	);
}

void test_extra_shared_and_character_boundaries()
{
	Fixture shared;
	shared.prepare_marked({
		{352U, 80U, 3U},
		{320U, 80U, 4U},
		{384U, 80U, 1U},
		{432U, 80U, 2U},
		{480U, 80U, 4U},
	});
	DeterministicState state{};
	reset(state, 0U);
	auto &common = state.extra_attacks[0U][0U];
	common.state = 2U;
	common.center_x = 100 << 4;
	common.center_y = 80 << 4;
	common.player_id = 1U;
	common.frame = 3U;
	observe_extra(shared, state);
	expect_pixel(shared.raster, {420, 40}, 0U, "odd transfer frame is a no-op");
	expect(shared.clear_output(), "clear odd transfer frame");
	common.frame = 0U;
	observe_extra(shared, state);
	expect_pixel(shared.raster, {420, 40}, 3U, "transfer frames 0..3 select cel 352");
	expect(shared.clear_output(), "clear transfer cel 352");
	common.frame = 4U;
	observe_extra(shared, state);
	expect_pixel(shared.raster, {420, 40}, 4U, "transfer frames 4..7 select cel 320");
	expect(shared.clear_output(), "clear transfer cel 320");

	constexpr std::array<std::uint8_t, 4U> material_states{{3U, 4U, 8U, 12U}};
	constexpr std::array<Index4, 4U> material_colors{{1U, 2U, 4U, 1U}};
	for(std::size_t i = 0U; i < material_states.size(); ++i) {
		common.state = material_states[i];
		observe_extra(shared, state);
		expect_pixel(
			shared.raster,
			{412, 36},
			material_colors[i],
			"material phase advances every four states and wraps modulo three"
		);
		expect(shared.clear_output(), "clear material boundary");
	}

	reset(state, 1U);
	auto &mima = state.extra_attacks[0U][0U];
	mima.state = 2U;
	mima.center_x = 100 << 4;
	mima.center_y = 80 << 4;
	mima.player_id = 1U;
	mima.frame = 1U;
	mima.mima_timer = 0U;
	observe_extra(shared, state);
	expect_pixel(shared.raster, {420, 40}, 3U, "Mima transfer phase uses timer low byte");
	expect(shared.clear_output(), "clear Mima timer-even transfer");
	mima.frame = 0U;
	mima.mima_timer = 1U;
	observe_extra(shared, state);
	expect_pixel(shared.raster, {420, 40}, 0U, "Mima odd timer suppresses even frame transfer");
	expect(shared.clear_output(), "clear Mima timer-odd transfer");
	mima.state = 3U;
	mima.frame = 8U;
	observe_extra(shared, state);
	expect_pixel(shared.raster, {412, 36}, 1U, "Mima material phase uses state rather than frame");

	Fixture reimu;
	reimu.prepare_marked({
		{0U, 8U, 1U},
		{48U, 8U, 2U},
		{0U, 32U, 4U},
		{48U, 32U, 8U},
	});
	constexpr std::array<std::uint8_t, 4U> reimu_frames{{0U, 4U, 8U, 12U}};
	constexpr std::array<Index4, 4U> reimu_colors{{1U, 2U, 4U, 8U}};
	for(std::size_t i = 0U; i < reimu_frames.size(); ++i) {
		reset(state, 0U);
		auto &fact = state.extra_attacks[0U][0U];
		fact.state = 1U;
		fact.frame = reimu_frames[i];
		fact.center_x = 100 << 4;
		fact.center_y = 80 << 4;
		fact.player_id = 1U;
		observe_extra(reimu, state);
		expect_pixel(
			reimu.raster,
			{412, 36},
			reimu_colors[i],
			"Reimu Extra observes the pre-increment frame cel"
		);
		expect(reimu.clear_output(), "clear Reimu Extra frame boundary");
	}

	Fixture marisa;
	marisa.prepare_marked({
		{208U, 0U, 15U},
		{224U, 0U, 1U},
		{240U, 0U, 2U},
		{256U, 0U, 4U},
		{272U, 0U, 8U},
		{288U, 0U, 3U},
	});
	constexpr std::array<std::uint8_t, 10U> marisa_frames{{
		47U, 48U, 95U, 96U, 101U, 102U, 107U, 108U, 113U, 114U,
	}};
	constexpr std::array<Index4, 10U> marisa_colors{{
		1U, 1U, 1U, 2U, 2U, 4U, 4U, 8U, 8U, 3U,
	}};
	for(std::size_t i = 0U; i < marisa_frames.size(); ++i) {
		reset(state, 2U);
		auto &fact = state.extra_attacks[0U][0U];
		fact.state = 1U;
		fact.frame = marisa_frames[i];
		fact.center_x = 100 << 4;
		fact.center_y = 80 << 4;
		fact.player_id = 1U;
		observe_extra(marisa, state);
		expect_pixel(
			marisa.raster,
			{428, 52},
			marisa_colors[i],
			"Marisa Extra column cel boundary"
		);
		expect_pixel(
			marisa.raster,
			{428, 44},
			static_cast<Index4>((marisa_frames[i] < 48U) ? 15U : 0U),
			"Marisa Extra head ends at frame 48"
		);
		expect(marisa.clear_output(), "clear Marisa Extra frame boundary");
	}

	Fixture kotohime;
	kotohime.prepare_marked({{16U, 48U, 5U}, {0U, 40U, 6U}});
	for(std::uint8_t variant = 0U; variant < 2U; ++variant) {
		reset(state, 4U);
		auto &fact = state.extra_attacks[0U][0U];
		fact.state = 1U;
		fact.frame = 0U;
		fact.center_x = 100 << 4;
		fact.center_y = 80 << 4;
		fact.player_id = 1U;
		fact.kotohime_target_y = 80 << 4;
		fact.behavior_variant = variant;
		observe_extra(kotohime, state);
		expect_pixel(
			kotohime.raster,
			{static_cast<std::int32_t>((variant == 0U) ? 420 : 404),
			 static_cast<std::int32_t>((variant == 0U) ? 40 : 32)},
			static_cast<Index4>((variant == 0U) ? 5U : 6U),
			"Kotohime Extra variant selects small or large cel"
		);
		expect(kotohime.clear_output(), "clear Kotohime Extra variant");
	}
}

void test_extra_chiyuri_and_yumemi_boundaries()
{
	Fixture chiyuri;
	chiyuri.prepare_marked({});
	DeterministicState state{};
	constexpr std::array<std::uint8_t, 8U> frames{{
		15U, 16U, 39U, 40U, 111U, 112U, 135U, 136U,
	}};
	for(const auto frame : frames) {
		reset(state, 7U);
		auto &first = state.extra_attacks[0U][0U];
		auto &second = state.extra_attacks[0U][1U];
		first.state = 1U;
		first.frame = frame;
		first.center_x = 100 << 4;
		first.player_id = 0U;
		second.center_x = 100 << 4;
		second.player_id = 0U;
		observe_extra(chiyuri, state);
		const auto center = static_cast<Index4>((frame < 136U) ? 15U : 0U);
		expect_pixel(chiyuri.raster, {116, 8}, center, "Chiyuri beam terminal boundary");
		if(frame == 39U) {
			expect_pixel(chiyuri.raster, {111, 8}, 9U, "Chiyuri expanding radius at frame 39");
		}
		if(frame == 40U || frame == 111U) {
			expect_pixel(chiyuri.raster, {115, 8}, 15U, "Chiyuri fixed inner band");
		}
		if(frame == 112U) {
			expect_pixel(chiyuri.raster, {115, 8}, 10U, "Chiyuri contraction begins at frame 112");
		}
		if(frame == 16U || frame == 135U || frame == 136U) {
			expect_pixel(chiyuri.raster, {115, 8}, 0U, "Chiyuri zero-radius or terminal edge");
		}
		expect(chiyuri.clear_output(), "clear Chiyuri beam boundary");
	}

	Fixture yumemi;
	yumemi.prepare_marked({{32U, 8U, 3U}});
	reset(state, 8U);
	auto &fact = state.extra_attacks[0U][0U];
	fact.state = 1U;
	fact.center_x = 100 << 4;
	fact.center_y = 80 << 4;
	fact.player_id = 1U;
	fact.yumemi_duration = 50U;
	fact.frame = 16U;
	fact.yumemi_extent = 24U;
	observe_extra(yumemi, state);
	expect_pixel(yumemi.raster, {420, 16}, 6U, "Yumemi frame 16 uses opening box geometry");
	expect_pixel(yumemi.raster, {420, 40}, 6U, "Yumemi extent 24 omits center cel");
	expect(yumemi.clear_output(), "clear Yumemi frame 16");
	fact.frame = 17U;
	fact.yumemi_extent = 25U;
	observe_extra(yumemi, state);
	expect_pixel(yumemi.raster, {445, 28}, 5U, "Yumemi frame 17 uses collapsed duration geometry");
	expect_pixel(yumemi.raster, {420, 40}, 3U, "Yumemi extent 25 includes center cel");
}

void set_position(std::int16_t &x, std::int16_t &y)
{
	x = 100 << 4;
	y = 80 << 4;
}

void test_charge_all_characters_and_boundaries()
{
	Fixture fixture;
	fixture.prepare();
	DeterministicState state{};

	reset(state, 0U);
	state.reimu_charge[0U][0U].state = 1U;
	for(const auto point : {0U, 2U, 4U, 6U}) {
		set_position(
			state.reimu_charge[0U][0U].x[point],
			state.reimu_charge[0U][0U].y[point]
		);
	}
	observe_charge(fixture, state);
	expect_pixel(fixture.raster, {92, 36}, 3U, "Reimu Charge full-color head is last");
	expect(fixture.clear_output(), "clear Reimu Charge");

	reset(state, 1U);
	state.mima_charge_active[0U] = 1U;
	state.mima_charge[0U][4U].active = 1U;
	set_position(state.mima_charge[0U][4U].x, state.mima_charge[0U][4U].y);
	state.mima_charge[0U][4U].visual_frame = 0xFFU;
	observe_charge(fixture, state);
	expect_pixel(fixture.raster, {100, 44}, 3U, "Mima Charge uses node-index cel");
	expect(fixture.clear_output(), "clear Mima Charge");

	reset(state, 2U);
	state.marisa_charge[0U].active = 1U;
	for(std::size_t node = 0U; node < 12U; ++node) {
		state.marisa_charge[0U].x[node] = 100 << 4;
		state.marisa_charge[0U].y[node] = 80 << 4;
	}
	observe_charge(fixture, state);
	expect_pixel(fixture.raster, {114, 8}, 15U, "Marisa Charge solid head beam");
	expect(fixture.clear_output(), "clear Marisa Charge");

	reset(state, 3U);
	state.ellen_charge[0U][0U].state = 1U;
	state.ellen_charge[0U][0U].angle = 0x10U;
	set_position(state.ellen_charge[0U][0U].x, state.ellen_charge[0U][0U].y);
	observe_charge(fixture, state);
	expect_pixel(fixture.raster, {108, 44}, 3U, "Ellen Charge angle cel");
	expect(fixture.clear_output(), "clear Ellen Charge");

	reset(state, 4U);
	state.kotohime_charge[0U].active = 1U;
	set_position(state.kotohime_charge[0U].x, state.kotohime_charge[0U].y);
	observe_charge(fixture, state);
	expect_pixel(fixture.raster, {68, 44}, 3U, "Kotohime Charge wide cel");
	expect(fixture.clear_output(), "clear Kotohime Charge");

	reset(state, 5U);
	state.kana_charge_state[0U] = 1U;
	for(std::size_t group = 0U; group < 4U; ++group) {
		for(const auto point : {0U, 4U, 8U, 12U}) {
			set_position(
				state.kana_charge[0U][group].x[point],
				state.kana_charge[0U][group].y[point]
			);
		}
	}
	observe_charge(fixture, state);
	expect_pixel(fixture.raster, {100, 40}, 3U, "Kana Charge OR trail");
	expect(fixture.clear_output(), "clear Kana Charge");

	reset(state, 6U);
	state.rikako_charge_state[0U] = 1U;
	set_position(state.rikako_charge[0U][0U].x, state.rikako_charge[0U][0U].y);
	observe_charge(fixture, state);
	expect_pixel(fixture.raster, {100, 40}, 3U, "Rikako Charge node");
	expect(fixture.clear_output(), "clear Rikako Charge");

	reset(state, 7U);
	state.chiyuri_charge[0U][7U].state = 1U;
	state.chiyuri_charge[0U][7U].frame = 1U;
	set_position(state.chiyuri_charge[0U][7U].x, state.chiyuri_charge[0U][7U].y);
	observe_charge(fixture, state);
	expect_pixel(fixture.raster, {100, 36}, 3U, "Chiyuri Charge reverse first node");
	expect(fixture.clear_output(), "clear Chiyuri Charge");

	reset(state, 8U);
	state.yumemi_charge[0U].state = 1U;
	state.yumemi_charge[0U].frame = 1U;
	set_position(state.yumemi_charge[0U].x, state.yumemi_charge[0U].y);
	observe_charge(fixture, state);
	expect_pixel(fixture.raster, {100, 40}, 3U, "Yumemi Charge state-1 cel");
}

void test_charge_layer_modes_and_boundaries()
{
	Fixture reimu;
	reimu.prepare_marked({
		{0U, 56U, 9U},
		{48U, 56U, 15U},
		{96U, 56U, 15U},
		{144U, 56U, 15U},
	});
	DeterministicState state{};
	reset(state, 0U);
	auto &group = state.reimu_charge[0U][0U];
	group.state = 1U;
	constexpr std::array<std::size_t, 4U> nodes{{0U, 2U, 4U, 6U}};
	constexpr std::array<std::int16_t, 4U> xs{{80, 120, 160, 200}};
	for(std::size_t i = 0U; i < nodes.size(); ++i) {
		group.x[nodes[i]] = static_cast<std::int16_t>(xs[i] << 4);
		group.y[nodes[i]] = 80 << 4;
	}
	observe_charge(reimu, state);
	expect_pixel(reimu.raster, {72, 36}, 9U, "Reimu Charge head keeps source color");
	expect_pixel(reimu.raster, {112, 36}, 6U, "Reimu Charge second layer is color 6");
	expect_pixel(reimu.raster, {152, 36}, 5U, "Reimu Charge third layer is color 5");
	expect_pixel(reimu.raster, {192, 36}, 2U, "Reimu Charge tail layer is color 2");
	expect(reimu.clear_output(), "clear Reimu Charge layer colors");
	for(const auto node : nodes) {
		group.x[node] = 100 << 4;
	}
	observe_charge(reimu, state);
	expect_pixel(reimu.raster, {92, 36}, 9U, "Reimu Charge full-color head draws last");

	Fixture marisa;
	marisa.prepare_marked({});
	reset(state, 2U);
	state.marisa_charge[0U].active = 1U;
	for(std::size_t node = 7U; node <= 11U; ++node) {
		state.marisa_charge[0U].x[node] = 100 << 4;
		state.marisa_charge[0U].y[node] = static_cast<std::int16_t>(
			((node == 10U) ? 160 : 80) << 4
		);
	}
	for(std::size_t node = 4U; node <= 6U; ++node) {
		state.marisa_charge[0U].x[node] = 140 << 4;
		state.marisa_charge[0U].y[node] = 80 << 4;
	}
	for(std::size_t node = 1U; node <= 3U; ++node) {
		state.marisa_charge[0U].x[node] = 180 << 4;
		state.marisa_charge[0U].y[node] = 80 << 4;
	}
	state.marisa_charge[0U].x[0U] = 220 << 4;
	state.marisa_charge[0U].y[0U] = 80 << 4;
	observe_charge(marisa, state);
	expect_pixel(marisa.raster, {114, 10}, 9U, "Marisa Charge band 0 even tile column");
	expect_pixel(marisa.raster, {115, 10}, 2U, "Marisa Charge band 0 odd tile column");
	expect_pixel(marisa.raster, {154, 10}, 10U, "Marisa Charge band 1 even tile column");
	expect_pixel(marisa.raster, {155, 10}, 9U, "Marisa Charge band 1 odd tile column");
	expect_pixel(marisa.raster, {194, 10}, 10U, "Marisa Charge band 2 even tile column");
	expect_pixel(marisa.raster, {195, 10}, 15U, "Marisa Charge band 2 odd tile column");
	expect_pixel(marisa.raster, {234, 10}, 15U, "Marisa Charge head is solid white");
	expect_pixel(marisa.raster, {114, 70}, 0U, "Marisa Charge suppresses duplicate X columns");

	Fixture ellen;
	ellen.prepare_marked({{128U, 0U, 1U}, {144U, 0U, 2U}});
	reset(state, 3U);
	auto &before = state.ellen_charge[0U][0U];
	before.state = 1U;
	before.angle = 0x0FU;
	set_position(before.x, before.y);
	auto &after = state.ellen_charge[0U][1U];
	after.state = 1U;
	after.angle = 0x10U;
	after.x = 140 << 4;
	after.y = 80 << 4;
	observe_charge(ellen, state);
	expect_pixel(ellen.raster, {108, 44}, 1U, "Ellen Charge angle 0x0F selects phase 0");
	expect_pixel(ellen.raster, {148, 44}, 2U, "Ellen Charge angle 0x10 selects phase 1");

	Fixture kana;
	kana.prepare_marked({{0U, 8U, 1U}, {0U, 24U, 2U}});
	reset(state, 5U);
	state.kana_charge_state[0U] = 1U;
	for(auto &charge_group : state.kana_charge[0U]) {
		for(std::size_t point = 0U; point < charge_group.x.size(); ++point) {
			charge_group.x[point] = -16000;
			charge_group.y[point] = -16000;
		}
	}
	set_position(
		state.kana_charge[0U][0U].x[0U],
		state.kana_charge[0U][0U].y[0U]
	);
	set_position(
		state.kana_charge[0U][0U].x[4U],
		state.kana_charge[0U][0U].y[4U]
	);
	observe_charge(kana, state);
	expect_pixel(kana.raster, {100, 40}, 3U, "Kana Charge ORs overlapping source planes");

	Fixture chiyuri;
	chiyuri.prepare_marked({{0U, 8U, 1U}, {32U, 8U, 2U}});
	reset(state, 7U);
	auto &latest = state.chiyuri_charge[0U][7U];
	latest.state = 1U;
	latest.frame = 0U;
	set_position(latest.x, latest.y);
	state.chiyuri_charge[0U][6U].state = 0U;
	auto &hidden = state.chiyuri_charge[0U][5U];
	hidden.state = 1U;
	hidden.frame = 1U;
	hidden.x = 140 << 4;
	hidden.y = 80 << 4;
	observe_charge(chiyuri, state);
	expect_pixel(chiyuri.raster, {100, 36}, 1U, "Chiyuri Charge draws nodes before the first free slot");
	expect_pixel(chiyuri.raster, {140, 36}, 0U, "Chiyuri Charge terminates at the first reverse free slot");
}

void test_charge_yumemi_schedule()
{
	Fixture fixture;
	fixture.trig.sine.fill(0);
	fixture.trig.cosine.fill(256);
	fixture.prepare_marked({{0U, 56U, 1U}, {32U, 56U, 2U}});
	DeterministicState state{};
	struct Case {
		std::uint8_t frame;
		std::int16_t first_x;
		std::int16_t second_x;
	};
	constexpr std::array<Case, 16U> cases{{
		{7U, 156, -1},
		{8U, 108, -1},
		{15U, 164, -1},
		{16U, 108, -1},
		{23U, 164, -1},
		{24U, 100, -1},
		{31U, 156, -1},
		{32U, 108, -1},
		{39U, 164, -1},
		{40U, 108, -1},
		{47U, 164, -1},
		{48U, 108, 100},
		{55U, 164, 156},
		{56U, 164, -1},
		{63U, 220, -1},
		{64U, -1, -1},
	}};
	for(const auto &entry : cases) {
		reset(state, 8U);
		auto &fact = state.yumemi_charge[0U];
		fact.state = 2U;
		fact.frame = entry.frame;
		set_position(fact.x, fact.y);
		observe_charge(fixture, state);
		const auto color = static_cast<Index4>((entry.frame & 1U) ? 2U : 1U);
		if(entry.first_x >= 0) {
			expect_pixel(
				fixture.raster,
				{entry.first_x, 40},
				color,
				"Yumemi Charge boundary selects the exact ring length"
			);
		} else {
			expect_pixel(fixture.raster, {100, 40}, 0U, "Yumemi Charge frame 64 is terminal");
		}
		if(entry.second_x >= 0) {
			expect_pixel(
				fixture.raster,
				{entry.second_x, 40},
				color,
				"Yumemi Charge late phase draws its additional ring"
			);
		}
		expect(fixture.clear_output(), "clear Yumemi Charge schedule boundary");
	}
}

} // namespace

int main()
{
	test_construction_and_atomic_rejection();
	test_extra_dispatch_and_kana_lifetime();
	test_extra_common_clip_and_atomic_validation();
	test_extra_shared_and_character_boundaries();
	test_extra_chiyuri_and_yumemi_boundaries();
	test_charge_all_characters_and_boundaries();
	test_charge_layer_modes_and_boundaries();
	test_charge_yumemi_schedule();

	if(failures != 0U) {
		std::fprintf(
			stderr,
			"%zu of %zu Extra/Charge checks failed\n",
			failures,
			checks
		);
		return 1;
	}
	std::printf("all %zu Extra/Charge checks passed\n", checks);
	return 0;
}
