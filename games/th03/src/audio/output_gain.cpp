#include "th03/audio/output_gain.hpp"

#include <cstdint>

namespace th03::audio {
namespace {

[[nodiscard]] constexpr std::int32_t scale(
	std::int32_t sample,
	std::uint8_t percent
) noexcept
{
	return static_cast<std::int32_t>(
		(static_cast<std::int64_t>(sample) * percent) / 100
	);
}

} // namespace

bool apply_output_gain(
	std::span<PmdCanonicalStereoSample> frames,
	std::uint8_t percent
) noexcept
{
	if(!valid_output_gain_percent(percent)) {
		return false;
	}
	if(percent == 100U) {
		return true;
	}
	for(auto &frame : frames) {
		frame.left = scale(frame.left, percent);
		frame.right = scale(frame.right, percent);
	}
	return true;
}

} // namespace th03::audio
