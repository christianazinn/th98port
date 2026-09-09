#include "th02/core/stage5_end.hpp"
#include "th02/core/stage_state.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>
using namespace th02::core;
namespace {
void require(bool value, std::source_location at = std::source_location::current())
{ if (!value) { std::fprintf(stderr, "stage4 intro: %u\n", at.line()); std::abort(); } }
struct Sink final : Stage5EndPresentation, EnemyBulletInvalidationCallbacks {
	std::vector<int> faces, rings, erased, music;
	bool first{};
	unsigned updates{}, forced{}, second_form{};
	void music_fade(std::uint8_t speed) noexcept override { require(speed == 10); }
	void sound_effect_force(std::uint8_t sound) noexcept override { require(sound == 5); ++forced; }
	void load_second_form() noexcept override { ++second_form; }
	void invalidate_bullet(EnemyBulletInvalidation) noexcept override {}
	void scene(BossIntroScene scene, std::int16_t value, const StageState &) noexcept override {
		if (scene == BossIntroScene::ring_put) rings.push_back(value);
		if (scene == BossIntroScene::ring_erase) erased.push_back(value);
	}
	void sound_effect(std::uint8_t) noexcept override {}
	void sound_effects_update() noexcept override { ++updates; }
	void music_stop() noexcept override { music.push_back(1); }
	void music_load(std::string_view name) noexcept override { require(name == "mima.m"); music.push_back(2); }
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
 for (const bool continued : {false, true}) {
  StageState s; s.stage_id = 4; s.continues_used = continued ? 1 : 0;
  s.frame.player.current = {200, 300}; s.stage_frame = 999; s.resident_frame = 1234;
  s.mima.star_angle = 77; const auto rng = s.random;
  Sink p; BossIntroState i;
  auto wait = boss_intro_begin(s, i, p, BossIntroKind::mima);
  unsigned calls = 0, samples = 0, delays = 0;
  while (wait != Stage1IntroWait::complete) {
   require(++calls < 4000);
   switch (wait) {
   case Stage1IntroWait::input_sample: ++samples; wait = boss_intro_input(s, i, 16, p); break;
   case Stage1IntroWait::delay_one_frame: ++delays; wait = boss_intro_after_delay(s, i, p); break;
   case Stage1IntroWait::delay_two_frames: delays += 2; wait = boss_intro_after_delay(s, i, p); break;
   case Stage1IntroWait::acknowledgement_first_sample:
   case Stage1IntroWait::acknowledgement_second_sample: wait = boss_intro_acknowledgement(s, i, i.box.acknowledgement_released, p); break;
   default: require(false);
   }
  }
  const unsigned boxes = continued ? 27 : 22;
  // 1 reset +23 pre slide +10 lead +150 rings +30 portrait +18/52 fades
  // +24 post slide; each box requires four release/press delay frames.
  require(samples == boxes * 38 && delays == 308 + boxes * 4);
  require(s.dialog.box_current == 44 && p.faces.size() == boxes);
  require(s.mima.star_angle == 77 && s.mima.phase == 0);
  require(s.boss.topleft_on_page[0] == ScreenPoint{144, 64});
  require(s.stage_frame == 999 && s.resident_frame == 1234 && s.random == rng);
  require(s.tile_mode == 2 && s.bomb_animation.palette_tone == 100);
  Stage5EndState e;
  wait = stage5_end_begin(s, e, true, p, p);
  calls = 0;
  while (wait != Stage1IntroWait::complete) {
   require(++calls < 2000);
   switch (wait) {
   case Stage1IntroWait::input_sample: wait = stage5_end_input(s, e, 16, p); break;
   case Stage1IntroWait::delay_one_frame:
   case Stage1IntroWait::delay_two_frames: wait = stage5_end_after_delay(s, e, p); break;
   case Stage1IntroWait::acknowledgement_first_sample:
   case Stage1IntroWait::acknowledgement_second_sample: wait = stage5_end_acknowledgement(s, e, e.box.acknowledgement_released, p); break;
   default: require(false);
   }
  }
  require(s.dialog.box_current == (continued ? 49 : 53));
  require(s.stage_frame == 999 && s.resident_frame == 1234 && s.random == rng);
  if (continued) {
   require(e.phase == Stage5EndPhase::maine && e.resident.stage == 127);
   require(e.resident.score_with_continues == 500001 && !e.resident.cleared);
   require(e.resident.skill == 57 && p.second_form == 0);
   continue;
  }
  require(e.phase == Stage5EndPhase::form2_ready && p.second_form == 1);
  s.score.score = 12345; s.item_lifecycle.item_skill = 30;
  s.player_miss.total_miss_count = 2; s.bomb.total_bombs_used = 3; s.rank = 2;
  wait = stage5_end_begin(s, e, false, p, p); calls = 0; delays = 0;
  while (wait != Stage1IntroWait::complete) {
   require(++calls < 2000);
   switch (wait) {
   case Stage1IntroWait::input_sample: wait = stage5_end_input(s, e, 16, p); break;
   case Stage1IntroWait::delay_one_frame: ++delays; wait = stage5_end_after_delay(s, e, p); break;
   case Stage1IntroWait::delay_two_frames: delays += 2; wait = stage5_end_after_delay(s, e, p); break;
   case Stage1IntroWait::acknowledgement_first_sample:
   case Stage1IntroWait::acknowledgement_second_sample: wait = stage5_end_acknowledgement(s, e, e.box.acknowledgement_released, p); break;
   default: require(false);
   }
  }
  require(e.phase == Stage5EndPhase::maine && e.resident.cleared && e.resident.stage == 127);
  require(e.resident.score_with_continues == 1123450 && e.resident.score_highest == 112345);
  require(e.resident.skill == 116 && s.item_lifecycle.item_skill == 25);
  require(p.forced == 5 && s.dialog.box_current == 57);
  require(delays == 4 * 4 + 5 * 2 + 120 + 52);
  require(s.stage_frame == 999 && s.resident_frame == 1234 && s.random == rng);
 }
}
