#include "th02/core/stage1_intro.hpp"
#include "th02/core/stage1_frame.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

namespace {
using namespace th02::core;
void require(bool condition, std::source_location at = std::source_location::current()) {
	if (!condition) { std::fprintf(stderr, "intro assertion failed: %u\n", at.line()); std::abort(); }
}
struct Sink final : Stage1IntroPresentation, Stage1FramePresentation {
	std::vector<int> music;
	std::vector<std::int16_t> faces;
	bool first_face{};
	void music_stop() noexcept override { music.push_back(1); }
	void music_load(std::string_view filename) noexcept override { require(filename == "boss1.m"); music.push_back(2); }
	void music_play() noexcept override { music.push_back(3); }
	void overlay_wipe() noexcept override {}
	void score_events(const ScoreEvents &) noexcept override {}
	void graph_scrollup(std::int16_t) noexcept override {}
	void palette_show() noexcept override {}
	void page_access(std::size_t) noexcept override {}
	void egc_begin() noexcept override {}
	void box_area_invalidate() noexcept override {}
	void tiles_render(const StageState &) noexcept override {}
	void egc_end() noexcept override {}
	void player_put(const StageState &) noexcept override {}
	void box_put(std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void box_wipe() noexcept override { first_face = true; }
	void face_put(std::int16_t face) noexcept override { if (first_face) { faces.push_back(face); first_face = false; } }
	void text_put(std::uint8_t, std::span<const std::uint8_t>) noexcept override {}
	void copy_line(const TileLineCopy &) noexcept override {}
	void graph_scroll(ScrollPresentation) noexcept override {}
	void wait_for_presentation(std::uint8_t) noexcept override {}
	void page_show(std::size_t) noexcept override {}
};

void complete_intro()
{
	StageState state{};
	Stage1IntroState intro{};
	Sink sink{};
	state.stage_frame = 900;
	state.resident_frame = 31;
	state.random.seed = 42;
	state.random.cursor = 7;
	const auto random_before = state.random;
	state.score.score_delta = 10;
	state.enemies_enabled = true;
	state.enemies.enemies[0].flag = enemy_alive;
	state.frame.page_front = 1;
	state.frame.page_back = 0;
	state.scroll.done = true;
	state.scroll.line = 272;
	state.boss_bgm_title_id = 1;
	state.tile_mode = 1;
	state.stage_progression = StageProgression::boss;
	Stage1FrameSchedule schedule{};
	schedule.phase = Stage1FramePhase::boss_init_pending;
	auto wait = stage1_intro_begin(state, intro, sink);
	unsigned delays = 0, samples = 0, calls = 0;
	while (wait != Stage1IntroWait::complete) {
		require(++calls < 2000);
		switch (wait) {
		case Stage1IntroWait::delay_one_frame:
			++delays;
			wait = stage1_intro_after_delay(state, intro, sink);
			break;
		case Stage1IntroWait::delay_two_frames:
			delays += 2;
			wait = stage1_intro_after_delay(state, intro, sink);
			break;
		case Stage1IntroWait::input_sample:
			++samples;
			wait = stage1_intro_input(state, intro, 0x10, sink);
			break;
		case Stage1IntroWait::acknowledgement_first_sample:
		case Stage1IntroWait::acknowledgement_second_sample:
			wait = stage1_intro_acknowledgement(state, intro, intro.box.acknowledgement_released, sink);
			break;
		default: require(false);
		}
	}
	require(sink.music == std::vector<int>({1, 2, 3}));
	require(sink.faces == std::vector<std::int16_t>({0, 96, 51, 96, 9, 96, 0, 96, 99, 99, 48, 12, 99, 99, 6, 99}));
	require(samples == 608 && delays == 111 && state.dialog.box_current == 16);
	require(state.input == 0x10 && state.random == random_before && state.resident_frame == 31);
	require(state.stage_frame == 0 && state.lasers_enabled && state.boss.topleft_on_page[0].x == 192);
	require(state.rika.topleft.y == 320 && state.score.score == 10 && state.score.score_delta == 10);
	require(stage1_frame_after_boss_init(state, schedule, sink) == Stage1FramePhase::complete);
	// Normal score animation transfers max(delta, 32) / 32 on this frame.
	require(state.stage_frame == 1 && state.resident_frame == 32 && state.score.score == 11);
	require(state.score.score_delta == 9 && state.score.score_delta_transferred_prev == 1);
	require(!state.enemies_enabled && state.enemies.enemies[0].flag == enemy_remove && state.boss_enabled);
	require(stage1_intro_after_delay(state, intro, sink) == Stage1IntroWait::complete && state.stage_frame == 1);
}

void invalid_text_keeps_prefix()
{
	StageState state{};
	Stage1IntroState intro{};
	Sink sink{};
	state.dialog.box_current = 64;
	state.stage_frame = 99;
	state.score.score_delta = 10;
	auto wait = stage1_intro_begin(state, intro, sink);
	for (unsigned i = 0; i < 23; ++i) {
		require(wait == Stage1IntroWait::delay_one_frame);
		wait = stage1_intro_after_delay(state, intro, sink);
	}
	require(wait == Stage1IntroWait::input_sample);
	require(stage1_intro_input(state, intro, 1, sink) == Stage1IntroWait::refused);
	require(state.stage_frame == 99 && state.dialog.box_current == 64 && state.score.score == 10);
}
} // namespace

int main() { complete_intro(); invalid_text_keeps_prefix(); }
