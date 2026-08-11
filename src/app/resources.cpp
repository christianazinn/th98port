#include "th03/app/resources.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

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

// These fingerprints were recorded from the pinned, read-only Anniversary
// extraction. They are integrity metadata only; no original bytes live here.
constexpr std::array<std::uint64_t, 18U> character_sheet_fnv1a64{
	0xF151189FFA599238ULL, 0x84874082D6C1A133ULL,
	0x55B637A90AAC1D59ULL, 0x54A135DE0639011BULL,
	0x70CDB80B729A9AEFULL, 0xF4DDB0736B6D7E91ULL,
	0x869C04C68DE4915AULL, 0x2943BA110E72157AULL,
	0xC083509A9AE1C059ULL, 0x1F7C9D716D044F06ULL,
	0x2A7554C4AC37C3F0ULL, 0x9EB550758AF05165ULL,
	0x9236906EFA4725BBULL, 0x8CA5F16880A8FF55ULL,
	0x0F20C8B74FF03632ULL, 0x274EEFEF2C97A69AULL,
	0x9066919A906DFEB6ULL, 0x74F777F93BB81C2FULL,
};
constexpr std::array<std::uint64_t, 18U> character_extra_fnv1a64{
	0xFE227804BE7ECC78ULL, 0x2E36D7EFFAFE91C3ULL,
	0x6B547BA0A16F7B9FULL, 0xE6100897AE1DBF07ULL,
	0xAE1CAE13DD87A06BULL, 0xDE9BBE5A219C5CB6ULL,
	0x243AA9E78E01DCD9ULL, 0x504335DA6936A5B5ULL,
	0x9B8BA5A21DEA004DULL, 0xD7B6D1C9D712394EULL,
	0x86746485D117DDA6ULL, 0x54FF33BF007E5F48ULL,
	0xC13A0C44078B5FEAULL, 0xD4D4F7E084037532ULL,
	0x8351DA5408665DB4ULL, 0xBE1968E1F5443000ULL,
	0xA16EF731178A8974ULL, 0x5822FE9FF2D75D28ULL,
};
constexpr std::array<std::uint64_t, 18U> character_bomb_fnv1a64{
	0x210F00FF0BBEE70FULL, 0xDB906427D8225633ULL,
	0x1CCEF5D46567BF3CULL, 0x2BFD77289BC45C37ULL,
	0x1885B8435BF9BF8AULL, 0xD5A7BF07E35EBD53ULL,
	0x3BB3F2E3F735F9E4ULL, 0x3BB3F2E3F735F9E4ULL,
	0x7B4C5AC1B0687997ULL, 0x5B2096D98FFE01F4ULL,
	0x086AA23A786DA624ULL, 0xDB786EA1D7B7436EULL,
	0x1EAD707E636C5088ULL, 0x8BDCFC523F4AB2B8ULL,
	0xE59FE56C1FC8AD68ULL, 0x4E6E19523AD551C9ULL,
	0x5A8DB5549DC26FC1ULL, 0x6D66AA44227E54BEULL,
};
constexpr std::array<std::uint64_t, 18U> character_stage_shot_fnv1a64{
	0xEAC1ECE9A328A708ULL, 0x1A1D4A006B08ABE2ULL,
	0x9F7B678F78B1AEF6ULL, 0x64050266B42A53DBULL,
	0x597B460C28772004ULL, 0x069178CE5B5ECFBEULL,
	0x9254AD4D7BBEDBACULL, 0xBE8126D1C35275C7ULL,
	0x60C765A060519DD0ULL, 0x3CEF927E0EEC0FD2ULL,
	0x8B09E6F0777A2340ULL, 0x3B0DAEA7E828D363ULL,
	0x0D47CF9D9F03F0E1ULL, 0x0BCEB8AEAC206D1BULL,
	0xA3E4693CB4D57B9DULL, 0x587F523AC206B51CULL,
	0xAD308AEEA47FD178ULL, 0x6143C688B2B9CCC3ULL,
};
constexpr std::array<std::uint64_t, 18U> character_stage_extra_shot_fnv1a64{
	0xDC6B1C35ED1A67ACULL, 0xC78E059DB58914E9ULL,
	0x73508D5D074BA292ULL, 0xD4C1347F2C97B122ULL,
	0xA639E50E834A618EULL, 0x408B7C50A33938A4ULL,
	0x38F19A645AE6878BULL, 0x317CD8576A56F334ULL,
	0x860F4FE3C870ACC8ULL, 0x3282B7A041B69EC9ULL,
	0x3F4F6CA5C539BE8BULL, 0x74C807655DDA9758ULL,
	0x36E0ABACE6AF230AULL, 0x1E6825B2234C2375ULL,
	0xAA08ABC1CB2F85C5ULL, 0xAB155F35293C2544ULL,
	0x60A0C0478B825DA7ULL, 0x48942A4E63523010ULL,
};
constexpr std::array<std::uint64_t, 5U> stage_enemy_group_fnv1a64{
	0x9E080DAAA809D13BULL, 0x720E44C9C7EFB435ULL,
	0x57829DA41AE90F86ULL, 0x0041BE7AD009BC24ULL,
	0x13FA6F425CCC8E13ULL,
};
constexpr std::array<std::size_t, 5U> stage_enemy_group_sizes{
	7378U, 7295U, 7064U, 7046U, 7035U,
};
constexpr std::array<std::uint64_t, 9U> stage_portrait_fnv1a64{
	0x2FBFB6DD25A59078ULL, 0x6582FF1CC9AF3A67ULL,
	0x938479CE49117012ULL, 0x4D7F54E3CA57D012ULL,
	0x18BAE1588F40D36AULL, 0xF98FB21F1FA2770CULL,
	0x4D25CE4120D055A6ULL, 0x8E821C13740C9AAEULL,
	0x4CAE741B35FC7549ULL,
};
constexpr std::array<std::uint64_t, 5U> stage_background_fnv1a64{
	0xD1EB015A5715A5B2ULL, 0x1A69A90D9A5FD12CULL,
	0x78B58FCF1C113EE9ULL, 0x58BDC78F7B0A8C75ULL,
	0x2A0E4DB97D2FF437ULL,
};
constexpr std::array<std::size_t, 5U> stage_background_sizes{
	1231U, 2057U, 2831U, 2901U, 2058U,
};
constexpr std::array<std::uint64_t, 9U> win_message_fnv1a64{
	0x2FC63AB673BBAF54ULL, 0x5B508A83F961E8EAULL,
	0x7EB1575BD10DF193ULL, 0x09184A8B4FA5CE29ULL,
	0x44C84E89632B9254ULL, 0xDB9CAAFA845B6A67ULL,
	0x14B79980B19991E3ULL, 0x854B23F7A2FC337DULL,
	0x00B4CF7FF4B8B1F6ULL,
};
constexpr std::array<std::size_t, 28U> cutscene_script_sizes{
	4770U, 4493U, 4491U, 3968U, 3950U, 4571U, 4139U, 1741U, 1943U,
	1885U, 1776U, 1671U, 1620U, 1532U, 1560U, 1816U, 866U, 578U,
	4425U, 2670U, 2597U, 2532U, 2812U, 2426U, 2376U, 1611U, 1687U,
	615U,
};
constexpr std::array<std::uint64_t, 28U> cutscene_script_fnv1a64{
	0x1C62B85E545EFC43ULL, 0xB90E857354BFC7EFULL,
	0x31856B3B3E0D98D6ULL, 0xE858076EC09B1BD4ULL,
	0xE1C94C595969E2B3ULL, 0x3D18976659C56195ULL,
	0xD979B052294FF2BFULL, 0x3941A2BBC726B0ADULL,
	0xD465EBCFCC3EDDB7ULL, 0xD79842B8FE74525CULL,
	0x7D5207E437659773ULL, 0x635CB362189D99DFULL,
	0x8D0895C111C1E328ULL, 0xCA90ABF223D60B96ULL,
	0xA643857FF74153D1ULL, 0xFDA66BB0D06829CDULL,
	0xBC61861FEA92ED1DULL, 0x0644C27BCA06A927ULL,
	0xF1081E2B559ECEB5ULL, 0x25EC2B265905376BULL,
	0x2425CE05683E8A67ULL, 0x519EE4D7973979E2ULL,
	0x12DD87A7920C8E0BULL, 0x4C0336C38244F062ULL,
	0x749AA230F0A078CEULL, 0x772E4335C445E69AULL,
	0x1B415E91DACA930AULL, 0xDC89B80A10BE849CULL,
};
constexpr std::array<std::string_view, 24U> cutscene_picture_filenames{
	"DM1A.PI", "DM1B.PI", "DM1C.PI", "DM1D.PI", "DM1E.PI", "DM1F.PI",
	"DM2A.PI",
	"DM3A.PI", "DM3B.PI", "DM3C.PI", "DM3D.PI", "DM3E.PI", "DM3F.PI",
	"DM3G.PI", "DM3H.PI", "DM3I.PI", "DM3J.PI", "DM3K.PI", "DM3L.PI",
	"DM3M.PI", "DM3N.PI", "DM3O.PI", "DM3P.PI",
	"DM4A.PI",
};
constexpr std::array<std::size_t, 24U> cutscene_picture_sizes{
	4652U, 6922U, 11335U, 8032U, 19686U, 20612U,
	25510U,
	31219U, 25365U, 46445U, 16766U, 35710U, 30599U, 32247U,
	23546U, 44898U, 33620U, 28267U, 18283U, 27358U, 34717U, 38614U,
	20050U,
	22284U,
};
constexpr std::array<std::uint64_t, 24U> cutscene_picture_fnv1a64{
	0xB68BF7AF62C15F99ULL, 0x5A39335B52B45A99ULL,
	0x43794198AF99BDA5ULL, 0x182DB303918A4F52ULL,
	0xB0657EE93C5249E0ULL, 0x8E5F014DC71E6283ULL,
	0x593AE8D4BD294393ULL,
	0xE99C792E21319DB2ULL, 0x51EB7C9197955CD8ULL,
	0xE0F2108D49190B87ULL, 0xC7FD8D94D0D1A4D2ULL,
	0x475B01ED6651E9B4ULL, 0x46AEC3918A9B6C94ULL,
	0x6C89AE11F85F4DBDULL, 0x2A94CBA3D6FCC184ULL,
	0x20BEE5F24E966D22ULL, 0x290F2B48900034B4ULL,
	0x5E95ABC40D3D88BEULL, 0x65FD0E6BCF6E5219ULL,
	0x80BD496DC66B5A85ULL, 0x2E588F93A5C5F1ABULL,
	0x867476B58A43F14BULL, 0xDE3EB9C13D9E074AULL,
	0x0B9A1D3A5AECEFD1ULL,
};
constexpr std::array<std::string_view, 5U> cutscene_music_filenames{
	"DEMO1.M", "DEMO2.M", "DEMO3.M", "DEMO4.M", "DEMO5.M",
};
constexpr std::array<std::size_t, 5U> cutscene_music_sizes{
	2319U, 1145U, 1284U, 821U, 1193U,
};
constexpr std::array<std::uint64_t, 5U> cutscene_music_fnv1a64{
	0xA479347AED2FCD59ULL, 0x5B478D191785CCC0ULL,
	0xF9A18439B7621C60ULL, 0xB6C760DD37D074CAULL,
	0x1E13270E55263E7EULL,
};
constexpr std::array<std::string_view, 12U> staff_cdg_filenames{
	"STF1.CDG", "STF11.CDG", "STF3.CDG", "STF4.CDG",
	"STF5.CDG", "STF6.CDG", "STF7.CDG", "STF8.CDG",
	"STF9.CDG", "STF10.CDG", "STF2.CDG", "STF12.CDG",
};
constexpr std::array<std::size_t, 12U> staff_cdg_sizes{
	7696U, 6256U, 8336U, 8016U, 5136U, 10256U,
	8976U, 36816U, 4336U, 11056U, 4336U, 5056U,
};
constexpr std::array<std::uint64_t, 12U> staff_cdg_fnv1a64{
	0xC104BD8D7F6FDC09ULL, 0xE55421CDB1960A92ULL,
	0x62C8662D33190286ULL, 0x7DEC24541A3FF49EULL,
	0x16E04971B5051CE6ULL, 0x41AF01068D34C493ULL,
	0xECA0CBEE7287BF22ULL, 0xE087C778D4A2C46FULL,
	0x73CE907471A15C66ULL, 0xC764918F00CAD525ULL,
	0x5D131B0A6A9FCD84ULL, 0xD15C59C7A26437CAULL,
};

