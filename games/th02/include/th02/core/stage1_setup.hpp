#pragma once

#include "th02/core/stage1_frame.hpp"

namespace th02::core {

class Stage1SetupPresentation {
public:
	virtual ~Stage1SetupPresentation() = default;
	virtual void background_particles_reset() noexcept = 0;
	virtual void sound_effects_reset() noexcept = 0;
	virtual void palette_show() noexcept = 0;
};

// Run-wide state suffix of gameplay_init, after its asset loads. No asset
// availability, high-score loading, or initialized stage is implied.
void gameplay_options_apply(Stage1State &state, bool reduce_effects,
	std::uint16_t continues_used) noexcept;

// Stage1 gameplay projection of stage_init through the asset-loading boundary.
// Caller first performs the opening screen work and sets random.seed as main
// does. Graphics-only background pool reset and SFX reset remain presentation.
// Shared boss fields not consumed by Stage1 are outside this projection.
// Refuses other stages before changing anything; no blank aggregate reset.
[[nodiscard]] bool stage1_setup_before_assets(Stage1State &state, bool demo,
	Stage1SetupPresentation &presentation) noexcept;
// Same common stage_init prefix; Stage2 BGM IDs are 2/3. Rejects other stages
// before mutation, and retains Meira/M2/scenery private state as the source does.
[[nodiscard]] bool stage2_setup_before_assets(StageState &state, bool demo,
	Stage1SetupPresentation &presentation) noexcept;

// Call after actual stage assets/map/script/dialogue loading and tiles reset.
// Installs Stage1's represented callbacks and resets only the source player
// fields. Does not certify that the caller supplied assets or a dialogue owner.
void stage1_setup_after_assets(Stage1State &state, Stage1FrameSchedule &schedule,
	bool &stage_title_enabled) noexcept;
void stage2_setup_after_assets(StageState &state, Stage1FrameSchedule &schedule,
	bool &stage_title_enabled) noexcept;

// Later item/score setup after tile-area initialization. Source input/fades,
// initial tile-ring filling and final page-show/access remain outside this call.
[[nodiscard]] bool stage1_setup_items_and_score(Stage1State &state,
	ItemScheduleCallbacks &presentation) noexcept;

} // namespace th02::core
