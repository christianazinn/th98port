#include "th03/app/new_classic_score_store.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

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
			("th03-new-score-test-" + std::to_string(++sequence));
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

template<std::size_t Size>
void write_bytes(const fs::path &path, const std::array<std::byte, Size> &bytes)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(
		reinterpret_cast<const char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
}

void test_create_commit_and_reload()
{
	using namespace th03::app;
	TemporaryDirectory temporary;
	const auto path = temporary.path() / "YUME.NEW.NEM";
	auto loaded = load_new_classic_score_store(path);
	expect(loaded && loaded->disposition ==
		NewClassicScoreStoreDisposition::created_empty &&
		loaded->document.generation == 1U &&
		fs::file_size(path) == new_classic_score_file_bytes,
		"missing expanded store creates one exact generation-1 document");
	if(!loaded) {
		return;
	}
	loaded->document.boards[2U][3U].stats.one_ccs = 9U;
	expect(commit_new_classic_score_store(path, loaded->document).has_value() &&
		loaded->document.generation == 2U,
		"commit atomically increments the caller's generation");
	auto reloaded = load_new_classic_score_store(path);
	expect(reloaded && reloaded->disposition ==
		NewClassicScoreStoreDisposition::loaded &&
		reloaded->document.generation == 2U &&
		reloaded->document.boards[2U][3U].stats.one_ccs == 9U,
		"canonical expanded store round-trips statistics");
}

void test_legacy_migration_preserves_source()
{
	using namespace th03::app;
	TemporaryDirectory temporary;
	const auto legacy_path = temporary.path() / "YUME.NEM";
	const auto path = temporary.path() / "YUME.NEW.NEM";
	auto legacy = classic_score_stock_file();
	legacy.boards[1U].rows[0U].playchar_optional = 5U;
	legacy.boards[1U].rows[0U].score[8U] =
		ClassicRegistrationGlyph::digit_8;
	auto legacy_bytes = encode_classic_score_file(legacy);
	expect(legacy_bytes.has_value(), "legacy migration fixture encodes");
	if(!legacy_bytes) {
		return;
	}
	write_bytes(legacy_path, *legacy_bytes);
	auto migrated = load_new_classic_score_store(path, legacy_path);
	expect(migrated && migrated->disposition ==
		NewClassicScoreStoreDisposition::migrated_classic &&
		migrated->document.boards[1U][4U].entries[0U].score[7U] == 8U,
		"valid Classic rows migrate into their character boards");
	std::ifstream input(legacy_path, std::ios::binary);
	auto after = std::array<std::byte, classic_score_file_bytes>{};
	input.read(reinterpret_cast<char *>(after.data()),
		static_cast<std::streamsize>(after.size()));
	expect(after == *legacy_bytes,
		"New Classic migration never mutates Classic YUME.NEM");
}

void test_candidate_selection_and_board_repair()
{
	using namespace th03::app;
	TemporaryDirectory temporary;
	const auto path = temporary.path() / "YUME.NEW.NEM";
	auto canonical = new_classic_score_empty();
	canonical.generation = 3U;
	auto old = canonical;
	old.generation = 4U;
	old.boards[0U][0U].stats.play_frames = 123U;
	auto canonical_bytes = encode_new_classic_score_file(canonical);
	auto old_bytes = encode_new_classic_score_file(old);
	expect(canonical_bytes && old_bytes, "candidate fixtures encode");
	if(!canonical_bytes || !old_bytes) {
		return;
	}
	write_bytes(path, *canonical_bytes);
	auto old_path = path;
	old_path += ".old";
	write_bytes(old_path, *old_bytes);
	auto recovered = load_new_classic_score_store(path);
	expect(recovered && recovered->disposition ==
		NewClassicScoreStoreDisposition::recovered_candidate &&
		recovered->document.generation == 5U &&
		recovered->document.boards[0U][0U].stats.play_frames == 123U,
		"newest valid .old candidate is promoted and generation-advanced");

	auto damaged = encode_new_classic_score_file(recovered->document);
	expect(damaged.has_value(), "repair fixture encodes");
	if(!damaged) {
		return;
	}
	constexpr std::size_t damaged_board = new_classic_score_header_bytes +
		(5U * new_classic_score_board_bytes);
	(*damaged)[damaged_board + 212U] ^= std::byte{1U};
	write_bytes(path, *damaged);
	auto repaired = load_new_classic_score_store(path);
	expect(repaired && repaired->disposition ==
		NewClassicScoreStoreDisposition::repaired_boards &&
		repaired->repaired_board_count == 1U &&
		repaired->document.generation == 6U &&
		!repaired->document.boards[0U][5U].entries[0U].valid &&
		repaired->document.boards[0U][0U].stats.play_frames == 123U,
		"load replaces only one corrupt board and commits the repair");
}

} // namespace

int main()
{
	test_create_commit_and_reload();
	test_legacy_migration_preserves_source();
	test_candidate_selection_and_board_repair();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
