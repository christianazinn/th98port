#include "th03/app/update/update_manifest.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
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

void set_u16(std::vector<std::byte> &bytes, std::size_t offset, std::uint16_t value)
{
	bytes[offset] = static_cast<std::byte>(value);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void set_u32(std::vector<std::byte> &bytes, std::size_t offset, std::uint32_t value)
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
	}
}

void set_u64(std::vector<std::byte> &bytes, std::size_t offset, std::uint64_t value)
{
	for(std::size_t index = 0U; index < 8U; ++index) {
		bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
	}
}

[[nodiscard]] std::vector<std::byte> valid_manifest()
{
	using namespace th03::app;
	constexpr std::string_view version = "0.5.0-beta.1";
	constexpr std::string_view artifact =
		"https://azinn.dev/th03/beta/PoDD-Port-Test.exe";
	constexpr std::string_view notes = "https://azinn.dev/th03/releases/0.5.0";
	const auto signed_size = update_manifest_header_bytes + version.size() +
		artifact.size() + notes.size();
	std::vector<std::byte> bytes(
		signed_size + update_manifest_signature_bytes, std::byte{0U}
	);
	constexpr std::array magic{'T', '3', 'U', 'P', 'D', '0', '0', '1'};
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		bytes[index] = static_cast<std::byte>(magic[index]);
	}
	set_u16(bytes, 8U, 1U);
	set_u16(bytes, 10U, update_manifest_header_bytes);
	set_u32(bytes, 12U, static_cast<std::uint32_t>(bytes.size()));
	set_u32(bytes, 16U, static_cast<std::uint32_t>(signed_size));
	bytes[20U] = std::byte{1U};
	bytes[21U] = std::byte{2U};
	set_u64(bytes, 24U, 5001U);
	set_u64(bytes, 32U, 1'787'000'000U);
	set_u64(bytes, 40U, 7'626'752U);
	for(std::size_t index = 0U; index < 32U; ++index) {
		bytes[48U + index] = static_cast<std::byte>(index + 1U);
	}
	for(std::size_t index = 0U; index < 16U; ++index) {
		bytes[80U + index] = static_cast<std::byte>(0x80U + index);
	}
	set_u16(bytes, 96U, static_cast<std::uint16_t>(version.size()));
	set_u16(bytes, 98U, static_cast<std::uint16_t>(artifact.size()));
	set_u16(bytes, 100U, static_cast<std::uint16_t>(notes.size()));
	set_u16(bytes, 102U, update_manifest_signature_bytes);
	std::size_t cursor = update_manifest_header_bytes;
	for(const auto text : {version, artifact, notes}) {
		for(const auto character : text) {
			bytes[cursor++] = static_cast<std::byte>(character);
		}
	}
	for(std::size_t index = 0U; index < update_manifest_signature_bytes; ++index) {
		bytes[signed_size + index] = static_cast<std::byte>(0x40U + index);
	}
	return bytes;
}

void test_valid_manifest_and_ordering()
{
	using namespace th03::app;
	const auto bytes = valid_manifest();
	const auto parsed = parse_update_manifest(bytes);
	expect(parsed.has_value(), "valid update manifest parses");
	if(!parsed) {
		return;
	}
	expect(parsed->platform == UpdatePlatform::windows_standalone &&
		parsed->channel == UpdateChannel::beta,
		"manifest exposes platform and channel");
	expect(parsed->sequence == 5001U && parsed->artifact_size == 7'626'752U,
		"manifest exposes monotonic sequence and artifact size");
	expect(parsed->version == "0.5.0-beta.1" &&
		parsed->artifact_url.ends_with("PoDD-Port-Test.exe") &&
		!parsed->release_notes_url.empty(),
		"manifest exposes bounded display and URL fields");
	expect(parsed->signed_bytes + update_manifest_signature_bytes == bytes.size() &&
		parsed->signature.front() == 0x40U,
		"manifest identifies the exact signed prefix and fixed signature");
	expect(update_disposition(
		*parsed, UpdatePlatform::windows_standalone, UpdateChannel::beta, 5000U
	) == UpdateDisposition::available,
		"higher sequence is available");
	expect(update_disposition(
		*parsed, UpdatePlatform::windows_standalone, UpdateChannel::beta, 5001U
	) == UpdateDisposition::current_or_older,
		"equal sequence cannot update");
	expect(update_disposition(
		*parsed, UpdatePlatform::linux_x64, UpdateChannel::beta, 0U
	) == UpdateDisposition::wrong_platform,
		"wrong platform cannot update");
	expect(update_disposition(
		*parsed, UpdatePlatform::windows_standalone, UpdateChannel::stable, 0U
	) == UpdateDisposition::wrong_channel,
		"wrong channel cannot update");
}

