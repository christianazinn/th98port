#pragma once

#include "th02/core/stage_scenery.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

inline constexpr std::size_t bg_particle_count = 30U;
inline constexpr std::uint8_t bg_particle_free = 0U;
inline constexpr std::uint8_t bg_particle_alive = 1U;
inline constexpr std::uint8_t bg_particle_remove = 2U;
inline constexpr std::uint8_t bg_particle_edge_max = 5U;

// Q12.4 source position. The particle's speed is deliberately a separate
// Q4.4 byte, exactly as bg_particle_t stores it.
struct BgParticleSubpixelPoint {
	std::int16_t x{};
	std::int16_t y{};
	bool operator==(const BgParticleSubpixelPoint &) const = default;
};

struct BgParticle {
	std::uint8_t flag{};
	std::uint8_t angle{};
	std::array<BgParticleSubpixelPoint, 2U> screen_topleft{};
	std::uint8_t edge{};
	std::uint8_t speed{};
	bool operator==(const BgParticle &) const = default;
};

static_assert(sizeof(BgParticle) == 12U);
static_assert(offsetof(BgParticle, flag) == 0U);
static_assert(offsetof(BgParticle, angle) == 1U);
static_assert(offsetof(BgParticle, screen_topleft) == 2U);
static_assert(offsetof(BgParticle, edge) == 10U);
static_assert(offsetof(BgParticle, speed) == 11U);

// One owner for bg_particles[] and the neighboring source globals. The four
// numeric parameters are source words even though the update consumes the low
// byte of the first three. The unused bytes are retained because the source
// writes them once per update.
struct BgParticleState {
	std::array<BgParticle, bg_particle_count> particles{};
	std::int16_t speed_initial{};
	std::int16_t speed_delta{};
	std::int16_t angle_delta{};
	std::int16_t edge_step{};
	std::uint8_t color{};
	std::uint8_t unput_color{};
	std::int8_t unused{};
	std::uint8_t unused_2{};
	std::uint8_t unused_3{};
	bool operator==(const BgParticleState &) const = default;
};

// The background-particle body uses the same GRCG/dot-square surface as stage
// scenery. StageSceneryCallbacks::dot_square_unput is its optional direct
// square-clear notification.
using BgParticleCallbacks = StageSceneryCallbacks;

enum class BgParticlePageResult : std::uint8_t {
	completed,
	invalid_page,
};

// th02/main/bg_particle.cpp::bg_particles_reset. Only flags are cleared; all
// remaining record bytes persist. The source defaults are restored afterwards.
void bg_particles_reset(BgParticleState &state) noexcept;

// th02/main/bg_particle.cpp::bg_particles_add. A full pool is a real no-op;
// the false return only makes that source outcome observable to callers.
[[nodiscard]] bool bg_particles_add(BgParticleState &state,
	std::int16_t left, std::int16_t top, std::uint8_t angle) noexcept;

// th02/main/bg_particle.cpp::bg_particles_invalidate. This owns direct
// particle-square clearing, F_REMOVE retirement, and the source page carry.
[[nodiscard]] BgParticlePageResult bg_particles_invalidate(
	BgParticleState &state, std::size_t page_back, std::size_t page_front,
	bool reduce_effects, BgParticleCallbacks &callbacks) noexcept;

// th02/main/bg_particle.cpp::bg_particles_update_and_render. It advances live
// particles, marks clipped entries F_REMOVE, updates edge sizes, and emits
// each visible dot square in source order.
[[nodiscard]] BgParticlePageResult bg_particles_update_and_render(
	BgParticleState &state, std::size_t page_back, bool reduce_effects,
	BgParticleCallbacks &callbacks) noexcept;

} // namespace th02::core
