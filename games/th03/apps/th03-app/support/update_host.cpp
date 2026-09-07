#include "update_host.hpp"

#if defined(TH03_ENABLE_UPDATE_HOST)

#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

#include "runtime_environment.hpp"

#include "th03/platform/sdl_update_process.hpp"

namespace th03::app::launcher_support {
namespace {

struct UpdateHealthRequest final {
	std::filesystem::path state{};
	UpdateTrialToken token{};
};

std::optional<UpdateHealthRequest> update_health_request{};
std::optional<UpdateRuntimeContext> update_runtime_context{};
bool update_health_acknowledged = false;

[[nodiscard]] std::expected<
	std::optional<UpdateRuntimeContext>,
	const char *
> load_update_runtime_context()
{
	const auto state = environment_value(
		platform::update_activation_state_environment
	);
	const auto generations = environment_value(
		platform::update_generation_root_environment
	);
	const auto parsed = parse_update_runtime_context(
		state ? std::optional<std::string_view>(*state) : std::nullopt,
		generations ? std::optional<std::string_view>(*generations) : std::nullopt
	);
	if(!parsed) {
		return std::unexpected("incomplete-environment");
	}
	return *parsed;
}

[[nodiscard]] std::expected<std::optional<UpdateHealthRequest>, const char *>
load_update_health_request()
{
	const auto token = environment_value(
		platform::update_trial_token_environment
	);
	if(!token) {
		return std::optional<UpdateHealthRequest>{};
	}
	const auto state = environment_value(
		platform::update_activation_state_environment
	);
	if(!state || state->empty() || token->empty()) {
		return std::unexpected("incomplete-environment");
	}
	auto decoded = decode_update_trial_token(*token);
	if(!decoded) {
		return std::unexpected("invalid-token");
	}
	return std::optional<UpdateHealthRequest>{UpdateHealthRequest{
		.state = *state,
		.token = *decoded,
	}};
}

} // namespace

[[nodiscard]] std::expected<void, int> initialize_update_host()
{
	auto runtime = load_update_runtime_context();
	if(!runtime) {
		std::cerr << "status=error\nstage=update-runtime\ncode=" <<
			runtime.error() << '\n';
		return std::unexpected(1);
	}
	update_runtime_context = std::move(*runtime);
	auto health = load_update_health_request();
	if(!health) {
		std::cerr << "status=error\nstage=update-health\ncode=" <<
			health.error() << '\n';
		return std::unexpected(1);
	}
	update_health_request = std::move(*health);
	return {};
}

[[nodiscard]] std::expected<void, int> acknowledge_update_health()
{
	if(!update_health_request || update_health_acknowledged) {
		return {};
	}
	auto acknowledged = acknowledge_update_launch(
		update_health_request->state, update_health_request->token
	);
	if(!acknowledged) {
		std::cerr << "status=error\nstage=update-health\n"
			"code=acknowledge-failed\n";
		return std::unexpected(1);
	}
	update_health_acknowledged = true;
	return {};
}

[[nodiscard]] const std::optional<UpdateRuntimeContext> &
current_update_runtime_context() noexcept
{
	return update_runtime_context;
}

} // namespace th03::app::launcher_support

#endif
