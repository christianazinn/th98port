#include "th03/platform/windows_online.hpp"
#include "th03/formats/v14.hpp"

#include <cstddef>
#include <cstdint>
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
			("th03-windows-online-test-" + std::to_string(++sequence));
		std::error_code ignored;
		fs::remove_all(path_, ignored);
		fs::create_directories(path_);
	}
	~TemporaryDirectory()
	{
		std::error_code ignored;
		fs::remove_all(path_, ignored);
	}
	[[nodiscard]] const fs::path &path() const noexcept { return path_; }
private:
	fs::path path_{};
};

[[nodiscard]] std::vector<char> read_file(const fs::path &path)
{
	std::ifstream input(path, std::ios::binary);
	return {
		std::istreambuf_iterator<char>{input},
		std::istreambuf_iterator<char>{},
	};
}

[[nodiscard]] std::vector<std::byte> read_bytes(const fs::path &path)
{
	std::ifstream input(path, std::ios::binary);
	const std::vector<char> source{
		std::istreambuf_iterator<char>{input},
		std::istreambuf_iterator<char>{},
	};
	return {
		reinterpret_cast<const std::byte *>(source.data()),
		reinterpret_cast<const std::byte *>(source.data() + source.size()),
	};
}

void test_origin_and_request_guards()
{
	expect(!create_windows_online_transport("http://example.test"),
		"plaintext HTTP origins are rejected");
	expect(!create_windows_online_transport("https://user@example.test"),
		"origins cannot carry user information");
	expect(!create_windows_online_transport("https://example.test/prefix"),
		"origins cannot redirect transactions through a path prefix");
	expect(!create_windows_online_transport("https://example.test?query"),
		"origins cannot carry a query");
	auto adapter = create_windows_online_transport("https://example.test:443/");
	expect(adapter.has_value(), "one explicit HTTPS origin is accepted");
	if(!adapter) {
		return;
	}
	const NewClassicOnlineHttpRequest invalid{
		.method = NewClassicOnlineHttpMethod::get,
		.target = "https://attacker.test/api/th03/replays/00",
		.maximum_response_bytes = 16U,
	};
	auto result = adapter->transport().perform(
		adapter->transport().context, invalid
	);
	expect(!result && result.error().code ==
		NewClassicOnlineTransportErrorCode::request,
		"absolute request targets fail before any network operation");
}

void test_protected_credentials()
{
	TemporaryDirectory root;
	const auto path = root.path() / "PODD.AUTH";
	const NewClassicOnlineCredentials credentials{
		"12345678-1234-1234-1234-123456789abc",
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef0123456789_-",
		"Resident of Fantasy Who Runs Through Time",
	};
	auto loaded = load_windows_online_credentials(path);
	expect(loaded && !*loaded, "missing credentials are a normal signed-out state");

	auto stored = store_windows_online_credentials(path, credentials);
	expect(stored.has_value(), "valid credentials are protected and published");
	loaded = load_windows_online_credentials(path);
	expect(loaded && *loaded && **loaded == credentials,
		"protected credentials round-trip for the current Windows user");
	const auto raw = read_file(path);
	const std::string raw_text(raw.begin(), raw.end());
	expect(raw_text.find(credentials.user_id) == std::string::npos &&
		raw_text.find(credentials.token) == std::string::npos,
		"the credential file contains neither plaintext identity nor token");

	auto corrupt = raw;
	corrupt.back() ^= 1;
	{
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		output.write(corrupt.data(), static_cast<std::streamsize>(corrupt.size()));
	}
	loaded = load_windows_online_credentials(path);
	expect(!loaded && (loaded.error().code == WindowsOnlineErrorCode::unprotect ||
		loaded.error().code == WindowsOnlineErrorCode::format),
		"tampered protected credentials fail closed");

	const NewClassicOnlineCredentials invalid{
		"not-a-user", credentials.token
	};
	stored = store_windows_online_credentials(path, invalid);
	expect(!stored && stored.error().code == WindowsOnlineErrorCode::format,
		"malformed credentials never replace an existing file");
	const NewClassicOnlineCredentials invalid_name{
		credentials.user_id, credentials.token, std::string(29U, 'X')
	};
	stored = store_windows_online_credentials(path, invalid_name);
	expect(!stored && stored.error().code == WindowsOnlineErrorCode::format,
		"an unwrappable cached nametag never replaces an existing file");

	stored = store_windows_online_credentials(path, credentials);
	expect(stored.has_value(), "a later valid credential write recovers atomically");
	expect(remove_windows_online_credentials(path).has_value() && !fs::exists(path),
		"explicit sign-out removes the protected credential file");
}

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
		if(self.fail) {
			return std::unexpected(NewClassicOnlineTransportError{
				.code = NewClassicOnlineTransportErrorCode::request,
			});
		}
		if(request.method != NewClassicOnlineHttpMethod::post ||
			request.target != "/api/th03/users" || !request.body.empty()) {
			return std::unexpected(NewClassicOnlineTransportError{
				.code = NewClassicOnlineTransportErrorCode::request,
			});
		}
		const std::string body =
			"{\"userId\":\"12345678-1234-1234-1234-123456789abc\","
			"\"token\":\"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef0123456789_-\"}";
		const std::vector<std::byte> body_bytes(
			reinterpret_cast<const std::byte *>(body.data()),
			reinterpret_cast<const std::byte *>(body.data() + body.size())
		);
		return NewClassicOnlineHttpResponse{
			.status = 201U,
			.content_type = "application/json",
			.body = body_bytes,
		};
	}

	[[nodiscard]] NewClassicOnlineTransport seam() noexcept
	{
		return {this, &perform};
	}
};

