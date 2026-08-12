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
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/app/direct_vs_cli.hpp"
#include "th03/app/direct_vs_launch.hpp"

#include "direct_vs_launch_internal.hpp"

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	checks++;
	if(!condition) {
		failures++;
		std::cerr << "FAIL: " << message << '\n';
	}
}

std::expected<th03::app::DirectVsCliRequest, th03::app::DirectVsCliError>
parse(std::initializer_list<std::string_view> arguments)
{
	return th03::app::parse_direct_vs_cli(
		std::span<const std::string_view>(arguments.begin(), arguments.size())
	);
}

std::vector<std::string_view> valid_arguments()
{
	return {
		"--vs",
		"--data", "data-root",
		"--main", "MAIN.EXE",
		"--font", "FONT.ROM",
	};
}

void test_help_and_valid_defaults()
{
	const auto help = parse({"--help"});
	expect(help && help->show_help,
		"help is a no-I/O parse result");
	const auto arguments = valid_arguments();
	const auto parsed = th03::app::parse_direct_vs_cli(arguments);
	expect(parsed && !parsed->show_help &&
		parsed->launch.process.player_one.character ==
			th03::app::MainCharacter::reimu &&
		parsed->launch.process.player_one.palette ==
			th03::app::MainPaletteVariant::first &&
		parsed->launch.process.player_two.character ==
			th03::app::MainCharacter::reimu &&
		parsed->launch.process.player_two.palette ==
			th03::app::MainPaletteVariant::second &&
		parsed->launch.process.rank == 1U &&
		parsed->launch.process.seed == 0U &&
		parsed->launch.process.game_options.input_protocol ==
			th03::InputProtocol::live &&
		parsed->tick_rate.ticks == 526'315U &&
		parsed->tick_rate.seconds == 9'328U,
		"valid request keeps source direct-VS defaults and nominal PC-98 rate");
	auto overridden_arguments = valid_arguments();
	overridden_arguments.insert(
		overridden_arguments.end(), {"--tick-rate", "3/2"}
	);
	const auto overridden = th03::app::parse_direct_vs_cli(overridden_arguments);
	expect(overridden && overridden->tick_rate.ticks == 3U &&
		overridden->tick_rate.seconds == 2U,
		"direct-VS accepts an explicit cadence override");
}

void test_all_character_palette_names()
{
	constexpr std::array names{
		std::string_view{"reimu"},
		std::string_view{"mima"},
		std::string_view{"marisa"},
		std::string_view{"ellen"},
		std::string_view{"kotohime"},
		std::string_view{"kana"},
		std::string_view{"rikako"},
		std::string_view{"chiyuri"},
		std::string_view{"yumemi"},
	};
	constexpr std::array palettes{
		std::string_view{"first"}, std::string_view{"second"},
	};
	for(std::size_t character = 0U; character < names.size(); ++character) {
		for(std::size_t palette = 0U; palette < palettes.size(); ++palette) {
			const auto selection = std::string(names[character]) + ":" +
				std::string(palettes[palette]);
			auto arguments = valid_arguments();
			arguments.push_back("--p1");
			arguments.push_back(selection);
			if(names[character] == "kotohime") {
				arguments.push_back("--kotohime-zero-red");
			}
			const auto parsed = th03::app::parse_direct_vs_cli(arguments);
			expect(parsed && static_cast<std::size_t>(
				parsed->launch.process.player_one.character
			) == character + 1U && static_cast<std::size_t>(
				parsed->launch.process.player_one.palette
			) == palette + 1U,
			"every character/palette spelling maps to its source enum");
		}
	}
}

