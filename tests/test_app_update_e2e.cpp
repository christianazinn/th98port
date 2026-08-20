#include "th03/app/update_activation_store.hpp"
#include "th03/app/update_bootstrap.hpp"
#include "th03/app/update_generation.hpp"
#include "th03/app/update_install.hpp"
#include "th03/app/update_process_host.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
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

[[nodiscard]] std::vector<std::byte> read_bytes(const fs::path &path)
{
	std::ifstream input(path, std::ios::binary);
	const std::vector<char> characters{
		std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()
	};
	std::vector<std::byte> result(characters.size());
	for(std::size_t index = 0U; index < characters.size(); ++index) {
		result[index] = static_cast<std::byte>(characters[index]);
	}
	return result;
}

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		path_ = fs::temp_directory_path() / (
			"th03-update-e2e-" + std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()
			)
		);
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
	output << "legacy";
}

struct FakeTransport final {
	std::vector<std::byte> manifest{};
	std::vector<std::byte> package{};
	std::vector<UpdateHttpRequest> requests{};

	[[nodiscard]] UpdateTransport seam() noexcept
	{
		return {.context = this, .perform = perform};
	}

	static std::expected<UpdateHttpResponse, UpdateTransportError> perform(
		void *context,
		const UpdateHttpRequest &request
	)
	{
		auto &self = *static_cast<FakeTransport *>(context);
		self.requests.push_back(request);
		return UpdateHttpResponse{
			.status = 200U,
			.body = self.requests.size() == 1U ?
				self.manifest : self.package,
		};
	}
};

enum class ChildAction : std::uint8_t {
	acknowledge,
	succeed,
	fail,
};

struct FakeExecutor final {
	std::vector<ChildAction> actions{};
	std::vector<UpdateChildInvocation> invocations{};

	[[nodiscard]] UpdateProcessExecutor seam() noexcept
	{
		return {.context = this, .execute = execute};
	}

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
		if(action == ChildAction::fail) {
			return std::unexpected(UpdateChildProcessError{"create failed"});
		}
		if(action == ChildAction::acknowledge) {
			if(!invocation.trial_token) {
				return std::unexpected(UpdateChildProcessError{"missing trial"});
			}
			auto token = decode_update_trial_token(*invocation.trial_token);
			if(!token || !acknowledge_update_launch(
				invocation.activation_state, *token
			)) {
				return std::unexpected(UpdateChildProcessError{"health failed"});
			}
		}
		return 0;
	}
};

[[nodiscard]] UpdateActivationState load_state(const fs::path &path)
{
	const auto loaded = load_update_activation_state(path);
	if(!loaded) {
		++failures;
		return {};
	}
	return loaded->state;
}

[[nodiscard]] UpdateProcessHostConfig host_config(
	const fs::path &root,
	const fs::path &state,
	const fs::path &generations
)
{
	return {
		.activation_state = state,
		.generation_root = generations,
		.legacy_executable = root / "legacy" / "th03-app.exe",
		.working_directory = root / "legacy",
		.generation_executable = "th03-app.exe",
		.application_arguments = {"--new-classic"},
	};
}

} // namespace

