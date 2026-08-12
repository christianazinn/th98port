#include "th03/app/classic_config.hpp"
#include "th03/app/classic_config_store.hpp"

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
using th03::app::ClassicBgmMode;
using th03::app::ClassicConfigDocument;
using th03::app::ClassicConfigErrorCode;
using th03::app::ClassicConfigMetadata;
using th03::app::ClassicConfigOptions;
using th03::app::ClassicConfigStoreDisposition;
using th03::app::ClassicConfigStoreErrorCode;
using th03::app::ClassicKeyMode;
using th03::app::ClassicRank;
using th03::app::classic_config_file_bytes;

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
			("th03-classic-config-" + std::to_string(suffix));
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
		if(entry->path().filename().string().starts_with(prefix)) {
			++count;
		}
	}
	return error ? static_cast<std::size_t>(-1) : count;
}

void test_codec_layout_and_domains()
{
	const std::array raw{
		std::byte{0x82U}, std::byte{1U}, std::byte{3U}, std::byte{1U},
		std::byte{0x81U}, std::byte{0x78U}, std::byte{0x56U},
		std::byte{0x9AU},
	};
	const auto decoded = th03::app::decode_classic_config_file(raw);
	expect(decoded && decoded->options == ClassicConfigOptions{
		.bgm_mode = ClassicBgmMode::midi,
		.key_mode = ClassicKeyMode::joy_key,
		.rank = ClassicRank::lunatic,
		.sfx_enabled = false,
		.legacy_autofire = true,
		.language = th03::app::ClassicLanguage::english,
		.reduced_flash = true,
	} && decoded->metadata == ClassicConfigMetadata{
		.resident_segment = 0x5678U,
		.debug = 0x9AU,
	}, "exact eight-byte layout decodes all Classic fields");
	const auto encoded = decoded ?
		th03::app::encode_classic_config_file(*decoded) :
		std::expected<
			std::array<std::byte, classic_config_file_bytes>,
			th03::app::ClassicConfigError
		>{std::unexpected(th03::app::ClassicConfigError{})};
	expect(encoded && *encoded == raw,
		"Replay Patch option bits and Classic metadata round-trip byte-exactly");

	constexpr std::array expected_defaults{
		std::byte{1U}, std::byte{0U}, std::byte{1U}, std::byte{0U},
		std::byte{0U}, std::byte{0U}, std::byte{0U}, std::byte{0U},
	};
	const auto defaults = th03::app::encode_classic_config_file(
		th03::app::classic_config_document(
			th03::app::classic_config_defaults()
		)
	);
	expect(defaults && *defaults == expected_defaults,
		"native defaults are FM, key-key, Normal with zero DOS metadata");

	for(std::uint8_t bgm = 0U; bgm <= 2U; ++bgm) {
		for(std::uint8_t key = 0U; key <= 2U; ++key) {
			for(std::uint8_t rank = 0U; rank <= 3U; ++rank) {
				const ClassicConfigDocument document{
					.options = {
						.bgm_mode = static_cast<ClassicBgmMode>(bgm),
						.key_mode = static_cast<ClassicKeyMode>(key),
						.rank = static_cast<ClassicRank>(rank),
					},
					.metadata = {
						.resident_segment = static_cast<std::uint16_t>(
							0x2000U + key
						),
						.debug = bgm,
					},
				};
				const auto bytes =
					th03::app::encode_classic_config_file(document);
				const auto round_trip = bytes ?
					th03::app::decode_classic_config_file(*bytes) :
					std::expected<
						ClassicConfigDocument,
						th03::app::ClassicConfigError
					>{std::unexpected(th03::app::ClassicConfigError{})};
				expect(round_trip && *round_trip == document,
					"every source option tuple round-trips");
			}
		}
	}

	for(const auto size : {0U, 7U, 9U}) {
		std::array<std::byte, 9U> bytes{};
		const auto result = th03::app::decode_classic_config_file(
			std::span<const std::byte>{bytes}.first(size)
		);
		expect(!result && result.error().code ==
			ClassicConfigErrorCode::input_size &&
			result.error().offset == size,
			"non-eight-byte config is rejected with its observed size");
	}

	for(const auto mutation : std::array{
		std::pair{0U, ClassicConfigErrorCode::invalid_bgm_mode},
		std::pair{1U, ClassicConfigErrorCode::invalid_key_mode},
		std::pair{2U, ClassicConfigErrorCode::invalid_rank},
		std::pair{3U, ClassicConfigErrorCode::invalid_autofire},
		std::pair{4U, ClassicConfigErrorCode::invalid_language},
	}) {
		auto bytes = expected_defaults;
		bytes[mutation.first] = std::byte{0xFFU};
		const auto result = th03::app::decode_classic_config_file(bytes);
		expect(!result && result.error().code == mutation.second &&
			result.error().offset == mutation.first,
			"invalid option byte reports its exact field offset");
	}

	auto invalid = th03::app::classic_config_document(
		th03::app::classic_config_defaults()
	);
	invalid.options.rank = static_cast<ClassicRank>(0xFFU);
	const auto invalid_encode =
		th03::app::encode_classic_config_file(invalid);
	expect(!invalid_encode && invalid_encode.error().code ==
		ClassicConfigErrorCode::invalid_rank &&
		invalid_encode.error().offset == 2U,
		"encode validates typed enum domains before publication");
}

