#include "th02/core/stage3_prepare.hpp"
#include "stage_invalidation_common.hpp"
#include "stage_prepare_common.hpp"

namespace th02::core {
Stage1InvalidationResult stage3_invalidate(StageState &s,
	Stage3InvalidationPresentation &p) noexcept
{
	if (s.boss_enabled) {
		for (std::size_t i = 0; i < stone_count; ++i) {
			if (s.stones.flag[i] < 4) {
				p.background.invalidate_scenery({s.stones.topleft[i], 32, 32});
				if (s.stones.flag[i] == 3) s.stones.flag[i] = 4;
			}
		}
	}
	if (s.midboss_active != 0) {
		const auto result = midboss3_invalidate(s.midboss3,
			{s.stones.flag, s.stones.damage, s.stones.patnum}, s.frame, p.midboss);
		if (result == Midboss2InvalidateResult::invalid_page)
			return Stage1InvalidationResult::midboss_invalid_page;
		s.midboss_active = result == Midboss2InvalidateResult::active ? 1 : 0;
	}
	return detail::stage_common_invalidate_state(s, p);
}
Stage1PreparationResult stage3_prepare(StageState &state,
	Stage3InvalidationPresentation &invalidation,
	Stage1PreparationPresentation &presentation) noexcept
{
	return detail::stage_prepare<2>(state, invalidation, presentation);
}
} // namespace th02::core
