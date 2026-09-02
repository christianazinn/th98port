#ifndef TH03_AUDIO_MMD_SOUNDFONT_HPP
#define TH03_AUDIO_MMD_SOUNDFONT_HPP

#include "th03/audio/fixed_resampler.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>

namespace th03::audio {

inline constexpr std::uint32_t mmd_ticks_per_quarter_note = 48U;
inline constexpr std::uint32_t mmd_ticks_per_measure = 192U;
// Matches the General MIDI reset default. Arrangements that need wider bends
// select their own range through RPN 0 and Data Entry.
inline constexpr float mmd_default_pitch_bend_range_semitones = 2.0F;

struct MmdSoundfontConfig final {
	std::span<const std::byte> soundfont{};
	std::uint32_t sample_rate_hz = pmd_canonical_sample_rate_hz;
};

enum class MmdSoundfontErrorCode : std::uint8_t {
	invalid_config,
	invalid_soundfont,
	invalid_song,
	unsupported_song_command,
	allocation_failed,
	synth_failed,
};

struct MmdSoundfontError final {
	MmdSoundfontErrorCode code = MmdSoundfontErrorCode::invalid_config;
	std::size_t offset = 0U;
	std::uint8_t command = 0U;
};

struct MmdSoundfontStatus final {
	bool song_loaded = false;
	bool playing = false;
	bool paused = false;
	std::uint16_t measure = 0U;
	std::uint8_t fade_volume = 0U;
	std::int8_t fade_speed = 0;
	std::uint8_t music_volume_percent = 100U;
};

// Platform-neutral KAJA MMD sequencer and SoundFont renderer. The player
// accepts the same compiled .MMD bytes used by the PC-98 MMD.COM driver. It
// implements the command subset emitted by the authenticated TH03 conversion
// pipeline and renders directly at the port's canonical 48 kHz boundary.
class MmdSoundfontEngine final {
public:
	[[nodiscard]] static std::expected<
		MmdSoundfontEngine,
		MmdSoundfontError
	> create(MmdSoundfontConfig config);

	MmdSoundfontEngine(MmdSoundfontEngine &&) noexcept;
	MmdSoundfontEngine &operator=(MmdSoundfontEngine &&) noexcept;
	~MmdSoundfontEngine();

	MmdSoundfontEngine(const MmdSoundfontEngine &) = delete;
	MmdSoundfontEngine &operator=(const MmdSoundfontEngine &) = delete;

	[[nodiscard]] std::expected<void, MmdSoundfontError> load_song(
		std::span<const std::byte> song
	);
	[[nodiscard]] std::expected<void, MmdSoundfontError> play();
	void stop() noexcept;
	void pause() noexcept;
	void resume() noexcept;
	void fade(std::int8_t speed) noexcept;
	void set_volume(std::uint8_t volume) noexcept;
	[[nodiscard]] std::expected<void, MmdSoundfontError> set_user_volume(
		std::uint8_t percent
	) noexcept;

	// Adds MIDI BGM to an existing canonical buffer, preserving PMD effects.
	[[nodiscard]] std::expected<void, MmdSoundfontError> render_mix(
		std::span<PmdCanonicalStereoSample> output
	);

	[[nodiscard]] MmdSoundfontStatus status() const noexcept;

private:
	struct Impl;
	explicit MmdSoundfontEngine(std::unique_ptr<Impl> impl) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::audio

#endif
