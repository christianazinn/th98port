#include "th01/platform/sdl_audio_device.hpp"

#include "sdl_audio_device_internal.hpp"
#include "sdl_runtime_internal.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

#include <SDL3/SDL.h>

namespace th01::platform {
namespace {

constexpr std::size_t bytes_per_frame =
	sizeof(audio::PmdCanonicalStereoSample);
constexpr std::size_t maximum_sdl_frames =
	static_cast<std::size_t>(std::numeric_limits<int>::max()) /
	bytes_per_frame;

static_assert(bytes_per_frame == 8U);
static_assert(std::is_standard_layout_v<audio::PmdCanonicalStereoSample>);

void saturating_add(std::uint64_t &target, std::size_t value) noexcept
{
	const auto bounded = static_cast<std::uint64_t>(value);
	if(target > std::numeric_limits<std::uint64_t>::max() - bounded) {
		target = std::numeric_limits<std::uint64_t>::max();
		return;
	}
	target += bounded;
}

void saturating_increment(std::uint64_t &target) noexcept
{
	if(target != std::numeric_limits<std::uint64_t>::max()) {
		++target;
	}
}

SdlAudioDeviceError error_with_sdl_detail(
	SdlAudioDeviceErrorCode code
) noexcept
{
	SdlAudioDeviceError error{.code = code};
	const char *const detail = SDL_GetError();
	if(detail == nullptr) {
		return error;
	}
	const auto text = std::string_view{detail};
	const auto count = std::min(text.size(), error.sdl_detail.size() - 1U);
	std::copy_n(text.begin(), count, error.sdl_detail.begin());
	error.sdl_detail[count] = '\0';
	return error;
}

} // namespace

struct SdlAudioDevice::Impl {
	SDL_AudioStream *stream = nullptr;
	std::size_t maximum_queued_frames = 0U;
	SdlAudioDeviceTelemetry telemetry{};
	bool paused = true;
	bool audio_initialized = false;
	bool runtime_lease = false;

	~Impl()
	{
		if(stream != nullptr) {
			SDL_DestroyAudioStream(stream);
		}
		if(audio_initialized) {
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
		}
		if(runtime_lease) {
			detail::release_platform_runtime_lease();
		}
	}

	SdlAudioDeviceError failure(SdlAudioDeviceErrorCode code) noexcept
	{
		saturating_increment(telemetry.failures);
		return error_with_sdl_detail(code);
	}

