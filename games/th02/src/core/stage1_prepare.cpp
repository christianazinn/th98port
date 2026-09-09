#include "th02/core/stage1_prepare.hpp"

namespace th02::core {

void stage1_notifications(Stage1State &state, bool &stage_title_enabled,
	bool demo, Stage1NotificationPresentation &presentation) noexcept
{
	presentation.sound_effects_update();
	if (stage_title_enabled && state.stage_frame == 160U) {
		if (!demo) presentation.stage_title_clear();
		stage_title_enabled = false;
	}
	if (state.bgm_show_timer == 0U) return;
	if (state.bgm_show_timer == 1U) presentation.bgm_title_show(state.bgm_title_id);
	state.bgm_show_timer = static_cast<std::uint8_t>(state.bgm_show_timer + 1U);
	if (state.bgm_show_timer >= 160U) {
		presentation.bgm_title_clear();
		state.bgm_show_timer = 0U;
	}
}

Stage1PreparationResult stage1_prepare(Stage1State &state,
	Stage1InvalidationPresentation &invalidation,
	Stage1PreparationPresentation &presentation) noexcept
{
	const auto saved_line = state.scroll.line;
	if (state.frame.page_back >= state.scroll_line_on_page.size())
		return Stage1PreparationResult::invalid_page;
	state.scroll.line = state.scroll_line_on_page[state.frame.page_back];
	if (stage1_invalidate(state, invalidation) != Stage1InvalidationResult::completed)
		return Stage1PreparationResult::invalidation_refused;
	if (state.scroll.step_advanced != 0) {
		if (state.stage_data.template_count < 0 ||
			static_cast<std::size_t>(state.stage_data.template_count) > state.stage_data.templates.size())
			return Stage1PreparationResult::spawn_refused;
		const EnemySpawnGrid grid{state.stage_data.spawn_rows, state.stage_data.spawn_row_count};
		const auto templates = std::span{state.stage_data.templates}.first(
			static_cast<std::size_t>(state.stage_data.template_count));
		if (enemies_spawn(state.enemies, state.random, grid, templates,
			{state.scroll.step, state.midboss_active != 0, state.frame.page_back}) != EnemySpawnResult::completed)
			return Stage1PreparationResult::spawn_refused;
		state.scroll.step_advanced = 0;
	}
	presentation.egc_begin();
	if (stage_scenery_invalidate(state.scenery, state.frame.page_back,
		state.frame.page_front, presentation) != StageSceneryPageResult::completed)
		return Stage1PreparationResult::scenery_refused;
	presentation.tiles_render(state);
	state.tiles_egc_render_all = false;
	if (stage1_update_and_render(state.scenery, state.midboss.scenery_frame,
		state.stage_bg_flash, state.bomb_animation.palette_tone, state.spark_parameters,
		state.random, state.frame.page_back, state.scroll.line, state.reduce_effects,
		presentation) == Stage1SceneryResult::invalid_page)
		return Stage1PreparationResult::scenery_refused;
	presentation.egc_off();
	if (state.scroll.step == 116) state.midboss_active = 1;
	if (!scroll_advance_line(state.scroll, saved_line))
		return Stage1PreparationResult::scroll_interval_zero;
	state.scroll_line_on_page[state.frame.page_back] = state.scroll.line;
	return Stage1PreparationResult::input_pending;
}

} // namespace th02::core
