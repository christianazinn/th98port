#pragma once

#include "th02/core/stage1_state.hpp"

namespace th02::core {

enum class Stage1FramePhase : std::uint8_t {
	entities_complete, paused, boss_init_pending, stage_end_pending,
	complete, cleared, quit, faulted,
};

// Continuation state, not additional source globals. The enclosing owner arms
// a new tail only after the actual entity pass has completed. No dialogue or
// pause simulation is replaced by a callback returning a success verdict.
struct Stage1FrameSchedule {
	Stage1FramePhase phase{Stage1FramePhase::entities_complete};
	bool boss_activation_enabled{true};
	bool stage_end_enabled{};
};

class Stage1FramePresentation : public ScrollCallbacks {
public:
	virtual void wait_for_presentation(std::uint8_t slowdown) noexcept = 0;
	virtual void page_show(std::size_t page) noexcept = 0;
	virtual void page_access(std::size_t page) noexcept = 0;
	virtual void score_events(const ScoreEvents &events) noexcept = 0;
};

// The contiguous stage_loop suffix after sparks_update_and_render. Refuses
// duplicate/out-of-order calls without restarting the pending frame. A paused
// frame has not scrolled, flipped, incremented counters, or transferred score.
[[nodiscard]] Stage1FramePhase stage1_frame_finish(StageState &state,
	Stage1FrameSchedule &schedule, Stage1FramePresentation &presentation) noexcept;
[[nodiscard]] Stage1FramePhase stage1_frame_resume(StageState &state,
	Stage1FrameSchedule &schedule, bool pause_quit,
	Stage1FramePresentation &presentation) noexcept;

// Call only after the real corresponding lifecycle has returned, including
// dialogue and its score/input work. These resume the suspended call site;
// they do NOT implement Rika init/end, reset state, or certify a full session.
[[nodiscard]] Stage1FramePhase stage1_frame_after_boss_init(StageState &state,
	Stage1FrameSchedule &schedule, Stage1FramePresentation &presentation) noexcept;
[[nodiscard]] Stage1FramePhase stage1_frame_after_stage_end(StageState &state,
	Stage1FrameSchedule &schedule, Stage1FramePresentation &presentation) noexcept;

} // namespace th02::core
