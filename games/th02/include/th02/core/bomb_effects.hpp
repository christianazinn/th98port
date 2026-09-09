#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "th02/core/bomb.hpp"

namespace th02::core {

struct RandomState;

inline constexpr std::size_t bomb_particle_count = 32U;
inline constexpr std::uint8_t bomb_particle_cels = 4U;
inline constexpr std::uint8_t bomb_circle_points = 64U;
inline constexpr std::uint8_t bomb_circle_angle_step = 4U;

// State local to th02/main/player/bomb.cpp's three rendering-effect helpers.
// The scroll words are source write-only state. They remain represented even
// though these helpers never read them back.
struct BombEffectState {
	std::array<ScreenPoint, bomb_particle_count> particle_positions{};
	std::array<std::uint8_t, bomb_particle_count> particle_cels{};
	std::array<std::int16_t, 2U> circle_scroll_line_unused{};
	bool operator==(const BombEffectState &) const = default;
};

enum class BombCircleEffectResult : std::uint8_t {
	completed,
	invalid_page,
};

// Final rendering calls in the source remain synchronous observation points.
// Callers must not mutate the borrowed BombState, BombEffectState, or random
// state while a callback is running.
class BombEffectCallbacks {
public:
	virtual ~BombEffectCallbacks() = default;
	virtual void grcg_set_color(std::uint8_t color) noexcept = 0;
	virtual void circle_point(ScreenPoint topleft) noexcept = 0;
	virtual void particle(ScreenPoint topleft, std::int16_t cel) noexcept = 0;
	virtual void grcg_off() noexcept = 0;
};

// th02/main/player/bomb.cpp::bomb_circle_update_and_render. The source
// increments circle_frame before writing the page-indexed unused scroll word;
// an unrepresentable native page therefore retains that prefix and reports
// invalid_page before any rendering callback.
[[nodiscard]] BombCircleEffectResult bomb_circle_update_and_render(
	BombState &bomb, BombEffectState &effects, std::size_t page_back,
	std::int16_t scroll_line, BombEffectCallbacks &callbacks
) noexcept;

// th02/main/player/bomb.cpp::bomb_b_ring_put. Samples the same 64-point ring
// with a caller-provided signed-word radius step.
void bomb_b_ring_put(const BombState &bomb, std::int16_t radius_step,
	BombEffectCallbacks &callbacks) noexcept;

// th02/main/player/bomb.cpp::bomb_particles_update_and_render. Takes exactly
// four randring words for the two replacement slots before scanning all 32
// byte cels and emitting any final particle presentation calls.
void bomb_particles_update_and_render(const BombState &bomb,
	BombEffectState &effects, RandomState &random,
	BombEffectCallbacks &callbacks) noexcept;

} // namespace th02::core
