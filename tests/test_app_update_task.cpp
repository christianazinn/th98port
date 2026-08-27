#include "th03/app/update/update_generation.hpp"
#include "th03/app/update/update_task.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <thread>
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
			"th03-update-task-" + std::to_string(
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

struct FakeTransport final {
	std::vector<std::byte> manifest{};
	std::vector<std::byte> artifact{};
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
				self.manifest : self.artifact,
		};
	}
};

[[nodiscard]] bool await(UpdateInstallTask &task)
{
	const auto deadline = std::chrono::steady_clock::now() +
		std::chrono::seconds(5);
	while(!task.finished() && std::chrono::steady_clock::now() < deadline) {
		std::this_thread::yield();
	}
	return task.finished();
}

} // namespace

int main(int argc, char **argv)
{
	bool automatic_attempted = false;
	expect(!claim_new_classic_automatic_update_attempt(
		false, true, automatic_attempted
	) && !automatic_attempted,
		"unavailable automatic checks do not consume the process latch");
	expect(!claim_new_classic_automatic_update_attempt(
		true, false, automatic_attempted
	) && !automatic_attempted,
		"disabled automatic checks do not consume the process latch");
	expect(claim_new_classic_automatic_update_attempt(
		true, true, automatic_attempted
	) && automatic_attempted,
		"first enabled capable automatic check claims the process latch");
	expect(!claim_new_classic_automatic_update_attempt(
		true, true, automatic_attempted
	) && automatic_attempted,
		"later automatic checks in the same process are suppressed");

	const auto absent_runtime = parse_update_runtime_context({}, {});
	expect(absent_runtime && !*absent_runtime,
		"ordinary direct launch has no update runtime context");
	const auto incomplete_runtime = parse_update_runtime_context(
		"PODD.UPDATE", {}
	);
	expect(!incomplete_runtime && incomplete_runtime.error() ==
		UpdateRuntimeContextError::incomplete,
		"partial bootstrap runtime context fails closed");
	const auto parsed_runtime = parse_update_runtime_context(
		"PODD.UPDATE", "generations"
	);
	expect(parsed_runtime && *parsed_runtime &&
		(*parsed_runtime)->activation_state == "PODD.UPDATE" &&
		(*parsed_runtime)->generation_root == "generations",
		"complete bootstrap runtime context is preserved exactly");

	if(argc != 4) {
		std::cerr << "usage: update-task-tests MANIFEST SPKI PACKAGE\n";
		return 2;
	}
	const auto manifest = read_bytes(argv[1]);
	const auto public_key = read_bytes(argv[2]);
	const auto package = read_bytes(argv[3]);
	const auto parsed = parse_update_manifest(manifest);
	expect(parsed && !public_key.empty() && !package.empty(),
		"signed task fixtures parse");
	if(!parsed) {
		return 1;
	}
	const UpdatePinnedKey pin{parsed->signing_key_id, public_key};
	const UpdateReleaseConfiguration configuration{
		.manifest_url = "https://updates.example.test/beta.t3upd",
		.platform = UpdatePlatform::windows_x64,
		.channel = UpdateChannel::beta,
		.installed_sequence = parsed->sequence - 1U,
		.pinned_keys = std::span<const UpdatePinnedKey>(&pin, 1U),
	};
	TemporaryDirectory temporary;
	const UpdateRuntimeContext runtime{
		.activation_state = temporary.path() / "update" / "PODD.UPDATE",
		.generation_root = temporary.path() / "update" / "generations",
	};

	FakeTransport transport{manifest, package};
	const std::optional<UpdateReleaseConfiguration> configured{configuration};
	const std::optional<UpdateRuntimeContext> runtime_available{runtime};
	expect(new_classic_update_available(
		true, false, false, configured, runtime_available, transport.seam()
	), "complete New Classic update capability is advertised");
	expect(!new_classic_update_available(
		false, false, false, configured, runtime_available, transport.seam()
	) && !new_classic_update_available(
		true, true, false, configured, runtime_available, transport.seam()
	) && !new_classic_update_available(
		true, false, true, configured, runtime_available, transport.seam()
	), "Classic and source-capture routes never advertise Updates");
	expect(!new_classic_update_available(
		true, false, false, {}, runtime_available, transport.seam()
	) && !new_classic_update_available(
		true, false, false, configured, {}, transport.seam()
	) && !new_classic_update_available(
		true, false, false, configured, runtime_available, {}
	), "partial host update capability omits the row completely");
	auto task = start_update_install_task(
		configuration, runtime, transport.seam()
	);
	expect(task.has_value(), "valid task starts without blocking source time");
	if(task) {
		expect(await(*task), "background install finishes within bounded test time");
		auto result = task->take_completed();
		expect(result && *result &&
			(*result)->status == UpdateInstallStatus::nominated &&
			(*result)->generation.sequence == parsed->sequence &&
			!task->worker_failed() && transport.requests.size() == 2U,
			"background task nominates the authenticated generation");
		expect(!task->take_completed(), "task completion is consumed exactly once");
	}

	FakeTransport current_transport{manifest, package};
	auto current = start_update_install_task(
		configuration, runtime, current_transport.seam()
	);
	expect(current && await(*current), "repeat check completes in background");
	if(current) {
		auto result = current->take_completed();
		expect(result && !*result && result->error().code ==
			UpdateInstallErrorCode::acquisition &&
			result->error().acquisition &&
			result->error().acquisition->code ==
				UpdateAcquisitionErrorCode::current_or_older &&
			current_transport.requests.size() == 1U,
			"already-current result avoids a package download");
	}

	auto bad_runtime = start_update_install_task(
		configuration, {}, transport.seam()
	);
	expect(!bad_runtime && bad_runtime.error().code ==
		UpdateTaskStartErrorCode::invalid_runtime_context,
		"missing bootstrap runtime paths fail before worker start");
	auto bad_transport = start_update_install_task(
		configuration, runtime, {}
	);
	expect(!bad_transport && bad_transport.error().code ==
		UpdateTaskStartErrorCode::invalid_transport,
		"missing transport fails before worker start");
	auto bad_configuration = configuration;
	bad_configuration.installed_sequence = 0U;
	auto bad_config_task = start_update_install_task(
		bad_configuration, runtime, transport.seam()
	);
	expect(!bad_config_task && bad_config_task.error().code ==
		UpdateTaskStartErrorCode::invalid_configuration,
		"invalid release trust fails before worker start");

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
