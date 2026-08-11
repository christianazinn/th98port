#include "th03/app/resources.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>

namespace {

namespace fs = std::filesystem;
using th03::app::MainCharacter;
using th03::app::MainCutsceneMusic;
using th03::app::MainCutscenePicture;
using th03::app::MainCutsceneScript;
using th03::app::MainPaletteVariant;
using th03::app::MainPlayerSelection;
using th03::app::MainResourceArchive;
using th03::app::MainResourceIntegrity;
using th03::app::MainResourceKey;
using th03::app::MainResourceLoader;
using th03::app::MainResourceRole;
using th03::app::MainRegistrationRank;
using th03::app::MainStaffCdg;
using th03::app::MainlCutsceneKind;
using th03::app::MainlStageBackground;
using th03::app::MainlStageResourceRequest;
using th03::app::ResourceLoadErrorCode;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

class TemporaryRoot final {
public:
	TemporaryRoot()
	{
		const auto temporary = fs::temp_directory_path();
		const auto nonce = std::chrono::high_resolution_clock::now()
			.time_since_epoch().count();
		for(std::uint32_t suffix = 0U; suffix < 1000U; ++suffix) {
			path_ = temporary / ("th03-app-resources-test-" +
				std::to_string(nonce) + "-" + std::to_string(suffix));
			std::error_code error;
			if(fs::create_directory(path_, error)) {
				return;
			}
		}
		path_.clear();
	}

	~TemporaryRoot()
	{
		if(path_.empty() || path_.parent_path() != fs::temp_directory_path() ||
			path_.filename().string().rfind("th03-app-resources-test-", 0U) != 0U) {
			return;
		}
		std::error_code error;
		fs::remove_all(path_, error);
	}

