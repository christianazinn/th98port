#include "th03/app/new_classic/online/new_classic_online.hpp"
#include "th03/app/new_classic/online/new_classic_online_store.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {

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

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		static std::uint64_t sequence = 0U;
		path_ = fs::temp_directory_path() /
			("th03-online-test-" + std::to_string(++sequence));
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

[[nodiscard]] th03::app::NewClassicOnlineReplayRecord replay_record()
{
	using namespace th03::app;
	return {
		identity<NewClassicOnlineReplayId>(0x20U),
		identity<NewClassicOnlineContentId>(0x40U),
		identity<NewClassicOnlineRulesetId>(0x60U),
		{NewClassicOnlineMode::story, 2U, 4U, 0U, 0U},
		{NewClassicOnlineResultKind::clear, 123456780ULL, 8U, 0U},
		NewClassicOnlineReplayAvailability::downloaded,
		0x5D2FU,
		213592U,
		1U,
		10'000U,
		200U,
		"Lunatic Reimu clear",
	};
}

[[nodiscard]] th03::app::NewClassicLeaderboardEntry verified_entry()
{
	using namespace th03::app;
	const auto replay = replay_record();
	return {
		identity<NewClassicOnlineEntryId>(0x10U),
		replay.content_id,
		replay.ruleset_id,
		replay.replay_id,
		identity<NewClassicOnlineVerifierId>(0x80U),
		replay.category,
		replay.result,
		NewClassicOnlineProvenance::replay_verified,
		replay.dos_date,
		1786765432ULL,
		"Alice",
		{},
		{},
	};
}

[[nodiscard]] th03::app::NewClassicLeaderboardEntry legacy_entry()
{
	using namespace th03::app;
	return {
		identity<NewClassicOnlineEntryId>(0x11U),
		identity<NewClassicOnlineContentId>(0x40U),
		identity<NewClassicOnlineRulesetId>(0x60U),
		std::nullopt,
		std::nullopt,
		{NewClassicOnlineMode::story, 2U, 4U, 0U, 0U},
		{NewClassicOnlineResultKind::game_over, 987654320ULL, 7U, 1U},
		NewClassicOnlineProvenance::curated_legacy,
		0x4A21U,
		1786765000ULL,
		"Pre-patch WR",
		{},
		{},
	};
}

[[nodiscard]] th03::app::NewClassicOnlineCacheDocument full_document()
{
	using namespace th03::app;
	NewClassicOnlineCacheDocument document{};
	document.generation = 7U;
	document.snapshot_id = identity<NewClassicOnlineSnapshotId>(0xA0U);
	document.fetched_unix_seconds = 1786766000ULL;
	document.board_category = replay_record().category;
	document.leaderboard_entries = {verified_entry(), legacy_entry()};
	document.replay_records = {replay_record()};
	return document;
}

void write_bytes(const fs::path &path, const std::vector<std::byte> &bytes)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(
		reinterpret_cast<const char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
}

[[nodiscard]] std::vector<std::byte> read_hex(const fs::path &path)
{
	std::ifstream input(path);
	std::string text{
		std::istreambuf_iterator<char>{input},
		std::istreambuf_iterator<char>{},
	};
	std::vector<std::byte> bytes;
	for(std::size_t index = 0U; index + 1U < text.size(); index += 2U) {
		auto digit = [](char value) -> std::uint8_t {
			if(value >= '0' && value <= '9') {
				return static_cast<std::uint8_t>(value - '0');
			}
			return static_cast<std::uint8_t>(value - 'A' + 10);
		};
		if(text[index] == '\n' || text[index] == '\r') {
			break;
		}
		bytes.push_back(static_cast<std::byte>(
			(digit(text[index]) << 4U) | digit(text[index + 1U])
		));
	}
	return bytes;
}

