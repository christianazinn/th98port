#include "th03/app/classic_cli.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <optional>

namespace th03::app {
namespace {

constexpr std::size_t option_count =
	static_cast<std::size_t>(ClassicCliOption::smoke_ticks) + 1U;

[[nodiscard]] ClassicCliError cli_error(
	ClassicCliErrorCode code,
	ClassicCliOption option,
	std::size_t argument_index
) noexcept
{
	return {
		.code = code,
		.option = option,
		.argument_index = argument_index,
	};
}

[[nodiscard]] std::optional<std::uint32_t> parse_u32(
	std::string_view text
) noexcept
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

[[nodiscard]] std::optional<std::uint64_t> parse_u64(
	std::string_view text
) noexcept
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

[[nodiscard]] std::optional<RationalTickRate> parse_tick_rate(
	std::string_view text
) noexcept
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
	const RationalTickRate rate{.ticks = *ticks, .seconds = *seconds};
	if(!TickInputScheduler::create(rate, 0U)) {
		return std::nullopt;
	}
	return rate;
}

[[nodiscard]] std::optional<ClassicCliOption> option_from_text(
	std::string_view text
) noexcept
{
	if(text == "--help" || text == "-h") {
		return ClassicCliOption::help;
	}
	if(text == "--classic") {
		return ClassicCliOption::classic;
	}
	if(text == "--data") {
		return ClassicCliOption::data;
	}
	if(text == "--main") {
		return ClassicCliOption::main_executable;
	}
	if(text == "--mainl") {
		return ClassicCliOption::mainl_executable;
	}
	if(text == "--op") {
		return ClassicCliOption::op_executable;
	}
	if(text == "--font") {
		return ClassicCliOption::font_rom;
	}
	if(text == "--rhythm-rom") {
		return ClassicCliOption::rhythm_rom;
	}
	if(text == "--config") {
		return ClassicCliOption::config;
	}
	if(text == "--score") {
		return ClassicCliOption::score;
	}
	if(text == "--tick-rate") {
		return ClassicCliOption::tick_rate;
	}
	if(text == "--seed") {
		return ClassicCliOption::seed;
	}
	if(text == "--smoke-ticks") {
		return ClassicCliOption::smoke_ticks;
	}
	return std::nullopt;
}

[[nodiscard]] constexpr bool option_takes_value(
	ClassicCliOption option
) noexcept
{
	return option != ClassicCliOption::help &&
		option != ClassicCliOption::classic;
}

} // namespace

std::expected<ClassicCliRequest, ClassicCliError> parse_classic_cli(
	std::span<const std::string_view> arguments
)
{
	if(arguments.size() == 1U &&
		(arguments[0] == "--help" || arguments[0] == "-h")) {
		return ClassicCliRequest{.show_help = true};
	}

	ClassicCliRequest request{};
	std::array<bool, option_count> seen{};
	for(std::size_t index = 0U; index < arguments.size(); ++index) {
		const auto option = option_from_text(arguments[index]);
		if(!option) {
			return std::unexpected(cli_error(
				ClassicCliErrorCode::unknown_option,
				ClassicCliOption::none,
				index
			));
		}
		const auto option_index = static_cast<std::size_t>(*option);
		if(seen[option_index]) {
			return std::unexpected(cli_error(
				ClassicCliErrorCode::duplicate_option, *option, index
			));
		}
		seen[option_index] = true;
		if(*option == ClassicCliOption::help) {
			request.show_help = true;
			continue;
		}
		if(!option_takes_value(*option)) {
			continue;
		}
		if(index + 1U >= arguments.size()) {
			return std::unexpected(cli_error(
				ClassicCliErrorCode::missing_value, *option, index
			));
		}
		const auto value_index = ++index;
		const auto value = arguments[value_index];
		switch(*option) {
		case ClassicCliOption::data:
			request.launch.content.data_root = std::filesystem::path(value);
			break;
		case ClassicCliOption::main_executable:
			request.launch.content.main_executable = std::filesystem::path(value);
			break;
		case ClassicCliOption::mainl_executable:
			request.launch.content.mainl_executable = std::filesystem::path(value);
			break;
		case ClassicCliOption::op_executable:
			request.launch.content.op_executable = std::filesystem::path(value);
			break;
		case ClassicCliOption::font_rom:
			request.launch.content.font_rom = std::filesystem::path(value);
			break;
		case ClassicCliOption::rhythm_rom:
			request.rhythm_rom = std::filesystem::path(value);
			break;
		case ClassicCliOption::config:
			request.launch.content.config = std::filesystem::path(value);
			break;
		case ClassicCliOption::score:
			request.launch.content.score = std::filesystem::path(value);
			break;
		case ClassicCliOption::tick_rate: {
			const auto rate = parse_tick_rate(value);
			if(!rate) {
				return std::unexpected(cli_error(
					ClassicCliErrorCode::invalid_tick_rate,
					*option,
					value_index
				));
			}
			request.tick_rate = *rate;
			break;
		}
		case ClassicCliOption::seed: {
			const auto seed = parse_u32(value);
			if(!seed) {
				return std::unexpected(cli_error(
					ClassicCliErrorCode::invalid_seed, *option, value_index
				));
			}
			request.launch.seed = *seed;
			break;
		}
		case ClassicCliOption::smoke_ticks: {
			const auto ticks = parse_u64(value);
			if(!ticks || *ticks == 0U) {
				return std::unexpected(cli_error(
					ClassicCliErrorCode::invalid_smoke_ticks,
					*option,
					value_index
				));
			}
			request.smoke_ticks = *ticks;
			break;
		}
		case ClassicCliOption::none:
		case ClassicCliOption::help:
		case ClassicCliOption::classic:
			break;
		}
	}
	if(request.show_help) {
		return request;
	}

	constexpr std::array required{
		ClassicCliOption::classic,
		ClassicCliOption::data,
		ClassicCliOption::main_executable,
		ClassicCliOption::mainl_executable,
		ClassicCliOption::op_executable,
		ClassicCliOption::font_rom,
	};
	for(const auto option : required) {
		if(!seen[static_cast<std::size_t>(option)]) {
			return std::unexpected(cli_error(
				ClassicCliErrorCode::missing_required_option,
				option,
				arguments.size()
			));
		}
	}
	request.launch.fm_available = false;
	return request;
}

} // namespace th03::app