void test_store_create_load_commit_and_recovery()
{
	TemporaryDirectory temporary;
	expect(temporary.valid(), "temporary config-store directory is created");
	if(!temporary.valid()) {
		return;
	}
	const auto path = temporary.path() / "YUME.CFG";
	const auto created = th03::app::load_classic_config_store(path);
	expect(created && created->disposition ==
		ClassicConfigStoreDisposition::created_defaults &&
		created->options == th03::app::classic_config_defaults() &&
		!created->recovered_from,
		"missing config creates source defaults without corruption report");
	expect(fs::is_regular_file(path) && fs::file_size(path) ==
		classic_config_file_bytes && temporary_count(temporary.path(), path) == 0U,
		"created config has exact size and leaves no temporary");

	constexpr ClassicConfigOptions changed{
		.bgm_mode = ClassicBgmMode::off,
		.key_mode = ClassicKeyMode::key_joy,
		.rank = ClassicRank::hard,
	};
	expect(th03::app::commit_classic_config_store(path, changed).has_value(),
		"changed options commit atomically");
	const auto loaded = th03::app::load_classic_config_store(path);
	expect(loaded && loaded->disposition ==
		ClassicConfigStoreDisposition::loaded &&
		loaded->options == changed,
		"committed options survive an independent load");
	const auto committed_bytes = read_bytes(path);
	const auto committed_document =
		th03::app::decode_classic_config_file(committed_bytes);
	expect(committed_document && committed_document->metadata ==
		ClassicConfigMetadata{},
		"native commit clears resident and debug metadata");

	const std::array metadata_bytes{
		std::byte{0x82U}, std::byte{1U}, std::byte{3U}, std::byte{1U},
		std::byte{0x81U}, std::byte{0xCCU}, std::byte{0xDDU},
		std::byte{0xEEU},
	};
	expect(write_bytes(path, metadata_bytes),
		"metadata-bearing Classic fixture is written");
	const auto metadata_loaded = th03::app::load_classic_config_store(path);
	expect(metadata_loaded && metadata_loaded->disposition ==
		ClassicConfigStoreDisposition::loaded &&
		metadata_loaded->options == ClassicConfigOptions{
			.bgm_mode = ClassicBgmMode::midi,
			.key_mode = ClassicKeyMode::joy_key,
			.rank = ClassicRank::lunatic,
			.sfx_enabled = false,
			.legacy_autofire = true,
			.language = th03::app::ClassicLanguage::english,
			.reduced_flash = true,
		}, "load accepts Replay Patch options and ignores DOS-only metadata");
	expect(read_bytes(path) == std::vector<std::byte>{
		metadata_bytes.begin(), metadata_bytes.end()
	}, "valid Classic load does not rewrite metadata bytes");

	const std::array short_file{std::byte{1U}, std::byte{0U}, std::byte{1U}};
	expect(write_bytes(path, short_file), "short corrupt fixture is written");
	const auto recovered_short = th03::app::load_classic_config_store(
		path, changed
	);
	expect(recovered_short && recovered_short->disposition ==
		ClassicConfigStoreDisposition::recovered_defaults &&
		recovered_short->options == changed && recovered_short->recovered_from &&
		recovered_short->recovered_from->code ==
			ClassicConfigErrorCode::input_size &&
		recovered_short->recovered_from->offset == short_file.size(),
		"wrong-size config recovers atomically to caller defaults");
	const auto recovered_short_bytes = read_bytes(path);
	const auto recovered_short_document =
		th03::app::decode_classic_config_file(recovered_short_bytes);
	expect(recovered_short_document &&
		recovered_short_document->options == changed &&
		recovered_short_document->metadata == ClassicConfigMetadata{},
		"wrong-size recovery publishes exact defaults with zero metadata");

	auto invalid_bytes = metadata_bytes;
	invalid_bytes[1U] = std::byte{3U};
	expect(write_bytes(path, invalid_bytes),
		"invalid key-mode fixture is written");
	const auto recovered_domain = th03::app::load_classic_config_store(path);
	expect(recovered_domain && recovered_domain->disposition ==
		ClassicConfigStoreDisposition::recovered_defaults &&
		recovered_domain->recovered_from &&
		recovered_domain->recovered_from->code ==
			ClassicConfigErrorCode::invalid_key_mode,
		"invalid option domain recovers with the precise codec error");
	expect(temporary_count(temporary.path(), path) == 0U,
		"all successful replacements remove transaction temporaries");
}

