#include "th02/core/meira.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

namespace {
using namespace th02::core;
void require(bool value, std::source_location at = std::source_location::current()) {
	if (!value) { std::fprintf(stderr, "meira assertion failed: %u\n", at.line()); std::abort(); }
}
struct Sink final : MeiraCallbacks {
	std::vector<MeiraInvalidation> invalidations;
	std::vector<MeiraSprite> sprites;
	std::vector<std::uint8_t> sounds;
	void invalidate_meira(MeiraInvalidation value) noexcept override { invalidations.push_back(value); }
	void render_meira(MeiraSprite value) noexcept override { sprites.push_back(value); }
	void sound_effect(std::uint8_t value) noexcept override { sounds.push_back(value); }
	void explosion_margins_clear() noexcept override {}
	void explosion_egc_begin() noexcept override {}
	void explosion_egc_end() noexcept override {}
	void explosion_ring(ScreenPoint, std::int16_t, bool) noexcept override {}
	void explosion_color(std::uint8_t) noexcept override {}
	void explosion_grcg_off() noexcept override {}
	void explosion_invalidate(ScreenPoint) noexcept override {}
	void explosion_tiles_render() noexcept override {}
	void explosion_sprite(ScreenPoint, std::int16_t) noexcept override {}
	void explosion_palette_show() noexcept override {}
	void explosion_graph_scroll(std::uint16_t, std::uint16_t, std::uint16_t) noexcept override {}
};
struct Fixture {
	MeiraState meira{};
	BossState boss{};
	PlayerFrameState frame{};
	PlayerHitState player{};
	EnemyBulletPoolState bullets{};
	RandomState random{};
	PlayerShotPoolState shots{};
	ScoreState score{};
	SparkPoolState sparks{};
	EnemyBulletSpecialState special{};
	std::uint8_t slowdown{1};
	std::uint16_t tone{100};
	Sink sink{};
	Fixture() {
		frame.page_back = 1;
		frame.page_front = 0;
		frame.player.current = {100, 200};
		frame.player.on_page = {{{100, 200}, {100, 200}}};
		bullets.rank_base_stack = 1;
	}
	EnemyBulletAddResult update(std::int8_t rank = 1) {
		return meira_slashes_update_and_render(meira, bullets, random, frame, player, 0, rank, false, 300, sink);
	}
	MeiraUpdateResult combat(bool bombing = false) {
		MeiraCombatContext context{frame, player, shots, score, sparks, bullets, special,
			random, 0, 0, 1, bombing, 0, 0, 300, slowdown, tone};
		return meira_update(boss, meira, context, sink);
	}
};

void allocation_and_clamp()
{
	Fixture f;
	f.boss.topleft_on_page[1] = {20, 48};
	f.meira.burst_group = 32;
	f.meira.burst_speed = 64;
	f.meira.slash_trail_frames = 258;
	f.meira.slash_trail_patnum = 121;
	f.meira.slashes[0].flag = 2;
	require(meira_slash_step(f.boss, f.meira, 1, 0, -8, -8));
	require(f.boss.topleft_on_page[1] == ScreenPoint{12, 40}); // Only right edge checked.
	require(f.meira.slashes[1] == MeiraSlash{1, 32, 24, 52, 0, 0, 64, 121, 2});
	require(f.meira.slashes[0].flag == 2);
	for (auto &slash : f.meira.slashes) slash.flag = 1;
	const auto full = f.meira.slashes;
	f.boss.topleft_on_page[1] = {360, 48};
	require(meira_slash_step(f.boss, f.meira, 1, 1, -16, 8));
	require(f.boss.topleft_on_page[1] == ScreenPoint{376, 56}); // Only left edge checked.
	require(f.meira.slashes == full);
	require(!meira_slash_step(f.boss, f.meira, 2, 0, 8, 8));
}

void lifecycle_and_burst()
{
	Fixture f;
	f.meira.slash_trail_patnum = 120;
	meira_slashes_add(f.meira, {108, 208}, 64, enemy_bullet_group::one, 48, 1);
	require(f.update() == EnemyBulletAddResult::completed);
	require(f.player.raw_hit == 1 && f.meira.slashes[0].trail_frames == 0 && f.meira.slashes[0].age == 0);
	require(f.sink.sprites.back().kind == MeiraSpriteKind::slash_trail && f.sink.sprites.back().topleft == ScreenPoint{108, 108});
	f.player.raw_hit = 0;
	for (unsigned age = 0; age < 32; ++age) {
		require(f.update() == EnemyBulletAddResult::completed);
		require(f.sink.sprites.back().kind == MeiraSpriteKind::slash_bloom &&
			f.sink.sprites.back().patnum == 26 + static_cast<int>(age / 4));
	}
	require(f.player.raw_hit == 0 && f.sink.sounds == std::vector<std::uint8_t>{10});
	require(f.meira.slashes[0].left == 100 && f.meira.slashes[0].top == 200);
	require(f.update() == EnemyBulletAddResult::completed);
	require(f.meira.slashes[0].flag == 2 && f.meira.slash_burst_i == 1);
	require(f.bullets.bullets[0].flag == enemy_bullet_alive);
	require(f.bullets.bullets[0].screen_topleft_on_page[1] == ShotSubpixelPoint{1792, 3392});
	meira_slashes_invalidate(f.meira, f.sink);
	require(f.meira.slashes[0].flag == 0 && f.sink.invalidations.back().width == 32);
}

void collision_edges_and_easy_gate()
{
	for (int x = 83; x <= 101; ++x) for (int y = 183; y <= 201; ++y) {
		Fixture f;
		f.frame.player.current = {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)};
		meira_slashes_add(f.meira, {100, 200}, 0, 32, 48, 1);
		require(f.update() == EnemyBulletAddResult::completed);
		require((f.player.raw_hit == 1) == (x >= 84 && x <= 100 && y > 184 && y < 200));
	}
	Fixture f;
	f.meira.slash_burst_i = 255;
	for (unsigned i = 0; i < 2; ++i) {
		meira_slashes_add(f.meira, {100, 100}, 0, 32, 48, 0);
		f.meira.slashes[i].age = 32;
	}
	require(f.update(0) == EnemyBulletAddResult::completed && f.meira.slash_burst_i == 1);
	require(std::count_if(f.bullets.bullets.begin(), f.bullets.bullets.end(),
		[](const auto &b) { return b.flag == enemy_bullet_alive; }) == 1);
}

