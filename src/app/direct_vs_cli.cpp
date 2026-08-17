#include "th03/app/direct_vs_cli.hpp"

#include <array>
#include <charconv>
#include <filesystem>
#include <optional>

namespace th03::app {
namespace {

constexpr std::size_t option_count =
	static_cast<std::size_t>(DirectVsCliOption::netplay_replay_output) + 1U;

DirectVsCliError cli_error(
	DirectVsCliErrorCode code,
	DirectVsCliOption option,
	std::size_t argument_index
) noexcept
{
	return DirectVsCliError{
		.code = code,
		.option = option,
		.argument_index = argument_index,
	};
}

std::optional<std::uint32_t> parse_u32(std::string_view text) noexcept
{
	std::uint32_t value = 0U;
	const auto parsed = std::from_chars(
		text.data(), text.data() + text.size(), value
	);
	if(parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
		return std::nullopt;
	}
	return value;
}

std::optional<std::uint64_t> parse_u64(std::string_view text) noexcept
{
	std::uint64_t value = 0U;
	const auto parsed = std::from_chars(
		text.data(), text.data() + text.size(), value
	);
	if(parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
		return std::nullopt;
	}
	return value;
}

std::optional<MainCharacter> parse_character(std::string_view text) noexcept
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
	for(std::size_t index = 0U; index < names.size(); ++index) {
		if(text == names[index]) {
			return static_cast<MainCharacter>(index + 1U);
		}
	}
	return std::nullopt;
}

std::optional<MainPlayerSelection> parse_selection(
	std::string_view text,
	MainPaletteVariant default_palette
) noexcept
{
	const auto separator = text.find(':');
	const auto name = text.substr(0U, separator);
	const auto character = parse_character(name);
	if(!character) {
		return std::nullopt;
	}
	auto palette = default_palette;
	if(separator != std::string_view::npos) {
		const auto suffix = text.substr(separator + 1U);
		if(suffix == "first") {
			palette = MainPaletteVariant::first;
		} else if(suffix == "second") {
			palette = MainPaletteVariant::second;
		} else {
			return std::nullopt;
		}
	}
	return MainPlayerSelection{
		.character = *character,
		.palette = palette,
	};
}

std::optional<RationalTickRate> parse_tick_rate(std::string_view text) noexcept
{
	const auto separator = text.find('/');
	if(separator == std::string_view::npos ||
		text.find('/', separator + 1U) != std::string_view::npos) {
		return std::nullopt;
	}
	const auto ticks = parse_u32(text.substr(0U, separator));
	const auto seconds = parse_u32(text.substr(separator + 1U));
	if(!ticks || !seconds) {
		return std::nullopt;
	}
	const auto rate = RationalTickRate{.ticks = *ticks, .seconds = *seconds};
	if(!TickInputScheduler::create(rate, 0U)) {
		return std::nullopt;
	}
	return rate;
}

std::optional<DirectVsCliOption> option_from_text(
	std::string_view text
) noexcept
{
	if(text == "--help" || text == "-h") {
		return DirectVsCliOption::help;
	}
	if(text == "--vs") {
		return DirectVsCliOption::vs;
	}
	if(text == "--data") {
		return DirectVsCliOption::data;
	}
	if(text == "--main") {
		return DirectVsCliOption::main_executable;
	}
	if(text == "--font") {
		return DirectVsCliOption::font_rom;
	}
	if(text == "--p1") {
		return DirectVsCliOption::player_one;
	}
	if(text == "--p2") {
		return DirectVsCliOption::player_two;
	}
	if(text == "--tick-rate") {
		return DirectVsCliOption::tick_rate;
	}
	if(text == "--rank") {
		return DirectVsCliOption::rank;
	}
	if(text == "--seed") {
		return DirectVsCliOption::seed;
	}
	if(text == "--smoke-ticks") {
		return DirectVsCliOption::smoke_ticks;
	}
	if(text == "--netplay-host") {
		return DirectVsCliOption::netplay_host;
	}
	if(text == "--netplay-join") {
		return DirectVsCliOption::netplay_join;
	}
	if(text == "--netplay-replay-output") {
		return DirectVsCliOption::netplay_replay_output;
	}
	return std::nullopt;
}

bool option_takes_value(DirectVsCliOption option) noexcept
{
	return option != DirectVsCliOption::help && option != DirectVsCliOption::vs;
}

} // namespace

