#pragma once
#include <cstdint>
namespace th05::core {
struct State;
struct Presentation;
// Source tally values are score/10. Host calls once at the corresponding
// post-boss boundary; dialogue, tally timing and transition remain host work.
std::uint32_t stage_clear_bonus(State &, Presentation &) noexcept;
std::uint32_t stage_allclear_bonus(State &, Presentation &) noexcept;
}
