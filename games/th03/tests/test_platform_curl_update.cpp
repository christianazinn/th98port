#include "th03/platform/curl_update.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

namespace {

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

void expect_invalid(std::string url, std::uint64_t maximum, const char *label)
{
	const auto transport = th03::platform::curl_update_transport();
	const th03::app::UpdateHttpRequest request{
		.url = std::move(url),
		.maximum_response_bytes = maximum,
	};
	const auto result = transport.perform(transport.context, request);
	expect(!result && result.error().code ==
		th03::app::UpdateTransportErrorCode::invalid_url, label);
}

} // namespace

int main()
{
	using namespace th03::app;
	const auto transport = th03::platform::curl_update_transport();
	expect(transport.perform != nullptr, "libcurl update adapter is available");
	expect_invalid("http://example.test/update", 2048U,
		"plaintext HTTP is rejected before network access");
	expect_invalid("file:///tmp/update", 2048U,
		"non-network URL schemes are rejected before access");
	expect_invalid("https://user@example.test/update", 2048U,
		"URL credentials are rejected before network access");
	expect_invalid("https://example.test/update#fragment", 2048U,
		"URL fragments are rejected before network access");
	expect_invalid("https://example.test\\update", 2048U,
		"backslash URL forms are rejected before network access");
	const UpdateHttpRequest zero{
		.url = "https://example.test/update",
		.maximum_response_bytes = 0U,
	};
	const auto zero_result = transport.perform(transport.context, zero);
	expect(!zero_result && zero_result.error().code ==
		UpdateTransportErrorCode::response_limit,
		"zero response bound is rejected before network access");
	const UpdateHttpRequest excessive{
		.url = "https://example.test/update",
		.maximum_response_bytes = update_acquisition_max_artifact_bytes + 1U,
	};
	const auto excessive_result = transport.perform(
		transport.context, excessive
	);
	expect(!excessive_result && excessive_result.error().code ==
		UpdateTransportErrorCode::response_limit,
		"excessive response bound is rejected before network access");
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
