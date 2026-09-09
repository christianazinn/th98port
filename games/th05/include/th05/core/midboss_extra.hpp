#pragma once

namespace th05::core {

struct State;
struct Presentation;

// th05/main/stage/setup.cpp::stagex_setup. The source callback is installed
// before the Extra stage's boss, and owns this one midboss record until its
// defeat animation reaches phase 255.
void extra_midboss_setup(State &) noexcept;
void extra_midboss_activate(State &) noexcept;

// th04/main/midboss/mx_update.cpp with GAME == 5. False reports a reached
// unsupported bullet, shot, spark, or random-ring operation; it is not the
// midboss-liveness predicate.
[[nodiscard]] bool extra_midboss_update(State &, Presentation &) noexcept;

// th04/main/midboss/mx.cpp with GAME == 5 clears a hit flash only after a
// visible normal-phase blit. The native rendering host owns the blit itself.
void extra_midboss_render_tail(State &) noexcept;

} // namespace th05::core
