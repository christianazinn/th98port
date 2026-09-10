#pragma once

#include "th01/core/state.hpp"

namespace th01::core {

enum class TickResult : std::uint8_t {
	running, waiting_for_shot, paused, route_selection, stage_cleared, life_lost, game_cleared,
	unsupported_numeric_state, unsupported_boss,
};

// One source gameplay iteration, ending at its pause or life/stage boundary.
// A modal client must call gameplay_resume() once before submitting more input.
[[nodiscard]] TickResult gameplay_tick(State &, const RawInput &) noexcept;
[[nodiscard]] TickResult gameplay_resume(State &) noexcept;
[[nodiscard]] TickResult gameplay_choose_route(State &, BossRoute) noexcept;

// Source life restart, excluding its blocking visual animation. Only call
// after life_lost with a spare life; game-over/continue remains a client choice.
[[nodiscard]] bool gameplay_restart_life(State &) noexcept;
void score_extend_update(State &) noexcept;

// Stage data is loaded separately. A new process additionally resets BSS and
// imports resident state; this function only owns the source stage/life entry.
[[nodiscard]] StageSetupResult gameplay_begin_stage(State &, bool first_stage_in_scene) noexcept;
// Source's initial Shot gate permits player movement without gameplay clocks.
[[nodiscard]] bool gameplay_poll_start(State &, const RawInput &) noexcept;
// Pure score arithmetic from stagebonus_animate()/totle_animate(). Does not
// advance the stage or silently answer the subsequent input modal.
[[nodiscard]] std::uint32_t gameplay_stage_bonus(State &, bool boss_stage) noexcept;

} // namespace th01::core
