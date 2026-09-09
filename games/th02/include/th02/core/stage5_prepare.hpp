#pragma once
#include "th02/core/stage3_prepare.hpp"
#include "th02/core/mima.hpp"
namespace th02::core {
struct Stage5InvalidationPresentation {
	MimaCallbacks &mima;
	EnemyInvalidationCallbacks &enemies;
	PlayerFrameInvalidationCallbacks &player;
	EnemyBulletInvalidationCallbacks &bullets;
	LaserInvalidationCallbacks &lasers;
	ItemScheduleCallbacks &items;
	SparkInvalidationCallbacks &sparks;
	BombPresentationCallbacks &bomb;
};
[[nodiscard]] Stage1InvalidationResult stage5_invalidate(StageState &state,
	Stage5InvalidationPresentation &presentation) noexcept;
[[nodiscard]] Stage1PreparationResult stage5_prepare(StageState &state,
	Stage5InvalidationPresentation &invalidation, Stage1PreparationPresentation &presentation) noexcept;
} // namespace th02::core
