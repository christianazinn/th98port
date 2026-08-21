#include "th03/platform/windows_online.hpp"

#if !defined(_WIN32)
#error The Windows online adapter requires Win32.
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace th03::platform {
namespace {

constexpr std::array<std::byte, 8U> credential_magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'A'}, std::byte{'U'},
	std::byte{'T'}, std::byte{'H'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::size_t credential_header_bytes = 12U;
constexpr std::size_t credential_max_bytes = 4096U;
constexpr std::wstring_view credential_entropy = L"TH03-PoDD-Online-v1";
constexpr int connect_timeout_ms = 5'000;
constexpr int request_timeout_ms = 15'000;

[[nodiscard]] std::error_code last_error() noexcept
{
	return {
		static_cast<int>(GetLastError()),
		std::system_category(),
	};
}

[[nodiscard]] WindowsOnlineError error(
	WindowsOnlineErrorCode code,
	std::error_code system = {}
) noexcept
{
	return {code, system};
}

[[nodiscard]] std::optional<std::wstring> ascii_wide(
	std::string_view input
)
{
	std::wstring result;
	result.reserve(input.size());
	for(const auto character : input) {
		if(static_cast<unsigned char>(character) > 0x7FU || character == '\0') {
			return std::nullopt;
		}
		result.push_back(static_cast<wchar_t>(character));
	}
	return result;
}

[[nodiscard]] bool safe_target(std::string_view target) noexcept
{
	return target.starts_with("/api/th03/") && target.size() <= 2048U &&
		target.find("://") == std::string_view::npos &&
		target.find('#') == std::string_view::npos &&
		target.find('\r') == std::string_view::npos &&
		target.find('\n') == std::string_view::npos;
}

[[nodiscard]] bool safe_header(
	const app::NewClassicOnlineHttpHeader &header
) noexcept
{
	if(header.name.empty() || header.value.size() > 4096U) {
		return false;
	}
	return std::ranges::all_of(header.name, [](char character) {
		return (character >= 'a' && character <= 'z') || character == '-';
	}) && std::ranges::all_of(header.value, [](char character) {
		const auto value = static_cast<unsigned char>(character);
		return value >= 0x20U && value <= 0x7EU;
	});
}

[[nodiscard]] app::NewClassicOnlineTransportError transport_error(
	DWORD system
) noexcept
{
	using Code = app::NewClassicOnlineTransportErrorCode;
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

class InternetHandle final {
public:
	InternetHandle() = default;
	explicit InternetHandle(HINTERNET handle) noexcept : handle_(handle) {}
	~InternetHandle() { reset(); }
	InternetHandle(InternetHandle &&other) noexcept : handle_(other.handle_) {
		other.handle_ = nullptr;
	}
	InternetHandle &operator=(InternetHandle &&other) noexcept {
		if(this != &other) {
			reset();
			handle_ = other.handle_;
			other.handle_ = nullptr;
		}
		return *this;
	}
	InternetHandle(const InternetHandle &) = delete;
	InternetHandle &operator=(const InternetHandle &) = delete;
	[[nodiscard]] HINTERNET get() const noexcept { return handle_; }
	[[nodiscard]] explicit operator bool() const noexcept {
		return handle_ != nullptr;
	}
private:
	void reset() noexcept {
		if(handle_ != nullptr) {
			(void)WinHttpCloseHandle(handle_);
			handle_ = nullptr;
		}
	}
	HINTERNET handle_ = nullptr;
};

class LocalBlob final {
public:
	LocalBlob() = default;
	explicit LocalBlob(DATA_BLOB blob) noexcept : blob_(blob) {}
	~LocalBlob() {
		if(blob_.pbData != nullptr) {
			(void)LocalFree(blob_.pbData);
		}
	}
	LocalBlob(LocalBlob &&other) noexcept : blob_(other.blob_) {
		other.blob_ = {};
	}
	LocalBlob(const LocalBlob &) = delete;
	LocalBlob &operator=(const LocalBlob &) = delete;
	[[nodiscard]] const DATA_BLOB &get() const noexcept { return blob_; }
private:
	DATA_BLOB blob_{};
};

[[nodiscard]] DATA_BLOB entropy_blob() noexcept
{
	return {
		static_cast<DWORD>(credential_entropy.size() * sizeof(wchar_t)),
		reinterpret_cast<BYTE *>(const_cast<wchar_t *>(credential_entropy.data())),
	};
}

[[nodiscard]] std::expected<std::vector<std::byte>, WindowsOnlineError>
protect_credentials(const app::NewClassicOnlineCredentials &credentials)
{
	if(!app::validate_new_classic_online_credentials(credentials)) {
		return std::unexpected(error(WindowsOnlineErrorCode::format));
	}
	const auto plaintext = credentials.user_id + "\n" + credentials.token;
	DATA_BLOB input{
		static_cast<DWORD>(plaintext.size()),
		reinterpret_cast<BYTE *>(const_cast<char *>(plaintext.data())),
	};
	auto entropy = entropy_blob();
	DATA_BLOB output{};
	if(CryptProtectData(
		&input, L"PoDD online identity", &entropy, nullptr, nullptr,
		CRYPTPROTECT_UI_FORBIDDEN, &output
	) == FALSE) {
		return std::unexpected(error(WindowsOnlineErrorCode::protect, last_error()));
	}
	LocalBlob protected_blob(output);
	if(output.cbData == 0U || output.cbData >
		credential_max_bytes - credential_header_bytes) {
		return std::unexpected(error(WindowsOnlineErrorCode::protect));
	}
	std::vector<std::byte> result(credential_header_bytes + output.cbData);
	std::ranges::copy(credential_magic, result.begin());
	for(std::size_t byte = 0U; byte < 4U; ++byte) {
		result[8U + byte] = static_cast<std::byte>(output.cbData >> (byte * 8U));
	}
	for(std::size_t byte = 0U; byte < output.cbData; ++byte) {
		result[credential_header_bytes + byte] =
			static_cast<std::byte>(output.pbData[byte]);
	}
	return result;
}

[[nodiscard]] std::expected<app::NewClassicOnlineCredentials, WindowsOnlineError>
unprotect_credentials(std::span<const std::byte> bytes)
{
	if(bytes.size() < credential_header_bytes ||
		!std::ranges::equal(credential_magic, bytes.first(credential_magic.size()))) {
		return std::unexpected(error(WindowsOnlineErrorCode::format));
	}
	std::uint32_t size = 0U;
	for(std::size_t byte = 0U; byte < 4U; ++byte) {
		size |= static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[8U + byte])
		) << (byte * 8U);
	}
	if(size == 0U || size != bytes.size() - credential_header_bytes) {
		return std::unexpected(error(WindowsOnlineErrorCode::format));
	}
	DATA_BLOB input{
		size,
		reinterpret_cast<BYTE *>(const_cast<std::byte *>(
			bytes.data() + credential_header_bytes
		)),
	};
	auto entropy = entropy_blob();
	DATA_BLOB output{};
	if(CryptUnprotectData(
		&input, nullptr, &entropy, nullptr, nullptr,
		CRYPTPROTECT_UI_FORBIDDEN, &output
	) == FALSE) {
		return std::unexpected(error(WindowsOnlineErrorCode::unprotect, last_error()));
	}
	LocalBlob plaintext(output);
	const std::string_view text{
		reinterpret_cast<const char *>(output.pbData), output.cbData
	};
	const auto separator = text.find('\n');
	if(separator == std::string_view::npos ||
		text.find('\n', separator + 1U) != std::string_view::npos) {
		return std::unexpected(error(WindowsOnlineErrorCode::format));
	}
	app::NewClassicOnlineCredentials result{
		std::string(text.substr(0U, separator)),
		std::string(text.substr(separator + 1U)),
	};
	if(!app::validate_new_classic_online_credentials(result)) {
		return std::unexpected(error(WindowsOnlineErrorCode::format));
	}
	return result;
}

[[nodiscard]] std::expected<std::vector<std::byte>, WindowsOnlineError>
read_file(const std::filesystem::path &path)
{
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system) {
		return std::unexpected(error(WindowsOnlineErrorCode::status, system));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(error(WindowsOnlineErrorCode::format));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system || size < credential_header_bytes || size > credential_max_bytes) {
		return std::unexpected(error(WindowsOnlineErrorCode::format, system));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(error(WindowsOnlineErrorCode::open));
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(error(WindowsOnlineErrorCode::read));
	}
	return bytes;
}

