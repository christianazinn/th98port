#include "th03/app/content/resources.hpp"

#include <algorithm>
#include <array>

namespace th03::app {
namespace {

constexpr std::uint8_t first_character_value =
	static_cast<std::uint8_t>(MainCharacter::reimu);
constexpr std::uint8_t last_character_value =
	static_cast<std::uint8_t>(MainCharacter::yumemi);
constexpr std::uint8_t first_palette_value =
	static_cast<std::uint8_t>(MainPaletteVariant::first);
constexpr std::uint8_t last_palette_value =
	static_cast<std::uint8_t>(MainPaletteVariant::second);
constexpr std::size_t music_room_track_count =
	static_cast<std::size_t>(MainMusicRoomTrack::high_score);

[[nodiscard]] constexpr bool is_character(MainCharacter character) noexcept
{
	const auto raw = static_cast<std::uint8_t>(character);
	return raw >= first_character_value && raw <= last_character_value;
}

[[nodiscard]] constexpr bool is_palette(
	MainPaletteVariant palette
) noexcept
{
	const auto raw = static_cast<std::uint8_t>(palette);
	return raw >= first_palette_value && raw <= last_palette_value;
}

[[nodiscard]] ResourceLoadError error(
	ResourceLoadErrorCode code,
	MainResourceKey key
) noexcept
{
	return {.code = code, .key = key};
}

} // namespace

std::vector<MainResourceKey> op_title_menu_resource_keys()
{
	return {
		{.role = MainResourceRole::op_title_long_background},
		{.role = MainResourceRole::op_menu_background},
		{.role = MainResourceRole::op_window_sheet},
		{.role = MainResourceRole::op_gaiji_font},
		{.role = MainResourceRole::op_menu_music},
	};
}

std::vector<MainResourceKey> op_character_selection_resource_keys()
{
	std::vector<MainResourceKey> result;
	result.reserve(16U);
	for(std::uint8_t character = first_character_value;
		character <= static_cast<std::uint8_t>(MainCharacter::marisa);
		++character) {
		result.push_back({
			.role = MainResourceRole::character_selection_portrait,
			.character = static_cast<MainCharacter>(character),
		});
	}
	result.push_back({.role = MainResourceRole::selection_extra_icons});
	for(std::uint8_t character = static_cast<std::uint8_t>(MainCharacter::ellen);
		character <= static_cast<std::uint8_t>(MainCharacter::kana);
		++character) {
		result.push_back({
			.role = MainResourceRole::character_selection_portrait,
			.character = static_cast<MainCharacter>(character),
		});
	}
	result.push_back({.role = MainResourceRole::selection_story_portrait});
	result.push_back({.role = MainResourceRole::selection_stats_sheet});
	result.push_back({.role = MainResourceRole::selection_extra_background});
	result.push_back({.role = MainResourceRole::selection_music});
	result.push_back({.role = MainResourceRole::selection_name_sheet});
	result.push_back({.role = MainResourceRole::selection_palette});
	for(std::uint8_t character = static_cast<std::uint8_t>(MainCharacter::rikako);
		character <= last_character_value;
		++character) {
		result.push_back({
			.role = MainResourceRole::character_selection_portrait,
			.character = static_cast<MainCharacter>(character),
		});
	}
	return result;
}

std::vector<MainResourceKey> op_music_room_resource_keys()
{
	std::vector<MainResourceKey> result{
		{.role = MainResourceRole::op_music_room_background},
		{.role = MainResourceRole::op_music_room_comments},
	};
	result.reserve(2U + music_room_track_count);
	for(std::uint8_t track = 1U;
		track <= static_cast<std::uint8_t>(MainMusicRoomTrack::high_score);
		++track) {
		result.push_back({
			.role = MainResourceRole::op_music_room_track,
			.music_room_track = static_cast<MainMusicRoomTrack>(track),
		});
	}
	return result;
}

std::expected<std::vector<MainResourceKey>, ResourceLoadError>
direct_vs_resource_keys(
	MainPlayerSelection player_one,
	MainPlayerSelection player_two
)
{
	constexpr std::array static_roles{
		MainResourceRole::enemy_scripts,
		MainResourceRole::game_font,
		MainResourceRole::hud_lose_sheet,
		MainResourceRole::hud_round_sheet,
		MainResourceRole::hud_result_sheet,
		MainResourceRole::stage_enemy_center,
	};
	constexpr std::array character_roles{
		MainResourceRole::character_sheet,
		MainResourceRole::character_extra_portrait,
		MainResourceRole::character_bomb_portrait,
		MainResourceRole::character_stage_shot,
		MainResourceRole::character_stage_extra_shot,
	};
	if(!is_character(player_one.character) || !is_palette(player_one.palette)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key,
			{.character = player_one.character, .palette = player_one.palette}
		));
	}
	if(!is_character(player_two.character) || !is_palette(player_two.palette)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key,
			{.character = player_two.character, .palette = player_two.palette}
		));
	}

	std::vector<MainResourceKey> result;
	result.reserve(static_roles.size() + (2U * character_roles.size()) + 1U);
	for(const auto role : static_roles) {
		result.push_back({.role = role});
	}
	for(const auto player : {player_one, player_two}) {
		for(const auto role : character_roles) {
			const MainResourceKey key{
				.role = role,
				.character = player.character,
				.palette = player.palette,
			};
			if(std::find(result.begin(), result.end(), key) == result.end()) {
				result.push_back(key);
			}
		}
	}
	result.push_back({
		.role = MainResourceRole::character_stage_enemy_group,
		.character = player_two.character,
	});
	return result;
}

