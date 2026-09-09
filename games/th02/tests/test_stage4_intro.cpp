#include "th02/core/boss_intro.hpp"
#include "th02/core/stage_state.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>
using namespace th02::core;
namespace {
void require(bool value, std::source_location at = std::source_location::current())
{ if (!value) { std::fprintf(stderr, "stage4 intro: %u\n", at.line()); std::abort(); } }
struct Sink final : BossIntroPresentation {
	std::vector<int> faces, rings, erased, music;
	bool first{};
	unsigned updates{};
	std::string_view track{"boss3.m"};
	void scene(BossIntroScene scene, std::int16_t value, const StageState &) noexcept override {
		if (scene == BossIntroScene::ring_put) rings.push_back(value);
		if (scene == BossIntroScene::ring_erase) erased.push_back(value);
	}
	void sound_effect(std::uint8_t) noexcept override {}
	void sound_effects_update() noexcept override { ++updates; }
	void music_stop() noexcept override { music.push_back(1); }
	void music_load(std::string_view name) noexcept override { require(name == track); music.push_back(2); }
	void music_play() noexcept override { music.push_back(3); }
	void boss_put(std::int16_t, std::int16_t, std::int16_t) noexcept override {}
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
	void face_put(std::int16_t f) noexcept override { if (first) { faces.push_back(f); first = false; } }
	void text_put(std::uint8_t, std::span<const std::uint8_t>) noexcept override {}
};
}
int main()
{
	StageState s;
	s.frame.player.current = {200, 300}; s.scroll.line = 370; s.scroll.sad = 999;
	s.stage_frame = 123; s.resident_frame = 456; s.marisa.defeat_frame = 17;
	s.score.score_delta = 10;
	const auto random = s.random;
	Sink p; BossIntroState i;
	auto wait = boss_intro_begin(s, i, p);
	unsigned calls = 0, samples = 0, delays = 0;
	while (wait != Stage1IntroWait::complete) {
		require(++calls < 4000);
		switch (wait) {
		case Stage1IntroWait::delay_one_frame: ++delays; wait = boss_intro_after_delay(s, i, p); break;
		case Stage1IntroWait::delay_two_frames: delays += 2; wait = boss_intro_after_delay(s, i, p); break;
		case Stage1IntroWait::input_sample: ++samples; wait = boss_intro_input(s, i, 16, p); break;
		case Stage1IntroWait::acknowledgement_first_sample:
		case Stage1IntroWait::acknowledgement_second_sample:
			wait = boss_intro_acknowledgement(s, i, i.box.acknowledgement_released, p); break;
		default: require(false);
		}
	}
	// 1 reset, 23 slide, 10 lead-in, 150 ring, 30 portrait, 36 white-fade,
	// and two release/press pairs (4 frames) for each of 35 boxes.
	require(delays == 390 && samples == 35 * 38);
	require(p.rings.size() == 150 && p.rings == p.erased);
	for (std::size_t n = 0; n < p.rings.size(); ++n) require(p.rings[n] == 150 - static_cast<int>(n));
	require(p.updates == 151 && p.music == std::vector<int>({1, 2, 3}));
	require(s.dialog.box_current == 35 && s.stage_frame == 123 && s.resident_frame == 456 && s.random == random);
	require(s.frame.option_left_on_page[0] == ScreenPoint{184, 316} && s.frame.option_left_on_page[1] == ScreenPoint{184, 316});
	require(s.scroll.line == 0 && s.scroll.sad == 0 && s.bomb_animation.palette_tone == 100);
	require(s.marisa.defeat_frame == 17 && s.marisa.topleft == ScreenPoint{176, 64});
	require(s.boss.phase_frame == 0 && s.tile_mode == 2 && s.score.score == 10);

	s = {}; p = {}; p.track = "boss5.m";
	s.sigma.frame = 12345;
	s.stage_frame = 123; s.resident_frame = 456;
	wait = boss_intro_begin(s, i, p, BossIntroKind::sigma);
	calls = samples = delays = 0;
	while (wait != Stage1IntroWait::complete) {
		require(++calls < 4000);
		switch (wait) {
		case Stage1IntroWait::delay_one_frame: ++delays; wait = boss_intro_after_delay(s, i, p); break;
		case Stage1IntroWait::delay_two_frames: delays += 2; wait = boss_intro_after_delay(s, i, p); break;
		case Stage1IntroWait::input_sample: ++samples; wait = boss_intro_input(s, i, 16, p); break;
		case Stage1IntroWait::acknowledgement_first_sample:
		case Stage1IntroWait::acknowledgement_second_sample:
			wait = boss_intro_acknowledgement(s, i, i.box.acknowledgement_released, p); break;
		default: require(false);
		}
	}
	// Reset, pre/post slides, two 18-frame fades, and four ack frames per box.
	require(delays == 1 + 23 + 24 + 36 + 25 * 4 && samples == 25 * 38);
	require(p.rings.empty() && p.erased.empty());
	require(p.music == std::vector<int>({1, 2, 3}));
	require(s.dialog.box_current == 25 && s.sigma.frame == 12345);
	require(s.stage_frame == 123 && s.resident_frame == 456);
	require(s.sigma.topleft == ScreenPoint{160, 48} && s.tile_mode == 2);
}
