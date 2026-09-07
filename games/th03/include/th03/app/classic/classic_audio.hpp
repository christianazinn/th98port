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
	pause_song,
	resume_song,
	fade_song,
	load_effects,
	reset_effects,
	dispatch_effect,
	play_effect,
	set_bgm_active,
	set_effects_active,
	set_music_volume,
	set_effect_volume,
};

// set_bgm_active retains its historical command name for replay/rollback
// stability. Its parameter is a ClassicBgmMode value: 0=Off, 1=FM, 2=MIDI.

enum class ClassicMusicCue : std::uint8_t {
	none,
	menu_main,
	character_select,
	gameplay,
	music_room_track,
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

// TH03's WIN.M and DEMO1.M presentation waits count PMD quarter notes. All
// other MMD cues expose ordinary four-quarter-note measures to source scenes.
[[nodiscard]] constexpr std::uint32_t
classic_midi_presentation_ticks_per_measure(ClassicMusicCue cue) noexcept
{
	return (cue == ClassicMusicCue::winner ||
		cue == ClassicMusicCue::cutscene_one) ? 48U : 192U;
}

// Parameter domain for ClassicMusicCue::gameplay. The order follows the
// contiguous Music Room filenames: 00MM.M through 06MM.M, DEC.M, 07MM.M,
// and 08MM.M.
enum class ClassicGameplayMusic : std::uint8_t {
	stage_00,
	stage_01,
	stage_02,
	stage_03,
	stage_04,
	stage_05,
	stage_06,
	decision,
	stage_07,
	stage_08,
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
	// Unlike bgm_active, this reflects the PMD driver's current Play/Stop
	// state. Host-owned transitions can therefore wait for a non-looping cue
	// to end without inventing a wall-clock duration.
	bool song_playing = false;
	std::uint16_t measure = 0U;
	std::uint8_t volume = 0U;
};

} // namespace th03::app

#endif
