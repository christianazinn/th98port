#include "th03/app/classic_config.hpp"
#include "th03/app/classic_config_store.hpp"
#include "th03/app/new_classic_settings.hpp"
#include "th03/app/new_classic_settings_store.hpp"

#include <array>
#include <chrono>
#include <cstddef>
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
using th03::app::ClassicConfigOptions;
using th03::app::ClassicKeyMode;
using th03::app::ClassicLanguage;
using th03::app::ClassicRank;
using th03::app::NewClassicSettings;
using th03::app::NewClassicSettingsErrorCode;
using th03::app::NewClassicSettingsStoreDisposition;
using th03::app::NewClassicSettingsStoreErrorCode;
using th03::app::NewClassicWindowMode;
using th03::app::NewClassicWindowScale;
using th03::app::new_classic_settings_file_bytes;

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
			("th03-new-classic-settings-" + std::to_string(suffix));
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
	[[nodiscard]] bool valid() const noexcept { return valid_; }
	[[nodiscard]] const fs::path &path() const noexcept { return path_; }

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

void test_codec()
{
	const NewClassicSettings settings{
		.options = {
			.bgm_mode = ClassicBgmMode::off,
			.key_mode = ClassicKeyMode::key_joy,
			.rank = ClassicRank::lunatic,
			.sfx_enabled = false,
			.legacy_autofire = true,
			.language = ClassicLanguage::english,
			.reduced_flash = true,
		},
		.master_volume_percent = 65U,
		.window = {
			.mode = NewClassicWindowMode::borderless_fullscreen,
			.scale = NewClassicWindowScale::x3,
		},
	};
	const auto encoded = th03::app::encode_new_classic_settings(settings);
	const auto decoded = encoded ?
		th03::app::decode_new_classic_settings(*encoded) :
		std::expected<NewClassicSettings, th03::app::NewClassicSettingsError>{
			std::unexpected(th03::app::NewClassicSettingsError{})
		};
	expect(encoded && decoded && *decoded == settings,
		"versioned New Classic settings round-trip every field");
	expect(encoded && encoded->size() == new_classic_settings_file_bytes,
		"New Classic settings have one frozen v1 size");

	if(encoded) {
		for(const auto mutation : std::array{
			std::pair{0U, NewClassicSettingsErrorCode::magic},
			std::pair{8U, NewClassicSettingsErrorCode::version},
			std::pair{10U, NewClassicSettingsErrorCode::declared_size},
			std::pair{20U, NewClassicSettingsErrorCode::digest},
		}) {
			auto bytes = *encoded;
			bytes[mutation.first] ^= std::byte{0xFFU};
			const auto invalid = th03::app::decode_new_classic_settings(bytes);
			expect(!invalid && invalid.error().code == mutation.second,
				"header and digest mutations report their exact domain");
		}
		auto reserved = *encoded;
		reserved[19U] = std::byte{1U};
		const auto invalid_reserved =
			th03::app::decode_new_classic_settings(reserved);
		expect(!invalid_reserved && invalid_reserved.error().code ==
			NewClassicSettingsErrorCode::reserved_nonzero,
			"nonzero v1 reserved bytes are rejected");
	}

	auto invalid = settings;
	invalid.master_volume_percent = 63U;
	const auto invalid_volume =
		th03::app::encode_new_classic_settings(invalid);
	expect(!invalid_volume && invalid_volume.error().code ==
		NewClassicSettingsErrorCode::invalid_master_volume,
		"master percentage is bounded to five-percent steps");
	invalid = settings;
	invalid.window.mode = static_cast<NewClassicWindowMode>(2U);
	const auto invalid_mode = th03::app::encode_new_classic_settings(invalid);
	expect(!invalid_mode && invalid_mode.error().code ==
		NewClassicSettingsErrorCode::invalid_window_mode,
		"window mode rejects values outside the frozen ring");
	invalid = settings;
	invalid.window.scale = static_cast<NewClassicWindowScale>(4U);
	const auto invalid_scale = th03::app::encode_new_classic_settings(invalid);
	expect(!invalid_scale && invalid_scale.error().code ==
		NewClassicSettingsErrorCode::invalid_window_scale,
		"window scale rejects values outside 1x through 3x and Auto");
}