void page_history_and_refusal()
{
	Fixture f;
	f.boss.topleft_on_page = {{{200, 70}, {100, 50}}};
	f.meira.afterimages_active = 1;
	f.meira.afterimage_left[1] = {100, 90, 80};
	f.meira.afterimage_top[1] = {40, 50, 30};
	f.meira.afterimage_left[0] = {10, 20, 30};
	f.meira.afterimage_top[0] = {15, 25, 35};
	f.meira.slashes[0].flag = 2;
	require(meira_bg_render(f.boss, f.meira, f.frame, f.sink));
	require(f.sink.invalidations.size() == 3 && f.sink.invalidations[1].topleft == ScreenPoint{80, 30});
	require(f.meira.afterimage_left[1] == f.meira.afterimage_left[0] && f.meira.slashes[0].flag == 0);
	require(f.boss.topleft_on_page[1] == ScreenPoint{200, 70});
	f.frame.page_front = 2;
	f.sink.invalidations.clear();
	require(!meira_bg_render(f.boss, f.meira, f.frame, f.sink));
	require(f.sink.invalidations.size() == 2); // Boss and first trail before bad source page.
	f.frame.page_back = 2;
	f.meira.slashes[0] = {1, 32, 100, 100, 0, 32, 48, 0, 0};
	const auto slash = f.meira.slashes[0];
	require(f.update() == EnemyBulletAddResult::invalid_page);
	require(f.meira.slashes[0] == slash && f.meira.slash_burst_i == 0);
}
void dash_patterns()
{
	Fixture f;
	f.boss.topleft_on_page[1] = {192, 48};
	for (std::int16_t clock = 1; clock <= 91; ++clock) {
		f.boss.phase_frame = clock;
		require(meira_dash_pattern_update(f.boss, f.meira, f.frame, MeiraDashPattern::fall, f.sink));
	}
	require(f.boss.topleft_on_page[1] == ScreenPoint{192, 288} && f.boss.phase_frame == 0);
	require(f.sink.sounds == std::vector<std::uint8_t>({9, 3}));
	f.frame.player.current.x = 300;
	f.boss.phase_frame = 20;
	require(meira_dash_pattern_update(f.boss, f.meira, f.frame, MeiraDashPattern::rising_slashes, f.sink));
	require(f.meira.player_is_right == 1 && f.meira.burst_speed == 64 && f.boss.patnum == 145);
	f.boss.topleft_on_page[1] = {192, 56};
	f.boss.phase_frame = 57;
	require(meira_dash_pattern_update(f.boss, f.meira, f.frame, MeiraDashPattern::rising_slashes, f.sink));
	require(f.boss.topleft_on_page[1].y == 48 && f.boss.phase_frame == 56);
	f.boss.phase_frame = 57;
	require(meira_dash_pattern_update(f.boss, f.meira, f.frame, MeiraDashPattern::rising_slashes, f.sink));
	require(f.boss.topleft_on_page[1].y == 40 && f.boss.phase_frame == 57);
	f.boss.phase_frame = 99;
	require(meira_dash_pattern_update(f.boss, f.meira, f.frame, MeiraDashPattern::double_slash, f.sink));
	require(f.boss.patnum == 148 && f.meira.slash_trail_frames == 80); // Preserve wrong-way sprite.
	f.boss.phase_frame = 120;
	require(meira_dash_pattern_update(f.boss, f.meira, f.frame, MeiraDashPattern::double_slash, f.sink));
	require(f.meira.slash_trail_frames == 79 && f.boss.patnum == 145); // Recovery still decrements.
	Fixture four;
	four.boss.topleft_on_page[1] = {192, 48};
	four.frame.player.current.x = 300;
	for (std::int16_t clock = 1; clock <= 224; ++clock) {
		four.boss.phase_frame = clock;
		require(meira_dash_pattern_update(four.boss, four.meira, four.frame, MeiraDashPattern::four_slashes, four.sink));
		require(four.update() == EnemyBulletAddResult::completed);
		meira_slashes_invalidate(four.meira, four.sink);
	}
	require(four.boss.phase_frame == 0 && four.boss.patnum == 141);
	require(four.boss.topleft_on_page[1] == ScreenPoint{192, 48});
	require(four.meira.burst_speed == 48 && four.meira.slash_trail_frames == 52);
	require(std::count(four.sink.sounds.begin(), four.sink.sounds.end(), 3) == 4);
	require(std::count(four.sink.sounds.begin(), four.sink.sounds.end(), 9) == 1);
}
void combat_phase_transitions()
{
	Fixture f;
	f.boss.topleft_on_page[1] = {192, 48};
	f.boss.phase_frame = 90;
	f.boss.damage = 701;
	require(f.combat() == MeiraUpdateResult::boss);
	require(f.meira.phase == 0 && f.meira.pattern == 1); // Cannot leave after pattern zero.
	f.boss.phase_frame = 100;
	f.boss.topleft_on_page[1] = {32, 48};
	require(f.combat() == MeiraUpdateResult::boss);
	require(f.meira.phase == 1 && f.meira.pattern == 0 && f.boss.phase_frame == 0);
	f.boss.damage = 1500;
	f.boss.phase_frame = 119;
	require(f.combat() == MeiraUpdateResult::boss && f.meira.phase == 1 && f.meira.pattern == 1);
	f.boss.damage = 1501;
	f.boss.phase_frame = 223;
	require(f.combat() == MeiraUpdateResult::boss && f.meira.phase == 2 && f.meira.pattern == 0);
	f.boss.phase_frame = 49;
	require(f.combat() == MeiraUpdateResult::boss && f.special.turns_or_drift_frames == 5);
	f.boss.phase_frame = 98;
	require(f.combat() == MeiraUpdateResult::boss && f.meira.afterimages_active == 1 && f.meira.pattern == 1);
	require(f.random.cursor == 3 && f.boss.phase_frame == 0);
	require(std::count_if(f.bullets.bullets.begin(), f.bullets.bullets.end(),
		[](const auto &bullet) { return bullet.flag == enemy_bullet_alive; }) == 3);
}

