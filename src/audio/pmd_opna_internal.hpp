#ifndef TH03_AUDIO_PMD_OPNA_INTERNAL_HPP
#define TH03_AUDIO_PMD_OPNA_INTERNAL_HPP

#include <algorithm>
#include <cstdint>
#include <limits>

namespace th03::audio::detail {

// ymfm's OPN lanes are signed-16-scale values held in int32_t containers.
// Mix one stereo lane with the shared SSG lane, clamp in that source domain,
// then promote exactly to the Q31/S32 domain used by the canonical endpoint.
[[nodiscard]] constexpr std::int32_t pmd_mix_to_q31(
	std::int32_t stereo,
	std::int32_t shared
) noexcept
{
	const auto mixed = std::clamp(
		static_cast<std::int64_t>(stereo) +
			static_cast<std::int64_t>(shared),
		static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::min()),
		static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::max())
	);
	return static_cast<std::int32_t>(mixed * 65'536LL);
}

} // namespace th03::audio::detail

#endif
