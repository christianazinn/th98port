#include "th02/core/stage_state.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
using namespace th02::core;
namespace {
void require(bool ok, std::source_location at = std::source_location::current())
{ if (!ok) { std::fprintf(stderr, "midboss4 assertion %u\n", at.line()); std::abort(); } }
struct Sink final : Midboss2Callbacks, LaserSpawnCallbacks {
	unsigned sounds{}, sprites{};
	Midboss2Sprite last{};
	void sound_effect(std::uint8_t) noexcept override { ++sounds; }
	void invalidate_midboss(Midboss2Invalidation) noexcept override {}
	void render_midboss(Midboss2Sprite s) noexcept override { ++sprites; last = s; }
};
}
int main()
{
	StageState s; Sink p;
	s.frame.page_back = 1; s.frame.page_front = 0;
	s.frame.player.current = {208, 336};
	s.midboss_scroll_step = 944;
	require(midboss4_update_and_render(s, p, p) == Midboss2UpdateResult::completed);
	require(s.midboss4.position_page == 0 && s.marisa.topleft == ScreenPoint{72, -14});
	require(s.boss.target == ScreenPoint{24, 24} && s.boss.phase_frame == 1);
	require(s.midboss4.pellet_top == 32 && s.marisa.intro_direction == 1);
	for (int i = 1; i < 32; ++i)
		require(midboss4_update_and_render(s, p, p) == Midboss2UpdateResult::completed);
	require(s.boss.phase_frame == 2 && s.marisa.intro_step == 1);
	require(s.marisa.intro_direction == -1 && s.random.cursor == 1);
	require(s.marisa.topleft == ScreenPoint{320, 48});
	s.boss.phase_frame = 113; s.midboss4.pattern = 255;
	require(midboss4_update_and_render(s, p, p) == Midboss2UpdateResult::completed);
	require(s.boss.phase_frame == 2 && s.midboss4.patterns_seen == 1 && s.random.cursor == 2);
	s.boss.phase_frame = 51; s.midboss4.patterns_seen = 12;
	s.midboss4.pattern = 255; s.boss.topleft_on_page[0].x = 383;
	require(midboss4_update_and_render(s, p, p) == Midboss2UpdateResult::completed);
	require(s.midboss4.active == 0 && s.midboss_scroll_step == 1632 && s.boss.phase_frame == 0);
	s.boss.phase_frame = 10; s.boss.phase = 1; s.midboss4.active = 1;
	s.midboss4.defeat_frame = 63;
	require(midboss4_update_and_render(s, p, p) == Midboss2UpdateResult::completed);
	require(s.boss.phase == 2 && s.boss.target == ScreenPoint{-1, -1} && s.midboss4.active == 0);
	s.frame.page_back = 1; s.frame.page_front = 0;
	s.boss.topleft_on_page[0] = {111, 222};
	require(midboss4_invalidate(s, p) == Midboss2InvalidateResult::inactive);
	require(s.midboss4.position_page == 1 && s.boss.topleft_on_page[1] == ScreenPoint{111, 222});
	for (const auto threshold : {944, 1632}) {
		StageState hit;
		hit.frame.page_back = 0; hit.frame.page_front = 1;
		hit.boss.phase_frame = 50; hit.boss.damage = 399;
		hit.marisa.intro_step = 1; hit.marisa.intro_direction = 1;
		hit.boss.topleft_on_page[0] = {100, 100}; hit.marisa.topleft = {100, 100};
		hit.midboss4.pattern = 255; hit.midboss_scroll_step = static_cast<std::int16_t>(threshold);
		auto &shot = hit.player_shots.shots[0];
		shot.flag = player_shot_alive; shot.patnum = 0x31;
		shot.pos_on_page[0] = {static_cast<std::int16_t>(120 * 16), static_cast<std::int16_t>(110 * 16)};
		require(midboss4_update_and_render(hit, p, p) == Midboss2UpdateResult::completed);
		require(hit.boss.phase == 1 && hit.score.score_delta >= 50000);
		require(hit.items.items[0].type == (threshold == 1632 ? ItemType::one_up : ItemType::bomb));
	}
}
