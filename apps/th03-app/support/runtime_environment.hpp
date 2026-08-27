#pragma once

#include <expected>
#include <fstream>
#include <optional>
#include <string>

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

} // namespace th03::app::launcher_support
