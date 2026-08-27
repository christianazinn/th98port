#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

#include "th03/app/new_classic/online/new_classic_online_client.hpp"

#if defined(_WIN32)
#include "th03/platform/windows_online.hpp"
#elif defined(TH03_ENABLE_CURL_ONLINE)
#include "th03/platform/curl_online.hpp"
#endif

namespace th03::app::launcher_support {

#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)

struct ProductOnlineError final {
	std::uint8_t code = 0U;
};

#if defined(_WIN32)
using ProductOnlineTransport = platform::WindowsOnlineTransport;
#else
using ProductOnlineTransport = platform::CurlOnlineTransport;
#endif

[[nodiscard]] std::expected<ProductOnlineTransport, ProductOnlineError>
create_product_online_transport(std::string origin);

[[nodiscard]] std::expected<
	std::optional<NewClassicOnlineCredentials>,
	ProductOnlineError
> load_product_online_credentials(const std::filesystem::path &path);

[[nodiscard]] std::expected<NewClassicOnlineCredentials, ProductOnlineError>
load_or_enroll_product_online_credentials(
	const std::filesystem::path &path,
	const NewClassicOnlineTransport &transport
);

[[nodiscard]] std::expected<void, ProductOnlineError>
remove_product_online_credentials(const std::filesystem::path &path);

[[nodiscard]] std::expected<void, ProductOnlineError>
store_product_online_credentials(
	const std::filesystem::path &path,
	const NewClassicOnlineCredentials &credentials
);

#endif

} // namespace th03::app::launcher_support
