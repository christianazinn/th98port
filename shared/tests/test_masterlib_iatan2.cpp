#include "pc98/masterlib_iatan2.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string_view>

namespace {

using pc98::masterlib::atan8_table;
using pc98::masterlib::iatan2_source;

std::size_t checks{};
std::size_t failures{};

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(stderr, "FAIL: %.*s\n", static_cast<int>(message.size()), message.data());
	}
}

// Independent transcription of public-domain atan8[data].asm, retained here
// so this test detects table movement/truncation rather than testing a table
// through itself.
constexpr std::array<std::uint8_t, 256U> reference_table{{
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

[[nodiscard]] constexpr std::uint8_t low_byte(std::uint16_t value) noexcept
{
	return static_cast<std::uint8_t>(value);
}

[[nodiscard]] constexpr std::uint8_t high_byte(std::uint16_t value) noexcept
{
	return static_cast<std::uint8_t>(value >> 8U);
}

[[nodiscard]] constexpr std::uint16_t word_negate(std::uint16_t value) noexcept
{
	return static_cast<std::uint16_t>(0U - value);
}

[[nodiscard]] constexpr std::optional<std::uint16_t> word_divide(
	std::uint16_t dx,
	std::uint16_t ax,
	std::uint16_t divisor
) noexcept
{
	if(divisor == 0U) return std::nullopt;
	const auto dividend = (static_cast<std::uint32_t>(dx) << 16U) | ax;
	const auto quotient = dividend / divisor;
	if(quotient > std::numeric_limits<std::uint16_t>::max()) return std::nullopt;
	return static_cast<std::uint16_t>(quotient);
}

// Register-level independent transcription of iatan2.asm. It intentionally
// retains CX/BP inputs and the AX/BX/DX byte shuffles preceding DIV instead of
// restating the production helper's numerator/denominator expression.
[[nodiscard]] constexpr std::optional<std::uint8_t> register_model(
	std::int16_t y,
	std::int16_t x
) noexcept
{
	const auto cx = std::bit_cast<std::uint16_t>(y);
	const auto bp = std::bit_cast<std::uint16_t>(x);
	auto ax = cx;
	if((ax | bp) == 0U) return std::uint8_t{0U};

	// mov AX,CX; CWD; XOR AX,DX; SUB AX,DX; mov BX,AX
	auto dx = static_cast<std::uint16_t>(y < 0 ? 0xFFFFU : 0U);
	ax = static_cast<std::uint16_t>(ax ^ dx);
	ax = static_cast<std::uint16_t>(ax - dx);
	auto bx = ax;
	// The corresponding x-magnitude sequence leaves the result in DX.
	ax = bp;
	dx = static_cast<std::uint16_t>(x < 0 ? 0xFFFFU : 0U);
	ax = static_cast<std::uint16_t>(ax ^ dx);
	ax = static_cast<std::uint16_t>(ax - dx);
	dx = ax;

	if(dx == bx) {
		ax = 32U;
	} else if(std::bit_cast<std::int16_t>(dx) < std::bit_cast<std::int16_t>(bx)) {
		// @@L150: AX=DX, clear DH, copy AH to DL, then form DX:AX for DIV BX.
		ax = dx;
		dx = static_cast<std::uint16_t>(low_byte(dx));
		dx = static_cast<std::uint16_t>(
			static_cast<std::uint16_t>(dx & 0xFF00U) | high_byte(ax)
		);
		ax = static_cast<std::uint16_t>(static_cast<std::uint16_t>(low_byte(ax)) << 8U);
		const auto quotient = word_divide(dx, ax, bx);
		if(!quotient) return std::nullopt;
		ax = *quotient;
		ax = static_cast<std::uint16_t>(
			(static_cast<std::uint16_t>(high_byte(ax)) << 8U) |
			static_cast<std::uint8_t>(64U - reference_table[low_byte(ax)])
		);
	} else {
		// Fallthrough path: AX=BX; BX=DX, then the same byte shuffles.
		ax = bx;
		bx = dx;
		dx = static_cast<std::uint16_t>(low_byte(dx));
		dx = static_cast<std::uint16_t>(
			static_cast<std::uint16_t>(dx & 0xFF00U) | high_byte(ax)
		);
		ax = static_cast<std::uint16_t>(static_cast<std::uint16_t>(low_byte(ax)) << 8U);
		const auto quotient = word_divide(dx, ax, bx);
		if(!quotient) return std::nullopt;
		ax = *quotient;
		ax = static_cast<std::uint16_t>(
			(static_cast<std::uint16_t>(high_byte(ax)) << 8U) |
			reference_table[low_byte(ax)]
		);
	}
	// @@L200 clears AH. The later x/y tests use BP/CX, not magnitudes.
	ax = low_byte(ax);
	if(x < 0) ax = static_cast<std::uint16_t>(word_negate(ax) + 128U);
	if(y < 0) ax = static_cast<std::uint16_t>(
		(static_cast<std::uint16_t>(high_byte(ax)) << 8U) |
		low_byte(word_negate(low_byte(ax)))
	);
	return low_byte(ax);
}

void test_table_and_defined_domain()
{
	expect(atan8_table == reference_table, "public-domain 256-byte atan table");
	constexpr std::array<std::int16_t, 7U> anchors{{
		-32767, -256, -1, 0, 1, 256, 32767,
	}};
	bool matches = true;
	for(std::int32_t raw = -32767; raw <= 32767; ++raw) {
		const auto value = static_cast<std::int16_t>(raw);
		for(const auto anchor : anchors) {
			if(iatan2_source(value, anchor) != register_model(value, anchor) ||
				iatan2_source(anchor, value) != register_model(anchor, value)) {
				matches = false;
				break;
			}
		}
		if(!matches) break;
	}
	expect(matches, "literal signed-word assembly model across ordinary word boundaries");
}

void test_all_table_indices_and_landmine()
{
	bool indices_match = true;
	for(std::uint16_t index = 0U; index < 256U; ++index) {
		for(const auto sign_y : std::array<std::int16_t, 2U>{{1, -1}}) {
			for(const auto sign_x : std::array<std::int16_t, 2U>{{1, -1}}) {
				const auto y = static_cast<std::int16_t>(index * sign_y);
				const auto x = static_cast<std::int16_t>(256 * sign_x);
				if(iatan2_source(y, x) != register_model(y, x) ||
					iatan2_source(x, y) != register_model(x, y)) {
					indices_match = false;
					break;
				}
			}
		}
	}
	expect(indices_match, "all table indices in both axis orders and all quadrants");

	constexpr auto int16_min = std::numeric_limits<std::int16_t>::min();
	bool landmine_matches = true;
	for(std::int32_t raw = std::numeric_limits<std::int16_t>::min();
		raw <= std::numeric_limits<std::int16_t>::max(); ++raw) {
		const auto other = static_cast<std::int16_t>(raw);
		const auto bits = std::bit_cast<std::uint16_t>(other);
		const auto other_magnitude = other < 0
			? static_cast<std::uint16_t>(0U - bits)
			: bits;
		const auto should_fault = other_magnitude <= 128U;
		const auto source_y = iatan2_source(int16_min, other);
		const auto source_x = iatan2_source(other, int16_min);
		if(source_y != register_model(int16_min, other) ||
			source_x != register_model(other, int16_min) ||
			source_y.has_value() == should_fault ||
			source_x.has_value() == should_fault) {
			landmine_matches = false;
			break;
		}
	}
	expect(landmine_matches, "single-INT16_MIN signed-CMP DIV faults and defined cases");
	expect(
		iatan2_source(int16_min, int16_min) == std::uint8_t{160U},
		"equal INT16_MIN magnitudes take the defined diagonal then both quadrant corrections"
	);
}

} // namespace

int main()
{
	test_table_and_defined_domain();
	test_all_table_indices_and_landmine();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return EXIT_FAILURE;
	}
	std::printf("%zu source-exact master.lib iatan2 checks passed\n", checks);
	return EXIT_SUCCESS;
}
