#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th05::core {

inline constexpr std::size_t b6ball_count = 63U;

enum class B6BallFlag : std::uint8_t {
	free = 0U,
	cloud = 1U,
	alive = 2U,
	decay = 3U,
};

// th05/main/bullet/b6ball.hpp. This has the same raw 26-byte custom-entity
// layout as the Stage 4 balls, Stage 5 swords, and Cheeto heads. In
// particular, the named B6 unused bytes are live Cheeto fields during
// Shinki's other patterns; do not reset them during b6balls_add().
struct B6Ball {
	B6BallFlag flag{};
	std::uint8_t unused_1{};
	pc98::th0405::Motion pos{};
	std::uint16_t age{};
	std::int16_t cloud_radius{};
	std::int16_t patnum_tiny{}, decay_frame{};
	std::array<std::int8_t, 4U> unused_2{};
	bool operator==(const B6Ball &) const = default;
};
static_assert(sizeof(B6Ball) == 26U);

// This source overlay is likewise shared with CheetoTemplate: its patnum
// word covers Cheeto's color byte when that pattern family is active.
struct B6BallTemplate {
	std::uint8_t angle{};
	pc98::th0405::SubpixelPoint origin{};
	std::int16_t patnum_tiny{};
	std::uint8_t speed{};
	bool operator==(const B6BallTemplate &) const = default;
};

struct B6BallState {
	B6BallTemplate b6ball_template{};
	std::array<B6Ball, b6ball_count> pool{};
	bool operator==(const B6BallState &) const = default;
};

struct State;

// th05/main/bullet/b6ball.cpp. Source-void pool operations: a full pool is a
// valid no-op, and update has no score/RNG or presentation callback seam.
void b6balls_add(State &) noexcept;
void b6balls_update(State &) noexcept;

// th05/main/bullet/b6_custombullets_render.asm has no native gameplay write.
void b6balls_render_tail(State &) noexcept;

} // namespace th05::core
