#include "th02/core/stage3_entities.hpp"
#include "stage_entities_common.hpp"

namespace th02::core {
Stage3EntitiesStepResult stage3_entities_update(StageState &s, Stage3EntitiesPresentation &p) noexcept
{
	Stage3EntitiesStepResult result;
	const auto refuse = [&](StageEntitiesResult why) { result.result = why; return result; };
	const auto enemy = detail::stage_enemies_update(s, p.enemies);
	if (enemy != StageEntitiesResult::completed) return refuse(enemy);
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
	result.result = detail::stage_entities_tail(s, p);
	return result;
}
} // namespace th02::core
