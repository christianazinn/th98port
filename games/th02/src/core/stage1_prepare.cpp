#include "th02/core/stage1_prepare.hpp"
#include "stage_prepare_common.hpp"

namespace th02::core {

void stage1_notifications(StageState &state, bool &stage_title_enabled,
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

Stage1PreparationResult stage1_prepare(StageState &state,
	Stage1InvalidationPresentation &invalidation,
	Stage1PreparationPresentation &presentation) noexcept
{
	return detail::stage_prepare<false>(state, invalidation, presentation);
}

} // namespace th02::core
