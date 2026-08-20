#include "th03/app/update_process_host.hpp"

#include "th03/app/update_activation_store.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <utility>

namespace th03::app {
namespace {

namespace fs = std::filesystem;
constexpr std::size_t transaction_attempts = 8U;
constexpr std::size_t launch_attempts = 4U;

[[nodiscard]] UpdateProcessHostError host_error(
	UpdateProcessHostErrorCode code,
	fs::path path = {},
	std::error_code system = {},
	std::optional<UpdateBootstrapError> bootstrap = std::nullopt,
	std::optional<UpdateActivationTransitionError> transition = std::nullopt,
	std::optional<UpdateChildProcessError> process = std::nullopt
)
{
	return {
		.code = code,
		.path = std::move(path),
		.system = system,
		.bootstrap = std::move(bootstrap),
		.transition = transition,
		.process = std::move(process),
	};
}

[[nodiscard]] bool single_filename(const fs::path &path)
{
	return !path.empty() && path == path.filename() && path != "." &&
		path != "..";
}

[[nodiscard]] bool valid_config(const UpdateProcessHostConfig &config)
{
	return !config.activation_state.empty() &&
		!config.generation_root.empty() &&
		!config.legacy_executable.empty() &&
		!config.working_directory.empty() &&
		single_filename(config.generation_executable);
}

[[nodiscard]] std::expected<bool, UpdateProcessHostError> regular_file(
	const fs::path &path
)
{
	std::error_code system;
	const auto status = fs::symlink_status(path, system);
	if(system == std::errc::no_such_file_or_directory) {
		return false;
	}
	if(system) {
		return std::unexpected(host_error(
			UpdateProcessHostErrorCode::filesystem, path, system
		));
	}
	return status.type() == fs::file_type::regular;
}

[[nodiscard]] std::expected<void, UpdateProcessHostError>
rollback_active(
	const fs::path &state_path,
	const UpdateGenerationIdentity &expected_active
)
{
	for(std::size_t attempt = 0U; attempt < transaction_attempts; ++attempt) {
		auto loaded = load_update_activation_state(state_path);
		if(!loaded) {
			return std::unexpected(host_error(
				UpdateProcessHostErrorCode::activation, state_path, {},
				UpdateBootstrapError{
					.code = UpdateBootstrapErrorCode::store,
					.store = loaded.error(),
				}
			));
		}
		auto rolled_back = rollback_unlaunchable_active_generation(
			loaded->state, expected_active
		);
		if(!rolled_back) {
			return std::unexpected(host_error(
				UpdateProcessHostErrorCode::activation, state_path, {}, {},
				rolled_back.error()
			));
		}
		auto committed = commit_update_activation_state(
			state_path, *rolled_back
		);
		if(committed) {
			return {};
		}
		if(committed.error().code !=
			UpdateActivationStoreErrorCode::stale_revision) {
			return std::unexpected(host_error(
				UpdateProcessHostErrorCode::activation, state_path, {},
				UpdateBootstrapError{
					.code = UpdateBootstrapErrorCode::store,
					.store = committed.error(),
				}
			));
		}
	}
	return std::unexpected(host_error(
		UpdateProcessHostErrorCode::fallback_exhausted, state_path
	));
}

[[nodiscard]] fs::path executable_for(
	const UpdateProcessHostConfig &config,
	const UpdateLaunchPlan &plan
)
{
	if(plan.source == UpdateLaunchSource::legacy_install) {
		return config.legacy_executable;
	}
	return config.generation_root / plan.generation_directory /
		config.generation_executable;
}

[[nodiscard]] std::expected<bool, UpdateProcessHostError> trial_acknowledged(
	const fs::path &state_path,
	const UpdateGenerationIdentity &generation
)
{
	auto loaded = load_update_activation_state(state_path);
	if(!loaded) {
		return std::unexpected(host_error(
			UpdateProcessHostErrorCode::activation, state_path, {},
			UpdateBootstrapError{
				.code = UpdateBootstrapErrorCode::store,
				.store = loaded.error(),
			}
		));
	}
	return loaded->state.active == generation &&
		loaded->state.phase != UpdateActivationPhase::trial;
}

} // namespace

std::expected<UpdateProcessHostResult, UpdateProcessHostError>
run_update_process_host(
	const UpdateProcessHostConfig &config,
	const UpdateProcessExecutor &executor
)
{
	if(!valid_config(config) || executor.execute == nullptr) {
		return std::unexpected(host_error(
			UpdateProcessHostErrorCode::invalid_config
		));
	}
	auto lease = acquire_update_bootstrap_lease(config.activation_state);
	if(!lease) {
		return std::unexpected(host_error(
			UpdateProcessHostErrorCode::bootstrap, config.activation_state, {},
			lease.error()
		));
	}
	bool recovered = false;
	for(std::size_t attempt = 0U; attempt < launch_attempts; ++attempt) {
		auto plan = prepare_update_launch(*lease);
		if(!plan) {
			return std::unexpected(host_error(
				UpdateProcessHostErrorCode::bootstrap,
				config.activation_state, {}, plan.error()
			));
		}
		recovered = recovered || plan->recovered_interrupted_trial;
		const auto executable = executable_for(config, *plan);
		auto exists = regular_file(executable);
		if(!exists) {
			return std::unexpected(exists.error());
		}
		if(!*exists) {
			if(plan->source == UpdateLaunchSource::legacy_install) {
				return std::unexpected(host_error(
					UpdateProcessHostErrorCode::filesystem, executable,
					std::make_error_code(std::errc::no_such_file_or_directory)
				));
			}
			if(plan->source == UpdateLaunchSource::trial_generation) {
				auto failed = fail_update_launch(
					config.activation_state, *plan->trial_token
				);
				if(!failed) {
					return std::unexpected(host_error(
						UpdateProcessHostErrorCode::bootstrap,
						config.activation_state, {}, failed.error()
					));
				}
			} else {
				auto rolled_back = rollback_active(
					config.activation_state, *plan->generation
				);
				if(!rolled_back) {
					return std::unexpected(rolled_back.error());
				}
			}
			recovered = true;
			continue;
		}

		UpdateChildInvocation invocation{
			.executable = executable,
			.working_directory = config.working_directory,
			.arguments = config.application_arguments,
		};
		if(plan->trial_token) {
			auto text = encode_update_trial_token(*plan->trial_token);
			if(!text) {
				return std::unexpected(host_error(
					UpdateProcessHostErrorCode::activation,
					config.activation_state
				));
			}
			invocation.activation_state = config.activation_state;
			invocation.trial_token = std::move(*text);
		}
		auto executed = executor.execute(executor.context, invocation);
		if(!executed) {
			if(plan->source == UpdateLaunchSource::legacy_install) {
				return std::unexpected(host_error(
					UpdateProcessHostErrorCode::process, executable, {}, {}, {},
					executed.error()
				));
			}
			if(plan->source == UpdateLaunchSource::trial_generation) {
				auto failed = fail_update_launch(
					config.activation_state, *plan->trial_token
				);
				if(!failed) {
					return std::unexpected(host_error(
						UpdateProcessHostErrorCode::bootstrap,
						config.activation_state, {}, failed.error()
					));
				}
			} else {
				auto rolled_back = rollback_active(
					config.activation_state, *plan->generation
				);
				if(!rolled_back) {
					return std::unexpected(rolled_back.error());
				}
			}
			recovered = true;
			continue;
		}

		if(plan->source == UpdateLaunchSource::trial_generation) {
			auto acknowledged = trial_acknowledged(
				config.activation_state, *plan->generation
			);
			if(!acknowledged) {
				return std::unexpected(acknowledged.error());
			}
			if(!*acknowledged) {
				auto failed = fail_update_launch(
					config.activation_state, *plan->trial_token
				);
				if(!failed) {
					return std::unexpected(host_error(
						UpdateProcessHostErrorCode::bootstrap,
						config.activation_state, {}, failed.error()
					));
				}
			}
		}
		return UpdateProcessHostResult{
			.exit_code = *executed,
			.source = plan->source,
			.recovered = recovered,
		};
	}
	return std::unexpected(host_error(
		UpdateProcessHostErrorCode::fallback_exhausted,
		config.activation_state
	));
}

} // namespace th03::app