void test_cli_rejections()
{
	const auto unknown = parse({"--wat"});
	const auto missing_value = parse({"--vs", "--data"});
	const auto duplicate = parse({"--vs", "--vs"});
	const auto missing_required = parse({"--vs"});
	const auto bad_player = parse({
		"--vs", "--data", "d", "--main", "m", "--font", "f",
		"--tick-rate", "1/1", "--p1", "satsuki:first",
	});
	const auto bad_palette = parse({
		"--vs", "--data", "d", "--main", "m", "--font", "f",
		"--tick-rate", "1/1", "--p2", "reimu:third",
	});
	const auto bad_rate_zero = parse({
		"--vs", "--data", "d", "--main", "m", "--font", "f",
		"--tick-rate", "0/1",
	});
	const auto bad_rate_shape = parse({
		"--vs", "--data", "d", "--main", "m", "--font", "f",
		"--tick-rate", "60",
	});
	const auto bad_rank = parse({
		"--vs", "--data", "d", "--main", "m", "--font", "f",
		"--tick-rate", "1/1", "--rank", "4",
	});
	const auto bad_seed = parse({
		"--vs", "--data", "d", "--main", "m", "--font", "f",
		"--tick-rate", "1/1", "--seed", "-1",
	});
	const auto bad_smoke = parse({
		"--vs", "--data", "d", "--main", "m", "--font", "f",
		"--tick-rate", "1/1", "--smoke-ticks", "0",
	});
	const auto kotohime_without_policy = parse({
		"--vs", "--data", "d", "--main", "m", "--font", "f",
		"--tick-rate", "1/1", "--p1", "kotohime:first",
	});
	const auto kotohime_with_policy = parse({
		"--vs", "--data", "d", "--main", "m", "--font", "f",
		"--tick-rate", "1/1", "--p1", "kotohime:first",
		"--kotohime-zero-red",
	});
	expect(!unknown && unknown.error().code ==
		th03::app::DirectVsCliErrorCode::unknown_option &&
		unknown.error().argument_index == 0U,
		"unknown option reports only its stable argument index");
	expect(!missing_value && missing_value.error().code ==
		th03::app::DirectVsCliErrorCode::missing_value,
		"option without value is rejected");
	expect(!duplicate && duplicate.error().code ==
		th03::app::DirectVsCliErrorCode::duplicate_option,
		"duplicate option is rejected");
	expect(!missing_required && missing_required.error().code ==
		th03::app::DirectVsCliErrorCode::missing_required_option,
		"all required direct-VS external inputs remain explicit");
	expect(!bad_player && !bad_palette &&
		bad_player.error().code ==
			th03::app::DirectVsCliErrorCode::invalid_player_selection &&
		bad_palette.error().code ==
			th03::app::DirectVsCliErrorCode::invalid_player_selection,
		"unknown character and palette names are rejected");
	expect(!bad_rate_zero && !bad_rate_shape &&
		bad_rate_zero.error().code ==
			th03::app::DirectVsCliErrorCode::invalid_tick_rate &&
		bad_rate_shape.error().code ==
			th03::app::DirectVsCliErrorCode::invalid_tick_rate,
		"zero and malformed tick rates are rejected");
	expect(!bad_rank && bad_rank.error().code ==
		th03::app::DirectVsCliErrorCode::invalid_rank &&
		!bad_seed && bad_seed.error().code ==
			th03::app::DirectVsCliErrorCode::invalid_seed,
		"rank and seed retain their numeric source domains");
	expect(!bad_smoke && bad_smoke.error().code ==
		th03::app::DirectVsCliErrorCode::invalid_smoke_ticks,
		"developer smoke bound must consume at least one process sample");
	expect(!kotohime_without_policy &&
		kotohime_without_policy.error().code ==
			th03::app::DirectVsCliErrorCode::kotohime_policy_required &&
		kotohime_with_policy && kotohime_with_policy->kotohime_zero_red,
		"Kotohime requires an explicit unresolved developer compatibility policy");
}

class TemporaryDirectory {
public:
	TemporaryDirectory()
	{
		const auto suffix = std::chrono::steady_clock::now()
			.time_since_epoch().count();
		path_ = std::filesystem::temp_directory_path() /
			("th03-direct-vs-launch-" + std::to_string(suffix));
		std::filesystem::create_directories(path_);
	}
	~TemporaryDirectory()
	{
		std::error_code ignored;
		std::filesystem::remove_all(path_, ignored);
	}
	const std::filesystem::path &path() const noexcept
	{
		return path_;
	}

private:
	std::filesystem::path path_{};
};

