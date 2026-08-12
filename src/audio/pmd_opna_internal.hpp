#ifndef TH03_AUDIO_PMD_OPNA_INTERNAL_HPP
#define TH03_AUDIO_PMD_OPNA_INTERNAL_HPP

#include <algorithm>
#include <cstdint>
#include <limits>

namespace th03::audio::detail {

// T98-Next 13.1th's configured Sound86/FMGEN profile uses FM=20 and SSG=14.
// FMGEN's six-channel FM full scale is nominally 32768. Its three SSG channels
// at that setting sum to 3 * 2896 = 8688, giving the exact reduced ratio below.
// This is a practical emulator-reference mix, not a physical PC-98 analog-gain
// claim. ymfm's OPN lanes are signed-16-scale values in int32_t containers.
inline constexpr std::int64_t t98_next_ssg_gain_numerator = 543LL;
inline constexpr std::int64_t t98_next_ssg_gain_denominator = 2'048LL;

// Weight the shared SSG lane, mix it with one FM/ADPCM stereo lane, clamp in
// the signed-16 source domain, then promote exactly to canonical Q31/S32.
[[nodiscard]] constexpr std::int32_t pmd_mix_to_q31(
	std::int32_t stereo,
	std::int32_t shared
) noexcept
{
	const auto weighted_shared =
		(static_cast<std::int64_t>(shared) * t98_next_ssg_gain_numerator) /
		t98_next_ssg_gain_denominator;
	const auto mixed = std::clamp(
		static_cast<std::int64_t>(stereo) +
			weighted_shared,
		static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::min()),
		static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::max())
	);
	return static_cast<std::int32_t>(mixed * 65'536LL);
}

} // namespace th03::audio::detail

#endif
