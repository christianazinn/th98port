#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "th03/audio/fixed_resampler.hpp"

namespace th03::app::launcher_support {

[[nodiscard]] std::expected<
	std::span<const audio::PmdCanonicalStereoSample>,
	const char *
> output_gain_frames(
	std::span<const audio::PmdCanonicalStereoSample> source,
	std::uint8_t percent,
	std::vector<audio::PmdCanonicalStereoSample> &storage
);

} // namespace th03::app::launcher_support
