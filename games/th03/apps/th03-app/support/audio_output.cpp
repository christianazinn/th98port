#include "audio_output.hpp"

#include "th03/audio/output_gain.hpp"

namespace th03::app::launcher_support {

std::expected<
	std::span<const audio::PmdCanonicalStereoSample>,
	const char *
> output_gain_frames(
	std::span<const audio::PmdCanonicalStereoSample> source,
	std::uint8_t percent,
	std::vector<audio::PmdCanonicalStereoSample> &storage
)
{
	if(!audio::valid_output_gain_percent(percent)) {
		return std::unexpected("invalid-master-volume");
	}
	if(percent == 100U) {
		return source;
	}
	storage.assign(source.begin(), source.end());
	if(!audio::apply_output_gain(storage, percent)) {
		return std::unexpected("master-volume-apply-failed");
	}
	return std::span<const audio::PmdCanonicalStereoSample>(storage);
}

} // namespace th03::app::launcher_support