int main(int argc, char **argv)
{
	if(argc != 5) {
		std::cerr << "usage: update-e2e-tests "
			"MANIFEST-1 MANIFEST-2 SPKI PACKAGE\n";
		return 2;
	}
	const auto manifest_first = read_bytes(argv[1]);
	const auto manifest_second = read_bytes(argv[2]);
	const auto public_key = read_bytes(argv[3]);
	const auto package = read_bytes(argv[4]);
	const auto first = parse_update_manifest(manifest_first);
	const auto second = parse_update_manifest(manifest_second);
	expect(first && second && first->sequence == 5001U &&
		second->sequence == 5002U &&
		first->signing_key_id == second->signing_key_id &&
		first->artifact_sha256 == second->artifact_sha256 &&
		!public_key.empty() && !package.empty(),
		"two signed consecutive generations share the exact package bytes");
	if(!first || !second) {
		return 1;
	}
	const UpdatePinnedKey pin{first->signing_key_id, public_key};
	const std::span<const UpdatePinnedKey> pins(&pin, 1U);
	const UpdateGenerationIdentity first_identity{
		.sequence = first->sequence,
		.artifact_sha256 = first->artifact_sha256,
	};
	const UpdateGenerationIdentity second_identity{
		.sequence = second->sequence,
		.artifact_sha256 = second->artifact_sha256,
	};

	TemporaryDirectory temporary;
	const auto root = temporary.path();
	const auto state_path = root / "update" / "PODD.UPDATE";
	const auto generation_root = root / "update" / "generations";
	const auto first_path = generation_root /
		update_generation_directory_name(first_identity);
	const auto second_path = generation_root /
		update_generation_directory_name(second_identity);
	touch(root / "legacy" / "th03-app.exe");
	auto config = host_config(root, state_path, generation_root);

	FakeTransport first_transport{manifest_first, package};
	const auto installed_first = install_update_for_next_launch(
		"https://updates.example.test/first.t3upd", pins,
		UpdatePlatform::windows_x64, UpdateChannel::beta, 5000U,
		state_path, generation_root, first_transport.seam()
	);
	expect(installed_first &&
		installed_first->status == UpdateInstallStatus::nominated &&
		installed_first->generation == first_identity &&
		installed_first->generation_path == first_path &&
		fs::is_regular_file(first_path / "th03-app.exe") &&
		load_state(state_path).pending == first_identity,
		"signed first package is materialized and nominated as exact trial bytes");

	FakeExecutor first_trial{{ChildAction::acknowledge}};
	const auto activated_first = run_update_process_host(
		config, first_trial.seam()
	);
	const auto active_first = load_state(state_path);
	expect(activated_first && activated_first->source ==
		UpdateLaunchSource::trial_generation && !activated_first->recovered &&
		first_trial.invocations.size() == 1U &&
		first_trial.invocations.front().executable ==
			first_path / "th03-app.exe" &&
		first_trial.invocations.front().trial_token &&
		active_first.phase == UpdateActivationPhase::stable &&
		active_first.active == first_identity && !active_first.pending,
		"bootstrap trials and health-activates the exact installed first generation");

	FakeExecutor stable_first{{ChildAction::succeed}};
	const auto relaunched_first = run_update_process_host(
		config, stable_first.seam()
	);
	expect(relaunched_first && relaunched_first->source ==
		UpdateLaunchSource::active_generation &&
		stable_first.invocations.size() == 1U &&
		!stable_first.invocations.front().trial_token &&
		stable_first.invocations.front().executable ==
			first_path / "th03-app.exe" &&
		load_state(state_path) == active_first,
		"next launch selects the stable generation without a trial token");

	FakeTransport second_transport{manifest_second, package};
	const auto installed_second = install_update_for_next_launch(
		"https://updates.example.test/second.t3upd", pins,
		UpdatePlatform::windows_x64, UpdateChannel::beta, 5001U,
		state_path, generation_root, second_transport.seam()
	);
	expect(installed_second &&
		installed_second->status == UpdateInstallStatus::nominated &&
		installed_second->generation == second_identity &&
		installed_second->generation_path == second_path &&
		second_path != first_path &&
		fs::is_regular_file(second_path / "th03-app.exe") &&
		load_state(state_path).pending == second_identity,
		"newer signed generation is independently materialized and nominated");

	FakeExecutor failed_second{{ChildAction::fail, ChildAction::succeed}};
	const auto rolled_back = run_update_process_host(
		config, failed_second.seam()
	);
	const auto recovered = load_state(state_path);
	expect(rolled_back && rolled_back->recovered && rolled_back->source ==
		UpdateLaunchSource::active_generation &&
		failed_second.invocations.size() == 2U &&
		failed_second.invocations[0U].executable ==
			second_path / "th03-app.exe" &&
		failed_second.invocations[0U].trial_token &&
		failed_second.invocations[1U].executable ==
			first_path / "th03-app.exe" &&
		!failed_second.invocations[1U].trial_token &&
		recovered.phase == UpdateActivationPhase::stable &&
		recovered.active == first_identity &&
		recovered.last_failed == second_identity && !recovered.pending,
		"failed newer trial rolls back and launches the retained stable generation");

	FakeExecutor stable_after_failure{{ChildAction::succeed}};
	const auto final_launch = run_update_process_host(
		config, stable_after_failure.seam()
	);
	expect(final_launch && final_launch->source ==
		UpdateLaunchSource::active_generation && !final_launch->recovered &&
		stable_after_failure.invocations.size() == 1U &&
		stable_after_failure.invocations.front().executable ==
			first_path / "th03-app.exe" &&
		load_state(state_path) == recovered,
		"post-rollback launch remains on the last health-acknowledged generation");

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
