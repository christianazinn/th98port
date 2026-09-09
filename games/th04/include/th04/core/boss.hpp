#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th04::core {

struct State;
struct Presentation;

using Point = pc98::th0405::SubpixelPoint;
using Motion = pc98::th0405::Motion;

inline constexpr std::size_t circle_count = 16U;
inline constexpr std::uint8_t circle_free = 0U;
inline constexpr std::uint8_t circle_alive = 1U;
inline constexpr std::uint8_t circle_remove = 2U;

// th04/main/circle.hpp. Centers and radii are source pixel units, unlike the
// gameplay positions passed to the allocation helpers.
struct Circle {
	std::uint8_t flag{}, age{};
	std::int16_t center_x{}, center_y{};
	std::int16_t radius_cur{}, radius_delta{};
	bool operator==(const Circle &) const = default;
};

struct CircleState {
	std::array<Circle, circle_count> pool{};
	bool operator==(const CircleState &) const = default;
};

// th04/main/boss/explode.hpp. The source keeps two small explosion slots and
// one big slot; their aging is deliberately a render-tail operation.
struct BossExplosion {
	bool alive{};
	std::uint8_t age{};
	Point center{}, radius_cur{}, radius_delta{};
	std::int8_t unused{};
	std::uint8_t angle_offset{};
	bool operator==(const BossExplosion &) const = default;
};

struct BossExplosionState {
	std::array<BossExplosion, 2U> small{};
	BossExplosion big{};
	std::int16_t big_frame{};
	bool operator==(const BossExplosionState &) const = default;
};

// th04/main/midboss/midboss.hpp, with Stage 1's otherwise-private fan byte.
struct MidbossState {
	Motion pos{};
	std::uint16_t frames_until{};
	std::int16_t hp{};
	std::uint8_t sprite{}, phase{};
	std::int16_t phase_frame{};
	std::uint8_t damage_this_frame{}, angle{};
	std::uint8_t stage1_fan_angle{};
	bool operator==(const MidbossState &) const = default;
};

// th04/main/boss/boss.hpp. The source globals boss_hitbox_radius and
// boss_phase_timed_out are grouped here for the native State owner.
struct BossState {
	Motion pos{};
	std::int16_t hp{};
	std::uint8_t sprite{}, phase{};
	std::int16_t phase_frame{};
	std::uint8_t damage_this_frame{}, mode{}, angle{};
	std::array<std::uint8_t, 16U> statebytes{};
	// patterns_seen during regular phases; defeat_bonus in Orange phase 4/5.
	std::uint8_t phase_state{};
	std::int16_t phase_end_hp{};
	Point hitbox_radius{};
	bool phase_timed_out{true};
	bool operator==(const BossState &) const = default;
};

// Source setup seeds state and callbacks separately. The driver calls the
// activation checks at their actual source positions: midboss before STD,
// boss only after the dialog/scroll condition succeeds.
void stage1_fights_setup(State &) noexcept;
void stage1_midboss_activate(State &) noexcept;
void stage1_boss_activate(State &) noexcept;

// Call in the source midboss/boss gameplay slot, after enemies_update(). A
// false result is a reached unsupported source boundary; no callback stands in
// for an allocation, RNG draw, or collision path.
[[nodiscard]] bool stage1_fights_update(State &, Presentation &) noexcept;

// th04/main/stage/loop.cpp updates circles before sparks/player gameplay.
void stage1_circles_update(State &, Presentation &) noexcept;

// The explosion age/animation updates belong to boss_fg_render()/midboss_render
// after bomb processing, rather than to the earlier gameplay update.
void stage1_fights_render_tail(State &, Presentation &) noexcept;

} // namespace th04::core
