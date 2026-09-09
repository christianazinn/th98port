#include "th02/core/stage2_entities.hpp"

namespace th02::core {
namespace {

void refresh_enemy_context(EnemyUpdateContext &enemy_context,
	const PlayerMissState &player_miss, const BombState &bomb,
	std::uint32_t stage_frame
) noexcept
{
	enemy_context.run.playperf = player_miss.playperf;
	enemy_context.run.bombing = bomb.bombing;
	enemy_context.stage_frame = stage_frame;
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

enum class EntityTailResult : std::uint8_t {
	completed,
	player_refused,
	items_refused,
	bullets_refused,
	sparks_refused,
};

struct EntityTailStepResult {
	EntityTailResult result{EntityTailResult::completed};
	PlayerUpdateResult player{PlayerUpdateResult::completed};
	ItemLifecycleResult items{ItemLifecycleResult::completed};
	EnemyBulletUpdateResult bullets{EnemyBulletUpdateResult::completed};
	bool sparks_updated{};
};

struct EntityTailContext {
	BombState &bomb;
	PlayerMissState &player_miss;
	PlayerHitState &player_hit;
	bool &quit;
	PlayerShotPoolState &player_shots;
	PlayerShotLifetimeState &player_shot_lifetime;
	PlayerMissUpdateCallbacks &player_miss_callbacks;

	ItemPoolState &items;
	ItemLifecycleState &item_lifecycle;
	PlayerShotStreamState &player_stream;
	ScoreState &score;
	ItemLifecyclePresentationCallbacks &item_callbacks;

	bool &lasers_enabled;
	LaserPoolState &lasers;
	LaserUpdateCallbacks &laser_callbacks;

	EnemyBulletPoolState &bullets;
	EnemyBulletSpecialState &bullet_special;
	EnemyBulletPresentationCallbacks &bullet_callbacks;

	SparkPoolState &sparks;
	SparkUpdateParameters &spark_parameters;
	PlayerFrameState &player_frame;
	std::uint32_t &stage_frame;
	std::int8_t rank{};
	std::int16_t scroll_line{};
};

[[nodiscard]] EntityTailStepResult entity_tail_update(
	EntityTailContext context
) noexcept
{
	EntityTailStepResult result{};

	result.player = player_update_state_step(context.player_hit, context.quit,
		context.player_shots, context.player_shot_lifetime,
		context.player_frame.page_back, context.stage_frame,
		context.player_miss_callbacks);
	if(player_result_refused(result.player)) {
		result.result = EntityTailResult::player_refused;
		return result;
	}

	result.items = items_lifecycle_step(context.items, context.item_lifecycle,
		context.player_miss, context.player_stream, context.score,
		context.player_frame, context.player_hit, context.rank,
		context.item_callbacks);
	if(result.items != ItemLifecycleResult::completed) {
		result.result = EntityTailResult::items_refused;
		return result;
	}

	if(context.lasers_enabled) {
		lasers_update_and_render(context.lasers, context.player_frame,
			context.player_hit, context.stage_frame, context.scroll_line,
			context.laser_callbacks);
	}

	result.bullets = bullets_update_and_render(context.bullets,
		context.player_frame, context.player_hit, context.bullet_special,
		context.stage_frame, context.scroll_line, context.bullet_callbacks);
	if(result.bullets != EnemyBulletUpdateResult::completed) {
		result.result = EntityTailResult::bullets_refused;
		return result;
	}

	result.sparks_updated = sparks_update(context.sparks,
		context.player_frame.page_back, context.spark_parameters);
	if(!result.sparks_updated) {
		result.result = EntityTailResult::sparks_refused;
	}
	return result;
}

[[nodiscard]] EntityTailContext entity_tail_context(
	Stage1EntitiesContext &context
) noexcept
{
	return {context.bomb, context.player_miss, context.player_hit, context.quit,
		context.player_shots, context.player_shot_lifetime,
		context.player_miss_callbacks, context.items, context.item_lifecycle,
		context.player_stream, context.score, context.item_callbacks,
		context.lasers_enabled, context.lasers, context.laser_callbacks,
		context.bullets, context.bullet_special, context.bullet_callbacks,
		context.sparks, context.spark_parameters, context.player_frame,
		context.stage_frame, context.enemy_context.run.rank,
		context.enemy_context.run.scroll_line};
}

[[nodiscard]] EntityTailContext entity_tail_context(
	Stage2EntitiesContext &context
) noexcept
{
	return {context.bomb, context.player_miss, context.player_hit, context.quit,
		context.player_shots, context.player_shot_lifetime,
		context.player_miss_callbacks, context.items, context.item_lifecycle,
		context.player_stream, context.score, context.item_callbacks,
		context.lasers_enabled, context.lasers, context.laser_callbacks,
		context.bullets, context.bullet_special, context.bullet_callbacks,
		context.sparks, context.spark_parameters, context.player_frame,
		context.stage_frame, context.enemy_context.run.rank,
		context.enemy_context.run.scroll_line};
}

void assign_tail_result(Stage1EntitiesStepResult &result,
	const EntityTailStepResult &tail
) noexcept
{
	result.player = tail.player;
	result.items = tail.items;
	result.bullets = tail.bullets;
	result.sparks_updated = tail.sparks_updated;
	switch(tail.result) {
	case EntityTailResult::completed:
		break;
	case EntityTailResult::player_refused:
		result.result = Stage1EntitiesResult::player_refused;
		break;
	case EntityTailResult::items_refused:
		result.result = Stage1EntitiesResult::items_refused;
		break;
	case EntityTailResult::bullets_refused:
		result.result = Stage1EntitiesResult::bullets_refused;
		break;
	case EntityTailResult::sparks_refused:
		result.result = Stage1EntitiesResult::sparks_refused;
		break;
	}
}

void assign_tail_result(Stage2EntitiesStepResult &result,
	const EntityTailStepResult &tail
) noexcept
{
	result.player = tail.player;
	result.items = tail.items;
	result.bullets = tail.bullets;
	result.sparks_updated = tail.sparks_updated;
	switch(tail.result) {
	case EntityTailResult::completed:
		break;
	case EntityTailResult::player_refused:
		result.result = Stage2EntitiesResult::player_refused;
		break;
	case EntityTailResult::items_refused:
		result.result = Stage2EntitiesResult::items_refused;
		break;
	case EntityTailResult::bullets_refused:
		result.result = Stage2EntitiesResult::bullets_refused;
		break;
	case EntityTailResult::sparks_refused:
		result.result = Stage2EntitiesResult::sparks_refused;
		break;
	}
}

} // namespace

Stage1EntitiesStepResult stage1_entities_update(
	Stage1EntitiesContext &context
) noexcept
{
	Stage1EntitiesStepResult result{};

	if(context.enemies_enabled) {
		refresh_enemy_context(context.enemy_context, context.player_miss,
			context.bomb, context.stage_frame);
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

	const auto tail = entity_tail_update(entity_tail_context(context));
	assign_tail_result(result, tail);
	return result;
}

Stage2EntitiesStepResult stage2_entities_update(
	Stage2EntitiesContext &context
) noexcept
{
	Stage2EntitiesStepResult result{};

	if(context.enemies_enabled) {
		refresh_enemy_context(context.enemy_context, context.player_miss,
			context.bomb, context.stage_frame);
		result.enemy = enemies_update_and_render(context.enemies,
			context.enemy_position, context.enemy_context, context.enemy_callbacks);
		context.boss.target = context.enemy_position.boss_position;
		if(result.enemy != EnemyUpdateResult::completed) {
			result.result = Stage2EntitiesResult::enemy_refused;
			return result;
		}
	}

	if(context.meira_active) {
		context.meira_context.playperf = context.player_miss.playperf;
		context.meira_context.rank = context.enemy_context.run.rank;
		context.meira_context.bombing = context.bomb.bombing;
		context.meira_context.shot_level = context.player_stream.shot_level;
		context.meira_context.bomb_damage_frame_mask =
			context.enemy_context.bomb_damage_frame_mask;
		context.meira_context.stage_frame = context.stage_frame;
		context.meira_context.scroll_line = context.enemy_context.run.scroll_line;
		result.meira = meira_update(context.boss, context.meira,
			context.meira_context, context.meira_callbacks);
		context.enemy_position.boss_position = context.boss.target;
		if(result.meira == MeiraUpdateResult::boss) {
			context.stage_progression = StageProgression::boss;
		} else if(result.meira == MeiraUpdateResult::stage_clear) {
			context.stage_progression = StageProgression::clear;
		} else {
			result.result = Stage2EntitiesResult::meira_refused;
			return result;
		}
	} else {
		context.stage_progression = StageProgression::stage;
	}

	if(context.midboss_active != 0) {
		context.midboss_context.playperf = context.player_miss.playperf;
		context.midboss_context.rank = context.enemy_context.run.rank;
		context.midboss_context.bombing = context.bomb.bombing;
		context.midboss_context.shot_level = context.player_stream.shot_level;
		context.midboss_context.bomb_damage_frame_mask =
			context.enemy_context.bomb_damage_frame_mask;
		context.midboss_context.stage_frame = context.stage_frame;
		context.midboss_context.scroll_line = context.enemy_context.run.scroll_line;
		result.midboss = midboss2_update_and_render(context.boss, context.midboss,
			context.midboss_context, context.midboss_callbacks);
		context.enemy_position.boss_position = context.boss.target;
		if(result.midboss != Midboss2UpdateResult::completed) {
			result.result = Stage2EntitiesResult::midboss_refused;
			return result;
		}
	}

	const auto tail = entity_tail_update(entity_tail_context(context));
	assign_tail_result(result, tail);
	return result;
}

} // namespace th02::core