bool write_bytes(
	const std::filesystem::path &path,
	std::span<const std::byte> bytes
)
{
	std::ofstream output(path, std::ios::binary);
	if(!output) {
		return false;
	}
	if(!bytes.empty()) {
		output.write(
			reinterpret_cast<const char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	return static_cast<bool>(output);
}

void test_external_file_boundary()
{
	TemporaryDirectory temporary;
	const auto missing = th03::app::DirectVsLaunchTestAccess::read_file(
		temporary.path() / "missing.bin",
		th03::app::DirectVsFile::font_rom
	);
	const auto directory = th03::app::DirectVsLaunchTestAccess::read_file(
		temporary.path(), th03::app::DirectVsFile::font_rom
	);
	std::array<std::byte, 3U> bytes{
		std::byte{0x11U}, std::byte{0x22U}, std::byte{0x33U},
	};
	const auto file = temporary.path() / "input.bin";
	expect(write_bytes(file, bytes), "temporary external input is written");
	const auto wrong_size = th03::app::DirectVsLaunchTestAccess::read_file(
		file, th03::app::DirectVsFile::font_rom, 4U
	);
	const auto loaded = th03::app::DirectVsLaunchTestAccess::read_file(
		file, th03::app::DirectVsFile::font_rom, bytes.size()
	);
	expect(!missing && missing.error().code ==
		th03::app::DirectVsLaunchErrorCode::file_missing &&
		missing.error().file == th03::app::DirectVsFile::font_rom,
		"missing file reports only its semantic role");
	expect(!directory && directory.error().code ==
		th03::app::DirectVsLaunchErrorCode::file_not_regular,
		"directory is not accepted as external content");
	expect(!wrong_size && wrong_size.error().code ==
		th03::app::DirectVsLaunchErrorCode::file_size_mismatch,
		"exact FONT-style size mismatch fails before reading");
	expect(loaded && *loaded == std::vector<std::byte>(bytes.begin(), bytes.end()),
		"successful external read returns an owning exact byte copy");
}

void test_transaction_stops_at_compiled_profile()
{
	TemporaryDirectory temporary;
	const auto main = temporary.path() / "MAIN.EXE";
	const auto font = temporary.path() / "FONT.ROM";
	std::array<std::byte, 4U> invalid_main{};
	std::vector<std::byte> valid_size_font(
		th03::app::Pc98FontRomProvider::font_rom_size
	);
	expect(write_bytes(main, invalid_main) && write_bytes(font, valid_size_font),
		"transaction fixture files are written");
	auto request = th03::app::DirectVsLaunchRequest{};
	request.content = {
		.data_root = temporary.path(),
		.main_executable = main,
		.font_rom = font,
	};
	const auto result = th03::app::load_direct_vs_process(std::move(request));
	expect(!result && result.error().code ==
		th03::app::DirectVsLaunchErrorCode::compiled_content_failed &&
		result.error().file == th03::app::DirectVsFile::main_executable &&
		result.error().compiled_content_error &&
		result.error().compiled_content_error->code ==
			th03::app::MainCompiledContentErrorCode::input_too_short,
		"MAIN authentication failure stops before FONT/resources and reports no bytes");
}

void test_invalid_config_precedes_external_io()
{
	auto request = th03::app::DirectVsLaunchRequest{};
	request.content = {
		.data_root = "missing-data-root",
		.main_executable = "missing-main.exe",
		.font_rom = "missing-font.rom",
	};
	request.process.rank = 4U;
	const auto result = th03::app::load_direct_vs_process(std::move(request));
	expect(
		!result && result.error().code ==
			th03::app::DirectVsLaunchErrorCode::invalid_request &&
			result.error().file == th03::app::DirectVsFile::none,
		"invalid process config is rejected before any external file access"
	);
}

} // namespace

int main()
{
	test_help_and_valid_defaults();
	test_all_character_palette_names();
	test_cli_rejections();
	test_external_file_boundary();
	test_transaction_stops_at_compiled_profile();
	test_invalid_config_precedes_external_io();

	if(failures != 0U) {
		std::cerr << failures << " of " << checks
			<< " app direct-VS launch checks failed\n";
		return 1;
	}
	std::cout << "all " << checks
		<< " app direct-VS launch checks passed\n";
	return 0;
}
