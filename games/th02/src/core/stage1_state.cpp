#include "th02/core/stage1_state.hpp"

namespace th02::core {
namespace {

enum class StageDataProjectionResult : std::uint8_t {
	valid,
	invalid_template_count,
	invalid_script_count,
};

[[nodiscard]] StageDataProjectionResult stage_data_projection_valid(
	const Stage1State &state
) noexcept
{
	if(state.stage_data.template_count < 0 ||
		static_cast<std::size_t>(state.stage_data.template_count) >
		state.stage_data.templates.size()) {
		return StageDataProjectionResult::invalid_template_count;
	}
	if(state.stage_data.script_count < 0 ||
		static_cast<std::size_t>(state.stage_data.script_count) >
		state.stage_data.scripts.size()) {
		return StageDataProjectionResult::invalid_script_count;
	}
	return StageDataProjectionResult::valid;
}

[[nodiscard]] EnemyUpdateScriptData enemy_script_data(
	Stage1State &state
) noexcept
{
	const auto template_count = static_cast<std::size_t>(
		state.stage_data.template_count
	);
	const auto script_count = static_cast<std::size_t>(state.stage_data.script_count);
	return {
		std::span{state.stage_data.templates}.first(template_count),
		std::span{state.stage_data.scripts}.first(script_count),
		std::span{state.stage_data.script_lengths}.first(script_count),
	};
}

} // namespace

Stage1InvalidationResult stage1_invalidate(
	Stage1State &state, Stage1InvalidationPresentation &presentation
) noexcept
{
	Stage1InvalidationContext context{
		state.boss,
		state.rika,
		state.midboss,
		state.midboss_active,
		state.enemies,
		state.frame,
		state.player_shots,
		state.player_shot_lifetime,
		state.bullets,
		state.lasers,
		state.items,
		state.sparks,
		state.bomb,
		state.boss_enabled,
		state.enemies_enabled,
		state.lasers_enabled,
		state.reduce_effects,
		state.shottype,
		state.stage_frame,
	};
	return stage1_invalidate(context, presentation);
}

PlayerBombUpdateOutcome player_move_and_bomb_update(
	Stage1State &state, BombAnimationCallbacks &animation_callbacks,
	BombPresentationCallbacks &bomb_callbacks
) noexcept
{
	PlayerBombUpdateContext context{
		state.frame,
		state.boss,
		state.player_hit,
		state.player_speeds,
		state.input,
		state.player_stream,
		state.player_shots,
		state.player_shot_lifetime,
		state.player_volley,
		state.random,
		state.bomb,
		state.score,
		state.bullets,
		state.sparks,
		state.bomb_animation,
		state.bomb_effects,
		state.bomb_selection,
		state.scroll.line,
		state.reduce_effects,
	};
	return player_move_and_bomb_update(context, animation_callbacks,
		bomb_callbacks);
}

Stage1EntitiesStepResult stage1_entities_update(
	Stage1State &state, Stage1EntitiesPresentation &presentation
) noexcept
{
	if(state.enemies_enabled) {
		switch(stage_data_projection_valid(state)) {
		case StageDataProjectionResult::valid:
			break;
		case StageDataProjectionResult::invalid_template_count:
			return {Stage1EntitiesResult::enemy_refused,
				EnemyUpdateResult::invalid_template};
		case StageDataProjectionResult::invalid_script_count:
			return {Stage1EntitiesResult::enemy_refused,
				EnemyUpdateResult::invalid_script};
		}
	}

	EnemyRunContext enemy_run{
		state.bullets,
		state.random,
		state.frame,
		state.player_miss.playperf,
		state.rank,
		state.bomb.bombing,
		state.scroll.delta,
		state.scroll.line,
	};
	EnemyUpdateContext enemy_context{
		// The null enemy callback cannot read stage data. Empty spans make that
		// fact explicit until an enabled pass has validated the decoder counts.
		state.enemies_enabled ? enemy_script_data(state) : EnemyUpdateScriptData{},
		enemy_run,
		state.player_hit,
		state.player_shots,
		state.player_stream,
		state.score,
		state.items,
		state.sparks,
		state.stage_frame,
		state.bomb_damage_frame_mask,
		state.max_enemy_vm_steps,
	};
	RikaCombatContext rika_context{
		state.frame,
		state.player_hit,
		state.player_shots,
		state.score,
		state.sparks,
		state.bullets,
		state.lasers,
		state.random,
		state.stage_frame,
		state.player_miss.playperf,
		state.rank,
		state.bomb.bombing,
		state.player_stream.shot_level,
		state.bomb_damage_frame_mask,
		state.scroll.line,
		state.bomb_animation.slowdown_factor,
		state.bomb_animation.palette_tone,
	};
	Midboss1Context midboss_context{
		state.frame,
		state.player_shots,
		state.score,
		state.sparks,
		state.bullets,
		state.random,
		state.player_miss.playperf,
		state.rank,
		state.bomb.bombing,
		state.player_stream.shot_level,
		state.bomb_damage_frame_mask,
		state.stage_frame,
		state.scroll.line,
		state.scroll.step,
	};
	PlayerMissUpdateCallbacks player_miss_callbacks{
		state.player_miss,
		state.frame,
		state.player_stream,
		state.score,
		state.items,
		state.sparks,
		state.random,
		presentation.player_miss,
	};
	Stage1EntitiesContext context{
		state.enemies_enabled,
		state.enemies,
		state.enemy_target_projection,
		enemy_context,
		presentation.enemies,
		state.boss,
		state.boss_enabled,
		state.rika,
		rika_context,
		presentation.rika,
		state.midboss_active != 0,
		state.midboss,
		midboss_context,
		presentation.midboss,
		state.bomb,
		state.player_miss,
		state.player_hit,
		state.quit,
		state.player_shots,
		state.player_shot_lifetime,
		player_miss_callbacks,
		state.items,
		state.item_lifecycle,
		state.player_stream,
		state.score,
		presentation.items,
		state.lasers_enabled,
		state.lasers,
		presentation.lasers,
		state.bullets,
		state.bullet_special,
		presentation.bullets,
		state.sparks,
		state.spark_parameters,
		state.frame,
		state.stage_frame,
		state.stage_progression,
	};
	return stage1_entities_update(context);
}

Stage2EntitiesStepResult stage2_entities_update(
	StageState &state, Stage2EntitiesPresentation &presentation
) noexcept
{
	if(state.enemies_enabled) {
		switch(stage_data_projection_valid(state)) {
		case StageDataProjectionResult::valid:
			break;
		case StageDataProjectionResult::invalid_template_count:
			return {Stage2EntitiesResult::enemy_refused,
				EnemyUpdateResult::invalid_template};
		case StageDataProjectionResult::invalid_script_count:
			return {Stage2EntitiesResult::enemy_refused,
				EnemyUpdateResult::invalid_script};
		}
	}

	EnemyRunContext enemy_run{
		state.bullets,
		state.random,
		state.frame,
		state.player_miss.playperf,
		state.rank,
		state.bomb.bombing,
		state.scroll.delta,
		state.scroll.line,
	};
	EnemyUpdateContext enemy_context{
		state.enemies_enabled ? enemy_script_data(state) : EnemyUpdateScriptData{},
		enemy_run,
		state.player_hit,
		state.player_shots,
		state.player_stream,
		state.score,
		state.items,
		state.sparks,
		state.stage_frame,
		state.bomb_damage_frame_mask,
		state.max_enemy_vm_steps,
	};
	MeiraCombatContext meira_context{
		state.frame,
		state.player_hit,
		state.player_shots,
		state.score,
		state.sparks,
		state.bullets,
		state.bullet_special,
		state.random,
		state.stage_frame,
		state.player_miss.playperf,
		state.rank,
		state.bomb.bombing,
		state.player_stream.shot_level,
		state.bomb_damage_frame_mask,
		state.scroll.line,
		state.bomb_animation.slowdown_factor,
		state.bomb_animation.palette_tone,
	};
	Midboss2Context midboss_context{
		state.frame,
		state.player_shots,
		state.score,
		state.sparks,
		state.bullets,
		state.random,
		state.stage_bg_flash,
		state.player_miss.playperf,
		state.rank,
		state.bomb.bombing,
		state.player_stream.shot_level,
		state.bomb_damage_frame_mask,
		state.stage_frame,
		state.scroll.line,
	};
	PlayerMissUpdateCallbacks player_miss_callbacks{
		state.player_miss,
		state.frame,
		state.player_stream,
		state.score,
		state.items,
		state.sparks,
		state.random,
		presentation.player_miss,
	};
	Stage2EntitiesContext context{
		state.enemies_enabled,
		state.enemies,
		state.enemy_target_projection,
		enemy_context,
		presentation.enemies,
		state.boss,
		state.boss_enabled,
		state.meira,
		meira_context,
		presentation.meira,
		state.midboss_active,
		state.midboss2,
		midboss_context,
		presentation.midboss,
		state.bomb,
		state.player_miss,
		state.player_hit,
		state.quit,
		state.player_shots,
		state.player_shot_lifetime,
		player_miss_callbacks,
		state.items,
		state.item_lifecycle,
		state.player_stream,
		state.score,
		presentation.items,
		state.lasers_enabled,
		state.lasers,
		presentation.lasers,
		state.bullets,
		state.bullet_special,
		presentation.bullets,
		state.sparks,
		state.spark_parameters,
		state.frame,
		state.stage_frame,
		state.stage_progression,
	};
	return stage2_entities_update(context);
}

} // namespace th02::core
