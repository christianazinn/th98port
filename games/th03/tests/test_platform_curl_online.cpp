#include "th03/platform/curl_online.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;
using namespace th03::app;
using namespace th03::platform;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *label)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << label << '\n';
	}
}

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		static std::uint64_t sequence = 0U;
		path_ = fs::temp_directory_path() /
			("th03-curl-online-test-" + std::to_string(++sequence));
		std::error_code ignored{};
		fs::remove_all(path_, ignored);
		fs::create_directories(path_);
	}
	~TemporaryDirectory()
	{
		std::error_code ignored{};
		fs::remove_all(path_, ignored);
	}
	[[nodiscard]] const fs::path &path() const noexcept { return path_; }
private:
	fs::path path_{};
};

struct EnrollmentTransport final {
	std::size_t calls = 0U;
	bool fail = false;

	static std::expected<
		NewClassicOnlineHttpResponse,
		NewClassicOnlineTransportError
	> perform(void *context, const NewClassicOnlineHttpRequest &request)
	{
		auto &self = *static_cast<EnrollmentTransport *>(context);
		++self.calls;
		if(self.fail || request.method != NewClassicOnlineHttpMethod::post ||
			request.target != "/api/th03/users" || !request.body.empty()) {
			return std::unexpected(NewClassicOnlineTransportError{
				.code = NewClassicOnlineTransportErrorCode::request,
			});
		}
		const std::string body =
			"{\"userId\":\"12345678-1234-1234-1234-123456789abc\","
			"\"token\":\"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef0123456789_-\"}";
		return NewClassicOnlineHttpResponse{
			.status = 201U,
			.content_type = "application/json",
			.body = std::vector<std::byte>(
				reinterpret_cast<const std::byte *>(body.data()),
				reinterpret_cast<const std::byte *>(body.data() + body.size())
			),
		};
	}

	[[nodiscard]] NewClassicOnlineTransport seam() noexcept
	{
		return {this, &perform};
	}
};

void test_origin_and_request_guards()
{
	expect(!create_curl_online_transport("http://example.test"),
		"plaintext HTTP origins are rejected");
	expect(!create_curl_online_transport("https://user@example.test"),
		"origins cannot carry user information");
	expect(!create_curl_online_transport("https://example.test/prefix"),
		"origins cannot carry a path prefix");
	expect(!create_curl_online_transport("https://example.test?query"),
		"origins cannot carry a query");
	expect(!create_curl_online_transport("https://example.test:99999"),
		"origins reject ports outside the HTTPS authority range");
	auto adapter = create_curl_online_transport("https://example.test:443/");
	expect(adapter.has_value(), "one explicit HTTPS origin is accepted");
	if(!adapter) {
		return;
	}
	const NewClassicOnlineHttpRequest invalid{
		.method = NewClassicOnlineHttpMethod::get,
		.target = "https://attacker.test/api/th03/replays/00",
		.maximum_response_bytes = 16U,
	};
	auto seam = adapter->transport();
	auto result = seam.perform(seam.context, invalid);
	expect(!result && result.error().code ==
		NewClassicOnlineTransportErrorCode::request,
		"absolute request targets fail before any network operation");
	const NewClassicOnlineHttpRequest oversized{
		.method = NewClassicOnlineHttpMethod::get,
		.target = "/api/th03/leaderboard?category=story:r0:c0",
		.maximum_response_bytes = new_classic_online_replay_max_bytes + 1U,
	};
	result = seam.perform(seam.context, oversized);
	expect(!result && result.error().code ==
		NewClassicOnlineTransportErrorCode::request,
		"excessive response limits fail before network operation");
}

