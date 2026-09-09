#include "th02/core/stage5_entities.hpp"
#include "stage_entities_common.hpp"
namespace th02::core {
Stage5EntitiesStepResult stage5_entities_update(StageState &s, Stage5EntitiesPresentation &p) noexcept
{
	Stage5EntitiesStepResult result;
	result.result = detail::stage_enemies_update(s, p.enemies);
	if (result.result != StageEntitiesResult::completed) return result;
	if (s.boss_enabled) {
		result.mima = mima_update(s, p.mima);
		s.enemy_target_projection.boss_position = s.boss.target;
		if (result.mima == MimaUpdateResult::form1_defeat_pending || result.mima == MimaUpdateResult::final_end_pending) return result;
		if (result.mima != MimaUpdateResult::boss) { result.result = StageEntitiesResult::mima_refused; return result; }
		s.stage_progression = StageProgression::boss;
	} else s.stage_progression = StageProgression::stage;
	result.result = detail::stage_entities_tail(s, p);
	return result;
}
StageEntitiesResult stage5_entities_after_form1(StageState &s, Stage5EntitiesPresentation &p) noexcept
{
	mima_after_form1_defeat(s, p.mima);
	s.stage_progression = StageProgression::boss;
	s.enemy_target_projection.boss_position = s.boss.target;
	return detail::stage_entities_tail(s, p);
}
} // namespace th02::core
