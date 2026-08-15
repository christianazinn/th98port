#include "th03/audio/output_gain.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>

namespace {

int checks = 0;
int failures = 0;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

[[nodiscard]] constexpr std::int32_t reference(
	std::int32_t value,
	std::uint8_t percent
) noexcept
{
	return static_cast<std::int32_t>(
		(static_cast<std::int64_t>(value) * percent) / 100
	);
}

void test_validation_and_atomic_rejection()
{
	for(std::uint16_t value = 0U; value <= 255U; ++value) {
		const bool expected = value <= 100U && (value % 5U) == 0U;
		expect(
			th03::audio::valid_output_gain_percent(
				static_cast<std::uint8_t>(value)
			) == expected,
			"valid percentages are exactly 0..100 in 5% steps"
		);
	}
	std::array frames{
		th03::audio::PmdCanonicalStereoSample{123, -456},
		th03::audio::PmdCanonicalStereoSample{-789, 1011},
	};
	const auto before = frames;
	expect(
		!th03::audio::apply_output_gain(frames, 63U),
		"invalid gain is rejected"
	);
	expect(frames == before, "invalid gain is atomic");
}

void test_unity_silence_and_signed_scaling()
{
	std::array source{
		th03::audio::PmdCanonicalStereoSample{
			std::numeric_limits<std::int32_t>::min(),
			std::numeric_limits<std::int32_t>::max(),
		},
		th03::audio::PmdCanonicalStereoSample{-101, 101},
		th03::audio::PmdCanonicalStereoSample{-1, 1},
	};
	const auto unity = source;
	auto actual = source;
	expect(
		th03::audio::apply_output_gain(actual, 100U),
		"unity gain is accepted"
	);
	expect(actual == unity, "unity gain is bit-identical");

	actual = source;
	expect(
		th03::audio::apply_output_gain(actual, 0U),
		"zero gain is accepted"
	);
	for(const auto &frame : actual) {
		expect(frame.left == 0 && frame.right == 0, "zero gain is silent");
	}

	constexpr std::array<std::uint8_t, 3U> percentages{{5U, 35U, 95U}};
	for(const auto percent : percentages) {
		actual = source;
		expect(
			th03::audio::apply_output_gain(actual, percent),
			"fractional gain is accepted"
		);
		for(std::size_t index = 0U; index < source.size(); ++index) {
			expect(
				actual[index].left == reference(source[index].left, percent) &&
				actual[index].right == reference(source[index].right, percent),
				"signed scaling uses deterministic integer division"
			);
		}
	}
}

} // namespace

int main()
{
	test_validation_and_atomic_rejection();
	test_unity_silence_and_signed_scaling();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
