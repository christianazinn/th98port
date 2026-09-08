#include "trig.hpp"

#include "pc98/masterlib_vector.hpp"
#include "pc98/masterlib_iatan2.hpp"
#include "th03/core/numeric.hpp"

namespace th03 {
namespace {

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
		angle = pc98::masterlib::atan8_table[ratio];
	} else {
		const auto ratio = static_cast<std::uint16_t>(
			(static_cast<std::uint32_t>(abs_x) << 8U) / abs_y
		);
		angle = static_cast<std::uint8_t>(
			64U - pc98::masterlib::atan8_table[ratio]
		);
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
