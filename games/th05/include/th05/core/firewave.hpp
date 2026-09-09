#pragma once
#include <array>
#include <cstdint>

namespace th05::core {
struct State;
// th05/main/boss/bx.cpp: separate retained BSS, not custom_entities.
struct Firewave {
	bool alive{}, is_right{};
	std::int16_t bottom{}, amplitude{};
	bool operator==(const Firewave &) const = default;
};
using FirewaveState = std::array<Firewave, 2>;
void firewaves_add(State &, std::int16_t amplitude, bool is_right) noexcept;
void firewaves_update(State &) noexcept;
// bx_custombullets.cpp performs player collision while drawing each row.
// Call only while that source custom-render callback is installed.
void firewaves_render_hittest(State &) noexcept;
}
