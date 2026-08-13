#ifndef TH03_APP_NEW_CLASSIC_PLAYBACK_INTERNAL_HPP
#define TH03_APP_NEW_CLASSIC_PLAYBACK_INTERNAL_HPP

#include <cstdint>

#include "th03/app/new_classic_playback.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/tram.hpp"

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
	terminal_mainl_bypass,
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
	[[nodiscard]] static std::optional<std::uint32_t> main_round_frame(
		const NewClassicReplayPlayback &playback
	) noexcept;
	[[nodiscard]] static std::optional<std::uint32_t> main_state_hash(
		const NewClassicReplayPlayback &playback
	) noexcept;
	[[nodiscard]] static bool terminal_mainl_bypass_taken(
		const NewClassicReplayPlayback &playback
	) noexcept;
	[[nodiscard]] static CoreInput mainl_end_input(
		CoreInput replay_input,
		CoreInput live_interface_input,
		bool registration
	) noexcept;
	[[nodiscard]] static bool mainl_error_progression_active(
		const NewClassicReplayPlayback &playback
	) noexcept;
	[[nodiscard]] static bool put_main_replay_overlay(
		present::Raster &raster,
		present::Tram &tram
	) noexcept;
};

} // namespace th03::app

#endif
