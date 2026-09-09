#pragma once

#include "th02/core/bomb.hpp"
#include "th02/core/bullet.hpp"
#include "th02/core/enemy_update.hpp"
#include "th02/core/item.hpp"
#include "th02/core/laser_update.hpp"
#include "th02/core/midboss1.hpp"
#include "th02/core/rika.hpp"
#include "th02/core/spark.hpp"

#include <cstdint>

namespace th02::core {

// Source-sized stage_progression_t values returned by the installed boss
// update slot. This narrow Stage-1 owner has no stage-end or page scheduling.
enum class StageProgression : std::int16_t {
	stage = 0,
	boss = 1,
	clear = 2,
};

// A normal result retains the source body result for every reached owner.
// Any `*_refused` outcome is a typed inner refusal at its source call site;
// no following source call has run.
enum class StageEntitiesResult : std::uint8_t {
	completed,
	enemy_refused,
	rika_refused,
	meira_refused,
	midboss_refused,
	player_refused,
	items_refused,
	bullets_refused,
	sparks_refused,
};

using Stage1EntitiesResult = StageEntitiesResult;
using Stage2EntitiesResult = StageEntitiesResult;

struct Stage1EntitiesStepResult {
	Stage1EntitiesResult result{Stage1EntitiesResult::completed};
	EnemyUpdateResult enemy{EnemyUpdateResult::completed};
	RikaUpdateResult rika{RikaUpdateResult::boss};
	Midboss1UpdateResult midboss{Midboss1UpdateResult::completed};
	PlayerUpdateResult player{PlayerUpdateResult::completed};
	ItemLifecycleResult items{ItemLifecycleResult::completed};
	EnemyBulletUpdateResult bullets{EnemyBulletUpdateResult::completed};
	bool sparks_updated{};
	bool operator==(const Stage1EntitiesStepResult &) const = default;
};

// All fields are existing owners or source-global adapters. `boss.target` is
// authoritative for the donor's boss_pos_x/y pair; enemy_position is a
// synchronized projection used only by enemies_update_and_render(). The
// caller supplies a single underlying stage-frame counter to both this context
// and RikaCombatContext; this struct never owns a second copy.
struct Stage1EntitiesContext {
	bool enemies_enabled{};
	EnemyPoolState &enemies;
	EnemyUpdateState &enemy_position;
	EnemyUpdateContext &enemy_context;
	EnemyUpdateCallbacks &enemy_callbacks;

	BossState &boss;
	bool rika_active{};
	RikaState &rika;
	RikaCombatContext &rika_context;
	RikaCallbacks &rika_callbacks;

	bool midboss_active{};
	Midboss1State &midboss;
	Midboss1Context &midboss_context;
	Midboss1Callbacks &midboss_callbacks;

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

// The common suffix of each ordinary stage entity pass is deliberately kept
// private to its implementation. Stage 2 supplies its own boss and midboss
// owners, declared in stage2_entities.hpp, before reaching this same tail.

// Contiguous stock-gameplay portion of th02/main/stage/loop.cpp after the
// player/bomb prefix: enabled enemies, installed boss, active M1, player
// state/miss body, items, enabled lasers, bullets, then spark state update.
// The patched replay_rank_lock_apply() hook between player and items is
// deliberately absent: practice replay rank-lock integration is not a stock
// gameplay owner. Pause, scroll, invalidation, render tails, and page flips
// remain with their respective frame/lifecycle owners.
[[nodiscard]] Stage1EntitiesStepResult stage1_entities_update(
	Stage1EntitiesContext &context
) noexcept;

} // namespace th02::core
