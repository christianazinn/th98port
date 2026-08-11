#ifndef TH03_APP_CLASSIC_CLI_HPP
#define TH03_APP_CLASSIC_CLI_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

#include "th03/app/classic_runner.hpp"

namespace th03::app {

enum class ClassicCliOption : std::uint8_t {
	none,
	help,
	classic,
	data,
	main_executable,
	mainl_executable,
	op_executable,
	font_rom,
	config,
	score,
	tick_rate,
	seed,
	kotohime_zero_red,
	smoke_ticks,
};

enum class ClassicCliErrorCode : std::uint8_t {
	unknown_option,
	missing_value,
	duplicate_option,
	missing_required_option,
	invalid_tick_rate,
	invalid_seed,
	invalid_smoke_ticks,
};

struct ClassicCliError final {
	ClassicCliErrorCode code = ClassicCliErrorCode::unknown_option;
	ClassicCliOption option = ClassicCliOption::none;
	std::size_t argument_index = 0U;
};

struct ClassicCliRequest final {
	bool show_help = false;
	ClassicRunnerRequest launch{};
	RationalTickRate tick_rate{};
	// Explicit developer compatibility policy for the donor's uninitialized
	// Kotohime Bomb fade byte. This is never an authenticity claim.
	bool kotohime_zero_red = false;
	std::optional<std::uint64_t> smoke_ticks{};
};

// Parses arguments after argv[0]. Cadence remains a required host policy until
// the PC-98 timing capture establishes a wall-clock rational and first phase.
[[nodiscard]] std::expected<ClassicCliRequest, ClassicCliError>
parse_classic_cli(std::span<const std::string_view> arguments);

} // namespace th03::app

#endif
