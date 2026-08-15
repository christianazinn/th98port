#include "th03/app/new_classic_online_client.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "th03/app/main_compiled_content.hpp"
#include "th03/formats/v13.hpp"

namespace {

namespace fs = std::filesystem;
using namespace th03::app;

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

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		static std::uint64_t sequence = 0U;
		path_ = fs::temp_directory_path() /
			("th03-online-client-test-" + std::to_string(++sequence));
		std::error_code ignored;
		fs::remove_all(path_, ignored);
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

template<typename Identity>
[[nodiscard]] Identity identity(std::uint8_t seed)
{
	Identity result{};
	for(std::size_t index = 0U; index < result.bytes.size(); ++index) {
		result.bytes[index] = static_cast<std::uint8_t>(seed + index);
	}
	return result;
}

[[nodiscard]] std::vector<std::byte> bytes(std::string_view value)
{
	return {
		reinterpret_cast<const std::byte *>(value.data()),
		reinterpret_cast<const std::byte *>(value.data() + value.size())
	};
}

struct FakeTransport final {
	std::expected<NewClassicOnlineHttpResponse, NewClassicOnlineTransportError>
		response = NewClassicOnlineHttpResponse{};
	std::size_t calls = 0U;
	NewClassicOnlineHttpMethod method = NewClassicOnlineHttpMethod::get;
	std::string target{};
	std::vector<NewClassicOnlineHttpHeader> headers{};
	std::vector<std::byte> body{};
	std::size_t maximum_response_bytes = 0U;

	[[nodiscard]] NewClassicOnlineTransport seam() noexcept
	{
		return {this, &perform};
	}

