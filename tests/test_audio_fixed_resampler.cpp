#include "th03/audio/fixed_resampler.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <limits>
#include <span>
#include <vector>

namespace {

using th03::audio::PmdCanonicalStereoSample;
using th03::audio::PmdFixedResampler;
using th03::audio::PmdFixedResamplerErrorCode;
using th03::audio::PmdOpnaStereoSample;

int failures = 0;
int checks = 0;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

std::size_t expected_output_count(std::size_t input_count)
{
	if(input_count == 0U) {
		return 0U;
	}
	// Output m is retained while floor(m * 52 / 15) <= input_count + 62.
	return ((input_count + 63U) * 15U + 51U) / 52U;
}

std::vector<PmdCanonicalStereoSample> resample(
	std::span<const PmdOpnaStereoSample> input,
	std::size_t input_chunk,
	std::size_t output_chunk
)
{
	PmdFixedResampler subject;
	std::vector<PmdCanonicalStereoSample> result{};
	std::vector<PmdCanonicalStereoSample> output(output_chunk);
	std::size_t cursor = 0U;
	while(cursor < input.size()) {
		const auto count = std::min(input_chunk, input.size() - cursor);
		const auto progress = subject.process(
			input.subspan(cursor, count),
			output
		);
		expect(static_cast<bool>(progress), "streaming process accepts input");
		if(!progress) {
			return result;
		}
		expect(
			progress->input_consumed != 0U || progress->output_written != 0U,
			"streaming process makes progress"
		);
		cursor += progress->input_consumed;
		result.insert(
			result.end(),
			output.begin(),
			output.begin() + static_cast<std::ptrdiff_t>(progress->output_written)
		);
	}

	for(;;) {
		const auto progress = subject.finish(output);
		result.insert(
			result.end(),
			output.begin(),
			output.begin() + static_cast<std::ptrdiff_t>(progress.output_written)
		);
		if(progress.complete) {
			break;
		}
		expect(progress.output_written != 0U, "finish makes bounded progress");
	}
	expect(subject.finished(), "finish reports terminal state");
	expect(subject.input_samples() == input.size(), "input count is exact");
	expect(subject.output_samples() == result.size(), "output count is exact");
	return result;
}

std::uint32_t sample_hash(
	std::span<const PmdCanonicalStereoSample> samples
) noexcept
{
	std::uint32_t hash = 2'166'136'261U;
	for(const auto &sample : samples) {
		hash ^= static_cast<std::uint32_t>(sample.left);
		hash *= 16'777'619U;
		hash ^= static_cast<std::uint32_t>(sample.right);
		hash *= 16'777'619U;
	}
	return hash;
}

void test_empty_finish_and_reset()
{
	PmdFixedResampler subject;
	PmdCanonicalStereoSample output{};
	const auto empty = subject.process({}, std::span(&output, 1U));
	expect(empty && empty->input_consumed == 0U, "empty input is accepted");
	expect(empty && empty->output_written == 0U, "empty input emits nothing");
	const auto first_finish = subject.finish(std::span(&output, 1U));
	expect(first_finish.complete, "empty stream finishes immediately");
	expect(first_finish.output_written == 0U, "empty finish emits nothing");
	const auto second_finish = subject.finish(std::span(&output, 1U));
	expect(second_finish.complete, "finish is idempotent");
	expect(second_finish.output_written == 0U, "repeated finish emits nothing");

	const PmdOpnaStereoSample input{.left = 1, .right = -1};
	const auto rejected = subject.process(
		std::span(&input, 1U),
		std::span(&output, 1U)
	);
	expect(!rejected, "input after finish is rejected");
	expect(
		!rejected &&
			rejected.error().code == PmdFixedResamplerErrorCode::input_after_finish,
		"input-after-finish error is typed"
	);

	subject.reset();
	const auto accepted = subject.process(
		std::span(&input, 1U),
		std::span(&output, 1U)
	);
	expect(accepted && accepted->input_consumed == 1U, "reset accepts input");
	expect(subject.input_samples() == 1U, "reset clears input count");
}

void test_output_backpressure()
{
	std::vector<PmdOpnaStereoSample> input(100U, {
		.left = 42,
		.right = -42,
	});
	PmdFixedResampler subject;
	const auto primed = subject.process(input, {});
	expect(primed && primed->input_consumed == 65U, "empty output primes lookahead only");
	expect(primed && primed->output_written == 0U, "empty output writes nothing");
	const auto blocked = subject.process(std::span(input).subspan(65U), {});
	expect(blocked && blocked->input_consumed == 0U, "ready output backpressures input");
	PmdCanonicalStereoSample output{};
	const auto released = subject.process(
		std::span(input).subspan(65U),
		std::span(&output, 1U)
	);
	expect(released && released->input_consumed == 3U, "one output releases one ratio step");
	expect(released && released->output_written == 1U, "one output slot writes one sample");
}

void test_chunk_invariance_and_reset()
{
	std::vector<PmdOpnaStereoSample> input(4'097U);
	std::uint32_t state = 0xC001D00DU;
	for(auto &sample : input) {
		state = (state * 1'664'525U) + 1'013'904'223U;
		sample.left = static_cast<std::int32_t>(state >> 3U);
		state = (state * 1'664'525U) + 1'013'904'223U;
		sample.right = static_cast<std::int32_t>(state >> 3U);
	}
	const auto whole = resample(input, input.size(), input.size());
	const auto fragmented = resample(input, 7U, 3U);
	expect(whole == fragmented, "arbitrary input/output chunks are identical");
	expect(
		whole.size() == expected_output_count(input.size()),
		"stream output length follows the exact rational tail contract"
	);

	PmdFixedResampler reused;
	std::vector<PmdCanonicalStereoSample> output(whole.size());
	const auto first = reused.process(input, output);
	expect(first && first->input_consumed == input.size(), "wide process consumes input");
	std::size_t written = first ? first->output_written : 0U;
	while(!reused.finished()) {
		const auto tail = reused.finish(std::span(output).subspan(written));
		written += tail.output_written;
	}
	expect(written == output.size(), "wide process and finish fill exact output");
	expect(output == whole, "wide process matches streaming helper");
	reused.reset();
	std::fill(output.begin(), output.end(), PmdCanonicalStereoSample{});
	const auto replay = reused.process(input, output);
	written = replay ? replay->output_written : 0U;
	while(!reused.finished()) {
		const auto tail = reused.finish(std::span(output).subspan(written));
		written += tail.output_written;
	}
	expect(output == whole, "reset reproduces the complete stream exactly");
}

void test_dc_gain_and_saturation()
{
	constexpr std::int32_t level = 123'456'789;
	std::vector<PmdOpnaStereoSample> input(1'024U, {
		.left = level,
		.right = -level,
	});
	const auto output = resample(input, 19U, 11U);
	std::size_t steady_samples = 0U;
	for(std::size_t index = 0U; index < output.size(); ++index) {
		const auto center = (index * 52U) / 15U;
		if(center >= 63U && center + 64U < input.size()) {
			expect(output[index].left == level, "steady positive DC gain is unity");
			expect(output[index].right == -level, "steady negative DC gain is unity");
			++steady_samples;
		}
	}
	expect(steady_samples > 200U, "DC gate covers a long steady interval");

	std::fill(input.begin(), input.end(), PmdOpnaStereoSample{
		.left = std::numeric_limits<std::int32_t>::max(),
		.right = std::numeric_limits<std::int32_t>::min(),
	});
	const auto extremes = resample(input, 31U, 13U);
	std::size_t steady_extremes = 0U;
	for(std::size_t index = 0U; index < extremes.size(); ++index) {
		const auto center = (index * 52U) / 15U;
		if(center >= 63U && center + 64U < input.size()) {
			expect(
				extremes[index].left == std::numeric_limits<std::int32_t>::max(),
				"steady positive full scale remains exact"
			);
			expect(
				extremes[index].right == std::numeric_limits<std::int32_t>::min(),
				"steady negative full scale remains exact"
			);
			++steady_extremes;
		}
	}
	expect(steady_extremes > 200U, "full-scale gate covers a long steady interval");
}

void test_impulse_identity()
{
	std::vector<PmdOpnaStereoSample> input(512U);
	input[200U] = PmdOpnaStereoSample{
		.left = 1 << 29U,
		.right = -(1 << 28U),
	};
	const auto output = resample(input, 23U, 17U);
	expect(
		output.size() == expected_output_count(input.size()),
		"impulse response has the bounded tail length"
	);
	const auto hash = sample_hash(output);
	std::cout << "impulse_hash=" << hash << '\n';
	expect(
		hash == 2'748'126'492U,
		"impulse response matches the frozen coefficient/runtime hash"
	);
}

double tone_rms(std::uint32_t frequency_hz)
{
	constexpr std::size_t sample_count = 16'640U;
	constexpr double pi = 3.141592653589793238462643383279502884;
	constexpr double amplitude = 1'048'576.0;
	std::vector<PmdOpnaStereoSample> input(sample_count);
	for(std::size_t index = 0U; index < input.size(); ++index) {
		const auto angle = (2.0 * pi * static_cast<double>(frequency_hz) *
			static_cast<double>(index)) /
			static_cast<double>(th03::audio::pmd_native_sample_rate_hz);
		const auto sample = static_cast<std::int32_t>(
			std::lround(amplitude * std::sin(angle))
		);
		input[index] = PmdOpnaStereoSample{.left = sample, .right = sample};
	}
	const auto output = resample(input, 257U, 73U);
	const std::size_t margin = 128U;
	double square_sum = 0.0;
	for(std::size_t index = margin; index + margin < output.size(); ++index) {
		const auto value = static_cast<double>(output[index].left);
		square_sum += value * value;
	}
	const auto retained = output.size() - (margin * 2U);
	return std::sqrt(square_sum / static_cast<double>(retained));
}

void test_frequency_response()
{
	constexpr double source_rms = 1'048'576.0 / 1.4142135623730950488;
	const auto passband = tone_rms(1'300U) / source_rms;
	const auto stopband = tone_rms(26'000U) / source_rms;
	std::cout << "passband_ratio=" << passband
		<< " stopband_ratio=" << stopband << '\n';
	expect(passband > 0.999 && passband < 1.001, "1.3 kHz passband gain is flat");
	expect(stopband < 0.001, "26 kHz stopband is below -60 dB");
}

} // namespace

int main()
{
	test_empty_finish_and_reset();
	test_output_backpressure();
	test_chunk_invariance_and_reset();
	test_dc_gain_and_saturation();
	test_impulse_identity();
	test_frequency_response();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
