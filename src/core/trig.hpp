#ifndef TH03_CORE_TRIG_HPP
#define TH03_CORE_TRIG_HPP

#include <cstdint>

namespace th03::detail {

struct Vector2 {
	std::int16_t x = 0;
	std::int16_t y = 0;
};

std::int16_t cosine8(std::uint8_t angle) noexcept;
std::int16_t sine8(std::uint8_t angle) noexcept;
Vector2 vector2(std::uint8_t angle, std::int16_t length) noexcept;

} // namespace th03::detail

#endif
