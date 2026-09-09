#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th05::core {

inline constexpr std::size_t sword_count = 63U;
inline constexpr std::uint8_t sword_free = 0U;
inline constexpr std::uint8_t sword_alive = 1U;
inline constexpr std::uint8_t sword_remove = 2U;

// th05/main/bullet/sword.hpp. The raw source's unused template patnum is
// retained even though swords_add() always derives the live sprite from angle.
struct Sword {
	std::uint8_t flag{}, angle{};
	pc98::th0405::Motion pos{};
	std::uint16_t twirl_time{};
	std::int16_t patnum_tiny{}, decay_frame{};
	std::uint8_t speed{};
	bool operator==(const Sword &) const = default;
};

struct SwordTemplate {
	std::uint8_t angle{};
	pc98::th0405::SubpixelPoint origin{};
	std::uint16_t twirl_time{};
	std::int16_t patnum_tiny{};
	std::uint8_t speed{};
	bool operator==(const SwordTemplate &) const = default;
};

struct SwordState {
	SwordTemplate sword_template{};
	std::array<Sword, sword_count> pool{};
	bool operator==(const SwordState &) const = default;
};

struct State;
struct Presentation;

// th05/main/bullet/swords_add_update.asm. Add always writes circles_color 9,
// including when the bounded source pool is already full. All three functions
// are source-void; rendering has no native gameplay mutation.
void swords_add(State &) noexcept;
void swords_update(State &, Presentation &) noexcept;
void swords_render_tail(State &) noexcept;

} // namespace th05::core