void teleport_and_streams()
{
	Fixture f;
	f.boss.topleft_on_page[1] = {192, 48};
	f.meira.phase = 2;
	f.meira.pattern = 1;
	f.random.ring[0] = 64;
	f.random.ring[1] = 0;
	f.boss.phase_frame = 49;
	require(f.combat() == MeiraUpdateResult::boss);
	require(f.meira.dash_target == ScreenPoint{96, 16} && f.random.cursor == 2);
	require(f.boss.target == ScreenPoint{232, 88});
	for (unsigned i = 0; i < 64; ++i) require(f.combat() == MeiraUpdateResult::boss);
	require(f.boss.topleft_on_page[1] == f.meira.dash_target && f.meira.dash_step == 65);
	require(f.sink.sounds.empty()); // Unreachable landing sound is not restored.
	f.boss.phase_frame = 163;
	require(f.combat() == MeiraUpdateResult::boss && f.meira.pattern == 1 && f.boss.phase_frame == 0 && f.random.cursor == 4);
	Fixture stream;
	stream.boss.topleft_on_page[1] = {192, 48};
	stream.boss.rank_param[1] = enemy_bullet_group::one;
	stream.meira.pattern = 3;
	stream.boss.phase_frame = 98;
	for (unsigned i = 0; i < 18; ++i) require(stream.combat() == MeiraUpdateResult::boss);
	require(stream.meira.down_stream_speed == 120 && stream.boss.phase_frame == 0);
	require(std::count_if(stream.bullets.bullets.begin(), stream.bullets.bullets.end(),
		[](const auto &bullet) { return bullet.flag == enemy_bullet_alive; }) == 8);
	Fixture triple;
	triple.boss.topleft_on_page[1] = {192, 48};
	triple.boss.rank_param[2] = enemy_bullet_group::one;
	triple.meira.phase = 1; triple.meira.pattern = 2;
	triple.boss.phase_frame = 98;
	for (unsigned i = 0; i < 19; ++i) require(triple.combat() == MeiraUpdateResult::boss);
	require(std::count(triple.sink.sounds.begin(), triple.sink.sounds.end(), 3) == 3);
	require(triple.boss.phase_frame == 0 && triple.meira.pattern == 0);
}

