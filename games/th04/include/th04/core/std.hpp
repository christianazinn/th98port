#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace th04::core {
struct State;
struct Presentation;

struct StdState {
	std::vector<std::uint8_t> data;
	std::array<std::uint16_t, 32> enemy_scripts{};
	std::uint16_t ip{}, scroll_speed_offset{};
	std::int16_t map_section_id{};
	std::uint8_t scroll_speed{};
	std::int8_t tile_row_in_section{};
	bool active{};
};

// th04/formats/std.cpp. Decode the file into its source segment layout.
// Malformed/unrepresentable input is refused without replacing the old data.
// Unassigned script table entries survive a successful reload, as in DOS.
[[nodiscard]] bool std_load(StdState &, std::span<const std::uint8_t> file);
// th04/formats/std_run.cpp. Execute the due spawn group, not a generic VM
// callback. False indicates a reached input or native gameplay boundary.
[[nodiscard]] bool std_run(State &, Presentation &) noexcept;
} // namespace th04::core
