#include "th03/app/resources.hpp"

#include <algorithm>
#include <array>
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
using th03::app::MainPaletteVariant;
using th03::app::MainPlayerSelection;
using th03::app::MainResourceArchive;
using th03::app::MainResourceIntegrity;
using th03::app::MainResourceKey;
using th03::app::MainResourceLoader;
using th03::app::MainResourceRole;
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
		for(std::uint32_t suffix = 0U; suffix < 1000U; ++suffix) {
			path_ = temporary / ("th03-app-resources-test-" +
				std::to_string(suffix));
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
		manifest.size() == 145U,
		"manifest enumerates MAIN, MAINL, paletted, and character-only assets"
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
			contains_stage_decisive,
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
	test_direct_vs_batch_and_cache();
	test_main_private_and_mainl_stage_batches();
	test_rejections();
	test_resolved_path_escape_if_supported();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return (failures == 0U) ? 0 : 1;
}