[[nodiscard]] constexpr bool is_character(MainCharacter character) noexcept
{
	const auto value = static_cast<std::uint8_t>(character);
	return value >= first_character_value && value <= last_character_value;
}

[[nodiscard]] constexpr bool is_palette(
	MainPaletteVariant palette
) noexcept
{
	const auto value = static_cast<std::uint8_t>(palette);
	return value >= first_palette_value && value <= last_palette_value;
}

[[nodiscard]] constexpr bool is_paletted_character_role(
	MainResourceRole role
) noexcept
{
	return role == MainResourceRole::character_sheet ||
		role == MainResourceRole::character_extra_portrait ||
		role == MainResourceRole::character_bomb_portrait ||
		role == MainResourceRole::character_stage_shot ||
		role == MainResourceRole::character_stage_extra_shot;
}

[[nodiscard]] constexpr bool is_character_only_role(
	MainResourceRole role
) noexcept
{
	return role == MainResourceRole::character_stage_enemy_group ||
		role == MainResourceRole::character_stage_portrait ||
		role == MainResourceRole::character_win_portrait ||
		role == MainResourceRole::character_win_message;
}

[[nodiscard]] constexpr bool is_stage_background(
	MainlStageBackground background
) noexcept
{
	return background >= MainlStageBackground::numbered &&
		background <= MainlStageBackground::non_story;
}

[[nodiscard]] constexpr bool has_cutscene_selector(
	const MainResourceKey &key
) noexcept
{
	return key.cutscene_script != MainCutsceneScript::none ||
		key.cutscene_picture != MainCutscenePicture::none ||
		key.cutscene_music != MainCutsceneMusic::none;
}

[[nodiscard]] constexpr bool has_staff_selector(
	const MainResourceKey &key
) noexcept
{
	return key.staff_cdg != MainStaffCdg::none;
}

[[nodiscard]] constexpr bool has_registration_selector(
	const MainResourceKey &key
) noexcept
{
	return key.registration_rank != MainRegistrationRank::none;
}

[[nodiscard]] constexpr std::uint8_t stage_enemy_group_index(
	MainCharacter character
) noexcept
{
	switch(character) {
	case MainCharacter::reimu:
	case MainCharacter::mima:
	case MainCharacter::rikako:
		return 0U;
	case MainCharacter::marisa:
	case MainCharacter::kotohime:
		return 1U;
	case MainCharacter::ellen:
	case MainCharacter::kana:
		return 2U;
	case MainCharacter::chiyuri:
		return 3U;
	case MainCharacter::yumemi:
		return 4U;
	case MainCharacter::none:
		break;
	}
	return 0U;
}