	[[nodiscard]] const fs::path &path() const noexcept { return path_; }
	[[nodiscard]] bool valid() const noexcept { return !path_.empty(); }

private:
	fs::path path_{};
};

void write_sized_file(
	const fs::path &root,
	MainResourceArchive archive,
	const std::string &filename,
	std::size_t size,
	std::byte fill
)
{
	const auto archive_name = (archive == MainResourceArchive::yume1) ?
		"YUME1.DAT" : "YUME2.DAT";
	std::error_code error;
	fs::create_directories(root / archive_name, error);
	std::ofstream output(root / archive_name / filename, std::ios::binary);
	for(std::size_t index = 0U; index < size; ++index) {
		output.put(static_cast<char>(std::to_integer<unsigned char>(fill)));
	}
}

void write_spec(const fs::path &root, MainResourceKey key, std::byte fill)
{
	const auto spec = th03::app::main_resource_spec(key);
	expect(static_cast<bool>(spec), "synthetic resource key has a manifest spec");
	if(spec) {
		write_sized_file(
			root, spec->archive, spec->filename, spec->expected_bytes, fill
		);
	}
}

void test_manifest()
{
	constexpr std::array hello{
		std::byte{0x68U}, std::byte{0x65U}, std::byte{0x6CU}, std::byte{0x6CU},
		std::byte{0x6FU},
	};
	expect(
		th03::app::main_resource_fnv1a64({}) == 0xCBF29CE484222325ULL &&
			th03::app::main_resource_fnv1a64(hello) == 0xA430D84680AABD0BULL,
		"resource fingerprint matches standard FNV-1a-64 vectors"
	);
	const auto manifest = th03::app::main_resource_manifest();
	expect(
		manifest.size() == 232U,
		"manifest enumerates OP, MAIN, MAINL, cutscene, staff, registration, paletted, and character-only assets"
	);
	expect(
		static_cast<bool>(th03::app::validate_main_resource_manifest(manifest)),
		"checked-in manifest has unique logical roles"
	);
	bool contains_enedat = false;
	bool contains_game_font = false;
	bool contains_reimu_sheet = false;
	bool contains_yumemi_stage_shot = false;
	bool contains_yumemi_stage_extra_shot = false;
	bool contains_enemy_center = false;
	bool contains_yumemi_enemy_group = false;
	bool contains_mainl_font = false;
	bool contains_stage_portrait = false;
	bool contains_stage_number = false;
	bool contains_stage_base = false;
	bool contains_stage_decisive = false;
	bool contains_continue = false;
	bool contains_continue_cels = false;
	bool contains_gameover = false;
	bool contains_first_cutscene_script = false;
	bool contains_last_cutscene_script = false;
	bool contains_first_cutscene_picture = false;
	bool contains_last_cutscene_picture = false;
	bool contains_last_cutscene_music = false;
	bool contains_staff_palette = false;
	bool contains_first_staff_cdg = false;
	bool contains_last_staff_cdg = false;
	bool contains_staff_music = false;
	bool contains_registration_background = false;
	bool contains_registration_lunatic = false;
	bool contains_registration_normal_glyphs = false;
	bool contains_registration_highlight_glyphs = false;
	bool contains_registration_music = false;
	for(const auto &spec : manifest) {
		contains_enedat = contains_enedat ||
			(spec.key.role == MainResourceRole::enemy_scripts &&
				spec.filename == "ENEDAT.DAT" && spec.expected_bytes == 5072U);
		contains_game_font = contains_game_font ||
			(spec.key.role == MainResourceRole::game_font &&
				spec.filename == "GAMEFT.BFT" && spec.expected_bytes == 8224U);
		contains_reimu_sheet = contains_reimu_sheet ||
			(spec.key.role == MainResourceRole::character_sheet &&
				spec.filename == "00CH.BF2");
		contains_yumemi_stage_shot = contains_yumemi_stage_shot ||
			(spec.key.role == MainResourceRole::character_stage_shot &&
				spec.filename == "1716.PI" && spec.expected_bytes == 416U);
		contains_yumemi_stage_extra_shot = contains_yumemi_stage_extra_shot ||
			(spec.key.role == MainResourceRole::character_stage_extra_shot &&
				spec.filename == "17EX.PI" && spec.expected_bytes == 2929U);
		contains_enemy_center = contains_enemy_center ||
			(spec.key.role == MainResourceRole::stage_enemy_center &&
				spec.filename == "EN2.PI" && spec.expected_bytes == 1841U);
		contains_yumemi_enemy_group = contains_yumemi_enemy_group ||
			(spec.key.role == MainResourceRole::character_stage_enemy_group &&
				spec.key.character == MainCharacter::yumemi &&
				spec.key.palette == MainPaletteVariant::none &&
				spec.filename == "ENEMY04.PI" && spec.expected_bytes == 7035U);
		contains_mainl_font = contains_mainl_font ||
			(spec.key.role == MainResourceRole::mainl_gaiji_font &&
				spec.archive == MainResourceArchive::yume1 &&
				spec.filename == "MIKOFT.BFT" && spec.expected_bytes == 8224U);
		contains_stage_portrait = contains_stage_portrait ||
			(spec.key.role == MainResourceRole::character_stage_portrait &&
				spec.key.character == MainCharacter::yumemi &&
				spec.filename == "16SL.CD2" && spec.expected_bytes == 46096U);
		contains_stage_number = contains_stage_number ||
			(spec.key.role == MainResourceRole::stage_number_sheet &&
				spec.filename == "ST.CD2" && spec.expected_bytes == 6816U);
		contains_stage_base = contains_stage_base ||
			(spec.key.role == MainResourceRole::stage_splash_base &&
				spec.filename == "STNX0.PI" && spec.expected_bytes == 2732U);
		contains_stage_decisive = contains_stage_decisive ||
			(spec.key.role == MainResourceRole::stage_splash_background &&
				spec.key.stage_background == MainlStageBackground::decisive &&
				spec.filename == "STNX4.PI" && spec.expected_bytes == 2901U);
		contains_continue = contains_continue ||
			(spec.key.role == MainResourceRole::continue_background &&
				spec.filename == "CONTI.PI" && spec.expected_bytes == 5646U &&
				spec.expected_fnv1a64 == 0x31741F129B8269D4ULL);
		contains_continue_cels = contains_continue_cels ||
			(spec.key.role == MainResourceRole::continue_choice_cels &&
				spec.filename == "CONTI.CD2" && spec.expected_bytes == 9856U &&
				spec.expected_fnv1a64 == 0x35AEAFB5AC967094ULL);
		contains_gameover = contains_gameover ||
			(spec.key.role == MainResourceRole::gameover_background &&
				spec.filename == "OVER.PI" && spec.expected_bytes == 3571U &&
				spec.expected_fnv1a64 == 0xFF41379103F8C92AULL);
		contains_first_cutscene_script = contains_first_cutscene_script ||
			(spec.key.role == MainResourceRole::cutscene_script &&
				spec.key.cutscene_script == MainCutsceneScript::dm0_reimu &&
				spec.filename == "@00DM0.TXT" && spec.expected_bytes == 4770U &&
				spec.expected_fnv1a64 == 0x1C62B85E545EFC43ULL);
		contains_last_cutscene_script = contains_last_cutscene_script ||
			(spec.key.role == MainResourceRole::cutscene_script &&
				spec.key.cutscene_script == MainCutsceneScript::extra_ending &&
				spec.filename == "@99ED.TXT" && spec.expected_bytes == 615U &&
				spec.expected_fnv1a64 == 0xDC89B80A10BE849CULL);
		contains_first_cutscene_picture = contains_first_cutscene_picture ||
			(spec.key.role == MainResourceRole::cutscene_picture &&
				spec.key.cutscene_picture == MainCutscenePicture::dm1a &&
				spec.filename == "DM1A.PI" && spec.expected_bytes == 4652U);
		contains_last_cutscene_picture = contains_last_cutscene_picture ||
			(spec.key.role == MainResourceRole::cutscene_picture &&
				spec.key.cutscene_picture == MainCutscenePicture::dm4a &&
				spec.filename == "DM4A.PI" && spec.expected_bytes == 22284U);
		contains_last_cutscene_music = contains_last_cutscene_music ||
			(spec.key.role == MainResourceRole::cutscene_music &&
				spec.key.cutscene_music == MainCutsceneMusic::demo5 &&
				spec.filename == "DEMO5.M" && spec.expected_bytes == 1193U &&
				spec.expected_fnv1a64 == 0x1E13270E55263E7EULL);
		contains_staff_palette = contains_staff_palette ||
			(spec.key.role == MainResourceRole::staff_background_palette &&
				spec.filename == "EDBK1.RGB" && spec.expected_bytes == 48U &&
				spec.expected_fnv1a64 == 0x5305C69FE55F8683ULL);
		contains_first_staff_cdg = contains_first_staff_cdg ||
			(spec.key.role == MainResourceRole::staff_cdg &&
				spec.key.staff_cdg == MainStaffCdg::staff_1 &&
				spec.filename == "STF1.CDG" && spec.expected_bytes == 7696U &&
				spec.expected_fnv1a64 == 0xC104BD8D7F6FDC09ULL);
		contains_last_staff_cdg = contains_last_staff_cdg ||
			(spec.key.role == MainResourceRole::staff_cdg &&
				spec.key.staff_cdg == MainStaffCdg::staff_12 &&
				spec.filename == "STF12.CDG" && spec.expected_bytes == 5056U &&
				spec.expected_fnv1a64 == 0xD15C59C7A26437CAULL);
		contains_staff_music = contains_staff_music ||
			(spec.key.role == MainResourceRole::staff_music &&
				spec.filename == "ED.M" && spec.expected_bytes == 3378U &&
				spec.expected_fnv1a64 == 0x897797694DA73B83ULL);
		contains_registration_background = contains_registration_background ||
			(spec.key.role == MainResourceRole::registration_background &&
				spec.filename == "REGIB.PI" && spec.expected_bytes == 44603U &&
				spec.expected_fnv1a64 == 0x24064C266EDFE6B2ULL);
		contains_registration_lunatic = contains_registration_lunatic ||
			(spec.key.role == MainResourceRole::registration_rank &&
				spec.key.registration_rank == MainRegistrationRank::lunatic &&
				spec.filename == "RFT3.CDG" && spec.expected_bytes == 17616U &&
				spec.expected_fnv1a64 == 0x558F9EEA22AAA179ULL);
		contains_registration_normal_glyphs =
			contains_registration_normal_glyphs ||
			(spec.key.role == MainResourceRole::registration_normal_glyphs &&
				spec.filename == "REGI2.BFT" && spec.expected_bytes == 25168U &&
				spec.expected_fnv1a64 == 0x1B8989FB9D38D1BAULL);
		contains_registration_highlight_glyphs =
			contains_registration_highlight_glyphs ||
			(spec.key.role == MainResourceRole::registration_highlight_glyphs &&
				spec.filename == "REGI1.BFT" && spec.expected_bytes == 25168U &&
				spec.expected_fnv1a64 == 0x9DDED02684AA4339ULL);
		contains_registration_music = contains_registration_music ||
			(spec.key.role == MainResourceRole::registration_music &&
				spec.filename == "SCORE.M" && spec.expected_bytes == 983U &&
				spec.expected_fnv1a64 == 0xC83D906B352A1597ULL);
		expect(
			th03::app::is_safe_extracted_entry_name(spec.filename),
			"every checked-in manifest filename is one safe extracted component"
		);
		expect(
			spec.expected_fnv1a64 != 0U,
			"every checked-in manifest entry has an integrity fingerprint"
		);
	}
	expect(
		contains_enedat && contains_game_font && contains_reimu_sheet &&
			contains_yumemi_stage_shot && contains_yumemi_stage_extra_shot &&
			contains_enemy_center && contains_yumemi_enemy_group &&
			contains_mainl_font && contains_stage_portrait &&
			contains_stage_number && contains_stage_base &&
			contains_stage_decisive && contains_continue &&
			contains_continue_cels && contains_gameover &&
			contains_first_cutscene_script && contains_last_cutscene_script &&
			contains_first_cutscene_picture && contains_last_cutscene_picture &&
			contains_last_cutscene_music && contains_staff_palette &&
			contains_first_staff_cdg && contains_last_staff_cdg &&
			contains_staff_music && contains_registration_background &&
			contains_registration_lunatic &&
			contains_registration_normal_glyphs &&
			contains_registration_highlight_glyphs &&
			contains_registration_music,
		"manifest retains the representative source-named resource families"
	);
	struct ExpectedEnemyGroup {
		MainCharacter character;
		const char *filename;
		std::size_t bytes;
		std::uint64_t fnv1a64;
	};
	constexpr std::array expected_enemy_groups{
		ExpectedEnemyGroup{MainCharacter::reimu, "ENEMY00.PI", 7378U,
			0x9E080DAAA809D13BULL},
		ExpectedEnemyGroup{MainCharacter::mima, "ENEMY00.PI", 7378U,
			0x9E080DAAA809D13BULL},
		ExpectedEnemyGroup{MainCharacter::marisa, "ENEMY01.PI", 7295U,
			0x720E44C9C7EFB435ULL},
		ExpectedEnemyGroup{MainCharacter::ellen, "ENEMY02.PI", 7064U,
			0x57829DA41AE90F86ULL},
		ExpectedEnemyGroup{MainCharacter::kotohime, "ENEMY01.PI", 7295U,
			0x720E44C9C7EFB435ULL},
		ExpectedEnemyGroup{MainCharacter::kana, "ENEMY02.PI", 7064U,
			0x57829DA41AE90F86ULL},
		ExpectedEnemyGroup{MainCharacter::rikako, "ENEMY00.PI", 7378U,
			0x9E080DAAA809D13BULL},
		ExpectedEnemyGroup{MainCharacter::chiyuri, "ENEMY03.PI", 7046U,
			0x0041BE7AD009BC24ULL},
		ExpectedEnemyGroup{MainCharacter::yumemi, "ENEMY04.PI", 7035U,
			0x13FA6F425CCC8E13ULL},
	};
	for(const auto &expected : expected_enemy_groups) {
		const auto spec = th03::app::main_resource_spec({
			.role = MainResourceRole::character_stage_enemy_group,
			.character = expected.character,
		});
		expect(
			spec && spec->filename == expected.filename &&
				spec->expected_bytes == expected.bytes &&
				spec->expected_fnv1a64 == expected.fnv1a64,
			"every character maps to its exact opponent-group PI metadata"
		);
	}
	expect(
		!th03::app::is_safe_extracted_entry_name("../ENEDAT.DAT") &&
			!th03::app::is_safe_extracted_entry_name("YUME2.DAT/ENEDAT.DAT") &&
			!th03::app::is_safe_extracted_entry_name("C:ENEDAT.DAT") &&
			!th03::app::is_safe_extracted_entry_name("\\ENEDAT.DAT") &&
			!th03::app::is_safe_extracted_entry_name("..\\ENEDAT.DAT"),
		"entry-name validation rejects traversal, nesting, and drive syntax"
	);
	auto duplicate_role = manifest;
	duplicate_role[1U].logical_role = duplicate_role.front().logical_role;
	const auto duplicate_check = th03::app::validate_main_resource_manifest(
		duplicate_role
	);
	expect(
		!duplicate_check && duplicate_check.error() ==
			ResourceLoadErrorCode::duplicate_logical_role,
		"duplicate logical roles fail with a deterministic named manifest error"
	);
}

void test_op_title_menu_resource_batch()
{
	const auto keys = th03::app::op_title_menu_resource_keys();
	const std::array expected{
		MainResourceKey{.role = MainResourceRole::op_title_long_background},
		MainResourceKey{.role = MainResourceRole::op_menu_background},
		MainResourceKey{.role = MainResourceRole::op_window_sheet},
		MainResourceKey{.role = MainResourceRole::op_gaiji_font},
		MainResourceKey{.role = MainResourceRole::op_menu_music},
	};
	expect(
		keys.size() == expected.size() &&
			std::equal(keys.begin(), keys.end(), expected.begin()),
		"OP title closure preserves TL01, TL02, OPWIN, MIKOFT, then OP.M"
	);
	const auto tl01 = th03::app::main_resource_spec(keys[0U]);
	const auto tl02 = th03::app::main_resource_spec(keys[1U]);
	const auto opwin = th03::app::main_resource_spec(keys[2U]);
	const auto gaiji = th03::app::main_resource_spec(keys[3U]);
	const auto music = th03::app::main_resource_spec(keys[4U]);
	expect(
		tl01 && tl01->filename == "TL01.PI" && tl01->expected_bytes == 4192U &&
		tl02 && tl02->filename == "TL02.PI" && tl02->expected_bytes == 31779U &&
		opwin && opwin->filename == "OPWIN.BFT" &&
			opwin->expected_bytes == 3152U &&
		gaiji && gaiji->filename == "MIKOFT.BFT" &&
			gaiji->expected_bytes == 8224U &&
		music && music->filename == "OP.M" && music->expected_bytes == 4431U,
		"OP title manifest records the measured original extraction sizes"
	);
	expect(
		!th03::app::main_resource_spec({
			.role = MainResourceRole::op_menu_background,
			.character = MainCharacter::reimu,
		}),
		"OP static roles reject unrelated character selectors"
	);

	TemporaryRoot root;
	expect(root.valid(), "temporary OP transaction root was created");
	if(!root.valid()) {
		return;
	}
	for(std::size_t index = 0U; index + 1U < keys.size(); ++index) {
		write_spec(root.path(), keys[index], std::byte{0x51U});
	}
	MainResourceLoader loader(root.path(), MainResourceIntegrity::size_only);
	const auto failed = loader.load_op_title_menu();
	expect(
		!failed && loader.cached_resource_count() == 0U,
		"late OP.M failure publishes no partial title cache"
	);
	write_spec(root.path(), keys.back(), std::byte{0x52U});
	const auto loaded = loader.load_op_title_menu();
	expect(
		loaded && loaded->size() == keys.size() &&
			loader.cached_resource_count() == keys.size(),
		"complete OP title closure commits atomically"
	);
}

void test_resolved_path_escape_if_supported()
{
	TemporaryRoot content_root;
	TemporaryRoot outside_root;
	expect(content_root.valid() && outside_root.valid(),
		"temporary roots for link-escape test were created");
	if(!content_root.valid() || !outside_root.valid()) {
		return;
	}
	write_sized_file(
		outside_root.path(), MainResourceArchive::yume2, "ENEDAT.DAT", 5072U,
		std::byte{0U}
	);
	std::error_code filesystem_error;
	fs::create_directories(content_root.path() / "YUME2.DAT", filesystem_error);
	if(filesystem_error) {
		expect(false, "link-escape test created its synthetic archive directory");
		return;
	}
	fs::create_symlink(
		outside_root.path() / "YUME2.DAT" / "ENEDAT.DAT",
		content_root.path() / "YUME2.DAT" / "ENEDAT.DAT",
		filesystem_error
	);
	if(filesystem_error) {
		expect(true, "host denied synthetic links; no unsafe fallback is attempted");
		return;
	}
	MainResourceLoader loader(content_root.path(), MainResourceIntegrity::size_only);
	const auto escaped = loader.load({.role = MainResourceRole::enemy_scripts});
	expect(
		!escaped && escaped.error().code == ResourceLoadErrorCode::path_escape,
		"a resolved symbolic-link escape is rejected after canonicalization"
	);
}

void test_direct_vs_batch_and_cache()
{
	TemporaryRoot root;
	expect(root.valid(), "temporary synthetic root was created");
	if(!root.valid()) {
		return;
	}
	const MainPlayerSelection player_one{
		.character = MainCharacter::reimu,
		.palette = MainPaletteVariant::first,
	};
	const MainPlayerSelection player_two{
		.character = MainCharacter::yumemi,
		.palette = MainPaletteVariant::second,
	};
	const auto keys = th03::app::direct_vs_resource_keys(player_one, player_two);
	expect(
		keys && keys->size() == 17U,
		"direct VS includes five static MAIN resources and the full MAINL atlas handoff"
	);
	if(!keys) {
		return;
	}
	const std::array expected_keys{
		MainResourceKey{.role = MainResourceRole::enemy_scripts},
		MainResourceKey{.role = MainResourceRole::game_font},
		MainResourceKey{.role = MainResourceRole::hud_lose_sheet},
		MainResourceKey{.role = MainResourceRole::hud_round_sheet},
		MainResourceKey{.role = MainResourceRole::hud_result_sheet},
		MainResourceKey{.role = MainResourceRole::stage_enemy_center},
		MainResourceKey{.role = MainResourceRole::character_sheet,
			.character = MainCharacter::reimu,
			.palette = MainPaletteVariant::first},
		MainResourceKey{.role = MainResourceRole::character_extra_portrait,
			.character = MainCharacter::reimu,
			.palette = MainPaletteVariant::first},
		MainResourceKey{.role = MainResourceRole::character_bomb_portrait,
			.character = MainCharacter::reimu,
			.palette = MainPaletteVariant::first},
		MainResourceKey{.role = MainResourceRole::character_stage_shot,
			.character = MainCharacter::reimu,
			.palette = MainPaletteVariant::first},
		MainResourceKey{.role = MainResourceRole::character_stage_extra_shot,
			.character = MainCharacter::reimu,
			.palette = MainPaletteVariant::first},
		MainResourceKey{.role = MainResourceRole::character_sheet,
			.character = MainCharacter::yumemi,
			.palette = MainPaletteVariant::second},
		MainResourceKey{.role = MainResourceRole::character_extra_portrait,
			.character = MainCharacter::yumemi,
			.palette = MainPaletteVariant::second},
		MainResourceKey{.role = MainResourceRole::character_bomb_portrait,
			.character = MainCharacter::yumemi,
			.palette = MainPaletteVariant::second},
		MainResourceKey{.role = MainResourceRole::character_stage_shot,
			.character = MainCharacter::yumemi,
			.palette = MainPaletteVariant::second},
		MainResourceKey{.role = MainResourceRole::character_stage_extra_shot,
			.character = MainCharacter::yumemi,
			.palette = MainPaletteVariant::second},
		MainResourceKey{.role = MainResourceRole::character_stage_enemy_group,
			.character = MainCharacter::yumemi},
	};
	expect(
		keys->size() == expected_keys.size() &&
			std::equal(keys->begin(), keys->end(), expected_keys.begin()),
		"direct VS keys preserve exact static then P1 then P2 source order"
	);
	std::byte fill{1U};
	for(const auto key : *keys) {
		write_spec(root.path(), key, fill);
		fill = std::byte{static_cast<unsigned char>(
			std::to_integer<unsigned char>(fill) + 1U
		)};
	}

	MainResourceLoader loader(root.path(), MainResourceIntegrity::size_only);
	const auto batch = loader.load_direct_vs(player_one, player_two);
	expect(
		batch && batch->size() == keys->size() &&
			loader.cached_resource_count() == keys->size(),
		"direct VS loads one deterministic, scene-scoped batch"
	);
	if(!batch) {
		return;
	}
	expect(
		batch->front().spec.key ==
			MainResourceKey{.role = MainResourceRole::enemy_scripts} &&
			std::is_const_v<std::remove_reference_t<
				decltype(*batch->front().bytes)>> &&
			!batch->front().bytes->empty(),
		"loaded bytes are tagged with their manifest spec and immutable ownership"
	);

	const auto enemy_key = MainResourceKey{.role = MainResourceRole::enemy_scripts};
	const auto first = loader.load(enemy_key);
	expect(first && (*first)->at(0U) == std::byte{1U}, "first resource exposes loaded bytes");
	write_spec(root.path(), enemy_key, std::byte{0x7FU});
	const auto second = loader.load(enemy_key);
	expect(
		first && second && *first == *second && (*second)->at(0U) == std::byte{1U},
		"a scene cache prevents later filesystem reads from changing loaded bytes"
	);

	const auto duplicate_selection = th03::app::direct_vs_resource_keys(
		player_one, player_one
	);
	const std::array expected_duplicate_keys{
		MainResourceKey{.role = MainResourceRole::enemy_scripts},
		MainResourceKey{.role = MainResourceRole::game_font},
		MainResourceKey{.role = MainResourceRole::hud_lose_sheet},
		MainResourceKey{.role = MainResourceRole::hud_round_sheet},
		MainResourceKey{.role = MainResourceRole::hud_result_sheet},
		MainResourceKey{.role = MainResourceRole::stage_enemy_center},
		MainResourceKey{.role = MainResourceRole::character_sheet,
			.character = MainCharacter::reimu,
			.palette = MainPaletteVariant::first},
		MainResourceKey{.role = MainResourceRole::character_extra_portrait,
			.character = MainCharacter::reimu,
			.palette = MainPaletteVariant::first},
		MainResourceKey{.role = MainResourceRole::character_bomb_portrait,
			.character = MainCharacter::reimu,
			.palette = MainPaletteVariant::first},
		MainResourceKey{.role = MainResourceRole::character_stage_shot,
			.character = MainCharacter::reimu,
			.palette = MainPaletteVariant::first},
		MainResourceKey{.role = MainResourceRole::character_stage_extra_shot,
			.character = MainCharacter::reimu,
			.palette = MainPaletteVariant::first},
		MainResourceKey{.role = MainResourceRole::character_stage_enemy_group,
			.character = MainCharacter::reimu},
	};
	expect(
		duplicate_selection &&
			duplicate_selection->size() == expected_duplicate_keys.size() &&
			std::equal(
				duplicate_selection->begin(), duplicate_selection->end(),
				expected_duplicate_keys.begin()
			),
		"identical selections preserve exact deduplicated order and opponent group"
	);
}

void test_main_private_and_mainl_stage_batches()
{
	const MainPlayerSelection reimu_first{
		.character = MainCharacter::reimu,
		.palette = MainPaletteVariant::first,
	};
	const MainPlayerSelection mima_second{
		.character = MainCharacter::mima,
		.palette = MainPaletteVariant::second,
	};
	const MainPlayerSelection chiyuri_first{
		.character = MainCharacter::chiyuri,
		.palette = MainPaletteVariant::first,
	};
	const MainPlayerSelection yumemi_second{
		.character = MainCharacter::yumemi,
		.palette = MainPaletteVariant::second,
	};

	const auto private_keys = th03::app::main_private_resource_keys(
		reimu_first, mima_second
	);
	expect(
		private_keys && private_keys->size() == 6U &&
			private_keys->front() == MainResourceKey{
				.role = MainResourceRole::character_sheet,
				.character = MainCharacter::reimu,
				.palette = MainPaletteVariant::first,
			} &&
			private_keys->back() == MainResourceKey{
				.role = MainResourceRole::character_bomb_portrait,
				.character = MainCharacter::mima,
				.palette = MainPaletteVariant::second,
			},
		"MAIN-private batch contains only exact P1/P2 process owners"
	);
	const auto duplicate_private = th03::app::main_private_resource_keys(
		reimu_first, reimu_first
	);
	expect(
		duplicate_private && duplicate_private->size() == 3U,
		"MAIN-private batch deduplicates an identical paletted selection"
	);
	const auto static_keys = th03::app::main_static_resource_keys();
	expect(
		static_keys.size() == 5U && static_keys.front() == MainResourceKey{
			.role = MainResourceRole::enemy_scripts,
		} && static_keys.back() == MainResourceKey{
			.role = MainResourceRole::hud_result_sheet,
		},
		"MAIN-static batch is the exact five campaign-owned resources"
	);

	const auto numbered = th03::app::mainl_stage_resource_keys({
		.player_one = reimu_first,
		.player_two = mima_second,
		.game_mode = 0x01U,
		.story_stage = 2U,
	});
	expect(
		numbered && numbered->size() == 12U &&
			std::find(numbered->begin(), numbered->end(), MainResourceKey{
				.role = MainResourceRole::stage_number_sheet,
			}) != numbered->end() &&
			std::find(numbered->begin(), numbered->end(), MainResourceKey{
				.role = MainResourceRole::stage_splash_background,
				.stage_background = MainlStageBackground::numbered,
			}) != numbered->end(),
		"ordinary Story resolves ST.CD2 and the numbered STNX background"
	);
	const auto chiyuri = th03::app::mainl_stage_resource_keys({
		.player_one = reimu_first,
		.player_two = chiyuri_first,
		.game_mode = 0x01U,
		.story_stage = 6U,
	});
	expect(
		chiyuri &&
			std::find(chiyuri->begin(), chiyuri->end(), MainResourceKey{
				.role = MainResourceRole::stage_number_sheet,
			}) == chiyuri->end() &&
			std::find(chiyuri->begin(), chiyuri->end(), MainResourceKey{
				.role = MainResourceRole::stage_splash_background,
				.stage_background = MainlStageBackground::chiyuri,
			}) != chiyuri->end(),
		"P2 Chiyuri background takes precedence over decisive-stage selection"
	);
	const auto non_story = th03::app::mainl_stage_resource_keys({
		.player_one = reimu_first,
		.player_two = mima_second,
		.game_mode = 0x81U,
		.story_stage = 0xFFU,
	});
	expect(
		non_story &&
			std::find(non_story->begin(), non_story->end(), MainResourceKey{
				.role = MainResourceRole::stage_splash_background,
				.stage_background = MainlStageBackground::non_story,
			}) != non_story->end(),
		"non-Story stage resolution ignores the Story-stage domain and selects STNX5"
	);
	expect(
		!th03::app::mainl_stage_resource_keys({
			.player_one = reimu_first,
			.player_two = mima_second,
			.game_mode = 0x01U,
			.story_stage = 9U,
		}),
		"Story stage resources reject a stage beyond the stock domain"
	);

	TemporaryRoot root;
	expect(root.valid(), "temporary MAINL transaction root was created");
	if(!root.valid() || !numbered) {
		return;
	}
	for(std::size_t index = 0U; index + 1U < numbered->size(); ++index) {
		write_spec(root.path(), (*numbered)[index], std::byte{0x41U});
	}
	MainResourceLoader loader(root.path(), MainResourceIntegrity::size_only);
	const auto failed = loader.load_mainl_stage({
		.player_one = reimu_first,
		.player_two = mima_second,
		.game_mode = 0x01U,
		.story_stage = 2U,
	});
	expect(
		!failed && loader.cached_resource_count() == 0U,
		"a late MAINL batch failure publishes no partial cache entries"
	);
	write_spec(root.path(), numbered->back(), std::byte{0x42U});
	const auto loaded = loader.load_mainl_stage({
		.player_one = reimu_first,
		.player_two = mima_second,
		.game_mode = 0x01U,
		.story_stage = 2U,
	});
	expect(
		loaded && loaded->size() == numbered->size() &&
			loader.cached_resource_count() == numbered->size(),
		"a complete MAINL batch commits all immutable resources atomically"
	);

	const auto win_keys = th03::app::mainl_win_resource_keys(yumemi_second);
	expect(
		win_keys && win_keys->size() == 6U &&
			win_keys->front() == MainResourceKey{
				.role = MainResourceRole::win_logo_palette_pre,
			} &&
			(*win_keys)[3U] == MainResourceKey{
				.role = MainResourceRole::character_win_portrait,
				.character = MainCharacter::yumemi,
			} &&
			(*win_keys)[4U] == MainResourceKey{
				.role = MainResourceRole::character_win_message,
				.character = MainCharacter::yumemi,
			} &&
			win_keys->back() == MainResourceKey{
				.role = MainResourceRole::win_logo_palette_post,
			},
		"winner batch preserves the source load order and base-character names"
	);
	expect(
		!th03::app::mainl_win_resource_keys({
			.character = MainCharacter::none,
			.palette = MainPaletteVariant::first,
		}),
		"winner batch rejects a non-character selection"
	);

	TemporaryRoot win_root;
	expect(win_root.valid(), "temporary winner transaction root was created");
	if(!win_root.valid() || !win_keys) {
		return;
	}
	for(std::size_t index = 0U; index + 1U < win_keys->size(); ++index) {
		write_spec(win_root.path(), (*win_keys)[index], std::byte{0x51U});
	}
	MainResourceLoader win_loader(
		win_root.path(), MainResourceIntegrity::size_only
	);
	const auto failed_win = win_loader.load_mainl_win(yumemi_second);
	expect(
		!failed_win && win_loader.cached_resource_count() == 0U,
		"a late winner batch failure publishes no partial cache entries"
	);
	write_spec(win_root.path(), win_keys->back(), std::byte{0x52U});
	const auto loaded_win = win_loader.load_mainl_win(yumemi_second);
	expect(
		loaded_win && loaded_win->size() == win_keys->size() &&
			win_loader.cached_resource_count() == win_keys->size(),
		"a complete winner batch commits all immutable resources atomically"
	);

	const auto no_credit = th03::app::mainl_continue_resource_keys(0U);
	const auto credited = th03::app::mainl_continue_resource_keys(3U);
	expect(
		no_credit && no_credit->size() == 1U && no_credit->front() ==
			MainResourceKey{.role = MainResourceRole::gameover_background},
		"zero-credit Continue resolves only the registration-preloaded OVER image"
	);
	expect(
		credited && credited->size() == 2U && credited->front() ==
			MainResourceKey{.role = MainResourceRole::continue_background} &&
			credited->back() == MainResourceKey{
				.role = MainResourceRole::continue_choice_cels,
			},
		"credited registration preloads CONTI background and cels without OVER"
	);
	expect(
		th03::app::mainl_gameover_resource_keys() ==
			std::vector<MainResourceKey>{{
				.role = MainResourceRole::gameover_background,
			}},
		"Continue black-out resolves OVER as a separate late batch"
	);
	expect(
		!th03::app::mainl_continue_resource_keys(4U),
		"Continue resources reject a credit count outside the resident domain"
	);

	TemporaryRoot continue_root;
	expect(continue_root.valid(), "temporary Continue transaction root was created");
	if(!continue_root.valid() || !credited) {
		return;
	}
	for(std::size_t index = 0U; index + 1U < credited->size(); ++index) {
		write_spec(continue_root.path(), (*credited)[index], std::byte{0x61U});
	}
	MainResourceLoader continue_loader(
		continue_root.path(), MainResourceIntegrity::size_only
	);
	const auto failed_continue = continue_loader.load_mainl_continue(3U);
	expect(
		!failed_continue && continue_loader.cached_resource_count() == 0U,
		"a late Continue batch failure publishes no partial cache entries"
	);
	write_spec(continue_root.path(), credited->back(), std::byte{0x62U});
	const auto loaded_continue = continue_loader.load_mainl_continue(3U);
	expect(
		loaded_continue && loaded_continue->size() == credited->size() &&
			continue_loader.cached_resource_count() == credited->size(),
		"a complete Continue batch commits all immutable resources atomically"
	);
}

void test_cutscene_resource_batches()
{
	const auto reimu_chiyuri = th03::app::mainl_cutscene_resource_keys({
		.kind = MainlCutsceneKind::interstage_chiyuri,
		.character = MainCharacter::reimu,
	});
	const auto yumemi_yumemi = th03::app::mainl_cutscene_resource_keys({
		.kind = MainlCutsceneKind::interstage_yumemi,
		.character = MainCharacter::yumemi,
	});
	const auto kana_ending = th03::app::mainl_cutscene_resource_keys({
		.kind = MainlCutsceneKind::ending,
		.character = MainCharacter::kana,
	});
	const auto extra = th03::app::mainl_cutscene_resource_keys({
		.kind = MainlCutsceneKind::extra_ending,
		.character = MainCharacter::chiyuri,
	});
	expect(
		reimu_chiyuri && reimu_chiyuri->size() == 31U &&
			reimu_chiyuri->front() == MainResourceKey{
				.role = MainResourceRole::mainl_gaiji_font,
			} && (*reimu_chiyuri)[1U] == MainResourceKey{
				.role = MainResourceRole::cutscene_script,
				.cutscene_script = MainCutsceneScript::dm0_reimu,
			} && (*reimu_chiyuri)[2U] == MainResourceKey{
				.role = MainResourceRole::cutscene_picture,
				.cutscene_picture = MainCutscenePicture::dm1a,
			} && reimu_chiyuri->back() == MainResourceKey{
				.role = MainResourceRole::cutscene_music,
				.cutscene_music = MainCutsceneMusic::demo5,
			},
		"cutscene batch is font, selected script, all pictures, then all music"
	);
	expect(
		yumemi_yumemi && (*yumemi_yumemi)[1U].cutscene_script ==
			MainCutsceneScript::dm1_yumemi &&
		kana_ending && (*kana_ending)[1U].cutscene_script ==
			MainCutsceneScript::ending_kana &&
		extra && (*extra)[1U].cutscene_script ==
			MainCutsceneScript::extra_ending,
		"cutscene request resolves all four source filename mutations"
	);
	expect(
		!th03::app::mainl_cutscene_resource_keys({
			.kind = MainlCutsceneKind::ending,
			.character = MainCharacter::none,
		}) &&
		!th03::app::mainl_cutscene_resource_keys({
			.kind = static_cast<MainlCutsceneKind>(0xFFU),
			.character = MainCharacter::reimu,
		}),
		"cutscene request rejects invalid character and scene domains"
	);

	TemporaryRoot root;
	expect(root.valid(), "temporary cutscene transaction root was created");
	if(!root.valid() || !kana_ending) {
		return;
	}
	for(std::size_t index = 0U; index + 1U < kana_ending->size(); ++index) {
		write_spec(root.path(), (*kana_ending)[index], std::byte{0x71U});
	}
	MainResourceLoader loader(root.path(), MainResourceIntegrity::size_only);
	const auto failed = loader.load_mainl_cutscene({
		.kind = MainlCutsceneKind::ending,
		.character = MainCharacter::kana,
	});
	expect(
		!failed && loader.cached_resource_count() == 0U,
		"a late cutscene closure failure publishes no partial cache entries"
	);
	write_spec(root.path(), kana_ending->back(), std::byte{0x72U});
	const auto loaded = loader.load_mainl_cutscene({
		.kind = MainlCutsceneKind::ending,
		.character = MainCharacter::kana,
	});
	expect(
		loaded && loaded->size() == kana_ending->size() &&
			loader.cached_resource_count() == kana_ending->size(),
		"the complete cutscene closure commits atomically"
	);
}

void test_staff_resource_batch()
{
	const auto keys = th03::app::mainl_staff_resource_keys();
	const std::array expected{
		MainResourceKey{.role = MainResourceRole::staff_background_palette},
		MainResourceKey{.role = MainResourceRole::staff_cdg,
			.staff_cdg = MainStaffCdg::staff_1},
		MainResourceKey{.role = MainResourceRole::staff_cdg,
			.staff_cdg = MainStaffCdg::staff_11},
		MainResourceKey{.role = MainResourceRole::staff_cdg,
			.staff_cdg = MainStaffCdg::staff_3},
		MainResourceKey{.role = MainResourceRole::staff_cdg,
			.staff_cdg = MainStaffCdg::staff_4},
		MainResourceKey{.role = MainResourceRole::staff_cdg,
			.staff_cdg = MainStaffCdg::staff_5},
		MainResourceKey{.role = MainResourceRole::staff_cdg,
			.staff_cdg = MainStaffCdg::staff_6},
		MainResourceKey{.role = MainResourceRole::staff_cdg,
			.staff_cdg = MainStaffCdg::staff_7},
		MainResourceKey{.role = MainResourceRole::staff_cdg,
			.staff_cdg = MainStaffCdg::staff_8},
		MainResourceKey{.role = MainResourceRole::staff_cdg,
			.staff_cdg = MainStaffCdg::staff_9},
		MainResourceKey{.role = MainResourceRole::staff_cdg,
			.staff_cdg = MainStaffCdg::staff_10},
		MainResourceKey{.role = MainResourceRole::staff_cdg,
			.staff_cdg = MainStaffCdg::staff_2},
		MainResourceKey{.role = MainResourceRole::staff_cdg,
			.staff_cdg = MainStaffCdg::staff_12},
		MainResourceKey{.role = MainResourceRole::staff_music},
	};
	expect(
		keys.size() == expected.size() &&
			std::equal(keys.begin(), keys.end(), expected.begin()),
		"staff closure preserves palette, source CDG slot order, then ED.M"
	);
	expect(
		!th03::app::main_resource_spec({
			.role = MainResourceRole::staff_music,
			.staff_cdg = MainStaffCdg::staff_1,
		}) && !th03::app::main_resource_spec({
			.role = MainResourceRole::staff_cdg,
			.staff_cdg = static_cast<MainStaffCdg>(0xFFU),
		}),
		"staff-only selectors cannot be forged onto another role or out of range"
	);

	TemporaryRoot root;
	expect(root.valid(), "temporary staff transaction root was created");
	if(!root.valid()) {
		return;
	}
	for(std::size_t index = 0U; index + 1U < keys.size(); ++index) {
		write_spec(root.path(), keys[index], std::byte{0x81U});
	}
	MainResourceLoader loader(root.path(), MainResourceIntegrity::size_only);
	const auto failed = loader.load_mainl_staff();
	expect(
		!failed && loader.cached_resource_count() == 0U,
		"a late staff closure failure publishes no partial cache entries"
	);
	write_spec(root.path(), keys.back(), std::byte{0x82U});
	const auto loaded = loader.load_mainl_staff();
	expect(
		loaded && loaded->size() == keys.size() &&
			loader.cached_resource_count() == keys.size(),
		"the complete staff closure commits all immutable resources atomically"
	);
}

void test_registration_resource_batch()
{
	const th03::app::MainlRegistrationResourceRequest request{
		.rank = MainRegistrationRank::hard,
		.next_screen = th03::app::MainlRegistrationNextScreen::continue_menu,
	};
	const auto keys = th03::app::mainl_registration_resource_keys(request);
	const std::array expected{
		MainResourceKey{.role = MainResourceRole::registration_background},
		MainResourceKey{.role = MainResourceRole::registration_rank,
			.registration_rank = MainRegistrationRank::hard},
		MainResourceKey{.role = MainResourceRole::registration_normal_glyphs},
		MainResourceKey{.role = MainResourceRole::registration_highlight_glyphs},
		MainResourceKey{.role = MainResourceRole::registration_music},
		MainResourceKey{.role = MainResourceRole::continue_background},
		MainResourceKey{.role = MainResourceRole::continue_choice_cels},
	};
	expect(
		keys && keys->size() == expected.size() &&
			std::equal(keys->begin(), keys->end(), expected.begin()),
		"registration closure preserves source assets then the credited Continue preload order"
	);
	expect(
		!th03::app::mainl_registration_resource_keys({
			.rank = MainRegistrationRank::none,
		}) &&
		!th03::app::main_resource_spec({
			.role = MainResourceRole::registration_music,
			.registration_rank = MainRegistrationRank::easy,
		}),
		"registration rank selectors are required only for the rank plate"
	);

	TemporaryRoot root;
	expect(root.valid(), "temporary registration transaction root was created");
	if(!root.valid() || !keys) {
		return;
	}
	for(std::size_t index = 0U; index + 1U < keys->size(); ++index) {
		write_spec(root.path(), (*keys)[index], std::byte{0x91U});
	}
	MainResourceLoader loader(root.path(), MainResourceIntegrity::size_only);
	const auto failed = loader.load_mainl_registration(request);
	expect(
		!failed && loader.cached_resource_count() == 0U,
		"a late registration closure failure publishes no partial cache entries"
	);
	write_spec(root.path(), keys->back(), std::byte{0x92U});
	const auto loaded = loader.load_mainl_registration(request);
	expect(
		loaded && loaded->size() == keys->size() &&
			loader.cached_resource_count() == keys->size(),
		"the complete registration closure commits all immutable resources atomically"
	);
	const auto gameover = th03::app::mainl_registration_resource_keys({
		.rank = MainRegistrationRank::hard,
		.next_screen = th03::app::MainlRegistrationNextScreen::game_over,
	});
	expect(
		gameover && gameover->size() == 6U &&
			gameover->back().role == MainResourceRole::gameover_background,
		"terminal registration preloads only OVER after the shared five assets"
	);
}

void test_rejections()
{
	const auto enemy_key = MainResourceKey{.role = MainResourceRole::enemy_scripts};
	TemporaryRoot missing_parent;
	expect(missing_parent.valid(), "temporary missing-root parent was created");
	if(!missing_parent.valid()) {
		return;
	}
	MainResourceLoader missing_root(missing_parent.path() / "missing-root");
	const auto invalid_root = missing_root.load(enemy_key);
	expect(
		!invalid_root && invalid_root.error().code == ResourceLoadErrorCode::invalid_content_root,
		"missing content root fails before any resource path is accepted"
	);

	TemporaryRoot root;
	expect(root.valid(), "temporary rejection root was created");
	if(!root.valid()) {
		return;
	}
	MainResourceLoader loader(root.path());
	const auto missing = loader.load(enemy_key);
	expect(
		!missing && missing.error().code == ResourceLoadErrorCode::missing,
		"missing extracted entry is a named load failure"
	);
	write_sized_file(
		root.path(), MainResourceArchive::yume2, "ENEDAT.DAT", 1U, std::byte{0U}
	);
	const auto wrong_size = loader.load(enemy_key);
	expect(
		!wrong_size && wrong_size.error().code == ResourceLoadErrorCode::size_mismatch,
		"wrong-sized extracted entry is rejected deterministically"
	);

	TemporaryRoot checksum_root;
	expect(checksum_root.valid(), "temporary checksum root was created");
	if(!checksum_root.valid()) {
		return;
	}
	write_sized_file(
		checksum_root.path(), MainResourceArchive::yume2, "ENEDAT.DAT", 5072U,
		std::byte{0U}
	);
	MainResourceLoader checksum_loader(checksum_root.path());
	const auto wrong_hash = checksum_loader.load(enemy_key);
	expect(
		!wrong_hash && wrong_hash.error().code ==
			ResourceLoadErrorCode::checksum_mismatch,
		"right-sized but altered data is rejected with a named checksum error"
	);

	TemporaryRoot case_root;
	expect(case_root.valid(), "temporary case root was created");
	if(!case_root.valid()) {
		return;
	}
	write_sized_file(
		case_root.path(), MainResourceArchive::yume2, "enedat.dat", 5072U,
		std::byte{0U}
	);
	MainResourceLoader case_loader(
		case_root.path(), MainResourceIntegrity::size_only
	);
	const auto wrong_case = case_loader.load(enemy_key);
	expect(
		!wrong_case && wrong_case.error().code == ResourceLoadErrorCode::case_mismatch,
		"uppercase manifest names reject case-only extracted filenames"
	);

	const auto malformed_key = loader.load({
		.role = MainResourceRole::character_sheet,
		.character = MainCharacter::none,
		.palette = MainPaletteVariant::none,
	});
	expect(
		!malformed_key &&
			malformed_key.error().code == ResourceLoadErrorCode::invalid_resource_key,
		"callers cannot turn a logical role into an arbitrary traversal pathname"
	);
	const auto malformed_group = loader.load({
		.role = MainResourceRole::character_stage_enemy_group,
		.character = MainCharacter::reimu,
		.palette = MainPaletteVariant::first,
	});
	expect(
		!malformed_group && malformed_group.error().code ==
			ResourceLoadErrorCode::invalid_resource_key,
		"opponent-group selection rejects a palette variant absent from the source"
	);
}

} // namespace

int main()
{
	test_manifest();
	test_op_title_menu_resource_batch();
	test_direct_vs_batch_and_cache();
	test_main_private_and_mainl_stage_batches();
	test_cutscene_resource_batches();
	test_staff_resource_batch();
	test_registration_resource_batch();
	test_rejections();
	test_resolved_path_escape_if_supported();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return (failures == 0U) ? 0 : 1;
}
