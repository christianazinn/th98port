#include "th03/exact/trig.hpp"

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

using th03::exact::Vector2;
using th03::exact::cosine8;
using th03::exact::sine8;
using th03::exact::vector2;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(
			stderr,
			"FAIL: %.*s\n",
			static_cast<int>(message.size()),
			message.data()
		);
	}
}

// The 65 nonnegative cosine entries transcribed from the pinned public-domain
// master.lib sin8[data].asm. Quadrant reconstruction below independently
// verifies every byte-angle lookup exposed by the native exact-math seam.
constexpr std::array<std::int16_t, 65U> cosine_quarter{{
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

[[nodiscard]] constexpr std::int16_t reference_cosine(
	std::uint8_t angle
) noexcept
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

[[nodiscard]] constexpr std::int16_t narrow_word(
	std::int32_t value
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(value) & 0xFFFFU
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] constexpr std::int16_t reference_component(
	std::int16_t length,
	std::int16_t ratio
) noexcept
{
	const auto product = static_cast<std::int32_t>(length) * ratio;
	const auto shifted = (product >= 0) ? (product / 256) :
		-(((-product) + 255) / 256);
	return narrow_word(shifted);
}

void test_complete_tables()
{
	for(std::size_t raw = 0U; raw < 256U; ++raw) {
		const auto angle = static_cast<std::uint8_t>(raw);
		const auto sine_angle = static_cast<std::uint8_t>(angle + 192U);
		expect(
			cosine8(angle) == reference_cosine(angle),
			"CosTable8 byte-angle entry matches master.lib"
		);
		expect(
			sine8(angle) == reference_cosine(sine_angle),
			"SinTable8 byte-angle entry matches master.lib"
		);
	}
}

void test_complete_vector_products()
{
	constexpr std::array<std::int16_t, 11U> lengths{{
		-32768, -257, -256, -255, -1, 0, 1, 255, 256, 257, 32767,
	}};
	for(const auto length : lengths) {
		for(std::size_t raw = 0U; raw < 256U; ++raw) {
			const auto angle = static_cast<std::uint8_t>(raw);
			const Vector2 expected{
				reference_component(length, reference_cosine(angle)),
				reference_component(
					length,
					reference_cosine(
						static_cast<std::uint8_t>(angle + 192U)
					)
				),
			};
			expect(
				vector2(angle, length) == expected,
				"vector2 matches signed multiply, SAR 8, and word narrowing"
			);
		}
	}
}

void test_public_contract()
{
	static_assert(std::same_as<decltype(cosine8(0U)), std::int16_t>);
	static_assert(std::same_as<decltype(sine8(0U)), std::int16_t>);
	static_assert(std::same_as<decltype(vector2(0U, 0)), Vector2>);
	static_assert(noexcept(cosine8(0U)));
	static_assert(noexcept(sine8(0U)));
	static_assert(noexcept(vector2(0U, 0)));
	static_assert(Vector2{} == Vector2{0, 0});

	expect(
		cosine8(0U) == 256 && sine8(0U) == 0 &&
		cosine8(64U) == 0 && sine8(64U) == 256 &&
		cosine8(128U) == -256 && sine8(128U) == 0 &&
		cosine8(192U) == 0 && sine8(192U) == -256,
		"public seam preserves all four master.lib cardinal vectors"
	);
	expect(
		vector2(32U, -1) == Vector2{-1, -1},
		"negative fractional products use source arithmetic shift semantics"
	);
	expect(
		vector2(128U, -32768) == Vector2{-32768, 0},
		"positive 32768 result narrows to the source signed word"
	);
}

} // namespace

int main()
{
	test_public_contract();
	test_complete_tables();
	test_complete_vector_products();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return EXIT_FAILURE;
	}
	std::printf("%zu checks passed\n", checks);
	return EXIT_SUCCESS;
}
