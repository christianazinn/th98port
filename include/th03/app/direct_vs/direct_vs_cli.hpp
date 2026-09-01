#ifndef TH03_APP_DIRECT_VS_CLI_HPP
#define TH03_APP_DIRECT_VS_CLI_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "th03/app/direct_vs/direct_vs_launch.hpp"
#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/runtime/tick_input.hpp"

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
	smoke_ticks,
	netplay_host,
	netplay_join,
	replay_root,
	netplay_replay_output,
	netplay_input_delay,
};

enum class DirectVsNetplayMode : std::uint8_t {
	none,
	host,
	join,
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
	invalid_smoke_ticks,
	invalid_netplay_endpoint,
	invalid_replay_output,
	invalid_netplay_input_delay,
	conflicting_netplay_mode,
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
	std::optional<std::uint64_t> smoke_ticks{};
	DirectVsNetplayMode netplay_mode = DirectVsNetplayMode::none;
	std::uint16_t netplay_host_port = 0U;
	std::string netplay_join_endpoint{};
	std::filesystem::path replay_root{};
	std::optional<std::filesystem::path> netplay_replay_output{};
	std::uint8_t netplay_input_delay_frames = 0U;
	// Each netplay peer contributes its ordinary local Story Autofire choice;
	// the negotiated role projects it into the deterministic P1/P2 mask.
	bool netplay_local_autofire = false;
	// Local presentation only; never serialized into the match contract.
	std::uint8_t master_volume_percent = 100U;
	std::uint8_t music_volume_percent = 100U;
	std::uint8_t effect_volume_percent = 100U;
	ClassicBgmMode bgm_mode = ClassicBgmMode::fm;
	bool mute_when_unfocused = false;
	NewClassicLanguageAssetProfile presentation_language =
		NewClassicLanguageAssetProfile::english;
};

// Parses arguments after argv[0]. Character names are the nine lowercase
// source identities, optionally followed by :first or :second. The nominal
// PC-9801 24 kHz cadence is the default; --tick-rate remains an explicit
// developer/host override.
[[nodiscard]] std::expected<DirectVsCliRequest, DirectVsCliError>
parse_direct_vs_cli(std::span<const std::string_view> arguments);

} // namespace th03::app

#endif
