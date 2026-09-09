#include "th02/core/stage1_invalidation.hpp"

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
	if(c.enemies_enabled && enemies_invalidate(c.enemies, c.frame.page_back,
		c.frame.page_front, p.enemies) != EnemyLifecycleResult::completed) {
		return Stage1InvalidationResult::enemy_invalid_page;
	}
	if(player_frame_invalidate(c.frame, c.shots, c.shot_lifetime,
		c.reduce_effects, p.player) != PlayerFrameInvalidationResult::completed) {
		return Stage1InvalidationResult::player_invalid_page;
	}
	if(bullets_invalidate(c.bullets, c.frame.page_back, c.frame.page_front,
		p.bullets) != EnemyBulletLifecycleResult::completed) {
		return Stage1InvalidationResult::bullet_invalid_page;
	}
	if(c.lasers_enabled) {
		lasers_invalidate(c.lasers, c.stage_frame, p.lasers);
	}
	if(items_invalidate(c.items, c.frame.page_back, c.frame.page_front, p.items)
		!= ItemInvalidationResult::completed) {
		return Stage1InvalidationResult::item_invalid_page;
	}
	if(!sparks_invalidate(c.sparks, c.frame.page_back, c.frame.page_front,
		p.sparks)) {
		return Stage1InvalidationResult::spark_invalid_page;
	}
	bomb_invalidate(c.bomb, c.shottype, p.bomb);
	return Stage1InvalidationResult::completed;
}

} // namespace th02::core
