#ifndef TH03_APP_UPDATE_PACKAGE_MATERIALIZATION_HPP
#define TH03_APP_UPDATE_PACKAGE_MATERIALIZATION_HPP

#include "th03/app/update/update_manifest_authentication.hpp"
#include "th03/app/update/update_package.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <system_error>

namespace th03::app {

enum class UpdateMaterializationErrorCode : std::uint8_t {
	authentication,
	wrong_platform,
	wrong_channel,
	current_or_older,
	artifact_size,
	artifact_digest,
	package,
	member_digest,
	invalid_directory,
	target_conflict,
	temporary_create,
	directory_create,
	file_create,
	file_write,
	file_flush,
	file_validation,
	permissions,
	publish,
};

struct UpdateMaterializationError final {
	UpdateMaterializationErrorCode code =
		UpdateMaterializationErrorCode::authentication;
	std::optional<UpdateAuthenticationError> authentication{};
	std::optional<UpdatePackageError> package{};
	std::size_t member = 0U;
	std::error_code system{};
};

// Repeats manifest authentication and artifact selection, verifies the
// complete T3PKG001 artifact and every declared member before filesystem
// mutation, then publishes one inert complete generation below generation_root.
// It does not activate, execute, clean, or replace a generation or user data.
[[nodiscard]] std::expected<std::filesystem::path, UpdateMaterializationError>
materialize_authenticated_update_package(
	std::span<const std::byte> manifest_bytes,
	std::span<const UpdatePinnedKey> pinned_keys,
	UpdatePlatform platform,
	UpdateChannel channel,
	std::uint64_t current_sequence,
	std::span<const std::byte> package_bytes,
	const std::filesystem::path &generation_root
);

} // namespace th03::app

#endif
