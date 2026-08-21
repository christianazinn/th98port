#include "th03/platform/curl_online.hpp"

#if defined(_WIN32)
#error The libcurl online adapter is for non-Windows hosts.
#endif

#include <curl/curl.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "curl_runtime_internal.hpp"

namespace th03::platform {
namespace {

constexpr long connect_timeout_ms = 5'000L;
constexpr long request_timeout_ms = 15'000L;
constexpr std::size_t maximum_origin_bytes = 2'048U;
constexpr std::size_t maximum_credential_bytes = 512U;
constexpr std::string_view credential_magic = "T3AUTH01";

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

class HeaderList final {
public:
	~HeaderList() { if(head_ != nullptr) { curl_slist_free_all(head_); } }
	HeaderList(const HeaderList &) = delete;
	HeaderList &operator=(const HeaderList &) = delete;
	HeaderList() = default;

	[[nodiscard]] bool append(const std::string &line) noexcept
	{
		auto *const next = curl_slist_append(head_, line.c_str());
		if(next == nullptr) {
			return false;
		}
		head_ = next;
		return true;
	}

	[[nodiscard]] curl_slist *get() const noexcept { return head_; }

private:
	curl_slist *head_ = nullptr;
};

struct ResponseBuffer final {
	std::vector<std::byte> bytes{};
	std::size_t maximum = 0U;
	bool exceeded = false;
	bool failed = false;
};

std::size_t write_response(
	char *data,
	std::size_t size,
	std::size_t count,
	void *context
) noexcept
{
	auto &response = *static_cast<ResponseBuffer *>(context);
	if(size != 0U && count > std::numeric_limits<std::size_t>::max() / size) {
		response.exceeded = true;
		return 0U;
	}
	const auto bytes = size * count;
	if(bytes > response.maximum - response.bytes.size()) {
		response.exceeded = true;
		return 0U;
	}
	try {
		const auto prior = response.bytes.size();
		response.bytes.resize(prior + bytes);
		std::copy_n(
			reinterpret_cast<const std::byte *>(data), bytes,
			response.bytes.data() + prior
		);
		return bytes;
	} catch(...) {
		response.failed = true;
		return 0U;
	}
}

[[nodiscard]] bool safe_target(std::string_view target) noexcept
{
	return target.starts_with("/api/th03/") && target.size() <= 2'048U &&
		target.find("://") == std::string_view::npos &&
		target.find('#') == std::string_view::npos &&
		target.find('\\') == std::string_view::npos &&
		target.find('\r') == std::string_view::npos &&
		target.find('\n') == std::string_view::npos;
}

[[nodiscard]] bool safe_header(
	const app::NewClassicOnlineHttpHeader &header
) noexcept
{
	if(header.name.empty() || header.value.size() > 4'096U) {
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
	CURLcode code
) noexcept
{
	using Error = app::NewClassicOnlineTransportErrorCode;
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
		return {Error::request};
	}
}

[[nodiscard]] std::string normalized_content_type(const char *value)
{
	if(value == nullptr) {
		return {};
	}
	std::string result;
	for(const char character : std::string_view(value)) {
		if(character == ';') {
			break;
		}
		if(static_cast<unsigned char>(character) > 0x7FU) {
			return {};
		}
		result.push_back(static_cast<char>(
			character >= 'A' && character <= 'Z' ?
				character + ('a' - 'A') : character
		));
	}
	while(!result.empty() && result.back() == ' ') {
		result.pop_back();
	}
	return result;
}

[[nodiscard]] bool valid_origin(std::string &origin) noexcept
{
	if(origin.size() <= 8U || origin.size() > maximum_origin_bytes ||
		!origin.starts_with("https://") ||
		origin.find('@') != std::string::npos ||
		origin.find('?') != std::string::npos ||
		origin.find('#') != std::string::npos ||
		origin.find('\\') != std::string::npos) {
		return false;
	}
	if(origin.back() == '/') {
		origin.pop_back();
	}
	if(origin.size() <= 8U || origin.find('/', 8U) != std::string::npos ||
		!std::ranges::all_of(origin, [](char character) {
			const auto byte = static_cast<unsigned char>(character);
			return byte >= 0x21U && byte <= 0x7EU;
		})) {
		return false;
	}
	CURLU *const url = curl_url();
	if(url == nullptr) {
		return false;
	}
	const auto parsed = curl_url_set(url, CURLUPART_URL, origin.c_str(), 0U);
	char *scheme = nullptr;
	char *host = nullptr;
	const auto scheme_result = parsed == CURLUE_OK ?
		curl_url_get(url, CURLUPART_SCHEME, &scheme, 0U) : parsed;
	const auto host_result = parsed == CURLUE_OK ?
		curl_url_get(url, CURLUPART_HOST, &host, 0U) : parsed;
	const bool valid = parsed == CURLUE_OK && scheme_result == CURLUE_OK &&
		host_result == CURLUE_OK && scheme != nullptr && host != nullptr &&
		std::string_view(scheme) == "https" && host[0] != '\0';
	if(scheme != nullptr) {
		curl_free(scheme);
	}
	if(host != nullptr) {
		curl_free(host);
	}
	curl_url_cleanup(url);
	return valid;
}

[[nodiscard]] PosixOnlineError posix_error(
	PosixOnlineErrorCode code,
	std::error_code system = {}
) noexcept
{
	return {code, system};
}

[[nodiscard]] std::error_code errno_error() noexcept
{
	return {errno, std::generic_category()};
}

[[nodiscard]] std::optional<app::NewClassicOnlineCredentials>
decode_credentials(std::string_view text)
{
	if(text.size() > maximum_credential_bytes ||
		!text.starts_with(credential_magic) ||
		text.size() <= credential_magic.size() ||
		text[credential_magic.size()] != '\n') {
		return std::nullopt;
	}
	const auto user_begin = credential_magic.size() + 1U;
	const auto user_end = text.find('\n', user_begin);
	if(user_end == std::string_view::npos) {
		return std::nullopt;
	}
	const auto token_begin = user_end + 1U;
	const auto token_end = text.find('\n', token_begin);
	if(token_end == std::string_view::npos || token_end + 1U != text.size()) {
		return std::nullopt;
	}
	app::NewClassicOnlineCredentials result{
		std::string(text.substr(user_begin, user_end - user_begin)),
		std::string(text.substr(token_begin, token_end - token_begin)),
	};
	return app::validate_new_classic_online_credentials(result) ?
		std::optional<app::NewClassicOnlineCredentials>{std::move(result)} :
		std::nullopt;
}

[[nodiscard]] bool write_all(int descriptor, std::string_view text) noexcept
{
	std::size_t written = 0U;
	while(written < text.size()) {
		const auto result = ::write(
			descriptor, text.data() + written, text.size() - written
		);
		if(result < 0 && errno == EINTR) {
			continue;
		}
		if(result <= 0) {
			return false;
		}
		written += static_cast<std::size_t>(result);
	}
	return true;
}

void remove_temporary(const std::filesystem::path &path) noexcept
{
	std::error_code ignored{};
	std::filesystem::remove(path, ignored);
}

} // namespace

CurlOnlineTransport::CurlOnlineTransport(std::string origin) noexcept :
	origin_(std::move(origin)) {}

app::NewClassicOnlineTransport CurlOnlineTransport::transport() noexcept
{
	return {this, &perform};
}

std::expected<
	app::NewClassicOnlineHttpResponse,
	app::NewClassicOnlineTransportError
> CurlOnlineTransport::perform(
	void *context,
	const app::NewClassicOnlineHttpRequest &request
)
{
	auto &self = *static_cast<CurlOnlineTransport *>(context);
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
	if(!detail::curl_runtime_ready()) {
		return std::unexpected(app::NewClassicOnlineTransportError{
			app::NewClassicOnlineTransportErrorCode::unavailable
		});
	}
	EasyHandle easy;
	if(!easy) {
		return std::unexpected(app::NewClassicOnlineTransportError{
			app::NewClassicOnlineTransportErrorCode::unavailable
		});
	}
	const auto url = self.origin_ + request.target;
	ResponseBuffer response{
		.maximum = request.maximum_response_bytes,
	};
	response.bytes.reserve(std::min<std::size_t>(
		request.maximum_response_bytes, 64U * 1024U
	));
	HeaderList headers;
	for(const auto &header : request.headers) {
		if(!headers.append(header.name + ": " + header.value)) {
			return std::unexpected(app::NewClassicOnlineTransportError{
				app::NewClassicOnlineTransportErrorCode::unavailable
			});
		}
	}
	auto set = [&](CURLoption option, auto value) {
		return curl_easy_setopt(easy.get(), option, value) == CURLE_OK;
	};
	if(!set(CURLOPT_URL, url.c_str()) ||
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
		!set(CURLOPT_USERAGENT, "PoDD-Port-online-v1") ||
		!set(CURLOPT_HTTPHEADER, headers.get()) ||
		!set(CURLOPT_WRITEFUNCTION, &write_response) ||
		!set(CURLOPT_WRITEDATA, &response)) {
		return std::unexpected(app::NewClassicOnlineTransportError{
			app::NewClassicOnlineTransportErrorCode::unavailable
		});
	}
	if((request.method == app::NewClassicOnlineHttpMethod::post ||
		request.method == app::NewClassicOnlineHttpMethod::put) &&
		(!set(CURLOPT_POST, 1L) ||
		 !set(CURLOPT_POSTFIELDS, request.body.empty() ?
			static_cast<const void *>("") :
			static_cast<const void *>(request.body.data())) ||
		 !set(CURLOPT_POSTFIELDSIZE_LARGE,
			static_cast<curl_off_t>(request.body.size())))) {
		return std::unexpected(app::NewClassicOnlineTransportError{
			app::NewClassicOnlineTransportErrorCode::unavailable
		});
	}
	if(request.method == app::NewClassicOnlineHttpMethod::put &&
		!set(CURLOPT_CUSTOMREQUEST, "PUT")) {
		return std::unexpected(app::NewClassicOnlineTransportError{
			app::NewClassicOnlineTransportErrorCode::unavailable
		});
	}
	if(request.method == app::NewClassicOnlineHttpMethod::delete_ &&
		!set(CURLOPT_CUSTOMREQUEST, "DELETE")) {
		return std::unexpected(app::NewClassicOnlineTransportError{
			app::NewClassicOnlineTransportErrorCode::unavailable
		});
	}
	const auto result = curl_easy_perform(easy.get());
	if(result != CURLE_OK) {
		if(response.exceeded) {
			return std::unexpected(app::NewClassicOnlineTransportError{
				app::NewClassicOnlineTransportErrorCode::response_limit
			});
		}
		if(response.failed) {
			return std::unexpected(app::NewClassicOnlineTransportError{
				app::NewClassicOnlineTransportErrorCode::request
			});
		}
		return std::unexpected(transport_error(result));
	}
	long status = 0L;
	char *content_type = nullptr;
	if(curl_easy_getinfo(easy.get(), CURLINFO_RESPONSE_CODE, &status) != CURLE_OK ||
		status < 0L || status > 65'535L ||
		curl_easy_getinfo(easy.get(), CURLINFO_CONTENT_TYPE, &content_type) != CURLE_OK) {
		return std::unexpected(app::NewClassicOnlineTransportError{
			app::NewClassicOnlineTransportErrorCode::request
		});
	}
	return app::NewClassicOnlineHttpResponse{
		.status = static_cast<std::uint16_t>(status),
		.content_type = normalized_content_type(content_type),
		.body = std::move(response.bytes),
	};
}

std::expected<CurlOnlineTransport, CurlOnlineError>
create_curl_online_transport(std::string origin)
{
	if(!detail::curl_runtime_ready()) {
		return std::unexpected(CurlOnlineError{CurlOnlineErrorCode::unavailable});
	}
	if(!valid_origin(origin)) {
		return std::unexpected(CurlOnlineError{CurlOnlineErrorCode::invalid_origin});
	}
	return CurlOnlineTransport(std::move(origin));
}

std::expected<std::optional<app::NewClassicOnlineCredentials>, PosixOnlineError>
load_posix_online_credentials(const std::filesystem::path &path)
{
	if(path.empty() || path.filename().empty()) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::invalid_path));
	}
	std::error_code system{};
	const auto status = std::filesystem::status(path, system);
	if(system) {
		if(system == std::errc::no_such_file_or_directory) {
			return std::optional<app::NewClassicOnlineCredentials>{};
		}
		return std::unexpected(posix_error(PosixOnlineErrorCode::status, system));
	}
	if(!std::filesystem::exists(status)) {
		return std::optional<app::NewClassicOnlineCredentials>{};
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::format));
	}
	constexpr auto forbidden =
		std::filesystem::perms::group_all | std::filesystem::perms::others_all;
	if((status.permissions() & forbidden) != std::filesystem::perms::none) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::permissions));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system || size == 0U || size > maximum_credential_bytes) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::format, system));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::open));
	}
	std::string text(static_cast<std::size_t>(size), '\0');
	input.read(text.data(), static_cast<std::streamsize>(text.size()));
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::read));
	}
	auto decoded = decode_credentials(text);
	if(!decoded) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::format));
	}
	return std::optional<app::NewClassicOnlineCredentials>{std::move(*decoded)};
}

