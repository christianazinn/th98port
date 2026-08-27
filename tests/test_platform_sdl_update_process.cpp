#include "th03/app/update/update_activation.hpp"
#include "th03/app/update/update_activation_store.hpp"
#include "th03/app/update/update_process_host.hpp"
#include "th03/platform/sdl_update_process.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

using namespace th03::app;
namespace fs = std::filesystem;

[[nodiscard]] UpdateGenerationIdentity identity()
{
	UpdateGenerationIdentity result{.sequence = 10U};
	for(std::size_t index = 0U; index < result.artifact_sha256.size(); ++index) {
		result.artifact_sha256[index] = static_cast<std::uint8_t>(index + 1U);
	}
	return result;
}

} // namespace

int main(int argc, char **argv)
{
	if(argc != 2) {
		return 1;
	}
	const auto root = fs::temp_directory_path() /
		("th03-sdl-update-process-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()
		));
	const auto state_path = root / "PODD.UPDATE";
	const auto generation = identity();
#if defined(_WIN32)
	const fs::path executable_name = "th03-update-health-child.exe";
#else
	const fs::path executable_name = "th03-update-health-child";
#endif
	const auto executable = root / "generations" /
		update_generation_directory_name(generation) / executable_name;
	std::error_code system;
	fs::create_directories(executable.parent_path(), system);
	if(system || !fs::copy_file(argv[1], executable, system) || system) {
		return 2;
	}
#if !defined(_WIN32)
	const auto source_status = fs::status(argv[1], system);
	if(system) {
		return 2;
	}
	fs::permissions(
		executable,
		source_status.permissions(),
		fs::perm_options::replace,
		system
	);
	if(system) {
		return 2;
	}
#endif
	auto pending = nominate_update_generation({}, generation);
	if(!pending || !commit_update_activation_state(state_path, *pending)) {
		return 3;
	}
	UpdateProcessHostConfig config{
		.activation_state = state_path,
		.generation_root = root / "generations",
		.legacy_executable = argv[1],
		.working_directory = root,
		.generation_executable = executable_name,
		.application_arguments = {"healthy"},
	};
	const auto result = run_update_process_host(
		config, th03::platform::sdl_update_process_executor()
	);
	const auto loaded = load_update_activation_state(state_path);
	bool passed = result && result->exit_code == 23 &&
		result->source == UpdateLaunchSource::trial_generation && loaded &&
		loaded->state.phase == UpdateActivationPhase::stable &&
		loaded->state.active == generation;
	config.application_arguments = {"runtime-only"};
	const auto stable_result = run_update_process_host(
		config, th03::platform::sdl_update_process_executor()
	);
	passed = passed && stable_result && stable_result->exit_code == 24 &&
		stable_result->source == UpdateLaunchSource::active_generation;
	fs::remove_all(root, system);
	std::cout << "checks=2 failures=" << (passed ? 0 : 1) << '\n';
	return passed ? 0 : 1;
}
