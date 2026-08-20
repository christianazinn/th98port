#include "th03/app/update_activation.hpp"
#include "th03/app/update_activation_store.hpp"
#include "th03/app/update_bootstrap.hpp"
#include "th03/app/update_generation.hpp"
#include "th03/app/update_process_host.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace th03::app;
namespace fs = std::filesystem;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *label)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << label << '\n';
	}
}

[[nodiscard]] UpdateGenerationIdentity identity(
	std::uint64_t sequence,
	std::uint8_t seed
)
{
	UpdateGenerationIdentity result{.sequence = sequence};
	for(std::size_t index = 0U; index < result.artifact_sha256.size(); ++index) {
		result.artifact_sha256[index] = static_cast<std::uint8_t>(seed + index);
	}
	return result;
}

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		path_ = fs::temp_directory_path() / ("th03-update-process-host-" +
			std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()
			));
		fs::create_directories(path_);
	}
	~TemporaryDirectory()
	{
		std::error_code ignored;
		fs::remove_all(path_, ignored);
	}
	[[nodiscard]] const fs::path &path() const noexcept { return path_; }

private:
	fs::path path_{};
};

void touch(const fs::path &path)
{
	fs::create_directories(path.parent_path());
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output << "test";
}

[[nodiscard]] fs::path write_owned_generation(
	const fs::path &root,
	const UpdateGenerationIdentity &generation
)
{
	const auto directory = root / update_generation_directory_name(generation);
	fs::create_directories(directory);
	const auto marker = encode_update_generation_marker(generation);
	if(!marker) {
		return {};
	}
	const auto marker_path = directory /
		fs::path(update_generation_marker_filename);
	std::ofstream output(marker_path, std::ios::binary);
	output.write(
		reinterpret_cast<const char *>(marker->data()),
		static_cast<std::streamsize>(marker->size())
	);
	output.close();
#if !defined(_WIN32)
	fs::permissions(
		directory,
		fs::perms::owner_read | fs::perms::owner_write |
			fs::perms::owner_exec,
		fs::perm_options::replace
	);
	fs::permissions(
		marker_path,
		fs::perms::owner_read | fs::perms::owner_write,
		fs::perm_options::replace
	);
#endif
	return directory;
}

[[nodiscard]] UpdateActivationState load_state(const fs::path &path)
{
	const auto loaded = load_update_activation_state(path);
	if(!loaded) {
		++failures;
		return {};
	}
	return loaded->state;
}

[[nodiscard]] UpdateActivationState commit_stable_generation(
	const fs::path &path,
	UpdateActivationState state,
	const UpdateGenerationIdentity &generation
)
{
	auto nominated = nominate_update_generation(state, generation);
	if(!nominated || !commit_update_activation_state(path, *nominated)) {
		++failures;
		return {};
	}
	auto begun = begin_update_trial(*nominated);
	if(!begun || !commit_update_activation_state(path, begun->state)) {
		++failures;
		return {};
	}
	auto acknowledged = acknowledge_update_trial(begun->state, begun->token);
	if(!acknowledged || !commit_update_activation_state(path, *acknowledged)) {
		++failures;
		return {};
	}
	return *acknowledged;
}

enum class FakeAction : std::uint8_t {
	launch,
	launch_and_acknowledge,
	launch_error,
};

struct FakeExecutor final {
	std::vector<FakeAction> actions{};
	std::vector<UpdateChildInvocation> invocations{};
	int exit_code = 0;

