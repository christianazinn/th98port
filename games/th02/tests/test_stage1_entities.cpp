#include "th02/core/stage1_entities.hpp"
#include "th02/core/bomb_animation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

constexpr std::size_t front_page = 0U;
constexpr std::size_t back_page = 1U;
using ScriptSlot = std::array<std::uint8_t, stage_script_size>;

class EnemyCallbacks final : public EnemyUpdateCallbacks {
public:
	void sound_effect(std::uint8_t) noexcept override {}
	void tile_ring_set_and_put_both_8(
		std::int16_t, std::int16_t, std::int16_t
	) noexcept override {}
	void render_enemy(EnemySprite) noexcept override {}
	void render_enemy_hit_white(EnemySprite) noexcept override {}
};

class RikaCallbacksRecorder final : public RikaCallbacks {
public:
	void sound_effect(std::uint8_t) noexcept override {}
	void invalidate_rika(RikaInvalidation) noexcept override {}
	void render_rika(RikaSprite) noexcept override {}
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
	void explosion_graph_scroll(std::uint16_t, std::uint16_t,
		std::uint16_t) noexcept override {}
};

class MidbossCallbacks final : public Midboss1Callbacks {
public:
	void invalidate_midboss(Midboss1Invalidation) noexcept override {}
	void tile_ring_set_and_put_both_8(
		std::int16_t, std::int16_t, std::int16_t
	) noexcept override {}
	void sound_effect(std::uint8_t) noexcept override {}
	void render_midboss(Midboss1Sprite) noexcept override {}
};

class PlayerMissCallbacks final : public PlayerMissPresentationCallbacks {
public:
	void sound_effect(std::uint8_t) noexcept override {}
	void hud_power(std::uint8_t, std::uint8_t) noexcept override {}
	void hud_lives(std::int8_t) noexcept override {}
	void hud_bombs(std::int8_t) noexcept override {}
	void render_player(PlayerMissSprite) noexcept override {}
};

class ItemCallbacks final : public ItemLifecyclePresentationCallbacks {
public:
	std::size_t update_count{};
	void hud_power(std::uint8_t, std::uint8_t) noexcept override {}
	void hud_bombs(std::int8_t) noexcept override {}
	void hud_lives(std::int8_t) noexcept override {}
	void sound_effect(std::uint8_t) noexcept override {}
	void pointnum_add(std::int16_t, std::int16_t, std::uint16_t) noexcept override {}
	void render_item(ItemSprite) noexcept override {}
	void pointnums_update_and_render() noexcept override { ++update_count; }
};

class LaserCallbacks final : public LaserUpdateCallbacks {
public:
	std::size_t render_count{};
	void sound_effect(std::uint8_t) noexcept override {}
	void render_laser(LaserPresentation) noexcept override { ++render_count; }
};

class BulletCallbacks final : public EnemyBulletPresentationCallbacks {
public:
	std::size_t present_count{};
	void present(EnemyBulletPresentationEvent) noexcept override { ++present_count; }
};

struct Fixture {
	PlayerFrameState frame{};
	PlayerHitState hit{};
	PlayerMissState miss{};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState shot_lifetime{};
	PlayerShotStreamState stream{};
	ScoreState score{};
	ItemPoolState items{};
	ItemLifecycleState item_lifecycle{};
	SparkPoolState sparks{};
	SparkUpdateParameters spark_parameters{};
	EnemyBulletPoolState bullets{};
	EnemyBulletSpecialState bullet_special{};
	LaserPoolState lasers{};
	RandomState random{};
	BombState bomb{};
	BombAnimationState animation{};
	bool quit{};
	std::uint32_t stage_frame{};
	StageProgression progression{StageProgression::clear};

	EnemyPoolState enemies{};
	EnemyUpdateState enemy_position{};
	std::array<EnemyTemplate, 1U> templates{};
	std::array<ScriptSlot, 1U> scripts{};
	std::array<std::uint8_t, 1U> script_lengths{};
	EnemyRunContext enemy_run;
	EnemyUpdateContext enemy_context;