void test_load_or_enroll()
{
	TemporaryDirectory root;
	const auto path = root.path() / "PODD.AUTH";
	EnrollmentTransport transport{};
	auto first = load_or_enroll_windows_online_credentials(
		path, transport.seam()
	);
	expect(first.has_value() && transport.calls == 1U && fs::exists(path),
		"missing identity enrolls once and is protected atomically");
	auto second = load_or_enroll_windows_online_credentials(
		path, transport.seam()
	);
	expect(second && first && *second == *first && transport.calls == 1U,
		"existing protected identity is reused without another request");

	expect(remove_windows_online_credentials(path).has_value(),
		"enrollment test removes the first identity");
	transport.fail = true;
	auto failed = load_or_enroll_windows_online_credentials(
		path, transport.seam()
	);
	expect(!failed && failed.error().code ==
		WindowsOnlineIdentityErrorCode::enroll &&
		failed.error().client.has_value() && !fs::exists(path),
		"failed enrollment publishes no credential file");
}

void test_live_upload(
	std::string origin,
	const fs::path &credential_path,
	const fs::path &replay_path
)
{
	auto adapter = create_windows_online_transport(std::move(origin));
	expect(adapter.has_value(), "live Windows online origin creates");
	if(!adapter) {
		return;
	}
	auto credentials = load_windows_online_credentials(credential_path);
	expect(credentials && *credentials,
		"live current-user credentials load without disclosure");
	const auto bytes = read_bytes(replay_path);
	auto replay = th03::formats::parse_v14_replay(bytes);
	expect(replay.has_value(), "live upload fixture is valid V14");
	if(!credentials || !*credentials || !replay) {
		return;
	}
	auto profile = fetch_new_classic_online_profile(
		**credentials, adapter->transport()
	);
	expect(profile.has_value() && profile->user_id == (**credentials).user_id,
		"live Windows adapter fetches the authenticated profile");
	auto uploaded = upload_new_classic_online_replay(
		*replay, **credentials, adapter->transport()
	);
	if(!uploaded) {
		std::cerr << "LIVE UPLOAD ERROR client="
			<< static_cast<unsigned>(uploaded.error().code)
			<< " http=" << uploaded.error().http_status;
		if(uploaded.error().transport) {
			std::cerr << " transport=" << static_cast<unsigned>(
				uploaded.error().transport->code
			);
		}
		std::cerr << '\n';
	}
	expect(uploaded.has_value() && uploaded->status ==
		NewClassicOnlineUploadStatus::accepted,
		"live Windows adapter uploads the exact V14 fixture");
}

} // namespace

int main(int argc, char **argv)
{
	test_origin_and_request_guards();
	test_protected_credentials();
	test_load_or_enroll();
	if(argc == 4) {
		test_live_upload(argv[1], argv[2], argv[3]);
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
