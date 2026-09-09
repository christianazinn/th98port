#pragma once

#include "th02/core/stage2_prepare.hpp"

namespace th02::core::detail {
// Compile-time stage dispatch shares the source loop without gameplay callbacks.
template<bool Stage2, class Invalidation>
Stage1PreparationResult stage_prepare(StageState &state, Invalidation &invalidation,
	Stage1PreparationPresentation &presentation) noexcept
{
	const auto saved_line = state.scroll.line;
	if (state.frame.page_back >= state.scroll_line_on_page.size())
		return Stage1PreparationResult::invalid_page;
	state.scroll.line = state.scroll_line_on_page[state.frame.page_back];
	Stage1InvalidationResult invalidated;
	if constexpr (Stage2) invalidated = stage2_invalidate(state, invalidation);
	else invalidated = stage1_invalidate(state, invalidation);
	if (invalidated != Stage1InvalidationResult::completed)
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
	StageSceneryResult scenery_result;
	if constexpr (Stage2) {
		scenery_result = stage2_update_and_render(state.scenery, state.stage2_scenery_frame,
			state.stage2_scenery_setup, state.stage_bg_flash, state.bomb_animation.palette_tone,
			state.spark_parameters, state.random, state.frame.page_back, state.scroll.line, presentation);
	} else {
		scenery_result = stage1_update_and_render(state.scenery, state.midboss.scenery_frame,
			state.stage_bg_flash, state.bomb_animation.palette_tone, state.spark_parameters,
			state.random, state.frame.page_back, state.scroll.line, state.reduce_effects, presentation);
	}
	if (scenery_result == StageSceneryResult::invalid_page)
		return Stage1PreparationResult::scenery_refused;
	presentation.egc_off();
	if (state.scroll.step == (Stage2 ? 80 : 116)) state.midboss_active = 1;
	if (!scroll_advance_line(state.scroll, saved_line))
		return Stage1PreparationResult::scroll_interval_zero;
	state.scroll_line_on_page[state.frame.page_back] = state.scroll.line;
	return Stage1PreparationResult::input_pending;
}
} // namespace th02::core::detail
