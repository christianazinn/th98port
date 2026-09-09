#include "th02/core/stage1_entities.hpp"

namespace th02::core {
namespace {

void refresh_enemy_context(Stage1EntitiesContext &context) noexcept
{
	context.enemy_context.run.playperf = context.player_miss.playperf;
	context.enemy_context.run.bombing = context.bomb.bombing;
	context.enemy_context.stage_frame = context.stage_frame;
}

void refresh_rika_context(Stage1EntitiesContext &context) noexcept
{
	context.rika_context.playperf = context.player_miss.playperf;
	context.rika_context.rank = context.enemy_context.run.rank;
	context.rika_context.bombing = context.bomb.bombing;
	context.rika_context.shot_level = context.player_stream.shot_level;
	context.rika_context.bomb_damage_frame_mask =
		context.enemy_context.bomb_damage_frame_mask;
	context.rika_context.scroll_line = context.enemy_context.run.scroll_line;
}

void refresh_midboss_context(Stage1EntitiesContext &context) noexcept
{
	context.midboss_context.playperf = context.player_miss.playperf;
	context.midboss_context.rank = context.enemy_context.run.rank;
	context.midboss_context.bombing = context.bomb.bombing;
	context.midboss_context.shot_level = context.player_stream.shot_level;
	context.midboss_context.bomb_damage_frame_mask =
		context.enemy_context.bomb_damage_frame_mask;
	context.midboss_context.stage_frame = context.stage_frame;
	context.midboss_context.scroll_line = context.enemy_context.run.scroll_line;
}

[[nodiscard]] bool player_result_refused(PlayerUpdateResult result) noexcept
{
	return (result != PlayerUpdateResult::completed) &&
		(result != PlayerUpdateResult::miss_handled);
}

} // namespace

Stage1EntitiesStepResult stage1_entities_update(
	Stage1EntitiesContext &context
) noexcept
{
	Stage1EntitiesStepResult result{};

	if(context.enemies_enabled) {
		refresh_enemy_context(context);
		result.enemy = enemies_update_and_render(context.enemies,
			context.enemy_position, context.enemy_context, context.enemy_callbacks);
		context.boss.target = context.enemy_position.boss_position;
		if(result.enemy != EnemyUpdateResult::completed) {
			result.result = Stage1EntitiesResult::enemy_refused;
			return result;
		}
	}

	if(context.rika_active) {
		refresh_rika_context(context);
		result.rika = rika_update(context.boss, context.rika,
			context.rika_context, context.rika_callbacks);
		context.enemy_position.boss_position = context.boss.target;
		if(result.rika == RikaUpdateResult::boss) {
			context.stage_progression = StageProgression::boss;
		} else if(result.rika == RikaUpdateResult::stage_clear) {
			context.stage_progression = StageProgression::clear;
		} else {
			result.result = Stage1EntitiesResult::rika_refused;
			return result;
		}
	} else {
		context.stage_progression = StageProgression::stage;
	}

	if(context.midboss_active) {
		refresh_midboss_context(context);
		result.midboss = midboss1_update_and_render(context.boss,
			context.midboss, context.midboss_context, context.midboss_callbacks);
		context.enemy_position.boss_position = context.boss.target;
		if(result.midboss != Midboss1UpdateResult::completed) {
			result.result = Stage1EntitiesResult::midboss_refused;
			return result;
		}
	}

	result.player = player_update_state_step(context.player_hit, context.quit,
		context.player_shots, context.player_shot_lifetime,
		context.player_frame.page_back, context.stage_frame,
		context.player_miss_callbacks);
	if(player_result_refused(result.player)) {
		result.result = Stage1EntitiesResult::player_refused;
		return result;
	}

	result.items = items_lifecycle_step(context.items, context.item_lifecycle,
		context.player_miss, context.player_stream, context.score,
		context.player_frame, context.player_hit, context.enemy_context.run.rank,
		context.item_callbacks);
	if(result.items != ItemLifecycleResult::completed) {
		result.result = Stage1EntitiesResult::items_refused;
		return result;
	}

	if(context.lasers_enabled) {
		lasers_update_and_render(context.lasers, context.player_frame,
			context.player_hit, context.stage_frame,
			context.enemy_context.run.scroll_line, context.laser_callbacks);
	}

	result.bullets = bullets_update_and_render(context.bullets,
		context.player_frame, context.player_hit, context.bullet_special,
		context.stage_frame, context.enemy_context.run.scroll_line,
		context.bullet_callbacks);
	if(result.bullets != EnemyBulletUpdateResult::completed) {
		result.result = Stage1EntitiesResult::bullets_refused;
		return result;
	}

	result.sparks_updated = sparks_update(context.sparks,
		context.player_frame.page_back, context.spark_parameters);
	if(!result.sparks_updated) {
		result.result = Stage1EntitiesResult::sparks_refused;
	}
	return result;
}

} // namespace th02::core
