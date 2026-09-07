#ifndef TH03_APP_NEW_CLASSIC_PLAYBACK_CONTENT_INTERNAL_HPP
#define TH03_APP_NEW_CLASSIC_PLAYBACK_CONTENT_INTERNAL_HPP

#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <vector>

#include "classic/mainl/classic_mainl_stage_internal.hpp"
#include "classic/mainl/mainl_compiled_text_internal.hpp"
#include "main/main_process_static_content_internal.hpp"
#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/replay/new_classic_playback.hpp"

namespace th03::app {

struct PreparedNewClassicPlaybackContent final {
	std::shared_ptr<MainResourceLoader> loader{};
	MainCompiledContent compiled_content;
	std::vector<std::byte> font{};
	std::shared_ptr<const MainProcessStaticContent> static_content{};
	MainlStageTextAssets stage_text{};
	MainlStaffCompiledAssets staff_compiled{};
	std::filesystem::path score_path{};
	std::filesystem::path legacy_score_path{};
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets{};
	const NewClassicGameTextCatalog *game_text = nullptr;
};

[[nodiscard]] std::expected<
	PreparedNewClassicPlaybackContent,
	NewClassicPlaybackError
> prepare_new_classic_playback_content(
	ClassicContentPaths content,
	ClassicLanguage language
);

} // namespace th03::app

#endif
