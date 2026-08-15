#ifndef TH03_AUDIO_OUTPUT_GAIN_HPP
#define TH03_AUDIO_OUTPUT_GAIN_HPP

#include <cstdint>
#include <span>

#include "th03/audio/fixed_resampler.hpp"

namespace th03::audio {

[[nodiscard]] constexpr bool valid_output_gain_percent(
	std::uint8_t percent
) noexcept
{
	return percent <= 100U && (percent % 5U) == 0U;
}

// Applies product-level gain only after the canonical PMD stream has been
// generated. This deliberately cannot affect source fades, effect arbitration,
// replay timing, or any subsequent canonical observation. Unity is a strict
// no-write path. Invalid values leave the entire span unchanged.
[[nodiscard]] bool apply_output_gain(
	std::span<PmdCanonicalStereoSample> frames,
	std::uint8_t percent
) noexcept;

} // namespace th03::audio

#endif
