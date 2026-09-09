#pragma once

#include "th02/core/stage1_invalidation.hpp"

namespace th02::core::detail {
// Shared contiguous source suffix after the installed boss/midboss calls.
// Contexts only borrow owners; P contains presentation callbacks, not gameplay.
template<class C, class P>
Stage1InvalidationResult stage_common_invalidate(C &c, P &p) noexcept
{
	if(c.enemies_enabled && enemies_invalidate(c.enemies, c.frame.page_back,
		c.frame.page_front, p.enemies) != EnemyLifecycleResult::completed)
		return Stage1InvalidationResult::enemy_invalid_page;
	if(player_frame_invalidate(c.frame, c.shots, c.shot_lifetime,
		c.reduce_effects, p.player) != PlayerFrameInvalidationResult::completed)
		return Stage1InvalidationResult::player_invalid_page;
	if(bullets_invalidate(c.bullets, c.frame.page_back, c.frame.page_front,
		p.bullets) != EnemyBulletLifecycleResult::completed)
		return Stage1InvalidationResult::bullet_invalid_page;
	if(c.lasers_enabled) lasers_invalidate(c.lasers, c.stage_frame, p.lasers);
	if(items_invalidate(c.items, c.frame.page_back, c.frame.page_front, p.items)
		!= ItemInvalidationResult::completed)
		return Stage1InvalidationResult::item_invalid_page;
	if(!sparks_invalidate(c.sparks, c.frame.page_back, c.frame.page_front, p.sparks))
		return Stage1InvalidationResult::spark_invalid_page;
	bomb_invalidate(c.bomb, c.shottype, p.bomb);
	return Stage1InvalidationResult::completed;
}
} // namespace th02::core::detail
