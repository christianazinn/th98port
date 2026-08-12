#ifndef TH03_PLATFORM_SDL_PLATFORM_INTERNAL_HPP
#define TH03_PLATFORM_SDL_PLATFORM_INTERNAL_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>

#include "th03/platform/sdl_platform.hpp"

namespace th03::platform::detail {

[[nodiscard]] std::expected<void, SdlPlatformError> write_locked_rgba(
	void *pixels,
	int pitch_bytes,
	const present::SourceSurface &surface
) noexcept;

// FNV-1a schema 1 over every SourceSurface field that can affect host pixel
// conversion. It is a fast localization fingerprint, not collision-proof
// evidence; a mismatch is decisive and a match still defers to raw captures.
[[nodiscard]] std::uint64_t source_surface_state_hash(
	const present::SourceSurface &surface
) noexcept;

struct SdlPlatformMetrics {
	std::uint32_t output_width = 0U;
	std::uint32_t output_height = 0U;
	float logical_x = 0.0F;
	float logical_y = 0.0F;
	float logical_width = 0.0F;
	float logical_height = 0.0F;
	float texture_width = 0.0F;
	float texture_height = 0.0F;
	std::uint32_t texture_format = 0U;
	std::int32_t texture_access = 0;
	std::int32_t texture_scale_mode = 0;
	std::uint32_t texture_blend_mode = 0U;
	std::int32_t renderer_vsync = 0;
	std::string_view video_driver{};
	std::string_view renderer_name{};
};

} // namespace th03::platform::detail

namespace th03::platform {

struct SdlPlatformTestAccess {
	[[nodiscard]] static std::expected<void, SdlPlatformError> resize(
		SdlPlatform &platform,
		std::uint32_t width,
		std::uint32_t height
	) noexcept;
	[[nodiscard]] static std::expected<
		detail::SdlPlatformMetrics,
		SdlPlatformError
	> metrics(const SdlPlatform &platform) noexcept;
};

} // namespace th03::platform

#endif
