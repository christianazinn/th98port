#pragma once

#include "th02/core/laser.hpp"

#include <cstdint>

namespace th02::core {

inline constexpr std::uint8_t laser_phase_active = 4U;
inline constexpr std::uint8_t laser_phase_shrink = 5U;
inline constexpr std::uint8_t laser_phase_done = 9U;

enum class LaserPresentationKind : std::uint8_t {
	charge,
	head,
	beam,
};

// A source-position super_roll_put* request. The charge uses the ordinary
// 32x32 variant while head and beam use the tiny 16x16 variant; no VRAM or
// implicit page operation is represented here.
struct LaserPresentation {
	LaserPresentationKind kind{};
	ScreenPoint topleft{};
	std::int16_t patnum{};
	bool operator==(const LaserPresentation &) const = default;
};

// The two presentation seams retain source order. They cannot mutate the
// borrowed laser, player-position, or raw-hit state; laser_render() performs
// its phase-4 collision only after emitting cap and beam requests.
class LaserUpdateCallbacks {
public:
	virtual ~LaserUpdateCallbacks() = default;
	virtual void sound_effect(std::uint8_t sound_id) noexcept = 0;
	virtual void render_laser(LaserPresentation presentation) noexcept = 0;
};

// th02/main/laser.cpp::laser_render. Uses the player's current top-left
// position, rather than either page copy, and writes only raw_hit==0 at the
// source collision point. No player immunity or miss processing belongs here.
void laser_render(const Laser &laser, const PlayerFrameState &player_frame,
	PlayerHitState &player_hit, std::int16_t scroll_line,
	LaserUpdateCallbacks &callbacks) noexcept;

// th02/main/laser.cpp::lasers_update_and_render. Advances only raw F_ALIVE
// slots, preserves byte/word phase timer arithmetic, then renders each
// nonremoved slot in source order. Charge-cell advancement and F_REMOVE->free
// remain lasers_invalidate()'s separate source owner.
void lasers_update_and_render(LaserPoolState &state,
	const PlayerFrameState &player_frame, PlayerHitState &player_hit,
	std::uint32_t stage_frame, std::int16_t scroll_line,
	LaserUpdateCallbacks &callbacks) noexcept;

} // namespace th02::core
