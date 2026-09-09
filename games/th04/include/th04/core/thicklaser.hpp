#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th04::core {

struct State;
struct Presentation;

using ThickLaserPoint = pc98::th0405::SubpixelPoint;

// th04/main/bullet/laser_t.hpp. The original reserves exactly two slots.
inline constexpr std::size_t thicklaser_count = 2U;
inline constexpr std::int16_t thicklaser_layer_width_max = 16;

// Preserve the source flag bytes: the source relies on an unsigned
// `flag <= LINE` test to exclude both FREE and LINE from collision.
enum class ThickLaserFlag : std::uint8_t {
	free = 0U,
	line = 1U,
	grow = 2U,
	static_ = 3U,
	shrink = 4U,
};

// th04/main/bullet/laser_t.hpp. This raw 24-byte shape keeps source timers
// and radii signed 16-bit values rather than promoting them to host integers.
struct ThickLaser {
	ThickLaserFlag flag{};
	std::int8_t unused_1{};
	ThickLaserPoint origin{};
	std::array<std::int8_t, 4U> unused_2{};
	std::int16_t cur_flag_frame{};
	std::int16_t line_frames{};
	std::int16_t static_frames{};
	std::uint8_t col_outline{};
	std::int8_t unused_3{};
	std::int16_t radius_max{};
	std::int16_t radius_cur{};
	std::int16_t radius_speed{};
	bool operator==(const ThickLaser &) const = default;
};
static_assert(sizeof(ThickLaser) == 24U);
static_assert(offsetof(ThickLaser, flag) == 0U);
static_assert(offsetof(ThickLaser, origin) == 2U);
static_assert(offsetof(ThickLaser, cur_flag_frame) == 10U);
static_assert(offsetof(ThickLaser, radius_max) == 18U);
static_assert(offsetof(ThickLaser, radius_cur) == 20U);
static_assert(offsetof(ThickLaser, radius_speed) == 22U);

struct ThickLaserState {
	std::array<ThickLaser, thicklaser_count> pool{};
	ThickLaser template_{};
	bool operator==(const ThickLaserState &) const = default;
};

// A field-inert snapshot for th04/main/bullet/laser_render.cpp's render pass.
// Callbacks arrive in source pool order. `layer_width` is radius / 4 clamped
// to 16 pixels; clients draw outline, inner color, then white in that order.
struct ThickLaserGeometry {
	ThickLaserFlag flag{};
	ThickLaserPoint origin{};
	std::uint8_t col_outline{};
	std::int16_t radius{};
	std::int16_t layer_width{};
	bool operator==(const ThickLaserGeometry &) const = default;
};

// th04/main/boss/b4m.cpp. Reset frees the pool and seeds only the template
// fields that source spawners leave untouched.
void thicklasers_reset(State &) noexcept;

// th04/main/boss/b4m.cpp. Copies the template into the first free slot and
// emits sound 5; a full pool is a reached no-op.
void thicklaser_add(State &, Presentation &) noexcept;

// th04/main/boss/b4m.cpp. Advances the lifecycle and performs the source's
// inclusive white-core collision, which intentionally ignores invincibility.
void thicklasers_update_and_hittest(State &, Presentation &) noexcept;

// th04/main/bullet/laser_render.cpp. Emits non-free snapshots in pool order
// without mutating lifecycle state.
void thicklasers_render(State &, Presentation &) noexcept;

} // namespace th04::core
