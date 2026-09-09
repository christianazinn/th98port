#pragma once

#include "th02/core/bullet_lifecycle.hpp"
#include "th02/core/stage_state_fwd.hpp"

#include <cstddef>
#include <cstdint>

namespace th02::core {

// The presentation-only calls reached by stage/init.cpp::continue_resume().
// `invalidate_bullet()` is the existing synchronous bullet-clear boundary;
// implementations must not mutate the borrowed StageState while a callback is
// active.
class StageContinuePresentationCallbacks : public EnemyBulletInvalidationCallbacks {
public:
	virtual ~StageContinuePresentationCallbacks() = default;
	virtual void hud_put() noexcept = 0;
	virtual void overlay_stage_leave_animate() noexcept = 0;
	virtual void tiles_render_all(std::size_t page) noexcept = 0;
	virtual void palette_100() noexcept = 0;
	virtual void overlay_stage_enter_animate() noexcept = 0;
};

enum class StageContinueResult : std::uint8_t {
	completed,
	bullet_invalid_page,
};

// th02/main/stage/init.cpp::continue_resume. This is the narrow post-continue
// restart sequence, not stage_init or the preceding continue-menu body. The
// source reaches bullet clearing only after its HUD, leave-overlay, player
// reset, and two tile redraw calls; a host-invalid page keeps that prefix and
// stops before palette and enter-overlay work.
[[nodiscard]] StageContinueResult stage1_continue_resume(
	StageState &state, StageContinuePresentationCallbacks &presentation
) noexcept;

} // namespace th02::core