std::expected<std::vector<MainResourceKey>, ResourceLoadError>
main_private_resource_keys(
	MainPlayerSelection player_one,
	MainPlayerSelection player_two
)
{
	constexpr std::array character_roles{
		MainResourceRole::character_sheet,
		MainResourceRole::character_extra_portrait,
		MainResourceRole::character_bomb_portrait,
	};
	if(!is_character(player_one.character) || !is_palette(player_one.palette)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key,
			{.character = player_one.character, .palette = player_one.palette}
		));
	}
	if(!is_character(player_two.character) || !is_palette(player_two.palette)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key,
			{.character = player_two.character, .palette = player_two.palette}
		));
	}

	std::vector<MainResourceKey> result;
	result.reserve(2U * character_roles.size());
	for(const auto player : {player_one, player_two}) {
		for(const auto role : character_roles) {
			const MainResourceKey key{
				.role = role,
				.character = player.character,
				.palette = player.palette,
			};
			if(std::find(result.begin(), result.end(), key) == result.end()) {
				result.push_back(key);
			}
		}
	}
	return result;
}

std::vector<MainResourceKey> main_static_resource_keys()
{
	return {
		{.role = MainResourceRole::enemy_scripts},
		{.role = MainResourceRole::game_font},
		{.role = MainResourceRole::hud_lose_sheet},
		{.role = MainResourceRole::hud_round_sheet},
		{.role = MainResourceRole::hud_result_sheet},
	};
}

std::expected<std::vector<MainResourceKey>, ResourceLoadError>
mainl_stage_resource_keys(MainlStageResourceRequest request)
{
	if(!is_character(request.player_one.character) ||
		!is_palette(request.player_one.palette)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key,
			{
				.character = request.player_one.character,
				.palette = request.player_one.palette,
			}
		));
	}
	if(!is_character(request.player_two.character) ||
		!is_palette(request.player_two.palette) ||
		(request.game_mode == 0x01U && request.story_stage > 8U)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key,
			{
				.character = request.player_two.character,
				.palette = request.player_two.palette,
			}
		));
	}

	MainlStageBackground background = MainlStageBackground::non_story;
	if(request.game_mode == 0x01U) {
		if(request.player_two.character == MainCharacter::chiyuri) {
			background = MainlStageBackground::chiyuri;
		} else if(request.player_two.character == MainCharacter::yumemi) {
			background = MainlStageBackground::yumemi;
		} else if(request.story_stage == 6U) {
			background = MainlStageBackground::decisive;
		} else {
			background = MainlStageBackground::numbered;
		}
	}

	std::vector<MainResourceKey> result;
	result.reserve(13U);
	result.push_back({.role = MainResourceRole::mainl_gaiji_font});
	for(const auto player : {request.player_one, request.player_two}) {
		const MainResourceKey key{
			.role = MainResourceRole::character_stage_portrait,
			.character = player.character,
		};
		if(std::find(result.begin(), result.end(), key) == result.end()) {
			result.push_back(key);
		}
	}
	if(background == MainlStageBackground::numbered) {
		result.push_back({.role = MainResourceRole::stage_number_sheet});
	}
	result.push_back({.role = MainResourceRole::stage_splash_base});
	result.push_back({
		.role = MainResourceRole::stage_splash_background,
		.stage_background = background,
	});
	for(const auto player : {request.player_one, request.player_two}) {
		for(const auto role : {
			MainResourceRole::character_stage_shot,
			MainResourceRole::character_stage_extra_shot,
		}) {
			const MainResourceKey key{
				.role = role,
				.character = player.character,
				.palette = player.palette,
			};
			if(std::find(result.begin(), result.end(), key) == result.end()) {
				result.push_back(key);
			}
		}
	}
	result.push_back({.role = MainResourceRole::stage_enemy_center});
	result.push_back({
		.role = MainResourceRole::character_stage_enemy_group,
		.character = request.player_two.character,
	});
	return result;
}