void test_identity_and_provenance_contract()
{
	using namespace th03::app;
	const auto document = full_document();
	expect(validate_new_classic_online_cache(document).has_value(),
		"verified and curated records satisfy the shared domain contract");
	expect(!document.leaderboard_entries[1U].replay_id &&
		!document.leaderboard_entries[1U].verifier_id &&
		document.leaderboard_entries[1U].evidence_source.empty() &&
		document.leaderboard_entries[1U].curator_note.empty(),
		"curated records need no invented replay, verifier, or public evidence");

	auto legacy_with_replay = document;
	legacy_with_replay.leaderboard_entries[1U].replay_id =
		document.replay_records[0U].replay_id;
	auto invalid = validate_new_classic_online_cache(legacy_with_replay);
	expect(!invalid && invalid.error().code ==
		NewClassicOnlineErrorCode::provenance,
		"curated records reject a synthetic replay association");

	auto verified_with_evidence = document;
	verified_with_evidence.leaderboard_entries[0U].evidence_source = "client claim";
	invalid = validate_new_classic_online_cache(verified_with_evidence);
	expect(!invalid && invalid.error().code ==
		NewClassicOnlineErrorCode::provenance,
		"verified records cannot smuggle client evidence into verifier authority");

	auto mismatched_replay = document;
	mismatched_replay.replay_records[0U].result.score += 10U;
	invalid = validate_new_classic_online_cache(mismatched_replay);
	expect(!invalid && invalid.error().code ==
		NewClassicOnlineErrorCode::provenance,
		"cached replay metadata must agree with a linked leaderboard result");

	auto duplicate = document;
	duplicate.leaderboard_entries.push_back(duplicate.leaderboard_entries[0U]);
	invalid = validate_new_classic_online_cache(duplicate);
	expect(!invalid && invalid.error().code ==
		NewClassicOnlineErrorCode::duplicate_identity,
		"entry identities are unique within one cached snapshot");

	NewClassicOnlineCacheDocument malformed_empty{};
	malformed_empty.snapshot_id = identity<NewClassicOnlineSnapshotId>(0xA0U);
	invalid = validate_new_classic_online_cache(malformed_empty);
	expect(!invalid && invalid.error().code ==
		NewClassicOnlineErrorCode::document_shape,
		"empty offline state cannot retain a stale service snapshot identity");

	NewClassicOnlineCacheDocument fetched_empty{};
	fetched_empty.snapshot_id = identity<NewClassicOnlineSnapshotId>(0xB0U);
	fetched_empty.fetched_unix_seconds = 1786766100ULL;
	fetched_empty.board_category = replay_record().category;
	expect(validate_new_classic_online_cache(fetched_empty).has_value(),
		"an explicitly fetched empty board remains distinguishable from no cache");

	auto mismatched_board = document;
	mismatched_board.board_category->character = 5U;
	invalid = validate_new_classic_online_cache(mismatched_board);
	expect(!invalid && invalid.error().code ==
		NewClassicOnlineErrorCode::category,
		"every projected row must belong to the cache's bound board category");

	auto overall_board = document;
	overall_board.board_category->character =
		new_classic_online_overall_character;
	expect(validate_new_classic_online_cache(overall_board).has_value(),
		"an Overall board retains each projected row's actual character");
	auto wrong_rank_overall = overall_board;
	wrong_rank_overall.leaderboard_entries[0U].category.rank = 1U;
	invalid = validate_new_classic_online_cache(wrong_rank_overall);
	expect(!invalid && invalid.error().code ==
		NewClassicOnlineErrorCode::category,
		"Overall never admits a row from another difficulty");

	auto netplay_records = document;
	netplay_records.leaderboard_entries.erase(
		netplay_records.leaderboard_entries.begin() + 1
	);
	netplay_records.board_category->mode = NewClassicOnlineMode::vs_1p_2p;
	netplay_records.board_category->character =
		new_classic_online_overall_character;
	netplay_records.leaderboard_entries[0U].category.mode =
		NewClassicOnlineMode::vs_1p_2p;
	netplay_records.leaderboard_entries[0U].result = {
		NewClassicOnlineResultKind::versus_complete, 123456780ULL, 0U, 0U,
	};
	netplay_records.replay_records[0U].category.mode =
		NewClassicOnlineMode::vs_1p_2p;
	netplay_records.replay_records[0U].result =
		netplay_records.leaderboard_entries[0U].result;
	expect(validate_new_classic_online_cache(netplay_records).has_value(),
		"one rank-local aggregate document carries private netplay replay evidence");
	auto invalid_netplay = netplay_records;
	invalid_netplay.replay_records[0U].result.continues_used = 1U;
	invalid_netplay.leaderboard_entries[0U].result.continues_used = 1U;
	invalid = validate_new_classic_online_cache(invalid_netplay);
	expect(!invalid && invalid.error().code == NewClassicOnlineErrorCode::result,
		"netplay evidence cannot carry Story stage or Continue semantics");

	auto invalid_telemetry = document;
	invalid_telemetry.replay_records[0U].slow_frames = 501U;
	invalid_telemetry.replay_records[0U].timed_frames = 10'000U;
	invalid = validate_new_classic_online_cache(invalid_telemetry);
	expect(!invalid && invalid.error().code == NewClassicOnlineErrorCode::telemetry,
		"cached replay telemetry enforces the exact five-percent boundary");
}

void test_codec_round_trip_and_corruption()
{
	using namespace th03::app;
	const auto document = full_document();
	auto encoded = encode_new_classic_online_cache(document);
	expect(encoded && encoded->size() < new_classic_online_cache_max_bytes,
		"bounded online cache encodes below its hard file limit");
	if(!encoded) {
		return;
	}
	auto decoded = decode_new_classic_online_cache(*encoded);
	expect(decoded && *decoded == document,
		"vendor-neutral cache round-trips every identity and provenance field");

	auto corrupt = *encoded;
	corrupt[80U] ^= std::byte{1U};
	auto rejected = decode_new_classic_online_cache(corrupt);
	expect(!rejected && rejected.error().code ==
		NewClassicOnlineErrorCode::checksum,
		"cache corruption is rejected before any record can be exposed");

	auto malformed = document;
	malformed.leaderboard_entries[0U].player = std::string("bad\nname");
	expect(!encode_new_classic_online_cache(malformed) &&
		validate_new_classic_online_cache(malformed).error().code ==
			NewClassicOnlineErrorCode::text,
		"display text rejects control characters before persistence");

	const auto filename = new_classic_online_replay_filename(
		document.replay_records[0U].replay_id
	);
	expect(filename.size() == 68U && filename.ends_with(".RPY") &&
		filename.starts_with("20212223"),
		"downloaded replay payloads use a stable content-addressed local name");
}