[[nodiscard]] constexpr std::uint8_t character_file_index(
	MainCharacter character,
	MainPaletteVariant palette
) noexcept
{
	return static_cast<std::uint8_t>(
		((static_cast<std::uint8_t>(character) - first_character_value) * 2U) +
		(static_cast<std::uint8_t>(palette) - first_palette_value)
	);
}

[[nodiscard]] constexpr std::uint64_t character_fnv1a64(
	MainResourceRole role,
	std::uint8_t index
) noexcept
{
	switch(role) {
	case MainResourceRole::character_sheet:
		return character_sheet_fnv1a64[index];
	case MainResourceRole::character_extra_portrait:
		return character_extra_fnv1a64[index];
	case MainResourceRole::character_bomb_portrait:
		return character_bomb_fnv1a64[index];
	case MainResourceRole::character_stage_shot:
		return character_stage_shot_fnv1a64[index];
	case MainResourceRole::character_stage_extra_shot:
		return character_stage_extra_shot_fnv1a64[index];
	case MainResourceRole::op_title_long_background:
	case MainResourceRole::op_menu_background:
	case MainResourceRole::op_window_sheet:
	case MainResourceRole::op_gaiji_font:
	case MainResourceRole::op_menu_music:
	case MainResourceRole::enemy_scripts:
	case MainResourceRole::game_font:
	case MainResourceRole::hud_lose_sheet:
	case MainResourceRole::hud_round_sheet:
	case MainResourceRole::hud_result_sheet:
	case MainResourceRole::stage_enemy_center:
	case MainResourceRole::selection_palette:
	case MainResourceRole::character_stage_enemy_group:
	case MainResourceRole::mainl_gaiji_font:
	case MainResourceRole::character_stage_portrait:
	case MainResourceRole::stage_number_sheet:
	case MainResourceRole::stage_splash_base:
	case MainResourceRole::stage_splash_background:
	case MainResourceRole::win_logo_palette_pre:
	case MainResourceRole::win_logo_fade_cels:
	case MainResourceRole::win_logo_final:
	case MainResourceRole::win_logo_palette_post:
	case MainResourceRole::character_win_portrait:
	case MainResourceRole::character_win_message:
	case MainResourceRole::continue_background:
	case MainResourceRole::continue_choice_cels:
	case MainResourceRole::gameover_background:
	case MainResourceRole::cutscene_script:
	case MainResourceRole::cutscene_picture:
	case MainResourceRole::cutscene_music:
	case MainResourceRole::staff_background_palette:
	case MainResourceRole::staff_cdg:
	case MainResourceRole::staff_music:
	case MainResourceRole::registration_background:
	case MainResourceRole::registration_rank:
	case MainResourceRole::registration_normal_glyphs:
	case MainResourceRole::registration_highlight_glyphs:
	case MainResourceRole::registration_music:
		return 0U;
	}
	return 0U;
}

[[nodiscard]] constexpr std::string_view character_name(
	MainCharacter character
) noexcept
{
	switch(character) {
	case MainCharacter::reimu: return "reimu";
	case MainCharacter::mima: return "mima";
	case MainCharacter::marisa: return "marisa";
	case MainCharacter::ellen: return "ellen";
	case MainCharacter::kotohime: return "kotohime";
	case MainCharacter::kana: return "kana";
	case MainCharacter::rikako: return "rikako";
	case MainCharacter::chiyuri: return "chiyuri";
	case MainCharacter::yumemi: return "yumemi";
	case MainCharacter::none: return "none";
	}
	return "invalid";
}

[[nodiscard]] std::string character_only_logical_role(
	MainResourceKey key,
	std::string_view role
)
{
	std::string result{"main.character."};
	result.append(character_name(key.character));
	result.push_back('.');
	result.append(role);
	return result;
}

[[nodiscard]] std::string stage_enemy_group_filename(std::uint8_t group)
{
	std::string result{"ENEMY0"};
	result.push_back(static_cast<char>('0' + group));
	result.append(".PI");
	return result;
}

[[nodiscard]] std::string stage_background_filename(
	MainlStageBackground background
)
{
	std::string result{"STNX"};
	result.push_back(static_cast<char>(
		'0' + static_cast<std::uint8_t>(background)
	));
	result.append(".PI");
	return result;
}

[[nodiscard]] std::string filename_with_index(
	std::uint8_t index,
	std::string_view suffix
)
{
	std::string result;
	result.reserve(2U + suffix.size());
	result.push_back(static_cast<char>('0' + (index / 10U)));
	result.push_back(static_cast<char>('0' + (index % 10U)));
	result.append(suffix);
	return result;
}

[[nodiscard]] std::string character_logical_role(
	MainResourceKey key,
	std::string_view role
)
{
	std::string result{"main.character."};
	result.append(character_name(key.character));
	result.append(".paletted_");
	result.append(
		(key.palette == MainPaletteVariant::first) ? "0." : "1."
	);
	result.append(role);
	return result;
}

[[nodiscard]] ResourceLoadError error(
	ResourceLoadErrorCode code,
	MainResourceKey key
) noexcept
{
	return {.code = code, .key = key};
}

[[nodiscard]] std::expected<MainResourceSpec, ResourceLoadError>
cutscene_resource_spec(MainResourceKey key)
{
	if(key.character != MainCharacter::none ||
		key.palette != MainPaletteVariant::none ||
		key.stage_background != MainlStageBackground::none ||
		has_staff_selector(key) || has_registration_selector(key)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key, key
		));
	}

	if(key.role == MainResourceRole::cutscene_script) {
		const auto raw = static_cast<std::uint8_t>(key.cutscene_script);
		if(raw == 0U || raw > static_cast<std::uint8_t>(
			MainCutsceneScript::extra_ending
		) || key.cutscene_picture != MainCutscenePicture::none ||
			key.cutscene_music != MainCutsceneMusic::none) {
			return std::unexpected(error(
				ResourceLoadErrorCode::invalid_resource_key, key
			));
		}
		const auto index = static_cast<std::size_t>(raw - 1U);
		std::string filename{};
		if(index == 27U) {
			filename = "@99ED.TXT";
		} else {
			const auto character = static_cast<std::uint8_t>(index % 9U);
			filename = "@0" + std::to_string(character);
			if(index < 9U) {
				filename += "DM0.TXT";
			} else if(index < 18U) {
				filename += "DM1.TXT";
			} else {
				filename += "ED.TXT";
			}
		}
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_cutscene,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::cutscene_script,
			.logical_role = "mainl.cutscene.script." + std::to_string(index),
			.filename = std::move(filename),
			.expected_bytes = cutscene_script_sizes[index],
			.expected_fnv1a64 = cutscene_script_fnv1a64[index],
		};
	}

	if(key.role == MainResourceRole::cutscene_picture) {
		const auto raw = static_cast<std::uint8_t>(key.cutscene_picture);
		if(raw == 0U || raw > static_cast<std::uint8_t>(
			MainCutscenePicture::dm4a
		) || key.cutscene_script != MainCutsceneScript::none ||
			key.cutscene_music != MainCutsceneMusic::none) {
			return std::unexpected(error(
				ResourceLoadErrorCode::invalid_resource_key, key
			));
		}
		const auto index = static_cast<std::size_t>(raw - 1U);
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_cutscene,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pi,
			.logical_role = "mainl.cutscene.picture." + std::to_string(index),
			.filename = std::string{cutscene_picture_filenames[index]},
			.expected_bytes = cutscene_picture_sizes[index],
			.expected_fnv1a64 = cutscene_picture_fnv1a64[index],
		};
	}

	if(key.role == MainResourceRole::cutscene_music) {
		const auto raw = static_cast<std::uint8_t>(key.cutscene_music);
		if(raw == 0U || raw > static_cast<std::uint8_t>(
			MainCutsceneMusic::demo5
		) || key.cutscene_script != MainCutsceneScript::none ||
			key.cutscene_picture != MainCutscenePicture::none) {
			return std::unexpected(error(
				ResourceLoadErrorCode::invalid_resource_key, key
			));
		}
		const auto index = static_cast<std::size_t>(raw - 1U);
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_cutscene,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pmd_music,
			.logical_role = "mainl.cutscene.music." + std::to_string(index),
			.filename = std::string{cutscene_music_filenames[index]},
			.expected_bytes = cutscene_music_sizes[index],
			.expected_fnv1a64 = cutscene_music_fnv1a64[index],
		};
	}

	return std::unexpected(error(ResourceLoadErrorCode::invalid_resource_key, key));
}

