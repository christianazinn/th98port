#include "th01/core/shot_orb.hpp"

#include <bit>

namespace th01::core {
namespace {

std::int16_t word_subtract(std::int16_t value, int delta) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(value) - static_cast<std::uint16_t>(delta)
	));
}

std::int16_t word_add(std::int16_t value, std::int16_t delta) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(value) + static_cast<std::uint16_t>(delta)
	));
}

std::uint8_t decay_quarter(std::uint8_t frame) noexcept
{
	return static_cast<std::uint8_t>((frame / 4U) + 1U);
}

void add_render(ShotRenderTrace &trace, ShotRenderKind kind, std::size_t slot,
	std::int16_t left, std::int16_t top, std::uint8_t quarter) noexcept
{
	// Eight slots can each issue only an unput and a put in one scan.
	trace.operations[trace.count] = {kind, slot, left, top, quarter};
	++trace.count;
}

enum class HitResult : std::uint8_t {
	no_hit,
	hit,
	unsupported_force,
	unsupported_vertical,
};

HitResult hittest_orb(Shot &shot, ShotOrbState &orb, std::uint32_t &random_seed) noexcept
{
	if(!shot.moving) return HitResult::no_hit;
	const auto delta_x = word_subtract(shot.left, orb.horizontal.left);
	const auto delta_y = word_subtract(shot.top, orb.top);
	if(delta_x >= 32 || delta_x <= -16 || delta_y >= 32 || delta_y <= -16) {
		return HitResult::no_hit;
	}
	if(delta_x > 8) {
		orb.horizontal.velocity = OrbVelocityX::left4;
	} else if(delta_x == 8) {
		orb.horizontal.velocity = OrbVelocityX::stationary;
	} else if(delta_x > -16) {
		orb.horizontal.velocity = OrbVelocityX::right4;
	}
	if(!apply_orb_force(
		orb.vertical, orb.horizontal.velocity, random_seed, 0.0, OrbForce::shot
	)) {
		return HitResult::unsupported_force;
	}
	shot.moving = false;
	move_orb_x(orb.horizontal, orb.horizontal.velocity);
	const auto delta_y_after = update_orb_velocity_y(orb.vertical);
	if(!delta_y_after) return HitResult::unsupported_vertical;
	orb.top = word_add(orb.top, *delta_y_after);
	return HitResult::hit;
}

} // namespace

ShotOrbUpdateResult update_shots_with_orb(
	Shots &shots, ShotOrbState &orb, std::uint32_t &random_seed
) noexcept
{
	ShotOrbUpdateResult result;
	for(std::size_t i = 0; i < shots.slots.size(); ++i) {
		auto &shot = shots.slots[i];
		if(shot.moving) {
			add_render(result.render, ShotRenderKind::moving_unput, i,
				shot.left, shot.top, 0);
			shot.top = word_subtract(shot.top, 12);
			if(!orb.in_portal) {
				switch(hittest_orb(shot, orb, random_seed)) {
				case HitResult::hit: continue;
				case HitResult::unsupported_force:
					result.status = ShotOrbUpdateStatus::unsupported_force;
					return result;
				case HitResult::unsupported_vertical:
					result.status = ShotOrbUpdateStatus::unsupported_vertical;
					return result;
				case HitResult::no_hit: break;
				}
			}
			if(shot.top >= 64) {
				add_render(result.render, ShotRenderKind::moving_put, i,
					shot.left, shot.top, 0);
			} else {
				shot.moving = false;
			}
		} else if(shot.decay_frame != 0) {
			add_render(result.render, ShotRenderKind::decay_unput, i,
				shot.left, shot.top, decay_quarter(shot.decay_frame));
			shot.decay_frame = static_cast<std::uint8_t>(shot.decay_frame + 1U);
			if(shot.decay_frame > 7) {
				shot.decay_frame = 0;
				shot.moving = false;
			} else {
				add_render(result.render, ShotRenderKind::decay_put, i,
					shot.left, shot.top, decay_quarter(shot.decay_frame));
			}
		}
	}
	return result;
}

} // namespace th01::core
