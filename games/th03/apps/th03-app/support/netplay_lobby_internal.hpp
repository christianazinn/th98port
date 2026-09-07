#pragma once

#include <expected>
#include <memory>
#include <optional>

#include "netplay_session.hpp"
#include "netplay_wire.hpp"

#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_lobby.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_match_contract.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_room.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_signaling.hpp"

namespace th03::app::launcher_support {

struct DirectNetplayLobbyResult final {
	NetplayWire wire;
	NewClassicNetplayLobby lobby{};
	NewClassicNetplayMatchContract contract{};
	std::optional<NewClassicNetplayRoomPaired> profiles{};
};

[[nodiscard]] std::expected<DirectNetplayLobbyResult, int>
negotiate_direct_netplay_lobby(
	const DirectVsCliRequest &request,
	platform::SdlPlatform &platform,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	const RetainedMenuAudio *retained_audio = nullptr,
	bool *application_quit = nullptr
);

#if defined(TH03_ENABLE_RTC_TRANSPORT)
[[nodiscard]] std::expected<DirectNetplayLobbyResult, int>
negotiate_rtc_netplay_lobby(
	const DirectVsCliRequest &request,
	const RtcNetplayRequest &rtc_request,
	platform::SdlPlatform &platform,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	const RetainedMenuAudio *retained_audio = nullptr,
	bool *application_quit = nullptr
);
#endif

} // namespace th03::app::launcher_support
