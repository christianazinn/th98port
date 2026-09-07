#include "th03/app/update/update_process_host.hpp"
#include "th03/platform/sdl_update_process.hpp"

#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace fs = std::filesystem;

void usage()
{
	std::cout <<
		"usage: podd-bootstrap --state PATH --generations DIR --legacy PATH "
		"--working-directory DIR -- [th03-app arguments...]\n";
}

[[nodiscard]] int fail(std::string_view code, std::string_view detail = {})
{
	std::cerr << "status=error\nstage=update-bootstrap\ncode=" << code << '\n';
	if(!detail.empty()) {
		std::cerr << "detail=" << detail << '\n';
	}
	return 1;
}

[[nodiscard]] std::string_view code(
	th03::app::UpdateProcessHostErrorCode value
)
{
	using enum th03::app::UpdateProcessHostErrorCode;
	switch(value) {
	case invalid_config: return "invalid-config";
	case filesystem: return "filesystem";
	case bootstrap: return "bootstrap";
	case activation: return "activation";
	case process: return "process";
	case fallback_exhausted: return "fallback-exhausted";
	}
	return "unknown";
}

} // namespace

int main(int argc, char **argv)
{
	if(argc == 2 && std::string_view(argv[1]) == "--help") {
		usage();
		return 0;
	}
	th03::app::UpdateProcessHostConfig config{};
#if defined(_WIN32)
	config.generation_executable = "th03-app.exe";
#else
	config.generation_executable = "th03-app";
#endif
	const auto arguments = std::span(argv, static_cast<std::size_t>(argc));
	std::size_t index = 1U;
	for(; index < arguments.size(); ++index) {
		const std::string_view argument(arguments[index]);
		if(argument == "--") {
			++index;
			break;
		}
		if(index + 1U >= arguments.size()) {
			return fail("missing-value", argument);
		}
		const fs::path value(arguments[++index]);
		if(argument == "--state") {
			config.activation_state = value;
		} else if(argument == "--generations") {
			config.generation_root = value;
		} else if(argument == "--legacy") {
			config.legacy_executable = value;
		} else if(argument == "--working-directory") {
			config.working_directory = value;
		} else {
			return fail("unknown-option", argument);
		}
	}
	for(; index < arguments.size(); ++index) {
		config.application_arguments.emplace_back(arguments[index]);
	}
	auto result = th03::app::run_update_process_host(
		config, th03::platform::sdl_update_process_executor()
	);
	if(!result) {
		std::string detail;
		if(result.error().process) {
			detail = result.error().process->detail;
		} else if(result.error().system) {
			detail = result.error().system.message();
		} else if(!result.error().path.empty()) {
			detail = result.error().path.string();
		}
		return fail(code(result.error().code), detail);
	}
	if(result->cleanup_warning) {
		std::cerr << "status=warning\nstage=update-generation-cleanup\n";
		if(result->cleanup_warning->activation) {
			std::cerr << "code=activation-state\n";
		} else if(result->cleanup_warning->cleanup) {
			std::cerr << "code=filesystem\n";
		} else {
			std::cerr << "code=unknown\n";
		}
	}
	return result->exit_code;
}
