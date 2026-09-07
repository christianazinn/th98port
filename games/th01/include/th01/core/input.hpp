#pragma once

#include <array>
#include <cstdint>

namespace th01::core {

// Canonical raw PC-98 key groups latched once per REIIDEN input sample.
// Arrow and numpad keys deliberately remain distinct for edge detection.
struct RawInput {
	std::uint8_t group0{};
	std::uint8_t group3{};
	std::uint8_t group5{};
	std::uint8_t group7{};
	std::uint8_t group8{};
	std::uint8_t group9{};
};

enum Direction : std::uint8_t {
	none = 0,
	right = 1,
	left = 2,
	both = 3,
};

struct InputState {
	std::array<std::uint8_t, 16> history{};
	std::uint8_t direction{};
	bool up{};
	bool down{};
	bool shot{};
	bool strike{};
	bool ok{};
	bool paused{};
	bool bomb{};
	bool player_is_hit{};
	bool memory_enter{};
	bool memory_leave{};
	std::int16_t bomb_doubletap_frame{};
};

// Production REIIDEN input only. Neither routine advances gameplay clocks.
void sense_input(InputState &state, const RawInput &raw) noexcept;
// input_sense(true): clear history 0..13 and bomb, preserving other state.
void reset_repeat(InputState &state) noexcept;
// input_reset_sense(): also clear menu/action outputs, but not player_is_hit.
void reset_input(InputState &state) noexcept;

} // namespace th01::core
