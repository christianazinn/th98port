#pragma once

#include "th02/core/player_bomb_update.hpp"
#include "th02/core/stage_state_fwd.hpp"
#include "th02/core/meira.hpp"
#include "th02/core/midboss2.hpp"
#include "th02/core/midboss3.hpp"
#include "th02/core/midboss4.hpp"
#include "th02/core/midbossx.hpp"
#include "th02/core/marisa.hpp"
#include "th02/core/mima.hpp"
#include "th02/core/sigma.hpp"
#include "th02/core/bg_particle.hpp"
#include "th02/core/stones.hpp"
#include "th02/core/stage3_effect.hpp"
#include "th02/core/dialog.hpp"
#include "th02/core/reset.hpp"
#include "th02/core/scroll.hpp"
#include "th02/core/stage1_entities.hpp"
#include "th02/core/stage2_entities.hpp"
#include "th02/core/stage1_invalidation.hpp"
#include "th02/core/stage_bonus.hpp"
#include "th02/core/stage_scenery.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

// One aggregate of existing TH02 stage-global owners. It has no
// reset, constructor, or factory that purports to establish a donor-valid
// stage: stage_init, continuation, dialogue, BGM, and the pending scheduler
// phases each retain their own exact setup contracts. Page ownership remains
// solely in `frame`; current scrolling remains solely in `scroll`.
// Boss-private fields persist across stage transitions just as the source
// globals do; switching the active boss must not destroy another boss's state.
struct StageState {
	DialogState dialog{};
	PlayerFrameState frame{};
	PlayerHitState player_hit{};
	PlayerMissState player_miss{};
	PlayerResetState player_reset{};
	MovementSpeeds player_speeds{};
	PlayerShotStreamState player_stream{};
	PlayerShotPoolState player_shots{};
	PlayerShotLifetimeState player_shot_lifetime{};
	PlayerShotVolleyContext player_volley{};

	BombState bomb{};
	BombAnimationState bomb_animation{};
	BombEffectState bomb_effects{};
	ScoreState score{};
	RandomState random{};
	ItemPoolState items{};
	ItemLifecycleState item_lifecycle{};
	SparkPoolState sparks{};
	SparkUpdateParameters spark_parameters{};
	std::uint8_t spark_sprite_interval{};
	EnemyBulletPoolState bullets{};
	EnemyBulletSpecialState bullet_special{};
	LaserPoolState lasers{};
	EnemyPoolState enemies{};
	// Adapter projection only. BossState::target is the canonical source pair.
	EnemyUpdateState enemy_target_projection{};
	EnemyStageData stage_data{};
	BossState boss{};
	RikaState rika{};
	Midboss1State midboss{};
	MeiraState meira{};
	Midboss2State midboss2{};
	Midboss3State midboss3{};
	Midboss4State midboss4{};
	MidbossxState midbossx{};
	MarisaState marisa{};
	MimaState mima{};
	SigmaState sigma{};
	BgParticleState background_particles{};
	std::int16_t stage4_tile_top{};
	StonesState stones{};
	Stage3EffectState stage3_effect{};
	std::uint16_t stage2_scenery_frame{};
	std::uint8_t stage2_scenery_setup{};

	ScrollState scroll{};
	// The old scroll line retained for each physical page is distinct from the
	// current line in ScrollState and is consumed by the invalidation pass.
	std::array<std::int16_t, 2U> scroll_line_on_page{};
	TileScrollState tiles{};
	TileSectionTiles tile_sections{};
	std::uint8_t tile_mode{};
	bool tiles_egc_render_all{};
	StageSceneryPoolState scenery{};
	StageBgFlashState stage_bg_flash{};
	StageClearBonusTally stage_clear_bonus{};

	// Stage-loop/resident scalar owners. `resident_frame` is the signed source
	// long, while `stage_frame` is the distinct unsigned source dword.
	std::int32_t resident_frame{};
	std::int32_t resident_score_highest{};
	std::uint32_t stage_frame{};
	std::uint16_t input{};
	std::int8_t rank{};
	std::int16_t playperf_max{};
	std::uint8_t shottype{};
	std::uint8_t bomb_damage_frame_mask{};
	bool reduce_effects{};
	bool quit{};
	// This is the stage-loop's raw bool16, distinct from Midboss1State::active.
	std::int16_t midboss_active{};
	std::int16_t midboss_scroll_step{};
	bool boss_enabled{};
	bool enemies_enabled{};
	bool lasers_enabled{};
	BombSelection bomb_selection{BombSelection::reimu_a};
	StageProgression stage_progression{StageProgression::stage};
	std::size_t max_enemy_vm_steps{};

	// Presentation metadata is source state, but no text/BGM render or title
	// lifecycle is implied by retaining these byte-width fields.
	std::uint8_t bgm_show_timer{};
	std::uint8_t bgm_title_id{};
	std::uint8_t boss_bgm_title_id{};
	std::uint8_t stage_id{};
	std::uint8_t stage_title_halflen{};
	std::uint8_t start_lives{};
	std::uint16_t continues_used{};
};

// The only new pass-level callback aggregate. Each member is an existing
// synchronous presentation interface; the aggregate owns no gameplay state.
struct Stage1EntitiesPresentation {
	EnemyUpdateCallbacks &enemies;
	RikaCallbacks &rika;
	Midboss1Callbacks &midboss;
	PlayerMissPresentationCallbacks &player_miss;
	ItemLifecyclePresentationCallbacks &items;
	LaserUpdateCallbacks &lasers;
	EnemyBulletPresentationCallbacks &bullets;
};

// These overloads assemble the pre-existing narrow contexts on their own
// stack and immediately dispatch the matching existing body. No context with
// borrowed references can outlive its call, and no state is reset, advanced,
// or otherwise initialized merely by forming an adapter.
[[nodiscard]] Stage1InvalidationResult stage1_invalidate(
	Stage1State &state, Stage1InvalidationPresentation &presentation
) noexcept;

[[nodiscard]] PlayerBombUpdateOutcome player_move_and_bomb_update(
	Stage1State &state, BombAnimationCallbacks &animation_callbacks,
	BombPresentationCallbacks &bomb_callbacks
) noexcept;

[[nodiscard]] Stage1EntitiesStepResult stage1_entities_update(
	Stage1State &state, Stage1EntitiesPresentation &presentation
) noexcept;

[[nodiscard]] Stage2EntitiesStepResult stage2_entities_update(
	StageState &state, Stage2EntitiesPresentation &presentation
) noexcept;

} // namespace th02::core