	BossState boss{};
	RikaState rika{};
	RikaCombatContext rika_context;
	Midboss1State midboss{};
	Midboss1Context midboss_context;

	EnemyCallbacks enemy_callbacks{};
	RikaCallbacksRecorder rika_callbacks{};
	MidbossCallbacks midboss_callbacks{};
	PlayerMissCallbacks player_miss_presentation{};
	PlayerMissUpdateCallbacks player_miss_callbacks;
	ItemCallbacks item_callbacks{};
	LaserCallbacks laser_callbacks{};
	BulletCallbacks bullet_callbacks{};
	bool enemies_enabled{};
	bool rika_active{};
	bool midboss_active{};
	bool lasers_enabled{};

	Fixture() noexcept :
		enemy_run{bullets, random, frame, 7, 1, false, 0U, 0},
		enemy_context{{templates, scripts, script_lengths}, enemy_run, hit, shots,
			stream, score, items, sparks, 0U, 1U, 64U},
		rika_context{frame, hit, shots, score, sparks, bullets, lasers, random,
			stage_frame, 0, 1, false, 0U, 1U, 0,
			animation.slowdown_factor, animation.palette_tone},
		midboss_context{frame, shots, score, sparks, bullets, random, 0, 1,
			false, 0U, 1U, 0U, 0, 0},
		player_miss_callbacks{miss, frame, stream, score, items, sparks, random,
			player_miss_presentation}
	{
		frame.page_front = front_page;
		frame.page_back = back_page;
		frame.player.current = {300, 300};
		frame.player.on_page[front_page] = {300, 300};
		frame.player.on_page[back_page] = {300, 300};
		boss.topleft_on_page = {{{100, 48}, {100, 48}}};
		rika.topleft = {100, 48};
		templates[0U].script_index = 0U;
		templates[0U].hp = -1;
		templates[0U].item = static_cast<std::uint8_t>(ItemType::point);
		scripts[0U][0U] = 0U;
		scripts[0U][1U] = 1U;
		script_lengths[0U] = 2U;
		for(std::size_t i = 0U; i < random.ring.size(); ++i) {
			random.ring[i] = static_cast<std::uint8_t>(i);
		}
	}

	[[nodiscard]] Stage1EntitiesContext context() noexcept
	{
		return {enemies_enabled, enemies, enemy_position, enemy_context,
			enemy_callbacks, boss, rika_active, rika, rika_context, rika_callbacks,
			midboss_active, midboss, midboss_context, midboss_callbacks, bomb, miss,
			hit, quit, shots, shot_lifetime, player_miss_callbacks, items,
			item_lifecycle, stream, score, item_callbacks, lasers_enabled, lasers,
			laser_callbacks, bullets, bullet_special, bullet_callbacks, sparks,
			spark_parameters, frame, stage_frame, progression};
	}
};

[[nodiscard]] bool test_disabled_enemies_preserve_target_and_null_boss_is_stage() noexcept
{
	Fixture fixture{};
	fixture.boss.target = {77, 88};
	fixture.enemy_position.boss_position = {11, 12};
	auto context = fixture.context();
	const auto result = stage1_entities_update(context);
	return result.result == Stage1EntitiesResult::completed &&
		fixture.boss.target == ScreenPoint{77, 88} &&
		fixture.enemy_position.boss_position == ScreenPoint{11, 12} &&
		fixture.progression == StageProgression::stage &&
		fixture.item_lifecycle.frame_unused == 1 &&
		fixture.item_callbacks.update_count == 1U;
}

