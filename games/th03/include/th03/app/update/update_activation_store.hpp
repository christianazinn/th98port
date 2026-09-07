#ifndef TH03_APP_UPDATE_ACTIVATION_STORE_HPP
#define TH03_APP_UPDATE_ACTIVATION_STORE_HPP

#include "th03/app/update/update_activation.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <system_error>

namespace th03::app {

enum class UpdateActivationStoreErrorCode : std::uint8_t {
	invalid_path,
	lock,
	status,
	read,
	corrupt,
	ambiguous,
	stale_revision,
	codec,
	temporary_create,
	write,
	flush,
	publish,
	readback,
};

struct UpdateActivationStoreError final {
	UpdateActivationStoreErrorCode code =
		UpdateActivationStoreErrorCode::invalid_path;
	std::error_code system{};
	std::optional<UpdateActivationCodecError> codec{};
};

struct UpdateActivationLoadResult final {
	UpdateActivationState state{};
	bool file_present = false;
	bool recovered_sibling = false;
};

// The logical path names an alternating .a/.b state family. Loading selects
// the greatest valid revision and never repairs or deletes either candidate.
[[nodiscard]] std::expected<
	UpdateActivationLoadResult,
	UpdateActivationStoreError
> load_update_activation_state(const std::filesystem::path &path);

// Publishes exactly the next revision into the inactive sibling, flushes and
// read-validates it, and retains the prior valid sibling for crash recovery.
[[nodiscard]] std::expected<void, UpdateActivationStoreError>
commit_update_activation_state(
	const std::filesystem::path &path,
	const UpdateActivationState &state
);

} // namespace th03::app

#endif
