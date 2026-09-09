#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th04::core {

struct State;
struct Presentation;

using Yuuka6CustomPoint = pc98::th0405::SubpixelPoint;

inline constexpr std::size_t yuuka6_custom_slot_count = 64U;
inline constexpr std::size_t yuuka6_chasecross_count = yuuka6_custom_slot_count - 1U;

// th04/main/boss/b6ent.hpp. The source turns the raw flag into a kill-animation
// frame counter, starting at PAT_ENEMY_KILL * 4 and ending before 12 * 4.
enum class Yuuka6ChasecrossFlag : std::uint8_t {
	free = 0U,
	alive = 1U,
	kill_anim = 16U,
};

// th04/main/boss/b6ent.hpp's chasecross_t, one 26-byte custom-entity slot.
struct Yuuka6Chasecross {
	Yuuka6ChasecrossFlag flag{};
	std::uint8_t angle{};
	Yuuka6CustomPoint center{};
	std::array<std::int8_t, 4U> unused_1{};
	Yuuka6CustomPoint velocity{};
	std::uint16_t age{};
	std::array<std::int8_t, 4U> unused_2{};
	std::int16_t hp{}, damage_this_frame{};
	std::uint8_t speed{};
	std::int8_t padding{};
	bool operator==(const Yuuka6Chasecross &) const = default;
};
static_assert(sizeof(Yuuka6Chasecross) == 26U);
static_assert(offsetof(Yuuka6Chasecross, flag) == 0U);
static_assert(offsetof(Yuuka6Chasecross, center) == 2U);
static_assert(offsetof(Yuuka6Chasecross, velocity) == 10U);
static_assert(offsetof(Yuuka6Chasecross, age) == 14U);
static_assert(offsetof(Yuuka6Chasecross, hp) == 20U);
static_assert(offsetof(Yuuka6Chasecross, speed) == 24U);

enum class Yuuka6SafetyCircleFlag : std::uint8_t {
	free = 0U,
	grow = 1U,
	shrink = 2U,
};

// Safety-circle centers are physical screen pixels, rather than Q12.4
// playfield coordinates. The record is exactly the final custom slot.
struct Yuuka6ScreenPoint {
	std::int16_t x{}, y{};
	bool operator==(const Yuuka6ScreenPoint &) const = default;
};

// th04/main/boss/b6ent.hpp's safetycircle_t. `unused_3` is four bytes; its
// shorter tail is what keeps this representation at one source custom slot.
struct Yuuka6SafetyCircle {
	Yuuka6SafetyCircleFlag flag{};
	std::int8_t unused_1{};
	Yuuka6ScreenPoint center{};
	std::array<std::int8_t, 8U> unused_2{};
	std::uint16_t shrink_frame{};
	std::int16_t radius_filled{}, radius_ring_distance{};
	std::array<std::int8_t, 4U> unused_3{};
	std::uint8_t color_ring{};
	std::int8_t padding{};
	bool operator==(const Yuuka6SafetyCircle &) const = default;
};
static_assert(sizeof(Yuuka6SafetyCircle) == sizeof(Yuuka6Chasecross));
static_assert(offsetof(Yuuka6SafetyCircle, flag) == 0U);
static_assert(offsetof(Yuuka6SafetyCircle, center) == 2U);
static_assert(offsetof(Yuuka6SafetyCircle, shrink_frame) == 14U);
static_assert(offsetof(Yuuka6SafetyCircle, radius_filled) == 16U);
static_assert(offsetof(Yuuka6SafetyCircle, color_ring) == 24U);

// The final source record has both views. The 64th cross spawn is a source
// landmine: it writes this circle view as raw chasecross bytes, but the next
// update/render still treats it only as a safety circle. Keep it one object.
struct Yuuka6CustomState {
	std::array<Yuuka6Chasecross, yuuka6_chasecross_count> chasecrosses{};
	Yuuka6SafetyCircle safetycircle{};
	bool operator==(const Yuuka6CustomState &) const = default;
};

// th04/main/boss/b6_spawn.cpp. Scans all 64 physical source slots; a full
// block is a valid no-op. `speed` is the source Q4.4 byte and is not tuned.
void yuuka6_chasecrosses_add(State &, std::uint8_t angle, std::uint8_t speed) noexcept;

// th04/main/boss/b6_spawn.cpp. Opens the final-slot safety circle around the
// current player pixel position and plays source sound 8.
void yuuka6_safetycircle_open(State &, Presentation &) noexcept;

// th04/main/boss/b6_next.cpp. Updates 63 crosses, then the same final physical
// slot as a safety circle. False preserves a reached source arithmetic, spark,
// or concrete bullet allocation fault; it never converts that path to a no-op.
[[nodiscard]] bool yuuka6_customs_update(State &, Presentation &) noexcept;

// th04/main/boss/b6_fg.cpp. The drawing itself has no native gameplay write,
// but its foreground pass advances kill-animation flags. Live cross damage
// stays set until the next gameplay hittest overwrites it.
void yuuka6_customs_render_tail(State &) noexcept;

} // namespace th04::core
