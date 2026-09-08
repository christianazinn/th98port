#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>

namespace pc98::masterlib {

// Exact public-domain master.lib atan8[data].asm. Successful iatan2 DIVs use
// only the quotient's low byte to index these 256 entries.
inline constexpr std::array<std::uint8_t, 256U> atan8_table{{
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

[[nodiscard]] constexpr std::uint16_t iatan2_magnitude(
	std::int16_t value
) noexcept
{
	const auto bits = std::bit_cast<std::uint16_t>(value);
	return (value < 0)
		? static_cast<std::uint16_t>(0U - bits)
		: bits;
}

// Literal master.lib iatan2.asm behavior. `nullopt` models the source 8086
// unsigned word DIV fault: either a zero denominator or quotient > 0xFFFF.
// Unlike TH03's native helper, signed comparison of the two magnitude bit
// patterns is deliberate and preserves the one-INT16_MIN landmine.
[[nodiscard]] constexpr std::optional<std::uint8_t> iatan2_source(
	std::int16_t y,
	std::int16_t x
) noexcept
{
	if((std::bit_cast<std::uint16_t>(y) |
		std::bit_cast<std::uint16_t>(x)) == 0U) {
		return std::uint8_t{0U};
	}
	const auto abs_y = iatan2_magnitude(y);
	const auto abs_x = iatan2_magnitude(x);
	std::uint8_t angle{};
	if(abs_x == abs_y) {
		angle = 32U;
	} else {
		const auto x_is_larger = std::bit_cast<std::int16_t>(abs_x) >
			std::bit_cast<std::int16_t>(abs_y);
		const auto numerator = x_is_larger ? abs_y : abs_x;
		const auto denominator = x_is_larger ? abs_x : abs_y;
		if(denominator == 0U) return std::nullopt;
		const auto quotient =
			(static_cast<std::uint32_t>(numerator) << 8U) / denominator;
		if(quotient > std::numeric_limits<std::uint16_t>::max()) {
			return std::nullopt;
		}
		const auto table_angle = atan8_table[static_cast<std::uint8_t>(quotient)];
		angle = x_is_larger ? table_angle : static_cast<std::uint8_t>(
			64U - table_angle
		);
	}
	if(x < 0) angle = static_cast<std::uint8_t>(128U - angle);
	if(y < 0) angle = static_cast<std::uint8_t>(0U - angle);
	return angle;
}

} // namespace pc98::masterlib
