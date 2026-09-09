#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace th05::core {
struct State;
struct Presentation;

struct StdState {
	std::vector<std::uint8_t> data;
	std::array<std::uint16_t, 3> title_offsets{};
	std::array<std::uint16_t, 32> enemy_scripts{};
	std::uint16_t ip{}, scroll_speed_offset{}, map_section_offset{};
	std::uint8_t scroll_speed{};
	std::int8_t tile_row_in_section{};
	bool active{};
};

// Preserve segment-relative offsets and untouched script entries across loads.
// Refuse incomplete input without replacing the previous stage data.
[[nodiscard]] bool std_load(StdState &, std::span<const std::uint8_t>);
[[nodiscard]] bool std_run(State &, Presentation &) noexcept;
}
