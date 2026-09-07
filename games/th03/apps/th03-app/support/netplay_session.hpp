#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "th03/app/direct_vs/direct_vs_cli.hpp"
#include "th03/app/new_classic/input/new_classic_device_store.hpp"
#include "th03/app/new_classic/input/new_classic_joy_config_store.hpp"
#include "th03/app/new_classic/input/new_classic_key_config_store.hpp"
#include "th03/platform/rtc_room.hpp"
#include "th03/platform/sdl_input.hpp"
#include "th03/platform/sdl_platform.hpp"

namespace th03::app::launcher_support {

struct RetainedMenuAudio final {
	std::function<std::expected<void, int>()> advance_tick{};
	std::function<std::expected<void, int>()> begin_match_ready{};
	std::function<std::expected<bool, int>()> match_ready_complete{};
	std::function<std::expected<void, int>()> cancel_match_ready{};
	std::function<void()> suspend{};
	std::function<std::expected<void, int>()> resume{};
};

struct RtcNetplayRequest final {
	platform::RtcRoomMode mode = platform::RtcRoomMode::create;
	std::string origin{};
	std::string user_id{};
	std::string token{};
	std::string room_code{};
	std::uint8_t rank = 1U;
	std::vector<platform::RtcIceServer> ice_servers{};
	bool relay_only = false;
};

[[nodiscard]] std::uint32_t fresh_netplay_seed() noexcept;

[[nodiscard]] int run_direct_netplay_on_platform(
	DirectVsCliRequest parsed,
	platform::SdlPlatform &platform,
	platform::SdlInputMapper &mapper,
	std::optional<RtcNetplayRequest> rtc_request = std::nullopt,
	const RetainedMenuAudio *retained_menu_audio = nullptr,
	bool *gameplay_started = nullptr,
	bool *application_quit = nullptr,
	const NewClassicKeyConfig *key_config = nullptr,
	const NewClassicJoyConfig *joy_config = nullptr,
	const NewClassicDeviceAssignment *device_assignment = nullptr
);

} // namespace th03::app::launcher_support
