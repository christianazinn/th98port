#include "trig.hpp"

#include <array>
#include <cstddef>

#include "th03/core/numeric.hpp"

namespace th03::detail {
namespace {

// master.lib sin8[data].asm is public domain. Keeping its integer quarter
// wave avoids host floating-point and preserves every intentionally repeated
// table value.
constexpr std::array<std::int16_t, 65> cosine_quarter{{
	256, 256, 256, 255, 255, 254, 253, 252,
	251, 250, 248, 247, 245, 243, 241, 239,
	237, 234, 231, 229, 226, 223, 220, 216,
	213, 209, 206, 202, 198, 194, 190, 185,
	181, 177, 172, 167, 162, 157, 152, 147,
	142, 137, 132, 126, 121, 115, 109, 104,
	98, 92, 86, 80, 74, 68, 62, 56,
	50, 44, 38, 31, 25, 19, 13, 6,
	0,
}};

} // namespace

std::int16_t cosine8(std::uint8_t angle) noexcept
{
	const auto index = static_cast<std::size_t>(angle);
	if(index <= 64U) {
		return cosine_quarter[index];
	}
	if(index <= 128U) {
		return static_cast<std::int16_t>(-cosine_quarter[128U - index]);
	}
	if(index <= 192U) {
		return static_cast<std::int16_t>(-cosine_quarter[index - 128U]);
	}
	return cosine_quarter[256U - index];
}

std::int16_t sine8(std::uint8_t angle) noexcept
{
	return cosine8(static_cast<std::uint8_t>(angle + 192U));
}

Vector2 vector2(std::uint8_t angle, std::int16_t length) noexcept
{
	const auto x_wide = static_cast<std::int32_t>(length) * cosine8(angle);
	const auto y_wide = static_cast<std::int32_t>(length) * sine8(angle);
	return Vector2{
		static_cast<std::int16_t>(exact::arithmetic_shift_right(x_wide, 8U)),
		static_cast<std::int16_t>(exact::arithmetic_shift_right(y_wide, 8U)),
	};
}

} // namespace th03::detail
