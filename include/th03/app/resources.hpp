#ifndef TH03_APP_RESOURCES_HPP
#define TH03_APP_RESOURCES_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace th03::app {

// This is the application/import boundary.  th03_assets receives only spans
// borrowed from ResourceBytes and never sees these paths or this cache.
enum class MainResourceArchive : std::uint8_t {
	yume1,
	yume2,
};

enum class MainResourceScene : std::uint8_t {
	direct_vs,
	selection_handoff,
	mainl_stage_transition,
	mainl_win,
	mainl_continue_gameover,
	mainl_cutscene,
	mainl_staff,
};

enum class MainResourceDecoder : std::uint8_t {
	enedat,
	rgb48,
	bfnt_gaiji,
	bfnt_superimpose,
	cdg,
	mrs,
	pi,
	shift_jis_blocks,
	cutscene_script,
	pmd_music,
};

enum class MainCharacter : std::uint8_t {
	none,
	reimu,
	mima,
	marisa,
	ellen,
	kotohime,
	kana,
	rikako,
	chiyuri,
	yumemi,
};

enum class MainPaletteVariant : std::uint8_t {
	none,
	first,
	second,
};

enum class MainlStageBackground : std::uint8_t {
	none,
	numbered,
	yumemi,
	chiyuri,
	decisive,
	non_story,
};

enum class MainlCutsceneKind : std::uint8_t {
	interstage_chiyuri,
	interstage_yumemi,
	ending,
	extra_ending,
};

enum class MainCutsceneScript : std::uint8_t {
	none,
	dm0_reimu,
	dm0_mima,
	dm0_marisa,
	dm0_ellen,
	dm0_kotohime,
	dm0_kana,
	dm0_rikako,
	dm0_chiyuri,
	dm0_yumemi,
	dm1_reimu,
	dm1_mima,
	dm1_marisa,
	dm1_ellen,
	dm1_kotohime,
	dm1_kana,
	dm1_rikako,
	dm1_chiyuri,
	dm1_yumemi,
	ending_reimu,
	ending_mima,
	ending_marisa,
	ending_ellen,
	ending_kotohime,
	ending_kana,
	ending_rikako,
	ending_chiyuri,
	ending_yumemi,
	extra_ending,
};

enum class MainCutscenePicture : std::uint8_t {
	none,
	dm1a,
	dm1b,
	dm1c,
	dm1d,
	dm1e,
	dm1f,
	dm2a,
	dm3a,
	dm3b,
	dm3c,
	dm3d,
	dm3e,
	dm3f,
	dm3g,
	dm3h,
	dm3i,
	dm3j,
	dm3k,
	dm3l,
	dm3m,
	dm3n,
	dm3o,
	dm3p,
	dm4a,
};

enum class MainCutsceneMusic : std::uint8_t {
	none,
	demo1,
	demo2,
	demo3,
	demo4,
	demo5,
};

// Source CDG slot order from staffroll_and_verdict_animate(). The filenames
// are intentionally not numerically ordered after slot 0.
enum class MainStaffCdg : std::uint8_t {
	none,
	staff_1,
	staff_11,
	staff_3,
	staff_4,
	staff_5,
	staff_6,
	staff_7,
	staff_8,
	staff_9,
	staff_10,
	staff_2,
	staff_12,
};

// Static roles use character == none and palette == none. Paletted character
// roles use both fields, making the original 00..17 family explicit. The
// stage enemy-group role uses character only because MAINL selects one of five
// ENEMY files from P2's base character, independent of its palette variant.
enum class MainResourceRole : std::uint8_t {
	enemy_scripts,
	game_font,
	hud_lose_sheet,
	hud_round_sheet,
	hud_result_sheet,
	stage_enemy_center,
	selection_palette,
	character_sheet,
	character_extra_portrait,
	character_bomb_portrait,
	character_stage_shot,
	character_stage_extra_shot,
	character_stage_enemy_group,
	mainl_gaiji_font,
	character_stage_portrait,
	stage_number_sheet,
	stage_splash_base,
	stage_splash_background,
	win_logo_palette_pre,
	win_logo_fade_cels,
	win_logo_final,
	win_logo_palette_post,
	character_win_portrait,
	character_win_message,
	continue_background,
	continue_choice_cels,
	gameover_background,
	cutscene_script,
	cutscene_picture,
	cutscene_music,
	staff_background_palette,
	staff_cdg,
	staff_music,
};

struct MainResourceKey {
	MainResourceRole role = MainResourceRole::enemy_scripts;
	MainCharacter character = MainCharacter::none;
	MainPaletteVariant palette = MainPaletteVariant::none;
	MainlStageBackground stage_background = MainlStageBackground::none;
	MainCutsceneScript cutscene_script = MainCutsceneScript::none;
	MainCutscenePicture cutscene_picture = MainCutscenePicture::none;
	MainCutsceneMusic cutscene_music = MainCutsceneMusic::none;
	MainStaffCdg staff_cdg = MainStaffCdg::none;

	friend constexpr auto operator<=>(
		const MainResourceKey &,
		const MainResourceKey &
	) = default;
};

struct MainResourceSpec {
	MainResourceKey key{};
	MainResourceScene scene = MainResourceScene::direct_vs;
	MainResourceArchive archive = MainResourceArchive::yume2;
	MainResourceDecoder decoder = MainResourceDecoder::enedat;
	std::string logical_role{};
	std::string filename{};
	std::size_t expected_bytes = 0U;
	// FNV-1a-64 is a compact corruption fingerprint, not a security signature.
	std::uint64_t expected_fnv1a64 = 0U;

	friend bool operator==(const MainResourceSpec &, const MainResourceSpec &) =
		default;
};

