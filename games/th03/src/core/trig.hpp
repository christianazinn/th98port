#ifndef TH03_CORE_TRIG_HPP
#define TH03_CORE_TRIG_HPP

#include "th03/exact/trig.hpp"

namespace th03::detail {

using exact::Vector2;
using exact::cosine8;
using exact::sine8;
using exact::vector2;

std::uint8_t iatan2(std::int16_t y, std::int16_t x) noexcept;
Vector2 vector2_between_plus(
	std::int16_t x1,
	std::int16_t y1,
	std::int16_t x2,
	std::int16_t y2,
	std::uint8_t plus_angle,
	std::int16_t length
) noexcept;

} // namespace th03::detail

#endif
