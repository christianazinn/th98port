#include "th02/core/stage2_prepare.hpp"
#include "stage_invalidation_common.hpp"
#include "stage_prepare_common.hpp"

namespace th02::core {
Stage1InvalidationResult stage2_invalidate(StageState &s,
	Stage2InvalidationPresentation &p) noexcept
{
	if (s.boss_enabled && !meira_bg_render(s.boss, s.meira, s.frame, p.meira))
		return Stage1InvalidationResult::boss_invalid_page;
	if (s.midboss_active != 0) {
		if (midboss2_invalidate(s.boss, s.frame, s.midboss2, p.midboss) == Midboss2InvalidateResult::invalid_page)
			return Stage1InvalidationResult::midboss_invalid_page;
		s.midboss_active = s.midboss2.active;
	}
	return detail::stage_common_invalidate_state(s, p);
}
Stage1PreparationResult stage2_prepare(StageState &state,
	Stage2InvalidationPresentation &invalidation,
	Stage1PreparationPresentation &presentation) noexcept
{
	return detail::stage_prepare<true>(state, invalidation, presentation);
}
} // namespace th02::core