enum class ResourceLoadErrorCode : std::uint8_t {
	invalid_resource_key,
	duplicate_logical_role,
	invalid_content_root,
	path_escape,
	case_mismatch,
	missing,
	not_directory,
	not_regular_file,
	size_mismatch,
	checksum_mismatch,
	read_failed,
};

struct ResourceLoadError {
	ResourceLoadErrorCode code = ResourceLoadErrorCode::invalid_resource_key;
	MainResourceKey key{};
};

// Immutable ownership keeps a loaded span valid after a scene batch returns
// while the scene-scoped loader retains its cache.
using ResourceBytes = std::shared_ptr<const std::vector<std::byte>>;

struct LoadedMainResource {
	MainResourceSpec spec{};
	ResourceBytes bytes{};
};

struct MainPlayerSelection {
	MainCharacter character = MainCharacter::reimu;
	MainPaletteVariant palette = MainPaletteVariant::first;
};

struct MainlStageResourceRequest {
	MainPlayerSelection player_one{};
	MainPlayerSelection player_two{};
	std::uint8_t game_mode = 0U;
	std::uint8_t story_stage = 0U;
};

struct MainlCutsceneResourceRequest {
	MainlCutsceneKind kind = MainlCutsceneKind::interstage_chiyuri;
	MainCharacter character = MainCharacter::reimu;
};

enum class MainResourceIntegrity : std::uint8_t {
	// Normal application policy: exact size plus the checked-in FNV-1a-64
	// fingerprint from the accepted extraction.
	size_and_fnv1a64,
	// Kept for hermetic synthetic tests and tools that deliberately inspect
	// alternate-but-size-compatible data. Native app startup uses the default.
	size_only,
};

// The manifest is generated from the exact static names and the source's
// 00..17 character filename family. It contains metadata only, never bytes.
[[nodiscard]] std::vector<MainResourceSpec> main_resource_manifest();

// Manifest construction is fixed in this translation unit, but keeping this
// validation public makes an import/tooling audit report duplicate logical
// roles deterministically instead of relying on an accidental last-wins map.
[[nodiscard]] std::expected<void, ResourceLoadErrorCode>
validate_main_resource_manifest(std::span<const MainResourceSpec> manifest);

[[nodiscard]] std::expected<MainResourceSpec, ResourceLoadError>
main_resource_spec(MainResourceKey key);

[[nodiscard]] std::expected<std::vector<MainResourceKey>, ResourceLoadError>
direct_vs_resource_keys(
	MainPlayerSelection player_one,
	MainPlayerSelection player_two
);

[[nodiscard]] std::vector<MainResourceKey> main_static_resource_keys();

[[nodiscard]] std::expected<std::vector<MainResourceKey>, ResourceLoadError>
main_private_resource_keys(
	MainPlayerSelection player_one,
	MainPlayerSelection player_two
);

[[nodiscard]] std::expected<std::vector<MainResourceKey>, ResourceLoadError>
mainl_stage_resource_keys(MainlStageResourceRequest request);

[[nodiscard]] std::expected<std::vector<MainResourceKey>, ResourceLoadError>
mainl_win_resource_keys(MainPlayerSelection winner);

[[nodiscard]] std::expected<std::vector<MainResourceKey>, ResourceLoadError>
mainl_continue_resource_keys(std::uint8_t remaining_credits);

[[nodiscard]] std::expected<std::vector<MainResourceKey>, ResourceLoadError>
mainl_cutscene_resource_keys(MainlCutsceneResourceRequest request);

[[nodiscard]] std::vector<MainResourceKey> mainl_staff_resource_keys();

// Importer-facing validation for the extracted-entry component. The loader
// calls this before joining a manifest name to the explicit content root.
[[nodiscard]] bool is_safe_extracted_entry_name(std::string_view name) noexcept;

// Public because import diagnostics need to report the same compact
// fingerprint the normal loader enforces. This is not a security primitive.
[[nodiscard]] std::uint64_t main_resource_fnv1a64(
	std::span<const std::byte> bytes
) noexcept;

class MainResourceLoader {
public:
	// content_root is the extracted root containing YUME1.DAT/ and YUME2.DAT/.
	explicit MainResourceLoader(
		std::filesystem::path content_root,
		MainResourceIntegrity integrity = MainResourceIntegrity::size_and_fnv1a64
	);

	[[nodiscard]] std::expected<ResourceBytes, ResourceLoadError> load(
		MainResourceKey key
	);

	[[nodiscard]] std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
	load_direct_vs(
		MainPlayerSelection player_one,
		MainPlayerSelection player_two
	);
	[[nodiscard]] std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
	load_main_static();
	[[nodiscard]] std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
	load_main_private(
		MainPlayerSelection player_one,
		MainPlayerSelection player_two
	);
	[[nodiscard]] std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
	load_mainl_stage(MainlStageResourceRequest request);
	[[nodiscard]] std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
	load_mainl_win(MainPlayerSelection winner);
	[[nodiscard]] std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
	load_mainl_continue(std::uint8_t remaining_credits);
	[[nodiscard]] std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
	load_mainl_cutscene(MainlCutsceneResourceRequest request);
	[[nodiscard]] std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
	load_mainl_staff();

	[[nodiscard]] std::size_t cached_resource_count() const noexcept;

private:
	std::filesystem::path content_root_{};
	MainResourceIntegrity integrity_ = MainResourceIntegrity::size_and_fnv1a64;
	std::map<MainResourceKey, ResourceBytes> cache_{};
	[[nodiscard]] std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
	load_batch(std::span<const MainResourceKey> keys);
};

} // namespace th03::app

#endif
