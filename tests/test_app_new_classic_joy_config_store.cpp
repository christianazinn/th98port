#include "th03/app/new_classic/input/new_classic_joy_config_store.hpp"

#include <cstddef>
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

[[nodiscard]] th03::app::NewClassicJoyConfig fixture()
{
	using enum th03::app::NewClassicJoyButton;
	return {
		.buttons = {south, east, west, back, start},
		.horizontal_axis = th03::app::NewClassicJoyAxis::right_x,
		.vertical_axis = th03::app::NewClassicJoyAxis::right_y,
		.horizontal_reversed = true,
		.dead_zone_percent = 65U,
	};
}

struct TemporaryDirectory final {
	std::filesystem::path path = std::filesystem::temp_directory_path() /
		("th03-new-classic-joy-" + std::to_string(
			static_cast<unsigned long long>(
				std::filesystem::file_time_type::clock::now().time_since_epoch().count()
			)
		));

	TemporaryDirectory() { std::filesystem::create_directories(path); }
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
	const auto config = fixture();
	auto encoded = encode_new_classic_joy_config(config);
	expect(encoded.has_value(), "valid controller config encodes");
	expect(encoded && encoded->size() == new_classic_joy_config_file_bytes,
		"controller file has frozen bounded size");
	auto decoded = encoded ? decode_new_classic_joy_config(*encoded) :
		std::expected<NewClassicJoyConfig, NewClassicJoyConfigCodecError>{
			std::unexpected(NewClassicJoyConfigCodecError{})
		};
	expect(decoded && *decoded == config, "controller config round-trips");

	if(encoded) {
		auto corrupted = *encoded;
		corrupted[12U] ^= std::byte{1U};
		expect(!decode_new_classic_joy_config(corrupted),
			"digest rejects changed controller config");
		auto reserved = *encoded;
		reserved[21U] = std::byte{1U};
		// Repair the digest by using the encoder is intentionally impossible for
		// nonzero reserved bytes; a changed raw file is rejected first by digest.
		expect(!decode_new_classic_joy_config(reserved),
			"reserved controller bytes cannot be accepted");
		expect(!decode_new_classic_joy_config(
			std::span<const std::byte>(*encoded).first(encoded->size() - 1U)
		), "truncated controller config is rejected");
	}

	auto invalid = config;
	invalid.buttons[1U] = invalid.buttons[0U];
	expect(!encode_new_classic_joy_config(invalid),
		"duplicate controller action button is rejected");
	invalid = config;
	invalid.buttons[0U] = static_cast<NewClassicJoyButton>(0x80U);
	expect(!encode_new_classic_joy_config(invalid),
		"unknown controller button is rejected");
	invalid = config;
	invalid.horizontal_axis = NewClassicJoyAxis::left_y;
	expect(!encode_new_classic_joy_config(invalid),
		"vertical axis cannot be assigned horizontally");
	invalid = config;
	invalid.dead_zone_percent = 52U;
	expect(!encode_new_classic_joy_config(invalid),
		"dead zone must use five-percent steps");
	invalid.dead_zone_percent = 0U;
	expect(!encode_new_classic_joy_config(invalid),
		"zero dead zone is outside the bounded policy");
	invalid = config;
	invalid.buttons[0U] = NewClassicJoyButton::unbound;
	expect(encode_new_classic_joy_config(invalid).has_value(),
		"explicit unbound controller action is valid");

	TemporaryDirectory temporary;
	const auto path = temporary.path / "TH3JOY.CFG";
	auto absent = load_new_classic_joy_config(path, config);
	expect(absent && !absent->file_present && absent->config == config,
		"absent controller file returns defaults");
	expect(store_new_classic_joy_config(path, config).has_value(),
		"controller file stores atomically");
	auto loaded = load_new_classic_joy_config(path, {});
	expect(loaded && loaded->file_present && !loaded->recovered_defaults &&
		loaded->config == config, "stored controller file reads back");

	if(encoded) {
		auto corrupted = *encoded;
		corrupted[0U] = std::byte{'X'};
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		output.write(reinterpret_cast<const char *>(corrupted.data()),
			static_cast<std::streamsize>(corrupted.size()));
		output.close();
		const NewClassicJoyConfig defaults{
			.buttons = {
				NewClassicJoyButton::south, NewClassicJoyButton::east,
				NewClassicJoyButton::west, NewClassicJoyButton::back,
				NewClassicJoyButton::start,
			},
		};
		auto recovered = load_new_classic_joy_config(path, defaults);
		expect(recovered && recovered->file_present &&
			recovered->recovered_defaults && recovered->config == defaults,
			"corrupt controller file recovers bounded defaults");
	}

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
