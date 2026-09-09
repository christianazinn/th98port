#include "th02/core/midboss3.hpp"
#include <cstdlib>
#include <cstdio>
#include <source_location>

using namespace th02::core;
namespace {
void require(bool v, std::source_location at = std::source_location::current()) {
	if (!v) { std::fprintf(stderr, "midboss3 assertion %u\n", at.line()); std::abort(); }
}
struct Sink final : Midboss2Callbacks {
	unsigned sprites{}, sounds{}, invalidations{};
	void invalidate_midboss(Midboss2Invalidation) noexcept override { ++invalidations; }
	void sound_effect(std::uint8_t) noexcept override { ++sounds; }
	void render_midboss(Midboss2Sprite) noexcept override { ++sprites; }
};
}
int main()
{
	BossState boss{};
	Midboss3State s{};
	std::array<std::uint8_t, 5> flags{};
	std::array<std::int16_t, 5> damage{}, patnum{};
	Midboss3SharedState shared{flags, damage, patnum};
	PlayerFrameState frame{};
	frame.page_front = 1;
	frame.page_back = 0;
	frame.player.current = {200, 300};
	PlayerShotPoolState shots{};
	ScoreState score{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	RandomState random{};
	Midboss3Context c{frame, shots, score, sparks, bullets, random};
	Sink p;
	require(midboss3_update_and_render(boss, s, shared, c, p) == Midboss2UpdateResult::completed);
	require(boss.phase_frame == 1 && boss.target == ScreenPoint{24, 16});
	require(s.topleft_on_page[0][0] == ScreenPoint{32, -16} && s.topleft_on_page[1][1] == ScreenPoint{352, -16});
	require(p.sprites == 0);
	c.scroll_delta = 1;
	require(midboss3_update_and_render(boss, s, shared, c, p) == Midboss2UpdateResult::completed);
	require(s.topleft_on_page[0][0].y == -15 && p.sprites == 2);
	boss.phase_frame = 127;
	require(midboss3_update_and_render(boss, s, shared, c, p) == Midboss2UpdateResult::completed);
	// Each copy re-seeds BOTH angles at128; copy1 overwrites copy0's increment.
	require(s.loops == 1 && s.angle[0] == 0 && s.angle[1] == 120);
	boss.phase_frame = 575;
	require(midboss3_update_and_render(boss, s, shared, c, p) == Midboss2UpdateResult::completed);
	require(boss.phase_frame == 127);
	s.loops = 3;
	require(midboss3_update_and_render(boss, s, shared, c, p) == Midboss2UpdateResult::completed);
	require(s.loops == 4 && flags[0] == 1 && flags[1] == 1);
	for (unsigned i = 0; i < 48; ++i)
		require(midboss3_update_and_render(boss, s, shared, c, p) == Midboss2UpdateResult::completed);
	require(flags[0] == 2 && flags[1] == 2 && boss.target == ScreenPoint{-1, -1});
	require(midboss3_invalidate(s, shared, frame, p) == Midboss2InvalidateResult::inactive);
	require(p.invalidations == 0 && score.score_delta == 0);
}