void test_private_credentials()
{
	TemporaryDirectory root;
	const auto path = root.path() / "PODD.AUTH";
	const NewClassicOnlineCredentials credentials{
		"12345678-1234-1234-1234-123456789abc",
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef0123456789_-",
		"Resident of Fantasy Who Runs Through Time",
	};
	auto loaded = load_posix_online_credentials(path);
	expect(loaded && !*loaded, "missing credentials are a signed-out state");

	auto stored = store_posix_online_credentials(path, credentials);
	expect(stored.has_value(), "valid credentials publish atomically");
	loaded = load_posix_online_credentials(path);
	expect(loaded && *loaded && **loaded == credentials,
		"owner-private credentials round-trip");
	std::error_code system{};
	const auto mode = fs::status(path, system).permissions();
	constexpr auto forbidden = fs::perms::group_all | fs::perms::others_all;
	expect(!system && (mode & forbidden) == fs::perms::none,
		"credential file grants no group or other permissions");

	fs::permissions(
		path, fs::perms::group_read, fs::perm_options::add, system
	);
	loaded = load_posix_online_credentials(path);
	expect(!loaded && loaded.error().code == PosixOnlineErrorCode::permissions,
		"over-broad credential permissions fail closed");
	fs::permissions(
		path, fs::perms::owner_read | fs::perms::owner_write,
		fs::perm_options::replace, system
	);
	const NewClassicOnlineCredentials invalid{"not-a-user", credentials.token};
	stored = store_posix_online_credentials(path, invalid);
	expect(!stored && stored.error().code == PosixOnlineErrorCode::format,
		"malformed credentials never replace an existing identity");
	const NewClassicOnlineCredentials invalid_name{
		credentials.user_id, credentials.token, std::string(29U, 'X')
	};
	stored = store_posix_online_credentials(path, invalid_name);
	expect(!stored && stored.error().code == PosixOnlineErrorCode::format,
		"an unwrappable cached nametag never replaces an existing identity");
	loaded = load_posix_online_credentials(path);
	expect(loaded && *loaded && **loaded == credentials,
		"failed replacement preserves the prior identity");
	expect(remove_posix_online_credentials(path).has_value() && !fs::exists(path),
		"explicit sign-out removes the private credential file");
}

void test_load_or_enroll()
{
	TemporaryDirectory root;
	const auto path = root.path() / "PODD.AUTH";
	EnrollmentTransport transport{};
	auto first = load_or_enroll_posix_online_credentials(path, transport.seam());
	expect(first.has_value() && transport.calls == 1U && fs::exists(path),
		"missing identity enrolls once and publishes privately");
	auto second = load_or_enroll_posix_online_credentials(path, transport.seam());
	expect(second && first && *second == *first && transport.calls == 1U,
		"existing identity is reused without another enrollment");
	expect(remove_posix_online_credentials(path).has_value(),
		"enrollment test removes the first identity");
	transport.fail = true;
	auto failed = load_or_enroll_posix_online_credentials(path, transport.seam());
	expect(!failed && failed.error().code ==
		PosixOnlineIdentityErrorCode::enroll &&
		failed.error().client.has_value() && !fs::exists(path),
		"failed enrollment publishes no credential file");
}

void test_live_public_board(const char *origin)
{
	constexpr std::array<std::byte, 8U> magic{
		std::byte{'T'}, std::byte{'3'}, std::byte{'O'}, std::byte{'N'},
		std::byte{'L'}, std::byte{'0'}, std::byte{'0'}, std::byte{'2'},
	};
	auto adapter = create_curl_online_transport(origin);
	expect(adapter.has_value(), "live HTTPS origin is accepted");
	if(!adapter) {
		return;
	}
	const NewClassicOnlineHttpRequest request{
		.method = NewClassicOnlineHttpMethod::get,
		.target = "/api/th03/leaderboard?category=story:r3:c1&format=t3onl2",
		.maximum_response_bytes = new_classic_online_cache_max_bytes,
	};
	auto seam = adapter->transport();
	auto response = seam.perform(seam.context, request);
	expect(response.has_value(), "live public board request completes");
	if(!response) {
		return;
	}
	expect(response->status == 200U, "live public board returns HTTP 200");
	expect(response->content_type == "application/vnd.th03.online-cache",
		"live public board returns the frozen cache content type");
	expect(response->body.size() >= magic.size() &&
		std::equal(magic.begin(), magic.end(), response->body.begin()),
		"live public board returns T3ONL002 bytes");
}

} // namespace

int main(int argc, char **argv)
{
	test_origin_and_request_guards();
	test_private_credentials();
	test_load_or_enroll();
	if(argc == 3 && std::string_view(argv[1]) == "--live-origin") {
		test_live_public_board(argv[2]);
	} else {
		expect(argc == 1, "arguments are empty or --live-origin URL");
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
