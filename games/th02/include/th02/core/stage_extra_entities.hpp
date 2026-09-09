#pragma once
#include "th02/core/stage_state.hpp"
namespace th02::core {
struct StageExtraEntitiesPresentation {
	EnemyUpdateCallbacks &enemies;
	SigmaCallbacks &sigma;
	MidbossxCallbacks &midboss;
	PlayerMissPresentationCallbacks &player_miss;
	ItemLifecyclePresentationCallbacks &items;
	LaserUpdateCallbacks &lasers;
	EnemyBulletPresentationCallbacks &bullets;
};
struct StageExtraEntitiesStepResult {
	StageEntitiesResult result{StageEntitiesResult::completed};
	SigmaUpdateResult sigma{SigmaUpdateResult::boss};
	EnemyBulletAddResult midboss{EnemyBulletAddResult::completed};
};
[[nodiscard]] StageExtraEntitiesStepResult stage_extra_entities_update(StageState &state,
	StageExtraEntitiesPresentation &presentation) noexcept;
} // namespace th02::core
