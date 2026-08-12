#include "th03/present/main_hud_result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

#include "th03/assets/types.hpp"
#include "th03/present/main_static.hpp"

namespace {

using th03::HudStartCompatibilityRead;
using th03::HudStartCompatibilityReadSite;
using th03::HudStartFrameStep;
using th03::MainComboState;
using th03::MainHudStartState;
using th03::MainResultState;
using th03::MainResultTransitionPhase;
using th03::MainResultTransitionState;
using th03::MainScoreState;
using th03::PlayerId;
using th03::assets::BfntConsumerPolicy;
using th03::assets::BfntGlyph;
using th03::assets::BfntSheet;
using th03::assets::PlanarBitOrder;
using th03::assets::PlanarBitmap4;
using th03::assets::PlanarRowOrder;
using th03::present::Index4;
using th03::present::MainHudResultPresenter;
using th03::present::MainHudResultSheets;
using th03::present::MainHudResultStatus;
using th03::present::MainHudResultTramAssets;
using th03::present::MainResultLabel;
using th03::present::PackedRowOrder;
using th03::present::Page;
using th03::present::Point;
using th03::present::PreparedSuperimposeSheet;
using th03::present::Raster;
using th03::present::RasterMode;
using th03::present::ScoreFontMasks;
using th03::present::StaffFlakeTrigTables;
using th03::present::Tram;
using th03::present::TramCell;
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

void expect_pixel(
	const Raster &raster,
	Point point,
	Index4 color,
	std::string_view message
)
{
	expect(raster.pixel(Page::zero, point) == color, message);
}

BfntSheet make_sheet(
	std::uint16_t glyph_count,
	std::uint16_t width = 32U,
	std::uint16_t height = 32U
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
		PlanarBitmap4 pixels{};
		pixels.row_order = PlanarRowOrder::top_to_bottom;
		pixels.bit_order = PlanarBitOrder::most_significant_bit_left;
		const auto color = static_cast<std::uint8_t>((code * 2U + 1U) & 15U);
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

struct Fixture {
	Raster raster;
	Tram tram;
	std::array<std::uint8_t, 80U> score_rows{};
	std::array<std::int16_t, 256U> sine{};
	std::array<std::int16_t, 256U> cosine{};
	std::array<std::array<TramCell, 32U>, 9U> labels{};
	std::array<TramCell, 4U> combo_hit{};
	std::array<TramCell, 8U> combo_bonus{};
	std::array<TramCell, 8U> combo_erase{};
	std::optional<PreparedSuperimposeSheet> lose;
	std::optional<PreparedSuperimposeSheet> round;
	std::optional<PreparedSuperimposeSheet> zikicw;
	std::optional<MainHudResultPresenter> presenter;

	Fixture()
	{
		score_rows.fill(0x80U);
		for(std::size_t label = 0U; label < labels.size(); ++label) {
			for(auto &cell : labels[label]) {
				cell = {
					static_cast<std::uint16_t>(0x2100U + label),
					tram_white,
				};
			}
		}
		for(auto &cell : combo_hit) {
			cell = {0x2201U, tram_white};
		}
		for(auto &cell : combo_bonus) {
			cell = {0x2202U, tram_white};
		}
		for(auto &cell : combo_erase) {
			cell = {' ', tram_white};
		}
	}

	[[nodiscard]] MainHudResultTramAssets tram_assets() const noexcept
	{
		MainHudResultTramAssets assets{};
		for(std::size_t index = 0U; index < labels.size(); ++index) {
			const auto id = static_cast<MainResultLabel>(index);
			const auto size =
				(id == MainResultLabel::winner_bonus ||
				 id == MainResultLabel::all_clear_bonus) ? 32U : 26U;
			assets.result_labels[index] = std::span<const TramCell>(labels[index])
				.first(size);
		}
		assets.combo_hit = combo_hit;
		assets.combo_bonus = combo_bonus;
		assets.combo_erase = combo_erase;
		return assets;
	}

	void prepare(std::uint8_t continues = 0U)
	{
		lose = th03::present::prepare_superimpose_sheet(make_sheet(4U, 64U, 32U));
		round = th03::present::prepare_superimpose_sheet(make_sheet(22U, 32U, 16U));
		zikicw = th03::present::prepare_superimpose_sheet(make_sheet(16U, 32U, 16U));
		expect(lose.has_value() && round.has_value() && zikicw.has_value(),
			"prepare synthetic HUD/result sheets");
		expect(
			raster.set_raster_mode(RasterMode::zoomed_200_upper),
			"select MAIN raster mode"
		);
		presenter.emplace(
			raster,
			tram,
			ScoreFontMasks{score_rows, PackedRowOrder::bottom_to_top},
			StaffFlakeTrigTables{sine, cosine},
			MainHudResultSheets{&*lose, &*round, &*zikicw},
			tram_assets(),
			continues
		);
		expect(presenter->ready(), "HUD/result presenter starts ready");
	}

	[[nodiscard]] bool clear(Index4 color = 0U)
	{
		return raster.clear({0, 0, 640U, 200U}, color);
	}
};

void send_particle_reads(
	MainHudResultPresenter &presenter,
	bool signed_first_delta
)
{
	for(std::uint16_t ordinal = 0U; ordinal < 128U; ++ordinal) {
		const auto particle = static_cast<std::uint8_t>(ordinal);
		presenter.hud_start_compatibility_read({
			HudStartCompatibilityReadSite::particle_y,
			particle,
			static_cast<std::uint16_t>((ordinal == 0U) ? 0U : 511U),
		});
		presenter.hud_start_compatibility_read({
			HudStartCompatibilityReadSite::particle_angle,
			particle,
			0U,
		});
		presenter.hud_start_compatibility_read({
			HudStartCompatibilityReadSite::angle_delta_first,
			particle,
			static_cast<std::uint16_t>(
				(signed_first_delta && ordinal == 0U) ? 0xFFFFU : 1U
			),
		});
		presenter.hud_start_compatibility_read({
			HudStartCompatibilityReadSite::angle_delta_second,
			particle,
			1U,
		});
	}
}

void test_constructor_and_support_helpers()
{
	Fixture fixture;
	fixture.prepare();
	expect(
		fixture.lose->pixel_width() == 64U &&
		fixture.lose->pixel_height() == 32U,
		"prepared superimpose dimensions are observable"
	);
	expect(fixture.clear(), "clear row primitive output");
	expect(
		th03::present::superimpose_put_blue_plane_row_white(
			fixture.raster, *fixture.round, {0, 12}, 0U, 7U
		),
		"flying row primitive accepts exact 32x16 sheet"
	);
	for(std::int32_t x = 0; x < 32; ++x) {
		expect_pixel(fixture.raster, {x, 12}, 15U, "flying row writes B mask white");
	}
	expect(fixture.clear(), "clear clipped row output");
	expect(
		th03::present::superimpose_put_blue_plane_row_white(
			fixture.raster, *fixture.round, {-8, 12}, 0U, 7U
		),
		"flying row accepts negative byte start"
	);
	expect_pixel(fixture.raster, {0, 12}, 15U, "negative first byte is skipped");
	expect_pixel(fixture.raster, {23, 12}, 15U, "remaining three bytes render");
	expect_pixel(fixture.raster, {24, 12}, 0U, "skipped byte is not shifted back in");
	auto sparse_decoded = make_sheet(1U);
	auto &sparse_pixels = std::get<PlanarBitmap4>(
		sparse_decoded.glyphs[0U].pixels
	);
	for(auto &plane : sparse_pixels.color_planes) {
		plane.assign(128U, 0U);
	}
	sparse_pixels.color_planes[0U][7U * 4U] = 0x80U;
	auto sparse = th03::present::prepare_superimpose_sheet(sparse_decoded);
	expect(sparse.has_value() && fixture.clear(), "prepare sparse rotated row");
	expect(
		th03::present::superimpose_put_blue_plane_row_white(
			fixture.raster, *sparse, {3, 12}, 0U, 7U
		),
		"flying row rotates a sparse source byte"
	);
	expect_pixel(fixture.raster, {3, 12}, 15U, "byte rotation preserves shifted MSB");
	expect_pixel(fixture.raster, {2, 12}, 0U, "byte rotation preserves left neighbor");
	expect_pixel(fixture.raster, {4, 12}, 0U, "byte rotation preserves right neighbor");

	auto wrong = th03::present::prepare_superimpose_sheet(make_sheet(4U, 32U, 32U));
	expect(wrong.has_value(), "prepare range-correct wrong-size sheet");
	MainHudResultPresenter invalid(
		fixture.raster,
		fixture.tram,
		{fixture.score_rows, PackedRowOrder::bottom_to_top},
		{fixture.sine, fixture.cosine},
		{&*wrong, &*fixture.round, &*fixture.zikicw},
		fixture.tram_assets(),
		0U
	);
	expect(
		invalid.status() == MainHudResultStatus::invalid_assets,
		"constructor rejects range-correct wrong-size sheet"
	);
	MainHudResultPresenter bad_continues(
		fixture.raster,
		fixture.tram,
		{fixture.score_rows, PackedRowOrder::bottom_to_top},
		{fixture.sine, fixture.cosine},
		{&*fixture.lose, &*fixture.round, &*fixture.zikicw},
		fixture.tram_assets(),
		4U
	);
	expect(
		bad_continues.status() == MainHudResultStatus::invalid_assets,
		"constructor rejects continues-used digit above three"
	);
}

void test_hud_banner_and_particles()
{
	Fixture banner;
	banner.prepare();
	for(std::size_t update = 0U; update < 68U; ++update) {
		banner.presenter->hud_start_frame_step({
			update == 0U,
			false,
			0U,
			1U,
			0U,
			static_cast<std::uint16_t>(update),
		});
		banner.presenter->main_hud_start_state({
			1U,
			false,
			0U,
			0U,
			static_cast<std::uint16_t>(update + 1U),
		});
	}
	expect(banner.clear(), "clear flying banner rows before full banner");
	banner.presenter->hud_start_frame_step({false, false, 0U, 1U, 0U, 68U});
	banner.presenter->main_hud_start_state({1U, false, 0U, 1U, 0U});
	expect_pixel(
		banner.raster, {64, 84}, 9U,
		"state-one handoff uses record fifteen X"
	);
	expect_pixel(
		banner.raster, {62, 84}, 0U,
		"state-one handoff does not use record zero X"
	);

	Fixture particles;
	particles.cosine[248U] = 256;
	particles.prepare();
	particles.presenter->hud_start_frame_step({false, false, 0U, 0U, 2U, 0U});
	send_particle_reads(*particles.presenter, true);
	particles.presenter->main_hud_start_state({0U, false, 0U, 3U, 0U});
	expect(particles.clear(), "clear initialized particles");
	particles.presenter->hud_start_frame_step({false, false, 0U, 0U, 3U, 0U});
	particles.presenter->main_hud_start_state({0U, false, 0U, 3U, 1U});
	expect(particles.clear(), "clear first particle angle update");
	particles.presenter->hud_start_frame_step({false, false, 0U, 0U, 3U, 1U});
	particles.presenter->main_hud_start_state({0U, false, 0U, 3U, 2U});
	expect_pixel(
		particles.raster, {101, 84}, 15U,
		"high-bit raw delta uses signed IDIV remainder"
	);
	expect(particles.presenter->ready(), "complete particle sequence remains ready");

	Fixture terminal;
	terminal.prepare();
	terminal.presenter->hud_start_frame_step({false, false, 0U, 0U, 2U, 0U});
	send_particle_reads(*terminal.presenter, false);
	terminal.presenter->hud_start_frame_step({true, true, 1U, 0U, 0U, 0U});
	expect(
		terminal.presenter->ready(),
		"repeat initialize accepts terminally omitted late HUD callback"
	);

	Fixture state_one_boundary;
	state_one_boundary.prepare();
	state_one_boundary.presenter->hud_start_frame_step({
		false, false, 0U, 0U, 1U, 0x56U,
	});
	state_one_boundary.presenter->main_hud_start_state({
		0U, false, 0U, 2U, 0U,
	});
	expect(
		state_one_boundary.presenter->ready(),
		"state-one transition to state two resets semantic frame to zero"
	);

	Fixture digit_overflow;
	digit_overflow.prepare();
	digit_overflow.presenter->hud_start_frame_step({
		false, true, 10U, 0U, 1U, 23U,
	});
	digit_overflow.presenter->main_hud_start_state({
		0U, true, 10U, 1U, 24U,
	});
	expect(
		digit_overflow.presenter->status() ==
			MainHudResultStatus::invalid_event_sequence,
		"story digit cannot spill from ROUND into ZIKICW"
	);

	Fixture malformed;
	malformed.prepare();
	malformed.presenter->hud_start_frame_step({true, false, 0U, 0U, 5U, 0U});
	expect(
		malformed.presenter->status() == MainHudResultStatus::invalid_event_sequence,
		"malformed HUD state is rejected before private mutation"
	);
}

MainResultState banner_state(PlayerId defeated, std::uint16_t frame)
{
	MainResultState state{};
	state.context.round_or_result_frame = frame;
	state.defeated_player = defeated;
	state.winner = (defeated == PlayerId::p1) ? PlayerId::p2 : PlayerId::p1;
	return state;
}

void test_result_banner_summary_and_story_boundaries()
{
	Fixture banner;
	banner.prepare();
	auto state = banner_state(PlayerId::p1, 0U);
	banner.presenter->main_result_state(state);
	expect_pixel(banner.raster, {96, 40}, 1U, "loser draws fixed pattern 18");
	expect_pixel(banner.raster, {160, 40}, 3U, "loser draws fixed pattern 19 at +64");
	expect_pixel(banner.raster, {416, 32}, 5U, "winner draws fixed pattern 20");
	expect_pixel(banner.raster, {480, 32}, 7U, "winner draws fixed pattern 21 at +64");

	Fixture later_stage;
	later_stage.prepare();
	state = banner_state(PlayerId::p2, 1U);
	state.summary_active = true;
	state.story_stage = 9U;
	state.story_lives = 7U;
	state.winner_stats.combo_hits_max = 5U;
	later_stage.presenter->main_result_state(state);
	const auto all_clear = later_stage.tram.cell({4, 6});
	expect(
		all_clear && all_clear->code == later_stage.labels[
			static_cast<std::size_t>(MainResultLabel::all_clear_bonus)
		][0U].code,
		"story stage above eight still selects ALL CLEAR label"
	);
	const auto lives_position = later_stage.tram.cell({26, 18});
	expect(
		lives_position && lives_position->code == later_stage.labels[
			static_cast<std::size_t>(MainResultLabel::player_remaining)
		][22U].code,
		"story stage above eight does not render or snapshot lives numeric"
	);
	state.context.round_or_result_frame = 0U;
	state.winner_stats = {};
	state.winner_stats.gauge_attacks_fired = 1U;
	expect(later_stage.clear(), "clear before stale frame-zero result redraw");
	later_stage.presenter->main_result_state(state);
	expect(
		later_stage.tram.cell({26, 8})->code ==
			th03::present::gaiji_cell_code(0xA5U, th03::present::GaijiHalf::left),
		"result frame zero retains prior snapshot"
	);
	expect_pixel(
		later_stage.raster, {248, 161}, 0U,
		"result frame zero retains prior quotient rather than new gauge total"
	);
	expect_pixel(
		later_stage.raster, {256, 161}, 12U,
		"result frame zero retains prior nonzero remainder raster"
	);

	Fixture final_stage;
	final_stage.prepare();
	state = banner_state(PlayerId::p2, 1U);
	state.summary_active = true;
	state.story_stage = 8U;
	state.story_lives = 7U;
	final_stage.presenter->main_result_state(state);
	expect(
		final_stage.tram.cell({26, 18})->code ==
			th03::present::gaiji_cell_code(0xA7U, th03::present::GaijiHalf::left),
		"story stage eight renders retained lives numeric"
	);
	expect_pixel(
		final_stage.raster, {288, 145}, 15U,
		"story stage eight renders 100000 constant"
	);

	Fixture player_two;
	player_two.prepare();
	state = banner_state(PlayerId::p1, 1U);
	state.summary_active = true;
	state.story_stage = 8U;
	state.story_lives = 7U;
	player_two.presenter->main_result_state(state);
	expect(
		player_two.tram.cell({44, 6})->code == player_two.labels[
			static_cast<std::size_t>(MainResultLabel::all_clear_bonus)
		][0U].code &&
		player_two.tram.cell({66, 18})->code ==
			th03::present::gaiji_cell_code(
				0xA7U, th03::present::GaijiHalf::left
			),
		"P2 winner summary shifts every TRAM row by forty source columns"
	);
	expect_pixel(
		player_two.raster, {608, 145}, 15U,
		"P2 winner summary shifts point values by 320 pixels"
	);

	Fixture invalid;
	invalid.prepare();
	state = {};
	state.summary_active = true;
	expect(invalid.clear(6U), "seed atomic invalid result frame");
	invalid.presenter->main_result_state(state);
	expect(
		invalid.presenter->status() == MainHudResultStatus::invalid_event_sequence,
		"summary without winner is rejected"
	);
	expect_pixel(invalid.raster, {96, 40}, 6U, "invalid result fact draws no early banner");
}

void test_score_combo_and_dynamic_hud()
{
	Fixture score;
	score.prepare(3U);
	MainScoreState score_state{};
	score_state.digits[0U][6U] = 2U;
	score.presenter->main_score_state(score_state);
	expect_pixel(score.raster, {224, 9}, 0U, "P1 leading score zero stays suppressed");
	expect_pixel(score.raster, {232, 9}, 15U, "P1 first nonzero score digit renders");
	expect_pixel(score.raster, {608, 9}, 15U, "P2 forced trailing zero renders");
	expect_pixel(score.raster, {288, 9}, 15U, "configured continues-used digit renders");

	Fixture invalid_score;
	invalid_score.prepare();
	expect(invalid_score.clear(6U), "seed atomic invalid score frame");
	score_state = {};
	score_state.digits[1U][0U] = 10U;
	invalid_score.presenter->main_score_state(score_state);
	expect(
		invalid_score.presenter->status() == MainHudResultStatus::invalid_event_sequence,
		"late malformed score digit rejects whole fact"
	);
	expect_pixel(
		invalid_score.raster, {224, 9}, 6U,
		"late malformed score digit prevents early P1 draw"
	);

	Fixture combo;
	combo.prepare();
	MainComboState combo_state{};
	combo_state.context.round_or_result_frame = 2U;
	combo_state.players[0U] = {80U, 12U, 12U};
	combo.presenter->main_combo_state(combo_state);
	expect(
		combo.tram.cell({4, 3})->code == combo.combo_bonus[0U].code,
		"combo frame starts at source TRAM x=4"
	);
	expect_pixel(combo.raster, {64, 25}, 12U, "ordinary combo uses bright-yellow color 12");
	combo_state = {};
	combo_state.context.round_or_result_frame = 0U;
	combo_state.players[0U] = {1U, 12U, 12U};
	combo.presenter->main_combo_state(combo_state);
	expect(
		combo.tram.cell({4, 2})->code == ' ' &&
		combo.tram.cell({4, 3})->code == ' ',
		"last combo frame erases both eight-cell TRAM rows"
	);
	expect_pixel(combo.raster, {64, 25}, 8U, "last combo leaves flashed raster bonus visible");

	Fixture three_digit;
	three_digit.prepare();
	combo_state = {};
	combo_state.players[0U] = {80U, 113U, 12U};
	three_digit.presenter->main_combo_state(combo_state);
	expect(
		three_digit.tram.cell({2, 2})->code ==
			th03::present::gaiji_cell_code(
				0xA1U, th03::present::GaijiHalf::left
			) && three_digit.tram.cell({4, 2})->code ==
			th03::present::gaiji_cell_code(
				0xA1U, th03::present::GaijiHalf::left
			) && three_digit.tram.cell({6, 2})->code ==
			th03::present::gaiji_cell_code(
				0xA3U, th03::present::GaijiHalf::left
			),
		"Replay Patch combo display adds a hundreds digit left of the source field"
	);
	combo_state.players[0U].time = 1U;
	three_digit.presenter->main_combo_state(combo_state);
	expect(
		three_digit.tram.cell({2, 2})->code ==
			th03::present::gaiji_cell_code(
				th03::present::gaiji_space, th03::present::GaijiHalf::left
			),
		"three-digit combo teardown erases the added hundreds cell"
	);

	Fixture dynamic;
	dynamic.prepare();
	dynamic.presenter->main_hud_player_value_changed({
		PlayerId::p1,
		th03::MainHudPlayerValueKind::halfhearts,
		7U,
	});
	expect(
		dynamic.tram.cell({15, 1})->code ==
			th03::present::gaiji_cell_code(0x05U, th03::present::GaijiHalf::left),
		"public dynamic halfheart callback uses static HUD put"
	);
}

void test_transition_boundaries()
{
	Fixture opening;
	opening.prepare();
	MainResultTransitionState transition{};
	transition.phase = MainResultTransitionPhase::opening;
	for(std::uint8_t frame = 0U; frame < 15U; ++frame) {
		transition.frame_before = frame;
		opening.presenter->main_result_transition_state(transition);
	}
	expect(opening.clear(), "clear before opening completion frame");
	transition.frame_before = 15U;
	opening.presenter->main_result_transition_state(transition);
	expect_pixel(opening.raster, {16, 0}, 2U, "opening completion fills P1 left edge");
	expect_pixel(opening.raster, {303, 183}, 2U, "opening completion fills P1 inclusive end");
	expect_pixel(opening.raster, {336, 0}, 2U, "opening completion fills P2 left edge");
	expect_pixel(opening.raster, {623, 183}, 2U, "opening completion fills P2 inclusive end");
	expect_pixel(opening.raster, {304, 0}, 0U, "opening completion preserves center gap");

	Fixture closing;
	closing.prepare();
	transition.phase = MainResultTransitionPhase::closing;
	transition.frame_before = 0U;
	closing.presenter->main_result_transition_state(transition);
	expect(closing.presenter->ready(), "closing frame zero initializes retained coordinates");
	expect_pixel(closing.raster, {16, 191}, 2U, "closing draws solid bottom triangle");

	Fixture malformed;
	malformed.prepare();
	transition.phase = MainResultTransitionPhase::opening;
	transition.frame_before = 16U;
	malformed.presenter->main_result_transition_state(transition);
	expect(
		malformed.presenter->status() == MainHudResultStatus::invalid_event_sequence,
		"transition frame beyond source domain is rejected"
	);
}

} // namespace

int main()
{
	test_constructor_and_support_helpers();
	test_hud_banner_and_particles();
	test_result_banner_summary_and_story_boundaries();
	test_score_combo_and_dynamic_hud();
	test_transition_boundaries();
	std::printf("checks=%zu failures=%zu\n", checks, failures);
	return (failures == 0U) ? 0 : 1;
}
