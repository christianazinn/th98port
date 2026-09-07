#include "product_online.hpp"

#include <utility>

namespace th03::app::launcher_support {

#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)

[[nodiscard]] std::expected<ProductOnlineTransport, ProductOnlineError>
create_product_online_transport(std::string origin)
{
#if defined(_WIN32)
	auto created = platform::create_windows_online_transport(std::move(origin));
#else
	auto created = platform::create_curl_online_transport(std::move(origin));
#endif
	if(!created) {
		return std::unexpected(ProductOnlineError{
			static_cast<std::uint8_t>(created.error().code)
		});
	}
	return std::move(*created);
}

[[nodiscard]] std::expected<
	std::optional<NewClassicOnlineCredentials>,
	ProductOnlineError
> load_product_online_credentials(const std::filesystem::path &path)
{
#if defined(_WIN32)
	auto loaded = platform::load_windows_online_credentials(path);
#else
	auto loaded = platform::load_posix_online_credentials(path);
#endif
	if(!loaded) {
		return std::unexpected(ProductOnlineError{
			static_cast<std::uint8_t>(loaded.error().code)
		});
	}
	return std::move(*loaded);
}

[[nodiscard]] std::expected<NewClassicOnlineCredentials, ProductOnlineError>
load_or_enroll_product_online_credentials(
	const std::filesystem::path &path,
	const NewClassicOnlineTransport &transport
)
{
#if defined(_WIN32)
	auto acquired = platform::load_or_enroll_windows_online_credentials(
		path, transport
	);
#else
	auto acquired = platform::load_or_enroll_posix_online_credentials(
		path, transport
	);
#endif
	if(!acquired) {
		return std::unexpected(ProductOnlineError{
			static_cast<std::uint8_t>(acquired.error().code)
		});
	}
	return std::move(*acquired);
}

[[nodiscard]] std::expected<void, ProductOnlineError>
remove_product_online_credentials(const std::filesystem::path &path)
{
#if defined(_WIN32)
	auto removed = platform::remove_windows_online_credentials(path);
#else
	auto removed = platform::remove_posix_online_credentials(path);
#endif
	if(!removed) {
		return std::unexpected(ProductOnlineError{
			static_cast<std::uint8_t>(removed.error().code)
		});
	}
	return {};
}

[[nodiscard]] std::expected<void, ProductOnlineError>
store_product_online_credentials(
	const std::filesystem::path &path,
	const NewClassicOnlineCredentials &credentials
)
{
#if defined(_WIN32)
	auto stored = platform::store_windows_online_credentials(path, credentials);
#else
	auto stored = platform::store_posix_online_credentials(path, credentials);
#endif
	if(!stored) {
		return std::unexpected(ProductOnlineError{
			static_cast<std::uint8_t>(stored.error().code)
		});
	}
	return {};
}

#endif

} // namespace th03::app::launcher_support
