#include "th03/app/update/update_manifest.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace th03::app {
namespace {

constexpr std::array<std::byte, 8U> manifest_magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'U'}, std::byte{'P'},
	std::byte{'D'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::uint16_t manifest_version = 1U;
constexpr std::uint64_t maximum_artifact_size = 1024ULL * 1024ULL * 1024ULL;
constexpr std::size_t maximum_version_bytes = 32U;
constexpr std::size_t maximum_url_bytes = 768U;

[[nodiscard]] constexpr std::uint16_t u16(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(
		std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U)
	);
}

[[nodiscard]] constexpr std::uint32_t u32(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	std::uint32_t result = 0U;
	for(std::size_t index = 0U; index < 4U; ++index) {
		result |= static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + index])
		) << (index * 8U);
	}
	return result;
}

[[nodiscard]] constexpr std::uint64_t u64(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	std::uint64_t result = 0U;
	for(std::size_t index = 0U; index < 8U; ++index) {
		result |= static_cast<std::uint64_t>(
			std::to_integer<std::uint8_t>(bytes[offset + index])
		) << (index * 8U);
	}
	return result;
}

[[nodiscard]] constexpr bool valid_platform(UpdatePlatform platform) noexcept
{
	return platform >= UpdatePlatform::windows_standalone &&
		platform <= UpdatePlatform::macos_arm64;
}

[[nodiscard]] constexpr bool valid_channel(UpdateChannel channel) noexcept
{
	return channel == UpdateChannel::stable || channel == UpdateChannel::beta;
}

template<std::size_t Size>
[[nodiscard]] bool all_zero(const std::array<std::uint8_t, Size> &bytes) noexcept
{
	return std::ranges::all_of(bytes, [](std::uint8_t value) {
		return value == 0U;
	});
}

[[nodiscard]] bool valid_version(std::string_view value) noexcept
{
	if(value.empty() || value.size() > maximum_version_bytes) {
		return false;
	}
	return std::ranges::all_of(value, [](char character) {
		const auto value = static_cast<unsigned char>(character);
		return value >= 0x21U && value <= 0x7EU;
	});
}

[[nodiscard]] bool valid_https_url(
	std::string_view value,
	bool allow_empty
) noexcept
{
	if(value.empty()) {
		return allow_empty;
	}
	if(value.size() > maximum_url_bytes || !value.starts_with("https://")) {
		return false;
	}
	const auto authority = value.substr(8U);
	const auto separator = authority.find('/');
	const auto host = authority.substr(0U, separator);
	if(host.empty() || host.find('@') != std::string_view::npos ||
		host.front() == '.' || host.back() == '.') {
		return false;
	}
	return std::ranges::all_of(value, [](char character) {
		const auto byte = static_cast<unsigned char>(character);
		return byte >= 0x21U && byte <= 0x7EU && character != '\\' &&
			character != '#';
	});
}

[[nodiscard]] UpdateManifestError error(
	UpdateManifestErrorCode code,
	std::size_t offset
) noexcept
{
	return {code, offset};
}

} // namespace