	static std::expected<int, UpdateChildProcessError> execute(
		void *context,
		const UpdateChildInvocation &invocation
	)
	{
		auto &self = *static_cast<FakeExecutor *>(context);
		self.invocations.push_back(invocation);
		if(self.actions.empty()) {
			return std::unexpected(UpdateChildProcessError{"no fake action"});
		}
		const auto action = self.actions.front();
		self.actions.erase(self.actions.begin());
		if(action == FakeAction::launch_error) {
			return std::unexpected(UpdateChildProcessError{"create failed"});
		}
		if(action == FakeAction::launch_and_acknowledge) {
			if(invocation.activation_state.empty() ||
				invocation.generation_root.empty() || !invocation.trial_token) {
				return std::unexpected(UpdateChildProcessError{"missing trial"});
			}
			auto token = decode_update_trial_token(*invocation.trial_token);
			if(!token || !acknowledge_update_launch(
				invocation.activation_state, *token
			)) {
				return std::unexpected(UpdateChildProcessError{"ack failed"});
			}
		}
		return self.exit_code;
	}

	[[nodiscard]] UpdateProcessExecutor seam()
	{
		return {.context = this, .execute = execute};
	}
};

[[nodiscard]] UpdateProcessHostConfig config(
	const fs::path &root,
	const fs::path &state
)
{
	return {
		.activation_state = state,
		.generation_root = state.parent_path() / "generations",
		.legacy_executable = root / "legacy" / "th03-app.exe",
		.working_directory = root / "legacy",
		.generation_executable = "th03-app.exe",
		.application_arguments = {"--new-classic", "--smoke-ticks", "1"},
	};
}

} // namespace

