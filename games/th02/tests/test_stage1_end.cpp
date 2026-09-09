#include "th02/core/stage1_end.hpp"
#include "th02/core/stage1_state.hpp"
#include "th02/core/stage1_frame.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

namespace {
using namespace th02::core;
void require(bool condition, std::source_location at = std::source_location::current()) {
	if (!condition) { std::fprintf(stderr, "end assertion failed: %u\n", at.line()); std::abort(); }
}
struct Sink final : Stage1EndPresentation, Stage1FramePresentation {
	StageState &state;
	explicit Sink(StageState &s) : state(s) {}
	std::vector<int> events;
	std::vector<std::int16_t> faces;
	std::vector<std::array<std::uint8_t, 3>> cells;
	std::vector<std::size_t> pages;
	bool first_face{};
	void overlay_wipe() noexcept override {}
	void score_events(const ScoreEvents &) noexcept override { events.push_back(2); }
	void graph_scrollup(std::int16_t) noexcept override {}
	void palette_show() noexcept override {}
	void page_access(std::size_t page) noexcept override { pages.push_back(page); }
	void egc_begin() noexcept override {}
	void box_area_invalidate() noexcept override {}
	void tiles_render(const StageState &) noexcept override {}
	void egc_end() noexcept override {}
	void player_put(const StageState &) noexcept override {}
	void box_put(std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void box_wipe() noexcept override { first_face = true; }
	void face_put(std::int16_t face) noexcept override { if (first_face) { faces.push_back(face); first_face = false; } }
	void text_put(std::uint8_t, std::span<const std::uint8_t>) noexcept override {}
	void bonus_title() noexcept override { require(state.bomb_animation.palette_tone == 62); events.push_back(3); }
	void bonus_row(std::uint8_t row, std::int16_t value) noexcept override {
		constexpr std::array<std::int16_t, 6> expected{2000, 3200, 2500, 3000, 800, 2000};
		require(value == expected[row]); events.push_back(10 + row);
	}
	void bonus_total(const StageClearBonusTally &tally, std::int16_t points) noexcept override {
		require(points == 2 && tally.bonus_delta == 2700 && state.score.score_delta == 10);
		events.push_back(4);
	}
	void sound_effects_reset() noexcept override {
		require(state.score.score_delta == 2710 && state.score.score == 10); events.push_back(5);
	}
	void sound_effects_update() noexcept override { require(state.score.score == 2720); events.push_back(6); }
	void leave_cell(std::uint8_t x, std::uint8_t y, std::uint8_t cel) noexcept override {
		require(state.stage_id == 0); cells.push_back({x, y, cel});
	}
	void copy_line(const TileLineCopy &) noexcept override {}
	void graph_scroll(ScrollPresentation) noexcept override {}
	void wait_for_presentation(std::uint8_t) noexcept override {}
	void page_show(std::size_t) noexcept override {}
};

void complete_end(StageEndBoss kind, std::uint16_t continues = 0)
{
	const bool meira = kind == StageEndBoss::meira;
	const bool stones = kind == StageEndBoss::stones;
	const bool marisa = kind == StageEndBoss::marisa;
	StageState state{};
	Stage1EndState end{};
	Sink sink{state};
	state.dialog.box_current = marisa ? 35 : stones ? 8 : meira ? 24 : 16;
	state.continues_used = continues;
	state.stage_frame = 901;
	state.resident_frame = 999;
	state.random.seed = 52;
	const auto random_before = state.random;
	state.frame.page_front = 1;
	state.frame.page_back = 0;
	state.tile_mode = 2;
	state.score.score_delta = 10;
	state.rank = 1;
	state.player_miss.start_bombs = 3;
	state.start_lives = 2;
	state.item_lifecycle.point_items_collected = 2;
	state.spark_parameters.acceleration_x = 5;
	state.stage_bg_flash.frame = 20;
	state.stage_progression = StageProgression::clear;
	auto wait = marisa ? stage4_end_begin(state, end, sink) : stones ? stage3_end_begin(state, end, sink) : meira ? stage2_end_begin(state, end, sink) : stage1_end_begin(state, end, sink);
	unsigned delays = 0, samples = 0, calls = 0;
	while (wait != Stage1EndWait::complete) {
		require(++calls < 2000);
		switch (wait) {
		case Stage1EndWait::delay_one_frame:
			++delays; wait = stage1_end_after_delay(state, end, sink); break;
		case Stage1EndWait::delay_two_frames:
			delays += 2; wait = stage1_end_after_delay(state, end, sink); break;
		case Stage1EndWait::input_sample:
			++samples; wait = stage1_end_input(state, end, 0x10, sink); break;
		case Stage1EndWait::acknowledgement_first_sample:
		case Stage1EndWait::acknowledgement_second_sample:
			wait = stage1_end_acknowledgement(state, end,
				end.phase == Stage1EndPhase::boxes ? end.box.acknowledgement_released : end.released, sink);
			break;
		default: require(false);
		}
	}
	require(samples == (marisa ? (continues != 0 ? 456U : 342U) : stones ? 114U : meira ? 418U : 304U) && delays == (marisa ? (continues != 0 ? 117U : 105U) : stones ? 81U : meira ? 117U : 105U));
	require(state.dialog.box_current == (marisa ? (continues != 0 ? 47 : 52) : stones ? 11 : meira ? 35 : 24) && state.input == 1);
	if (marisa && continues != 0) {
		const auto offset = 42U * 80U;
		require(state.dialog.text[offset + 8] == 0x82 && state.dialog.text[offset + 9] == 0x50);
		require(state.dialog.text[offset + 10] == 0x82 && state.dialog.text[offset + 11] == 0x51);
	}
	require(sink.faces == (marisa ? (continues != 0 ? std::vector<std::int16_t>({147, 0, 96, 0, 96, 54, 144, 144, 12, 96, 57, 144}) : std::vector<std::int16_t>({147, 0, 96, 0, 96, 96, 57, 96, 57})) : stones ? std::vector<std::int16_t>({12, 96, 12}) : meira ? std::vector<std::int16_t>({9, 54, 57, 9, 96, 150, 57, 96, 54, 96, 12}) :
		std::vector<std::int16_t>({99, 99, 57, 96, 3, 99, 9, 96})));
	require(sink.events == std::vector<int>({2, 3, 10, 11, 12, 13, 14, 15, 4, 5, 2, 6}));
	require(sink.pages == std::vector<std::size_t>({1})); // No dialog_post.
	require(state.tile_mode == 0 && state.dialog.restore_tile_mode_none_at_post);
	require(state.stage_id == 1 && state.spark_parameters.acceleration_x == (stones || marisa ? 5 : 0) && state.stage_bg_flash.frame == (meira || stones || marisa ? 20 : 0));
	require(state.score.score == 2720 && state.score.score_delta == 2710);
	require(state.stage_frame == 901 && state.resident_frame == 999 && state.random == random_before);
	require(sink.cells.size() == 21U * 24U * 23U);
	for (std::size_t i = 0; i < sink.cells.size(); ++i) {
		const auto frame = i / (24U * 23U);
		const auto column = (i / 23U) % 24U;
		const auto row = i % 23U;
		const auto cel = frame < column / 2U ? 0U :
			(frame - column / 2U > 8U ? 8U : frame - column / 2U);
		require(sink.cells[i] == std::array<std::uint8_t, 3>{
			static_cast<std::uint8_t>(4U + column * 2U), static_cast<std::uint8_t>(row + 1U), static_cast<std::uint8_t>(cel)});
	}
	Stage1FrameSchedule schedule{};
	schedule.phase = Stage1FramePhase::stage_end_pending;
	require(stage1_frame_after_stage_end(state, schedule, sink) == Stage1FramePhase::cleared);
	require(state.score.score_delta == 2710 && state.stage_frame == 901);
	require(stage1_end_after_delay(state, end, sink) == Stage1EndWait::complete && state.stage_id == 1);
}

void acknowledgement_requires_new_release()
{
	StageState state{};
	Stage1EndState end{};
	Sink sink{state};
	end.phase = Stage1EndPhase::bonus_acknowledgement;
	state.input = 1;
	const auto pair = [&](bool first, bool second) {
		require(stage1_end_acknowledgement(state, end, first, sink) == Stage1EndWait::delay_two_frames);
		require(stage1_end_after_delay(state, end, sink) == Stage1EndWait::acknowledgement_second_sample);
		return stage1_end_acknowledgement(state, end, second, sink);
	};
	require(pair(true, false) == Stage1EndWait::acknowledgement_first_sample && !end.released);
	(void)pair(false, false);
	(void)pair(false, true);
	require(end.phase == Stage1EndPhase::end_acknowledgement && !end.released);
	(void)pair(true, true);
	require(end.phase == Stage1EndPhase::end_acknowledgement && !end.released);
	(void)pair(false, false);
	require(pair(true, false) == Stage1EndWait::delay_two_frames);
	require(end.phase == Stage1EndPhase::leave_overlay && state.input == 1 && state.dialog.box_current == 0);
}
} // namespace
int main() { complete_end(StageEndBoss::rika); complete_end(StageEndBoss::meira); complete_end(StageEndBoss::stones); complete_end(StageEndBoss::marisa); complete_end(StageEndBoss::marisa, 12); acknowledgement_requires_new_release(); }
