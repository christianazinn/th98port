#ifndef TH03_APP_DIRECT_VS_CLI_HPP
#define TH03_APP_DIRECT_VS_CLI_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

#include "th03/app/direct_vs_launch.hpp"
#include "th03/app/tick_input.hpp"

namespace th03::app {

enum class DirectVsCliOption : std::uint8_t {
	none,
	help,
	vs,
	data,
	main_executable,
	font_rom,
	player_one,
	player_two,
	tick_rate,
	rank,
	seed,
	kotohime_zero_red,
	smoke_ticks,
};

enum class DirectVsCliErrorCode : std::uint8_t {
	unknown_option,
	missing_value,
	duplicate_option,
	missing_required_option,
	invalid_player_selection,
	invalid_tick_rate,
	invalid_rank,
	invalid_seed,
	kotohime_policy_required,
	invalid_smoke_ticks,
};

struct DirectVsCliError {
	DirectVsCliErrorCode code = DirectVsCliErrorCode::unknown_option;
	DirectVsCliOption option = DirectVsCliOption::none;
	std::size_t argument_index = 0U;
};

struct DirectVsCliRequest {
	bool show_help = false;
	DirectVsLaunchRequest launch{};
	RationalTickRate tick_rate = pc98_24khz_nominal_tick_rate;
	// Explicit developer compatibility policy for the donor's uninitialized
	// Kotohime Bomb fade byte. This is never an authenticity claim.
	bool kotohime_zero_red = false;
	std::optional<std::uint64_t> smoke_ticks{};
};

// Parses arguments after argv[0]. Character names are the nine lowercase
// source identities, optionally followed by :first or :second. The nominal
// PC-9801 24 kHz cadence is the default; --tick-rate remains an explicit
// developer/host override.
[[nodiscard]] std::expected<DirectVsCliRequest, DirectVsCliError>
parse_direct_vs_cli(std::span<const std::string_view> arguments);

} // namespace th03::app

#endif
