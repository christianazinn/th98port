#pragma once
#include "th02/core/stage_state.hpp"

namespace th02::core::detail {
inline StageEntitiesResult stage_enemies_update(StageState &s, EnemyUpdateCallbacks &p) noexcept
{
	if (!s.enemies_enabled) return StageEntitiesResult::completed;
	if (s.stage_data.template_count < 0 || s.stage_data.script_count < 0 ||
		static_cast<std::size_t>(s.stage_data.template_count) > s.stage_data.templates.size() ||
		static_cast<std::size_t>(s.stage_data.script_count) > s.stage_data.scripts.size())
		return StageEntitiesResult::enemy_refused;
	EnemyRunContext run{s.bullets, s.random, s.frame, s.player_miss.playperf,
		s.rank, s.bomb.bombing, s.scroll.delta, s.scroll.line};
	EnemyUpdateContext context{{std::span{s.stage_data.templates}.first(static_cast<std::size_t>(s.stage_data.template_count)),
		std::span{s.stage_data.scripts}.first(static_cast<std::size_t>(s.stage_data.script_count)),
		std::span{s.stage_data.script_lengths}.first(static_cast<std::size_t>(s.stage_data.script_count))},
		run, s.player_hit, s.player_shots, s.player_stream, s.score, s.items, s.sparks,
		s.stage_frame, s.bomb_damage_frame_mask, s.max_enemy_vm_steps};
	const auto result = enemies_update_and_render(s.enemies, s.enemy_target_projection, context, p);
	s.boss.target = s.enemy_target_projection.boss_position;
	return result == EnemyUpdateResult::completed ? StageEntitiesResult::completed : StageEntitiesResult::enemy_refused;
}

template<class Presentation>
StageEntitiesResult stage_entities_tail(StageState &s, Presentation &p) noexcept
{
	PlayerMissUpdateCallbacks miss{s.player_miss, s.frame, s.player_stream,
		s.score, s.items, s.sparks, s.random, p.player_miss};
	const auto player = player_update_state_step(s.player_hit, s.quit, s.player_shots,
		s.player_shot_lifetime, s.frame.page_back, s.stage_frame, miss);
	if (player != PlayerUpdateResult::completed && player != PlayerUpdateResult::miss_handled)
		return StageEntitiesResult::player_refused;
	if (items_lifecycle_step(s.items, s.item_lifecycle, s.player_miss, s.player_stream,
		s.score, s.frame, s.player_hit, s.rank, p.items) != ItemLifecycleResult::completed)
		return StageEntitiesResult::items_refused;
	if (s.lasers_enabled) lasers_update_and_render(s.lasers, s.frame, s.player_hit,
		s.stage_frame, s.scroll.line, p.lasers);
	if (bullets_update_and_render(s.bullets, s.frame, s.player_hit, s.bullet_special,
		s.stage_frame, s.scroll.line, p.bullets) != EnemyBulletUpdateResult::completed)
		return StageEntitiesResult::bullets_refused;
	if (!sparks_update(s.sparks, s.frame.page_back, s.spark_parameters))
		return StageEntitiesResult::sparks_refused;
	return StageEntitiesResult::completed;
}
} // namespace th02::core::detail
