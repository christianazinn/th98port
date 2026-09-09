#pragma once
#include <cstdint>
#include <span>

namespace th04::core {
struct State;
struct Presentation;
[[nodiscard]] bool map_load(State &, std::span<const std::uint8_t>);
[[nodiscard]] bool tiles_fill_initial(State &) noexcept;
[[nodiscard]] bool scroll_update(State &, Presentation &) noexcept;
}
