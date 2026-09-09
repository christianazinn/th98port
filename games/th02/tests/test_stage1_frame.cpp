#include "th02/core/stage1_frame.hpp"
#include "th02/core/stage1_lifecycle.hpp"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <vector>

namespace {
using namespace th02::core;
void require(bool value, const std::source_location where = std::source_location::current()) {
	if (!value) { std::fprintf(stderr, "stage1 frame assertion failed: %u\n", where.line()); std::abort(); }
}
struct Sink final : Stage1FramePresentation {
	Stage1State &state;
	std::vector<int> events;
	explicit Sink(Stage1State &s) : state(s) {}
	void copy_line(const TileLineCopy &) noexcept override { events.push_back(1); }
	void egc_begin() noexcept override { events.push_back(2); }
	void egc_end() noexcept override { events.push_back(3); }
	void graph_scroll(ScrollPresentation) noexcept override { events.push_back(4); }
	void wait_for_presentation(std::uint8_t) noexcept override { events.push_back(5); }
	void page_show(std::size_t page) noexcept override {
		require(state.frame.page_front == page && state.frame.page_back == page);
		events.push_back(6);
	}
	void page_access(std::size_t page) noexcept override {
		require(state.frame.page_back == page && state.frame.page_front != page);
		events.push_back(7);
	}
	void score_events(const ScoreEvents &) noexcept override { events.push_back(8); }
};

void paused_tail()
{
	Stage1State state{};
	Stage1FrameSchedule schedule{};
	Sink sink{state};
	state.input = 0x40;
	state.stage_frame = 1499;
	state.player_miss.playperf = 4;
	state.playperf_max = 5;
	require(stage1_frame_finish(state, schedule, sink) == Stage1FramePhase::paused);
	require(sink.events.empty() && state.stage_frame == 1499 && state.scroll.cycle == 0);
	require(stage1_frame_finish(state, schedule, sink) == Stage1FramePhase::paused);
	require(stage1_frame_resume(state, schedule, false, sink) == Stage1FramePhase::complete);
	require(state.stage_frame == 1500 && state.resident_frame == 1 && state.player_miss.playperf == 5);
	require(state.scroll.cycle == 1 && state.frame.page_front == 1 && state.frame.page_back == 0);
	require(sink.events == std::vector<int>({5, 6, 7, 8}));
	require(stage1_frame_resume(state, schedule, false, sink) == Stage1FramePhase::complete);
	require(state.stage_frame == 1500 && sink.events.size() == 4);
	// The enclosing entity owner arms a fresh frame; held Cancel is source
	// input, not a host edge detector suppressed by the previous resume.
	schedule.phase = Stage1FramePhase::entities_complete;
	require(stage1_frame_finish(state, schedule, sink) == Stage1FramePhase::paused);
	require(stage1_frame_resume(state, schedule, true, sink) == Stage1FramePhase::quit);
	require(!state.quit && state.stage_frame == 1501);
}

void activation_boundary()
{
	Stage1State state{};
	Stage1FrameSchedule schedule{};
	Sink sink{state};
	state.scroll.done = true;
	state.stage_frame = 9000;
	state.enemies_enabled = true;
	state.boss_bgm_title_id = 3;
	require(stage1_frame_finish(state, schedule, sink) == Stage1FramePhase::boss_init_pending);
	require(state.stage_frame == 9000 && state.resident_frame == 0);
	require(state.frame.page_front == 1 && state.frame.page_back == 0);
	require(sink.events == std::vector<int>({5, 6, 7}));
	// Test the already-ported init suffix, not a fabricated dialogue body.
	rika_combat_setup_after_dialogue(state.boss, state.rika, state.stage_frame,
		state.lasers_enabled, state.scroll.line, state.rank);
	require(stage1_frame_after_boss_init(state, schedule, sink) == Stage1FramePhase::complete);
	require(state.stage_frame == 1 && state.resident_frame == 1);
	require(!state.enemies_enabled && state.boss_enabled && !schedule.boss_activation_enabled);
	require(schedule.stage_end_enabled && state.scroll.cycle == 255);
	require(state.bgm_show_timer == 1 && state.bgm_title_id == 3);
	require(stage1_frame_after_boss_init(state, schedule, sink) == Stage1FramePhase::complete);
	require(state.stage_frame == 1);
}

void end_boundary(bool quit)
{
	Stage1State state{};
	Stage1FrameSchedule schedule{};
	Sink sink{state};
	schedule.boss_activation_enabled = false;
	schedule.stage_end_enabled = true;
	state.stage_progression = StageProgression::clear;
	state.quit = quit;
	state.score.score_delta = 100;
	require(stage1_frame_finish(state, schedule, sink) == Stage1FramePhase::stage_end_pending);
	require(state.stage_frame == 1 && state.score.score == 0);
	require(sink.events == std::vector<int>({5, 6, 7}));
	// This calls only the continuation: the unported ending itself is not
	// asserted to have executed, and its effects are outside this unit test.
	require(stage1_frame_after_stage_end(state, schedule, sink) ==
		(quit ? Stage1FramePhase::quit : Stage1FramePhase::cleared));
	require(state.stage_frame == 1 && !state.quit);
	if (quit) require(state.score.score == 100 && state.score.score_delta == 0);
	else require(state.score.score == 0 && state.score.score_delta == 100);
}

void wrap_and_refusal()
{
	Stage1State state{};
	Stage1FrameSchedule schedule{};
	Sink sink{state};
	state.resident_frame = std::numeric_limits<std::int32_t>::max();
	state.stage_frame = std::numeric_limits<std::uint32_t>::max();
	require(stage1_frame_finish(state, schedule, sink) == Stage1FramePhase::complete);
	require(state.resident_frame == std::numeric_limits<std::int32_t>::min() && state.stage_frame == 0);
	schedule.phase = Stage1FramePhase::entities_complete;
	state.scroll.delta = 1;
	state.scroll.speed = 1;
	state.scroll.line = -1;
	state.stage_data.map_length = 4;
	sink.events.clear();
	require(stage1_frame_finish(state, schedule, sink) == Stage1FramePhase::faulted);
	require(state.stage_frame == 0 && sink.events == std::vector<int>({2}));
}

void final_scroll()
{
	Stage1State state{};
	Stage1FrameSchedule schedule{};
	Sink sink{state};
	state.scroll.line = 271;
	state.scroll.delta = 1;
	state.scroll.speed = 1;
	state.scroll.cycle = 128;
	state.tiles.map_full_row_at_top_of_screen = 31;
	state.stage_data.map_length = 4;
	require(stage1_frame_finish(state, schedule, sink) == Stage1FramePhase::boss_init_pending);
	require(state.scroll.done && state.scroll.line == 272 && state.scroll.cycle == 128);
	require(state.stage_frame == 0);
	// Map end corrects the line and skips GDC submission/cycle increment, but
	// still closes EGC and flips before suspending at the actual boss call.
	require(sink.events == std::vector<int>({2, 3, 5, 6, 7}));
}
} // namespace

int main() { paused_tail(); activation_boundary(); end_boundary(false); end_boundary(true); wrap_and_refusal(); final_scroll(); }
