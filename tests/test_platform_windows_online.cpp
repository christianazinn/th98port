#include "th03/platform/windows_online.hpp"

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

	stored = store_windows_online_credentials(path, credentials);
	expect(stored.has_value(), "a later valid credential write recovers atomically");
	expect(remove_windows_online_credentials(path).has_value() && !fs::exists(path),
		"explicit sign-out removes the protected credential file");
}

} // namespace

int main()
{
	test_origin_and_request_guards();
	test_protected_credentials();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