void test_empty_and_atomic_store()
{
	using namespace th03::app;
	TemporaryDirectory temporary;
	const auto path = temporary.path() / "PODD.ONL";
	auto missing = load_new_classic_online_store(path);
	expect(missing && missing->disposition ==
		NewClassicOnlineStoreDisposition::empty &&
		missing->document.generation == 0U && !fs::exists(path),
		"missing cache is a write-free normal offline state");

	auto document = full_document();
	expect(commit_new_classic_online_store(path, document).has_value() &&
		document.generation == 8U && fs::is_regular_file(path),
		"cache commit validates and atomically publishes the next generation");
	auto loaded = load_new_classic_online_store(path);
	expect(loaded && loaded->disposition ==
		NewClassicOnlineStoreDisposition::loaded && loaded->document == document,
		"canonical cache reloads without mutating local gameplay persistence");
}

void test_recovery_and_fail_closed_corruption()
{
	using namespace th03::app;
	TemporaryDirectory temporary;
	const auto path = temporary.path() / "PODD.ONL";
	auto old_document = full_document();
	old_document.generation = 12U;
	auto old_bytes = encode_new_classic_online_cache(old_document);
	expect(old_bytes.has_value(), "recovery candidate encodes");
	if(!old_bytes) {
		return;
	}
	auto old_path = path;
	old_path += ".old";
	write_bytes(old_path, *old_bytes);
	std::vector<std::byte> corrupt(80U, std::byte{0xFFU});
	write_bytes(path, corrupt);
	auto recovered = load_new_classic_online_store(path);
	expect(recovered && recovered->disposition ==
		NewClassicOnlineStoreDisposition::recovered_candidate &&
		recovered->document == old_document && fs::exists(path) &&
		!fs::exists(old_path),
		"valid .old cache recovers over a corrupt canonical file");

	write_bytes(path, corrupt);
	auto failed = load_new_classic_online_store(path);
	expect(!failed && failed.error().code ==
		NewClassicOnlineStoreErrorCode::no_valid_candidate,
		"a wholly corrupt cache fails closed rather than becoming online data");
}

void test_worker_projection_fixture(const fs::path &fixture)
{
	using namespace th03::app;
	const auto bytes = read_hex(fixture);
	auto decoded = decode_new_classic_online_cache(bytes);
	expect(bytes.size() == 629U && decoded.has_value(),
		"the Worker T3ONL002 golden vector decodes in the native client");
	if(!decoded) {
		return;
	}
	expect(decoded->board_category &&
		decoded->board_category->mode == NewClassicOnlineMode::story &&
		decoded->board_category->rank == 1U &&
		decoded->board_category->character == 0U &&
		decoded->leaderboard_entries.size() == 2U &&
		decoded->replay_records.size() == 1U,
		"the projected cache binds one exact category and its ordered rows");
	expect(decoded->leaderboard_entries[0U].provenance ==
		NewClassicOnlineProvenance::curated_legacy &&
		!decoded->leaderboard_entries[0U].replay_id &&
		!decoded->leaderboard_entries[0U].autofire &&
		decoded->leaderboard_entries[0U].evidence_source.empty() &&
		decoded->leaderboard_entries[1U].provenance ==
			NewClassicOnlineProvenance::replay_verified &&
		decoded->leaderboard_entries[1U].autofire == true,
		"historical and replay-backed rows retain authority and explicit Autofire state");
	const auto &replay = decoded->replay_records[0U];
	expect(replay.availability ==
		NewClassicOnlineReplayAvailability::metadata_only &&
		replay.autofire == 1U && replay.timed_frames == 10'000U &&
		replay.slow_frames == 500U && replay.payload_bytes == 7U,
		"online replay metadata carries visible Autofire and exact slowdown facts");
	auto encoded = encode_new_classic_online_cache(*decoded);
	expect(encoded && *encoded == bytes,
		"native re-encoding is byte-identical to the Worker golden vector");
}

} // namespace

int main(int argc, char **argv)
{
	test_identity_and_provenance_contract();
	test_codec_round_trip_and_corruption();
	test_empty_and_atomic_store();
	test_recovery_and_fail_closed_corruption();
	expect(argc == 2, "the cross-language projection fixture is supplied");
	if(argc == 2) {
		test_worker_projection_fixture(argv[1]);
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
