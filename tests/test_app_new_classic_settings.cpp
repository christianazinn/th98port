#include "th03/app/classic_config.hpp"
#include "th03/app/classic_config_store.hpp"
#include "th03/app/new_classic_settings.hpp"
#include "th03/app/new_classic_settings_store.hpp"

#include <algorithm>
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
using th03::app::NewClassicAspectRatio;
using th03::app::NewClassicFiltering;
using th03::app::NewClassicNetplayConnection;
using th03::app::NewClassicWindowMode;
using th03::app::NewClassicWindowScale;
using th03::app::new_classic_settings_file_bytes;
using th03::app::new_classic_settings_v1_file_bytes;
using th03::app::new_classic_settings_v2_v3_file_bytes;

std::size_t checks = 0U;
std::size_t failures = 0U;

[[nodiscard]] std::uint32_t fnv1a32(std::span<const std::byte> bytes)
{
	std::uint32_t hash = 2'166'136'261U;
	for(const auto value : bytes) {
		hash ^= std::to_integer<std::uint8_t>(value);
		hash *= 16'777'619U;
	}
	return hash;
}

void put_u16(std::span<std::byte> bytes, std::size_t offset, std::uint16_t value)
{
	bytes[offset] = static_cast<std::byte>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void put_u32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value)
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
	}
}

[[nodiscard]] std::array<std::byte, new_classic_settings_v1_file_bytes>
v1_settings(bool music, bool effects)
{
	std::array<std::byte, new_classic_settings_v1_file_bytes> bytes{};
	constexpr std::array magic{
		std::byte{'T'}, std::byte{'3'}, std::byte{'N'}, std::byte{'S'},
		std::byte{'E'}, std::byte{'T'}, std::byte{'0'}, std::byte{'1'},
	};
	std::copy(magic.begin(), magic.end(), bytes.begin());
	put_u16(bytes, 8U, 1U);
	put_u16(bytes, 10U, new_classic_settings_v1_file_bytes);
	bytes[12U] = static_cast<std::byte>(static_cast<std::uint8_t>(
		music ? ClassicBgmMode::fm : ClassicBgmMode::off
	));
	bytes[15U] = static_cast<std::byte>(effects ? 0x01U : 0U);
	bytes[16U] = std::byte{75U};
	put_u32(bytes, 20U, fnv1a32(std::span<const std::byte>(bytes).first(20U)));
	return bytes;
}

[[nodiscard]] std::array<std::byte, new_classic_settings_v2_v3_file_bytes>
legacy_settings(const NewClassicSettings &settings, std::uint8_t version)
{
	const auto current = th03::app::encode_new_classic_settings(settings).value();
	std::array<std::byte, new_classic_settings_v2_v3_file_bytes> encoded{};
	std::copy_n(current.begin(), 23U, encoded.begin());
	encoded[15U] &= std::byte{0x0FU};
	encoded[7U] = static_cast<std::byte>('0' + version);
	put_u16(encoded, 8U, version);
	put_u16(encoded, 10U, new_classic_settings_v2_v3_file_bytes);
	if(version == 2U) {
		encoded[22U] = std::byte{0U};
	}
	put_u32(encoded, 24U, fnv1a32(
		std::span<const std::byte>(encoded).first(24U)
	));
	return encoded;
}

[[nodiscard]] std::array<std::byte, new_classic_settings_file_bytes>
v4_settings(NewClassicSettings settings)
{
	settings.netplay.input_delay_frames = 0U;
	auto encoded = th03::app::encode_new_classic_settings(settings).value();
	encoded[15U] &= std::byte{0x0FU};
	encoded[7U] = std::byte{'4'};
	put_u16(encoded, 8U, 4U);
	encoded[27U] = std::byte{0U};
	put_u32(encoded, 28U, fnv1a32(
		std::span<const std::byte>(encoded).first(28U)
	));
	return encoded;
}

[[nodiscard]] std::array<std::byte, new_classic_settings_file_bytes>
v5_settings(NewClassicSettings settings)
{
	auto encoded = th03::app::encode_new_classic_settings(settings).value();
	encoded[7U] = std::byte{'5'};
	put_u16(encoded, 8U, 5U);
	encoded[15U] &= std::byte{0x0FU};
	put_u32(encoded, 28U, fnv1a32(
		std::span<const std::byte>(encoded).first(28U)
	));
	return encoded;
}