std::expected<void, PosixOnlineError> store_posix_online_credentials(
	const std::filesystem::path &path,
	const app::NewClassicOnlineCredentials &credentials
)
{
	if(path.empty() || path.filename().empty() || path.parent_path().empty()) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::invalid_path));
	}
	if(!app::validate_new_classic_online_credentials(credentials)) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::format));
	}
	const auto text = std::string(credential_magic) + "\n" +
		credentials.user_id + "\n" + credentials.token + "\n";
	auto temporary = path;
	temporary += ".tmp";
	std::error_code system{};
	std::filesystem::create_directories(path.parent_path(), system);
	if(system) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::write, system));
	}
	std::filesystem::remove(temporary, system);
	if(system) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::status, system));
	}
	const int descriptor = ::open(
		temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR
	);
	if(descriptor < 0) {
		return std::unexpected(posix_error(
			PosixOnlineErrorCode::open, errno_error()
		));
	}
	if(!write_all(descriptor, text)) {
		const auto error = errno_error();
		(void)::close(descriptor);
		remove_temporary(temporary);
		return std::unexpected(posix_error(PosixOnlineErrorCode::write, error));
	}
	if(::fsync(descriptor) != 0) {
		const auto error = errno_error();
		(void)::close(descriptor);
		remove_temporary(temporary);
		return std::unexpected(posix_error(PosixOnlineErrorCode::flush, error));
	}
	if(::close(descriptor) != 0) {
		const auto error = errno_error();
		remove_temporary(temporary);
		return std::unexpected(posix_error(PosixOnlineErrorCode::flush, error));
	}
	auto readback = load_posix_online_credentials(temporary);
	if(!readback || !*readback || **readback != credentials) {
		remove_temporary(temporary);
		return std::unexpected(readback ?
			posix_error(PosixOnlineErrorCode::format) :
			std::move(readback.error()));
	}
	std::filesystem::rename(temporary, path, system);
	if(system) {
		remove_temporary(temporary);
		return std::unexpected(posix_error(PosixOnlineErrorCode::publish, system));
	}
	const int directory = ::open(path.parent_path().c_str(), O_RDONLY);
	if(directory < 0 || ::fsync(directory) != 0) {
		const auto error = errno_error();
		if(directory >= 0) {
			(void)::close(directory);
		}
		return std::unexpected(posix_error(PosixOnlineErrorCode::flush, error));
	}
	if(::close(directory) != 0) {
		return std::unexpected(posix_error(
			PosixOnlineErrorCode::flush, errno_error()
		));
	}
	return {};
}

