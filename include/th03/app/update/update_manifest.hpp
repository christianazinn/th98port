#ifndef TH03_APP_UPDATE_MANIFEST_HPP
#define TH03_APP_UPDATE_MANIFEST_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>

namespace th03::app {

inline constexpr std::size_t update_manifest_header_bytes = 128U;
inline constexpr std::size_t update_manifest_signature_bytes = 64U;
inline constexpr std::size_t update_manifest_max_bytes = 2048U;

enum class UpdatePlatform : std::uint8_t {
	windows_standalone = 1U,
	windows_x64 = 2U,
	linux_x64 = 3U,
	macos_x64 = 4U,
	macos_arm64 = 5U,
};

enum class UpdateChannel : std::uint8_t {
	stable = 1U,
	beta = 2U,
};

struct UpdateManifest final {
	UpdatePlatform platform = UpdatePlatform::windows_standalone;
	UpdateChannel channel = UpdateChannel::beta;
	std::uint64_t sequence = 0U;
	std::uint64_t published_unix = 0U;
	std::uint64_t artifact_size = 0U;
	std::array<std::uint8_t, 32U> artifact_sha256{};
	std::array<std::uint8_t, 16U> signing_key_id{};
	std::string version{};
	std::string artifact_url{};
	std::string release_notes_url{};
	std::array<std::uint8_t, update_manifest_signature_bytes> signature{};
	// The signature covers input bytes [0, signed_bytes). The parser never
	// treats a structurally valid document as authenticated; a platform owner
	// must select the pinned key ID and verify this exact prefix first.
	std::size_t signed_bytes = 0U;

	friend bool operator==(const UpdateManifest &, const UpdateManifest &) =
		default;
};

enum class UpdateManifestErrorCode : std::uint8_t {
	too_short,
	too_large,
	invalid_magic,
	unsupported_version,
	invalid_header_size,
	invalid_size,
	invalid_platform,
	invalid_channel,
	unsupported_flags,
	invalid_sequence,
	invalid_timestamp,
	invalid_artifact_size,
	invalid_artifact_digest,
	invalid_key_id,
	invalid_text_length,
	invalid_version_text,
	invalid_artifact_url,
	invalid_release_notes_url,
	invalid_signature_size,
	nonzero_reserved,
};

struct UpdateManifestError final {
	UpdateManifestErrorCode code = UpdateManifestErrorCode::too_short;
	std::size_t offset = 0U;

	friend constexpr bool operator==(
		const UpdateManifestError &,
		const UpdateManifestError &
	) = default;
};

[[nodiscard]] std::expected<UpdateManifest, UpdateManifestError>
parse_update_manifest(std::span<const std::byte> bytes);

enum class UpdateDisposition : std::uint8_t {
	available,
	current_or_older,
	wrong_platform,
	wrong_channel,
};

// Version text and wall-clock timestamps are display metadata. Only the
// release sequence decides ordering, preventing clock or lexical-version
// mistakes from downgrading a build.
[[nodiscard]] constexpr UpdateDisposition update_disposition(
	const UpdateManifest &manifest,
	UpdatePlatform platform,
	UpdateChannel channel,
	std::uint64_t current_sequence
) noexcept
{
	if(manifest.platform != platform) {
		return UpdateDisposition::wrong_platform;
	}
	if(manifest.channel != channel) {
		return UpdateDisposition::wrong_channel;
	}
	return manifest.sequence > current_sequence ? UpdateDisposition::available :
		UpdateDisposition::current_or_older;
}

} // namespace th03::app

#endif
