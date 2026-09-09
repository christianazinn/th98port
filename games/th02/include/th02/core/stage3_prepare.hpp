#pragma once
#include "th02/core/stage2_prepare.hpp"

namespace th02::core {
struct Stage3InvalidationPresentation {
	StageSceneryCallbacks &stones;
	Midboss2Callbacks &midboss;
	EnemyInvalidationCallbacks &enemies;
	PlayerFrameInvalidationCallbacks &player;
	EnemyBulletInvalidationCallbacks &bullets;
	LaserInvalidationCallbacks &lasers;
	ItemScheduleCallbacks &items;
	SparkInvalidationCallbacks &sparks;
	BombPresentationCallbacks &bomb;
};
[[nodiscard]] Stage1InvalidationResult stage3_invalidate(StageState &state,
	Stage3InvalidationPresentation &presentation) noexcept;
[[nodiscard]] Stage1PreparationResult stage3_prepare(StageState &state,
	Stage3InvalidationPresentation &invalidation, Stage1PreparationPresentation &presentation) noexcept;
} // namespace th02::core