[[nodiscard]] std::expected<MainResourceSpec, ResourceLoadError>
staff_resource_spec(MainResourceKey key)
{
	if(key.character != MainCharacter::none ||
		key.palette != MainPaletteVariant::none ||
		key.stage_background != MainlStageBackground::none ||
		has_cutscene_selector(key) || has_registration_selector(key)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key, key
		));
	}
	if(key.role == MainResourceRole::staff_cdg) {
		const auto raw = static_cast<std::uint8_t>(key.staff_cdg);
		if(raw == 0U || raw > static_cast<std::uint8_t>(MainStaffCdg::staff_12)) {
			return std::unexpected(error(
				ResourceLoadErrorCode::invalid_resource_key, key
			));
		}
		const auto index = static_cast<std::size_t>(raw - 1U);
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_staff,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::cdg,
			.logical_role = "mainl.staff.cdg." + std::to_string(index),
			.filename = std::string{staff_cdg_filenames[index]},
			.expected_bytes = staff_cdg_sizes[index],
			.expected_fnv1a64 = staff_cdg_fnv1a64[index],
		};
	}
	if(has_staff_selector(key)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key, key
		));
	}
	if(key.role == MainResourceRole::staff_background_palette) {
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_staff,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::rgb48,
			.logical_role = "mainl.staff.background_palette",
			.filename = "EDBK1.RGB",
			.expected_bytes = 48U,
			.expected_fnv1a64 = 0x5305C69FE55F8683ULL,
		};
	}
	if(key.role == MainResourceRole::staff_music) {
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_staff,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pmd_music,
			.logical_role = "mainl.staff.music",
			.filename = "ED.M",
			.expected_bytes = 3378U,
			.expected_fnv1a64 = 0x897797694DA73B83ULL,
		};
	}
	return std::unexpected(error(ResourceLoadErrorCode::invalid_resource_key, key));
}

[[nodiscard]] std::expected<MainResourceSpec, ResourceLoadError>
registration_resource_spec(MainResourceKey key)
{
	if(key.character != MainCharacter::none ||
		key.palette != MainPaletteVariant::none ||
		key.stage_background != MainlStageBackground::none ||
		has_cutscene_selector(key) || has_staff_selector(key)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key, key
		));
	}
	if(key.role == MainResourceRole::registration_rank) {
		const auto raw = static_cast<std::uint8_t>(key.registration_rank);
		if(raw == 0U || raw > static_cast<std::uint8_t>(
			MainRegistrationRank::lunatic
		)) {
			return std::unexpected(error(
				ResourceLoadErrorCode::invalid_resource_key, key
			));
		}
		constexpr std::array fnv1a64{
			0x307D700DB4D69C05ULL, 0x507886929B535358ULL,
			0x6995CE65044F0186ULL, 0x558F9EEA22AAA179ULL,
		};
		const auto index = static_cast<std::size_t>(raw - 1U);
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_registration,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::cdg,
			.logical_role = "mainl.registration.rank." + std::to_string(index),
			.filename = "RFT" + std::to_string(index) + ".CDG",
			.expected_bytes = 17616U,
			.expected_fnv1a64 = fnv1a64[index],
		};
	}
	if(has_registration_selector(key)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key, key
		));
	}
	switch(key.role) {
	case MainResourceRole::registration_background:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_registration,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pi,
			.logical_role = "mainl.registration.background",
			.filename = "REGIB.PI",
			.expected_bytes = 44603U,
			.expected_fnv1a64 = 0x24064C266EDFE6B2ULL,
		};
	case MainResourceRole::registration_normal_glyphs:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_registration,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::bfnt_superimpose,
			.logical_role = "mainl.registration.normal_glyphs",
			.filename = "REGI2.BFT",
			.expected_bytes = 25168U,
			.expected_fnv1a64 = 0x1B8989FB9D38D1BAULL,
		};
	case MainResourceRole::registration_highlight_glyphs:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_registration,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::bfnt_superimpose,
			.logical_role = "mainl.registration.highlight_glyphs",
			.filename = "REGI1.BFT",
			.expected_bytes = 25168U,
			.expected_fnv1a64 = 0x9DDED02684AA4339ULL,
		};
	case MainResourceRole::registration_music:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_registration,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pmd_music,
			.logical_role = "mainl.registration.music",
			.filename = "SCORE.M",
			.expected_bytes = 983U,
			.expected_fnv1a64 = 0xC83D906B352A1597ULL,
		};
	case MainResourceRole::registration_rank:
		break;
	default:
		break;
	}
	return std::unexpected(error(ResourceLoadErrorCode::invalid_resource_key, key));
}