std::expected<DirectVsCliRequest, DirectVsCliError> parse_direct_vs_cli(
	std::span<const std::string_view> arguments
)
{
	if(arguments.size() == 1U &&
		(arguments[0] == "--help" || arguments[0] == "-h")) {
		return DirectVsCliRequest{.show_help = true};
	}

	DirectVsCliRequest request{};
	std::array<bool, option_count> seen{};
	for(std::size_t index = 0U; index < arguments.size(); ++index) {
		const auto option = option_from_text(arguments[index]);
		if(!option) {
			return std::unexpected(cli_error(
				DirectVsCliErrorCode::unknown_option,
				DirectVsCliOption::none,
				index
			));
		}
		const auto option_index = static_cast<std::size_t>(*option);
		if(seen[option_index]) {
			return std::unexpected(cli_error(
				DirectVsCliErrorCode::duplicate_option, *option, index
			));
		}
		seen[option_index] = true;
		if(*option == DirectVsCliOption::help) {
			return std::unexpected(cli_error(
				DirectVsCliErrorCode::unknown_option, *option, index
			));
		}
		if(!option_takes_value(*option)) {
			continue;
		}
		if(index + 1U >= arguments.size()) {
			return std::unexpected(cli_error(
				DirectVsCliErrorCode::missing_value, *option, index
			));
		}
		const auto value_index = ++index;
		const auto value = arguments[value_index];
		switch(*option) {
		case DirectVsCliOption::data:
			request.launch.content.data_root = std::filesystem::path(value);
			break;
		case DirectVsCliOption::main_executable:
			request.launch.content.main_executable = std::filesystem::path(value);
			break;
		case DirectVsCliOption::font_rom:
			request.launch.content.font_rom = std::filesystem::path(value);
			break;
		case DirectVsCliOption::player_one: {
			const auto selected = parse_selection(
				value, MainPaletteVariant::first
			);
			if(!selected) {
				return std::unexpected(cli_error(
					DirectVsCliErrorCode::invalid_player_selection,
					*option,
					value_index
				));
			}
			request.launch.process.player_one = *selected;
			break;
		}
		case DirectVsCliOption::player_two: {
			const auto selected = parse_selection(
				value, MainPaletteVariant::second
			);
			if(!selected) {
				return std::unexpected(cli_error(
					DirectVsCliErrorCode::invalid_player_selection,
					*option,
					value_index
				));
			}
			request.launch.process.player_two = *selected;
			break;
		}
		case DirectVsCliOption::tick_rate: {
			const auto rate = parse_tick_rate(value);
			if(!rate) {
				return std::unexpected(cli_error(
					DirectVsCliErrorCode::invalid_tick_rate,
					*option,
					value_index
				));
			}
			request.tick_rate = *rate;
			break;
		}
		case DirectVsCliOption::rank: {
			const auto rank = parse_u32(value);
			if(!rank || *rank > 3U) {
				return std::unexpected(cli_error(
					DirectVsCliErrorCode::invalid_rank,
					*option,
					value_index
				));
			}
			request.launch.process.rank = static_cast<std::uint8_t>(*rank);
			break;
		}
		case DirectVsCliOption::seed: {
			const auto seed = parse_u32(value);
			if(!seed) {
				return std::unexpected(cli_error(
					DirectVsCliErrorCode::invalid_seed,
					*option,
					value_index
				));
			}
			request.launch.process.seed = *seed;
			break;
		}
		case DirectVsCliOption::smoke_ticks: {
			const auto ticks = parse_u64(value);
			if(!ticks || *ticks == 0U) {
				return std::unexpected(cli_error(
					DirectVsCliErrorCode::invalid_smoke_ticks,
					*option,
					value_index
				));
			}
			request.smoke_ticks = *ticks;
			break;
		}
		case DirectVsCliOption::netplay_host: {
			const auto port = parse_u32(value);
			if(!port || *port == 0U || *port > 65'535U) {
				return std::unexpected(cli_error(
					DirectVsCliErrorCode::invalid_netplay_endpoint,
					*option,
					value_index
				));
			}
			request.netplay_mode = DirectVsNetplayMode::host;
			request.netplay_host_port = static_cast<std::uint16_t>(*port);
			break;
		}
		case DirectVsCliOption::netplay_join:
			if(value.empty()) {
				return std::unexpected(cli_error(
					DirectVsCliErrorCode::invalid_netplay_endpoint,
					*option,
					value_index
				));
			}
			request.netplay_mode = DirectVsNetplayMode::join;
			request.netplay_join_endpoint = std::string(value);
			break;
		case DirectVsCliOption::netplay_replay_output:
			if(value.empty()) {
				return std::unexpected(cli_error(
					DirectVsCliErrorCode::invalid_replay_output,
					*option,
					value_index
				));
			}
			request.netplay_replay_output = std::filesystem::path(value);
			break;
		case DirectVsCliOption::none:
		case DirectVsCliOption::help:
		case DirectVsCliOption::vs:
			break;
		}
	}
	if(seen[static_cast<std::size_t>(DirectVsCliOption::netplay_host)] &&
		seen[static_cast<std::size_t>(DirectVsCliOption::netplay_join)]) {
		return std::unexpected(cli_error(
			DirectVsCliErrorCode::conflicting_netplay_mode,
			DirectVsCliOption::netplay_join,
			arguments.size()
		));
	}
	if(seen[static_cast<std::size_t>(
		DirectVsCliOption::netplay_replay_output
	)] && request.netplay_mode == DirectVsNetplayMode::none) {
		return std::unexpected(cli_error(
			DirectVsCliErrorCode::conflicting_netplay_mode,
			DirectVsCliOption::netplay_replay_output,
			arguments.size()
		));
	}

	constexpr std::array required{
		DirectVsCliOption::vs,
		DirectVsCliOption::data,
		DirectVsCliOption::main_executable,
		DirectVsCliOption::font_rom,
	};
	for(const auto option : required) {
		if(!seen[static_cast<std::size_t>(option)]) {
			return std::unexpected(cli_error(
				DirectVsCliErrorCode::missing_required_option,
				option,
				arguments.size()
			));
		}
	}
	request.launch.process.game_options.input_protocol = InputProtocol::live;
	return request;
}

} // namespace th03::app
