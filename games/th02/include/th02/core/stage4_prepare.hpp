#pragma once
#include "th02/core/stage3_prepare.hpp"
namespace th02::core {
using Stage4InvalidationPresentation = Stage3InvalidationPresentation;
[[nodiscard]] Stage1InvalidationResult stage4_invalidate(StageState &state,
	Stage4InvalidationPresentation &presentation) noexcept;
[[nodiscard]] Stage1PreparationResult stage4_prepare(StageState &state,
	Stage4InvalidationPresentation &invalidation, Stage1PreparationPresentation &presentation) noexcept;
} // namespace th02::core
