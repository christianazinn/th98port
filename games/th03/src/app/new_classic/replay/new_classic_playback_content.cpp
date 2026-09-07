#include "new_classic/replay/new_classic_playback_content_internal.hpp"

#include "new_classic/content/new_classic_language_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace th03::app {
namespace {

[[nodiscard]] NewClassicPlaybackError playback_error(
	NewClassicPlaybackErrorCode code
) noexcept
{
	return {.code = code};
}

[[nodiscard]] std::expected<std::vector<std::byte>, NewClassicPlaybackError>
read_file(const std::filesystem::path &path)
{
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system || !std::filesystem::is_regular_file(status)) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::file_missing
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::size_t>::max()
	) || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::streamsize>::max()
	)) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::file_read_failed
		));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::file_read_failed
		));
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	if(!bytes.empty()) {
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::file_read_failed
		));
	}
	return bytes;
}

} // namespace

std::expected<PreparedNewClassicPlaybackContent, NewClassicPlaybackError>
prepare_new_classic_playback_content(
	ClassicContentPaths content,
	ClassicLanguage language
)
{
	auto main = read_file(content.main_executable);
	if(!main) {
		return std::unexpected(std::move(main.error()));
	}
	auto compiled = import_main_compiled_content(*main);
	if(!compiled) {
		auto error = playback_error(
			NewClassicPlaybackErrorCode::compiled_content_failed
		);
		error.compiled_content = std::move(compiled.error());
		return std::unexpected(std::move(error));
	}
	auto font = read_file(content.font_rom);
	if(!font) {
		return std::unexpected(std::move(font.error()));
	}
	auto mainl = read_file(content.mainl_executable);
	if(!mainl) {
		return std::unexpected(std::move(mainl.error()));
	}
	auto mainl_text = import_mainl_stage_text(*mainl);
	if(!mainl_text) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::mainl_content_failed
		));
	}
	auto loader = std::make_shared<MainResourceLoader>(
		content.data_root
	);
	std::optional<NewClassicGameTextCatalog> custom_game_text{};
	std::optional<NewClassicCutsceneTextCatalog> custom_cutscene_text{};
	std::optional<NewClassicPicturePatchCatalog> custom_picture_patch{};
	const NewClassicGameTextCatalog *game_text =
		classic_language_uses_english_assets(language) ?
			&new_classic_builtin_english_game_text() :
			&new_classic_builtin_japanese_game_text();
	if(!content.game_text_pack.empty()) {
		if(classic_language_uses_english_assets(language) &&
			content.language_overlay.empty()) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::game_text_pack_failed
			);
			error.game_text_pack = NewClassicGameTextPackError{
				.code = NewClassicGameTextPackErrorCode::fallback_missing,
			};
			return std::unexpected(std::move(error));
		}
		auto loaded = load_new_classic_game_text_pack(
			content.game_text_pack, game_text
		);
		if(!loaded) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::game_text_pack_failed
			);
			error.game_text_pack = std::move(loaded.error());
			return std::unexpected(std::move(error));
		}
		const bool japanese_catalog = loaded->locale() == "ja-JP";
		if(japanese_catalog !=
			(language == ClassicLanguage::japanese)) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::game_text_pack_failed
			);
			error.game_text_pack = NewClassicGameTextPackError{
				.code = NewClassicGameTextPackErrorCode::locale_shape,
			};
			return std::unexpected(std::move(error));
		}
		custom_game_text.emplace(std::move(*loaded));
		game_text = &*custom_game_text;
	}
	if(!content.cutscene_text_pack.empty()) {
		auto loaded = load_new_classic_cutscene_text_pack(
			content.cutscene_text_pack
		);
		if(!loaded) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::cutscene_text_pack_failed
			);
			error.cutscene_text_pack = std::move(loaded.error());
			return std::unexpected(std::move(error));
		}
		const bool japanese_catalog = loaded->source_profile_fnv1a64() ==
			new_classic_cutscene_original_profile_fnv1a64;
		const bool english_catalog = loaded->source_profile_fnv1a64() ==
			new_classic_cutscene_english_profile_fnv1a64;
		const bool selected_japanese =
			language == ClassicLanguage::japanese;
		if((!japanese_catalog && !english_catalog) ||
			japanese_catalog != selected_japanese ||
			(!japanese_catalog && content.language_overlay.empty()) ||
			(!japanese_catalog && (!game_text ||
				loaded->locale() != game_text->locale()))) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::cutscene_text_pack_failed
			);
			error.cutscene_text_pack = NewClassicCutsceneTextPackError{
				.code = NewClassicCutsceneTextPackErrorCode::locale_shape,
			};
			return std::unexpected(std::move(error));
		}
		custom_cutscene_text.emplace(std::move(*loaded));
	}
	if(!content.picture_patch_pack.empty()) {
		auto loaded = load_new_classic_picture_patch(
			content.picture_patch_pack
		);
		if(!loaded) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::picture_patch_pack_failed
			);
			error.picture_patch_pack = std::move(loaded.error());
			return std::unexpected(std::move(error));
		}
		if(!game_text || loaded->locale() != game_text->locale()) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::picture_patch_pack_failed
			);
			error.picture_patch_pack = NewClassicPicturePatchError{
				.code = NewClassicPicturePatchErrorCode::locale_shape,
			};
			return std::unexpected(std::move(error));
		}
		custom_picture_patch.emplace(std::move(*loaded));
	}
	if(classic_language_uses_english_assets(language)) {
		auto overlay = load_new_classic_language_overlay(
			content.language_overlay
		);
		if(!overlay) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::language_overlay_failed
			);
			error.language_overlay.emplace(std::move(overlay.error()));
			return std::unexpected(std::move(error));
		}
		if(!loader->install_authenticated_overrides(
			std::move(*overlay).release_members()
		)) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::language_overlay_failed
			));
		}
		OpMusicRoomTextImport unused_music{};
		apply_new_classic_english_text(
			*compiled, mainl_text->assets, unused_music,
			*game_text
		);
	} else {
		OpMusicRoomTextImport unused_music{};
		apply_new_classic_english_text(
			*compiled, mainl_text->assets, unused_music, *game_text
		);
	}
	if(custom_cutscene_text) {
		auto profile = std::uint64_t{14695981039346656037ULL};
		for(std::size_t index = 0U;
			index < new_classic_cutscene_script_count; ++index) {
			auto loaded = loader->load({
				.role = MainResourceRole::cutscene_script,
				.cutscene_script = static_cast<MainCutsceneScript>(index + 1U),
			});
			if(!loaded) {
				auto error = playback_error(
					NewClassicPlaybackErrorCode::resource_failed
				);
				error.resource = std::move(loaded.error());
				return std::unexpected(std::move(error));
			}
			const auto bytes = std::span<const std::byte>{**loaded};
			const auto source = std::span<const std::uint8_t>{
				reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size()
			};
			for(const auto value : source) {
				profile ^= value;
				profile *= 1099511628211ULL;
			}
			if(auto replacements =
				custom_cutscene_text->replacement_glyphs(index, source);
				!replacements) {
				auto error = playback_error(
					NewClassicPlaybackErrorCode::cutscene_text_pack_failed
				);
				error.cutscene_text_pack = std::move(replacements.error());
				return std::unexpected(std::move(error));
			}
		}
		if(profile != custom_cutscene_text->source_profile_fnv1a64()) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::cutscene_text_pack_failed
			);
			error.cutscene_text_pack = NewClassicCutsceneTextPackError{
				.code = NewClassicCutsceneTextPackErrorCode::source_profile_mismatch,
			};
			return std::unexpected(std::move(error));
		}
	}
	auto static_resources = loader->load_main_static();
	if(!static_resources) {
		auto error = playback_error(NewClassicPlaybackErrorCode::resource_failed);
		error.resource = std::move(static_resources.error());
		return std::unexpected(std::move(error));
	}
	auto static_content = MainProcessStaticContentAccess::create(
		*static_resources, *compiled, *font, true, game_text
	);
	if(!static_content) {
		auto error = playback_error(
			NewClassicPlaybackErrorCode::static_content_failed
		);
		error.process = std::move(static_content.error());
		return std::unexpected(std::move(error));
	}
	if(content.replay_patch_assets.empty()) {
		content.replay_patch_assets =
			content.data_root / "AZINN.DAT";
	}
	auto loaded_patch_assets = load_new_classic_replay_patch_assets(
		content.replay_patch_assets,
		content.new_classic_menu_assets,
		content.font_rom
	);
	if(!loaded_patch_assets) {
		auto error = playback_error(
			NewClassicPlaybackErrorCode::replay_patch_assets_failed
		);
		error.replay_patch_assets.emplace(
			std::move(loaded_patch_assets.error())
		);
		return std::unexpected(std::move(error));
	}
	if(custom_game_text) {
		loaded_patch_assets->custom_game_text =
			std::make_shared<const NewClassicGameTextCatalog>(
				std::move(*custom_game_text)
			);
	}
	if(custom_cutscene_text) {
		loaded_patch_assets->custom_cutscene_text =
			std::make_shared<const NewClassicCutsceneTextCatalog>(
				std::move(*custom_cutscene_text)
			);
	}
	if(custom_picture_patch) {
		loaded_patch_assets->custom_picture_patch =
			std::make_shared<const NewClassicPicturePatchCatalog>(
				std::move(*custom_picture_patch)
			);
	}
	auto patch_assets = std::make_shared<const NewClassicReplayPatchAssets>(
		std::move(*loaded_patch_assets)
	);
	game_text = classic_language_uses_english_assets(language) ?
		(patch_assets->custom_game_text ? patch_assets->custom_game_text.get() :
			&new_classic_builtin_english_game_text()) :
		(patch_assets->custom_game_text ? patch_assets->custom_game_text.get() :
			&new_classic_builtin_japanese_game_text());
	auto legacy_score_path = std::move(content.score);
	auto score_path = legacy_score_path.parent_path() / "YUME.NEW.NEM";
	return PreparedNewClassicPlaybackContent{
		.loader = std::move(loader),
		.compiled_content = std::move(*compiled),
		.font = std::move(*font),
		.static_content = std::move(*static_content),
		.stage_text = std::move(mainl_text->assets),
		.staff_compiled = std::move(mainl_text->staff),
		.score_path = std::move(score_path),
		.legacy_score_path = std::move(legacy_score_path),
		.patch_assets = std::move(patch_assets),
		.game_text = game_text,
	};
}

} // namespace th03::app
