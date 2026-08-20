#ifndef TH03_APP_UPDATE_PROCESS_HOST_HPP
#define TH03_APP_UPDATE_PROCESS_HOST_HPP

#include "th03/app/update_bootstrap.hpp"
#include "th03/app/update_activation_store.hpp"
#include "th03/app/update_generation_cleanup.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace th03::app {

struct UpdateChildInvocation final {
	std::filesystem::path executable{};
	std::filesystem::path working_directory{};
	std::vector<std::string> arguments{};
	std::filesystem::path activation_state{};
	std::filesystem::path generation_root{};
	std::optional<std::string> trial_token{};
};

struct UpdateChildProcessError final {
	std::string detail{};
};

struct UpdateProcessExecutor final {
	void *context = nullptr;
	std::expected<int, UpdateChildProcessError> (*execute)(
		void *context,
		const UpdateChildInvocation &invocation
	) = nullptr;
};

struct UpdateProcessHostConfig final {
	std::filesystem::path activation_state{};
	std::filesystem::path generation_root{};
	std::filesystem::path legacy_executable{};
	std::filesystem::path working_directory{};
	std::filesystem::path generation_executable{};
	std::vector<std::string> application_arguments{};
	std::size_t retain_unreferenced_generations = 2U;
};

struct UpdateProcessHostCleanupWarning final {
	std::optional<UpdateActivationStoreError> activation{};
	std::optional<UpdateGenerationCleanupError> cleanup{};
};

struct UpdateProcessHostResult final {
	int exit_code = 0;
	UpdateLaunchSource source = UpdateLaunchSource::legacy_install;
	bool recovered = false;
	std::optional<UpdateProcessHostCleanupWarning> cleanup_warning{};
};

enum class UpdateProcessHostErrorCode : std::uint8_t {
	invalid_config,
	filesystem,
	bootstrap,
	activation,
	process,
	fallback_exhausted,
};

struct UpdateProcessHostError final {
	UpdateProcessHostErrorCode code = UpdateProcessHostErrorCode::invalid_config;
	std::filesystem::path path{};
	std::error_code system{};
	std::optional<UpdateBootstrapError> bootstrap{};
	std::optional<UpdateActivationTransitionError> transition{};
	std::optional<UpdateChildProcessError> process{};
};

// Owns the bootstrap lease for the complete child lifetime. A process creation
// failure is safe to recover immediately because no candidate UI could have
// become visible. Once a child starts, its exit code is returned without
// relaunching another generation in the same user-visible session.
[[nodiscard]] std::expected<UpdateProcessHostResult, UpdateProcessHostError>
run_update_process_host(
	const UpdateProcessHostConfig &config,
	const UpdateProcessExecutor &executor
);

} // namespace th03::app

#endif
