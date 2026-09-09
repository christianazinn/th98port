#pragma once

namespace th05::core {

struct State;
struct Presentation;

// th05_main.asm's [PLAYCHAR_COUNT][SHOT_LEVEL_MAX + 1] table. This call owns
// exactly one source shot-function invocation; the caller owns shot_time's
// post-call decrement.
[[nodiscard]] bool player_shots_fire(State &, Presentation &) noexcept;

} // namespace th05::core
