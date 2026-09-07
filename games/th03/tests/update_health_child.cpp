#include "th03/app/update/update_bootstrap.hpp"
#include "th03/platform/sdl_update_process.hpp"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] std::optional<std::string> environment_value(const char *name)
{
#if defined(_MSC_VER)
	char *value = nullptr;
	std::size_t bytes = 0U;
	if(_dupenv_s(&value, &bytes, name) != 0 || value == nullptr) {
		std::free(value);
		return std::nullopt;
	}
	std::string result(value);
	std::free(value);
	return result;
#else
	const auto *value = std::getenv(name);
	return value == nullptr ? std::nullopt :
		std::optional<std::string>(value);
#endif
}

} // namespace

int main(int argc, char **argv)
{
	if(argc != 2) {
		return 90;
	}
	const auto state = environment_value(
		th03::platform::update_activation_state_environment
	);
	const auto text = environment_value(
		th03::platform::update_trial_token_environment
	);
	const auto generations = environment_value(
		th03::platform::update_generation_root_environment
	);
	if(!state || !generations || generations->empty()) {
		return 91;
	}
	if(std::string_view(argv[1]) == "runtime-only") {
		return text ? 94 : 24;
	}
	if(!text) {
		return 91;
	}
	auto token = th03::app::decode_update_trial_token(*text);
	if(!token || !th03::app::acknowledge_update_launch(*state, *token)) {
		return 92;
	}
	if(std::string_view(argv[1]) != "healthy") {
		return 93;
	}
	return 23;
}