void lethal_hit_defeat_and_setup()
{
	Fixture f;
	f.boss.topleft_on_page[1] = {192, 48};
	f.boss.damage = 2399;
	f.meira.phase = 2;
	f.meira.pattern = 1;
	require(f.combat(true) == MeiraUpdateResult::boss);
	require(f.boss.phase == 1 && f.boss.damage == 2400 && f.player.invincibility_time == 200);
	require(f.score.score_delta == 30001 && f.sink.sprites.back().kind == MeiraSpriteKind::hit_white);
	f.meira.defeat_frame = 95;
	f.meira.slashes[0] = {1, 32, 100, 100, 0, 32, 48, 0, 0};
	require(f.combat() == MeiraUpdateResult::stage_clear && f.meira.defeat_frame == 0);
	require(f.meira.slashes[0].flag == 1 && f.meira.slash_burst_i == 0); // Clear returns before slash pass.
	f.meira.defeat_frame = 17;
	f.boss.phase_frame = 123;
	f.boss.rank_param[3] = 77;
	f.meira.slash_burst_i = 9;
	meira_combat_setup_after_dialogue(f.boss, f.meira, f.special, 0);
	require(f.boss.phase_frame == 123 && f.meira.defeat_frame == 17 && f.meira.slash_burst_i == 9);
	require(f.boss.rank_param[1] == enemy_bullet_group::spread_3_wide && f.boss.rank_param[3] == 77);
	require(f.special.turns_or_drift_frames == 2 && f.meira.slashes[0].flag == 1);
	f.frame.page_back = 2;
	require(f.combat() == MeiraUpdateResult::invalid_page && f.boss.phase_frame == 124);
}
} // namespace
int main() {
	allocation_and_clamp(); lifecycle_and_burst(); collision_edges_and_easy_gate();
	page_history_and_refusal(); dash_patterns(); combat_phase_transitions();
	teleport_and_streams(); lethal_hit_defeat_and_setup();
}
