#include "th03/app/new_classic_online_task.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;
int checks = 0;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

template<class Tag>
th03::app::NewClassicOnlineIdentity<Tag> identity(std::uint8_t seed)
{
	th03::app::NewClassicOnlineIdentity<Tag> value{};
	for(std::size_t index = 0U; index < value.bytes.size(); ++index) {
		value.bytes[index] = static_cast<std::uint8_t>(seed + index);
	}
	return value;
}

struct FakeTransport final {
	std::atomic<unsigned> calls = 0U;
	std::atomic<bool> release = false;
	std::atomic<bool> throw_request = false;
	std::mutex targets_mutex{};
	std::vector<std::string> targets{};

	static std::expected<
		th03::app::NewClassicOnlineHttpResponse,
		th03::app::NewClassicOnlineTransportError
	> perform(
		void *context,
		const th03::app::NewClassicOnlineHttpRequest &request
	)
	{
		auto &self = *static_cast<FakeTransport *>(context);
		++self.calls;
		if(self.throw_request.load(std::memory_order_acquire)) {
			throw std::runtime_error("injected worker failure");
		}
		while(!self.release.load(std::memory_order_acquire)) {
			std::this_thread::yield();
		}
		{
			const std::scoped_lock lock(self.targets_mutex);
			self.targets.push_back(request.target);
		}
		const auto query = request.target.find(":c1") != std::string::npos;
		const th03::app::NewClassicOnlineCategory category{
			.mode = th03::app::NewClassicOnlineMode::story,
			.rank = 2U,
			.character = static_cast<std::uint8_t>(query ? 1U : 0U),
		};
		th03::app::NewClassicOnlineCacheDocument document{};
		document.generation = 0U;
		document.snapshot_id = identity<th03::app::NewClassicOnlineSnapshotTag>(
			static_cast<std::uint8_t>(0x10U + category.character)
		);
		document.fetched_unix_seconds = 1786766100ULL;
		document.board_category = category;
		auto encoded = th03::app::encode_new_classic_online_cache(document);
		if(!encoded) {
			return std::unexpected(th03::app::NewClassicOnlineTransportError{
				.code = th03::app::NewClassicOnlineTransportErrorCode::request,
			});
		}
		return th03::app::NewClassicOnlineHttpResponse{
			.status = 200U,
			.content_type = "application/vnd.th03.online-cache",
			.body = std::move(*encoded),
		};
	}

	th03::app::NewClassicOnlineTransport seam() noexcept
	{
		return {this, &perform};
	}
};

std::filesystem::path temporary_root()
{
	const auto root = std::filesystem::temp_directory_path() /
		"th03-online-task-tests";
	std::error_code ignored{};
	std::filesystem::remove_all(root, ignored);
	return root;
}

} // namespace

int main()
{
	using namespace th03::app;
	const auto invalid = start_new_classic_online_board_refresh(
		{{NewClassicOnlineMode::story, 2U, 0U, 0U, 0U}}, {}, {}
	);
	expect(!invalid && invalid.error().code ==
		NewClassicOnlineTaskErrorCode::invalid_transport,
		"invalid transport is rejected before a worker starts");

	FakeTransport fake{};
	const auto root = temporary_root();
	std::filesystem::create_directories(root);
	auto task = start_new_classic_online_board_refresh(
		{
			{NewClassicOnlineMode::story, 2U, 0U, 0U, 0U},
			{NewClassicOnlineMode::story, 2U, 1U, 0U, 0U},
		},
		root,
		fake.seam()
	);
	expect(task.has_value(), "valid task starts");
	if(!task) {
		return 1;
	}
	for(unsigned spin = 0U; spin < 100000U && fake.calls.load() == 0U; ++spin) {
		std::this_thread::yield();
	}
	expect(fake.calls.load() == 1U,
		"background request starts without blocking the caller");
	expect(!task->take_completed(),
		"an in-flight request cannot leak a partial completion");
	fake.release.store(true, std::memory_order_release);

	std::vector<NewClassicOnlineBoardRefreshResult> results{};
	const auto deadline = std::chrono::steady_clock::now() +
		std::chrono::seconds(5);
	while(std::chrono::steady_clock::now() < deadline &&
		(!task->finished() || results.size() < 2U)) {
		while(auto result = task->take_completed()) {
			results.push_back(std::move(*result));
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	while(auto result = task->take_completed()) {
		results.push_back(std::move(*result));
	}
	expect(task->finished(), "worker reports completion");
	expect(results.size() == 2U, "every requested board has one completion");
	if(results.size() == 2U) {
		expect(results[0].category.character == 0U && results[0].document &&
			!results[0].error,
			"first board returns one complete validated document");
		expect(results[1].category.character == 1U && results[1].document &&
			!results[1].error,
			"second board returns one complete validated document");
	}
	expect(std::filesystem::exists(
		new_classic_online_board_cache_path(
			root, {NewClassicOnlineMode::story, 2U, 0U, 0U, 0U}
		)
	), "worker atomically publishes the first cache");
	expect(std::filesystem::exists(
		new_classic_online_board_cache_path(
			root, {NewClassicOnlineMode::story, 2U, 1U, 0U, 0U}
		)
	), "worker atomically publishes the second cache");

	FakeTransport throwing{};
	throwing.throw_request.store(true, std::memory_order_release);
	auto failed_task = start_new_classic_online_board_refresh(
		{{NewClassicOnlineMode::story, 2U, 0U, 0U, 0U}},
		root,
		throwing.seam()
	);
	expect(failed_task.has_value(), "throwing transport still starts a worker");
	if(failed_task) {
		const auto failure_deadline = std::chrono::steady_clock::now() +
			std::chrono::seconds(5);
		while(!failed_task->finished() &&
			std::chrono::steady_clock::now() < failure_deadline) {
			std::this_thread::yield();
		}
		expect(failed_task->finished() && failed_task->worker_failed(),
			"unexpected worker exceptions fail closed without terminating");
		expect(!failed_task->take_completed(),
			"a failed worker cannot publish a partial completion");
	}

	std::error_code ignored{};
	std::filesystem::remove_all(root, ignored);
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
