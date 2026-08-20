#ifndef TH03_APP_UPDATE_ACQUISITION_HPP
#define TH03_APP_UPDATE_ACQUISITION_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "th03/app/update_manifest_authentication.hpp"

namespace th03::app {

inline constexpr std::uint64_t update_acquisition_max_artifact_bytes =
	256ULL * 1024ULL * 1024ULL;

struct UpdateHttpRequest final {
	std::string url{};
	std::uint64_t maximum_response_bytes = 0U;
};

struct UpdateHttpResponse final {
	std::uint16_t status = 0U;
	std::vector<std::byte> body{};
};

struct UpdateAcquiredPayload final {
	UpdateManifest manifest{};
	std::vector<std::byte> manifest_bytes{};
	std::vector<std::byte> artifact_bytes{};
};

enum class UpdateTransportErrorCode : std::uint8_t {
	invalid_url,
	unavailable,
	timeout,
	tls,
	response_limit,
	canceled,
};

struct UpdateTransportError final {
	UpdateTransportErrorCode code = UpdateTransportErrorCode::unavailable;
};

// Platform adapters must accept HTTPS only, disable redirects, apply the
// response bound while reading, and return one complete response or a typed
// failure. This synchronous seam is owned by a host worker, never source time.
struct UpdateTransport final {
	void *context = nullptr;
	std::expected<UpdateHttpResponse, UpdateTransportError>
		(*perform)(void *, const UpdateHttpRequest &) = nullptr;
};

enum class UpdateAcquisitionErrorCode : std::uint8_t {
	invalid_transport,
	manifest_transport,
	manifest_status,
	manifest_authentication,
	wrong_platform,
	wrong_channel,
	current_or_older,
	artifact_too_large,
	artifact_transport,
	artifact_status,
	artifact_size,
	staging,
};

struct UpdateAcquisitionError final {
	UpdateAcquisitionErrorCode code =
		UpdateAcquisitionErrorCode::invalid_transport;
	std::uint16_t http_status = 0U;
	std::optional<UpdateTransportError> transport{};
	std::optional<UpdateAuthenticationError> authentication{};
	std::optional<UpdateStagingError> staging{};
};

// Fetches and authenticates one manifest before requesting exactly its bounded
// signed artifact. The returned bytes remain inert; a caller must pass them to
// stage_authenticated_update() or
// materialize_authenticated_update_package(), both of which repeat all
// authorization and artifact-digest checks before filesystem mutation.
[[nodiscard]] std::expected<UpdateAcquiredPayload, UpdateAcquisitionError>
acquire_update_payload(
	std::string manifest_url,
	std::span<const UpdatePinnedKey> pinned_keys,
	UpdatePlatform platform,
	UpdateChannel channel,
	std::uint64_t current_sequence,
	const UpdateTransport &transport
);

// Fetches and authenticates one manifest before requesting its signed artifact
// URL, bounds that response by the authenticated size, and enters the existing
// inert staging transaction. It never executes or replaces product files.
[[nodiscard]] std::expected<std::filesystem::path, UpdateAcquisitionError>
acquire_and_stage_update(
	std::string manifest_url,
	std::span<const UpdatePinnedKey> pinned_keys,
	UpdatePlatform platform,
	UpdateChannel channel,
	std::uint64_t current_sequence,
	const std::filesystem::path &staging_directory,
	const UpdateTransport &transport
);

} // namespace th03::app

#endif
