#ifndef TH03_APP_UPDATE_GENERATION_CLEANUP_HPP
#define TH03_APP_UPDATE_GENERATION_CLEANUP_HPP

#include "th03/app/update_activation.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <system_error>

namespace th03::app {

enum class UpdateGenerationCleanupErrorCode : std::uint8_t {
	invalid_state,
	invalid_root,
	status,
	enumerate,
	remove,
	flush,
};

struct UpdateGenerationCleanupError final {
	UpdateGenerationCleanupErrorCode code =
		UpdateGenerationCleanupErrorCode::invalid_state;
	std::filesystem::path path{};
	std::error_code system{};
};

struct UpdateGenerationCleanupResult final {
	std::size_t inspected = 0U;
	std::size_t retained_referenced = 0U;
	std::size_t retained_history = 0U;
	std::size_t removed = 0U;
	std::size_t ignored = 0U;

	friend bool operator==(
		const UpdateGenerationCleanupResult &,
		const UpdateGenerationCleanupResult &
	) = default;
};

// Removes only atomically published generation directories carrying a valid
// PODD.GEN ownership marker whose full identity agrees with their canonical
// directory name. Every identity referenced by activation state is retained,
// followed by the newest retain_unreferenced complete generations.
//
// The caller must serialize this operation with materialization/activation and
// bootstrap launches (normally by retaining the bootstrap lifetime lease).
// Unknown files, links, temporary trees, legacy unmarked generations, and
// malformed marker/name pairs are ignored rather than deleted.
[[nodiscard]] std::expected<
	UpdateGenerationCleanupResult,
	UpdateGenerationCleanupError
> cleanup_update_generations(
	const UpdateActivationState &state,
	const std::filesystem::path &generation_root,
	std::size_t retain_unreferenced = 2U
);

} // namespace th03::app

#endif
