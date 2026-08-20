#include "th03/app/update_manifest_authentication.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <vector>

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

[[nodiscard]] std::vector<std::byte> read_bytes(
	const std::filesystem::path &path
)
{
	std::ifstream input(path, std::ios::binary);
	const std::vector<char> characters{
		std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()
	};
	std::vector<std::byte> result(characters.size());
	for(std::size_t index = 0U; index < characters.size(); ++index) {
		result[index] = static_cast<std::byte>(characters[index]);
	}
	return result;
}

void expect_error(
	std::span<const std::byte> manifest,
	std::span<const th03::app::UpdatePinnedKey> pins,
	th03::app::UpdateAuthenticationErrorCode code,
	const char *label
)
{
	const auto authenticated =
		th03::app::authenticate_update_manifest(manifest, pins);
	expect(!authenticated && authenticated.error().code == code, label);
}

void expect_fixture_error(
	const char *manifest_path,
	const char *key_path,
	th03::app::UpdateAuthenticationErrorCode code,
	const char *label
)
{
	const auto manifest = read_bytes(manifest_path);
	const auto public_key = read_bytes(key_path);
	const auto parsed = th03::app::parse_update_manifest(manifest);
	expect(parsed.has_value(), "rebound key fixture structurally parses");
	if(!parsed) {
		return;
	}
	const th03::app::UpdatePinnedKey pin{
		.key_id = parsed->signing_key_id,
		.subject_public_key_info = public_key,
	};
	const std::array pins{pin};
	expect_error(manifest, pins, code, label);
}

} // namespace

int main(int argc, char **argv)
{
	using namespace th03::app;
	if(argc != 9) {
		std::cerr << "usage: update-auth-tests MANIFEST PUBLIC-SPKI-DER "
			"P384-MANIFEST P384-SPKI RSA-MANIFEST RSA-SPKI "
			"INVALID-MANIFEST INVALID-SPKI\n";
		return 2;
	}
	const auto manifest = read_bytes(argv[1]);
	const auto public_key = read_bytes(argv[2]);
	expect(!manifest.empty() && !public_key.empty(), "cross-provider fixtures read");
	const auto parsed = parse_update_manifest(manifest);
	expect(parsed.has_value(), "auth fixture structurally parses");
	if(!parsed) {
		return 1;
	}

	const UpdatePinnedKey pin{
		.key_id = parsed->signing_key_id,
		.subject_public_key_info = public_key,
	};
	const std::array pins{pin};
	const auto authenticated = authenticate_update_manifest(manifest, pins);
	expect(authenticated.has_value(),
		"Mbed TLS accepts the independently authored P-256 signature");
	if(authenticated) {
		expect(*authenticated == *parsed,
			"authentication returns the exact parsed manifest");
	}

	expect_error(manifest, {}, UpdateAuthenticationErrorCode::unknown_key_id,
		"empty pin set fails closed");
	const std::array duplicate_pins{pin, pin};
	expect_error(
		manifest, duplicate_pins, UpdateAuthenticationErrorCode::duplicate_key_id,
		"duplicate key IDs fail closed"
	);
	auto wrong_key_id = pin;
	wrong_key_id.key_id[0U] ^= 0x01U;
	const std::array wrong_key_pins{wrong_key_id};
	expect_error(
		manifest, wrong_key_pins, UpdateAuthenticationErrorCode::unknown_key_id,
		"unrecognized declared key ID fails closed"
	);
	auto wrong_public_key = public_key;
	wrong_public_key.back() ^= std::byte{0x01U};
	const UpdatePinnedKey wrong_public_pin{
		.key_id = parsed->signing_key_id,
		.subject_public_key_info = wrong_public_key,
	};
	const std::array wrong_public_pins{wrong_public_pin};
	expect_error(
		manifest, wrong_public_pins,
		UpdateAuthenticationErrorCode::invalid_pinned_key_id,
		"SPKI digest must match the selected pin ID"
	);

	auto changed_prefix = manifest;
	changed_prefix[32U] ^= std::byte{0x01U};
	expect_error(
		changed_prefix, pins, UpdateAuthenticationErrorCode::invalid_signature,
		"signed-prefix mutation fails authentication"
	);
	auto changed_signature = manifest;
	changed_signature.back() ^= std::byte{0x01U};
	expect_error(
		changed_signature, pins, UpdateAuthenticationErrorCode::invalid_signature,
		"signature mutation fails authentication"
	);
	auto malformed = manifest;
	malformed[0U] = std::byte{'X'};
	const auto malformed_result = authenticate_update_manifest(malformed, pins);
	expect(
		!malformed_result && malformed_result.error().code ==
			UpdateAuthenticationErrorCode::invalid_manifest &&
			malformed_result.error().manifest_error.code ==
			UpdateManifestErrorCode::invalid_magic,
		"structural parser errors remain typed through authentication"
	);
	expect_fixture_error(
		argv[3], argv[4], UpdateAuthenticationErrorCode::wrong_curve,
		"non-P-256 ECDSA keys fail closed"
	);
	expect_fixture_error(
		argv[5], argv[6], UpdateAuthenticationErrorCode::wrong_key_type,
		"non-ECDSA keys fail closed"
	);
	expect_fixture_error(
		argv[7], argv[8], UpdateAuthenticationErrorCode::invalid_public_key,
		"malformed SPKI keys fail closed"
	);

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