[[nodiscard]] std::expected<MainResourceSpec, ResourceLoadError>
static_resource_spec(MainResourceKey key)
{
	if(key.role == MainResourceRole::stage_splash_background) {
		if(key.character != MainCharacter::none ||
			key.palette != MainPaletteVariant::none ||
			!is_stage_background(key.stage_background) ||
			has_cutscene_selector(key) || has_staff_selector(key) ||
			has_registration_selector(key)) {
			return std::unexpected(error(
				ResourceLoadErrorCode::invalid_resource_key, key
			));
		}
		const auto index = static_cast<std::size_t>(
			static_cast<std::uint8_t>(key.stage_background) - 1U
		);
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_stage_transition,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pi,
			.logical_role = "mainl.stage.background." +
				std::to_string(index + 1U),
			.filename = stage_background_filename(key.stage_background),
			.expected_bytes = stage_background_sizes[index],
			.expected_fnv1a64 = stage_background_fnv1a64[index],
		};
	}
	if(key.character != MainCharacter::none ||
		key.palette != MainPaletteVariant::none ||
		key.stage_background != MainlStageBackground::none ||
		has_cutscene_selector(key) || has_staff_selector(key) ||
		has_registration_selector(key)) {
		return std::unexpected(error(ResourceLoadErrorCode::invalid_resource_key, key));
	}

	switch(key.role) {
	case MainResourceRole::op_title_long_background:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::op_title_menu,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pi,
			.logical_role = "op.title.long_background",
			.filename = "TL01.PI",
			.expected_bytes = 4192U,
			.expected_fnv1a64 = 0x23386537C864AD57ULL,
		};
	case MainResourceRole::op_menu_background:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::op_title_menu,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pi,
			.logical_role = "op.title.menu_background",
			.filename = "TL02.PI",
			.expected_bytes = 31779U,
			.expected_fnv1a64 = 0xE5CB3BACFA722E3BULL,
		};
	case MainResourceRole::op_window_sheet:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::op_title_menu,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::bfnt_superimpose,
			.logical_role = "op.title.window_sheet",
			.filename = "OPWIN.BFT",
			.expected_bytes = 3152U,
			.expected_fnv1a64 = 0x734FF8C10A526CFBULL,
		};
	case MainResourceRole::op_gaiji_font:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::op_title_menu,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::bfnt_gaiji,
			.logical_role = "op.title.gaiji_font",
			.filename = "MIKOFT.BFT",
			.expected_bytes = 8224U,
			.expected_fnv1a64 = 0xBD42C433C6DDFB3CULL,
		};
	case MainResourceRole::op_menu_music:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::op_title_menu,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pmd_music,
			.logical_role = "op.title.menu_music",
			.filename = "OP.M",
			.expected_bytes = 4431U,
			.expected_fnv1a64 = 0x8D65B16BB1BB433DULL,
		};
	case MainResourceRole::enemy_scripts:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::direct_vs,
			.archive = MainResourceArchive::yume2,
			.decoder = MainResourceDecoder::enedat,
			.logical_role = "main.enemy_scripts",
			.filename = "ENEDAT.DAT",
			.expected_bytes = 5072U,
			.expected_fnv1a64 = 0x923309B0E3C281B7ULL,
		};
	case MainResourceRole::game_font:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::direct_vs,
			.archive = MainResourceArchive::yume2,
			.decoder = MainResourceDecoder::bfnt_gaiji,
			.logical_role = "main.game_font",
			.filename = "GAMEFT.BFT",
			.expected_bytes = 8224U,
			.expected_fnv1a64 = 0xE9EB8A648BD19D86ULL,
		};
	case MainResourceRole::hud_lose_sheet:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::direct_vs,
			.archive = MainResourceArchive::yume2,
			.decoder = MainResourceDecoder::bfnt_superimpose,
			.logical_role = "main.hud.lose_sheet",
			.filename = "LOSE.BF2",
			.expected_bytes = 4176U,
			.expected_fnv1a64 = 0xE18009B393E4FBD5ULL,
		};
	case MainResourceRole::hud_round_sheet:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::direct_vs,
			.archive = MainResourceArchive::yume2,
			.decoder = MainResourceDecoder::bfnt_superimpose,
			.logical_role = "main.hud.round_sheet",
			.filename = "ROUND.BF2",
			.expected_bytes = 5712U,
			.expected_fnv1a64 = 0x93AEF148376AC125ULL,
		};
	case MainResourceRole::hud_result_sheet:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::direct_vs,
			.archive = MainResourceArchive::yume2,
			.decoder = MainResourceDecoder::bfnt_superimpose,
			.logical_role = "main.hud.result_sheet",
			.filename = "ZIKICW.BF2",
			.expected_bytes = 4176U,
			.expected_fnv1a64 = 0x6A86F6592DDD7BC4ULL,
		};
	case MainResourceRole::stage_enemy_center:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_stage_transition,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pi,
			.logical_role = "main.stage.enemy_center",
			.filename = "EN2.PI",
			.expected_bytes = 1841U,
			.expected_fnv1a64 = 0x227E9A80259725B3ULL,
		};
	case MainResourceRole::selection_palette:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::selection_handoff,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::rgb48,
			.logical_role = "select.palette_handoff",
			.filename = "TLSL.RGB",
			.expected_bytes = 48U,
			.expected_fnv1a64 = 0x9BE11DA2C74091A9ULL,
		};
	case MainResourceRole::mainl_gaiji_font:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_stage_transition,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::bfnt_gaiji,
			.logical_role = "mainl.gaiji_font",
			.filename = "MIKOFT.BFT",
			.expected_bytes = 8224U,
			.expected_fnv1a64 = 0xBD42C433C6DDFB3CULL,
		};
	case MainResourceRole::stage_number_sheet:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_stage_transition,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::cdg,
			.logical_role = "mainl.stage.number_sheet",
			.filename = "ST.CD2",
			.expected_bytes = 6816U,
			.expected_fnv1a64 = 0x60740A0AF72128B7ULL,
		};
	case MainResourceRole::stage_splash_base:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_stage_transition,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pi,
			.logical_role = "mainl.stage.base",
			.filename = "STNX0.PI",
			.expected_bytes = 2732U,
			.expected_fnv1a64 = 0x0805E1FF5167740BULL,
		};
	case MainResourceRole::win_logo_palette_pre:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_win,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::rgb48,
			.logical_role = "mainl.win.logo_palette_pre",
			.filename = "LOGO0.RGB",
			.expected_bytes = 48U,
			.expected_fnv1a64 = 0x41A855E4DBCBA616ULL,
		};
	case MainResourceRole::win_logo_fade_cels:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_win,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::cdg,
			.logical_role = "mainl.win.logo_fade_cels",
			.filename = "LOGO.CD2",
			.expected_bytes = 57616U,
			.expected_fnv1a64 = 0x13D2328E9F88A790ULL,
		};
	case MainResourceRole::win_logo_final:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_win,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::cdg,
			.logical_role = "mainl.win.logo_final",
			.filename = "LOGO5.CDG",
			.expected_bytes = 11536U,
			.expected_fnv1a64 = 0x65F0060D4A61BABBULL,
		};
	case MainResourceRole::win_logo_palette_post:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_win,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::rgb48,
			.logical_role = "mainl.win.logo_palette_post",
			.filename = "LOGO1.RGB",
			.expected_bytes = 48U,
			.expected_fnv1a64 = 0xBC162D7C5FF6182DULL,
		};
	case MainResourceRole::continue_background:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_continue_gameover,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pi,
			.logical_role = "mainl.continue.background",
			.filename = "CONTI.PI",
			.expected_bytes = 5646U,
			.expected_fnv1a64 = 0x31741F129B8269D4ULL,
		};
	case MainResourceRole::continue_choice_cels:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_continue_gameover,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::cdg,
			.logical_role = "mainl.continue.choice_cels",
			.filename = "CONTI.CD2",
			.expected_bytes = 9856U,
			.expected_fnv1a64 = 0x35AEAFB5AC967094ULL,
		};
	case MainResourceRole::gameover_background:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_continue_gameover,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pi,
			.logical_role = "mainl.gameover.background",
			.filename = "OVER.PI",
			.expected_bytes = 3571U,
			.expected_fnv1a64 = 0xFF41379103F8C92AULL,
		};
	case MainResourceRole::cutscene_script:
	case MainResourceRole::cutscene_picture:
	case MainResourceRole::cutscene_music:
	case MainResourceRole::staff_background_palette:
	case MainResourceRole::staff_cdg:
	case MainResourceRole::staff_music:
	case MainResourceRole::registration_background:
	case MainResourceRole::registration_rank:
	case MainResourceRole::registration_normal_glyphs:
	case MainResourceRole::registration_highlight_glyphs:
	case MainResourceRole::registration_music:
		return std::unexpected(error(ResourceLoadErrorCode::invalid_resource_key, key));
	case MainResourceRole::character_sheet:
	case MainResourceRole::character_extra_portrait:
	case MainResourceRole::character_bomb_portrait:
	case MainResourceRole::character_stage_shot:
	case MainResourceRole::character_stage_extra_shot:
	case MainResourceRole::character_stage_enemy_group:
	case MainResourceRole::character_stage_portrait:
	case MainResourceRole::stage_splash_background:
	case MainResourceRole::character_win_portrait:
	case MainResourceRole::character_win_message:
		return std::unexpected(error(ResourceLoadErrorCode::invalid_resource_key, key));
	}
	return std::unexpected(error(ResourceLoadErrorCode::invalid_resource_key, key));
}

[[nodiscard]] constexpr std::size_t stage_shot_size(std::uint8_t index) noexcept
{
	constexpr std::array<std::size_t, 18U> sizes{
		449U, 447U, 330U, 326U, 432U, 432U, 588U, 585U, 266U,
		266U, 311U, 311U, 265U, 265U, 235U, 236U, 416U, 416U,
	};
	return sizes[index];
}

