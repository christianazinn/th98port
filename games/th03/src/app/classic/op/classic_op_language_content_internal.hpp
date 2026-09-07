#ifndef TH03_APP_CLASSIC_OP_LANGUAGE_CONTENT_INTERNAL_HPP
#define TH03_APP_CLASSIC_OP_LANGUAGE_CONTENT_INTERNAL_HPP

#include <cstddef>
#include <expected>
#include <map>
#include <memory>
#include <span>
#include <string>

#include "classic/mainl/classic_mainl_stage_internal.hpp"
#include "classic/mainl/mainl_compiled_text_internal.hpp"
#include "classic/op/classic_op_session_internal.hpp"
#include "main/main_process_static_content_internal.hpp"
#include "th03/app/new_classic/content/new_classic_assets.hpp"

namespace th03::app::detail {

class ClassicOpLanguageContent final {
public:
	ClassicOpLanguageContent(
		MainCompiledContent compiled,
		MainlStageTextAssets stage_text,
		OpMusicRoomTextImport op_music_labels,
		std::map<std::string, ResourceBytes> english_resources
	);

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	apply_language_profile(
		MainResourceLoader &loader,
		const std::shared_ptr<const NewClassicReplayPatchAssets> &patch_assets,
		ClassicLanguage language
	);

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	ensure_static_content(
		MainResourceLoader &loader,
		std::span<const std::byte> font_rom,
		const std::shared_ptr<const NewClassicReplayPatchAssets> &patch_assets,
		ClassicLanguage language
	);

	[[nodiscard]] const NewClassicGameTextCatalog *game_text_for(
		const std::shared_ptr<const NewClassicReplayPatchAssets> &patch_assets,
		ClassicLanguage language
	) const noexcept;
	[[nodiscard]] std::shared_ptr<const NewClassicCutsceneTextCatalog>
	cutscene_text_for(
		const std::shared_ptr<const NewClassicReplayPatchAssets> &patch_assets,
		ClassicLanguage language
	) const noexcept;
	[[nodiscard]] std::shared_ptr<const NewClassicPicturePatchCatalog>
	picture_patch_for(
		const std::shared_ptr<const NewClassicReplayPatchAssets> &patch_assets,
		ClassicLanguage language
	) const noexcept;

	[[nodiscard]] const MainlStageTextAssets &stage_text() const noexcept;
	[[nodiscard]] const OpMusicRoomTextImport &op_music_labels() const noexcept;
	[[nodiscard]] ClassicLanguage active_language() const noexcept;
	[[nodiscard]] bool english_assets_available() const noexcept;
	[[nodiscard]] const std::shared_ptr<const MainProcessStaticContent> &
	static_content() const noexcept;

private:
	MainCompiledContent compiled_;
	MainlStageTextAssets stage_text_{};
	OpMusicRoomTextImport op_music_labels_{};
	MainCompiledContent japanese_compiled_;
	MainlStageTextAssets japanese_stage_text_{};
	OpMusicRoomTextImport japanese_op_music_labels_{};
	std::map<std::string, ResourceBytes> english_resources_{};
	ClassicLanguage active_language_ = ClassicLanguage::japanese;
	std::shared_ptr<const MainProcessStaticContent> static_content_{};
};

} // namespace th03::app::detail

#endif