[[nodiscard]] std::array<std::byte, new_classic_settings_file_bytes>
v6_settings(NewClassicSettings settings)
{
	auto encoded = th03::app::encode_new_classic_settings(settings).value();
	encoded[7U] = std::byte{'6'};
	put_u16(encoded, 8U, 6U);
	encoded[15U] &= std::byte{0x1FU};
	put_u32(encoded, 28U, fnv1a32(
		std::span<const std::byte>(encoded).first(28U)
	));
	return encoded;
}

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
		.gameplay_ruleset = th03::dopamine_test_v1_ruleset,
		.master_volume_percent = 65U,
		.music_volume_percent = 35U,
		.effect_volume_percent = 80U,
		.mute_when_unfocused = true,
		.automatic_updates = false,
		.window = {
			.mode = NewClassicWindowMode::borderless_fullscreen,
			.scale = NewClassicWindowScale::x3,
			.aspect = NewClassicAspectRatio::display_4_3,
			.filtering = NewClassicFiltering::linear,
		},
		.netplay = {
			.connection = NewClassicNetplayConnection::direct_udp,
			.relay_only = true,
			.direct_udp_port = 42'424U,
			.input_delay_frames = 5U,
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
		"New Classic settings retain the frozen 32-byte size in v7");

	if(encoded) {
		for(const auto mutation : std::array{
			std::pair{0U, NewClassicSettingsErrorCode::magic},
			std::pair{8U, NewClassicSettingsErrorCode::version},
			std::pair{10U, NewClassicSettingsErrorCode::declared_size},
			std::pair{28U, NewClassicSettingsErrorCode::digest},
		}) {
			auto bytes = *encoded;
			bytes[mutation.first] ^= std::byte{0xFFU};
			const auto invalid = th03::app::decode_new_classic_settings(bytes);
			expect(!invalid && invalid.error().code == mutation.second,
				"header and digest mutations report their exact domain");
		}
		auto reserved = *encoded;
		reserved[19U] |= std::byte{0x10U};
		const auto invalid_reserved =
			th03::app::decode_new_classic_settings(reserved);
		expect(!invalid_reserved && invalid_reserved.error().code ==
			NewClassicSettingsErrorCode::reserved_nonzero,
			"nonzero v2 reserved bytes are rejected");
		auto reserved_tail = *encoded;
		reserved_tail[22U] = std::byte{2U};
		const auto invalid_tail =
			th03::app::decode_new_classic_settings(reserved_tail);
		expect(!invalid_tail && invalid_tail.error().code ==
			NewClassicSettingsErrorCode::reserved_nonzero,
			"out-of-range v3 focus policy values are rejected");
		const auto legacy_v2 = legacy_settings(settings, 2U);
		const auto decoded_v2 =
			th03::app::decode_new_classic_settings(legacy_v2);
		expect(decoded_v2 && !decoded_v2->mute_when_unfocused &&
			decoded_v2->music_volume_percent == settings.music_volume_percent &&
			decoded_v2->effect_volume_percent == settings.effect_volume_percent &&
			decoded_v2->netplay == th03::app::NewClassicNetplayPreferences{},
			"v2 documents preserve audio percentages and default focus mute Off");
		const auto legacy_v3 = legacy_settings(settings, 3U);
		const auto decoded_v3 =
			th03::app::decode_new_classic_settings(legacy_v3);
		expect(decoded_v3 && decoded_v3->mute_when_unfocused &&
			decoded_v3->netplay == th03::app::NewClassicNetplayPreferences{},
			"v3 documents preserve focus mute and default netplay preferences");
		const auto decoded_v6 = th03::app::decode_new_classic_settings(
			v6_settings(settings)
		);
		expect(decoded_v6 &&
			th03::stock_gameplay_rules(decoded_v6->gameplay_ruleset),
			"v6 documents default the persisted Arrange setting to Stock");
	}
	const auto legacy_on = v1_settings(true, true);
	const auto decoded_legacy_on =
		th03::app::decode_new_classic_settings(legacy_on);
	expect(decoded_legacy_on &&
		decoded_legacy_on->music_volume_percent == 100U &&
		decoded_legacy_on->effect_volume_percent == 100U &&
		decoded_legacy_on->automatic_updates,
		"v1 On flags migrate to full category levels");
	const auto legacy_off = v1_settings(false, false);
	const auto decoded_legacy_off =
		th03::app::decode_new_classic_settings(legacy_off);
	expect(decoded_legacy_off &&
		decoded_legacy_off->music_volume_percent == 0U &&
		decoded_legacy_off->effect_volume_percent == 0U,
		"v1 Off flags migrate to muted category levels");

	auto invalid = settings;
	invalid.gameplay_ruleset.id = static_cast<th03::GameplayRulesetId>(2U);
	const auto invalid_ruleset =
		th03::app::encode_new_classic_settings(invalid);
	expect(!invalid_ruleset && invalid_ruleset.error().code ==
		NewClassicSettingsErrorCode::invalid_gameplay_ruleset,
		"settings reject unsupported Arrange rule identities");
	invalid = settings;
	invalid.master_volume_percent = 63U;
	const auto invalid_volume =
		th03::app::encode_new_classic_settings(invalid);
	expect(!invalid_volume && invalid_volume.error().code ==
		NewClassicSettingsErrorCode::invalid_master_volume,
		"master percentage is bounded to five-percent steps");
	invalid = settings;
	invalid.music_volume_percent = 101U;
	const auto invalid_music = th03::app::encode_new_classic_settings(invalid);
	expect(!invalid_music && invalid_music.error().code ==
		NewClassicSettingsErrorCode::invalid_music_volume,
		"music percentage is bounded to five-percent steps");
	invalid = settings;
	invalid.effect_volume_percent = 42U;
	const auto invalid_effect = th03::app::encode_new_classic_settings(invalid);
	expect(!invalid_effect && invalid_effect.error().code ==
		NewClassicSettingsErrorCode::invalid_effect_volume,
		"effect percentage is bounded to five-percent steps");
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
	invalid = settings;
	invalid.window.aspect = static_cast<NewClassicAspectRatio>(2U);
	const auto invalid_aspect = th03::app::encode_new_classic_settings(invalid);
	expect(!invalid_aspect && invalid_aspect.error().code ==
		NewClassicSettingsErrorCode::invalid_aspect_ratio,
		"aspect ratio rejects values outside 8:5 Pixels and 4:3 Display");
	invalid = settings;
	invalid.window.filtering = static_cast<NewClassicFiltering>(2U);
	const auto invalid_filtering =
		th03::app::encode_new_classic_settings(invalid);
	expect(!invalid_filtering && invalid_filtering.error().code ==
		NewClassicSettingsErrorCode::invalid_filtering,
		"filtering rejects values outside Nearest and Linear");
	invalid = settings;
	invalid.netplay.connection = static_cast<NewClassicNetplayConnection>(2U);
	const auto invalid_connection =
		th03::app::encode_new_classic_settings(invalid);
	expect(!invalid_connection && invalid_connection.error().code ==
		NewClassicSettingsErrorCode::invalid_netplay_connection,
		"netplay connection rejects values outside Online and Direct UDP");
	invalid = settings;
	invalid.netplay.direct_udp_port = 0U;
	const auto invalid_port = th03::app::encode_new_classic_settings(invalid);
	expect(!invalid_port && invalid_port.error().code ==
		NewClassicSettingsErrorCode::invalid_netplay_port,
		"Direct UDP port rejects zero");
	invalid = settings;
	invalid.netplay.input_delay_frames = 7U;
	const auto invalid_delay = th03::app::encode_new_classic_settings(invalid);
	expect(!invalid_delay && invalid_delay.error().code ==
		NewClassicSettingsErrorCode::invalid_netplay_input_delay,
		"netplay input delay rejects values above six frames");
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
		migrated->settings.music_volume_percent == 0U &&
		migrated->settings.effect_volume_percent == 0U &&
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
		"recovery publishes the exact current document");

	const auto legacy_path = temporary.path() / "LEGACY.CFG";
	expect(write_bytes(legacy_path, v1_settings(true, false)),
		"v1 settings fixture writes");
	const auto upgraded = th03::app::load_new_classic_settings_store(
		legacy_path, classic_path
	);
	expect(upgraded && upgraded->disposition ==
		NewClassicSettingsStoreDisposition::upgraded_v1 &&
		upgraded->settings.music_volume_percent == 100U &&
		upgraded->settings.effect_volume_percent == 0U &&
		fs::file_size(legacy_path) == new_classic_settings_file_bytes,
		"v1 documents upgrade atomically to current category levels");

	const auto v2_path = temporary.path() / "V2.CFG";
	auto v2_fixture = NewClassicSettings{};
	v2_fixture.music_volume_percent = 45U;
	v2_fixture.effect_volume_percent = 70U;
	expect(write_bytes(v2_path, legacy_settings(v2_fixture, 2U)),
		"v2 settings fixture writes");
	const auto upgraded_v2 = th03::app::load_new_classic_settings_store(
		v2_path, classic_path
	);
	expect(upgraded_v2 && upgraded_v2->disposition ==
		NewClassicSettingsStoreDisposition::upgraded_v2 &&
		upgraded_v2->settings.music_volume_percent == 45U &&
		upgraded_v2->settings.effect_volume_percent == 70U &&
		!upgraded_v2->settings.mute_when_unfocused &&
		upgraded_v2->settings.automatic_updates &&
		read_bytes(v2_path)[7U] == std::byte{'7'},
		"v2 documents upgrade atomically with focus mute defaulted Off");

	const auto v3_path = temporary.path() / "V3.CFG";
	auto v3_fixture = NewClassicSettings{};
	v3_fixture.mute_when_unfocused = true;
	expect(write_bytes(v3_path, legacy_settings(v3_fixture, 3U)),
		"v3 settings fixture writes");
	const auto upgraded_v3 = th03::app::load_new_classic_settings_store(
		v3_path, classic_path
	);
	expect(upgraded_v3 && upgraded_v3->disposition ==
		NewClassicSettingsStoreDisposition::upgraded_v3 &&
		upgraded_v3->settings.mute_when_unfocused &&
		upgraded_v3->settings.netplay ==
			th03::app::NewClassicNetplayPreferences{} &&
		upgraded_v3->settings.automatic_updates &&
		read_bytes(v3_path)[7U] == std::byte{'7'},
		"v3 documents upgrade atomically with default netplay preferences");

	const auto v4_path = temporary.path() / "V4.CFG";
	auto v4_fixture = NewClassicSettings{};
	v4_fixture.netplay.connection = NewClassicNetplayConnection::direct_udp;
	v4_fixture.netplay.relay_only = true;
	v4_fixture.netplay.direct_udp_port = 42'424U;
	expect(write_bytes(v4_path, v4_settings(v4_fixture)),
		"v4 settings fixture writes");
	const auto upgraded_v4 = th03::app::load_new_classic_settings_store(
		v4_path, classic_path
	);
	expect(upgraded_v4 && upgraded_v4->disposition ==
		NewClassicSettingsStoreDisposition::upgraded_v4 &&
		upgraded_v4->settings.netplay.connection ==
			NewClassicNetplayConnection::direct_udp &&
		upgraded_v4->settings.netplay.relay_only &&
		upgraded_v4->settings.netplay.direct_udp_port == 42'424U &&
		upgraded_v4->settings.netplay.input_delay_frames == 0U &&
		upgraded_v4->settings.automatic_updates &&
		read_bytes(v4_path)[7U] == std::byte{'7'},
		"v4 documents upgrade atomically with zero input delay");

	const auto v5_path = temporary.path() / "V5.CFG";
	auto v5_fixture = NewClassicSettings{};
	v5_fixture.mute_when_unfocused = true;
	v5_fixture.netplay.input_delay_frames = 4U;
	expect(write_bytes(v5_path, v5_settings(v5_fixture)),
		"v5 settings fixture writes");
	const auto upgraded_v5 = th03::app::load_new_classic_settings_store(
		v5_path, classic_path
	);
	expect(upgraded_v5 && upgraded_v5->disposition ==
		NewClassicSettingsStoreDisposition::upgraded_v5 &&
		upgraded_v5->settings.mute_when_unfocused &&
		upgraded_v5->settings.netplay.input_delay_frames == 4U &&
		upgraded_v5->settings.automatic_updates &&
		read_bytes(v5_path)[7U] == std::byte{'7'},
		"v5 documents upgrade atomically with automatic updates enabled");

	const auto v6_path = temporary.path() / "V6.CFG";
	expect(write_bytes(v6_path, v6_settings(NewClassicSettings{})),
		"v6 settings fixture writes");
	const auto upgraded_v6 = th03::app::load_new_classic_settings_store(
		v6_path, classic_path
	);
	expect(upgraded_v6 && upgraded_v6->disposition ==
		NewClassicSettingsStoreDisposition::upgraded_v6 &&
		th03::stock_gameplay_rules(upgraded_v6->settings.gameplay_ruleset) &&
		read_bytes(v6_path)[7U] == std::byte{'7'},
		"v6 documents upgrade atomically with Arrange defaulted to Stock");

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
