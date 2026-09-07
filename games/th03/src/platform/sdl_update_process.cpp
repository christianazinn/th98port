#include "th03/platform/sdl_update_process.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <string>
#include <vector>

namespace th03::platform {
namespace {

[[nodiscard]] std::string utf8(const std::filesystem::path &path)
{
	const auto encoded = path.u8string();
	return {
		reinterpret_cast<const char *>(encoded.data()), encoded.size()
	};
}

[[nodiscard]] th03::app::UpdateChildProcessError sdl_error()
{
	const auto *detail = SDL_GetError();
	return {detail == nullptr ? "unknown SDL process error" : detail};
}

[[nodiscard]] std::expected<int, th03::app::UpdateChildProcessError> execute(
	void *,
	const th03::app::UpdateChildInvocation &invocation
)
{
	std::vector<std::string> arguments;
	arguments.reserve(invocation.arguments.size() + 1U);
	arguments.push_back(utf8(invocation.executable));
	arguments.insert(
		arguments.end(), invocation.arguments.begin(), invocation.arguments.end()
	);
	std::vector<const char *> argument_pointers;
	argument_pointers.reserve(arguments.size() + 1U);
	for(const auto &argument : arguments) {
		argument_pointers.push_back(argument.c_str());
	}
	argument_pointers.push_back(nullptr);

	auto *environment = SDL_CreateEnvironment(true);
	if(environment == nullptr) {
		return std::unexpected(sdl_error());
	}
	const auto destroy_environment = [&]() {
		SDL_DestroyEnvironment(environment);
	};
	const auto state = utf8(invocation.activation_state);
	const auto generations = utf8(invocation.generation_root);
	bool environment_ready = SDL_SetEnvironmentVariable(
		environment, update_activation_state_environment, state.c_str(), true
	) && SDL_SetEnvironmentVariable(
		environment, update_generation_root_environment, generations.c_str(), true
	);
	if(environment_ready && invocation.trial_token) {
		environment_ready = SDL_SetEnvironmentVariable(
			environment, update_trial_token_environment,
			invocation.trial_token->c_str(), true
		);
	} else if(environment_ready) {
		environment_ready = SDL_UnsetEnvironmentVariable(
			environment, update_trial_token_environment
		);
	}
	if(!environment_ready) {
		auto error = sdl_error();
		destroy_environment();
		return std::unexpected(std::move(error));
	}

	const auto working_directory = utf8(invocation.working_directory);
	const auto properties = SDL_CreateProperties();
	if(properties == 0U) {
		auto error = sdl_error();
		destroy_environment();
		return std::unexpected(std::move(error));
	}
	const bool properties_ready = SDL_SetPointerProperty(
		properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER,
		argument_pointers.data()
	) && SDL_SetPointerProperty(
		properties, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, environment
	) && SDL_SetStringProperty(
		properties, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING,
		working_directory.c_str()
	) && SDL_SetNumberProperty(
		properties, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,
		SDL_PROCESS_STDIO_INHERITED
	) && SDL_SetNumberProperty(
		properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,
		SDL_PROCESS_STDIO_INHERITED
	) && SDL_SetNumberProperty(
		properties, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,
		SDL_PROCESS_STDIO_INHERITED
	);
	if(!properties_ready) {
		auto error = sdl_error();
		SDL_DestroyProperties(properties);
		destroy_environment();
		return std::unexpected(std::move(error));
	}
	auto *process = SDL_CreateProcessWithProperties(properties);
	SDL_DestroyProperties(properties);
	destroy_environment();
	if(process == nullptr) {
		return std::unexpected(sdl_error());
	}
	int exit_code = 0;
	if(!SDL_WaitProcess(process, true, &exit_code)) {
		auto error = sdl_error();
		SDL_DestroyProcess(process);
		return std::unexpected(std::move(error));
	}
	SDL_DestroyProcess(process);
	return exit_code;
}

} // namespace

th03::app::UpdateProcessExecutor sdl_update_process_executor() noexcept
{
	return {.context = nullptr, .execute = execute};
}

} // namespace th03::platform
