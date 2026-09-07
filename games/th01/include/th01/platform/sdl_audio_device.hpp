#ifndef TH01_PLATFORM_SDL_AUDIO_DEVICE_HPP
#define TH01_PLATFORM_SDL_AUDIO_DEVICE_HPP

#include "th01/audio/fixed_resampler.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>

namespace th01::platform {

inline constexpr std::size_t sdl_audio_default_queue_frames = 4'800U;

enum class SdlAudioDeviceErrorCode : std::uint8_t {
	invalid_config,
	runtime_unavailable,
	audio_initialization_failed,
	stream_open_failed,
	queue_query_failed,
	invalid_queued_bytes,
	queue_clear_failed,
	invalid_frame_count,
	submit_failed,
	pause_failed,
	resume_failed,
	allocation_failed,
};

struct SdlAudioDeviceError {
	SdlAudioDeviceErrorCode code = SdlAudioDeviceErrorCode::invalid_config;
	// SDL_GetError() returns transient storage. Retain a bounded diagnostic so
	// cleanup, device threads, and subsequent calls cannot change this error.
	std::array<char, 256U> sdl_detail{};
};

struct SdlAudioDeviceConfig {
	// Canonical input frames. 4,800 frames are exactly 100 ms at 48 kHz.
	std::size_t maximum_queued_frames = sdl_audio_default_queue_frames;
	// Classic startup pre-fills one block before exposing the device clock.
	bool start_paused = false;
};

struct SdlAudioSubmitResult {
	std::size_t frames_submitted = 0U;
	// Includes stale queued frames cleared for this submission and an oversized
	// caller block's discarded prefix. The newest caller frames are retained.
	std::size_t frames_discarded = 0U;
	bool discontinuity = false;
};

struct SdlAudioDeviceTelemetry {
	std::uint64_t frames_submitted = 0U;
	std::uint64_t frames_discarded = 0U;
	std::uint64_t discontinuities = 0U;
	std::uint64_t failures = 0U;

	friend constexpr bool operator==(
		const SdlAudioDeviceTelemetry &,
		const SdlAudioDeviceTelemetry &
	) = default;
};

// Main-thread SDL playback endpoint for canonical signed-stereo 48 kHz PCM.
// A SdlPlatform must already own the SDL runtime. The device takes a subordinate
// lease, so either owner may be destroyed first. Device state and failures are
// final-output facts only; this type exposes no timing or pacing signal.
class SdlAudioDevice final {
public:
	[[nodiscard]] static std::expected<
		SdlAudioDevice,
		SdlAudioDeviceError
	> create(const SdlAudioDeviceConfig &config = {});

	SdlAudioDevice(SdlAudioDevice &&) noexcept;
	SdlAudioDevice &operator=(SdlAudioDevice &&) noexcept;
	~SdlAudioDevice();

	SdlAudioDevice(const SdlAudioDevice &) = delete;
	SdlAudioDevice &operator=(const SdlAudioDevice &) = delete;

	[[nodiscard]] std::expected<
		SdlAudioSubmitResult,
		SdlAudioDeviceError
	> submit(
		std::span<const audio::PmdCanonicalStereoSample> frames
	) noexcept;
	[[nodiscard]] std::expected<void, SdlAudioDeviceError> pause() noexcept;
	[[nodiscard]] std::expected<void, SdlAudioDeviceError> resume() noexcept;
	[[nodiscard]] std::expected<std::size_t, SdlAudioDeviceError>
	queued_frames() noexcept;

	[[nodiscard]] bool paused() const noexcept;
	[[nodiscard]] std::size_t maximum_queued_frames() const noexcept;
	[[nodiscard]] SdlAudioDeviceTelemetry telemetry() const noexcept;

private:
	friend struct SdlAudioDeviceTestAccess;
	struct Impl;
	explicit SdlAudioDevice(std::unique_ptr<Impl> impl) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th01::platform

#endif
