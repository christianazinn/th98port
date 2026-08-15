#ifndef TH03_PLATFORM_WINDOWS_ONLINE_HPP
#define TH03_PLATFORM_WINDOWS_ONLINE_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

#include "th03/app/new_classic_online_client.hpp"

namespace th03::platform {

enum class WindowsOnlineErrorCode : std::uint8_t {
	invalid_origin,
	invalid_path,
	status,
	open,
	read,
	write,
	flush,
	protect,
	unprotect,
	format,
	replace,
	remove,
};

struct WindowsOnlineError final {
	WindowsOnlineErrorCode code = WindowsOnlineErrorCode::invalid_origin;
	std::error_code system{};
};

class WindowsOnlineTransport final {
public:
	WindowsOnlineTransport(WindowsOnlineTransport &&) noexcept;
	WindowsOnlineTransport &operator=(WindowsOnlineTransport &&) noexcept;
	~WindowsOnlineTransport();

	WindowsOnlineTransport(const WindowsOnlineTransport &) = delete;
	WindowsOnlineTransport &operator=(const WindowsOnlineTransport &) = delete;

	[[nodiscard]] app::NewClassicOnlineTransport transport() noexcept;

private:
	struct Impl;
	explicit WindowsOnlineTransport(Impl *impl) noexcept;
	Impl *impl_ = nullptr;

	friend std::expected<WindowsOnlineTransport, WindowsOnlineError>
	create_windows_online_transport(std::string origin);
};

// Accepts one origin only: `https://host[:port]` with no credentials, query,
// fragment, or path prefix. Requests remain fixed relative `/api/th03/...`
// targets supplied by the platform-neutral transaction layer.
[[nodiscard]] std::expected<WindowsOnlineTransport, WindowsOnlineError>
create_windows_online_transport(std::string origin);

[[nodiscard]] std::expected<
	std::optional<app::NewClassicOnlineCredentials>,
	WindowsOnlineError
> load_windows_online_credentials(const std::filesystem::path &path);

[[nodiscard]] std::expected<void, WindowsOnlineError>
store_windows_online_credentials(
	const std::filesystem::path &path,
	const app::NewClassicOnlineCredentials &credentials
);

[[nodiscard]] std::expected<void, WindowsOnlineError>
remove_windows_online_credentials(const std::filesystem::path &path);

} // namespace th03::platform

#endif
