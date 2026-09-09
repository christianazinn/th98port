#pragma once

namespace th04::core {
struct State;
struct Presentation;
void extra_midboss_setup(State &) noexcept;
void extra_midboss_activate(State &) noexcept;
// th04/main/midboss/mb_upd.cpp: an invulnerable scripted fly-by. Its HP
// field is the orbit radius, not a damageable health pool.
[[nodiscard]] bool extra_midboss_update(State &, Presentation &) noexcept;
// mx.cpp draws only; it does not clear damage or advance gameplay state.
}
