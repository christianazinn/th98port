#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace pc98::masterlib {

// Signed Cartesian result of master.lib's Q8 Vector2 operation.
struct Vector2 {
	std::int16_t x{}, y{};
	bool operator==(const Vector2 &) const = default;
};

// Exact public-domain master.lib sin8[data].asm quarter wave. Keep repeated
// values: callers use this integer table, not host floating-point trigonometry.
inline constexpr std::array<std::int16_t, 65U> cosine_quarter{{
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

[[nodiscard]] constexpr std::int16_t cosine8(std::uint8_t angle) noexcept
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

[[nodiscard]] constexpr std::int16_t sine8(std::uint8_t angle) noexcept
{
	return cosine8(static_cast<std::uint8_t>(angle + 192U));
}

[[nodiscard]] constexpr std::int16_t vector_component(
	std::int16_t length,
	std::int16_t ratio
) noexcept
{
	const auto product = static_cast<std::int32_t>(length) * ratio;
	auto shifted = std::bit_cast<std::uint32_t>(product) >> 8U;
	if(product < 0) {
		shifted |= 0xFF000000U;
	}
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(shifted));
}

// Vector2: signed word length multiplied by the Q8 table, SAR 8, then word
// narrowing. This is deliberately the complete shared master.lib primitive,
// not a host-math approximation.
[[nodiscard]] constexpr Vector2 vector2(
	std::uint8_t angle,
	std::int16_t length
) noexcept
{
	return Vector2{
		vector_component(length, cosine8(angle)),
		vector_component(length, sine8(angle)),
	};
}

} // namespace pc98::masterlib
