#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th05::core {

struct State;
struct Presentation;

inline constexpr std::size_t laser_count = 32U;
inline constexpr std::int16_t laser_distance_min = 256;
inline constexpr std::int16_t laser_distance_max = 8800;

// th05/main/bullet/laser.hpp. Preserve raw source flag bytes, including the
// signed-byte shrink interpretation used only by the two fixed shrink modes.
enum class LaserFlag : std::uint8_t {
	free = 0U,
	shootout = 1U,
	fixed_wait_to_grow = 2U,
	fixed_grow = 3U,
	fixed_active = 4U,
	fixed_shrink = 5U,
	fixed_shrink_and_wait_to_grow = 6U,
	shootout_decay = 7U,
};

struct LaserCoords {
	pc98::th0405::SubpixelPoint origin{};
	std::int16_t starts_at_distance{};
	std::int16_t ends_at_distance{};
	std::uint8_t angle{};
	std::uint8_t width{};
	bool operator==(const LaserCoords &) const = default;
};
static_assert(sizeof(LaserCoords) == 10U);

struct Laser {
	LaserFlag flag{};
	std::uint8_t color{};
	LaserCoords coords{};
	std::int16_t shootout_speed{};
	std::int16_t age{};
	std::int16_t active_at_age{};
	std::int16_t shrink_at_age{};
	std::uint8_t grow_to_width{};
	std::array<std::uint8_t, 3U> padding{};
	bool operator==(const Laser &) const = default;
};
static_assert(sizeof(Laser) == 24U);
static_assert(offsetof(Laser, flag) == 0U);
static_assert(offsetof(Laser, color) == 1U);
static_assert(offsetof(Laser, coords) == 2U);
static_assert(offsetof(Laser, shootout_speed) == 12U);
static_assert(offsetof(Laser, age) == 14U);
static_assert(offsetof(Laser, active_at_age) == 16U);
static_assert(offsetof(Laser, shrink_at_age) == 18U);
static_assert(offsetof(Laser, grow_to_width) == 20U);

struct LaserState {
	std::array<Laser, laser_count> pool{};
	Laser laser_template{};
	bool operator==(const LaserState &) const = default;
};

// A by-value source snapshot at the render boundary. It cannot supply an
// offscreen answer or mutate lifecycle state; lasers_render owns both.
struct LaserGeometry {
	LaserFlag flag{};
	std::uint8_t color{};
	LaserCoords coords{};
	bool operator==(const LaserGeometry &) const = default;
};

// th05/main/bullet/laser.cpp + laser_rh.cpp. Update owns active-ray hittest;
// render owns the source's offscreen-to-free lifecycle transition.
[[nodiscard]] bool lasers_update(State &, Presentation &) noexcept;
void lasers_render(State &, Presentation &) noexcept;

// Source laser controls. Invalid host slot values are explicit false
// results before indexing; occupied/inapplicable source slots are success
// no-ops after their reached state reads.
// th05/main/bullet/laser.cpp's forward first-free shootout allocator. A full
// source pool is a successful no-op after tuning the local speed.
[[nodiscard]] bool lasers_shootout_add(State &, Presentation &) noexcept;
[[nodiscard]] bool laser_fixed_spawn(
	State &, std::uint8_t slot, Presentation &
) noexcept;
[[nodiscard]] bool laser_manual_fixed_spawn(
	State &, std::uint8_t slot, Presentation &
) noexcept;
[[nodiscard]] bool laser_manual_grow(
	State &, std::uint8_t slot, Presentation &
) noexcept;
[[nodiscard]] bool laser_stop(State &, std::uint8_t slot) noexcept;

} // namespace th05::core
