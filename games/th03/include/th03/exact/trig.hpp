#ifndef TH03_EXACT_TRIG_HPP
#define TH03_EXACT_TRIG_HPP

#include <cstdint>

namespace th03::exact {

// Signed Cartesian result of master.lib's Q8 vector2() operation.
struct Vector2 {
	std::int16_t x = 0;
	std::int16_t y = 0;

	[[nodiscard]] friend constexpr bool operator==(
		const Vector2 &,
		const Vector2 &
	) = default;
};

// Exact signed Q8 entries from public-domain master.lib SinTable8/CosTable8.
// The byte angle wraps naturally over one 256-step revolution.
[[nodiscard]] std::int16_t cosine8(std::uint8_t angle) noexcept;
[[nodiscard]] std::int16_t sine8(std::uint8_t angle) noexcept;

// Source-exact vector2(): signed word length multiplied by the Q8 tables,
// followed by a signed 32-bit arithmetic shift by 8 and word narrowing.
[[nodiscard]] Vector2 vector2(
	std::uint8_t angle,
	std::int16_t length
) noexcept;

} // namespace th03::exact

#endif
