#include "th03/platform/curl_update.hpp"

#if defined(_WIN32)
#error The libcurl update adapter is for non-Windows hosts.
#endif

#include <curl/curl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <utility>
#include <vector>

#include "curl_runtime_internal.hpp"

namespace th03::platform {
namespace {

constexpr long connect_timeout_ms = 5'000L;
constexpr long request_timeout_ms = 120'000L;
constexpr std::size_t maximum_url_bytes = 2048U;

class EasyHandle final {
public:
	EasyHandle() noexcept : handle_(curl_easy_init()) {}
	~EasyHandle() { if(handle_ != nullptr) { curl_easy_cleanup(handle_); } }
	EasyHandle(const EasyHandle &) = delete;
	EasyHandle &operator=(const EasyHandle &) = delete;
	[[nodiscard]] CURL *get() const noexcept { return handle_; }
	[[nodiscard]] explicit operator bool() const noexcept {
		return handle_ != nullptr;
	}
private:
	CURL *handle_ = nullptr;
};

[[nodiscard]] bool valid_url(std::string_view url) noexcept
{
	if(url.size() <= 8U || url.size() > maximum_url_bytes ||
		!url.starts_with("https://") || url.find('@') != std::string_view::npos) {
		return false;
	}
	return std::ranges::all_of(url, [](char character) {
		const auto byte = static_cast<unsigned char>(character);
		return byte >= 0x21U && byte <= 0x7EU && character != '\\' &&
			character != '#';
	});
}

[[nodiscard]] app::UpdateTransportError map_error(CURLcode code) noexcept
{
	using Error = app::UpdateTransportErrorCode;
	switch(code) {
	case CURLE_OPERATION_TIMEDOUT:
		return {Error::timeout};
	case CURLE_SSL_CONNECT_ERROR:
	case CURLE_PEER_FAILED_VERIFICATION:
	case CURLE_SSL_CERTPROBLEM:
	case CURLE_SSL_CIPHER:
		return {Error::tls};
	case CURLE_ABORTED_BY_CALLBACK:
		return {Error::canceled};
	default:
		return {Error::unavailable};
	}
}

struct ResponseBuffer final {
	std::vector<std::byte> bytes{};
	std::uint64_t maximum = 0U;
	bool exceeded = false;
};

std::size_t write_response(
	char *data,
	std::size_t size,
	std::size_t count,
	void *context
) noexcept
{
	auto &response = *static_cast<ResponseBuffer *>(context);
	if(size != 0U && count > static_cast<std::size_t>(-1) / size) {
		response.exceeded = true;
		return 0U;
	}
	const auto bytes = size * count;
	if(static_cast<std::uint64_t>(response.bytes.size()) + bytes >
		response.maximum) {
		response.exceeded = true;
		return 0U;
	}
	const auto prior = response.bytes.size();
	response.bytes.resize(prior + bytes);
	std::copy_n(
		reinterpret_cast<const std::byte *>(data), bytes,
		response.bytes.data() + prior
	);
	return bytes;
}

[[nodiscard]] std::expected<app::UpdateHttpResponse, app::UpdateTransportError>
perform(void *, const app::UpdateHttpRequest &request)
{
	if(request.maximum_response_bytes == 0U ||
		request.maximum_response_bytes >
			app::update_acquisition_max_artifact_bytes) {
		return std::unexpected(app::UpdateTransportError{
			app::UpdateTransportErrorCode::response_limit
		});
	}
	if(!valid_url(request.url)) {
		return std::unexpected(app::UpdateTransportError{
			app::UpdateTransportErrorCode::invalid_url
		});
	}
	if(!detail::curl_runtime_ready()) {
		return std::unexpected(app::UpdateTransportError{
			app::UpdateTransportErrorCode::unavailable
		});
	}
	EasyHandle easy;
	if(!easy) {
		return std::unexpected(app::UpdateTransportError{
			app::UpdateTransportErrorCode::unavailable
		});
	}
	ResponseBuffer response{
		.maximum = request.maximum_response_bytes,
	};
	response.bytes.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
		request.maximum_response_bytes, 64U * 1024U
	)));
	auto set = [&](CURLoption option, auto value) {
		return curl_easy_setopt(easy.get(), option, value) == CURLE_OK;
	};
	if(!set(CURLOPT_URL, request.url.c_str()) ||
#if LIBCURL_VERSION_NUM >= 0x075500
		!set(CURLOPT_PROTOCOLS_STR, "https") ||
		!set(CURLOPT_REDIR_PROTOCOLS_STR, "https") ||
#else
		!set(CURLOPT_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS)) ||
		!set(CURLOPT_REDIR_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS)) ||
#endif
		!set(CURLOPT_FOLLOWLOCATION, 0L) ||
		!set(CURLOPT_MAXREDIRS, 0L) ||
		!set(CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_ms) ||
		!set(CURLOPT_TIMEOUT_MS, request_timeout_ms) ||
		!set(CURLOPT_NOSIGNAL, 1L) ||
		!set(CURLOPT_SSL_VERIFYPEER, 1L) ||
		!set(CURLOPT_SSL_VERIFYHOST, 2L) ||
		!set(CURLOPT_ACCEPT_ENCODING, "identity") ||
		!set(CURLOPT_USERAGENT, "PoDD-Port-updater-v1") ||
		!set(CURLOPT_WRITEFUNCTION, &write_response) ||
		!set(CURLOPT_WRITEDATA, &response)) {
		return std::unexpected(app::UpdateTransportError{
			app::UpdateTransportErrorCode::unavailable
		});
	}
	const auto result = curl_easy_perform(easy.get());
	if(result != CURLE_OK) {
		if(response.exceeded) {
			return std::unexpected(app::UpdateTransportError{
				app::UpdateTransportErrorCode::response_limit
			});
		}
		return std::unexpected(map_error(result));
	}
	long status = 0L;
	if(curl_easy_getinfo(easy.get(), CURLINFO_RESPONSE_CODE, &status) != CURLE_OK ||
		status < 0L || status > 65'535L) {
		return std::unexpected(app::UpdateTransportError{
			app::UpdateTransportErrorCode::unavailable
		});
	}
	return app::UpdateHttpResponse{
		.status = static_cast<std::uint16_t>(status),
		.body = std::move(response.bytes),
	};
}

} // namespace

app::UpdateTransport curl_update_transport() noexcept
{
	return {.perform = &perform};
}

} // namespace th03::platform
