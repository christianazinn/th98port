#ifndef TH03_AUDIO_FIXED_RESAMPLER_HPP
#define TH03_AUDIO_FIXED_RESAMPLER_HPP

#include "th03/audio/pmd_opna.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace th03::audio {

// This is the one canonical boundary supported by the current minimum-fidelity
// PMD/OPNA engine. It is deliberately not a general host-device resampler.
inline constexpr std::uint32_t pmd_native_sample_rate_hz = 166'400U;
inline constexpr std::uint32_t pmd_canonical_sample_rate_hz = 48'000U;
inline constexpr std::size_t pmd_resampler_phase_count = 15U;
inline constexpr std::size_t pmd_resampler_tap_count = 128U;
inline constexpr std::size_t pmd_resampler_lookahead_samples = 64U;

struct PmdCanonicalStereoSample {
	std::int32_t left = 0;
	std::int32_t right = 0;

	friend constexpr bool operator==(
		const PmdCanonicalStereoSample &,
		const PmdCanonicalStereoSample &
	) = default;
};

enum class PmdFixedResamplerErrorCode : std::uint8_t {
	input_after_finish,
};

struct PmdFixedResamplerError {
	PmdFixedResamplerErrorCode code =
		PmdFixedResamplerErrorCode::input_after_finish;
};

struct PmdFixedResamplerProgress {
	std::size_t input_consumed = 0U;
	std::size_t output_written = 0U;
	bool complete = false;
};

// Fixed 166,400 -> 48,000 Hz rational polyphase FIR. Construction and all
// runtime calls are allocation-free. process() may make partial progress when
// its output span fills; callers must retain the unconsumed input suffix.
// finish() applies the specified zero extension and is bounded/idempotent.
class PmdFixedResampler final {
public:
	PmdFixedResampler() noexcept = default;

	[[nodiscard]] std::expected<
		PmdFixedResamplerProgress,
		PmdFixedResamplerError
	> process(
		std::span<const PmdOpnaStereoSample> input,
		std::span<PmdCanonicalStereoSample> output
	) noexcept;

	[[nodiscard]] PmdFixedResamplerProgress finish(
		std::span<PmdCanonicalStereoSample> output
	) noexcept;

	void reset() noexcept;

	[[nodiscard]] bool finishing() const noexcept { return finishing_; }
	[[nodiscard]] bool finished() const noexcept { return complete_; }
	[[nodiscard]] std::uint64_t input_samples() const noexcept
	{
		return real_input_samples_;
	}
	[[nodiscard]] std::uint64_t output_samples() const noexcept
	{
		return output_samples_;
	}

private:
	[[nodiscard]] bool output_ready() const noexcept;
	[[nodiscard]] bool tail_output_remaining() const noexcept;
	void push(PmdOpnaStereoSample sample) noexcept;
	[[nodiscard]] PmdCanonicalStereoSample render_one() const noexcept;
	void advance_output() noexcept;

	std::array<PmdOpnaStereoSample, pmd_resampler_tap_count> history_{};
	std::uint64_t pushed_samples_ = 0U;
	std::uint64_t real_input_samples_ = 0U;
	std::uint64_t output_samples_ = 0U;
	std::uint64_t source_center_ = 0U;
	std::uint8_t phase_ = 0U;
	bool finishing_ = false;
	bool complete_ = false;
};

} // namespace th03::audio

#endif
