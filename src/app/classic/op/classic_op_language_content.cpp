#include "classic/op/classic_op_language_content_internal.hpp"

#include "new_classic/content/new_classic_language_internal.hpp"

#include <cstdint>
#include <utility>

namespace th03::app::detail {
namespace {

[[nodiscard]] ClassicOpSessionError session_error(
	ClassicOpSessionErrorCode code
) noexcept
{
	return {.code = code};
}

[[nodiscard]] ClassicOpSessionError resource_error(ResourceLoadError error)
{
	auto result = session_error(ClassicOpSessionErrorCode::resource_load_failed);
	result.resource_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError story_error(
	ClassicStorySessionError error
)
{
	auto result = session_error(ClassicOpSessionErrorCode::story_failed);
	result.story_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError static_content_error(
	MainProcessError error
)
{
	auto result = session_error(ClassicOpSessionErrorCode::static_content_failed);
	result.main_error.emplace(std::move(error));
	return result;
}

} // namespace

ClassicOpLanguageContent::ClassicOpLanguageContent(
	MainCompiledContent compiled,
	MainlStageTextAssets stage_text,
	OpMusicRoomTextImport op_music_labels,
	std::map<std::string, ResourceBytes> english_resources
) :
	compiled_(std::move(compiled)),
	stage_text_(std::move(stage_text)),
	op_music_labels_(std::move(op_music_labels)),
	japanese_compiled_(compiled_),
	japanese_stage_text_(stage_text_),
	japanese_op_music_labels_(op_music_labels_),
	english_resources_(std::move(english_resources))
{
}

const NewClassicGameTextCatalog *ClassicOpLanguageContent::game_text_for(
	const std::shared_ptr<const NewClassicReplayPatchAssets> &patch_assets,
	ClassicLanguage language
) const noexcept
{
	if(patch_assets && patch_assets->custom_game_text) {
		const bool japanese =
			patch_assets->custom_game_text->locale() == "ja-JP";
		if((language == ClassicLanguage::japanese) == japanese) {
			return patch_assets->custom_game_text.get();
		}
	}
	return classic_language_uses_english_assets(language) ?
		&new_classic_builtin_english_game_text() :
		&new_classic_builtin_japanese_game_text();
}

std::shared_ptr<const NewClassicCutsceneTextCatalog>
ClassicOpLanguageContent::cutscene_text_for(
	const std::shared_ptr<const NewClassicReplayPatchAssets> &patch_assets,
	ClassicLanguage language
) const noexcept
{
	if(!patch_assets || !patch_assets->custom_cutscene_text) {
		return {};
	}
	const bool japanese_catalog =
		patch_assets->custom_cutscene_text->source_profile_fnv1a64() ==
			new_classic_cutscene_original_profile_fnv1a64;
	if((language == ClassicLanguage::japanese) != japanese_catalog) {
		return {};
	}
	return patch_assets->custom_cutscene_text;
}

std::shared_ptr<const NewClassicPicturePatchCatalog>
ClassicOpLanguageContent::picture_patch_for(
	const std::shared_ptr<const NewClassicReplayPatchAssets> &patch_assets,
	ClassicLanguage language
) const noexcept
{
	if(!patch_assets || !patch_assets->custom_picture_patch) {
		return {};
	}
	const auto *game_text = game_text_for(patch_assets, language);
	if(!game_text || patch_assets->custom_picture_patch->locale() !=
		game_text->locale()) {
		return {};
	}
	return patch_assets->custom_picture_patch;
}

std::expected<void, ClassicOpSessionError>
ClassicOpLanguageContent::apply_language_profile(
	MainResourceLoader &loader,
	const std::shared_ptr<const NewClassicReplayPatchAssets> &patch_assets,
	ClassicLanguage language
)
{
	if(classic_language_uses_english_assets(language) &&
		english_resources_.empty()) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	if(active_language_ == language) {
		return {};
	}
	if(!loader.switch_authenticated_overrides(
		classic_language_uses_english_assets(language) ? english_resources_ :
			std::map<std::string, ResourceBytes>{}
	)) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	const auto cutscene_text = cutscene_text_for(patch_assets, language);
	if(cutscene_text) {
		auto profile = std::uint64_t{14695981039346656037ULL};
		for(std::size_t index = 0U;
			index < new_classic_cutscene_script_count; ++index) {
			auto loaded = loader.load({
				.role = MainResourceRole::cutscene_script,
				.cutscene_script = static_cast<MainCutsceneScript>(index + 1U),
			});
			if(!loaded) {
				return std::unexpected(resource_error(
					std::move(loaded.error())
				));
			}
			const auto bytes = std::span<const std::byte>{**loaded};
			const auto source = std::span<const std::uint8_t>{
				reinterpret_cast<const std::uint8_t *>(bytes.data()),
				bytes.size(),
			};
			for(const auto value : source) {
				profile ^= value;
				profile *= 1099511628211ULL;
			}
			if(auto replacements =
				cutscene_text->replacement_glyphs(index, source);
				!replacements) {
				ClassicStorySessionError nested{
					.code = ClassicStorySessionErrorCode::cutscene_text_failed,
				};
				nested.cutscene_text_error.emplace(
					std::move(replacements.error())
				);
				return std::unexpected(story_error(std::move(nested)));
			}
		}
		if(profile != cutscene_text->source_profile_fnv1a64()) {
			ClassicStorySessionError nested{
				.code = ClassicStorySessionErrorCode::cutscene_text_failed,
			};
			nested.cutscene_text_error.emplace(
				NewClassicCutsceneTextPackError{
					.code = NewClassicCutsceneTextPackErrorCode::source_profile_mismatch,
				}
			);
			return std::unexpected(story_error(std::move(nested)));
		}
	}
	compiled_ = japanese_compiled_;
	stage_text_ = japanese_stage_text_;
	op_music_labels_ = japanese_op_music_labels_;
	if(const auto *game_text = game_text_for(patch_assets, language)) {
		apply_new_classic_english_text(
			compiled_, stage_text_, op_music_labels_, *game_text
		);
	}
	static_content_.reset();
	active_language_ = language;
	return {};
}

std::expected<void, ClassicOpSessionError>
ClassicOpLanguageContent::ensure_static_content(
	MainResourceLoader &loader,
	std::span<const std::byte> font_rom,
	const std::shared_ptr<const NewClassicReplayPatchAssets> &patch_assets,
	ClassicLanguage language
)
{
	if(static_content_) {
		return {};
	}
	auto resources = loader.load_main_static();
	if(!resources) {
		return std::unexpected(resource_error(std::move(resources.error())));
	}
	auto prepared = MainProcessStaticContentAccess::create(
		*resources, compiled_, font_rom, true,
		patch_assets ? game_text_for(patch_assets, language) : nullptr
	);
	if(!prepared) {
		return std::unexpected(static_content_error(
			std::move(prepared.error())
		));
	}
	static_content_ = std::move(*prepared);
	return {};
}

const MainlStageTextAssets &ClassicOpLanguageContent::stage_text() const noexcept
{
	return stage_text_;
}

const OpMusicRoomTextImport &ClassicOpLanguageContent::op_music_labels()
	const noexcept
{
	return op_music_labels_;
}

ClassicLanguage ClassicOpLanguageContent::active_language() const noexcept
{
	return active_language_;
}

bool ClassicOpLanguageContent::english_assets_available() const noexcept
{
	return !english_resources_.empty();
}

const std::shared_ptr<const MainProcessStaticContent> &
ClassicOpLanguageContent::static_content() const noexcept
{
	return static_content_;
}

} // namespace th03::app::detail
