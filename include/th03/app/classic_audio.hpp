#ifndef TH03_APP_CLASSIC_AUDIO_HPP
#define TH03_APP_CLASSIC_AUDIO_HPP

#include <cstdint>

namespace th03::app {

// Application-scene audio intent. A later byte-fed PMD engine consumes these
// requests and supplies status; scenes never inspect an audio device or wait on
// a host callback.
enum class ClassicAudioCommandType : std::uint8_t {
	load_song,
	play_song,
	stop_song,
	fade_song,
	load_effects,
	play_effect,
};

enum class ClassicMusicCue : std::uint8_t {
	none,
	menu_main,
	winner,
	game_over,
	staff,
	high_score,
	cutscene_one,
	cutscene_two,
	cutscene_three,
	cutscene_four,
	cutscene_five,
};

struct ClassicAudioCommand final {
	ClassicAudioCommandType type = ClassicAudioCommandType::stop_song;
	ClassicMusicCue cue = ClassicMusicCue::none;
	// Cutscene script parameters remain 16-bit through the semantic boundary;
	// stock data stays within the PMD byte domain, while malformed/modified
	// scripts must not be silently narrowed by the scene driver.
	std::uint16_t parameter = 0U;

	friend constexpr bool operator==(
		const ClassicAudioCommand &,
		const ClassicAudioCommand &
	) = default;
};

// PMD measures are cumulative across the first playback pass. Scenes latch
// bgm_active when entering each source blocking call, matching
// snd_delay_until_measure() rather than changing policy halfway through it.
struct ClassicAudioStatus final {
	bool bgm_active = false;
	std::uint16_t measure = 0U;
	std::uint8_t volume = 0U;
};

} // namespace th03::app

#endif
