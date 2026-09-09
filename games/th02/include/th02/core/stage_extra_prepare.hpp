#pragma once
#include "th02/core/stage5_prepare.hpp"
#include "th02/core/sigma.hpp"
namespace th02::core {
struct StageExtraInvalidationPresentation {
	SigmaCallbacks &sigma;
	EnemyInvalidationCallbacks &enemies;
	PlayerFrameInvalidationCallbacks &player;
	EnemyBulletInvalidationCallbacks &bullets;
	LaserInvalidationCallbacks &lasers;
	ItemScheduleCallbacks &items;
	SparkInvalidationCallbacks &sparks;
	BombPresentationCallbacks &bomb;
};
[[nodiscard]] Stage1InvalidationResult stage_extra_invalidate(StageState &state,
	StageExtraInvalidationPresentation &presentation) noexcept;
[[nodiscard]] Stage1PreparationResult stage_extra_prepare(StageState &state,
	StageExtraInvalidationPresentation &invalidation, Stage1PreparationPresentation &presentation) noexcept;
} // namespace th02::core
