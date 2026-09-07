#include "th03/audio/fixed_resampler.hpp"

#include <algorithm>
#include <limits>

namespace th03::audio {
namespace {

inline constexpr std::size_t right_span = pmd_resampler_lookahead_samples;
inline constexpr std::size_t left_span =
	pmd_resampler_tap_count - right_span - 1U;
inline constexpr std::int64_t coefficient_scale = 1LL << 30U;
inline constexpr std::int64_t coefficient_half = coefficient_scale / 2LL;

#include "fixed_resampler_coefficients.inl"

static_assert(left_span + right_span + 1U == pmd_resampler_tap_count);
static_assert(
	pmd_resampler_maximum_l1 *
		(static_cast<std::int64_t>(
			std::numeric_limits<std::int32_t>::max()
		) + 1LL) <
	std::numeric_limits<std::int64_t>::max()
);

std::int32_t round_and_saturate(std::int64_t accumulator) noexcept
{
	const auto rounded = accumulator >= 0LL ?
		(accumulator + coefficient_half) / coefficient_scale :
		-((-accumulator + coefficient_half) / coefficient_scale);
	return static_cast<std::int32_t>(std::clamp(
		rounded,
		static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
		static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())
	));
}

} // namespace

bool PmdFixedResampler::output_ready() const noexcept
{
	if(source_center_ >
		std::numeric_limits<std::uint64_t>::max() - right_span) {
		return false;
	}
	return pushed_samples_ > source_center_ + right_span;
}

bool PmdFixedResampler::tail_output_remaining() const noexcept
{
	if(real_input_samples_ == 0U) {
		return false;
	}
	const auto last_real = real_input_samples_ - 1U;
	if(last_real > std::numeric_limits<std::uint64_t>::max() - left_span) {
		return true;
	}
	return source_center_ <= last_real + left_span;
}

void PmdFixedResampler::push(PmdOpnaStereoSample sample) noexcept
{
	history_[static_cast<std::size_t>(
		pushed_samples_ % pmd_resampler_tap_count
	)] = sample;
	++pushed_samples_;
}

PmdCanonicalStereoSample PmdFixedResampler::render_one() const noexcept
{
	std::int64_t left = 0LL;
	std::int64_t right = 0LL;
	const auto &coefficients = pmd_resampler_coefficients[phase_];
	for(std::size_t tap = 0U; tap < pmd_resampler_tap_count; ++tap) {
		std::uint64_t input_index = 0U;
		if(tap < left_span) {
			const auto distance = left_span - tap;
			if(source_center_ < distance) {
				continue;
			}
			input_index = source_center_ - distance;
		} else {
			input_index = source_center_ + (tap - left_span);
		}
		const auto &sample = history_[static_cast<std::size_t>(
			input_index % pmd_resampler_tap_count
		)];
		const auto coefficient = static_cast<std::int64_t>(coefficients[tap]);
		left += static_cast<std::int64_t>(sample.left) * coefficient;
		right += static_cast<std::int64_t>(sample.right) * coefficient;
	}
	return PmdCanonicalStereoSample{
		.left = round_and_saturate(left),
		.right = round_and_saturate(right),
	};
}

void PmdFixedResampler::advance_output() noexcept
{
	const auto accumulated = static_cast<std::uint16_t>(phase_) + 52U;
	source_center_ += accumulated / pmd_resampler_phase_count;
	phase_ = static_cast<std::uint8_t>(
		accumulated % pmd_resampler_phase_count
	);
	++output_samples_;
}

std::expected<PmdFixedResamplerProgress, PmdFixedResamplerError>
PmdFixedResampler::process(
	std::span<const PmdOpnaStereoSample> input,
	std::span<PmdCanonicalStereoSample> output
) noexcept
{
	if(finishing_) {
		return std::unexpected(PmdFixedResamplerError{
			.code = PmdFixedResamplerErrorCode::input_after_finish,
		});
	}

	PmdFixedResamplerProgress progress{};
	for(;;) {
		if(output_ready()) {
			if(progress.output_written == output.size()) {
				return progress;
			}
			output[progress.output_written] = render_one();
			++progress.output_written;
			advance_output();
			continue;
		}
		if(progress.input_consumed == input.size()) {
			return progress;
		}
		push(input[progress.input_consumed]);
		++progress.input_consumed;
		++real_input_samples_;
	}
}

PmdFixedResamplerProgress PmdFixedResampler::finish(
	std::span<PmdCanonicalStereoSample> output
) noexcept
{
	finishing_ = true;
	PmdFixedResamplerProgress progress{};
	if(complete_) {
		progress.complete = true;
		return progress;
	}

	for(;;) {
		if(!tail_output_remaining()) {
			complete_ = true;
			progress.complete = true;
			return progress;
		}
		if(output_ready()) {
			if(progress.output_written == output.size()) {
				return progress;
			}
			output[progress.output_written] = render_one();
			++progress.output_written;
			advance_output();
			continue;
		}
		push(PmdOpnaStereoSample{});
	}
}

void PmdFixedResampler::reset() noexcept
{
	history_.fill(PmdOpnaStereoSample{});
	pushed_samples_ = 0U;
	real_input_samples_ = 0U;
	output_samples_ = 0U;
	source_center_ = 0U;
	phase_ = 0U;
	finishing_ = false;
	complete_ = false;
}

} // namespace th03::audio