[[nodiscard]] bool test_enabled_enemy_then_rika_handoff_uses_one_target() noexcept
{
	Fixture fixture{};
	fixture.enemies_enabled = true;
	fixture.enemies.enemies_loop_bound = 1U;
	auto &enemy = fixture.enemies.enemies[0U];
	enemy.flag = enemy_alive;
	enemy.template_id = 0;
	enemy.pos_on_page = {{{100, 100}, {100, 100}}};
	enemy.render_as = 0U;
	fixture.rika_active = true;
	auto context = fixture.context();
	const auto result = stage1_entities_update(context);
	return result.result == Stage1EntitiesResult::completed &&
		fixture.progression == StageProgression::boss &&
		fixture.boss.target == ScreenPoint{124, 48} &&
		fixture.enemy_position.boss_position == fixture.boss.target &&
		fixture.enemy_context.run.playperf == fixture.miss.playperf &&
		fixture.rika_context.playperf == fixture.miss.playperf &&
		fixture.rika_context.bombing == fixture.bomb.bombing &&
		fixture.rika_context.shot_level == fixture.stream.shot_level;
}

[[nodiscard]] bool test_midboss_runs_after_stage_boss_slot_and_overwrites_target() noexcept
{
	Fixture fixture{};
	fixture.midboss_active = true;
	fixture.boss.target = {9, 10};
	auto context = fixture.context();
	const auto result = stage1_entities_update(context);
	return result.result == Stage1EntitiesResult::completed &&
		fixture.progression == StageProgression::stage &&
		fixture.midboss.active == 1 && fixture.midboss.scenery_frame == 1 &&
		fixture.boss.target == ScreenPoint{216, 64} &&
		fixture.enemy_position.boss_position == fixture.boss.target &&
		fixture.midboss_context.playperf == fixture.miss.playperf &&
		fixture.midboss_context.bombing == fixture.bomb.bombing &&
		fixture.midboss_context.shot_level == fixture.stream.shot_level;
}

[[nodiscard]] bool test_player_miss_continues_to_items_lasers_bullets_and_sparks() noexcept
{
	Fixture fixture{};
	fixture.hit.raw_hit = 1U;
	fixture.hit.miss_active = true;
	fixture.miss.miss_frame = 67U;
	fixture.lasers_enabled = true;
	fixture.lasers.lasers[0U] = {laser_alive, laser_phase_wait,
		{100, 100}, 1, 4, 0U, 30U};
	fixture.bullets.bullets[0U].flag = enemy_bullet_alive;
	fixture.bullets.bullets[0U].size_type = enemy_bullet_pellet;
	fixture.bullets.bullets[0U].screen_topleft_on_page[back_page] = {1600, 1600};
	fixture.bullets.bullets[0U].patnum = 1U;
	fixture.sparks.sparks[0U].flag = spark_alive;
	fixture.sparks.sparks[0U].age = 0U;
	fixture.sparks.sparks[0U].screen_topleft[back_page] = {1600, 1600};
	fixture.spark_parameters = {0, 8U};
	auto context = fixture.context();
	const auto result = stage1_entities_update(context);
	return result.result == Stage1EntitiesResult::completed &&
		result.player == PlayerUpdateResult::miss_handled &&
		fixture.miss.miss_frame == 0U && !fixture.hit.miss_active &&
		fixture.hit.raw_hit == 0U && fixture.item_lifecycle.frame_unused == 1 &&
		fixture.item_callbacks.update_count == 1U &&
		fixture.lasers.lasers[0U].wait_frames == 0 &&
		fixture.bullet_callbacks.present_count == 3U &&
		fixture.sparks.sparks[0U].age == 1U;
}

