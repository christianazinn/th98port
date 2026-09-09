#include "th02/core/stage4_prepare.hpp"
#include "stage_invalidation_common.hpp"
#include "stage_prepare_common.hpp"

namespace th02::core {
Stage1InvalidationResult stage4_invalidate(StageState &s, Stage4InvalidationPresentation &p) noexcept
{
	if (s.boss_enabled) {
		if (s.frame.page_back >= 2) return Stage1InvalidationResult::midboss_invalid_page;
		p.background.egc_off(); p.background.grcg_set_color(0);
		p.background.clear_boss_playfield();
		if (bg_particles_invalidate(s.background_particles, s.frame.page_back, s.frame.page_front,
			s.reduce_effects, p.background) != BgParticlePageResult::completed)
			return Stage1InvalidationResult::midboss_invalid_page;
		if (s.frame.page_front >= 2) return Stage1InvalidationResult::midboss_invalid_page;
		s.boss.topleft_on_page[s.frame.page_back] = s.boss.topleft_on_page[s.frame.page_front];
		for (std::size_t orb = 0; orb < marisa_orb_count; ++orb)
			if (s.marisa.orb_flag[orb] < 2)
				s.marisa.orb_on_page[s.frame.page_back][orb] = s.marisa.orb_on_page[s.frame.page_front][orb];
		p.background.grcg_off();
	}
	if (s.midboss_active != 0) {
		if (midboss4_invalidate(s, p.midboss) == Midboss2InvalidateResult::invalid_page)
			return Stage1InvalidationResult::midboss_invalid_page;
		s.midboss_active = s.midboss4.active;
	}
	return detail::stage_common_invalidate_state(s, p);
}
Stage1PreparationResult stage4_prepare(StageState &s, Stage4InvalidationPresentation &invalidation,
	Stage1PreparationPresentation &p) noexcept
{ return detail::stage_prepare<3>(s, invalidation, p); }
} // namespace th02::core
