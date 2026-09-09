#include "th02/core/stage1_setup.hpp"
#include "th02/core/random.hpp"

namespace th02::core {

void gameplay_options_apply(Stage1State &state, bool reduce_effects,
	std::uint16_t continues_used) noexcept
{
	state.reduce_effects = reduce_effects;
	state.continues_used = continues_used;
	if (continues_used != 0U) {
		state.score.score = continues_used >= 9U ? 9 : continues_used;
		state.items.bigpower_override = static_cast<std::uint16_t>((state.stage_id % 5U) + 2U);
	}
	state.playperf_max = state.rank == 0 ? 4 : 16;
}

namespace {
bool stage_setup_before_assets(StageState &state, bool demo,
	Stage1SetupPresentation &presentation) noexcept
{
	const auto spark_defaults = bullets_and_sparks_init(state.bullets, state.sparks,
		state.random, state.bullet_special, state.rank, state.reduce_effects);
	state.spark_sprite_interval = spark_defaults.sprite_interval;
	state.spark_parameters.age_max = spark_defaults.age_max;
	enemies_reset(state.enemies);
	bg_particles_reset(state.background_particles);
	presentation.background_particles_reset();
	lasers_reset(state.lasers);
	bomb_reset(state.bomb);
	presentation.sound_effects_reset();
	state.boss.phase_frame = 0;
	state.stones.damage.fill(0);
	state.stones.flag.fill(0);
	state.stones.hit_flash.fill(0);
	state.stage3_effect.frame = 0;
	state.boss.phase = 0;
	state.boss.damage = 0;
	scroll_reset(state.scroll);
	randring_fill(state.random);
	state.bomb_animation.palette_tone = 100;
	presentation.palette_show();
	state.frame.player.on_page[0] = {208, 336};
	state.frame.player.on_page[1] = {208, 336};
	state.stage_frame = 0;
	state.midboss_active = 0;
	constexpr std::int16_t midboss_steps[]{116, 80, 103, 944, -1, 200};
	state.midboss_scroll_step = midboss_steps[state.stage_id];
	state.stage_progression = StageProgression::stage;
	state.bomb_animation.slowdown_factor = 1;
	if (!demo) {
		state.bgm_show_timer = 1;
		state.bgm_title_id = static_cast<std::uint8_t>(state.stage_id * 2U);
		state.boss_bgm_title_id = static_cast<std::uint8_t>(state.stage_id * 2U + 1U);
	} else {
		state.bgm_show_timer = 0;
	}
	return true;
}
} // namespace

bool stage1_setup_before_assets(Stage1State &state, bool demo,
	Stage1SetupPresentation &presentation) noexcept
{
	return state.stage_id == 0U && stage_setup_before_assets(state, demo, presentation);
}

bool stage2_setup_before_assets(StageState &state, bool demo,
	Stage1SetupPresentation &presentation) noexcept
{
	return state.stage_id == 1U && stage_setup_before_assets(state, demo, presentation);
}

bool stage3_setup_before_assets(StageState &state, bool demo,
	Stage1SetupPresentation &presentation) noexcept
{
	return state.stage_id == 2U && stage_setup_before_assets(state, demo, presentation);
}

bool stage4_setup_before_assets(StageState &state, bool demo,
	Stage1SetupPresentation &presentation) noexcept
{
	return state.stage_id == 3U && stage_setup_before_assets(state, demo, presentation);
}

void stage4_setup_after_assets(StageState &state, Stage1FrameSchedule &schedule,
	bool &stage_title_enabled) noexcept
{
	stage1_setup_after_assets(state, schedule, stage_title_enabled);
	state.lasers_enabled = true;
	state.scroll.speed = 2;
	state.scroll.interval = 1;
}

void stage1_setup_after_assets(Stage1State &state, Stage1FrameSchedule &schedule,
	bool &stage_title_enabled) noexcept
{
	state.boss_enabled = false;
	state.lasers_enabled = false;
	state.enemies_enabled = true;
	schedule.boss_activation_enabled = true;
	stage_title_enabled = true;
	schedule.stage_end_enabled = false;
	player_reset(state.player_reset, state.quit, state.player_hit, state.player_miss,
		state.player_shots, state.player_shot_lifetime, state.player_stream,
		state.player_volley, state.player_speeds, state.shottype);
	state.boss.target = {state.player_volley.boss_position.x, state.player_volley.boss_position.y};
	state.scroll.speed = 1;
	state.scroll.interval = 4;
	// Concrete pass types supply the stage-specific callbacks. No boss-private
	// clock is cleared here in the source.
}

void stage2_setup_after_assets(StageState &state, Stage1FrameSchedule &schedule,
	bool &stage_title_enabled) noexcept
{
	// Stages 1 and 2 have the same callback-enable flags and scroll defaults.
	stage1_setup_after_assets(state, schedule, stage_title_enabled);
}

bool stage1_setup_items_and_score(Stage1State &state,
	ItemScheduleCallbacks &presentation) noexcept
{
	items_init_and_reset(state.items, state.item_lifecycle, state.random, presentation);
	return score_extend_init(state.score);
}

} // namespace th02::core
