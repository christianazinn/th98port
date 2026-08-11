#include "th03/app/classic_score_store.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;
using th03::app::ClassicScoreCleared;
using th03::app::ClassicScoreCodecFields;
using th03::app::ClassicScoreErrorCode;
using th03::app::ClassicScoreStoreDisposition;
using th03::app::ClassicScoreStoreErrorCode;
using th03::app::classic_score_file_bytes;
using th03::app::classic_score_rank_count;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		const auto suffix = std::chrono::steady_clock::now()
			.time_since_epoch().count();
		path_ = fs::temp_directory_path() /
			("th03-classic-score-store-" + std::to_string(suffix));
		std::error_code error;
		fs::create_directories(path_, error);
		valid_ = !error;
	}
	~TemporaryDirectory()
	{
		if(valid_) {
			std::error_code ignored;
			fs::remove_all(path_, ignored);
		}
	}
	[[nodiscard]] bool valid() const noexcept
	{
		return valid_;
	}
	[[nodiscard]] const fs::path &path() const noexcept
	{
		return path_;
	}

private:
	fs::path path_{};
	bool valid_ = false;
};

[[nodiscard]] std::array<
	ClassicScoreCodecFields,
	classic_score_rank_count
> codec_fields() noexcept
{
	return {{
		{.unknown = 0x11U, .key1 = 0x22U, .key2 = 0x33U},
		{.unknown = 0x44U, .key1 = 0x55U, .key2 = 0x66U},
		{.unknown = 0x77U, .key1 = 0x88U, .key2 = 0x99U},
		{.unknown = 0xAAU, .key1 = 0xBBU, .key2 = 0xCCU},
	}};
}

[[nodiscard]] bool write_bytes(
	const fs::path &path,
	std::span<const std::byte> bytes
)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return false;
	}
	output.write(
		reinterpret_cast<const char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	return static_cast<bool>(output);
}

