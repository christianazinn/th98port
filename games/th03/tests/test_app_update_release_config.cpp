#include "th03/app/update/update_release_config.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>

namespace {

using namespace th03::app;

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

} // namespace

int main()
{
	std::array<std::byte, 3U> spki{
		std::byte{0x01U}, std::byte{0x02U}, std::byte{0x03U}
	};
	UpdatePinnedKey first{};
	first.key_id[0U] = 1U;
	first.subject_public_key_info = spki;
	std::array pins{first};
	UpdateReleaseConfiguration valid{
		.manifest_url = "https://updates.example.test/podd/beta.t3upd",
		.platform = UpdatePlatform::linux_x64,
		.channel = UpdateChannel::beta,
		.installed_sequence = 42U,
		.pinned_keys = pins,
	};
	expect(valid_update_release_configuration(valid),
		"complete absolute-HTTPS release configuration is valid");

	auto invalid = valid;
	invalid.manifest_url = "http://updates.example.test/beta.t3upd";
	expect(!valid_update_release_configuration(invalid),
		"plaintext origin is rejected");
	invalid.manifest_url = "https://user@updates.example.test/beta.t3upd";
	expect(!valid_update_release_configuration(invalid),
		"credential-bearing origin is rejected");
	invalid = valid;
	invalid.installed_sequence = 0U;
	expect(!valid_update_release_configuration(invalid),
		"zero installed sequence is rejected");
	invalid = valid;
	invalid.platform = static_cast<UpdatePlatform>(0U);
	expect(!valid_update_release_configuration(invalid),
		"unknown release platform is rejected");
	invalid = valid;
	invalid.channel = static_cast<UpdateChannel>(0U);
	expect(!valid_update_release_configuration(invalid),
		"unknown release channel is rejected");
	invalid = valid;
	invalid.pinned_keys = {};
	expect(!valid_update_release_configuration(invalid),
		"empty trust root is rejected");

	UpdatePinnedKey zero{};
	zero.subject_public_key_info = spki;
	std::array zero_pins{zero};
	invalid = valid;
	invalid.pinned_keys = zero_pins;
	expect(!valid_update_release_configuration(invalid),
		"zero pin identity is rejected");

	std::array duplicate_pins{first, first};
	invalid = valid;
	invalid.pinned_keys = duplicate_pins;
	expect(!valid_update_release_configuration(invalid),
		"duplicate pin identity is rejected");

	const auto &compiled = compiled_update_release_configuration();
	expect(!compiled || valid_update_release_configuration(*compiled),
		"compiled release configuration is absent or complete");

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
