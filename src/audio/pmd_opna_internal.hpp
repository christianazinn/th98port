#ifndef TH03_AUDIO_PMD_OPNA_INTERNAL_HPP
#define TH03_AUDIO_PMD_OPNA_INTERNAL_HPP

#include <algorithm>
#include <cstdint>
#include <limits>

namespace th03::audio::detail {

// T98-Next 13.1th's configured Sound86/FMGEN profile uses FM=20 and SSG=14.
// Sound86 interprets that SSG setting as -6 half-dB units and produces at most
// 3866 per PSG voice, or 11598 for all three. ymfm pre-scales its shared SSG
// lane by 2/3 to a 32764 maximum. 725/2048 is the nearest bounded ratio.
// With key edges consumed correctly, the exact Sound86 renderer measures a
// stable 2.40..2.61 FM amplitude ratio across 19 sounding TH03 songs. The 5/2
// center restores that reference scale. This is an emulator-reference mix,
// not a physical PC-98 analog-gain claim.
inline constexpr std::int64_t t98_next_fm_gain_numerator = 5LL;
inline constexpr std::int64_t t98_next_fm_gain_denominator = 2LL;
inline constexpr std::int64_t t98_next_ssg_gain_numerator = 725LL;
inline constexpr std::int64_t t98_next_ssg_gain_denominator = 2'048LL;

// Weight the shared SSG lane, mix it with one FM/ADPCM stereo lane, clamp in
// the signed-16 source domain, then promote exactly to canonical Q31/S32.
[[nodiscard]] constexpr std::int32_t pmd_mix_to_q31(
	std::int32_t stereo,
	std::int32_t shared,
	std::int32_t rhythm = 0
) noexcept
{
	const auto weighted_shared =
		(static_cast<std::int64_t>(shared) * t98_next_ssg_gain_numerator) /
		t98_next_ssg_gain_denominator;
	const auto mixed = std::clamp(
		((static_cast<std::int64_t>(stereo) *
			t98_next_fm_gain_numerator) / t98_next_fm_gain_denominator) +
			weighted_shared + static_cast<std::int64_t>(rhythm),
		static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::min()),
		static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::max())
	);
	return static_cast<std::int32_t>(mixed * 65'536LL);
}

} // namespace th03::audio::detail

#endif