[[nodiscard]] std::expected<void, WindowsOnlineError> write_file(
	const std::filesystem::path &path,
	std::span<const std::byte> bytes
)
{
	auto temporary = path;
	temporary += L".tmp";
	if(DeleteFileW(temporary.c_str()) == FALSE &&
		GetLastError() != ERROR_FILE_NOT_FOUND) {
		return std::unexpected(error(WindowsOnlineErrorCode::open, last_error()));
	}
	const auto handle = CreateFileW(
		temporary.c_str(), GENERIC_WRITE, 0U, nullptr, CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr
	);
	if(handle == INVALID_HANDLE_VALUE) {
		return std::unexpected(error(WindowsOnlineErrorCode::open, last_error()));
	}
	DWORD written = 0U;
	const bool wrote = WriteFile(
		handle, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr
	) != FALSE && written == bytes.size();
	if(!wrote) {
		const auto system = last_error();
		(void)CloseHandle(handle);
		(void)DeleteFileW(temporary.c_str());
		return std::unexpected(error(WindowsOnlineErrorCode::write, system));
	}
	if(FlushFileBuffers(handle) == FALSE) {
		const auto system = last_error();
		(void)CloseHandle(handle);
		(void)DeleteFileW(temporary.c_str());
		return std::unexpected(error(WindowsOnlineErrorCode::flush, system));
	}
	if(CloseHandle(handle) == FALSE) {
		(void)DeleteFileW(temporary.c_str());
		return std::unexpected(error(WindowsOnlineErrorCode::flush, last_error()));
	}
	auto readback = read_file(temporary);
	if(!readback || !std::ranges::equal(*readback, bytes)) {
		(void)DeleteFileW(temporary.c_str());
		return std::unexpected(readback ?
			error(WindowsOnlineErrorCode::format) : std::move(readback.error()));
	}
	if(MoveFileExW(
		temporary.c_str(), path.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
	) == FALSE) {
		const auto system = last_error();
		(void)DeleteFileW(temporary.c_str());
		return std::unexpected(error(WindowsOnlineErrorCode::replace, system));
	}
	return {};
}

} // namespace

