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
template<class State, class Presentation>
Stage1InvalidationResult stage_common_invalidate_state(State &s, Presentation &p) noexcept
{
	struct CommonView {
		EnemyPoolState &enemies;
		PlayerFrameState &frame;
		PlayerShotPoolState &shots;
		PlayerShotLifetimeState &shot_lifetime;
		EnemyBulletPoolState &bullets;
		LaserPoolState &lasers;
		ItemPoolState &items;
		SparkPoolState &sparks;
		BombState &bomb;
		bool enemies_enabled, lasers_enabled, reduce_effects;
		std::uint8_t shottype;
		std::uint32_t stage_frame;
	} common{s.enemies, s.frame, s.player_shots, s.player_shot_lifetime,
		s.bullets, s.lasers, s.items, s.sparks, s.bomb, s.enemies_enabled,
		s.lasers_enabled, s.reduce_effects, s.shottype, s.stage_frame};
	return stage_common_invalidate(common, p);
}
} // namespace th02::core::detail
