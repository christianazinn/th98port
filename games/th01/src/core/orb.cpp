#include "th01/core/orb.hpp"
#include "pc98/random.hpp"

#include <cfenv>
#include <cmath>
#include <limits>

namespace th01::core {
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559 &&
	std::numeric_limits<double>::digits == 53);

std::optional<std::int16_t> update_orb_velocity_y(OrbVertical &orb) noexcept
{
	if(std::fegetround() != FE_TONEAREST) return std::nullopt;
	const double gravity = static_cast<double>(orb.force_frame / 5) + orb.force;
	if(!std::isfinite(gravity)) return std::nullopt;
	const double delta = std::trunc(gravity);
	if(delta < -32768.0 || delta > 32767.0) return std::nullopt;
	orb.velocity = gravity > 16.0 ? 16.0 : (gravity < -16.0 ? -16.0 : gravity);
	return static_cast<std::int16_t>(delta);
}

bool apply_orb_force(OrbVertical &orb, OrbVelocityX &velocity_x,
	std::uint32_t &random_seed, double immediate, OrbForce force) noexcept
{
	if(std::fegetround() != FE_TONEAREST) return false;
	double next = orb.force;
	switch(force) {
	case OrbForce::surface: next = -orb.velocity * 0.78; break;
	case OrbForce::top: next = -orb.velocity - static_cast<double>(orb.force_frame / 4); break;
	case OrbForce::shot: next = -10.0 + orb.velocity / 2.0; break;
	case OrbForce::immediate: next = immediate; break;
	default: break; // The source still clears the frame for an unknown force code.
	}
	if(!std::isfinite(next)) return false;
	orb.force = next;
	if(force == OrbForce::surface && velocity_x == OrbVelocityX::stationary && orb.force_frame < 17) {
		// Two independent draws inside one stationary test. The second can
		// override the first even after it changed velocity_x to left4.
		if(pc98::irand(random_seed) % 50 == 0) velocity_x = OrbVelocityX::left4;
		if(pc98::irand(random_seed) % 50 == 1) velocity_x = OrbVelocityX::right4;
	}
	orb.force_frame = 0;
	return true;
}
} // namespace th01::core
