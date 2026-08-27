#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>

#include "th03/app/classic/classic_audio_session.hpp"
#include "th03/app/direct_vs/direct_vs_launch.hpp"
#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/new_classic_frontend.hpp"
#include "th03/app/new_classic/replay/replay_slot_store.hpp"
#include "th03/app/runtime/tick_input.hpp"
#include "th03/formats/replay.hpp"
#include "th03/platform/sdl_audio_device.hpp"
#include "th03/platform/sdl_input.hpp"
#include "th03/platform/sdl_platform.hpp"

namespace th03::app::launcher_support {

struct DirectNetplayReplaySaveResources final {
	std::filesystem::path replay_root{};
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets{};
	std::shared_ptr<const NewClassicReplaySaveArt> save_art{};
};

struct ReplaySaveRunResult final {
	int exit_code = 0;
	bool application_quit = false;
	bool replay_published = false;
};

struct ReplaySaveAudioContext final {
	ClassicAudioSession *session = nullptr;
	std::optional<platform::SdlAudioDevice> *device = nullptr;
	bool *started = nullptr;
	const std::uint8_t *master_volume_percent = nullptr;
	const bool *mute_when_unfocused = nullptr;
};

[[nodiscard]] std::expected<DirectNetplayReplaySaveResources, int>
prepare_direct_netplay_replay_save_resources(
	const DirectVsContentPaths &content
);

[[nodiscard]] ReplaySaveRunResult run_replay_save(
	formats::ReplayDocument document,
	const std::filesystem::path &replay_root,
	RationalTickRate tick_rate,
	platform::SdlPlatform &platform,
	platform::SdlInputMapper &mapper,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {},
	std::shared_ptr<const NewClassicReplaySaveArt> save_art = {},
	ReplaySaveAudioContext audio = {},
	std::optional<ReplaySlotLocalView> local_view = std::nullopt
);

} // namespace th03::app::launcher_support
