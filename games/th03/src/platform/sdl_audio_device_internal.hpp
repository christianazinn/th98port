#ifndef TH03_PLATFORM_SDL_AUDIO_DEVICE_INTERNAL_HPP
#define TH03_PLATFORM_SDL_AUDIO_DEVICE_INTERNAL_HPP

#include "th03/platform/sdl_audio_device.hpp"

#include <cstdint>
#include <expected>
#include <string_view>

namespace th03::platform::detail {

struct SdlAudioDeviceMetrics {
	std::uint32_t device_id = 0U;
	std::uint32_t source_format = 0U;
	std::int32_t source_channels = 0;
	std::int32_t source_frequency = 0;
	std::string_view audio_driver{};
};

} // namespace th03::platform::detail

namespace th03::platform {

struct SdlAudioDeviceTestAccess {
	[[nodiscard]] static std::expected<
		detail::SdlAudioDeviceMetrics,
		SdlAudioDeviceError
	> metrics(SdlAudioDevice &device) noexcept;
};

} // namespace th03::platform

#endif
