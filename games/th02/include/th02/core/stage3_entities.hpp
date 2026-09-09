#pragma once
#include "th02/core/stage_state.hpp"

namespace th02::core {
struct Stage3EntitiesPresentation {
	EnemyUpdateCallbacks &enemies;
	StonesCallbacks &stones;
	Midboss2Callbacks &midboss;
	PlayerMissPresentationCallbacks &player_miss;
	ItemLifecyclePresentationCallbacks &items;
	LaserUpdateCallbacks &lasers;
	EnemyBulletPresentationCallbacks &bullets;
};
struct Stage3EntitiesStepResult {
	StageEntitiesResult result{StageEntitiesResult::completed};
	StonesUpdateResult stones{StonesUpdateResult::boss};
	Midboss2UpdateResult midboss{Midboss2UpdateResult::completed};
};
[[nodiscard]] Stage3EntitiesStepResult stage3_entities_update(StageState &state,
	Stage3EntitiesPresentation &presentation) noexcept;
} // namespace th02::core
