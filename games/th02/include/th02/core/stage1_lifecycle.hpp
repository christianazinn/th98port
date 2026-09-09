#pragma once

#include "th02/core/rika.hpp"
#include "th02/core/stage_scenery.hpp"

namespace th02::core {

// Exact suffix of rika_init after dialog_post has returned. The caller must
// complete the preceding BGM/dialogue work first; this is not a substitute
// for that lifecycle. lasers_enabled represents both installed laser passes.
// Retained globals (notably phase_frame, target, Rika X and pattern clocks)
// are deliberately not reset here.
void rika_combat_setup_after_dialogue(BossState &boss, RikaState &rika,
	std::uint32_t &stage_frame, bool &lasers_enabled,
	std::int16_t scroll_line, std::int8_t rank) noexcept;

// Exact tail of rika_end after overlay_stage_leave_animate returns. The
// preceding dialogue, bonus and input wait are not performed by this call.
void rika_stage_leave_finish(std::uint8_t &stage_id,
	SparkUpdateParameters &sparks, StageBgFlashState &flash) noexcept;

} // namespace th02::core
