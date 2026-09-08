#include "trig.hpp"

#include <array>

#include "pc98/masterlib_vector.hpp"
#include "th03/core/numeric.hpp"

namespace th03 {
namespace {

// public-domain master.lib atan8[data].asm. The assembly indexes entries
// 0..255; the equal-axis case is handled directly as angle 32.
constexpr std::array<std::uint8_t, 256> atan_table{{
	0,0,0,0,1,1,1,1,1,1,2,2,2,2,2,2,
	3,3,3,3,3,3,3,4,4,4,4,4,4,5,5,5,
	5,5,5,6,6,6,6,6,6,6,7,7,7,7,7,7,
	8,8,8,8,8,8,8,9,9,9,9,9,9,10,10,10,
	10,10,10,10,11,11,11,11,11,11,11,12,12,12,12,12,
	12,12,13,13,13,13,13,13,13,14,14,14,14,14,14,14,
	15,15,15,15,15,15,15,16,16,16,16,16,16,16,17,17,
	17,17,17,17,17,17,18,18,18,18,18,18,18,19,19,19,
	19,19,19,19,19,20,20,20,20,20,20,20,20,21,21,21,
	21,21,21,21,21,21,22,22,22,22,22,22,22,22,23,23,
	23,23,23,23,23,23,23,24,24,24,24,24,24,24,24,24,
	25,25,25,25,25,25,25,25,25,25,26,26,26,26,26,26,
	26,26,26,27,27,27,27,27,27,27,27,27,27,28,28,28,
	28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29,
	29,29,29,30,30,30,30,30,30,30,30,30,30,30,31,31,
	31,31,31,31,31,31,31,31,31,31,32,32,32,32,32,32,
}};

std::uint16_t magnitude(std::int16_t value) noexcept
{
	const auto bits = exact::bits(value);
	return (value < 0)
		? static_cast<std::uint16_t>(0U - bits)
		: bits;
}

} // namespace

namespace exact {

std::int16_t cosine8(std::uint8_t angle) noexcept
{
	return pc98::masterlib::cosine8(angle);
}

std::int16_t sine8(std::uint8_t angle) noexcept
{
	return pc98::masterlib::sine8(angle);
}

} // namespace exact

namespace detail {

std::uint8_t iatan2(std::int16_t y, std::int16_t x) noexcept
{
	if((exact::bits(y) | exact::bits(x)) == 0U) {
		return 0U;
	}
	const auto abs_y = magnitude(y);
	const auto abs_x = magnitude(x);
	// iatan2.asm compares these words with signed JL. If exactly one input is
	// INT16_MIN, that selects the larger unsigned magnitude as the DIV numerator:
	// the quotient's low byte becomes the XLAT index, or DIV faults when the
	// other magnitude is at most 128. Stock gameplay cannot reach this landmine,
	// so the native core deliberately uses unsigned magnitude order to provide a
	// deterministic result over its complete input domain.
	const auto x_magnitude_is_larger = (abs_x > abs_y);
	std::uint8_t angle = 0U;
	if(abs_x == abs_y) {
		angle = 32U;
	} else if(x_magnitude_is_larger) {
		const auto ratio = static_cast<std::uint16_t>(
			(static_cast<std::uint32_t>(abs_y) << 8U) / abs_x
		);
		angle = atan_table[ratio];
	} else {
		const auto ratio = static_cast<std::uint16_t>(
			(static_cast<std::uint32_t>(abs_x) << 8U) / abs_y
		);
		angle = static_cast<std::uint8_t>(64U - atan_table[ratio]);
	}
	if(x < 0) {
		angle = static_cast<std::uint8_t>(128U - angle);
	}
	if(y < 0) {
		angle = static_cast<std::uint8_t>(0U - angle);
	}
	return angle;
}

} // namespace detail

namespace exact {

Vector2 vector2(std::uint8_t angle, std::int16_t length) noexcept
{
	const auto result = pc98::masterlib::vector2(angle, length);
	return Vector2{
		result.x,
		result.y,
	};
}

} // namespace exact

namespace detail {

Vector2 vector2_between_plus(
	std::int16_t x1,
	std::int16_t y1,
	std::int16_t x2,
	std::int16_t y2,
	std::uint8_t plus_angle,
	std::int16_t length
) noexcept
{
	const auto angle = static_cast<std::uint8_t>(
		iatan2(exact::wrap_subtract(y2, y1), exact::wrap_subtract(x2, x1)) +
		plus_angle
	);
	return exact::vector2(angle, length);
}

} // namespace detail
} // namespace th03
