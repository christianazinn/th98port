#include "runtime_environment.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <utility>

#include "th03/app/oracle/pixel_oracle_identity.hpp"

namespace th03::app::launcher_support {
namespace {

constexpr const char *pixel_oracle_log_environment = "TH03_PIXEL_ORACLE_LOG";
constexpr const char *pixel_oracle_rgb_environment =
	"TH03_PIXEL_ORACLE_RGB_DIR";
constexpr const char *pixel_oracle_identity_environment =
	"TH03_PIXEL_ORACLE_IDENTITY_LOG";
constexpr const char *pixel_oracle_fixture_environment =
	"TH03_PIXEL_ORACLE_FIXTURE_SHA256";
constexpr const char *online_origin_environment = "TH03_ONLINE_ORIGIN";
constexpr const char *official_online_origin = "https://azinn.dev";

} // namespace

std::optional<std::string> environment_value(const char *name)
{
#if defined(_MSC_VER)
	char *value = nullptr;
	std::size_t value_bytes = 0U;
	if(_dupenv_s(&value, &value_bytes, name) != 0 || value == nullptr) {
		std::free(value);
		return std::nullopt;
	}
	std::string result(value);
	std::free(value);
	return result;
#else
	const char *const value = std::getenv(name);
	return value ? std::optional<std::string>(value) : std::nullopt;
#endif
}

std::optional<std::string> product_online_origin()
{
	auto configured = environment_value(online_origin_environment);
	if(configured) {
		return configured->empty() ? std::nullopt : std::move(configured);
	}
	return std::string{official_online_origin};
}

std::expected<PixelOracleIdentityOutput, const char *>
pixel_oracle_identity_output(bool publication_oracle_enabled)
{
	const auto path = environment_value(pixel_oracle_identity_environment);
	const auto fixture = environment_value(pixel_oracle_fixture_environment);
	if(!path && !fixture) {
		return PixelOracleIdentityOutput{};
	}
	if(!path || !fixture || path->empty() || !publication_oracle_enabled ||
		!valid_pixel_oracle_fixture_sha256(*fixture)) {
		return std::unexpected("invalid-identity-oracle-config");
	}
	std::error_code error;
	if(std::filesystem::exists(*path, error) || error) {
		return std::unexpected("identity-oracle-path-used");
	}
	PixelOracleIdentityOutput output{
		.stream = std::ofstream(*path, std::ios::out | std::ios::binary),
		.fixture_sha256 = *fixture,
	};
	std::ranges::transform(
		output.fixture_sha256, output.fixture_sha256.begin(),
		[](char digit) { return static_cast<char>(std::tolower(
			static_cast<unsigned char>(digit)
		)); }
	);
	if(!output.stream || !write_t3ali1_header(output.stream)) {
		return std::unexpected("identity-oracle-open-failed");
	}
	return output;
}

platform::SdlPlatformConfig platform_config(bool force_hidden)
{
	platform::SdlPlatformConfig config{};
	config.title = platform::th03_product_window_title;
	if(const auto path = environment_value(pixel_oracle_log_environment)) {
		config.oracle_log_path = *path;
	}
	if(const auto path = environment_value(pixel_oracle_rgb_environment)) {
		config.oracle_rgb_directory = *path;
	}
	// Developer identity captures are unattended evidence runs. Hide the native
	// window from creation onward so capture cannot steal focus.
	const auto identity_path = environment_value(
		pixel_oracle_identity_environment
	);
	config.hidden = force_hidden || pixel_oracle_identity_capture_hidden(
		identity_path.value_or("")
	);
	return config;
}

} // namespace th03::app::launcher_support
