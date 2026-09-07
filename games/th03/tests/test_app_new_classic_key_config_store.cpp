#include "th03/app/new_classic/input/new_classic_key_config_store.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

[[nodiscard]] th03::app::NewClassicKeyConfig fixture()
{
	th03::app::NewClassicKeyConfig result{.autofire = 2U};
	for(std::size_t index = 0U; index < result.bindings.size(); ++index) {
		result.bindings[index] = static_cast<std::uint16_t>(index + 1U);
	}
	return result;
}

struct TemporaryDirectory final {
	std::filesystem::path path = std::filesystem::temp_directory_path() /
		("th03-new-classic-keys-" + std::to_string(
			static_cast<unsigned long long>(
				std::filesystem::file_time_type::clock::now().time_since_epoch().count()
			)
		));

	TemporaryDirectory()
	{
		std::filesystem::create_directories(path);
	}
	~TemporaryDirectory()
	{
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
};

} // namespace

int main()
{
	using namespace th03::app;
	constexpr std::uint16_t maximum_key = 512U;
	const auto config = fixture();
	auto encoded = encode_new_classic_key_config(config, maximum_key);
	expect(encoded.has_value(), "valid New Classic keys encode");
	expect(encoded && encoded->size() == new_classic_key_config_file_bytes,
		"key file has frozen bounded size");
	auto decoded = encoded ? decode_new_classic_key_config(*encoded, maximum_key) :
		std::expected<NewClassicKeyConfig, NewClassicKeyConfigCodecError>{
			std::unexpected(NewClassicKeyConfigCodecError{})
		};
	expect(decoded && *decoded == config, "key file round-trips exactly");

	if(encoded) {
		auto corrupted = *encoded;
		corrupted[20U] ^= std::byte{1U};
		auto rejected = decode_new_classic_key_config(corrupted, maximum_key);
		expect(!rejected && rejected.error().code ==
			NewClassicKeyConfigCodecErrorCode::digest,
			"digest rejects a modified binding");
		expect(!decode_new_classic_key_config(
			std::span<const std::byte>(corrupted).first(corrupted.size() - 1U),
			maximum_key
		), "truncated key file is rejected");
	}

	auto invalid = config;
	invalid.autofire = 4U;
	expect(!encode_new_classic_key_config(invalid, maximum_key),
		"invalid Autofire is rejected");
	invalid = config;
	invalid.bindings[1U] = invalid.bindings[0U];
	expect(!encode_new_classic_key_config(invalid, maximum_key),
		"duplicate player binding is rejected");
	invalid = config;
	invalid.bindings.back() = invalid.bindings[
		2U * new_classic_player_action_count
	];
	expect(!encode_new_classic_key_config(invalid, maximum_key),
		"duplicate Story binding is rejected");
	invalid = config;
	invalid.bindings[0U] = new_classic_host_key_unbound;
	expect(encode_new_classic_key_config(invalid, maximum_key).has_value(),
		"explicit unbound action is valid");
	invalid = config;
	invalid.bindings[0U] = maximum_key;
	expect(!encode_new_classic_key_config(invalid, maximum_key),
		"out-of-range host key is rejected");

	TemporaryDirectory temporary;
	const auto path = temporary.path / "TH3KEY.CFG";
	auto absent = load_new_classic_key_config(path, config, maximum_key);
	expect(absent && !absent->file_present && absent->config == config,
		"absent key file returns caller defaults");
	auto migrated = load_new_classic_key_config(
		path, config, maximum_key, true
	);
	expect(migrated && !migrated->file_present &&
		migrated->config.autofire == 3U,
		"absent key file migrates legacy Autofire to both players");
	expect(store_new_classic_key_config(path, config, maximum_key).has_value(),
		"key file stores atomically");
	auto loaded = load_new_classic_key_config(path, config, maximum_key);
	expect(loaded && loaded->file_present && !loaded->recovered_defaults &&
		loaded->config == config, "stored key file reads back");

	if(encoded) {
		auto corrupted = *encoded;
		corrupted[0U] = std::byte{'X'};
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		output.write(reinterpret_cast<const char *>(corrupted.data()),
			static_cast<std::streamsize>(corrupted.size()));
		output.close();
		auto recovered = load_new_classic_key_config(path, config, maximum_key);
		expect(recovered && recovered->file_present &&
			recovered->recovered_defaults && recovered->config == config,
			"corrupt key file recovers bounded defaults");
		auto recovered_legacy = load_new_classic_key_config(
			path, config, maximum_key, true
		);
		expect(recovered_legacy && recovered_legacy->file_present &&
			recovered_legacy->recovered_defaults &&
			recovered_legacy->config == config,
			"corrupt present key file does not apply legacy migration");
	}

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