std::expected<UpdateManifest, UpdateManifestError> parse_update_manifest(
	std::span<const std::byte> bytes
)
{
	if(bytes.size() < update_manifest_header_bytes +
		update_manifest_signature_bytes) {
		return std::unexpected(error(UpdateManifestErrorCode::too_short, bytes.size()));
	}
	if(bytes.size() > update_manifest_max_bytes) {
		return std::unexpected(error(UpdateManifestErrorCode::too_large, bytes.size()));
	}
	if(!std::ranges::equal(manifest_magic, bytes.first(manifest_magic.size()))) {
		return std::unexpected(error(UpdateManifestErrorCode::invalid_magic, 0U));
	}
	if(u16(bytes, 8U) != manifest_version) {
		return std::unexpected(error(
			UpdateManifestErrorCode::unsupported_version, 8U
		));
	}
	if(u16(bytes, 10U) != update_manifest_header_bytes) {
		return std::unexpected(error(
			UpdateManifestErrorCode::invalid_header_size, 10U
		));
	}
	const auto total_size = static_cast<std::size_t>(u32(bytes, 12U));
	const auto signed_size = static_cast<std::size_t>(u32(bytes, 16U));
	if(total_size != bytes.size() || signed_size < update_manifest_header_bytes ||
		signed_size > total_size ||
		total_size - signed_size != update_manifest_signature_bytes) {
		return std::unexpected(error(UpdateManifestErrorCode::invalid_size, 12U));
	}

	UpdateManifest result{};
	result.platform = static_cast<UpdatePlatform>(
		std::to_integer<std::uint8_t>(bytes[20U])
	);
	if(!valid_platform(result.platform)) {
		return std::unexpected(error(UpdateManifestErrorCode::invalid_platform, 20U));
	}
	result.channel = static_cast<UpdateChannel>(
		std::to_integer<std::uint8_t>(bytes[21U])
	);
	if(!valid_channel(result.channel)) {
		return std::unexpected(error(UpdateManifestErrorCode::invalid_channel, 21U));
	}
	if(u16(bytes, 22U) != 0U) {
		return std::unexpected(error(
			UpdateManifestErrorCode::unsupported_flags, 22U
		));
	}
	result.sequence = u64(bytes, 24U);
	if(result.sequence == 0U) {
		return std::unexpected(error(UpdateManifestErrorCode::invalid_sequence, 24U));
	}
	result.published_unix = u64(bytes, 32U);
	if(result.published_unix == 0U) {
		return std::unexpected(error(UpdateManifestErrorCode::invalid_timestamp, 32U));
	}
	result.artifact_size = u64(bytes, 40U);
	if(result.artifact_size == 0U || result.artifact_size > maximum_artifact_size) {
		return std::unexpected(error(
			UpdateManifestErrorCode::invalid_artifact_size, 40U
		));
	}
	std::ranges::transform(
		bytes.subspan(48U, result.artifact_sha256.size()),
		result.artifact_sha256.begin(),
		[](std::byte value) { return std::to_integer<std::uint8_t>(value); }
	);
	if(all_zero(result.artifact_sha256)) {
		return std::unexpected(error(
			UpdateManifestErrorCode::invalid_artifact_digest, 48U
		));
	}
	std::ranges::transform(
		bytes.subspan(80U, result.signing_key_id.size()),
		result.signing_key_id.begin(),
		[](std::byte value) { return std::to_integer<std::uint8_t>(value); }
	);
	if(all_zero(result.signing_key_id)) {
		return std::unexpected(error(UpdateManifestErrorCode::invalid_key_id, 80U));
	}

	const auto version_size = static_cast<std::size_t>(u16(bytes, 96U));
	const auto artifact_url_size = static_cast<std::size_t>(u16(bytes, 98U));
	const auto notes_url_size = static_cast<std::size_t>(u16(bytes, 100U));
	if(u16(bytes, 102U) != update_manifest_signature_bytes) {
		return std::unexpected(error(
			UpdateManifestErrorCode::invalid_signature_size, 102U
		));
	}
	if(std::ranges::any_of(bytes.subspan(104U, 24U), [](std::byte value) {
		return value != std::byte{0U};
	})) {
		return std::unexpected(error(UpdateManifestErrorCode::nonzero_reserved, 104U));
	}
	if(version_size > maximum_version_bytes ||
		artifact_url_size > maximum_url_bytes || notes_url_size > maximum_url_bytes ||
		version_size > std::numeric_limits<std::size_t>::max() - artifact_url_size ||
		version_size + artifact_url_size >
			std::numeric_limits<std::size_t>::max() - notes_url_size ||
		update_manifest_header_bytes + version_size + artifact_url_size +
			notes_url_size != signed_size) {
		return std::unexpected(error(
			UpdateManifestErrorCode::invalid_text_length, 96U
		));
	}

	std::size_t cursor = update_manifest_header_bytes;
	auto take_text = [&](std::size_t size) {
		const auto view = bytes.subspan(cursor, size);
		cursor += size;
		return std::string(
			reinterpret_cast<const char *>(view.data()), view.size()
		);
	};
	result.version = take_text(version_size);
	result.artifact_url = take_text(artifact_url_size);
	result.release_notes_url = take_text(notes_url_size);
	if(!valid_version(result.version)) {
		return std::unexpected(error(
			UpdateManifestErrorCode::invalid_version_text,
			update_manifest_header_bytes
		));
	}
	if(!valid_https_url(result.artifact_url, false)) {
		return std::unexpected(error(
			UpdateManifestErrorCode::invalid_artifact_url,
			update_manifest_header_bytes + version_size
		));
	}
	if(!valid_https_url(result.release_notes_url, true)) {
		return std::unexpected(error(
			UpdateManifestErrorCode::invalid_release_notes_url,
			update_manifest_header_bytes + version_size + artifact_url_size
		));
	}
	std::ranges::transform(
		bytes.subspan(signed_size, result.signature.size()),
		result.signature.begin(),
		[](std::byte value) { return std::to_integer<std::uint8_t>(value); }
	);
	result.signed_bytes = signed_size;
	return result;
}

} // namespace th03::app
