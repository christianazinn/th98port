#pragma once
#include <cstdint>
namespace th04::core {
struct State;
struct Presentation;
// Source tally values are score/10. These calls add to score_delta once;
// the host owns the blocking tally display and subsequent transition.
std::uint32_t stage_clear_bonus(State &, Presentation &) noexcept;
std::uint32_t stage_allclear_bonus(State &, Presentation &) noexcept;
}