[[nodiscard]] std::vector<std::byte> read_bytes(const fs::path &path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if(!input) {
		return {};
	}
	const auto end = input.tellg();
	if(end < 0) {
		return {};
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(end));
	input.seekg(0, std::ios::beg);
	if(!bytes.empty()) {
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	return input ? bytes : std::vector<std::byte>{};
}

[[nodiscard]] std::size_t temporary_count(
	const fs::path &directory,
	const fs::path &target
)
{
	std::size_t count = 0U;
	const auto prefix = target.filename().string() + ".tmp-";
	std::error_code error;
	for(fs::directory_iterator entry(directory, error), end;
		!error && entry != end;
		entry.increment(error)) {
		const auto filename = entry->path().filename().string();
		if(filename.starts_with(prefix)) {
			++count;
		}
	}
	return error ? static_cast<std::size_t>(-1) : count;
}

void test_create_load_and_commit()
{
	TemporaryDirectory temporary;
	expect(temporary.valid(), "temporary score-store directory is created");
	if(!temporary.valid()) {
		return;
	}
	const auto path = temporary.path() / "YUME.NEM";
	const auto fields = codec_fields();
	const auto created = th03::app::load_classic_score_store(path, fields);
	expect(created && created->disposition ==
		ClassicScoreStoreDisposition::created_stock,
		"missing score file creates all stock ranks");
	expect(created && !created->recovered_from,
		"missing creation is not reported as corruption");
	expect(fs::is_regular_file(path) && fs::file_size(path) ==
		classic_score_file_bytes,
		"created score file has the exact source size");
	expect(temporary_count(temporary.path(), path) == 0U,
		"successful creation leaves no transaction temporary");

	const auto loaded = th03::app::load_classic_score_store(path, {});
	expect(loaded && loaded->disposition ==
		ClassicScoreStoreDisposition::loaded,
		"existing valid file loads without rewrite");
	expect(created && loaded && loaded->document == created->document,
		"load preserves every board and codec field");

	if(!loaded) {
		return;
	}
	auto modified = loaded->document;
	modified.boards[2U].cleared = ClassicScoreCleared::cleared;
	const auto committed = th03::app::commit_classic_score_store(path, modified);
	expect(committed.has_value(), "modified whole document commits atomically");
	const auto reloaded = th03::app::load_classic_score_store(path, {});
	expect(reloaded && reloaded->document == modified,
		"committed document survives an independent load");
	expect(temporary_count(temporary.path(), path) == 0U,
		"successful replacement leaves no transaction temporary");
}

void test_source_compatible_recovery()
{
	TemporaryDirectory temporary;
	if(!temporary.valid()) {
		expect(false, "recovery temporary directory is created");
		return;
	}
	const auto path = temporary.path() / "YUME.NEM";
	const auto old_fields = codec_fields();
	const auto old_document = th03::app::classic_score_stock_file(old_fields);
	const auto old_bytes = th03::app::encode_classic_score_file(old_document);
	expect(old_bytes.has_value(), "valid pre-recovery fixture encodes");
	if(!old_bytes) {
		return;
	}
	auto corrupt = *old_bytes;
	corrupt[2U] ^= std::byte{0x01U};
	expect(write_bytes(path, corrupt), "checksum-corrupt fixture is written");

	auto recovery_fields = old_fields;
	recovery_fields[0U].unknown ^= 0xFFU;
	const auto recovered = th03::app::load_classic_score_store(
		path, recovery_fields
	);
	const auto expected_stock = th03::app::classic_score_stock_file(
		recovery_fields
	);
	expect(recovered && recovered->disposition ==
		ClassicScoreStoreDisposition::recovered_stock,
		"one malformed rank recreates the whole source document");
	expect(recovered && recovered->recovered_from &&
		recovered->recovered_from->code ==
			ClassicScoreErrorCode::checksum_mismatch &&
		recovered->recovered_from->rank == 0U,
		"recovery retains the load-bearing rank/checksum diagnostic");
	expect(recovered && recovered->document == expected_stock,
		"recovery does not retain any good rank from the old document");
	const auto disk = th03::app::load_classic_score_store(path, {});
	expect(disk && disk->document == expected_stock,
		"recovered all-rank stock is published to disk");

	std::array<std::byte, 17U> short_file{};
	expect(write_bytes(path, short_file), "short score fixture is written");
	const auto short_recovered = th03::app::load_classic_score_store(
		path, old_fields
	);
	expect(short_recovered && short_recovered->recovered_from &&
		short_recovered->recovered_from->code ==
			ClassicScoreErrorCode::input_size,
		"short existing file reports format size then recreates all ranks");
	expect(fs::file_size(path) == classic_score_file_bytes,
		"short recovery restores the exact whole-file size");
}

void test_failure_preserves_target()
{
	TemporaryDirectory temporary;
	if(!temporary.valid()) {
		expect(false, "failure temporary directory is created");
		return;
	}
	const auto path = temporary.path() / "YUME.NEM";
	const auto document = th03::app::classic_score_stock_file(codec_fields());
	const auto committed = th03::app::commit_classic_score_store(path, document);
	expect(committed.has_value(), "preservation baseline commits");
	const auto baseline = read_bytes(path);

	auto invalid = document;
	invalid.boards[1U].cleared = static_cast<ClassicScoreCleared>(0U);
	const auto rejected = th03::app::commit_classic_score_store(path, invalid);
	expect(!rejected && rejected.error().code ==
		ClassicScoreStoreErrorCode::codec && rejected.error().codec &&
		rejected.error().codec->code == ClassicScoreErrorCode::invalid_cleared,
		"invalid document retains exact codec diagnostics");
	expect(read_bytes(path) == baseline,
		"codec rejection does not alter the existing target");
	expect(temporary_count(temporary.path(), path) == 0U,
		"codec rejection creates no temporary");

	const auto absent_parent = temporary.path() / "absent" / "YUME.NEM";
	const auto create_failed = th03::app::commit_classic_score_store(
		absent_parent, document
	);
	expect(!create_failed && create_failed.error().code ==
		ClassicScoreStoreErrorCode::temporary_create,
		"missing parent is a diagnosed temporary-create failure");
	expect(!fs::exists(absent_parent),
		"temporary-create failure publishes no target");

	const auto directory_target = temporary.path() / "directory-target";
	std::error_code error;
	fs::create_directory(directory_target, error);
	const auto replace_failed = th03::app::commit_classic_score_store(
		directory_target, document
	);
	expect(!error && !replace_failed && replace_failed.error().code ==
		ClassicScoreStoreErrorCode::replace_target,
		"directory target deterministically rejects atomic replacement");
	expect(fs::is_directory(directory_target),
		"failed replacement preserves the prior target object");
	expect(temporary_count(temporary.path(), directory_target) == 0U,
		"failed replacement cleans its same-directory temporary");

	const auto directory_load = th03::app::load_classic_score_store(
		directory_target, codec_fields()
	);
	expect(!directory_load && directory_load.error().code ==
		ClassicScoreStoreErrorCode::source_not_regular,
		"non-regular source is not mistaken for corrupt score data");
	const auto empty = th03::app::commit_classic_score_store({}, document);
	expect(!empty && empty.error().code ==
		ClassicScoreStoreErrorCode::invalid_path,
		"empty target path is rejected before encoding or I/O");
}

void test_authentic_copy(const fs::path &source)
{
	const auto original = read_bytes(source);
	expect(original.size() == classic_score_file_bytes,
		"authenticated local score file has exact source size");
	const auto loaded = th03::app::load_classic_score_store(source, {});
	expect(loaded && loaded->disposition ==
		ClassicScoreStoreDisposition::loaded,
		"authenticated local score file loads without recovery");
	if(!loaded || original.size() != classic_score_file_bytes) {
		return;
	}
	TemporaryDirectory temporary;
	if(!temporary.valid()) {
		expect(false, "authenticated-copy temporary directory is created");
		return;
	}
	const auto copy = temporary.path() / "YUME.NEM";
	expect(write_bytes(copy, original),
		"authenticated bytes are copied away from the original installation");
	const auto committed = th03::app::commit_classic_score_store(
		copy, loaded->document
	);
	expect(committed.has_value(), "authenticated document commits to its copy");
	expect(read_bytes(copy) == original,
		"authenticated unchanged document remains byte-exact after replacement");
}

} // namespace

int main(int argc, char **argv)
{
	test_create_load_and_commit();
	test_source_compatible_recovery();
	test_failure_preserves_target();
	if(argc >= 2) {
		test_authentic_copy(fs::path(argv[1]));
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
