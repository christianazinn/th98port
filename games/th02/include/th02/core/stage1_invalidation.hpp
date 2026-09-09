#pragma once

#include "th02/core/bomb.hpp"
#include "th02/core/enemy_lifecycle.hpp"
#include "th02/core/item.hpp"
#include "th02/core/midboss1.hpp"
#include "th02/core/rika.hpp"

namespace th02::core {

struct Stage1InvalidationContext {
	BossState &boss;
	const RikaState &rika;
	const Midboss1State &midboss;
	std::int16_t &midboss_active;
	EnemyPoolState &enemies;
	PlayerFrameState &frame;
	PlayerShotPoolState &shots;
	PlayerShotLifetimeState &shot_lifetime;
	EnemyBulletPoolState &bullets;
	LaserPoolState &lasers;
	ItemPoolState &items;
	SparkPoolState &sparks;
	BombState &bomb;
	bool boss_enabled{};
	bool enemies_enabled{};
	bool lasers_enabled{};
	bool reduce_effects{};
	std::uint8_t shottype{};
	std::uint32_t stage_frame{};
};

struct Stage1InvalidationPresentation {
	RikaCallbacks &rika;
	Midboss1Callbacks &midboss;
	EnemyInvalidationCallbacks &enemies;
	PlayerFrameInvalidationCallbacks &player;
	EnemyBulletInvalidationCallbacks &bullets;
	LaserInvalidationCallbacks &lasers;
	ItemScheduleCallbacks &items;
	SparkInvalidationCallbacks &sparks;
	BombPresentationCallbacks &bomb;
};

enum class Stage1InvalidationResult : std::uint8_t {
	completed, boss_invalid_page, midboss_invalid_page, enemy_invalid_page,
	player_invalid_page, bullet_invalid_page, item_invalid_page,
	spark_invalid_page,
};

// stage_loop's boss_bg_render through bomb_invalidate region, calling the
// real bodies. The enclosing caller must install the back page's OLD scroll
// line for its presentation sink before entering. This does not spawn enemies,
// advance scroll, draw scenery or flip pages. Refusals retain reached prefixes.
[[nodiscard]] Stage1InvalidationResult stage1_invalidate(
	Stage1InvalidationContext &context,
	Stage1InvalidationPresentation &presentation) noexcept;

} // namespace th02::core
