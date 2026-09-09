#include "th02/core/stage_extra_prepare.hpp"
#include "stage_invalidation_common.hpp"
#include "stage_prepare_common.hpp"
namespace th02::core {
Stage1InvalidationResult stage_extra_invalidate(StageState &s, StageExtraInvalidationPresentation &p) noexcept
{
	if (s.boss_enabled && sigma_bg_render(s, p.sigma) != SigmaUpdateResult::boss)
		return Stage1InvalidationResult::midboss_invalid_page;
	if (s.midboss_active) s.midboss_active = s.midbossx.active ? 1 : 0;
	return detail::stage_common_invalidate_state(s, p);
}
Stage1PreparationResult stage_extra_prepare(StageState &s, StageExtraInvalidationPresentation &i,
	Stage1PreparationPresentation &p) noexcept
{ return detail::stage_prepare<5>(s, i, p); }
} // namespace th02::core
