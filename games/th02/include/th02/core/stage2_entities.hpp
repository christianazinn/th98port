#pragma once

#include "th02/core/meira.hpp"
#include "th02/core/midboss2.hpp"
#include "th02/core/stage1_entities.hpp"

#include <cstdint>

namespace th02::core {

struct Stage2EntitiesStepResult {
	Stage2EntitiesResult result{Stage2EntitiesResult::completed};
	EnemyUpdateResult enemy{EnemyUpdateResult::completed};
	MeiraUpdateResult meira{MeiraUpdateResult::boss};
	Midboss2UpdateResult midboss{Midboss2UpdateResult::completed};
	PlayerUpdateResult player{PlayerUpdateResult::completed};
	ItemLifecycleResult items{ItemLifecycleResult::completed};
	EnemyBulletUpdateResult bullets{EnemyBulletUpdateResult::completed};
	bool sparks_updated{};
	bool operator==(const Stage2EntitiesStepResult &) const = default;
};

// These are the existing synchronous presentation boundaries reached by the
// Stage 2 loop. The aggregate owns no gameplay state and supplies no simulated
// callback result.
struct Stage2EntitiesPresentation {
	EnemyUpdateCallbacks &enemies;
	MeiraCallbacks &meira;
	Midboss2Callbacks &midboss;
	PlayerMissPresentationCallbacks &player_miss;
	ItemLifecyclePresentationCallbacks &items;
	LaserUpdateCallbacks &lasers;
	EnemyBulletPresentationCallbacks &bullets;
};

// One source-ordered Stage 2 entity pass. BossState::target is the donor's
// canonical boss_pos_x/y pair; enemy_position is updated after each reached
// owner that writes it. midboss_active deliberately remains a raw bool16 word.
struct Stage2EntitiesContext {
	bool enemies_enabled{};
	EnemyPoolState &enemies;
	EnemyUpdateState &enemy_position;
	EnemyUpdateContext &enemy_context;
	EnemyUpdateCallbacks &enemy_callbacks;

	BossState &boss;
	bool meira_active{};
	MeiraState &meira;
	MeiraCombatContext &meira_context;
	MeiraCallbacks &meira_callbacks;

	std::int16_t &midboss_active;
	Midboss2State &midboss;
	Midboss2Context &midboss_context;
	Midboss2Callbacks &midboss_callbacks;

	BombState &bomb;
	PlayerMissState &player_miss;
	PlayerHitState &player_hit;
	bool &quit;
	PlayerShotPoolState &player_shots;
	PlayerShotLifetimeState &player_shot_lifetime;
	PlayerMissUpdateCallbacks &player_miss_callbacks;

	ItemPoolState &items;
	ItemLifecycleState &item_lifecycle;
	PlayerShotStreamState &player_stream;
	ScoreState &score;
	ItemLifecyclePresentationCallbacks &item_callbacks;

	bool &lasers_enabled;
	LaserPoolState &lasers;
	LaserUpdateCallbacks &laser_callbacks;

	EnemyBulletPoolState &bullets;
	EnemyBulletSpecialState &bullet_special;
	EnemyBulletPresentationCallbacks &bullet_callbacks;

	SparkPoolState &sparks;
	SparkUpdateParameters &spark_parameters;
	PlayerFrameState &player_frame;
	std::uint32_t &stage_frame;
	StageProgression &stage_progression;
};

// Contiguous Stage-2 counterpart of stage_loop()'s entity portion: enemies,
// installed Meira, active M2, player/miss, items, enabled lasers, bullets,
// and sparks. The caller owns the surrounding pause, scroll, invalidation,
// rendering, and page-flip passes.
[[nodiscard]] Stage2EntitiesStepResult stage2_entities_update(
	Stage2EntitiesContext &context
) noexcept;

} // namespace th02::core
