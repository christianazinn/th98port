#pragma once

#include "th02/core/stage1_state.hpp"

namespace th02::core {

class Stage1NotificationPresentation {
public:
	virtual ~Stage1NotificationPresentation() = default;
	virtual void sound_effects_update() noexcept = 0;
	virtual void stage_title_clear() noexcept = 0;
	virtual void bgm_title_show(std::uint8_t title) noexcept = 0;
	virtual void bgm_title_clear() noexcept = 0;
};

// Initial three calls in stage_loop, including the actual byte clock and
// one-shot title callback retirement. `stage_title_enabled` represents the
// installed callback slot; demo only suppresses the title erase, not retirement.
void stage1_notifications(Stage1State &state, bool &stage_title_enabled,
	bool demo, Stage1NotificationPresentation &presentation) noexcept;

class Stage1PreparationPresentation : public StageSceneryCallbacks {
public:
	virtual void egc_begin() noexcept = 0;
	// Drawing and dirty-tile bookkeeping only, not gameplay. Called with the
	// old back-page scroll line; the core clears render_all after this call.
	virtual void tiles_render(const Stage1State &state) noexcept = 0;
};

enum class Stage1PreparationResult : std::uint8_t {
	input_pending, invalid_page, invalidation_refused, spawn_refused,
	scenery_refused, scroll_interval_zero,
};

// stage_loop's contiguous old-page invalidation through new-page scroll save.
// Call after SFX/title notification updates, before input sampling and the
// actual player/bomb/entity passes. It neither advances counters nor flips.
[[nodiscard]] Stage1PreparationResult stage1_prepare(Stage1State &state,
	Stage1InvalidationPresentation &invalidation,
	Stage1PreparationPresentation &presentation) noexcept;

} // namespace th02::core
