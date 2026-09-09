#pragma once
#include "th02/core/stage_state.hpp"
namespace th02::core {
struct Stage5EntitiesPresentation {
	EnemyUpdateCallbacks &enemies;
	MimaCallbacks &mima;
	PlayerMissPresentationCallbacks &player_miss;
	ItemLifecyclePresentationCallbacks &items;
	LaserUpdateCallbacks &lasers;
	EnemyBulletPresentationCallbacks &bullets;
};
struct Stage5EntitiesStepResult {
	StageEntitiesResult result{StageEntitiesResult::completed};
	MimaUpdateResult mima{MimaUpdateResult::boss};
};
[[nodiscard]] Stage5EntitiesStepResult stage5_entities_update(StageState &state,
	Stage5EntitiesPresentation &presentation) noexcept;
// Resume only after the real noncontinued first-defeat dialogue has returned.
[[nodiscard]] StageEntitiesResult stage5_entities_after_form1(StageState &state,
	Stage5EntitiesPresentation &presentation) noexcept;
} // namespace th02::core