[[nodiscard]] bool test_bullet_hit_starts_miss_only_on_next_entity_pass() noexcept
{
	Fixture fixture{};
	fixture.bullets.bullets[0U].flag = enemy_bullet_alive;
	fixture.bullets.bullets[0U].size_type = enemy_bullet_pellet;
	fixture.bullets.bullets[0U].screen_topleft_on_page[back_page] = {4912, 4992};
	auto context = fixture.context();
	const auto first = stage1_entities_update(context);
	if(first.result != Stage1EntitiesResult::completed ||
		first.player != PlayerUpdateResult::completed || fixture.hit.raw_hit != 1U ||
		fixture.miss.miss_frame != 0U || fixture.hit.miss_active) {
		return false;
	}
	const auto second = stage1_entities_update(context);
	if(second.result != Stage1EntitiesResult::completed ||
		second.player != PlayerUpdateResult::miss_handled ||
		fixture.miss.miss_frame != 1U || !fixture.hit.miss_active ||
		fixture.hit.raw_hit != 1U || fixture.miss.stage_miss_count != 1U) {
		return false;
	}

	Fixture immune{};
	immune.hit.invincibility_time = 2U;
	immune.bullets.bullets[0U].flag = enemy_bullet_alive;
	immune.bullets.bullets[0U].size_type = enemy_bullet_pellet;
	immune.bullets.bullets[0U].screen_topleft_on_page[back_page] = {4912, 4992};
	auto immune_context = immune.context();
	if(stage1_entities_update(immune_context).result !=
		Stage1EntitiesResult::completed || immune.hit.raw_hit != 1U) {
		return false;
	}
	const auto immune_second = stage1_entities_update(immune_context);
	if(immune_second.result != Stage1EntitiesResult::completed ||
		immune_second.player != PlayerUpdateResult::completed ||
		immune.hit.raw_hit != 0U || immune.hit.invincibility_time != 0U ||
		immune.miss.miss_frame != 0U || immune.hit.miss_active) {
		return false;
	}
	return true;
}

[[nodiscard]] bool test_midboss_refusal_keeps_reached_target_prefix() noexcept
{
	Fixture fixture{};
	fixture.midboss_active = true;
	fixture.boss.phase_frame = 1;
	fixture.frame.page_back = 2U;
	fixture.item_lifecycle.frame_unused = 19;
	auto context = fixture.context();
	const auto result = stage1_entities_update(context);
	return result.result == Stage1EntitiesResult::midboss_refused &&
		result.midboss == Midboss1UpdateResult::invalid_page &&
		fixture.boss.target == ScreenPoint{216, 64} &&
		fixture.enemy_position.boss_position == fixture.boss.target &&
		fixture.boss.phase_frame == 2 && fixture.item_lifecycle.frame_unused == 19;
}

[[nodiscard]] bool test_enabled_enemy_refusal_synchronizes_reset_prefix() noexcept
{
	Fixture fixture{};
	fixture.enemies_enabled = true;
	fixture.enemies.enemies_loop_bound = 26U;
	fixture.boss.target = {50, 60};
	auto context = fixture.context();
	const auto result = stage1_entities_update(context);
	return result.result == Stage1EntitiesResult::enemy_refused &&
		result.enemy == EnemyUpdateResult::invalid_loop_bound &&
		fixture.boss.target == ScreenPoint{-1, -1} &&
		fixture.enemy_position.boss_position == fixture.boss.target &&
		fixture.progression == StageProgression::clear;
}

} // namespace

int main()
{
	struct Test {
		const char *name;
		bool (*run)();
	};
	const std::array<Test, 7U> tests{{
		Test{"disabled enemy/null boss", test_disabled_enemies_preserve_target_and_null_boss_is_stage},
		Test{"enemy/Rika target handoff", test_enabled_enemy_then_rika_handoff_uses_one_target},
		Test{"M1 target handoff", test_midboss_runs_after_stage_boss_slot_and_overwrites_target},
		Test{"miss continuation", test_player_miss_continues_to_items_lasers_bullets_and_sparks},
		Test{"bullet hit order", test_bullet_hit_starts_miss_only_on_next_entity_pass},
		Test{"M1 refusal prefix", test_midboss_refusal_keeps_reached_target_prefix},
		Test{"enemy refusal prefix", test_enabled_enemy_refusal_synchronizes_reset_prefix},
	}};
	for(const auto &test : tests) {
		if(!test.run()) {
			std::fprintf(stderr, "stage1 entity composition test failed: %s\n", test.name);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
