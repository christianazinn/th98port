#include "th01/core/shots.hpp"

#include <bit>

namespace th01::core {
namespace {

std::int16_t word(int value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

void hit(Shot &shot) noexcept
{
	shot.moving = false;
	shot.decay_frame = 1;
}

} // namespace

std::optional<std::size_t> add_shot(Shots &shots, std::int16_t left, std::int16_t top) noexcept
{
	if(left < 0 || left > 639) return std::nullopt;
	for(std::size_t i = 0; i < shots.slots.size(); ++i) {
		auto &shot = shots.slots[i];
		if(shot.moving || shot.decay_frame != 0) continue;
		shot.left = left;
		shot.top = top;
		shot.moving = true;
		shot.decay_frame = 0;
		return i;
	}
	return std::nullopt;
}

std::optional<std::size_t> hit_pellet(Shots &shots, std::int16_t left, std::int16_t top) noexcept
{
	for(std::size_t i = 0; i < shots.slots.size(); ++i) {
		auto &shot = shots.slots[i];
		if(!shot.moving || shot.decay_frame == 1) continue;
		const auto dx = word(static_cast<int>(shot.left) - left);
		const auto dy = word(static_cast<int>(shot.top) - top);
		if(dx >= -14 && dx <= 6 && dy >= -14 && dy <= 6) {
			hit(shot);
			return i;
		}
	}
	return std::nullopt;
}

std::optional<std::size_t> hit_boss(Shots &shots, std::int16_t left, std::int16_t top,
	std::int16_t width_minus_shot, std::int16_t height_minus_shot) noexcept
{
	const auto right = word(static_cast<int>(left) + width_minus_shot);
	const auto bottom = word(static_cast<int>(top) + height_minus_shot);
	for(std::size_t i = 0; i < shots.slots.size(); ++i) {
		auto &shot = shots.slots[i];
		if(!shot.moving) continue;
		if(shot.left >= left && shot.left <= right && shot.top >= top && shot.top <= bottom) {
			hit(shot);
			return i;
		}
	}
	return std::nullopt;
}

} // namespace th01::core
