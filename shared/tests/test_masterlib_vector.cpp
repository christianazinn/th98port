#include "pc98/masterlib_vector.hpp"

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

using pc98::masterlib::Vector2;
using pc98::masterlib::cosine8;
using pc98::masterlib::sine8;
using pc98::masterlib::vector2;

std::size_t checks{};
std::size_t failures{};

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

constexpr std::array<std::int16_t, 65U> expected_cosine_quarter{{
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

[[nodiscard]] constexpr std::int16_t expected_cosine(
	std::uint8_t angle
) noexcept
{
	const auto index = static_cast<std::size_t>(angle);
	if(index <= 64U) {
		return expected_cosine_quarter[index];
	}
	if(index <= 128U) {
		return static_cast<std::int16_t>(
			-expected_cosine_quarter[128U - index]
		);
	}
	if(index <= 192U) {
		return static_cast<std::int16_t>(
			-expected_cosine_quarter[index - 128U]
		);
	}
	return expected_cosine_quarter[256U - index];
}

[[nodiscard]] constexpr std::int16_t expected_component(
	std::int16_t length,
	std::int16_t ratio
) noexcept
{
	const auto product = static_cast<std::int32_t>(length) * ratio;
	const auto shifted = (product >= 0) ? (product / 256) :
		-(((-product) + 255) / 256);
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(shifted));
}

void test_complete_table_and_vector()
{
	constexpr std::array<std::int16_t, 11U> lengths{{
		-32768, -257, -256, -255, -1, 0, 1, 255, 256, 257, 32767,
	}};
	for(std::size_t raw = 0U; raw < 256U; ++raw) {
		const auto angle = static_cast<std::uint8_t>(raw);
		expect(
			cosine8(angle) == expected_cosine(angle) &&
			sine8(angle) == expected_cosine(
				static_cast<std::uint8_t>(angle + 192U)
			),
			"Q8 master.lib table entry"
		);
		for(const auto length : lengths) {
			const Vector2 expected{
				expected_component(length, expected_cosine(angle)),
				expected_component(
					length,
					expected_cosine(static_cast<std::uint8_t>(angle + 192U))
				),
			};
			expect(
				vector2(angle, length) == expected,
				"Vector2 signed multiply, SAR 8, and word narrowing"
			);
		}
	}
}

void test_contract()
{
	static_assert(std::same_as<decltype(cosine8(0U)), std::int16_t>);
	static_assert(std::same_as<decltype(sine8(0U)), std::int16_t>);
	static_assert(std::same_as<decltype(vector2(0U, 0)), Vector2>);
	static_assert(noexcept(cosine8(0U)));
	static_assert(noexcept(sine8(0U)));
	static_assert(noexcept(vector2(0U, 0)));
	expect(
		vector2(32U, -1) == Vector2{-1, -1} &&
		vector2(128U, -32768) == Vector2{-32768, 0},
		"negative shift and word narrowing boundary"
	);
}

} // namespace

int main()
{
	test_contract();
	test_complete_table_and_vector();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return EXIT_FAILURE;
	}
	std::printf("%zu checks passed\n", checks);
	return EXIT_SUCCESS;
}
