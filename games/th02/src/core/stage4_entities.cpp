#include "th02/core/stage4_entities.hpp"
#include "stage_entities_common.hpp"

namespace th02::core {
Stage4EntitiesStepResult stage4_entities_update(StageState &s, Stage4EntitiesPresentation &p) noexcept
{
	Stage4EntitiesStepResult result;
	result.result = detail::stage_enemies_update(s, p.enemies);
	if (result.result != StageEntitiesResult::completed) return result;
	if (s.boss_enabled) {
		MarisaCombatContext context{s.frame, s.player_hit, s.player_shots, s.items,
			s.score, s.sparks, s.bullets, s.bullet_special, s.random, s.background_particles,
			p.background, s.stage_frame, s.reduce_effects, s.player_miss.playperf, s.rank,
			s.bomb.bombing, s.player_stream.shot_level, s.bomb_damage_frame_mask,
			s.player_miss.power, s.scroll.line, s.bomb_animation.slowdown_factor, s.bomb_animation.palette_tone};
		result.marisa = marisa_update(s.boss, s.marisa, context, p.marisa);
		s.enemy_target_projection.boss_position = s.boss.target;
		if (result.marisa == MarisaUpdateResult::boss) s.stage_progression = StageProgression::boss;
		else if (result.marisa == MarisaUpdateResult::stage_clear) s.stage_progression = StageProgression::clear;
		else { result.result = StageEntitiesResult::marisa_refused; return result; }
	} else s.stage_progression = StageProgression::stage;
	if (s.midboss_active != 0) {
		result.midboss = midboss4_update_and_render(s, p.midboss, p.laser_spawn);
		s.enemy_target_projection.boss_position = s.boss.target;
		if (result.midboss != Midboss2UpdateResult::completed) {
			result.result = StageEntitiesResult::midboss_refused; return result;
		}
	}
	result.result = detail::stage_entities_tail(s, p);
	return result;
}
} // namespace th02::core