[[nodiscard]] constexpr std::size_t stage_extra_shot_size(
	std::uint8_t index
) noexcept
{
	constexpr std::array<std::size_t, 18U> sizes{
		2530U, 2532U, 2929U, 2931U, 1987U, 1953U, 1953U, 1855U, 2276U,
		2277U, 4104U, 4049U, 1743U, 1710U, 2742U, 2717U, 3168U, 2929U,
	};
	return sizes[index];
}

[[nodiscard]] std::filesystem::path archive_directory(
	MainResourceArchive archive
)
{
	return (archive == MainResourceArchive::yume1) ?
		std::filesystem::path{"YUME1.DAT"} :
		std::filesystem::path{"YUME2.DAT"};
}

[[nodiscard]] bool path_is_within_root(
	const std::filesystem::path &root,
	const std::filesystem::path &candidate
) noexcept
{
	const auto relative = candidate.lexically_relative(root);
	if(relative.empty()) {
		return false;
	}
	for(const auto &component : relative) {
		if(component == "..") {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr bool ascii_equal_ignoring_case(
	std::string_view left,
	std::string_view right
) noexcept
{
	if(left.size() != right.size()) {
		return false;
	}
	for(std::size_t index = 0U; index < left.size(); ++index) {
		auto left_character = static_cast<unsigned char>(left[index]);
		auto right_character = static_cast<unsigned char>(right[index]);
		if(left_character >= static_cast<unsigned char>('A') &&
			left_character <= static_cast<unsigned char>('Z')) {
			left_character = static_cast<unsigned char>(
				left_character + (static_cast<unsigned char>('a') -
					static_cast<unsigned char>('A'))
			);
		}
		if(right_character >= static_cast<unsigned char>('A') &&
			right_character <= static_cast<unsigned char>('Z')) {
			right_character = static_cast<unsigned char>(
				right_character + (static_cast<unsigned char>('a') -
					static_cast<unsigned char>('A'))
			);
		}
		if(left_character != right_character) {
			return false;
		}
	}
	return true;
}

struct ExactChild {
	std::filesystem::path path{};
	ResourceLoadErrorCode error = ResourceLoadErrorCode::missing;
};

[[nodiscard]] ExactChild find_exact_child(
	const std::filesystem::path &directory,
	std::string_view expected_name
)
{
	std::error_code filesystem_error;
	std::filesystem::directory_iterator entry(directory, filesystem_error);
	if(filesystem_error) {
		return {.error = ResourceLoadErrorCode::read_failed};
	}
	bool case_mismatch = false;
	for(; entry != std::filesystem::directory_iterator{};
		entry.increment(filesystem_error)) {
		if(filesystem_error) {
			return {.error = ResourceLoadErrorCode::read_failed};
		}
		const auto actual_name = entry->path().filename().string();
		if(actual_name == expected_name) {
			return {.path = entry->path()};
		}
		case_mismatch = case_mismatch || ascii_equal_ignoring_case(
			actual_name, expected_name
		);
	}
	return {.error = case_mismatch ? ResourceLoadErrorCode::case_mismatch :
		ResourceLoadErrorCode::missing};
}

} // namespace

std::uint64_t main_resource_fnv1a64(
	std::span<const std::byte> bytes
) noexcept
{
	std::uint64_t result = 14695981039346656037ULL;
	for(const auto byte : bytes) {
		result ^= std::to_integer<unsigned char>(byte);
		result *= 1099511628211ULL;
	}
	return result;
}

bool is_safe_extracted_entry_name(std::string_view name) noexcept
{
	return !name.empty() && name != "." && name != ".." &&
		name.find_first_of("/\\:") == std::string_view::npos &&
		name.find("..") == std::string_view::npos;
}

std::expected<MainResourceSpec, ResourceLoadError> main_resource_spec(
	MainResourceKey key
)
{
	if(key.role == MainResourceRole::cutscene_script ||
		key.role == MainResourceRole::cutscene_picture ||
		key.role == MainResourceRole::cutscene_music) {
		return cutscene_resource_spec(key);
	}
	if(key.role == MainResourceRole::staff_background_palette ||
		key.role == MainResourceRole::staff_cdg ||
		key.role == MainResourceRole::staff_music) {
		return staff_resource_spec(key);
	}
	if(key.role == MainResourceRole::registration_background ||
		key.role == MainResourceRole::registration_rank ||
		key.role == MainResourceRole::registration_normal_glyphs ||
		key.role == MainResourceRole::registration_highlight_glyphs ||
		key.role == MainResourceRole::registration_music) {
		return registration_resource_spec(key);
	}
	if(has_cutscene_selector(key) || has_staff_selector(key) ||
		has_registration_selector(key)) {
		return std::unexpected(error(
			ResourceLoadErrorCode::invalid_resource_key, key
		));
	}
	if(is_character_only_role(key.role)) {
		if(!is_character(key.character) ||
			key.palette != MainPaletteVariant::none ||
			key.stage_background != MainlStageBackground::none ||
			has_registration_selector(key)) {
			return std::unexpected(error(
				ResourceLoadErrorCode::invalid_resource_key, key
			));
		}
		if(key.role == MainResourceRole::character_stage_enemy_group) {
			const auto group = stage_enemy_group_index(key.character);
			return MainResourceSpec{
				.key = key,
				.scene = MainResourceScene::mainl_stage_transition,
				.archive = MainResourceArchive::yume1,
				.decoder = MainResourceDecoder::pi,
				.logical_role = character_only_logical_role(
					key, "stage_enemy_group"
				),
				.filename = stage_enemy_group_filename(group),
				.expected_bytes = stage_enemy_group_sizes[group],
				.expected_fnv1a64 = stage_enemy_group_fnv1a64[group],
			};
		}
		const auto index = static_cast<std::size_t>(
			static_cast<std::uint8_t>(key.character) - first_character_value
		);
		if(key.role == MainResourceRole::character_win_message) {
			return MainResourceSpec{
				.key = key,
				.scene = MainResourceScene::mainl_win,
				.archive = MainResourceArchive::yume1,
				.decoder = MainResourceDecoder::shift_jis_blocks,
				.logical_role = "mainl.win.character." +
					std::string{character_name(key.character)} + ".message",
				.filename = "@0" + std::to_string(index) + "TX.TXT",
				.expected_bytes = 1980U,
				.expected_fnv1a64 = win_message_fnv1a64[index],
			};
		}
		const bool win_portrait =
			key.role == MainResourceRole::character_win_portrait;
		return MainResourceSpec{
			.key = key,
			.scene = win_portrait ? MainResourceScene::mainl_win :
				MainResourceScene::mainl_stage_transition,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::cdg,
			.logical_role = win_portrait ?
				("mainl.win.character." +
					std::string{character_name(key.character)} + ".portrait") :
				character_only_logical_role(key, "stage_portrait"),
			.filename = filename_with_index(
				static_cast<std::uint8_t>(index * 2U), "SL.CD2"
			),
			.expected_bytes = 46096U,
			.expected_fnv1a64 = stage_portrait_fnv1a64[index],
		};
	}
	if(!is_paletted_character_role(key.role)) {
		return static_resource_spec(key);
	}
	if(!is_character(key.character) || !is_palette(key.palette) ||
		key.stage_background != MainlStageBackground::none ||
		has_registration_selector(key)) {
		return std::unexpected(error(ResourceLoadErrorCode::invalid_resource_key, key));
	}

	const auto index = character_file_index(key.character, key.palette);
	switch(key.role) {
	case MainResourceRole::character_sheet:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::direct_vs,
			.archive = MainResourceArchive::yume2,
			.decoder = MainResourceDecoder::bfnt_superimpose,
			.logical_role = character_logical_role(key, "sheet"),
			.filename = filename_with_index(index, "CH.BF2"),
			.expected_bytes = 4688U,
			.expected_fnv1a64 = character_fnv1a64(key.role, index),
		};
	case MainResourceRole::character_extra_portrait:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::direct_vs,
			.archive = MainResourceArchive::yume2,
			.decoder = MainResourceDecoder::mrs,
			.logical_role = character_logical_role(key, "extra_portrait"),
			.filename = filename_with_index(index, "EX.MRS"),
			.expected_bytes = 33120U,
			.expected_fnv1a64 = character_fnv1a64(key.role, index),
		};
	case MainResourceRole::character_bomb_portrait:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::direct_vs,
			.archive = MainResourceArchive::yume2,
			.decoder = MainResourceDecoder::mrs,
			.logical_role = character_logical_role(key, "bomb_portrait"),
			.filename = filename_with_index(index, "BM.MRS"),
			.expected_bytes = 33120U,
			.expected_fnv1a64 = character_fnv1a64(key.role, index),
		};
	case MainResourceRole::character_stage_shot:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_stage_transition,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pi,
			.logical_role = character_logical_role(key, "stage_shot"),
			.filename = filename_with_index(index, "16.PI"),
			.expected_bytes = stage_shot_size(index),
			.expected_fnv1a64 = character_fnv1a64(key.role, index),
		};
	case MainResourceRole::character_stage_extra_shot:
		return MainResourceSpec{
			.key = key,
			.scene = MainResourceScene::mainl_stage_transition,
			.archive = MainResourceArchive::yume1,
			.decoder = MainResourceDecoder::pi,
			.logical_role = character_logical_role(key, "stage_extra_shot"),
			.filename = filename_with_index(index, "EX.PI"),
			.expected_bytes = stage_extra_shot_size(index),
			.expected_fnv1a64 = character_fnv1a64(key.role, index),
		};
	case MainResourceRole::op_title_long_background:
	case MainResourceRole::op_menu_background:
	case MainResourceRole::op_window_sheet:
	case MainResourceRole::op_gaiji_font:
	case MainResourceRole::op_menu_music:
	case MainResourceRole::enemy_scripts:
	case MainResourceRole::game_font:
	case MainResourceRole::hud_lose_sheet:
	case MainResourceRole::hud_round_sheet:
	case MainResourceRole::hud_result_sheet:
	case MainResourceRole::stage_enemy_center:
	case MainResourceRole::selection_palette:
	case MainResourceRole::character_stage_enemy_group:
	case MainResourceRole::mainl_gaiji_font:
	case MainResourceRole::character_stage_portrait:
	case MainResourceRole::stage_number_sheet:
	case MainResourceRole::stage_splash_base:
	case MainResourceRole::stage_splash_background:
	case MainResourceRole::win_logo_palette_pre:
	case MainResourceRole::win_logo_fade_cels:
	case MainResourceRole::win_logo_final:
	case MainResourceRole::win_logo_palette_post:
	case MainResourceRole::character_win_portrait:
	case MainResourceRole::character_win_message:
	case MainResourceRole::continue_background:
	case MainResourceRole::continue_choice_cels:
	case MainResourceRole::gameover_background:
	case MainResourceRole::cutscene_script:
	case MainResourceRole::cutscene_picture:
	case MainResourceRole::cutscene_music:
	case MainResourceRole::staff_background_palette:
	case MainResourceRole::staff_cdg:
	case MainResourceRole::staff_music:
	case MainResourceRole::registration_background:
	case MainResourceRole::registration_rank:
	case MainResourceRole::registration_normal_glyphs:
	case MainResourceRole::registration_highlight_glyphs:
	case MainResourceRole::registration_music:
		break;
	}
	return std::unexpected(error(ResourceLoadErrorCode::invalid_resource_key, key));
}

