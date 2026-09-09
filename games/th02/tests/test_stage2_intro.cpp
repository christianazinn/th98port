#include "th02/core/stage2_intro.hpp"
#include "th02/core/stage_state.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

using namespace th02::core;
namespace {
void require(bool value, std::source_location at = std::source_location::current())
{
	if (!value) { std::fprintf(stderr, "stage2 intro: %u\n", at.line()); std::abort(); }
}
struct Sink final : Stage2IntroPresentation {
	StageState &state;
	std::vector<int> faces;
	std::vector<int> music;
	bool first{};
	unsigned sprites{};
	explicit Sink(StageState &s) : state(s) {}
	void music_stop() noexcept override { require(sprites == 1); music.push_back(1); }
	void music_load(std::string_view name) noexcept override { require(name == "boss4.m"); music.push_back(2); }
	void music_play() noexcept override { music.push_back(3); }
	void boss_put(std::int16_t x, std::int16_t y, std::int16_t pat) noexcept override {
		require(state.dialog.box_current == 2 && state.bomb_animation.palette_tone == 200);
		require(x == 192 && y == 18 && pat == 141);
		++sprites;
	}
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
	void box_wipe() noexcept override { first = true; }
	void face_put(std::int16_t face) noexcept override { if (first) { faces.push_back(face); first = false; } }
	void text_put(std::uint8_t, std::span<const std::uint8_t>) noexcept override {}
};
}
int main()
{
	StageState state{};
	state.frame.page_back = 0;
	state.frame.page_front = 1;
	state.scroll.line = 370;
	state.stage_frame = 917;
	state.resident_frame = 37;
	state.boss.phase_frame = 123;
	state.meira.defeat_frame = 19;
	state.score.score_delta = 10;
	const auto random = state.random;
	Sink sink(state);
	Stage2IntroState intro{};
	auto wait = stage2_intro_begin(state, intro, sink);
	unsigned samples = 0, delays = 0, calls = 0;
	while (wait != Stage1IntroWait::complete) {
		require(++calls < 3000);
		switch (wait) {
		case Stage1IntroWait::delay_one_frame:
			++delays; wait = stage2_intro_after_delay(state, intro, sink); break;
		case Stage1IntroWait::delay_two_frames:
			delays += 2; wait = stage2_intro_after_delay(state, intro, sink); break;
		case Stage1IntroWait::input_sample:
			++samples; wait = stage2_intro_input(state, intro, 16, sink); break;
		case Stage1IntroWait::acknowledgement_first_sample:
		case Stage1IntroWait::acknowledgement_second_sample:
			wait = stage2_intro_acknowledgement(state, intro, intro.box.acknowledgement_released, sink); break;
		default: require(false);
		}
	}
	require(samples == 912 && delays == 179);
	require(sink.faces == std::vector<int>({0, 96, 102, 102, 57, 60, 102, 102, 60, 9, 150, 54,
		102, 102, 96, 54, 102, 60, 9, 102, 9, 150, 57, 150}));
	require(sink.music == std::vector<int>({1, 2, 3}) && sink.sprites == 1);
	require(state.dialog.box_current == 24 && state.random == random);
	require(state.stage_frame == 917 && state.resident_frame == 37);
	require(state.boss.phase_frame == 123 && state.meira.defeat_frame == 19);
	require(state.bomb_animation.palette_tone == 100 && state.score.score == 10);
	require(stage2_intro_after_delay(state, intro, sink) == Stage1IntroWait::complete);
	std::puts("Meira intro source sequence passed");
}
