#include "th02/core/stage1_invalidation.hpp"

#include <cstdlib>
#include <vector>

using namespace th02::core;

struct Trace final : RikaCallbacks, Midboss1Callbacks,
	EnemyInvalidationCallbacks, PlayerFrameInvalidationCallbacks,
	LaserInvalidationCallbacks, ItemScheduleCallbacks,
	SparkInvalidationCallbacks, BombPresentationCallbacks {
	std::vector<int> events;
	void invalidate_rika(RikaInvalidation) noexcept override { events.push_back(1); }
	void invalidate_midboss(Midboss1Invalidation) noexcept override { events.push_back(2); }
	void invalidate_enemy(EnemyInvalidation) noexcept override { events.push_back(3); }
	void invalidate_shot(PlayerShotInvalidation) noexcept override { events.push_back(4); }
	void invalidate_player(PlayerFrameInvalidation) noexcept override { events.push_back(5); }
	void invalidate_option(PlayerFrameInvalidation) noexcept override { events.push_back(6); }
	void invalidate_bullet(EnemyBulletInvalidation) noexcept override { events.push_back(7); }
	void invalidate_laser(LaserInvalidation) noexcept override { events.push_back(8); }
	void invalidate_item(ItemInvalidation) noexcept override { events.push_back(9); }
	void pointnums_invalidate() noexcept override { events.push_back(10); }
	void invalidate_spark(SparkInvalidation) noexcept override { events.push_back(11); }
	void render_all() noexcept override { events.push_back(12); }
	void pointnums_init_for_rank_and_reset() noexcept override {}
	void hud_bombs(std::int8_t) noexcept override {}
	void invalidate_bomb(BombInvalidation) noexcept override {}
	void sound_effect(std::uint8_t) noexcept override {}
	void render_rika(RikaSprite) noexcept override {}
	void render_midboss(Midboss1Sprite) noexcept override {}
	void tile_ring_set_and_put_both_8(std::int16_t, std::int16_t, std::int16_t) noexcept override {}
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
	BossState boss{};
	RikaState rika{};
	Midboss1State midboss{};
	std::int16_t active{1};
	EnemyPoolState enemies{};
	PlayerFrameState frame{};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	EnemyBulletPoolState bullets{};
	LaserPoolState lasers{};
	ItemPoolState items{};
	SparkPoolState sparks{};
	BombState bomb{};
	Trace trace{};
	Stage1InvalidationContext context{boss, rika, midboss, active, enemies,
		frame, shots, lifetime, bullets, lasers, items, sparks, bomb,
		true, true, true, false, 0U, 1U};
	Stage1InvalidationPresentation presentation{trace, trace, trace, trace,
		trace, trace, trace, trace, trace};
	Fixture() {
		frame.page_front = 0U;
		frame.page_back = 1U;
		frame.player.on_page = {{{100, 200}, {300, 350}}};
		boss.phase_frame = 4;
		boss.topleft_on_page = {{{11, 21}, {31, 41}}};
		midboss.active = -1;
		enemies.enemies[0].flag = enemy_remove;
		shots.shots[0].flag = player_shot_remove;
		bullets.bullets[0].flag = enemy_bullet_remove;
		bullets.bullets[0].size_type = enemy_bullet_pellet;
		lasers.lasers[0].flag = laser_remove;
		items.items[0].flag = item_remove;
		sparks.sparks[0].flag = spark_remove;
		bomb.bombing = true;
		bomb.bomb_frame = 1;
		bomb.bomb_circle_done = 1;
	}
};

int main()
{
	Fixture all;
	if(stage1_invalidate(all.context, all.presentation) != Stage1InvalidationResult::completed
		|| all.trace.events != std::vector<int>{1, 2, 3, 4, 5, 6, 6, 7, 8, 9, 10, 11, 12}
		|| all.active != -1 || all.boss.topleft_on_page[1] != ScreenPoint{11, 21}
		|| all.frame.player.on_page[1] != ScreenPoint{100, 200}
		|| all.frame.page_front != 0U || all.frame.page_back != 1U
		|| all.enemies.enemies[0].flag != enemy_free
		|| all.shots.shots[0].flag != player_shot_free
		|| all.bullets.bullets[0].flag != enemy_bullet_free
		|| all.lasers.lasers[0].flag != laser_free
		|| all.items.items[0].flag != item_free
		|| all.sparks.sparks[0].flag != spark_free) return EXIT_FAILURE;

	Fixture disabled;
	disabled.context.boss_enabled = false;
	disabled.context.enemies_enabled = false;
	disabled.context.lasers_enabled = false;
	disabled.active = 0;
	if(stage1_invalidate(disabled.context, disabled.presentation) != Stage1InvalidationResult::completed
		|| disabled.trace.events != std::vector<int>{4, 5, 6, 6, 7, 9, 10, 11, 12}
		|| disabled.enemies.enemies[0].flag != enemy_remove
		|| disabled.lasers.lasers[0].flag != laser_remove
		|| disabled.boss.topleft_on_page[1] != ScreenPoint{31, 41}) return EXIT_FAILURE;

	Fixture invalid;
	invalid.frame.page_back = 2U;
	if(stage1_invalidate(invalid.context, invalid.presentation) != Stage1InvalidationResult::boss_invalid_page
		|| !invalid.trace.events.empty() || invalid.active != 1) return EXIT_FAILURE;
	invalid.context.boss_enabled = false;
	if(stage1_invalidate(invalid.context, invalid.presentation) != Stage1InvalidationResult::midboss_invalid_page)
		return EXIT_FAILURE;
	invalid.active = 0;
	if(stage1_invalidate(invalid.context, invalid.presentation) != Stage1InvalidationResult::enemy_invalid_page)
		return EXIT_FAILURE;
	invalid.context.enemies_enabled = false;
	if(stage1_invalidate(invalid.context, invalid.presentation) != Stage1InvalidationResult::player_invalid_page
		|| !invalid.trace.events.empty() || invalid.bomb.bomb_frame != 1) return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