void test_store_error_boundaries()
{
	TemporaryDirectory temporary;
	expect(temporary.valid(), "error-boundary directory is created");
	if(!temporary.valid()) {
		return;
	}
	expect(!th03::app::load_classic_config_store({}) &&
		!th03::app::commit_classic_config_store(
			{}, th03::app::classic_config_defaults()
		), "empty paths are rejected");

	const auto directory_target = temporary.path() / "directory.cfg";
	std::error_code system;
	fs::create_directory(directory_target, system);
	const auto directory_load =
		th03::app::load_classic_config_store(directory_target);
	expect(!directory_load && directory_load.error().code ==
		ClassicConfigStoreErrorCode::source_not_regular,
		"directory source is rejected without replacement");

	const auto missing_parent = temporary.path() / "missing" / "YUME.CFG";
	const auto missing_parent_load =
		th03::app::load_classic_config_store(missing_parent);
	expect(!missing_parent_load && missing_parent_load.error().code ==
		ClassicConfigStoreErrorCode::temporary_create,
		"missing parent reports transaction creation failure");

	const auto path = temporary.path() / "preserved.cfg";
	const std::array sentinel{
		std::byte{1U}, std::byte{0U}, std::byte{1U}, std::byte{2U},
		std::byte{3U}, std::byte{4U}, std::byte{5U}, std::byte{6U},
	};
	expect(write_bytes(path, sentinel), "preservation sentinel is written");
	auto invalid = th03::app::classic_config_defaults();
	invalid.bgm_mode = static_cast<ClassicBgmMode>(0xFFU);
	const auto invalid_commit =
		th03::app::commit_classic_config_store(path, invalid);
	expect(!invalid_commit && invalid_commit.error().code ==
		ClassicConfigStoreErrorCode::codec &&
		invalid_commit.error().codec &&
		invalid_commit.error().codec->code ==
			ClassicConfigErrorCode::invalid_bgm_mode &&
		read_bytes(path) == std::vector<std::byte>{
			sentinel.begin(), sentinel.end()
		} && temporary_count(temporary.path(), path) == 0U,
		"invalid typed commit preserves target and creates no temporary");

	const auto invalid_defaults =
		th03::app::load_classic_config_store(path, invalid);
	expect(!invalid_defaults && invalid_defaults.error().code ==
		ClassicConfigStoreErrorCode::codec &&
		read_bytes(path) == std::vector<std::byte>{
			sentinel.begin(), sentinel.end()
		}, "invalid recovery defaults fail before source mutation");
}

void test_local_original(const fs::path &path)
{
	const auto bytes = read_bytes(path);
	expect(bytes.size() == classic_config_file_bytes,
		"local Classic config has exact source size");
	const auto decoded = th03::app::decode_classic_config_file(bytes);
	expect(decoded.has_value(),
		"local Classic config decodes with all option fields in range");
	if(!decoded) {
		return;
	}
	const auto encoded = th03::app::encode_classic_config_file(*decoded);
	expect(encoded && std::vector<std::byte>{
		encoded->begin(), encoded->end()
	} == bytes, "local Classic config round-trips byte-exactly");
	expect(decoded->metadata.resident_segment == 0U,
		"local clean-exit config carries no resident segment");
}

} // namespace

int main(int argc, char **argv)
{
	test_codec_layout_and_domains();
	test_store_create_load_commit_and_recovery();
	test_store_error_boundaries();
	if(argc == 2) {
		test_local_original(argv[1]);
	}
	if(failures != 0U) {
		std::cerr << "classic config checks: " << checks << ", failures: "
			<< failures << '\n';
		return 1;
	}
	std::cout << "classic config checks: " << checks << '\n';
	return 0;
}
