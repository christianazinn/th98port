#ifndef TH03_APP_CLASSIC_AUDIO_SESSION_HPP
#define TH03_APP_CLASSIC_AUDIO_SESSION_HPP

#include "th03/app/classic_audio.hpp"
#include "th03/app/resources.hpp"
#include "th03/app/tick_input.hpp"
#include "th03/audio/fixed_resampler.hpp"
#include "th03/audio/pmd_opna.hpp"
#include "th03/audio/th03_pmd_service.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>

namespace th03::app {

// Accepted minimum-fidelity clock policy. The fixed canonical resampler depends
// on the resulting 166,400 Hz native stream.
inline constexpr std::uint32_t classic_opna_clock_hz = 7'987'200U;
inline constexpr std::size_t classic_audio_maximum_native_frames_per_tick =
	audio::pmd_native_sample_rate_hz;

struct ClassicAudioSessionConfig final {
	std::filesystem::path data_root{};
	RationalTickRate tick_rate{};
	// Empty selects the engine's deterministic silent-rhythm mode. A supplied
	// ROM is copied by the engine and must contain exactly 8 KiB.
	std::span<const std::byte> rhythm_rom{};
	MainResourceIntegrity integrity = MainResourceIntegrity::size_and_fnv1a64;
};

enum class ClassicAudioSessionErrorCode : std::uint8_t {
	invalid_config,
	invalid_cue,
	invalid_parameter,
	command_order,
	resource_load_failed,
	engine_failed,
	service_failed,
	resampler_failed,
	allocation_failed,
};

struct ClassicAudioSessionError final {
	ClassicAudioSessionErrorCode code =
		ClassicAudioSessionErrorCode::invalid_config;
	ClassicAudioCommand command{};
	std::optional<ResourceLoadError> resource_error{};
	std::optional<audio::PmdOpnaError> engine_error{};
	std::optional<audio::Th03PmdServiceError> service_error{};
	std::optional<audio::PmdFixedResamplerError> resampler_error{};
};

struct ClassicAudioTick final {
	std::uint64_t tick_index = 0U;
	std::size_t native_frames = 0U;
	std::span<const audio::PmdCanonicalStereoSample> canonical_frames{};
	std::optional<std::uint8_t> effect_started{};
	ClassicAudioStatus status{};
};

// Platform-neutral owner of Classic audio state. Commands and logical ticks are
// synchronous; the returned canonical PCM remains valid until the next tick.
// No device clock, queue depth, or delivery result enters this type.
class ClassicAudioSession final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicAudioSession>,
		ClassicAudioSessionError
	> create(ClassicAudioSessionConfig config);

	~ClassicAudioSession();
	ClassicAudioSession(const ClassicAudioSession &) = delete;
	ClassicAudioSession &operator=(const ClassicAudioSession &) = delete;
	ClassicAudioSession(ClassicAudioSession &&) = delete;
	ClassicAudioSession &operator=(ClassicAudioSession &&) = delete;

	[[nodiscard]] std::expected<void, ClassicAudioSessionError> apply(
		ClassicAudioCommand command
	);
	// Ordinary MAIN call sites use the source one-slot priority scheduler. The
	// existing semantic play_effect command remains the forced cutscene path.
	[[nodiscard]] std::expected<bool, ClassicAudioSessionError> request_effect(
		std::uint8_t effect_id
	) noexcept;
	[[nodiscard]] std::expected<ClassicAudioTick, ClassicAudioSessionError>
	advance_tick();

	[[nodiscard]] ClassicAudioStatus status() const noexcept;
	[[nodiscard]] std::uint64_t tick_index() const noexcept;
	[[nodiscard]] std::optional<ClassicAudioCommand> loaded_song() const noexcept;

private:
	struct Impl;
	explicit ClassicAudioSession(std::unique_ptr<Impl> impl) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif
