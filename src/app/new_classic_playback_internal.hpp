#ifndef TH03_APP_NEW_CLASSIC_PLAYBACK_INTERNAL_HPP
#define TH03_APP_NEW_CLASSIC_PLAYBACK_INTERNAL_HPP

#include <cstdint>

#include "th03/app/new_classic_playback.hpp"

namespace th03::app {

enum class NewClassicPlaybackOwner : std::uint8_t {
	main,
	story_stage_splash,
	story_win,
	story_cutscene,
	story_staff_verdict,
	story_registration,
	story_continue,
	story_game_over,
	vs_win,
	final_clear_skip,
	none,
};

struct NewClassicReplayPlaybackTestAccess final {
	[[nodiscard]] static NewClassicPlaybackOwner owner(
		const NewClassicReplayPlayback &playback
	) noexcept;
	[[nodiscard]] static std::uint32_t interstitial_sample_count(
		const NewClassicReplayPlayback &playback
	) noexcept;
	[[nodiscard]] static std::uint8_t main_input_context(
		const NewClassicReplayPlayback &playback
	) noexcept;
};

} // namespace th03::app

#endif
