#include "th03/platform/windows_update.hpp"

#if !defined(_WIN32)
#error The Windows update adapter requires Win32.
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace th03::platform {
namespace {

constexpr int connect_timeout_ms = 5'000;
constexpr int request_timeout_ms = 120'000;
constexpr std::size_t maximum_url_bytes = 2048U;

class InternetHandle final {
public:
	InternetHandle() = default;
	explicit InternetHandle(HINTERNET handle) noexcept : handle_(handle) {}
	~InternetHandle() {
		if(handle_ != nullptr) {
			(void)WinHttpCloseHandle(handle_);
		}
	}
	InternetHandle(InternetHandle &&other) noexcept : handle_(other.handle_) {
		other.handle_ = nullptr;
	}
	InternetHandle(const InternetHandle &) = delete;
	InternetHandle &operator=(const InternetHandle &) = delete;
	[[nodiscard]] HINTERNET get() const noexcept { return handle_; }
	[[nodiscard]] explicit operator bool() const noexcept {
		return handle_ != nullptr;
	}
private:
	HINTERNET handle_ = nullptr;
};

[[nodiscard]] std::optional<std::wstring> ascii_wide(std::string_view input)
{
	std::wstring result;
	result.reserve(input.size());
	for(const auto character : input) {
		const auto byte = static_cast<unsigned char>(character);
		if(byte > 0x7FU || byte < 0x21U || character == '\\' ||
			character == '#') {
			return std::nullopt;
		}
		result.push_back(static_cast<wchar_t>(byte));
	}
	return result;
}

[[nodiscard]] app::UpdateTransportError transport_error(DWORD system) noexcept
{
	using Code = app::UpdateTransportErrorCode;
	if(system == ERROR_WINHTTP_TIMEOUT) {
		return {Code::timeout};
	}
	if(system == ERROR_WINHTTP_SECURE_FAILURE ||
		system == ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED ||
		system == ERROR_WINHTTP_SECURE_INVALID_CERT) {
		return {Code::tls};
	}
	if(system == ERROR_CANCELLED || system == ERROR_OPERATION_ABORTED) {
		return {Code::canceled};
	}
	return {Code::unavailable};
}

struct ParsedUrl final {
	std::wstring host{};
	std::wstring target{};
	INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
};

[[nodiscard]] std::optional<ParsedUrl> parse_url(std::string_view url)
{
	if(url.size() <= 8U || url.size() > maximum_url_bytes ||
		!url.starts_with("https://") || url.find('@') != std::string_view::npos) {
		return std::nullopt;
	}
	const auto wide = ascii_wide(url);
	if(!wide) {
		return std::nullopt;
	}
	URL_COMPONENTS parts{};
	parts.dwStructSize = sizeof(parts);
	parts.dwHostNameLength = static_cast<DWORD>(-1);
	parts.dwUrlPathLength = static_cast<DWORD>(-1);
	parts.dwExtraInfoLength = static_cast<DWORD>(-1);
	parts.dwUserNameLength = static_cast<DWORD>(-1);
	parts.dwPasswordLength = static_cast<DWORD>(-1);
	if(WinHttpCrackUrl(wide->c_str(), 0U, 0U, &parts) == FALSE ||
		parts.nScheme != INTERNET_SCHEME_HTTPS || parts.dwHostNameLength == 0U ||
		parts.dwUserNameLength != 0U || parts.dwPasswordLength != 0U) {
		return std::nullopt;
	}
	ParsedUrl result{};
	result.host.assign(parts.lpszHostName, parts.dwHostNameLength);
	result.port = parts.nPort;
	if(parts.dwUrlPathLength == 0U) {
		result.target = L"/";
	} else {
		result.target.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
	}
	if(parts.dwExtraInfoLength != 0U) {
		result.target.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
	}
	return result;
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
	const auto url = parse_url(request.url);
	if(!url) {
		return std::unexpected(app::UpdateTransportError{
			app::UpdateTransportErrorCode::invalid_url
		});
	}
	InternetHandle session(WinHttpOpen(
		L"PoDD Port updater-v1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0U
	));
	if(!session || WinHttpSetTimeouts(
		session.get(), connect_timeout_ms, connect_timeout_ms,
		request_timeout_ms, request_timeout_ms
	) == FALSE) {
		return std::unexpected(transport_error(GetLastError()));
	}
	InternetHandle connection(WinHttpConnect(
		session.get(), url->host.c_str(), url->port, 0U
	));
	if(!connection) {
		return std::unexpected(transport_error(GetLastError()));
	}
	InternetHandle query(WinHttpOpenRequest(
		connection.get(), L"GET", url->target.c_str(), nullptr,
		WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE
	));
	if(!query) {
		return std::unexpected(transport_error(GetLastError()));
	}
	DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
	if(WinHttpSetOption(
		query.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy,
		sizeof(redirect_policy)
	) == FALSE || WinHttpSendRequest(
		query.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0U,
		WINHTTP_NO_REQUEST_DATA, 0U, 0U, 0U
	) == FALSE || WinHttpReceiveResponse(query.get(), nullptr) == FALSE) {
		return std::unexpected(transport_error(GetLastError()));
	}
	DWORD status = 0U;
	DWORD status_size = sizeof(status);
	if(WinHttpQueryHeaders(
		query.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
		WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
		WINHTTP_NO_HEADER_INDEX
	) == FALSE || status > std::numeric_limits<std::uint16_t>::max()) {
		return std::unexpected(transport_error(GetLastError()));
	}
	std::vector<std::byte> body;
	body.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
		request.maximum_response_bytes, 64U * 1024U
	)));
	while(true) {
		DWORD available = 0U;
		if(WinHttpQueryDataAvailable(query.get(), &available) == FALSE) {
			return std::unexpected(transport_error(GetLastError()));
		}
		if(available == 0U) {
			break;
		}
		if(static_cast<std::uint64_t>(body.size()) + available >
			request.maximum_response_bytes) {
			return std::unexpected(app::UpdateTransportError{
				app::UpdateTransportErrorCode::response_limit
			});
		}
		const auto prior = body.size();
		body.resize(prior + available);
		DWORD received = 0U;
		if(WinHttpReadData(
			query.get(), body.data() + prior, available, &received
		) == FALSE || received == 0U) {
			return std::unexpected(transport_error(GetLastError()));
		}
		body.resize(prior + received);
	}
	return app::UpdateHttpResponse{
		.status = static_cast<std::uint16_t>(status),
		.body = std::move(body),
	};
}

} // namespace

app::UpdateTransport windows_update_transport() noexcept
{
	return {.perform = &perform};
}

} // namespace th03::platform
