#pragma once

namespace th04::core {

struct State;
struct Presentation;

// th04/main/stage/setup.cpp::stage4_setup() and
// th04/main/midboss/m4_updt.cpp. The Stage 4 driver owns the source call
// positions; these are deliberately not folded into a generic fight loop.
void stage4_midboss_setup(State &) noexcept;
void stage4_midboss_activate(State &) noexcept;
[[nodiscard]] bool stage4_midboss_update(State &, Presentation &) noexcept;

// th04/main/midboss/m4.cpp. In this native core the render tail preserves the
// source's clipped-sprite damage-flag clear, including its small-roll bounds.
void stage4_midboss_render_tail(State &, Presentation &) noexcept;

} // namespace th04::core
