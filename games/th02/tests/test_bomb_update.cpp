#include "th02/core/bomb_update.hpp"
#include "th02/core/random.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
using namespace th02::core;

void require(bool condition) {
	if (!condition) { std::fputs("bomb update assertion failed\n", stderr); std::abort(); }
}

struct Sink final : BombAnimationCallbacks, BombPresentationCallbacks {
	unsigned circles{};
	void grcg_set_color(std::uint8_t) noexcept override {}
	void circle_point(ScreenPoint) noexcept override { ++circles; }
	void particle(ScreenPoint, std::int16_t) noexcept override {}
	void grcg_off() noexcept override {}
	void invalidate_bullet(EnemyBulletInvalidation) noexcept override {}
	BombRgb palette_get(std::uint8_t) noexcept override { return {}; }
	void palette_set(std::uint8_t, BombRgb) noexcept override {}
	void palette_show() noexcept override {}
	std::uint8_t tile_mode_get() noexcept override { return 1; }
	void tile_mode_set(std::uint8_t) noexcept override {}
	void boxfill(std::int16_t, std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void bomb_bft(std::int16_t) noexcept override {}
	void bomb_b_tram(std::uint8_t) noexcept override {}
	void bomb_smear(std::uint8_t, std::int16_t) noexcept override {}
	void pi_put(std::int16_t, std::int16_t, std::uint8_t) noexcept override {}
	void sound_effect(std::uint8_t) noexcept override {}
	void overlay_wipe() noexcept override {}
	void hud_bombs(std::int8_t) noexcept override {}
	void invalidate_bomb(BombInvalidation) noexcept override {}
	void render_all() noexcept override {}
};

void timeline(BombSelection selection, unsigned expected_updates,
	unsigned expected_words, unsigned expected_sparks, unsigned clear_update)
{
	BombState bomb{};
	PlayerHitState player{};
	ScoreState score{};
	score.bombs = 1;
	BombAnimationState animation{};
	BombEffectState effects{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	RandomState random{};
	Sink sink{};
	require(player_bomb(bomb, score, player, bullets, 0, 1, sink) == PlayerBombResult::started);
	// A bullet spawned after the initial trigger survives until the animation's
	// own clear. No external bullet update is needed to observe that boundary.
	bullets.bullets[0].flag = enemy_bullet_alive;
	unsigned updates = 0;
	unsigned words = 0;
	while (bomb.bombing && updates < 200) {
		if (updates != 0) bomb_invalidate(bomb, static_cast<std::uint8_t>(selection), sink);
		const auto before = random.cursor;
		const auto result = bomb_update_and_render(bomb, player, selection,
			animation, effects, sparks, bullets, random, 0, 1, 0, false, sink);
		words += static_cast<std::uint8_t>(random.cursor - before);
		++updates;
		require(bullets.bullets[0].flag == (updates < clear_update ?
			enemy_bullet_alive : enemy_bullet_remove));
		require(result == (updates == expected_updates ? BombUpdateResult::completed : BombUpdateResult::running));
		if (updates == 34) {
			require(bomb.bomb_frame == 0 && bomb.bomb_circle_done == 1);
			require(sink.circles == 32U * 64U);
		}
	}
	require(updates == expected_updates && words == expected_words);
	require(sparks.ring_index == expected_sparks % 64U);
	require(!player.invincible_via_bomb && player.invincibility_time == 70);
	require(animation.palette_tone == 100);
	const auto ended = bomb;
	require(bomb_update_and_render(bomb, player, selection, animation, effects,
		sparks, bullets, random, 99, 99, 0, false, sink) == BombUpdateResult::inactive);
	require(bomb == ended);
}
} // namespace

int main()
{
	// Independently traced source timelines; these are not emulator parity claims.
	timeline(BombSelection::reimu_a, 190, 212, 71, 170);
	timeline(BombSelection::reimu_b, 190, 288, 0, 170);
	timeline(BombSelection::reimu_c, 162, 268, 0, 146);
	BombState bomb{};
	bomb.bombing = true;
	PlayerHitState player{};
	BombAnimationState animation{};
	BombEffectState effects{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	RandomState random{};
	Sink sink{};
	auto update = [&]() {
		return bomb_update_and_render(bomb, player, BombSelection::reimu_a,
			animation, effects, sparks, bullets, random, 99, 1, 0, false, sink);
	};
	// Invalid page retains the source increments before the page write.
	require(update() == BombUpdateResult::invalid_page);
	require(bomb.bomb_frame == 1 && bomb.bomb_circle_frame == 1);
	require(sink.circles == 0);
	// bool16 compares with exactly 0/1; another raw value does not dispatch.
	bomb.bomb_circle_done = 2;
	bomb.bomb_frame = 32767;
	require(update() == BombUpdateResult::running);
	require(bomb.bomb_frame == -32768 && bomb.bombing);
}
