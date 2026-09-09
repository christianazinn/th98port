#include "th02/core/stage3_entities.hpp"

namespace th02::core {
Stage3EntitiesStepResult stage3_entities_update(StageState &s, Stage3EntitiesPresentation &p) noexcept
{
	Stage3EntitiesStepResult result;
	const auto refuse = [&](StageEntitiesResult why) { result.result = why; return result; };
	if (s.enemies_enabled) {
		if (s.stage_data.template_count < 0 || s.stage_data.script_count < 0 ||
			static_cast<std::size_t>(s.stage_data.template_count) > s.stage_data.templates.size() ||
			static_cast<std::size_t>(s.stage_data.script_count) > s.stage_data.scripts.size())
			return refuse(StageEntitiesResult::enemy_refused);
		EnemyRunContext run{s.bullets, s.random, s.frame, s.player_miss.playperf,
			s.rank, s.bomb.bombing, s.scroll.delta, s.scroll.line};
		EnemyUpdateContext context{{std::span{s.stage_data.templates}.first(static_cast<std::size_t>(s.stage_data.template_count)),
			std::span{s.stage_data.scripts}.first(static_cast<std::size_t>(s.stage_data.script_count)),
			std::span{s.stage_data.script_lengths}.first(static_cast<std::size_t>(s.stage_data.script_count))},
			run, s.player_hit, s.player_shots, s.player_stream, s.score, s.items, s.sparks,
			s.stage_frame, s.bomb_damage_frame_mask, s.max_enemy_vm_steps};
		const auto enemy = enemies_update_and_render(s.enemies, s.enemy_target_projection, context, p.enemies);
		s.boss.target = s.enemy_target_projection.boss_position;
		if (enemy != EnemyUpdateResult::completed) return refuse(StageEntitiesResult::enemy_refused);
	}
	if (s.boss_enabled) {
		StonesCombatContext context{s.frame, s.player_hit, s.player_shots, s.items,
			s.score, s.sparks, s.bullets, s.lasers, s.random, s.stage_frame,
			s.player_miss.playperf, s.rank, s.bomb.bombing, s.player_stream.shot_level,
			s.bomb_damage_frame_mask, s.scroll.line, s.bomb_animation.slowdown_factor, s.bomb_animation.palette_tone};
		result.stones = stones_update(s.boss, s.stones, context, p.stones);
		s.enemy_target_projection.boss_position = s.boss.target;
		if (result.stones == StonesUpdateResult::boss) s.stage_progression = StageProgression::boss;
		else if (result.stones == StonesUpdateResult::stage_clear) s.stage_progression = StageProgression::clear;
		else return refuse(StageEntitiesResult::stones_refused);
	} else s.stage_progression = StageProgression::stage;
	if (s.midboss_active != 0) {
		Midboss3Context context{s.frame, s.player_shots, s.score, s.sparks, s.bullets,
			s.random, s.player_miss.playperf, s.rank, s.bomb.bombing, s.player_stream.shot_level,
			s.bomb_damage_frame_mask, s.stage_frame, s.scroll.line, s.scroll.delta};
		result.midboss = midboss3_update_and_render(s.boss, s.midboss3,
			{s.stones.flag, s.stones.damage, s.stones.patnum}, context, p.midboss);
		s.enemy_target_projection.boss_position = s.boss.target;
		if (result.midboss != Midboss2UpdateResult::completed) return refuse(StageEntitiesResult::midboss_refused);
	}
	PlayerMissUpdateCallbacks miss{s.player_miss, s.frame, s.player_stream,
		s.score, s.items, s.sparks, s.random, p.player_miss};
	const auto player = player_update_state_step(s.player_hit, s.quit, s.player_shots,
		s.player_shot_lifetime, s.frame.page_back, s.stage_frame, miss);
	if (player != PlayerUpdateResult::completed && player != PlayerUpdateResult::miss_handled)
		return refuse(StageEntitiesResult::player_refused);
	if (items_lifecycle_step(s.items, s.item_lifecycle, s.player_miss, s.player_stream,
		s.score, s.frame, s.player_hit, s.rank, p.items) != ItemLifecycleResult::completed)
		return refuse(StageEntitiesResult::items_refused);
	if (s.lasers_enabled) lasers_update_and_render(s.lasers, s.frame, s.player_hit,
		s.stage_frame, s.scroll.line, p.lasers);
	if (bullets_update_and_render(s.bullets, s.frame, s.player_hit, s.bullet_special,
		s.stage_frame, s.scroll.line, p.bullets) != EnemyBulletUpdateResult::completed)
		return refuse(StageEntitiesResult::bullets_refused);
	if (!sparks_update(s.sparks, s.frame.page_back, s.spark_parameters))
		return refuse(StageEntitiesResult::sparks_refused);
	return result;
}
} // namespace th02::core
