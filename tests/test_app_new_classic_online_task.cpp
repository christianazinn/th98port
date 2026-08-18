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

#include "th03/app/main_compiled_content.hpp"
#include "th03/formats/v13.hpp"

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
		if(request.target == "/api/th03/replays") {
			th03::app::NewClassicOnlineReplayId replay_id{};
			replay_id.bytes = th03::app::app_sha256(request.body);
			const auto filename =
				th03::app::new_classic_online_replay_filename(replay_id);
			const auto json = std::string{"{\"status\":\"accepted\","}
				+ "\"replayId\":\"" + filename.substr(0U, 64U) + "\","
				+ "\"entryId\":\"abcdef12-1234-1234-1234-123456789abc\"}";
			return th03::app::NewClassicOnlineHttpResponse{
				.status = 201U,
				.content_type = "application/json",
				.body = std::vector<std::byte>(
					reinterpret_cast<const std::byte *>(json.data()),
					reinterpret_cast<const std::byte *>(json.data() + json.size())
				),
			};
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

struct FakeCredentialSource final {
	std::atomic<unsigned> calls = 0U;
	bool fail = false;

	static std::expected<
		th03::app::NewClassicOnlineCredentials,
		th03::app::NewClassicOnlineCredentialAcquireError
	> acquire(void *context)
	{
		auto &self = *static_cast<FakeCredentialSource *>(context);
		++self.calls;
		if(self.fail) {
			return std::unexpected(
				th03::app::NewClassicOnlineCredentialAcquireError{7U}
			);
		}
		return th03::app::NewClassicOnlineCredentials{
			"12345678-1234-1234-1234-123456789abc",
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef0123456789_-",
		};
	}

	[[nodiscard]] th03::app::NewClassicOnlineCredentialSource seam() noexcept
	{
		return {this, &acquire};
	}
};

[[nodiscard]] th03::formats::ReplayDocument story_document()
{
	th03::GameInit init{};
	init.resident_rand = 29;
	init.random_seed = 29U;
	init.game_mode = th03::game_mode::story;
	init.rank = 2U;
	init.skill = 120U;
	init.story_lives = 2U;
	init.remaining_credits = 3U;
	init.is_cpu = {0U, 1U};
	init.paletted_character = {1, 4};
	init.story_opponents[0U] = 4;
	init.autofire_mask = 1U;
	init.compact_runtime_snapshot = th03::CompactRuntimeSnapshot{
		.round_reset_seed = 29U,
		.random_seed = init.random_seed,
		.first_formation = 1U,
	};
	init.round_start_snapshot = th03::RoundStartSnapshot{};

	th03::formats::ReplayMetadata metadata{};
	metadata.flags = 3U;
	metadata.status = th03::formats::ReplayStatus::finalized;
	metadata.end_reason = th03::formats::ReplayEndReason::game_over;
	metadata.game_mode = init.game_mode;
	metadata.rank = init.rank;
	metadata.paletted_character = init.paletted_character;
	metadata.is_cpu = init.is_cpu;
	metadata.sample_count = 1U;
	metadata.final_frame_count = 1U;
	metadata.resident_rand = init.resident_rand;
	metadata.random_seed = init.random_seed;
	metadata.summary_flags = 0x7FU;
	metadata.final_route = std::uint8_t{2U};
	metadata.final_game_mode = init.game_mode;
	metadata.final_story_stage = std::uint8_t{0U};
	metadata.final_round_id = std::uint8_t{0U};
	metadata.final_winner = std::int8_t{1};
	metadata.final_story_lives = 0U;
	metadata.stage_reached_count = 1U;
	metadata.story.emplace();
	metadata.story->stage_opponents[0U] = 4U;
	metadata.story->stage_scores[0U] = {1U, 2U, 3U, 4U};
	metadata.final_score = {1U, 2U, 3U, 4U};
	metadata.autofire = init.autofire_mask;
	metadata.dos_date = 0x5D2FU;
	metadata.name = {{'A','L','P','H','A',' ',' ',' '}};

	th03::formats::ReplayDocument document{};
	document.session.source_version = th03::formats::replay_v13_version;
	document.session.init = init;
	document.session.records = {
		th03::replay::InputSample{
			.frame_index = 0U,
			.phase = th03::replay::InputPhase::gameplay,
			.input = {},
			.round_frame = std::nullopt,
			.round_or_result_frame = std::nullopt,
		},
		th03::replay::ProcessBoundary{
			.sample_index = 1U,
			.completed_process = th03::replay::ProcessId::main,
		},
	};
	document.session.sample_count = 1U;
	document.metadata = metadata;
	document.checkpoints.push_back({
		.stage = 0U,
		.round = 0U,
		.sample_index = 0U,
		.global_frame = 0U,
		.input_byte_cursor = 0U,
		.init = init,
	});
	document.expected_round_splits.push_back({
		.story_stage = 0U,
		.round_id = 0U,
		.route = 2U,
		.winner = 1,
		.p1_score = metadata.final_score,
		.p2_score = {},
		.real_frames = 1U,
	});
	document.stage_clear_bonuses.emplace();
	document.slowdown = th03::formats::ReplaySlowdown{
		.timed_frames = 100U,
		.slow_frames = 5U,
	};
	return document;
}

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

	FakeCredentialSource my_records_credentials{};
	auto my_records = start_new_classic_online_my_records_refresh(
		{
			{NewClassicOnlineMode::story, 2U, 0U, 0U, 0U},
			{NewClassicOnlineMode::story, 2U, 1U, 0U, 0U},
		},
		my_records_credentials.seam(), fake.seam()
	);
	expect(my_records.has_value(), "My Records background refresh starts");
	if(my_records) {
		std::vector<NewClassicOnlineBoardRefreshResult> private_results{};
		const auto private_deadline = std::chrono::steady_clock::now() +
			std::chrono::seconds(5);
		while(std::chrono::steady_clock::now() < private_deadline &&
			(!my_records->finished() || private_results.size() < 2U)) {
			while(auto result = my_records->take_completed()) {
				private_results.push_back(std::move(*result));
			}
			std::this_thread::yield();
		}
		while(auto result = my_records->take_completed()) {
			private_results.push_back(std::move(*result));
		}
		expect(my_records->finished() && private_results.size() == 2U &&
			my_records_credentials.calls.load() == 1U &&
			!my_records->take_credential_error(),
			"My Records acquires identity once and returns every category");
		const std::scoped_lock lock(fake.targets_mutex);
		expect(fake.targets.size() >= 4U &&
			fake.targets[fake.targets.size() - 2U].starts_with(
				"/api/th03/users/12345678-1234-1234-1234-"
				"123456789abc/records?category=story:"
			), "My Records uses the authenticated per-user route");
	}

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

	NewClassicOnlineReplayRecord replay{};
	replay.replay_id = identity<NewClassicOnlineReplayTag>(0x90U);
	replay.content_id = identity<NewClassicOnlineContentTag>(0x40U);
	replay.ruleset_id = identity<NewClassicOnlineRulesetTag>(0x60U);
	replay.category = {NewClassicOnlineMode::story, 2U, 0U, 0U, 0U};
	replay.result = {
		.kind = NewClassicOnlineResultKind::clear,
		.score = 123'456'780U,
		.end_stage = 8U,
	};
	replay.dos_date = 0x5D2FU;
	replay.payload_bytes = 100U;
	replay.timed_frames = 100U;
	replay.title = "ALPHA";
	auto download = start_new_classic_online_replay_download(
		replay, root, fake.seam()
	);
	expect(download.has_value(),
		"valid replay download starts without blocking the caller");
	if(download) {
		const auto download_deadline = std::chrono::steady_clock::now() +
			std::chrono::seconds(5);
		while(!download->finished() &&
			std::chrono::steady_clock::now() < download_deadline) {
			std::this_thread::yield();
		}
		auto result = download->take_completed();
		expect(download->finished() && result && result->error &&
			!result->path && result->replay_id == replay.replay_id,
			"download worker publishes one complete typed failure result");
		const std::scoped_lock lock(fake.targets_mutex);
		expect(!fake.targets.empty() && fake.targets.back().starts_with(
			"/api/th03/replays/"
		), "download worker derives the service target from ReplayId");
	}

	FakeCredentialSource credential_source{};
	auto upload = start_new_classic_online_replay_upload(
		story_document(), 4U, credential_source.seam(), fake.seam()
	);
	expect(upload.has_value(),
		"valid immutable replay upload starts without blocking the caller");
	if(upload) {
		const auto upload_deadline = std::chrono::steady_clock::now() +
			std::chrono::seconds(5);
		while(!upload->finished() &&
			std::chrono::steady_clock::now() < upload_deadline) {
			std::this_thread::yield();
		}
		auto result = upload->take_completed();
		expect(upload->finished() && result && result->slot == 4U &&
			result->upload && result->upload->status ==
				NewClassicOnlineUploadStatus::accepted && !result->error &&
			!result->credential_error && credential_source.calls.load() == 1U,
			"upload worker acquires identity and publishes one typed acceptance");
		const std::scoped_lock lock(fake.targets_mutex);
		expect(!fake.targets.empty() && fake.targets.back() ==
			"/api/th03/replays",
			"upload worker sends canonical V13 directly to the replay endpoint");
	}

	FakeCredentialSource rejected_credentials{};
	rejected_credentials.fail = true;
	auto credential_failure = start_new_classic_online_replay_upload(
		story_document(), 5U, rejected_credentials.seam(), fake.seam()
	);
	expect(credential_failure.has_value(),
		"credential failure is represented by a started bounded worker");
	if(credential_failure) {
		const auto credential_deadline = std::chrono::steady_clock::now() +
			std::chrono::seconds(5);
		while(!credential_failure->finished() &&
			std::chrono::steady_clock::now() < credential_deadline) {
			std::this_thread::yield();
		}
		auto result = credential_failure->take_completed();
		expect(result && result->slot == 5U && result->credential_error &&
			result->credential_error->detail_code == 7U && !result->upload &&
			!result->error,
			"credential acquisition failure cannot start an upload transaction");
	}

	std::error_code ignored{};
	std::filesystem::remove_all(root, ignored);
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
