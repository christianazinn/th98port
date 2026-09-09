#include "th02/core/stage_extra_entities.hpp"
#include "stage_entities_common.hpp"
namespace th02::core {
StageExtraEntitiesStepResult stage_extra_entities_update(StageState &s, StageExtraEntitiesPresentation &p) noexcept
{
	StageExtraEntitiesStepResult result;
	result.result = detail::stage_enemies_update(s, p.enemies);
	if (result.result != StageEntitiesResult::completed) return result;
	if (s.boss_enabled) {
		result.sigma = sigma_update(s, p.sigma);
		s.enemy_target_projection.boss_position = s.boss.target;
		if (result.sigma == SigmaUpdateResult::boss) s.stage_progression = StageProgression::boss;
		else if (result.sigma == SigmaUpdateResult::stage_clear) s.stage_progression = StageProgression::clear;
		else { result.result = StageEntitiesResult::sigma_refused; return result; }
	} else s.stage_progression = StageProgression::stage;
	if (s.midboss_active) {
		result.midboss = midbossx_update_and_render(s, p.midboss);
		if (result.midboss != EnemyBulletAddResult::completed) { result.result = StageEntitiesResult::midboss_refused; return result; }
	}
	result.result = detail::stage_entities_tail(s, p);
	return result;
}
} // namespace th02::core