	static std::expected<
		NewClassicOnlineHttpResponse,
		NewClassicOnlineTransportError
	> perform(void *context, const NewClassicOnlineHttpRequest &request)
	{
		auto &self = *static_cast<FakeTransport *>(context);
		++self.calls;
		self.method = request.method;
		self.target = request.target;
		self.headers = request.headers;
		self.body.assign(request.body.begin(), request.body.end());
		self.maximum_response_bytes = request.maximum_response_bytes;
		return self.response;
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
	metadata.story_stage = 0U;
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

[[nodiscard]] std::uint64_t score_value(
	const std::array<std::uint8_t, 4U> &packed
)
{
	std::uint64_t result = 0U;
	for(auto byte = packed.rbegin(); byte != packed.rend(); ++byte) {
		result = result * 10U + ((*byte >> 4U) & 0x0FU);
		result = result * 10U + (*byte & 0x0FU);
	}
	return result * 10U;
}

[[nodiscard]] NewClassicOnlineReplayRecord replay_record(
	std::span<const std::byte> encoded,
	const th03::formats::ReplayDocument &document
)
{
	NewClassicOnlineReplayId replay_id{};
	replay_id.bytes = app_sha256(encoded);
	return {
		.replay_id = replay_id,
		.content_id = identity<NewClassicOnlineContentId>(0x40U),
		.ruleset_id = identity<NewClassicOnlineRulesetId>(0x60U),
		.category = {NewClassicOnlineMode::story, 2U, 0U, 0U, 0U},
		.result = {
			NewClassicOnlineResultKind::game_over,
			score_value(document.metadata->final_score), 0U, 0U,
		},
		.availability = NewClassicOnlineReplayAvailability::metadata_only,
		.dos_date = document.metadata->dos_date,
		.payload_bytes = static_cast<std::uint32_t>(encoded.size()),
		.autofire = document.metadata->autofire,
		.timed_frames = document.slowdown->timed_frames,
		.slow_frames = document.slowdown->slow_frames,
		.title = "ALPHA",
	};
}

[[nodiscard]] NewClassicOnlineCacheDocument cache_document(
	const NewClassicOnlineCategory &category
)
{
	NewClassicOnlineCacheDocument document{};
	document.snapshot_id = identity<NewClassicOnlineSnapshotId>(0xA0U);
	document.fetched_unix_seconds = 1786766000ULL;
	document.board_category = category;
	return document;
}

[[nodiscard]] std::vector<std::byte> read_file(const fs::path &path)
{
	std::ifstream input(path, std::ios::binary);
	input.seekg(0, std::ios::end);
	const auto size = input.tellg();
	input.seekg(0, std::ios::beg);
	std::vector<std::byte> result(static_cast<std::size_t>(size));
	input.read(
		reinterpret_cast<char *>(result.data()),
		static_cast<std::streamsize>(result.size())
	);
	return result;
}

void test_board_refresh()
{
	TemporaryDirectory root;
	const NewClassicOnlineCategory category{
		NewClassicOnlineMode::story, 2U, 4U, 0U, 0U
	};
	auto document = cache_document(category);
	auto encoded = encode_new_classic_online_cache(document);
	expect(encoded.has_value(), "board fixture encodes");
	FakeTransport fake{};
	fake.response = NewClassicOnlineHttpResponse{
		.status = 200U,
		.content_type = "application/vnd.th03.online-cache",
		.body = *encoded,
	};
	auto refreshed = refresh_new_classic_online_board(
		category, root.path(), fake.seam()
	);
	expect(refreshed && refreshed->generation == 1U,
		"first board refresh atomically assigns local generation one");
	expect(fake.calls == 1U && fake.method == NewClassicOnlineHttpMethod::get &&
		fake.target == "/api/th03/leaderboard?category=story:r2:c4&format=t3onl2" &&
		fake.maximum_response_bytes == new_classic_online_cache_max_bytes,
		"board refresh uses the exact bounded binary route");
	const auto path = new_classic_online_board_cache_path(root.path(), category);
	auto loaded = load_new_classic_online_store(path);
	expect(loaded && loaded->document == *refreshed,
		"published per-board cache reloads exactly");

	fake.response->body = *encoded;
	refreshed = refresh_new_classic_online_board(category, root.path(), fake.seam());
	expect(refreshed && refreshed->generation == 2U,
		"service generation zero cannot roll back the local atomic generation");

	auto wrong = cache_document({
		NewClassicOnlineMode::story, 1U, 4U, 0U, 0U
	});
	fake.response->body = *encode_new_classic_online_cache(wrong);
	auto rejected = refresh_new_classic_online_board(
		category, root.path(), fake.seam()
	);
	expect(!rejected && rejected.error().code ==
		NewClassicOnlineClientErrorCode::cache_mismatch,
		"a response for another board never replaces the requested cache");
	loaded = load_new_classic_online_store(path);
	expect(loaded && loaded->document.generation == 2U,
		"failed refresh preserves the previous board cache");

	expect(new_classic_online_board_cache_path(root.path(), {
		NewClassicOnlineMode::practice, 0U, 0U, 0U, 0U
	}).empty(), "v1 rejects non-Story board paths");
}

void test_enrollment_and_upload()
{
	FakeTransport fake{};
	fake.response = NewClassicOnlineHttpResponse{
		.status = 201U,
		.content_type = "application/json",
		.body = bytes(
			"{\"userId\":\"12345678-1234-1234-1234-123456789abc\","
			"\"token\":\"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef0123456789_-\"}"
		),
	};
	auto credentials = enroll_new_classic_online_user(fake.seam());
	expect(credentials && fake.target == "/api/th03/users" &&
		fake.method == NewClassicOnlineHttpMethod::post && fake.body.empty(),
		"enrollment accepts only the bounded service credential document");

	auto document = story_document();
	auto encoded = th03::formats::write_v13_replay(document);
	expect(encoded.has_value(), "upload fixture is canonical V13");
	NewClassicOnlineReplayId upload_id{};
	upload_id.bytes = app_sha256(*encoded);
	const auto upload_name = new_classic_online_replay_filename(upload_id);
	fake.response = NewClassicOnlineHttpResponse{
		.status = 201U,
		.content_type = "application/json",
		.body = bytes(
			"{\"status\":\"accepted\","
			"\"replayId\":\"" + upload_name.substr(0U, 64U) + "\","
			"\"entryId\":\"abcdef12-1234-1234-1234-123456789abc\"}"
		),
	};
	auto upload = upload_new_classic_online_replay(
		document, *credentials, fake.seam()
	);
	expect(upload && upload->status == NewClassicOnlineUploadStatus::accepted &&
		fake.target == "/api/th03/replays" && fake.body == *encoded,
		"upload sends the canonical V13 body directly from memory");
	expect(std::any_of(fake.headers.begin(), fake.headers.end(), [](const auto &header) {
		return header.name == "authorization" &&
			header.value.starts_with("Bearer ");
	}), "upload carries the private bearer credential only in HTTPS headers");

	fake.response = NewClassicOnlineHttpResponse{
		.status = 422U,
		.content_type = "application/json",
		.body = bytes("{\"status\":\"rejected\",\"reason\":\"slowdown\"}"),
	};
	upload = upload_new_classic_online_replay(document, *credentials, fake.seam());
	expect(upload && upload->status == NewClassicOnlineUploadStatus::rejected &&
		upload->reason == "slowdown",
		"server rejection remains a normal two-state upload result");

	const auto calls = fake.calls;
	document.slowdown->slow_frames = 6U;
	upload = upload_new_classic_online_replay(document, *credentials, fake.seam());
	expect(!upload && upload.error().code ==
		NewClassicOnlineClientErrorCode::invalid_document && fake.calls == calls,
		"greater-than-five-percent slowdown is rejected before any network call");
}

void test_download()
{
	TemporaryDirectory root;
	auto document = story_document();
	auto encoded = th03::formats::write_v13_replay(document);
	expect(encoded.has_value(), "download fixture is canonical V13");
	auto record = replay_record(*encoded, document);
	FakeTransport fake{};
	fake.response = NewClassicOnlineHttpResponse{
		.status = 200U,
		.content_type = "application/vnd.th03.v13-replay",
		.body = *encoded,
	};
	auto downloaded = download_new_classic_online_replay(
		record, root.path(), fake.seam()
	);
	expect(downloaded && downloaded->parent_path().filename() == "ONLINE" &&
		read_file(*downloaded) == *encoded,
		"download publishes exact content-addressed bytes after validation");
	expect(fake.target == "/api/th03/replays/" +
		new_classic_online_replay_filename(record.replay_id).substr(0U, 64U),
		"download target is derived only from the cached replay digest");

	auto tampered = *encoded;
	tampered.back() ^= std::byte{1U};
	fake.response->body = tampered;
	auto rejected = download_new_classic_online_replay(
		record, root.path(), fake.seam()
	);
	expect(!rejected && rejected.error().code ==
		NewClassicOnlineClientErrorCode::replay_digest &&
		read_file(*downloaded) == *encoded,
		"digest failure preserves a previously downloaded replay");

	fake.response->body = *encoded;
	record.result.score += 10U;
	rejected = download_new_classic_online_replay(record, root.path(), fake.seam());
	expect(!rejected && rejected.error().code ==
		NewClassicOnlineClientErrorCode::replay_metadata,
		"cached leaderboard metadata must agree with downloaded V13 metadata");
}

} // namespace

int main()
{
	test_board_refresh();
	test_enrollment_and_upload();
	test_download();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
