#ifndef TH03_AUDIO_PMD_OPNA_HPP
#define TH03_AUDIO_PMD_OPNA_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>

namespace th03::audio {

inline constexpr std::size_t ym2608_rhythm_rom_bytes = 8192U;

enum class PmdOpnaFidelity : std::uint8_t {
	minimum,
	medium,
	maximum,
};

struct PmdOpnaConfig {
	// Deliberately caller-supplied. No PC-98 hardware clock is claimed here
	// until the tracked hardware-evidence gate establishes one.
	std::uint32_t input_clock_hz = 0U;
	PmdOpnaFidelity fidelity = PmdOpnaFidelity::maximum;
	// Empty selects an explicit deterministic silent-rhythm mode. Authentic
	// playback requires the caller-supplied 8 KiB YM2608 ADPCM-A ROM.
	std::span<const std::byte> rhythm_rom{};
};

enum class PmdOpnaErrorCode : std::uint8_t {
	invalid_config,
	invalid_rhythm_rom_size,
	invalid_song_size,
	invalid_song,
	invalid_part,
	no_song_loaded,
	allocation_failed,
};

struct PmdOpnaError {
	PmdOpnaErrorCode code = PmdOpnaErrorCode::invalid_config;
	std::size_t supplied_size = 0U;
};

struct PmdOpnaStereoSample {
	std::int32_t left = 0;
	std::int32_t right = 0;
};

struct PmdOpnaRegisterWrite {
	std::uint64_t sequence = 0U;
	std::uint64_t chip_clock = 0U;
	std::uint16_t address = 0U;
	std::uint8_t value = 0U;
};

class PmdOpnaRegisterTraceSink {
public:
	virtual ~PmdOpnaRegisterTraceSink() = default;
	virtual void register_written(const PmdOpnaRegisterWrite &write) noexcept = 0;
};

struct PmdOpnaStatus {
	bool song_loaded = false;
	bool playing = false;
	bool paused = false;
	bool rhythm_rom_present = false;
	std::uint32_t native_sample_rate = 0U;
	std::uint64_t samples_rendered = 0U;
	std::uint64_t chip_clocks = 0U;
	std::uint64_t register_writes = 0U;
	std::uint32_t register_hash = 0U;
	std::uint8_t timer_b = 0U;
	std::uint32_t timer_b_count = 0U;
	std::uint8_t loop_count = 0U;
	std::uint16_t measure = 0U;
	std::uint8_t fade_volume = 0U;
	std::int8_t fade_speed = 0;
	std::uint32_t part_mask = 0U;
};

// Platform-neutral, byte-fed PMD/YM2608 engine. It owns both the immutable
// prepared song and the fresh mutable image used by one playback. Filesystem,
// SDL, device clocks, resampling, and TH03 scene/service policy stay outside.
class PmdOpnaEngine {
public:
	static std::expected<PmdOpnaEngine, PmdOpnaError> create(
		PmdOpnaConfig config
	);

	PmdOpnaEngine(PmdOpnaEngine &&) noexcept;
	PmdOpnaEngine &operator=(PmdOpnaEngine &&) noexcept;
	~PmdOpnaEngine();

	PmdOpnaEngine(const PmdOpnaEngine &) = delete;
	PmdOpnaEngine &operator=(const PmdOpnaEngine &) = delete;

	// A rejected image leaves the previously prepared song untouched.
	std::expected<void, PmdOpnaError> load_song(
		std::span<const std::byte> song
	);
	// Every play starts from a new mutable copy, including after a completed loop.
	std::expected<void, PmdOpnaError> play();
	// Source-style resident-driver controls. Stop and pause silence current chip
	// output without discarding the prepared song. A subsequent Play is always a
	// fresh start and therefore resets fade, pause, and part masks.
	void stop() noexcept;
	void pause() noexcept;
	void resume() noexcept;
	void fade(std::int8_t speed) noexcept;
	void set_volume(std::uint8_t volume) noexcept;
	// PMD part numbers 0..23 follow function 1Eh. Parts 16..23 are accepted for
	// contract completeness even though TH03 ships no PPZ data.
	std::expected<void, PmdOpnaError> set_part_mask(
		std::uint8_t part,
		bool masked
	) noexcept;
	void render_native(std::span<PmdOpnaStereoSample> output) noexcept;

	void set_register_trace_sink(PmdOpnaRegisterTraceSink *sink) noexcept;
	[[nodiscard]] PmdOpnaStatus status() const noexcept;

private:
	struct Impl;
	explicit PmdOpnaEngine(std::unique_ptr<Impl> impl) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::audio

#endif
