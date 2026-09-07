#pragma once

#include <cstdint>
#include <expected>
#include <fstream>
#include <optional>
#include <string>

#include "th03/app/new_classic/persistence/new_classic_settings.hpp"
#include "th03/platform/sdl_platform.hpp"

namespace th03::app::launcher_support {

inline constexpr const char *pixel_oracle_selector_environment =
	"TH03_PIXEL_ORACLE_SELECTOR";
inline constexpr const char *online_user_environment = "TH03_ONLINE_USER_ID";
inline constexpr const char *online_token_environment = "TH03_ONLINE_TOKEN";
inline constexpr const char *netplay_ice_environment =
	"TH03_NETPLAY_ICE_SERVERS";

struct PixelOracleIdentityOutput final {
	std::ofstream stream{};
	std::string fixture_sha256{};

	[[nodiscard]] bool enabled() const noexcept { return stream.is_open(); }
};

[[nodiscard]] std::optional<std::string> environment_value(const char *name);

[[nodiscard]] std::optional<std::string> product_online_origin();

[[nodiscard]] std::expected<PixelOracleIdentityOutput, const char *>
pixel_oracle_identity_output(bool publication_oracle_enabled);

[[nodiscard]] platform::SdlPlatformConfig platform_config(
	bool force_hidden = false
);

[[nodiscard]] constexpr platform::SdlWindowPresentation
platform_window_presentation(
	NewClassicWindowPresentation presentation
) noexcept
{
	return {
		.mode = presentation.mode == NewClassicWindowMode::windowed ?
			platform::SdlWindowMode::windowed :
			platform::SdlWindowMode::borderless_fullscreen,
		.integer_scale = static_cast<std::uint8_t>(
			presentation.scale == NewClassicWindowScale::automatic ? 0U :
				static_cast<unsigned>(presentation.scale) + 1U
		),
		.aspect = presentation.aspect == NewClassicAspectRatio::pixels_8_5 ?
			platform::SdlAspectRatio::pixels_8_5 :
			platform::SdlAspectRatio::display_4_3,
		.filtering = presentation.filtering == NewClassicFiltering::nearest ?
			platform::SdlFiltering::nearest : platform::SdlFiltering::linear,
	};
}

} // namespace th03::app::launcher_support