std::expected<std::vector<MainResourceKey>, ResourceLoadError>
mainl_win_resource_keys(MainPlayerSelection winner)
{
	if(!is_character(winner.character) || !is_palette(winner.palette)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key,
			{.character = winner.character, .palette = winner.palette}
		));
	}
	return std::vector<MainResourceKey>{
		{.role = MainResourceRole::win_logo_palette_pre},
		{.role = MainResourceRole::win_logo_fade_cels},
		{.role = MainResourceRole::win_logo_final},
		{
			.role = MainResourceRole::character_win_portrait,
			.character = winner.character,
		},
		{
			.role = MainResourceRole::character_win_message,
			.character = winner.character,
		},
		{.role = MainResourceRole::win_logo_palette_post},
	};
}

std::expected<std::vector<MainResourceKey>, ResourceLoadError>
mainl_continue_resource_keys(std::uint8_t remaining_credits)
{
	if(remaining_credits > 3U) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key,
			{.role = MainResourceRole::continue_background}
		));
	}
	if(remaining_credits == 0U) {
		return std::vector<MainResourceKey>{
			{.role = MainResourceRole::gameover_background},
		};
	}
	return std::vector<MainResourceKey>{
		{.role = MainResourceRole::continue_background},
		{.role = MainResourceRole::continue_choice_cels},
	};
}

std::vector<MainResourceKey> mainl_gameover_resource_keys()
{
	return {{.role = MainResourceRole::gameover_background}};
}

std::expected<std::vector<MainResourceKey>, ResourceLoadError>
mainl_cutscene_resource_keys(MainlCutsceneResourceRequest request)
{
	if(!is_character(request.character)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key,
			{.role = MainResourceRole::cutscene_script}
		));
	}

	const auto character_index = static_cast<std::uint8_t>(
		static_cast<std::uint8_t>(request.character) - first_character_value
	);
	MainCutsceneScript script = MainCutsceneScript::none;
	switch(request.kind) {
	case MainlCutsceneKind::interstage_chiyuri:
		script = static_cast<MainCutsceneScript>(1U + character_index);
		break;
	case MainlCutsceneKind::interstage_yumemi:
		script = static_cast<MainCutsceneScript>(10U + character_index);
		break;
	case MainlCutsceneKind::ending:
		script = static_cast<MainCutsceneScript>(19U + character_index);
		break;
	case MainlCutsceneKind::extra_ending:
		script = MainCutsceneScript::extra_ending;
		break;
	default:
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key,
			{.role = MainResourceRole::cutscene_script}
		));
	}

	std::vector<MainResourceKey> result;
	result.reserve(31U);
	result.push_back({.role = MainResourceRole::mainl_gaiji_font});
	result.push_back({
		.role = MainResourceRole::cutscene_script,
		.cutscene_script = script,
	});
	for(std::uint8_t picture = 1U;
		picture <= static_cast<std::uint8_t>(MainCutscenePicture::dm4a);
		++picture) {
		result.push_back({
			.role = MainResourceRole::cutscene_picture,
			.cutscene_picture = static_cast<MainCutscenePicture>(picture),
		});
	}
	for(std::uint8_t music = 1U;
		music <= static_cast<std::uint8_t>(MainCutsceneMusic::demo5);
		++music) {
		result.push_back({
			.role = MainResourceRole::cutscene_music,
			.cutscene_music = static_cast<MainCutsceneMusic>(music),
		});
	}
	return result;
}

std::vector<MainResourceKey> mainl_staff_resource_keys()
{
	std::vector<MainResourceKey> result;
	result.reserve(14U);
	result.push_back({.role = MainResourceRole::staff_background_palette});
	for(std::uint8_t raw = 1U;
		raw <= static_cast<std::uint8_t>(MainStaffCdg::staff_12);
		++raw) {
		result.push_back({
			.role = MainResourceRole::staff_cdg,
			.staff_cdg = static_cast<MainStaffCdg>(raw),
		});
	}
	result.push_back({.role = MainResourceRole::staff_music});
	return result;
}

std::expected<std::vector<MainResourceKey>, ResourceLoadError>
mainl_registration_resource_keys(MainlRegistrationResourceRequest request)
{
	const auto rank = request.rank;
	if(rank < MainRegistrationRank::easy || rank > MainRegistrationRank::lunatic) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key,
			{.role = MainResourceRole::registration_rank,
				.registration_rank = rank}
		));
	}
	std::vector<MainResourceKey> keys{
		{.role = MainResourceRole::registration_background},
		{.role = MainResourceRole::registration_rank,
			.registration_rank = rank},
		{.role = MainResourceRole::registration_normal_glyphs},
		{.role = MainResourceRole::registration_highlight_glyphs},
		{.role = MainResourceRole::registration_music},
	};
	switch(request.next_screen) {
	case MainlRegistrationNextScreen::continue_menu:
		keys.push_back({.role = MainResourceRole::continue_background});
		keys.push_back({.role = MainResourceRole::continue_choice_cels});
		break;
	case MainlRegistrationNextScreen::game_over:
		keys.push_back({.role = MainResourceRole::gameover_background});
		break;
	default:
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key,
			{.role = MainResourceRole::registration_background}
		));
	}
	return keys;
}


} // namespace th03::app