std::vector<MainResourceSpec> main_resource_manifest()
{
	constexpr std::array static_roles{
		MainResourceRole::op_title_long_background,
		MainResourceRole::op_menu_background,
		MainResourceRole::op_window_sheet,
		MainResourceRole::op_gaiji_font,
		MainResourceRole::op_menu_music,
		MainResourceRole::enemy_scripts,
		MainResourceRole::game_font,
		MainResourceRole::hud_lose_sheet,
		MainResourceRole::hud_round_sheet,
		MainResourceRole::hud_result_sheet,
		MainResourceRole::stage_enemy_center,
		MainResourceRole::selection_palette,
		MainResourceRole::mainl_gaiji_font,
		MainResourceRole::stage_number_sheet,
		MainResourceRole::stage_splash_base,
		MainResourceRole::win_logo_palette_pre,
		MainResourceRole::win_logo_fade_cels,
		MainResourceRole::win_logo_final,
		MainResourceRole::win_logo_palette_post,
		MainResourceRole::continue_background,
		MainResourceRole::continue_choice_cels,
		MainResourceRole::gameover_background,
		MainResourceRole::staff_background_palette,
		MainResourceRole::staff_music,
		MainResourceRole::registration_background,
		MainResourceRole::registration_normal_glyphs,
		MainResourceRole::registration_highlight_glyphs,
		MainResourceRole::registration_music,
	};
	constexpr std::array character_roles{
		MainResourceRole::character_sheet,
		MainResourceRole::character_extra_portrait,
		MainResourceRole::character_bomb_portrait,
		MainResourceRole::character_stage_shot,
		MainResourceRole::character_stage_extra_shot,
	};

	constexpr std::array character_only_roles{
		MainResourceRole::character_stage_enemy_group,
		MainResourceRole::character_stage_portrait,
		MainResourceRole::character_win_portrait,
		MainResourceRole::character_win_message,
	};

	std::vector<MainResourceSpec> result;
	result.reserve(static_roles.size() +
		((last_character_value - first_character_value + 1U) *
		(last_palette_value - first_palette_value + 1U) *
		character_roles.size()) +
		((last_character_value - first_character_value + 1U) *
		character_only_roles.size()) + 5U + 28U + 24U + 5U + 12U + 4U);
	for(const auto role : static_roles) {
		result.push_back(*main_resource_spec({.role = role}));
	}
	for(std::uint8_t background =
		static_cast<std::uint8_t>(MainlStageBackground::numbered);
		background <= static_cast<std::uint8_t>(MainlStageBackground::non_story);
		++background) {
		result.push_back(*main_resource_spec({
			.role = MainResourceRole::stage_splash_background,
			.stage_background = static_cast<MainlStageBackground>(background),
		}));
	}
	for(std::uint8_t script = 1U;
		script <= static_cast<std::uint8_t>(MainCutsceneScript::extra_ending);
		++script) {
		result.push_back(*main_resource_spec({
			.role = MainResourceRole::cutscene_script,
			.cutscene_script = static_cast<MainCutsceneScript>(script),
		}));
	}
	for(std::uint8_t picture = 1U;
		picture <= static_cast<std::uint8_t>(MainCutscenePicture::dm4a);
		++picture) {
		result.push_back(*main_resource_spec({
			.role = MainResourceRole::cutscene_picture,
			.cutscene_picture = static_cast<MainCutscenePicture>(picture),
		}));
	}
	for(std::uint8_t music = 1U;
		music <= static_cast<std::uint8_t>(MainCutsceneMusic::demo5);
		++music) {
		result.push_back(*main_resource_spec({
			.role = MainResourceRole::cutscene_music,
			.cutscene_music = static_cast<MainCutsceneMusic>(music),
		}));
	}
	for(std::uint8_t staff_cdg = 1U;
		staff_cdg <= static_cast<std::uint8_t>(MainStaffCdg::staff_12);
		++staff_cdg) {
		result.push_back(*main_resource_spec({
			.role = MainResourceRole::staff_cdg,
			.staff_cdg = static_cast<MainStaffCdg>(staff_cdg),
		}));
	}
	for(std::uint8_t rank = 1U;
		rank <= static_cast<std::uint8_t>(MainRegistrationRank::lunatic);
		++rank) {
		result.push_back(*main_resource_spec({
			.role = MainResourceRole::registration_rank,
			.registration_rank = static_cast<MainRegistrationRank>(rank),
		}));
	}
	for(std::uint8_t character = first_character_value;
		character <= last_character_value;
		++character) {
		for(std::uint8_t palette = first_palette_value;
			palette <= last_palette_value;
			++palette) {
			for(const auto role : character_roles) {
				result.push_back(*main_resource_spec({
					.role = role,
					.character = static_cast<MainCharacter>(character),
					.palette = static_cast<MainPaletteVariant>(palette),
				}));
			}
		}
		for(const auto role : character_only_roles) {
			result.push_back(*main_resource_spec({
				.role = role,
				.character = static_cast<MainCharacter>(character),
			}));
		}
	}
	return result;
}

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

