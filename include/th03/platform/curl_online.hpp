#ifndef TH03_PLATFORM_CURL_ONLINE_HPP
#define TH03_PLATFORM_CURL_ONLINE_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

#include "th03/app/new_classic_online_client.hpp"

namespace th03::platform {

enum class CurlOnlineErrorCode : std::uint8_t {
	invalid_origin,
	unavailable,
};

struct CurlOnlineError final {
	CurlOnlineErrorCode code = CurlOnlineErrorCode::invalid_origin;
};

class CurlOnlineTransport final {
public:
	CurlOnlineTransport(CurlOnlineTransport &&) noexcept = default;
	CurlOnlineTransport &operator=(CurlOnlineTransport &&) noexcept = default;

	CurlOnlineTransport(const CurlOnlineTransport &) = delete;
	CurlOnlineTransport &operator=(const CurlOnlineTransport &) = delete;

	[[nodiscard]] app::NewClassicOnlineTransport transport() noexcept;

private:
	explicit CurlOnlineTransport(std::string origin) noexcept;
	static std::expected<
		app::NewClassicOnlineHttpResponse,
		app::NewClassicOnlineTransportError
	> perform(void *, const app::NewClassicOnlineHttpRequest &);

	std::string origin_{};

	friend std::expected<CurlOnlineTransport, CurlOnlineError>
	create_curl_online_transport(std::string origin);
};

// Accepts one origin only: `https://host[:port]` with no credentials, query,
// fragment, or path prefix. Request targets remain fixed relative API paths.
[[nodiscard]] std::expected<CurlOnlineTransport, CurlOnlineError>
create_curl_online_transport(std::string origin);

enum class PosixOnlineErrorCode : std::uint8_t {
	invalid_path,
	status,
	open,
	read,
	write,
	flush,
	format,
	permissions,
	publish,
	remove,
};

struct PosixOnlineError final {
	PosixOnlineErrorCode code = PosixOnlineErrorCode::invalid_path;
	std::error_code system{};
};

enum class PosixOnlineIdentityErrorCode : std::uint8_t {
	load,
	enroll,
	store,
};

struct PosixOnlineIdentityError final {
	PosixOnlineIdentityErrorCode code = PosixOnlineIdentityErrorCode::load;
	std::optional<PosixOnlineError> platform{};
	std::optional<app::NewClassicOnlineClientError> client{};
};

[[nodiscard]] std::expected<
	std::optional<app::NewClassicOnlineCredentials>,
	PosixOnlineError
> load_posix_online_credentials(const std::filesystem::path &path);

[[nodiscard]] std::expected<void, PosixOnlineError>
store_posix_online_credentials(
	const std::filesystem::path &path,
	const app::NewClassicOnlineCredentials &credentials
);

[[nodiscard]] std::expected<void, PosixOnlineError>
remove_posix_online_credentials(const std::filesystem::path &path);

[[nodiscard]] std::expected<
	app::NewClassicOnlineCredentials,
	PosixOnlineIdentityError
> load_or_enroll_posix_online_credentials(
	const std::filesystem::path &path,
	const app::NewClassicOnlineTransport &transport
);

} // namespace th03::platform

#endif
