#ifndef TH03_PLATFORM_SDL_PLATFORM_HPP
#define TH03_PLATFORM_SDL_PLATFORM_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string_view>

#include "th03/app/host_surface.hpp"

namespace th03::platform {

enum class SdlPlatformErrorCode : std::uint8_t {
	invalid_config,
	already_initialized,
	initialization_failed,
	window_renderer_failed,
	window_setup_failed,
	logical_presentation_failed,
	texture_failed,
	render_output_failed,
	texture_lock_failed,
	invalid_texture_pitch,
	surface_conversion_failed,
	render_failed,
};

struct SdlPlatformError {
	SdlPlatformErrorCode code = SdlPlatformErrorCode::invalid_config;
	std::optional<app::HostSurfaceError> surface_error{};
	// SDL_GetError() returns transient storage. Platform errors retain a
	// bounded copy so later SDL cleanup cannot change the diagnostic.
	std::array<char, 256U> sdl_detail{};
};

struct SdlPlatformConfig {
	// Consumed synchronously by create(); no title storage is borrowed.
	std::string_view title{"Touhou 3 Classic"};
	std::uint32_t initial_width = 640U;
	std::uint32_t initial_height = 400U;
	bool hidden = false;
};

struct SdlPresentResult {
	// Destination in physical output pixels. A drawable smaller than 640x400
	// is a successful black-only presentation rather than a stretched frame.
	std::optional<app::IntegerLetterboxRect> destination{};
};

// Main-thread owner of the SDL video and gamepad subsystems, one resizable
// window/renderer, and one persistent 640x400 streaming texture. Simulation
// cadence is intentionally outside this class; presenting never emits or
// drops a logical tick.
class SdlPlatform final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<SdlPlatform>,
		SdlPlatformError
	> create(const SdlPlatformConfig &config);

	~SdlPlatform();
	SdlPlatform(const SdlPlatform &) = delete;
	SdlPlatform &operator=(const SdlPlatform &) = delete;
	SdlPlatform(SdlPlatform &&) = delete;
	SdlPlatform &operator=(SdlPlatform &&) = delete;

	[[nodiscard]] std::expected<SdlPresentResult, SdlPlatformError> present(
		const present::SourceSurface &surface
	) noexcept;
	// Stable for this platform's lifetime and accepted directly by
	// SdlInputMapper::create(). Keeping the numeric type here leaves SDL types
	// out of this opaque host header.
	[[nodiscard]] std::uint32_t window_id() const noexcept;

private:
	friend struct SdlPlatformTestAccess;
	struct Impl;
	explicit SdlPlatform(std::unique_ptr<Impl> impl) noexcept;

	std::unique_ptr<Impl> impl_;
};

} // namespace th03::platform

#endif