struct WindowsOnlineTransport::Impl final {
	std::wstring host{};
	INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;

	static std::expected<
		app::NewClassicOnlineHttpResponse,
		app::NewClassicOnlineTransportError
	> perform(void *context, const app::NewClassicOnlineHttpRequest &request)
	{
		auto &self = *static_cast<Impl *>(context);
		if(!safe_target(request.target) ||
			request.method > app::NewClassicOnlineHttpMethod::delete_ ||
			request.maximum_response_bytes == 0U ||
			request.maximum_response_bytes > app::new_classic_online_replay_max_bytes ||
			request.body.size() > app::new_classic_online_replay_max_bytes ||
			!std::ranges::all_of(request.headers, safe_header)) {
			return std::unexpected(app::NewClassicOnlineTransportError{
				app::NewClassicOnlineTransportErrorCode::request
			});
		}
		const auto target = ascii_wide(request.target);
		if(!target) {
			return std::unexpected(app::NewClassicOnlineTransportError{
				app::NewClassicOnlineTransportErrorCode::request
			});
		}
		InternetHandle session(WinHttpOpen(
			L"PoDD Port online-v1",
			WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0U
		));
		if(!session) {
			return std::unexpected(transport_error(GetLastError()));
		}
		if(WinHttpSetTimeouts(
			session.get(), connect_timeout_ms, connect_timeout_ms,
			request_timeout_ms, request_timeout_ms
		) == FALSE) {
			return std::unexpected(transport_error(GetLastError()));
		}
		InternetHandle connection(WinHttpConnect(
			session.get(), self.host.c_str(), self.port, 0U
		));
		if(!connection) {
			return std::unexpected(transport_error(GetLastError()));
		}
		const auto *verb = request.method == app::NewClassicOnlineHttpMethod::get ?
			L"GET" : (request.method == app::NewClassicOnlineHttpMethod::post ?
				L"POST" : (request.method == app::NewClassicOnlineHttpMethod::put ?
					L"PUT" : L"DELETE"));
		InternetHandle query(WinHttpOpenRequest(
			connection.get(), verb, target->c_str(), nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_SECURE
		));
		if(!query) {
			return std::unexpected(transport_error(GetLastError()));
		}
		DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
		if(WinHttpSetOption(
			query.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy,
			sizeof(redirect_policy)
		) == FALSE) {
			return std::unexpected(transport_error(GetLastError()));
		}
		std::wstring headers;
		for(const auto &header : request.headers) {
			const auto name = ascii_wide(header.name);
			const auto value = ascii_wide(header.value);
			if(!name || !value) {
				return std::unexpected(app::NewClassicOnlineTransportError{
					app::NewClassicOnlineTransportErrorCode::request
				});
			}
			headers += *name;
			headers += L": ";
			headers += *value;
			headers += L"\r\n";
		}
		const auto body_size = static_cast<DWORD>(request.body.size());
		if(WinHttpSendRequest(
			query.get(), headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
			headers.empty() ? 0U : static_cast<DWORD>(headers.size()),
			request.body.empty() ? WINHTTP_NO_REQUEST_DATA :
				const_cast<std::byte *>(request.body.data()),
			body_size, body_size, 0U
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
		DWORD type_size = 0U;
		(void)WinHttpQueryHeaders(
			query.get(), WINHTTP_QUERY_CONTENT_TYPE,
			WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &type_size,
			WINHTTP_NO_HEADER_INDEX
		);
		std::wstring type;
		if(GetLastError() == ERROR_INSUFFICIENT_BUFFER && type_size >= sizeof(wchar_t)) {
			type.resize(type_size / sizeof(wchar_t));
			if(WinHttpQueryHeaders(
				query.get(), WINHTTP_QUERY_CONTENT_TYPE,
				WINHTTP_HEADER_NAME_BY_INDEX, type.data(), &type_size,
				WINHTTP_NO_HEADER_INDEX
			) == FALSE) {
				return std::unexpected(transport_error(GetLastError()));
			}
			while(!type.empty() && type.back() == L'\0') {
				type.pop_back();
			}
		}
		std::vector<std::byte> body;
		while(true) {
			DWORD available = 0U;
			if(WinHttpQueryDataAvailable(query.get(), &available) == FALSE) {
				return std::unexpected(transport_error(GetLastError()));
			}
			if(available == 0U) {
				break;
			}
			if(available > request.maximum_response_bytes - body.size()) {
				return std::unexpected(app::NewClassicOnlineTransportError{
					app::NewClassicOnlineTransportErrorCode::response_limit
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
		std::string normalized_type;
		for(const auto character : type) {
			if(character == L';') {
				break;
			}
			if(character > 0x7F) {
				return std::unexpected(app::NewClassicOnlineTransportError{
					app::NewClassicOnlineTransportErrorCode::request
				});
			}
			normalized_type.push_back(static_cast<char>(
				(character >= L'A' && character <= L'Z')
					? character + (L'a' - L'A') : character
			));
		}
		while(!normalized_type.empty() && normalized_type.back() == ' ') {
			normalized_type.pop_back();
		}
		return app::NewClassicOnlineHttpResponse{
			static_cast<std::uint16_t>(status),
			std::move(normalized_type),
			std::move(body),
		};
	}
};

WindowsOnlineTransport::WindowsOnlineTransport(
	Impl *impl
) noexcept : impl_(impl) {}

WindowsOnlineTransport::WindowsOnlineTransport(
	WindowsOnlineTransport &&other
) noexcept : impl_(other.impl_)
{
	other.impl_ = nullptr;
}

WindowsOnlineTransport &WindowsOnlineTransport::operator=(
	WindowsOnlineTransport &&other
) noexcept
{
	if(this != &other) {
		delete impl_;
		impl_ = other.impl_;
		other.impl_ = nullptr;
	}
	return *this;
}

WindowsOnlineTransport::~WindowsOnlineTransport()
{
	delete impl_;
}

app::NewClassicOnlineTransport WindowsOnlineTransport::transport() noexcept
{
	return impl_ == nullptr ? app::NewClassicOnlineTransport{} :
		app::NewClassicOnlineTransport{impl_, &Impl::perform};
}

std::expected<WindowsOnlineTransport, WindowsOnlineError>
create_windows_online_transport(std::string origin)
{
	if(!origin.starts_with("https://") || origin.size() <= 8U ||
		origin.find('@') != std::string::npos ||
		origin.find('?') != std::string::npos ||
		origin.find('#') != std::string::npos) {
		return std::unexpected(error(WindowsOnlineErrorCode::invalid_origin));
	}
	if(origin.back() == '/') {
		origin.pop_back();
	}
	if(origin.find('/', 8U) != std::string::npos) {
		return std::unexpected(error(WindowsOnlineErrorCode::invalid_origin));
	}
	const auto wide = ascii_wide(origin);
	if(!wide) {
		return std::unexpected(error(WindowsOnlineErrorCode::invalid_origin));
	}
	URL_COMPONENTS parts{};
	parts.dwStructSize = sizeof(parts);
	parts.dwHostNameLength = static_cast<DWORD>(-1);
	parts.dwUrlPathLength = static_cast<DWORD>(-1);
	parts.dwExtraInfoLength = static_cast<DWORD>(-1);
	if(WinHttpCrackUrl(wide->c_str(), 0U, 0U, &parts) == FALSE ||
		parts.nScheme != INTERNET_SCHEME_HTTPS || parts.dwHostNameLength == 0U ||
		parts.dwUrlPathLength != 0U || parts.dwExtraInfoLength != 0U) {
		return std::unexpected(error(WindowsOnlineErrorCode::invalid_origin));
	}
	auto *impl = new WindowsOnlineTransport::Impl();
	impl->host.assign(parts.lpszHostName, parts.dwHostNameLength);
	impl->port = parts.nPort;
	return WindowsOnlineTransport(impl);
}

std::expected<
	std::optional<app::NewClassicOnlineCredentials>,
	WindowsOnlineError
> load_windows_online_credentials(const std::filesystem::path &path)
{
	if(path.empty() || path.filename().empty()) {
		return std::unexpected(error(WindowsOnlineErrorCode::invalid_path));
	}
	std::error_code system;
	const bool exists = std::filesystem::exists(path, system);
	if(system) {
		return std::unexpected(error(WindowsOnlineErrorCode::status, system));
	}
	if(!exists) {
		return std::optional<app::NewClassicOnlineCredentials>{};
	}
	auto bytes = read_file(path);
	if(!bytes) {
		return std::unexpected(std::move(bytes.error()));
	}
	auto credentials = unprotect_credentials(*bytes);
	if(!credentials) {
		return std::unexpected(std::move(credentials.error()));
	}
	return std::optional<app::NewClassicOnlineCredentials>{
		std::move(*credentials)
	};
}

std::expected<void, WindowsOnlineError> store_windows_online_credentials(
	const std::filesystem::path &path,
	const app::NewClassicOnlineCredentials &credentials
)
{
	if(path.empty() || path.filename().empty() || path.parent_path().empty()) {
		return std::unexpected(error(WindowsOnlineErrorCode::invalid_path));
	}
	auto bytes = protect_credentials(credentials);
	if(!bytes) {
		return std::unexpected(std::move(bytes.error()));
	}
	std::error_code system;
	std::filesystem::create_directories(path.parent_path(), system);
	if(system) {
		return std::unexpected(error(WindowsOnlineErrorCode::write, system));
	}
	if(auto written = write_file(path, *bytes); !written) {
		return std::unexpected(std::move(written.error()));
	}
	auto readback = load_windows_online_credentials(path);
	if(!readback || !*readback || **readback != credentials) {
		return std::unexpected(readback ?
			error(WindowsOnlineErrorCode::format) : std::move(readback.error()));
	}
	return {};
}

std::expected<void, WindowsOnlineError> remove_windows_online_credentials(
	const std::filesystem::path &path
)
{
	if(path.empty() || path.filename().empty()) {
		return std::unexpected(error(WindowsOnlineErrorCode::invalid_path));
	}
	std::error_code system;
	static_cast<void>(std::filesystem::remove(path, system));
	if(system) {
		return std::unexpected(error(WindowsOnlineErrorCode::remove, system));
	}
	return {};
}

std::expected<
	app::NewClassicOnlineCredentials,
	WindowsOnlineIdentityError
> load_or_enroll_windows_online_credentials(
	const std::filesystem::path &path,
	const app::NewClassicOnlineTransport &transport
)
{
	auto loaded = load_windows_online_credentials(path);
	if(!loaded) {
		return std::unexpected(WindowsOnlineIdentityError{
			.code = WindowsOnlineIdentityErrorCode::load,
			.platform = std::move(loaded.error()),
		});
	}
	if(*loaded) {
		return std::move(**loaded);
	}
	auto enrolled = app::enroll_new_classic_online_user(transport);
	if(!enrolled) {
		return std::unexpected(WindowsOnlineIdentityError{
			.code = WindowsOnlineIdentityErrorCode::enroll,
			.client = std::move(enrolled.error()),
		});
	}
	if(auto stored = store_windows_online_credentials(path, *enrolled); !stored) {
		return std::unexpected(WindowsOnlineIdentityError{
			.code = WindowsOnlineIdentityErrorCode::store,
			.platform = std::move(stored.error()),
		});
	}
	return std::move(*enrolled);
}

} // namespace th03::platform
