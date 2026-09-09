#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th05::core {

inline constexpr std::size_t shinki_background_particle_count = 64U;
inline constexpr std::size_t shinki_background_lineset_count = 4U;
inline constexpr std::size_t shinki_background_lines_per_set = 20U;

// th05/main/boss/render[bss].asm. The first two sets are Shinki's active
// sets; the remaining source storage is retained for the Extra background.
struct ShinkiBackgroundLineSet {
	std::array<pc98::th0405::SubpixelPoint, shinki_background_lines_per_set> center{};
	std::int16_t velocity_y{};
	std::array<std::int16_t, shinki_background_lines_per_set> radius{};
	std::array<std::uint8_t, shinki_background_lines_per_set> angle{};
	bool operator==(const ShinkiBackgroundLineSet &) const = default;
};
static_assert(sizeof(ShinkiBackgroundLineSet) == 142U);

// th05/main/boss/render[bss].asm. pos.x == -15984 (Subpixel::None()) means
// inactive; origin, velocity, age, and angle deliberately retain BSS data.
struct ShinkiBackgroundParticle {
	pc98::th0405::SubpixelPoint pos{};
	pc98::th0405::SubpixelPoint origin{};
	pc98::th0405::SubpixelPoint velocity{};
	std::int16_t age{};
	std::uint8_t angle{}, patnum{};
	bool operator==(const ShinkiBackgroundParticle &) const = default;
};
static_assert(sizeof(ShinkiBackgroundParticle) == 16U);

// The arrays are source BSS and survive stage_init. The six scalar values are
// source DATA: only type_a_particles_alive starts at 0x00FF.
struct ShinkiBackgroundState {
	std::array<ShinkiBackgroundLineSet, shinki_background_lineset_count> linesets{};
	std::array<ShinkiBackgroundParticle, shinki_background_particle_count> particles{};
	std::uint8_t linesets_zoomed_out{};
	std::int16_t type_a_particles_alive{0x00FF};
	bool type_b_initialized{};
	std::uint16_t spinline_frame{};
	bool type_c_initialized{}, type_d_initialized{};
	bool operator==(const ShinkiBackgroundState &) const = default;
};

struct State;

// th05/main/boss/render.cpp's installed Stage 6 background callback. It owns
// the render-time particle/line mutations and its randring1 draws; callers
// keep it before sparks, player, bullets, and boss_update.
[[nodiscard]] bool shinki_background_update_and_render(State &) noexcept;

} // namespace th05::core
