#ifndef TH01_PLATFORM_SDL_PLATFORM_HPP
#define TH01_PLATFORM_SDL_PLATFORM_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "th01/app/host_surface.hpp"

namespace th01::platform {

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
	present_vsync_failed,
	oracle_log_open_failed,
	oracle_log_write_failed,
	oracle_rgb_open_failed,
	oracle_rgb_write_failed,
};

struct SdlPlatformError {
	SdlPlatformErrorCode code = SdlPlatformErrorCode::invalid_config;
	std::optional<app::HostSurfaceError> surface_error{};
	// SDL_GetError() returns transient storage. Platform errors retain a
	// bounded copy so later SDL cleanup cannot change the diagnostic.
	std::array<char, 256U> sdl_detail{};
};

inline constexpr std::string_view th01_product_window_title =
	"The Phantasmagoria of Dim. Dream";

struct SdlPlatformConfig {
	// Consumed synchronously by create(); no title storage is borrowed.
	std::string_view title{th01_product_window_title};
	std::uint32_t initial_width = 640U;
	std::uint32_t initial_height = 400U;
	bool hidden = false;
	// Optional developer-only TSV path. Every successful publication records
	// source-domain and complete host-input hashes for PC-98 visual-oracle
	// comparison. Consumed synchronously by create().
	std::string oracle_log_path{};
	// Optional developer-only directory for one exact 640x400 PPM per
	// successful host publication plus a T3RGB1 identity manifest. The
	// directory may exist but its manifest and frame names must be unused, so
	// separate runs cannot silently mix. Consumed synchronously by create().
	std::string oracle_rgb_directory{};
};

struct SdlPresentResult {
	// Destination in physical output pixels. A drawable smaller than 640x400
	// is a successful black-only presentation rather than a stretched frame.
	std::optional<app::IntegerLetterboxRect> destination{};
	// Shared successful T3PXH2/T3RGB1 ordinal when either developer oracle is
	// active. This binds a separate app-layer identity sidecar without adding
	// route semantics to SDL or changing either frozen manifest schema.
	std::optional<std::uint64_t> oracle_publication{};
};

struct SdlPresentationOracleIdentity {
	std::optional<std::uint64_t> logical_tick{};
	std::uint32_t coalesced_ticks = 0U;
};

// Main-thread owner of the SDL video and gamepad subsystems, one resizable
// window/renderer, and one persistent 640x400 streaming texture. Simulation
// cadence is intentionally outside this class. Presentation requests strict
// interval-one VSync to prevent mid-scanout replacement on native renderers;
// waiting for a physical refresh never emits or drops a logical tick.
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
		const present::SourceSurface &surface,
		SdlPresentationOracleIdentity identity = {}
	) noexcept;
	[[nodiscard]] bool oracle_enabled() const noexcept;
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

} // namespace th01::platform

#endif
