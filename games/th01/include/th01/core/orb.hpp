#pragma once

#include <bit>
#include <cstdint>
#include <optional>

namespace th01::core {

// ReC98 th01/main/player/orb.hpp. These are enum codes, not pixel deltas.
enum class OrbVelocityX : std::int16_t {
	stationary = 0, left4 = 1, right4 = 2, left8 = 3, right8 = 4,
};

struct OrbHorizontal {
	std::int16_t left{};
	OrbVelocityX velocity{};
};

enum class OrbForce : std::int16_t {
	surface = 0, top = 1, shot = 2, immediate = 3,
};

struct OrbVertical {
	double force{}, velocity{};
	std::int16_t force_frame{};
	bool operator==(const OrbVertical &) const = default;
};

// Scalar helpers from th01/main/player/orb.cpp. The stored velocity is
// clamped, but the returned pixel delta is the UNCLAMPED second evaluation.
// Unsupported nonfinite/out-of-range conversions or a non-nearest host
// rounding mode are refused without mutation; no guest fault is invented.
[[nodiscard]] std::optional<std::int16_t> update_orb_velocity_y(OrbVertical &orb) noexcept;
[[nodiscard]] bool apply_orb_force(OrbVertical &orb, OrbVelocityX &velocity_x,
	std::uint32_t &random_seed, double immediate, OrbForce force) noexcept;

[[nodiscard]] constexpr OrbVelocityX reflect_orb_x(OrbVelocityX velocity) noexcept
{
	switch(velocity) {
	case OrbVelocityX::left4: return OrbVelocityX::right4;
	case OrbVelocityX::right4: return OrbVelocityX::left4;
	case OrbVelocityX::left8: return OrbVelocityX::right8;
	case OrbVelocityX::right8: return OrbVelocityX::left8;
	default: return OrbVelocityX::stationary;
	}
}

// Complete orb_move_x helper from th01/main/player/orb.cpp. The argument
// controls displacement; the stored velocity controls edge reflection. The
// coordinate is not clamped, even when the helper overshoots a boundary.
constexpr void move_orb_x(OrbHorizontal &orb, OrbVelocityX movement) noexcept
{
	std::int16_t delta{};
	switch(movement) {
	case OrbVelocityX::left4: delta = -4; break;
	case OrbVelocityX::right4: delta = 4; break;
	case OrbVelocityX::left8: delta = -8; break;
	case OrbVelocityX::right8: delta = 8; break;
	default: break;
	}
	orb.left = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(orb.left) + static_cast<std::uint16_t>(delta)
	));
	if(orb.left <= 0 && (orb.velocity == OrbVelocityX::left4 || orb.velocity == OrbVelocityX::left8)) {
		orb.velocity = reflect_orb_x(orb.velocity);
	}
	if(orb.left >= 608 && (orb.velocity == OrbVelocityX::right4 || orb.velocity == OrbVelocityX::right8)) {
		orb.velocity = reflect_orb_x(orb.velocity);
	}
}

// Integer rotation block of orb_and_pellets_and_stage_unput_update_render__vsync_wait.
// Call after horizontal movement/reflection, only when the Orb is not in a
// portal. This deliberately retains the one-frame fourth cel (frame 9).
[[nodiscard]] constexpr std::int16_t rotate_orb(
	std::int16_t frame, OrbVelocityX velocity
) noexcept
{
	int delta = 0;
	if(velocity == OrbVelocityX::left4 || velocity == OrbVelocityX::left8) delta = 1;
	if(velocity == OrbVelocityX::right4 || velocity == OrbVelocityX::right8) delta = -1;
	frame = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<int>(frame) + delta
	));
	if(frame >= 10) frame = 0;
	if(frame < 0) frame = 9;
	return frame;
}

} // namespace th01::core
