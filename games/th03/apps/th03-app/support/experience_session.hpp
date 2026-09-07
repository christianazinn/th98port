#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

#include "netplay_session.hpp"

#include "th03/app/classic/classic_cli.hpp"
#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_room.hpp"
#include "th03/formats/replay.hpp"
#include "th03/platform/sdl_input.hpp"
#include "th03/platform/sdl_platform.hpp"

namespace th03::app::launcher_support {

enum class ExperienceRunDisposition : std::uint8_t {
	return_to_menu,
	restart_new_classic,
	switch_to_classic,
	netplay_requested,
	application_quit,
	smoke_complete,
	replay_completed,
	failed,
};

struct ExperienceRunResult final {
	int exit_code = 0;
	ExperienceRunDisposition disposition = ExperienceRunDisposition::failed;
	std::optional<formats::ReplayDocument> replay{};
	std::optional<NewClassicNetplayRoomLaunch> netplay_launch{};
};

enum class NetplayFrontendDisposition : std::uint8_t {
	resume_title,
	restart_new_classic,
	application_quit,
	failed,
};

struct NetplayFrontendResult final {
	int exit_code = 0;
	NetplayFrontendDisposition disposition =
		NetplayFrontendDisposition::failed;
};

using NetplayFrontendHandler = std::function<NetplayFrontendResult(
	const RetainedMenuAudio &
)>;

[[nodiscard]] ExperienceRunResult run_classic_session(
	ClassicCliRequest parsed,
	platform::SdlPlatform &platform,
	platform::SdlInputMapper &mapper,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {},
	NetplayFrontendHandler *netplay_frontend = nullptr
);

} // namespace th03::app::launcher_support