	std::expected<std::size_t, SdlAudioDeviceError> queued_frames() noexcept
	{
		const auto queued_bytes = SDL_GetAudioStreamQueued(stream);
		if(queued_bytes < 0) {
			return std::unexpected(failure(
				SdlAudioDeviceErrorCode::queue_query_failed
			));
		}
		const auto bounded = static_cast<std::size_t>(queued_bytes);
		if((bounded % bytes_per_frame) != 0U) {
			return std::unexpected(failure(
				SdlAudioDeviceErrorCode::invalid_queued_bytes
			));
		}
		return bounded / bytes_per_frame;
	}
};

SdlAudioDevice::SdlAudioDevice(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{
}

SdlAudioDevice::SdlAudioDevice(SdlAudioDevice &&) noexcept = default;
SdlAudioDevice &SdlAudioDevice::operator=(SdlAudioDevice &&) noexcept = default;
SdlAudioDevice::~SdlAudioDevice() = default;

std::expected<SdlAudioDevice, SdlAudioDeviceError> SdlAudioDevice::create(
	const SdlAudioDeviceConfig &config
)
{
	if(config.maximum_queued_frames == 0U ||
		config.maximum_queued_frames > maximum_sdl_frames) {
		return std::unexpected(SdlAudioDeviceError{
			.code = SdlAudioDeviceErrorCode::invalid_config,
		});
	}

	std::unique_ptr<Impl> impl{};
	try {
		impl = std::make_unique<Impl>();
	} catch(const std::bad_alloc &) {
		return std::unexpected(SdlAudioDeviceError{
			.code = SdlAudioDeviceErrorCode::allocation_failed,
		});
	}
	impl->maximum_queued_frames = config.maximum_queued_frames;
	if(!detail::acquire_platform_runtime_lease()) {
		return std::unexpected(SdlAudioDeviceError{
			.code = SdlAudioDeviceErrorCode::runtime_unavailable,
		});
	}
	impl->runtime_lease = true;
	if(!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		return std::unexpected(error_with_sdl_detail(
			SdlAudioDeviceErrorCode::audio_initialization_failed
		));
	}
	impl->audio_initialized = true;

	constexpr SDL_AudioSpec source{
		.format = SDL_AUDIO_S32,
		.channels = 2,
		.freq = static_cast<int>(audio::pmd_canonical_sample_rate_hz),
	};
	impl->stream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		&source,
		nullptr,
		nullptr
	);
	if(impl->stream == nullptr) {
		return std::unexpected(error_with_sdl_detail(
			SdlAudioDeviceErrorCode::stream_open_failed
		));
	}
	if(!config.start_paused && !SDL_ResumeAudioStreamDevice(impl->stream)) {
		return std::unexpected(error_with_sdl_detail(
			SdlAudioDeviceErrorCode::resume_failed
		));
	}
	impl->paused = config.start_paused;
	return SdlAudioDevice(std::move(impl));
}

std::expected<SdlAudioSubmitResult, SdlAudioDeviceError>
SdlAudioDevice::submit(
	std::span<const audio::PmdCanonicalStereoSample> frames
) noexcept
{
	if(impl_ == nullptr) {
		return std::unexpected(SdlAudioDeviceError{
			.code = SdlAudioDeviceErrorCode::runtime_unavailable,
		});
	}
	if(frames.empty()) {
		return SdlAudioSubmitResult{};
	}
	if(frames.size() > maximum_sdl_frames) {
		return std::unexpected(impl_->failure(
			SdlAudioDeviceErrorCode::invalid_frame_count
		));
	}

	auto queued = impl_->queued_frames();
	if(!queued) {
		return std::unexpected(queued.error());
	}
	std::span<const audio::PmdCanonicalStereoSample> submitted = frames;
	SdlAudioSubmitResult result{};
	if(*queued > impl_->maximum_queued_frames ||
		frames.size() > impl_->maximum_queued_frames -
			std::min(*queued, impl_->maximum_queued_frames)) {
		if(!SDL_ClearAudioStream(impl_->stream)) {
			return std::unexpected(impl_->failure(
				SdlAudioDeviceErrorCode::queue_clear_failed
			));
		}
		const auto retained = std::min(
			frames.size(), impl_->maximum_queued_frames
		);
		const auto discarded_prefix = frames.size() - retained;
		submitted = frames.last(retained);
		result.frames_discarded = *queued + discarded_prefix;
		result.discontinuity = true;
		saturating_add(impl_->telemetry.frames_discarded,
			result.frames_discarded);
		saturating_increment(impl_->telemetry.discontinuities);
	}

	const auto bytes = submitted.size() * bytes_per_frame;
	if(!SDL_PutAudioStreamData(
		impl_->stream,
		submitted.data(),
		static_cast<int>(bytes)
	)) {
		return std::unexpected(impl_->failure(
			SdlAudioDeviceErrorCode::submit_failed
		));
	}
	result.frames_submitted = submitted.size();
	saturating_add(impl_->telemetry.frames_submitted, submitted.size());
	return result;
}

std::expected<void, SdlAudioDeviceError> SdlAudioDevice::pause() noexcept
{
	if(impl_ == nullptr) {
		return std::unexpected(SdlAudioDeviceError{
			.code = SdlAudioDeviceErrorCode::runtime_unavailable,
		});
	}
	if(impl_->paused) {
		return {};
	}
	if(!SDL_PauseAudioStreamDevice(impl_->stream)) {
		return std::unexpected(impl_->failure(
			SdlAudioDeviceErrorCode::pause_failed
		));
	}
	impl_->paused = true;
	return {};
}

std::expected<void, SdlAudioDeviceError> SdlAudioDevice::resume() noexcept
{
	if(impl_ == nullptr) {
		return std::unexpected(SdlAudioDeviceError{
			.code = SdlAudioDeviceErrorCode::runtime_unavailable,
		});
	}
	if(!impl_->paused) {
		return {};
	}
	if(!SDL_ResumeAudioStreamDevice(impl_->stream)) {
		return std::unexpected(impl_->failure(
			SdlAudioDeviceErrorCode::resume_failed
		));
	}
	impl_->paused = false;
	return {};
}

std::expected<std::size_t, SdlAudioDeviceError>
SdlAudioDevice::queued_frames() noexcept
{
	if(impl_ == nullptr) {
		return std::unexpected(SdlAudioDeviceError{
			.code = SdlAudioDeviceErrorCode::runtime_unavailable,
		});
	}
	return impl_->queued_frames();
}

bool SdlAudioDevice::paused() const noexcept
{
	return impl_ == nullptr || impl_->paused;
}

std::size_t SdlAudioDevice::maximum_queued_frames() const noexcept
{
	return impl_ == nullptr ? 0U : impl_->maximum_queued_frames;
}

SdlAudioDeviceTelemetry SdlAudioDevice::telemetry() const noexcept
{
	return impl_ == nullptr ? SdlAudioDeviceTelemetry{} : impl_->telemetry;
}

std::expected<detail::SdlAudioDeviceMetrics, SdlAudioDeviceError>
SdlAudioDeviceTestAccess::metrics(SdlAudioDevice &device) noexcept
{
	if(device.impl_ == nullptr) {
		return std::unexpected(SdlAudioDeviceError{
			.code = SdlAudioDeviceErrorCode::runtime_unavailable,
		});
	}
	SDL_AudioSpec source{};
	if(!SDL_GetAudioStreamFormat(device.impl_->stream, &source, nullptr)) {
		return std::unexpected(device.impl_->failure(
			SdlAudioDeviceErrorCode::queue_query_failed
		));
	}
	return detail::SdlAudioDeviceMetrics{
		.device_id = SDL_GetAudioStreamDevice(device.impl_->stream),
		.source_format = static_cast<std::uint32_t>(source.format),
		.source_channels = source.channels,
		.source_frequency = source.freq,
		.audio_driver = SDL_GetCurrentAudioDriver() == nullptr ?
			std::string_view{} : std::string_view{SDL_GetCurrentAudioDriver()},
	};
}

} // namespace th01::platform