void expect_error(
	std::vector<std::byte> bytes,
	th03::app::UpdateManifestErrorCode code,
	const char *label
)
{
	const auto parsed = th03::app::parse_update_manifest(bytes);
	expect(!parsed && parsed.error().code == code, label);
}

void test_rejections()
{
	using enum th03::app::UpdateManifestErrorCode;
	auto bytes = valid_manifest();
	expect_error({bytes.begin(), bytes.begin() + 100}, too_short,
		"short manifest rejected");
	bytes[0U] = std::byte{'X'};
	expect_error(bytes, invalid_magic, "wrong magic rejected");
	bytes = valid_manifest();
	set_u16(bytes, 8U, 2U);
	expect_error(bytes, unsupported_version, "future version rejected");
	bytes = valid_manifest();
	set_u16(bytes, 10U, 64U);
	expect_error(bytes, invalid_header_size, "wrong header size rejected");
	bytes = valid_manifest();
	set_u32(bytes, 16U, 128U);
	expect_error(bytes, invalid_size, "inconsistent signed size rejected");
	bytes = valid_manifest();
	bytes[20U] = std::byte{0U};
	expect_error(bytes, invalid_platform, "unknown platform rejected");
	bytes = valid_manifest();
	bytes[21U] = std::byte{3U};
	expect_error(bytes, invalid_channel, "unknown channel rejected");
	bytes = valid_manifest();
	set_u16(bytes, 22U, 1U);
	expect_error(bytes, unsupported_flags, "unknown flags rejected");
	bytes = valid_manifest();
	set_u64(bytes, 24U, 0U);
	expect_error(bytes, invalid_sequence, "zero sequence rejected");
	bytes = valid_manifest();
	set_u64(bytes, 32U, 0U);
	expect_error(bytes, invalid_timestamp, "zero publication time rejected");
	bytes = valid_manifest();
	set_u64(bytes, 40U, 0U);
	expect_error(bytes, invalid_artifact_size, "zero artifact rejected");
	bytes = valid_manifest();
	for(std::size_t index = 0U; index < 32U; ++index) {
		bytes[48U + index] = std::byte{0U};
	}
	expect_error(bytes, invalid_artifact_digest, "zero digest rejected");
	bytes = valid_manifest();
	for(std::size_t index = 0U; index < 16U; ++index) {
		bytes[80U + index] = std::byte{0U};
	}
	expect_error(bytes, invalid_key_id, "zero signing key ID rejected");
	bytes = valid_manifest();
	set_u16(bytes, 96U, 0U);
	expect_error(bytes, invalid_text_length, "inconsistent text lengths rejected");
	bytes = valid_manifest();
	set_u16(bytes, 102U, 63U);
	expect_error(bytes, invalid_signature_size, "wrong signature size rejected");
	bytes = valid_manifest();
	bytes[104U] = std::byte{1U};
	expect_error(bytes, nonzero_reserved, "reserved bytes rejected");
	bytes = valid_manifest();
	const auto version_size = static_cast<std::size_t>(
		std::to_integer<std::uint8_t>(bytes[96U])
	);
	bytes[th03::app::update_manifest_header_bytes + version_size] = std::byte{'x'};
	expect_error(bytes, invalid_artifact_url, "non-HTTPS artifact rejected");
	bytes = valid_manifest();
	const auto artifact_offset = th03::app::update_manifest_header_bytes +
		version_size;
	bytes[artifact_offset + 8U] = std::byte{'@'};
	expect_error(bytes, invalid_artifact_url, "credentialed URL rejected");
	bytes = valid_manifest();
	bytes[th03::app::update_manifest_header_bytes] = std::byte{' '};
	expect_error(bytes, invalid_version_text, "version spaces rejected");
	bytes = valid_manifest();
	const auto artifact_size = static_cast<std::size_t>(
		std::to_integer<std::uint8_t>(bytes[98U])
	);
	const auto notes_offset = artifact_offset + artifact_size;
	bytes[notes_offset] = std::byte{'x'};
	expect_error(bytes, invalid_release_notes_url, "non-HTTPS notes rejected");
}

} // namespace

int main()
{
	test_valid_manifest_and_ordering();
	test_rejections();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