void test_absent_only_migration_and_isolation()
{
	TemporaryDirectory temporary;
	expect(temporary.valid(), "migration temporary directory creates");
	if(!temporary.valid()) {
		return;
	}
	const auto classic_path = temporary.path() / "YUME.CFG";
	const auto settings_path = temporary.path() / "PODD.CFG";
	const ClassicConfigOptions classic{
		.bgm_mode = ClassicBgmMode::off,
		.key_mode = ClassicKeyMode::joy_key,
		.rank = ClassicRank::hard,
		.sfx_enabled = false,
		.legacy_autofire = true,
		.language = ClassicLanguage::english,
		.reduced_flash = true,
	};
	expect(th03::app::commit_classic_config_store(
		classic_path, classic
	).has_value(), "Classic migration fixture commits");
	const auto classic_before = read_bytes(classic_path);
	const auto migrated = th03::app::load_new_classic_settings_store(
		settings_path, classic_path
	);
	expect(migrated && migrated->disposition ==
		NewClassicSettingsStoreDisposition::migrated_classic &&
		migrated->settings.options == classic &&
		migrated->settings.master_volume_percent == 100U &&
		migrated->settings.window ==
			th03::app::NewClassicWindowPresentation{},
		"absent New Classic settings migrate Classic values once");
	expect(read_bytes(classic_path) == classic_before,
		"migration never rewrites Classic YUME.CFG");

	auto changed = migrated ? migrated->settings : NewClassicSettings{};
	changed.options.rank = ClassicRank::easy;
	changed.master_volume_percent = 35U;
	expect(th03::app::commit_new_classic_settings_store(
		settings_path, changed
	).has_value(), "New Classic settings commit independently");
	expect(read_bytes(classic_path) == classic_before,
		"New Classic mutation leaves Classic bytes unchanged");

	const ClassicConfigOptions later_classic{
		.bgm_mode = ClassicBgmMode::fm,
		.key_mode = ClassicKeyMode::key_key,
		.rank = ClassicRank::lunatic,
	};
	expect(th03::app::commit_classic_config_store(
		classic_path, later_classic
	).has_value(), "Classic changes after migration");
	const auto loaded = th03::app::load_new_classic_settings_store(
		settings_path, classic_path
	);
	expect(loaded && loaded->disposition ==
		NewClassicSettingsStoreDisposition::loaded &&
		loaded->settings == changed,
		"present New Classic settings never remigrate changed Classic values");
}

void test_defaults_recovery_and_errors()
{
	TemporaryDirectory temporary;
	expect(temporary.valid(), "recovery temporary directory creates");
	if(!temporary.valid()) {
		return;
	}
	const auto classic_path = temporary.path() / "YUME.CFG";
	const auto settings_path = temporary.path() / "PODD.CFG";
	const auto created = th03::app::load_new_classic_settings_store(
		settings_path, classic_path
	);
	expect(created && created->disposition ==
		NewClassicSettingsStoreDisposition::created_defaults,
		"missing files create New Classic defaults");

	const std::array corrupt{std::byte{'x'}, std::byte{'y'}};
	expect(write_bytes(settings_path, corrupt), "corrupt settings fixture writes");
	const auto recovered = th03::app::load_new_classic_settings_store(
		settings_path, classic_path
	);
	expect(recovered && recovered->disposition ==
		NewClassicSettingsStoreDisposition::recovered_defaults &&
		recovered->recovered_from && recovered->recovered_from->code ==
			NewClassicSettingsErrorCode::input_size,
		"present malformed settings recover defaults without migration");
	expect(fs::file_size(settings_path) == new_classic_settings_file_bytes,
		"recovery publishes the exact v1 document");

	const auto malformed_classic = temporary.path() / "BROKEN.CFG";
	const auto migrated_defaults = temporary.path() / "DEFAULT.CFG";
	expect(write_bytes(malformed_classic, corrupt),
		"malformed Classic migration fixture writes");
	const auto malformed_before = read_bytes(malformed_classic);
	const auto defaulted = th03::app::load_new_classic_settings_store(
		migrated_defaults, malformed_classic
	);
	expect(defaulted && defaulted->disposition ==
		NewClassicSettingsStoreDisposition::created_defaults &&
		defaulted->settings == th03::app::new_classic_settings_defaults(),
		"malformed Classic migration input creates New Classic defaults");
	expect(read_bytes(malformed_classic) == malformed_before,
		"malformed Classic migration input remains byte-identical");

	expect(!th03::app::load_new_classic_settings_store({}, classic_path) &&
		!th03::app::commit_new_classic_settings_store(
			{}, NewClassicSettings{}
		), "empty New Classic settings paths are rejected");
	const auto directory = temporary.path() / "directory.cfg";
	std::error_code system;
	fs::create_directory(directory, system);
	const auto invalid_source = th03::app::load_new_classic_settings_store(
		directory, classic_path
	);
	expect(!invalid_source && invalid_source.error().code ==
		NewClassicSettingsStoreErrorCode::source_not_regular,
		"directory settings source is rejected without replacement");
}

} // namespace

int main()
{
	test_codec();
	test_absent_only_migration_and_isolation();
	test_defaults_recovery_and_errors();
	if(failures != 0U) {
		std::cerr << "New Classic settings checks: " << checks
			<< ", failures: " << failures << '\n';
		return 1;
	}
	std::cout << "New Classic settings checks: " << checks << '\n';
	return 0;
}
