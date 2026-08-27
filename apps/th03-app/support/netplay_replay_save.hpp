#pragma once

#include <expected>
#include <filesystem>
#include <memory>

#include "th03/app/direct_vs/direct_vs_launch.hpp"
#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/new_classic_frontend.hpp"

namespace th03::app::launcher_support {

struct DirectNetplayReplaySaveResources final {
	std::filesystem::path replay_root{};
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets{};
	std::shared_ptr<const NewClassicReplaySaveArt> save_art{};
};

[[nodiscard]] std::expected<DirectNetplayReplaySaveResources, int>
prepare_direct_netplay_replay_save_resources(
	const DirectVsContentPaths &content
);

} // namespace th03::app::launcher_support
