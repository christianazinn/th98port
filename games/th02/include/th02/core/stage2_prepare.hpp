#pragma once

#include "th02/core/stage1_prepare.hpp"

namespace th02::core {
struct Stage2InvalidationPresentation {
	MeiraCallbacks &meira;
	Midboss2Callbacks &midboss;
	EnemyInvalidationCallbacks &enemies;
	PlayerFrameInvalidationCallbacks &player;
	EnemyBulletInvalidationCallbacks &bullets;
	LaserInvalidationCallbacks &lasers;
	ItemScheduleCallbacks &items;
	SparkInvalidationCallbacks &sparks;
	BombPresentationCallbacks &bomb;
};
[[nodiscard]] Stage1InvalidationResult stage2_invalidate(StageState &state,
	Stage2InvalidationPresentation &presentation) noexcept;
[[nodiscard]] Stage1PreparationResult stage2_prepare(StageState &state,
	Stage2InvalidationPresentation &invalidation,
	Stage1PreparationPresentation &presentation) noexcept;
} // namespace th02::core