std::expected<void, ResourceLoadErrorCode> validate_main_resource_manifest(
	std::span<const MainResourceSpec> manifest
)
{
	for(std::size_t left = 0U; left < manifest.size(); ++left) {
		for(std::size_t right = left + 1U; right < manifest.size(); ++right) {
			if(manifest[left].logical_role == manifest[right].logical_role) {
				return std::unexpected(ResourceLoadErrorCode::duplicate_logical_role);
			}
		}
	}
	return {};
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

MainResourceLoader::MainResourceLoader(
	std::filesystem::path content_root,
	MainResourceIntegrity integrity
) :
	content_root_(std::move(content_root)),
	integrity_(integrity)
{}

std::expected<ResourceBytes, ResourceLoadError> MainResourceLoader::load(
	MainResourceKey key
)
{
	const auto spec = main_resource_spec(key);
	if(!spec) {
		return std::unexpected(spec.error());
	}
	const auto cached = cache_.find(key);
	if(cached != cache_.end()) {
		return cached->second;
	}

	const auto archive = archive_directory(spec->archive);
	if(!is_safe_extracted_entry_name(archive.string()) ||
		!is_safe_extracted_entry_name(spec->filename)) {
		return std::unexpected(error(ResourceLoadErrorCode::path_escape, key));
	}

	std::error_code filesystem_error;
	const auto canonical_root = std::filesystem::canonical(
		content_root_, filesystem_error
	);
	if(filesystem_error ||
		!std::filesystem::is_directory(canonical_root, filesystem_error) ||
		filesystem_error) {
		return std::unexpected(error(ResourceLoadErrorCode::invalid_content_root, key));
	}

	const auto selected_archive = find_exact_child(canonical_root, archive.string());
	if(selected_archive.path.empty()) {
		return std::unexpected(error(selected_archive.error, key));
	}
	const auto canonical_archive = std::filesystem::canonical(
		selected_archive.path, filesystem_error
	);
	if(filesystem_error) {
		return std::unexpected(error(ResourceLoadErrorCode::read_failed, key));
	}
	if(!path_is_within_root(canonical_root, canonical_archive)) {
		return std::unexpected(error(ResourceLoadErrorCode::path_escape, key));
	}
	if(!std::filesystem::is_directory(canonical_archive, filesystem_error) ||
		filesystem_error) {
		return std::unexpected(error(ResourceLoadErrorCode::not_directory, key));
	}
	const auto selected_file = find_exact_child(canonical_archive, spec->filename);
	if(selected_file.path.empty()) {
		return std::unexpected(error(selected_file.error, key));
	}
	const auto canonical_selected = std::filesystem::canonical(
		selected_file.path, filesystem_error
	);
	if(filesystem_error) {
		return std::unexpected(error(ResourceLoadErrorCode::read_failed, key));
	}
	if(!path_is_within_root(canonical_root, canonical_selected)) {
		return std::unexpected(error(ResourceLoadErrorCode::path_escape, key));
	}
	if(!std::filesystem::is_regular_file(canonical_selected, filesystem_error) ||
		filesystem_error) {
		return std::unexpected(error(ResourceLoadErrorCode::not_regular_file, key));
	}
	const auto actual_size = std::filesystem::file_size(
		canonical_selected, filesystem_error
	);
	if(filesystem_error) {
		return std::unexpected(error(ResourceLoadErrorCode::read_failed, key));
	}
	if(actual_size != spec->expected_bytes) {
		return std::unexpected(error(ResourceLoadErrorCode::size_mismatch, key));
	}
	if(spec->expected_bytes >
		static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
		return std::unexpected(error(ResourceLoadErrorCode::read_failed, key));
	}

	std::ifstream input(canonical_selected, std::ios::binary);
	if(!input) {
		return std::unexpected(error(ResourceLoadErrorCode::read_failed, key));
	}
	std::vector<std::byte> bytes(spec->expected_bytes);
	input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	if(!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
		return std::unexpected(error(ResourceLoadErrorCode::read_failed, key));
	}
	if(integrity_ == MainResourceIntegrity::size_and_fnv1a64 &&
		main_resource_fnv1a64(bytes) != spec->expected_fnv1a64) {
		return std::unexpected(error(ResourceLoadErrorCode::checksum_mismatch, key));
	}
	const auto resource = std::make_shared<const std::vector<std::byte>>(
		std::move(bytes)
	);
	cache_.emplace(key, resource);
	return resource;
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_op_title_menu()
{
	return load_batch(op_title_menu_resource_keys());
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_direct_vs(
	MainPlayerSelection player_one,
	MainPlayerSelection player_two
)
{
	const auto keys = direct_vs_resource_keys(player_one, player_two);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_main_static()
{
	return load_batch(main_static_resource_keys());
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_main_private(
	MainPlayerSelection player_one,
	MainPlayerSelection player_two
)
{
	const auto keys = main_private_resource_keys(player_one, player_two);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_stage(MainlStageResourceRequest request)
{
	const auto keys = mainl_stage_resource_keys(request);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_win(MainPlayerSelection winner)
{
	const auto keys = mainl_win_resource_keys(winner);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_continue(std::uint8_t remaining_credits)
{
	const auto keys = mainl_continue_resource_keys(remaining_credits);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_cutscene(MainlCutsceneResourceRequest request)
{
	const auto keys = mainl_cutscene_resource_keys(request);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_staff()
{
	return load_batch(mainl_staff_resource_keys());
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_registration(
	MainlRegistrationResourceRequest request
)
{
	const auto keys = mainl_registration_resource_keys(request);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_gameover()
{
	return load_batch(mainl_gameover_resource_keys());
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_batch(std::span<const MainResourceKey> keys)
{
	MainResourceLoader candidate(*this);
	std::vector<LoadedMainResource> result;
	result.reserve(keys.size());
	for(const auto key : keys) {
		const auto spec = main_resource_spec(key);
		if(!spec) {
			return std::unexpected(spec.error());
		}
		const auto resource = candidate.load(key);
		if(!resource) {
			return std::unexpected(resource.error());
		}
		result.push_back({.spec = *spec, .bytes = *resource});
	}
	cache_ = std::move(candidate.cache_);
	return result;
}

std::size_t MainResourceLoader::cached_resource_count() const noexcept
{
	return cache_.size();
}

} // namespace th03::app
