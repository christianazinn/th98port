#pragma once

#include <array>
#include <cstdint>

namespace pc98 {
// th04/score.h and th02/score.h: shared TH04/05 storage, not shared policy.
// Index zero aliases continues_used; score digits occupy indices 1 through 7.
using ScoreDigits = std::array<std::uint8_t, 8>;
} // namespace pc98
