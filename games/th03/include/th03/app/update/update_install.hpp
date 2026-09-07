#ifndef TH03_APP_UPDATE_INSTALL_HPP
#define TH03_APP_UPDATE_INSTALL_HPP

#include "th03/app/update/update_acquisition.hpp"
#include "th03/app/update/update_activation_store.hpp"
#include "th03/app/update/update_package_materialization.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace th03::app {

enum class UpdateInstallStatus : std::uint8_t {
	nominated,
	already_nominated,
	already_active,
};

struct UpdateInstallResult final {
	UpdateInstallStatus status = UpdateInstallStatus::nominated;
	UpdateGenerationIdentity generation{};
	std::filesystem::path generation_path{};
	UpdateActivationState state{};
};

enum class UpdateInstallErrorCode : std::uint8_t {
	activation_load,
	acquisition,
	materialization,
	transition,
	activation_commit,
	transaction_exhausted,
};

struct UpdateInstallError final {
	UpdateInstallErrorCode code = UpdateInstallErrorCode::activation_load;
	std::optional<UpdateActivationStoreError> activation{};
	std::optional<UpdateAcquisitionError> acquisition{};
	std::optional<UpdateMaterializationError> materialization{};
	std::optional<UpdateActivationTransitionError> transition{};
};

// Host-owned end-to-end install transaction. It loads the current monotonic
// sequence before any request, acquires one exact authenticated payload,
// atomically materializes the marked immutable generation, then nominates that
// same full identity for the next bootstrap trial. It never launches a child
// or replaces the running executable.
[[nodiscard]] std::expected<UpdateInstallResult, UpdateInstallError>
install_update_for_next_launch(
	std::string manifest_url,
	std::span<const UpdatePinnedKey> pinned_keys,
	UpdatePlatform platform,
	UpdateChannel channel,
	std::uint64_t installed_sequence,
	const std::filesystem::path &activation_state,
	const std::filesystem::path &generation_root,
	const UpdateTransport &transport
);

} // namespace th03::app

#endif
