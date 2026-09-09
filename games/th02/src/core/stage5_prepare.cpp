#include "th02/core/stage5_prepare.hpp"
#include "stage_invalidation_common.hpp"
#include "stage_prepare_common.hpp"
namespace th02::core {
Stage1InvalidationResult stage5_invalidate(StageState &s, Stage5InvalidationPresentation &p) noexcept
{
	if (s.boss_enabled && mima_bg_render(s, p.mima) != MimaUpdateResult::boss)
		return Stage1InvalidationResult::midboss_invalid_page;
	return detail::stage_common_invalidate_state(s, p);
}
Stage1PreparationResult stage5_prepare(StageState &s, Stage5InvalidationPresentation &i,
	Stage1PreparationPresentation &p) noexcept
{ return detail::stage_prepare<4>(s, i, p); }
} // namespace th02::core