int main()
{
	TemporaryDirectory temporary;
	const auto root = temporary.path();
	const auto first = identity(10U, 1U);
	const auto second = identity(11U, 33U);
	touch(root / "legacy" / "th03-app.exe");

	{
		auto request = config(root, root / "legacy-state" / "PODD.UPDATE");
		request.retain_unreferenced_generations = 0U;
		const auto stale = write_owned_generation(request.generation_root, first);
		FakeExecutor fake{{FakeAction::launch}, {}, 7};
		auto result = run_update_process_host(request, fake.seam());
		expect(result && result->exit_code == 7 &&
			result->source == UpdateLaunchSource::legacy_install &&
			fake.invocations.size() == 1U &&
			!fake.invocations.front().trial_token && !result->cleanup_warning &&
			fake.invocations.front().activation_state ==
				fs::absolute(request.activation_state).lexically_normal() &&
			fake.invocations.front().generation_root ==
				fs::absolute(request.generation_root).lexically_normal() &&
			!fs::exists(stale) &&
			fake.invocations.front().arguments == request.application_arguments,
			"legacy launch cleans unreferenced owned generations under the retained lifetime lease");
	}

	{
		const auto state_path = root / "trial-state" / "PODD.UPDATE";
		auto pending = nominate_update_generation({}, first);
		expect(pending && commit_update_activation_state(state_path, *pending),
			"trial fixture commits pending generation");
		auto request = config(root, state_path);
		const auto generation_executable = request.generation_root /
			update_generation_directory_name(first) / request.generation_executable;
		touch(generation_executable);
		FakeExecutor fake{{FakeAction::launch_and_acknowledge}, {}, 0};
		auto result = run_update_process_host(request, fake.seam());
		expect(result && result->source ==
			UpdateLaunchSource::trial_generation &&
			fake.invocations.size() == 1U &&
			fake.invocations.front().executable == generation_executable &&
			fake.invocations.front().activation_state ==
				fs::absolute(state_path).lexically_normal() &&
			fake.invocations.front().generation_root ==
				fs::absolute(request.generation_root).lexically_normal() &&
			fake.invocations.front().trial_token &&
			load_state(state_path).active == first,
			"trial child receives exact state/token and health acknowledgement activates it");
	}

	{
		const auto state_path = root / "missing-trial" / "PODD.UPDATE";
		auto pending = nominate_update_generation({}, first);
		expect(pending && commit_update_activation_state(state_path, *pending),
			"missing trial fixture commits pending generation");
		auto request = config(root, state_path);
		FakeExecutor fake{{FakeAction::launch}, {}, 3};
		auto result = run_update_process_host(request, fake.seam());
		const auto state = load_state(state_path);
		expect(result && result->exit_code == 3 && result->recovered &&
			result->source == UpdateLaunchSource::legacy_install &&
			fake.invocations.size() == 1U && state.last_failed == first &&
			state.failed_trials == 1U,
			"missing trial executable is failed before launch and falls back to legacy");
	}

	{
		const auto state_path = root / "failed-trial" / "PODD.UPDATE";
		auto pending = nominate_update_generation({}, first);
		expect(pending && commit_update_activation_state(state_path, *pending),
			"failed trial fixture commits pending generation");
		auto request = config(root, state_path);
		touch(request.generation_root / update_generation_directory_name(first) /
			request.generation_executable);
		FakeExecutor fake{
			{FakeAction::launch_error, FakeAction::launch}, {}, 4
		};
		auto result = run_update_process_host(request, fake.seam());
		expect(result && result->exit_code == 4 && result->recovered &&
			result->source == UpdateLaunchSource::legacy_install &&
			fake.invocations.size() == 2U &&
			load_state(state_path).last_failed == first,
			"trial process-creation failure rolls back and launches legacy in the same host session");
	}

	{
		const auto state_path = root / "unhealthy-trial" / "PODD.UPDATE";
		auto pending = nominate_update_generation({}, first);
		expect(pending && commit_update_activation_state(state_path, *pending),
			"unhealthy trial fixture commits pending generation");
		auto request = config(root, state_path);
		touch(request.generation_root / update_generation_directory_name(first) /
			request.generation_executable);
		FakeExecutor fake{{FakeAction::launch}, {}, 9};
		auto result = run_update_process_host(request, fake.seam());
		expect(result && result->exit_code == 9 &&
			result->source == UpdateLaunchSource::trial_generation &&
			fake.invocations.size() == 1U &&
			load_state(state_path).last_failed == first,
			"started child that exits without health acknowledgement is failed without opening a second UI");
	}

	{
		const auto state_path = root / "active-fallback" / "PODD.UPDATE";
		auto state = commit_stable_generation(state_path, {}, first);
		state = commit_stable_generation(state_path, state, second);
		expect(load_state(state_path) == state,
			"active fallback fixture commits two stable generations");
		auto request = config(root, state_path);
		const auto first_executable = request.generation_root /
			update_generation_directory_name(first) / request.generation_executable;
		touch(first_executable);
		FakeExecutor fake{{FakeAction::launch}, {}, 0};
		auto result = run_update_process_host(request, fake.seam());
		const auto rolled_back = load_state(state_path);
		expect(result && result->recovered &&
			result->source == UpdateLaunchSource::active_generation &&
			fake.invocations.size() == 1U &&
			fake.invocations.front().executable == first_executable &&
			rolled_back.active == first && rolled_back.last_failed == second,
			"missing active executable transactionally restores and launches the retained previous generation");
	}

	{
		const auto state_path = root / "active-launch-error" / "PODD.UPDATE";
		auto state = commit_stable_generation(state_path, {}, first);
		state = commit_stable_generation(state_path, state, second);
		expect(load_state(state_path) == state,
			"active launch-error fixture commits two stable generations");
		auto request = config(root, state_path);
		touch(request.generation_root / update_generation_directory_name(first) /
			request.generation_executable);
		touch(request.generation_root / update_generation_directory_name(second) /
			request.generation_executable);
		FakeExecutor fake{
			{FakeAction::launch_error, FakeAction::launch}, {}, 0
		};
		auto result = run_update_process_host(request, fake.seam());
		expect(result && result->recovered && fake.invocations.size() == 2U &&
			load_state(state_path).active == first,
			"active process-creation failure rolls back once and launches previous generation");
	}

	{
		auto request = config(root, root / "invalid" / "PODD.UPDATE");
		request.generation_executable = "nested/th03-app.exe";
		FakeExecutor fake{{FakeAction::launch}, {}, 0};
		auto result = run_update_process_host(request, fake.seam());
		expect(!result && result.error().code ==
			UpdateProcessHostErrorCode::invalid_config &&
			fake.invocations.empty(),
			"generation executable must be one canonical filename");
	}

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
