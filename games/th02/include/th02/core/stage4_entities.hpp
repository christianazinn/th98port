#pragma once
#include "th02/core/stage_state.hpp"
namespace th02::core {
struct Stage4EntitiesPresentation {
	EnemyUpdateCallbacks &enemies;
	MarisaCallbacks &marisa;
	Midboss2Callbacks &midboss;
	LaserSpawnCallbacks &laser_spawn;
	BgParticleCallbacks &background;
	PlayerMissPresentationCallbacks &player_miss;
	ItemLifecyclePresentationCallbacks &items;
	LaserUpdateCallbacks &lasers;
	EnemyBulletPresentationCallbacks &bullets;
};
struct Stage4EntitiesStepResult {
	StageEntitiesResult result{StageEntitiesResult::completed};
	MarisaUpdateResult marisa{MarisaUpdateResult::boss};
	Midboss2UpdateResult midboss{Midboss2UpdateResult::completed};
};
[[nodiscard]] Stage4EntitiesStepResult stage4_entities_update(StageState &state,
	Stage4EntitiesPresentation &presentation) noexcept;
} // namespace th02::core
