#include "th03/app/update_acquisition.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <utility>

namespace th03::app {
namespace {

[[nodiscard]] UpdateAcquisitionError error(
	UpdateAcquisitionErrorCode code
) noexcept
{
	return {.code = code};
}

[[nodiscard]] UpdateAcquisitionError transport_error(
	UpdateAcquisitionErrorCode code,
	UpdateTransportError detail
) noexcept
{
	return {.code = code, .transport = detail};
}

} // namespace

std::expected<UpdateAcquiredPayload, UpdateAcquisitionError>
acquire_update_payload(
	std::string manifest_url,
	std::span<const UpdatePinnedKey> pinned_keys,
	UpdatePlatform platform,
	UpdateChannel channel,
	std::uint64_t current_sequence,
	const UpdateTransport &transport
)
{
	if(manifest_url.empty() || transport.perform == nullptr) {
		return std::unexpected(error(
			UpdateAcquisitionErrorCode::invalid_transport
		));
	}
	const UpdateHttpRequest manifest_request{
		.url = std::move(manifest_url),
		.maximum_response_bytes = update_manifest_max_bytes,
	};
	auto manifest_response = transport.perform(
		transport.context, manifest_request
	);
	if(!manifest_response) {
		return std::unexpected(transport_error(
			UpdateAcquisitionErrorCode::manifest_transport,
			manifest_response.error()
		));
	}
	if(manifest_response->status != 200U) {
		return std::unexpected(UpdateAcquisitionError{
			.code = UpdateAcquisitionErrorCode::manifest_status,
			.http_status = manifest_response->status,
		});
	}
	auto authenticated = authenticate_update_manifest(
		manifest_response->body, pinned_keys
	);
	if(!authenticated) {
		return std::unexpected(UpdateAcquisitionError{
			.code = UpdateAcquisitionErrorCode::manifest_authentication,
			.authentication = std::move(authenticated.error()),
		});
	}
	switch(update_disposition(
		*authenticated, platform, channel, current_sequence
	)) {
	case UpdateDisposition::wrong_platform:
		return std::unexpected(error(
			UpdateAcquisitionErrorCode::wrong_platform
		));
	case UpdateDisposition::wrong_channel:
		return std::unexpected(error(
			UpdateAcquisitionErrorCode::wrong_channel
		));
	case UpdateDisposition::current_or_older:
		return std::unexpected(error(
			UpdateAcquisitionErrorCode::current_or_older
		));
	case UpdateDisposition::available:
		break;
	}
	if(authenticated->artifact_size > update_acquisition_max_artifact_bytes) {
		return std::unexpected(error(
			UpdateAcquisitionErrorCode::artifact_too_large
		));
	}
	const UpdateHttpRequest artifact_request{
		.url = authenticated->artifact_url,
		.maximum_response_bytes = authenticated->artifact_size,
	};
	auto artifact_response = transport.perform(
		transport.context, artifact_request
	);
	if(!artifact_response) {
		return std::unexpected(transport_error(
			UpdateAcquisitionErrorCode::artifact_transport,
			artifact_response.error()
		));
	}
	if(artifact_response->status != 200U) {
		return std::unexpected(UpdateAcquisitionError{
			.code = UpdateAcquisitionErrorCode::artifact_status,
			.http_status = artifact_response->status,
		});
	}
	if(artifact_response->body.size() != authenticated->artifact_size) {
		return std::unexpected(error(
			UpdateAcquisitionErrorCode::artifact_size
		));
	}
	return UpdateAcquiredPayload{
		.manifest = std::move(*authenticated),
		.manifest_bytes = std::move(manifest_response->body),
		.artifact_bytes = std::move(artifact_response->body),
	};
}

std::expected<std::filesystem::path, UpdateAcquisitionError>
acquire_and_stage_update(
	std::string manifest_url,
	std::span<const UpdatePinnedKey> pinned_keys,
	UpdatePlatform platform,
	UpdateChannel channel,
	std::uint64_t current_sequence,
	const std::filesystem::path &staging_directory,
	const UpdateTransport &transport
)
{
	auto acquired = acquire_update_payload(
		std::move(manifest_url), pinned_keys, platform, channel,
		current_sequence, transport
	);
	if(!acquired) {
		return std::unexpected(std::move(acquired.error()));
	}
	auto staged = stage_authenticated_update(
		acquired->manifest_bytes, pinned_keys, platform, channel,
		current_sequence, acquired->artifact_bytes, staging_directory
	);
	if(!staged) {
		return std::unexpected(UpdateAcquisitionError{
			.code = UpdateAcquisitionErrorCode::staging,
			.staging = std::move(staged.error()),
		});
	}
	return *staged;
}

} // namespace th03::app