std::expected<void, PosixOnlineError> remove_posix_online_credentials(
	const std::filesystem::path &path
)
{
	if(path.empty() || path.filename().empty()) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::invalid_path));
	}
	std::error_code system{};
	static_cast<void>(std::filesystem::remove(path, system));
	if(system) {
		return std::unexpected(posix_error(PosixOnlineErrorCode::remove, system));
	}
	return {};
}

std::expected<app::NewClassicOnlineCredentials, PosixOnlineIdentityError>
load_or_enroll_posix_online_credentials(
	const std::filesystem::path &path,
	const app::NewClassicOnlineTransport &transport
)
{
	auto loaded = load_posix_online_credentials(path);
	if(!loaded) {
		return std::unexpected(PosixOnlineIdentityError{
			.code = PosixOnlineIdentityErrorCode::load,
			.platform = std::move(loaded.error()),
		});
	}
	if(*loaded) {
		return std::move(**loaded);
	}
	auto enrolled = app::enroll_new_classic_online_user(transport);
	if(!enrolled) {
		return std::unexpected(PosixOnlineIdentityError{
			.code = PosixOnlineIdentityErrorCode::enroll,
			.client = std::move(enrolled.error()),
		});
	}
	if(auto stored = store_posix_online_credentials(path, *enrolled); !stored) {
		return std::unexpected(PosixOnlineIdentityError{
			.code = PosixOnlineIdentityErrorCode::store,
			.platform = std::move(stored.error()),
		});
	}
	return std::move(*enrolled);
}

} // namespace th03::platform
