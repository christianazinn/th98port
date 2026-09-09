#include "th02/core/stage1_invalidation.hpp"
#include "stage_invalidation_common.hpp"

namespace th02::core {

Stage1InvalidationResult stage1_invalidate(Stage1InvalidationContext &c,
	Stage1InvalidationPresentation &p) noexcept
{
	if(c.boss_enabled && rika_bg_render(c.boss, c.rika, c.frame, p.rika)
		!= RikaBackgroundResult::completed) {
		return Stage1InvalidationResult::boss_invalid_page;
	}
	if(c.midboss_active != 0) {
		if(midboss1_invalidate(c.boss, c.frame, c.midboss, p.midboss)
			== Midboss1InvalidateResult::invalid_page) {
			return Stage1InvalidationResult::midboss_invalid_page;
		}
		// The source returns its raw bool16, not a canonical 0/1 conversion.
		c.midboss_active = c.midboss.active;
	}
	return detail::stage_common_invalidate(c, p);
}

} // namespace th02::core
