#include "../src/app/classic_op_options_scene_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/app/resources.hpp"
#include "th03/assets/decode.hpp"
#include "th03/present/pi.hpp"
#include "th03/present/source_surface.hpp"
#include "th03/present/superimpose.hpp"

namespace {

using th03::GameInit;
using th03::app::ClassicAudioCommandType;
using th03::app::ClassicBgmMode;
using th03::app::ClassicConfigOptions;
using th03::app::ClassicKeyMode;
using th03::app::ClassicMusicCue;
using th03::app::ClassicOpErrorCode;
using th03::app::ClassicOpInput;
using th03::app::ClassicOpOptionsChoice;
using th03::app::ClassicOpOptionsOutcome;
using th03::app::ClassicOpOptionsPage;
using th03::app::ClassicOpOptionsPhase;
using th03::app::ClassicOpOptionsScene;
using th03::app::ClassicRank;
using th03::app::NewClassicWindowMode;
using th03::app::NewClassicWindowScale;
using th03::assets::BfntConsumerPolicy;
using th03::assets::BfntSheet;
using th03::assets::PiImage;
using th03::assets::PlanarBitmap4;
using th03::present::Page;

std::size_t checks = 0U;
std::size_t failures = 0U;

class EmptyAnk final : public th03::present::AnkGlyphProvider {
public:
	[[nodiscard]] std::optional<th03::present::AnkGlyph> glyph(
		std::uint8_t
	) const override {
		return std::nullopt;
	}
	[[nodiscard]] std::optional<th03::present::JisGlyph> jis_glyph(
		std::uint16_t canonical_jis
	) const override {
		th03::present::JisGlyph result{};
		result.row_halves[0U] = static_cast<std::uint8_t>(
			0x80U >> (canonical_jis & 0x07U)
		);
		return result;
	}
};

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

PiImage menu_background()
{
	PiImage result{};
	result.palette.emplace();
	result.image.width = th03::present::graphics_width;
	result.image.height = th03::present::graphics_height;
	result.image.indices.assign(th03::present::graphics_page_pixels, 4U);
	return result;
}

BfntSheet window_sheet()
{
	BfntSheet sheet{};
	sheet.consumer = BfntConsumerPolicy::superimpose;
	sheet.header.pixel_width = 16U;
	sheet.header.pixel_height = 128U;
	sheet.header.first_code = 0U;
	sheet.header.last_code = 2U;
	sheet.transparent_color = 0U;
	constexpr std::size_t plane_size = (16U / 8U) * 128U;
	for(std::uint16_t code = 0U; code < 3U; ++code) {
		PlanarBitmap4 bitmap{};
		for(auto &plane : bitmap.color_planes) {
			plane.assign(plane_size, 0U);
		}
		bitmap.color_planes[(code == 2U) ? 1U : 0U].assign(
			plane_size, 0xFFU
		);
		sheet.glyphs.push_back({code, std::move(bitmap)});
	}
	return sheet;
}

th03::present::Raster main_menu_raster()
{
	th03::present::Raster raster{};
	const auto background = menu_background();
	expect(
		raster.blit({
			.pixels = background.image.indices,
			.width = background.image.width,
			.height = background.image.height,
			.stride = background.image.width,
		}, {0, 0}),
		"synthetic main-menu background draws"
	);
	expect(
		raster.clear({280, 256, 16U, 128U}, 2U),
		"synthetic main-menu right edge draws"
	);
	return raster;
}

std::unique_ptr<ClassicOpOptionsScene> make_scene(
	ClassicConfigOptions config = {},
	bool fm_available = true,
	std::int32_t resident_rand = 40
)
{
	GameInit resident{};
	resident.rank = static_cast<std::uint8_t>(config.rank);
	resident.key_mode = static_cast<std::uint8_t>(config.key_mode);
	resident.resident_rand = resident_rand;
	auto created = ClassicOpOptionsScene::create(
		resident, config, fm_available, main_menu_raster(), {},
		menu_background(), window_sheet()
	);
	expect(created.has_value(), "synthetic Options scene creates");
	return created ? std::move(*created) : nullptr;
}

void test_replay_patch_option_unput_if_assets_supplied()
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
	const auto *path = std::getenv("TH03_AZINN_DAT");
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
	if(!path || *path == '\0') {
		return;
	}
	auto loaded = th03::app::load_new_classic_replay_patch_assets(path);
	expect(loaded.has_value(), "authenticated Replay Patch Option assets load");
	if(!loaded) {
		return;
	}
	auto assets = std::make_shared<const th03::app::NewClassicReplayPatchAssets>(
		std::move(*loaded)
	);
	auto raster = main_menu_raster();
	const auto background = menu_background();
	expect(raster.set_access_page(Page::one) && raster.blit({
		.pixels = background.image.indices,
		.width = background.image.width,
		.height = background.image.height,
		.stride = background.image.width,
	}, {0, 0}) && raster.set_access_page(Page::zero) &&
		raster.clear({162, 272, 132U, 112U}, 15U),
		"Replay Patch Option fixture has bare TL02 page 1 and rasterized main labels"
	);
	GameInit resident{};
	resident.rank = static_cast<std::uint8_t>(ClassicRank::normal);
	EmptyAnk font{};
	auto created = ClassicOpOptionsScene::create(
		resident, {}, true, std::move(raster), {}, menu_background(),
		window_sheet(), assets, &font, true
	);
	expect(created.has_value(), "Replay Patch Option scene creates");
	if(!created) {
		return;
	}
	auto &scene = **created;
	expect(scene.advance({.ok = true}).has_value(),
		"first Option expansion boundary advances");
	bool main_rows_clean = true;
	for(std::int32_t y = 272; y < 384 && main_rows_clean; ++y) {
		for(std::int32_t x = 162; x < 294; ++x) {
			const auto expected = x >= 288 ? 2U :
				(((x + (y - 264)) & 1) == 0 ? 3U : 4U);
			const auto actual = scene.raster().pixel(Page::zero, {x, y});
			main_rows_clean = main_rows_clean && actual == expected;
		}
	}
	expect(main_rows_clean,
		"Option grow immediately unputs main labels onto the transparent checker");
	for(std::uint16_t frame = 1U; frame < 17U; ++frame) {
		expect(scene.advance().has_value(), "Replay Patch Option grow advances");
	}
	expect(scene.advance().has_value(), "Replay Patch Option labels draw");
	expect(scene.advance().has_value(), "Replay Patch Option input unlocks");
	expect(scene.advance({.cancel = true}).has_value(),
		"Replay Patch Option cancel starts contraction");
	expect(scene.advance().has_value(),
		"first Option contraction boundary unputs all option labels");
	bool option_rows_clean = true;
	for(std::int32_t y = 272; y < 384 && option_rows_clean; ++y) {
		for(std::int32_t x = 162; x < 430; ++x) {
			const auto expected = x >= 416 ? 2U :
				(((x + (y - 264)) & 1) == 0 ? 3U : 4U);
			const auto actual = scene.raster().pixel(Page::zero, {x, y});
			option_rows_clean = option_rows_clean && actual == expected;
		}
	}
	expect(option_rows_clean,
		"Option contraction removes all proportional labels before shrinking");
}

std::unique_ptr<ClassicOpOptionsScene> make_patch_scene_if_assets_supplied(
	bool localized = false
)
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
	const auto *path = std::getenv("TH03_AZINN_DAT");
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
	if(!path || *path == '\0') {
		return {};
	}
	auto loaded = th03::app::load_new_classic_replay_patch_assets(path);
	expect(loaded.has_value(), "hierarchical Options assets load");
	if(!loaded) {
		return {};
	}
	if(localized) {
		const auto pack = std::filesystem::path{__FILE__}.parent_path().parent_path() /
			"localization" / "qps-ploc.t3lang";
		const auto &fallback = th03::app::new_classic_builtin_messages(
			th03::app::NewClassicLanguageAssetProfile::english
		);
		auto messages = th03::app::load_new_classic_message_pack(
			pack, loaded->menu_font, &fallback
		);
		expect(messages.has_value(),
			"width-stressing Options message package loads");
		if(!messages) {
			return {};
		}
		loaded->custom_messages =
			std::make_shared<const th03::app::NewClassicMessageCatalog>(
				std::move(*messages)
			);
	}
	auto assets = std::make_shared<const th03::app::NewClassicReplayPatchAssets>(
		std::move(*loaded)
	);
	auto raster = main_menu_raster();
	const auto background = menu_background();
	expect(raster.set_access_page(Page::one) && raster.blit({
		.pixels = background.image.indices,
		.width = background.image.width,
		.height = background.image.height,
		.stride = background.image.width,
	}, {0, 0}) && raster.set_access_page(Page::zero),
		"hierarchical Options fixture prepares bare title page");
	GameInit resident{};
	resident.rank = static_cast<std::uint8_t>(ClassicRank::normal);
	EmptyAnk font{};
	auto created = ClassicOpOptionsScene::create(
		resident, {}, true, std::move(raster), {}, menu_background(),
		window_sheet(), std::move(assets), &font, true, 35U
	);
	expect(created.has_value(), "hierarchical Options scene creates");
	return created ? std::move(*created) : nullptr;
}

void enter_patch_options(ClassicOpOptionsScene &scene);

void test_localized_option_snapshot_if_assets_supplied()
{
	auto baseline = make_patch_scene_if_assets_supplied();
	auto localized_a = make_patch_scene_if_assets_supplied(true);
	auto localized_b = make_patch_scene_if_assets_supplied(true);
	if(!baseline || !localized_a || !localized_b) {
		return;
	}
	enter_patch_options(*baseline);
	enter_patch_options(*localized_a);
	enter_patch_options(*localized_b);
	const auto baseline_hash = baseline->raster().display_state_hash();
	const auto localized_hash = localized_a->raster().display_state_hash();
	expect(localized_hash != baseline_hash,
		"pseudo-locale changes the representative Options publication");
	expect(localized_hash == localized_b->raster().display_state_hash(),
		"pseudo-locale representative Options publication is deterministic");
}

void enter_patch_options(ClassicOpOptionsScene &scene)
{
	for(std::uint16_t frame = 0U; frame < 17U; ++frame) {
		expect(scene.advance(
			frame == 0U ? ClassicOpInput{.ok = true} : ClassicOpInput{}
		).has_value(), "hierarchical Options grow advances");
	}
	expect(scene.advance().has_value(),
		"hierarchical category page publishes");
	expect(scene.advance().has_value(),
		"hierarchical transition input releases");
}

void test_replay_patch_option_hierarchy_if_assets_supplied()
{
	auto scene = make_patch_scene_if_assets_supplied();
	if(!scene) {
		return;
	}
	enter_patch_options(*scene);
	expect(scene->page() == ClassicOpOptionsPage::categories &&
		scene->selected_choice() ==
			ClassicOpOptionsChoice::category_gameplay,
		"New Classic Options opens on Gameplay in the category page");
	expect(scene->advance({.ok = true}).has_value() &&
		scene->page() == ClassicOpOptionsPage::gameplay &&
		scene->selected_choice() == ClassicOpOptionsChoice::rank,
		"Gameplay opens as a retained child page");
	expect(scene->advance().has_value(), "Gameplay entry releases");
	expect(scene->advance({.right = true}).has_value() &&
		scene->config().rank == ClassicRank::hard,
		"Gameplay Rank retains its live ring behavior");
	expect(scene->advance().has_value(), "Gameplay Rank releases");
	expect(scene->advance({.cancel = true}).has_value() &&
		scene->page() == ClassicOpOptionsPage::categories &&
		scene->selected_choice() ==
			ClassicOpOptionsChoice::category_gameplay,
		"child Cancel restores the exact parent category");
	expect(scene->advance().has_value(), "category return releases");
	expect(scene->advance({.down = true}).has_value() &&
		scene->advance().has_value() &&
		scene->advance({.ok = true}).has_value() &&
		scene->page() == ClassicOpOptionsPage::audio &&
		scene->selected_choice() == ClassicOpOptionsChoice::master_volume,
		"Audio opens from the category page");
	expect(scene->advance().has_value(), "Audio entry releases");
	expect(scene->advance({.right = true}).has_value() &&
		scene->master_volume_percent() == 40U,
		"Audio Master Volume advances in 5% steps");
	expect(scene->advance().has_value() &&
		scene->advance({.down = true}).has_value() &&
		scene->advance().has_value() &&
		scene->selected_choice() == ClassicOpOptionsChoice::bgm,
		"Audio navigation reaches source Music below Master Volume");
	expect(scene->advance({.right = true}).has_value() &&
		scene->config().bgm_mode == ClassicBgmMode::off &&
		scene->music_volume_percent() == 0U,
		"Audio Music wraps through 0% in five-percent steps");
	const auto music_stop = scene->take_audio_command();
	const auto music_disable = scene->take_audio_command();
	const auto music_level = scene->take_audio_command();
	expect(music_stop && music_stop->type == ClassicAudioCommandType::stop_song &&
		music_disable && music_disable->type ==
			ClassicAudioCommandType::set_bgm_active &&
		music_level && music_level->type ==
			ClassicAudioCommandType::set_music_volume &&
		music_level->parameter == 0U &&
		!scene->take_audio_command(),
		"Audio Music 0% preserves source Stop/disable and installs its gain");
	expect(scene->advance().has_value() &&
		scene->advance({.right = true}).has_value() &&
		scene->music_volume_percent() == 5U &&
		scene->config().bgm_mode == ClassicBgmMode::fm,
		"Audio Music raises 0% to 5% and re-enables FM");
	const auto music_restore_level = scene->take_audio_command();
	const auto music_enable = scene->take_audio_command();
	const auto music_restart = scene->take_audio_command();
	expect(music_restore_level && music_restore_level->type ==
			ClassicAudioCommandType::set_music_volume &&
			music_restore_level->parameter == 5U && music_enable &&
			music_enable->type == ClassicAudioCommandType::set_bgm_active &&
			music_enable->parameter == 1U && music_restart &&
			music_restart->type == ClassicAudioCommandType::play_song &&
			music_restart->cue == ClassicMusicCue::menu_main &&
			!scene->take_audio_command(),
		"Audio Music re-enable installs gain before restarting the menu song");
	expect(scene->advance().has_value() &&
		scene->advance({.down = true}).has_value() &&
		scene->advance().has_value() &&
		scene->selected_choice() == ClassicOpOptionsChoice::sfx,
		"Audio navigation reaches independent SFX percentage");
	expect(scene->advance({.right = true}).has_value() &&
		scene->effect_volume_percent() == 0U && !scene->config().sfx_enabled,
		"Audio SFX wraps through 0% in five-percent steps");
	const auto effect_disable = scene->take_audio_command();
	const auto effect_level = scene->take_audio_command();
	expect(effect_disable && effect_disable->type ==
			ClassicAudioCommandType::set_effects_active &&
		effect_level && effect_level->type ==
			ClassicAudioCommandType::set_effect_volume &&
		effect_level->parameter == 0U && !scene->take_audio_command(),
		"Audio SFX 0% disables arbitration and installs its gain");
	expect(scene->advance().has_value() &&
		scene->advance({.right = true}).has_value() &&
		scene->effect_volume_percent() == 5U && scene->config().sfx_enabled,
		"Audio SFX raises 0% to 5% and re-enables effects");
	const auto effect_restore_level = scene->take_audio_command();
	const auto effect_enable = scene->take_audio_command();
	expect(effect_restore_level && effect_restore_level->type ==
			ClassicAudioCommandType::set_effect_volume &&
			effect_restore_level->parameter == 5U && effect_enable &&
			effect_enable->type == ClassicAudioCommandType::set_effects_active &&
			effect_enable->parameter == 1U && !scene->take_audio_command(),
		"Audio SFX re-enable installs gain before restoring arbitration");
	expect(scene->advance().has_value() &&
		scene->advance({.down = true}).has_value() &&
		scene->advance().has_value() &&
		scene->selected_choice() ==
			ClassicOpOptionsChoice::mute_when_unfocused,
		"Audio navigation reaches New Classic focus mute");
	expect(scene->advance({.right = true}).has_value() &&
		scene->mute_when_unfocused(),
		"focus mute toggles live without emitting a source audio command");
	expect(!scene->take_audio_command(),
		"focus mute remains a final-output policy outside PMD");
	expect(scene->advance().has_value(), "Audio focus mute releases");
	expect(scene->advance({.cancel = true}).has_value() &&
		scene->advance().has_value() &&
		scene->advance({.down = true}).has_value() &&
		scene->advance().has_value() &&
		scene->advance({.ok = true}).has_value() &&
		scene->page() == ClassicOpOptionsPage::input &&
		scene->selected_choice() == ClassicOpOptionsChoice::key_mode,
		"Input opens with Key Mode separate from KeyConfig");
	expect(scene->advance().has_value(), "Input entry releases");
	expect(scene->advance({.down = true}).has_value() &&
		scene->advance().has_value() &&
		scene->selected_choice() == ClassicOpOptionsChoice::key_config,
		"Input exposes a distinct KeyConfig action");
	expect(scene->advance({.ok = true}).has_value(),
		"KeyConfig action completes the Options owner");
	auto key_config = scene->take_completion();
	expect(key_config && key_config->outcome() ==
		ClassicOpOptionsOutcome::key_config &&
		key_config->master_volume_percent() == 40U &&
		key_config->music_volume_percent() == 5U &&
		key_config->effect_volume_percent() == 5U &&
		key_config->mute_when_unfocused(),
		"KeyConfig retains its typed handoff outcome");

	auto video = make_patch_scene_if_assets_supplied();
	if(!video) {
		return;
	}
	enter_patch_options(*video);
	for(std::uint8_t row = 0U; row < 3U; ++row) {
		expect(video->advance({.down = true}).has_value() &&
			video->advance().has_value(),
			"Video category navigation advances and releases");
	}
	expect(video->advance({.ok = true}).has_value() &&
		video->page() == ClassicOpOptionsPage::video &&
		video->selected_choice() == ClassicOpOptionsChoice::window_mode,
		"Video opens on Window Mode");
	expect(video->advance().has_value() &&
		video->advance({.right = true}).has_value() &&
		video->window_presentation().mode ==
			NewClassicWindowMode::borderless_fullscreen,
		"Window Mode applies its two-value live ring");
	expect(video->advance().has_value() &&
		video->advance({.down = true}).has_value() &&
		video->advance().has_value() &&
		video->selected_choice() == ClassicOpOptionsChoice::window_scale &&
		video->advance({.right = true}).has_value() &&
		video->window_presentation().scale == NewClassicWindowScale::x2,
		"Window Scale advances through exact integer presets");
	expect(video->advance().has_value() &&
		video->advance({.down = true}).has_value() &&
		video->advance().has_value() &&
		video->selected_choice() == ClassicOpOptionsChoice::aspect_ratio &&
		video->advance({.right = true}).has_value() &&
		video->window_presentation().aspect ==
			th03::app::NewClassicAspectRatio::display_4_3,
		"Aspect Ratio switches the live host policy to 4:3 Display");
	expect(video->advance().has_value() &&
		video->advance({.down = true}).has_value() &&
		video->advance().has_value() &&
		video->selected_choice() == ClassicOpOptionsChoice::filtering &&
		video->advance({.right = true}).has_value() &&
		video->window_presentation().filtering ==
			th03::app::NewClassicFiltering::linear,
		"Filtering switches the live host policy to Linear");

	auto system = make_patch_scene_if_assets_supplied();
	if(!system) {
		return;
	}
	enter_patch_options(*system);
	for(std::uint8_t row = 0U; row < 5U; ++row) {
		expect(system->advance({.down = true}).has_value(),
			"System category navigation advances");
		expect(system->advance().has_value(),
			"System category navigation releases");
	}
	expect(system->advance({.ok = true}).has_value() &&
		system->page() == ClassicOpOptionsPage::system &&
		system->selected_choice() == ClassicOpOptionsChoice::switch_classic,
		"System exposes Switch to Classic as its working action");
	expect(system->advance().has_value(), "System entry releases");
	expect(system->advance({.ok = true}).has_value() &&
		system->page() == ClassicOpOptionsPage::switch_confirmation &&
		system->selected_choice() ==
			ClassicOpOptionsChoice::confirm_switch_classic,
		"Switch to Classic opens an explicit confirmation page");
	expect(system->advance().has_value(),
		"Switch-to-Classic confirmation releases");
	expect(system->advance({.ok = true}).has_value() &&
		system->phase() == ClassicOpOptionsPhase::shrink_window,
		"confirmed Switch to Classic owns the normal source-style contraction");
	for(std::uint16_t frame = 0U; frame < 18U; ++frame) {
		expect(system->advance().has_value(),
			"Switch to Classic contraction advances");
	}
	expect(system->advance().has_value(),
		"Switch to Classic retained title boundary publishes");
	auto switch_classic = system->take_completion();
	expect(switch_classic && switch_classic->outcome() ==
		ClassicOpOptionsOutcome::switch_classic,
		"System publishes a typed Switch-to-Classic outcome");
}

void expect_gaiji(
	const th03::present::Tram &tram,
	std::int32_t x,
	std::int32_t y,
	std::uint8_t glyph,
	th03::present::TramAttribute attribute,
	std::string_view message
)
{
	const auto cell = tram.cell({x, y});
	const auto decoded = cell ?
		th03::present::decode_gaiji_cell(cell->code) : std::nullopt;
	expect(
		decoded && decoded->glyph_id == glyph &&
			decoded->half == th03::present::GaijiHalf::left &&
			cell->attribute == attribute,
		message
	);
}

void grow_and_enter(
	ClassicOpOptionsScene &scene,
	ClassicOpInput transition_input
)
{
	const auto initial_rand = scene.resident().resident_rand;
	for(std::uint16_t frame = 0U; frame < 13U; ++frame) {
		const auto tick = scene.advance(
			frame == 0U ? transition_input : ClassicOpInput{.cancel = true}
		);
		expect(tick.has_value(), "Options grow frame advances");
		if(tick) {
			expect(
				tick->input_sampled == (frame == 0U),
				"Options grow samples input only before the blocking animation"
			);
		}
		expect(
			scene.resident().resident_rand == initial_rand,
			"Options grow VSyncs do not advance resident RNG"
		);
	}
	expect(
		scene.phase() == ClassicOpOptionsPhase::enter_options,
		"13 source grow VSyncs reach initial option drawing"
	);
	const auto entered = scene.advance({.right = true});
	expect(
		entered && entered->phase == ClassicOpOptionsPhase::options_menu &&
			!entered->input_sampled && entered->tram_changed,
		"initial Options draw consumes the latched pre-animation sample"
	);
	expect(
		scene.resident().resident_rand == initial_rand + 1,
		"initial Options loop tail advances resident RNG once"
	);
}

void test_exact_rows_and_transition_lock()
{
	auto scene = make_scene();
	if(!scene) {
		return;
	}
	const auto first = scene->advance({.ok = true});
	expect(
		first && first->input_sampled && first->tram_changed,
		"menu-change iteration samples and clears before box growth"
	);
	expect(
		scene->raster().pixel(Page::zero, {288, 256}) == 2U,
		"first grow frame moves the source right edge by eight pixels"
	);
	for(std::uint16_t frame = 1U; frame < 13U; ++frame) {
		expect(
			scene->advance({.cancel = true}).has_value(),
			"remaining grow frame advances"
		);
	}
	expect(
		scene->raster().pixel(Page::zero, {384, 256}) == 2U,
		"final expanded right edge starts at x=384"
	);
	expect(scene->advance().has_value(), "initial option rows draw");
	expect(
		!scene->input_allowed(),
		"held main-menu confirmation remains locked after the grow"
	);

	expect_gaiji(
		scene->tram(), 25, 17, 0x6EU, th03::present::tram_white,
		"Rank label preserves source x/y and selected attribute"
	);
	expect_gaiji(
		scene->tram(), 37, 17, 0x74U, th03::present::tram_white,
		"Normal value preserves source shifted center"
	);
	expect_gaiji(
		scene->tram(), 25, 19, 0x4BU, th03::present::tram_black,
		"Music label preserves source row"
	);
	expect_gaiji(
		scene->tram(), 35, 19, 0xCFU, th03::present::tram_black,
		"FM value preserves the fixed left padding"
	);
	expect_gaiji(
		scene->tram(), 23, 21, 0x5CU, th03::present::tram_black,
		"KeyConfig keeps the source off-center left column"
	);
	expect_gaiji(
		scene->tram(), 32, 22, 0x48U, th03::present::tram_black,
		"Quit is centered in the full submenu"
	);

	const auto held = scene->advance({.right = true});
	expect(
		held && !held->tram_changed,
		"held transition input advances without redrawing"
	);
	expect(
		scene->config().rank == ClassicRank::normal,
		"held transition input cannot alter Rank"
	);
	expect(scene->advance().has_value(), "empty sample unlocks Options");
	expect(scene->input_allowed(), "empty sample enables input");
}

void test_config_audio_and_return_cadence()
{
	auto scene = make_scene();
	if(!scene) {
		return;
	}
	grow_and_enter(*scene, {.ok = true});
	expect(scene->advance({.right = true}).has_value(), "held Right is ignored");
	expect(scene->advance().has_value(), "Rank unlock sample advances");
	const auto rank = scene->advance({.right = true});
	expect(
		rank && rank->config_changed &&
			scene->config().rank == ClassicRank::hard &&
			scene->resident().rank == static_cast<std::uint8_t>(ClassicRank::hard),
		"Rank changes resident and persisted option together"
	);
	std::vector<th03::present::ComposedPixel> composed_pixels(
		th03::present::graphics_page_pixels
	);
	const auto composed = th03::present::compose_source_surface(
		std::span<
			th03::present::ComposedPixel,
			th03::present::graphics_page_pixels
		>(composed_pixels.data(), composed_pixels.size()),
		scene->raster(), scene->tram(), EmptyAnk{}
	);
	expect(
		composed.has_value(),
		"Normal-to-Hard source gaiji overlap composes without malformed JIS"
	);
	expect(scene->advance().has_value(), "post-Rank release advances");
	const auto opposed = scene->advance({.up = true, .down = true});
	expect(
		opposed && opposed->selection_changed &&
			scene->selected_choice() == ClassicOpOptionsChoice::rank,
		"simultaneous Up and Down execute sequentially and cancel"
	);
	expect(scene->advance().has_value(), "post-navigation release advances");
	const auto leave = scene->advance({
		.down = true,
		.left = true,
		.cancel = true,
	});
	expect(
		leave && leave->phase == ClassicOpOptionsPhase::shrink_window &&
			leave->selection_changed && leave->config_changed &&
			scene->config().bgm_mode == ClassicBgmMode::off,
		"vertical, horizontal, and Cancel ordering matches option_update"
	);
	const auto stop = scene->take_audio_command();
	const auto disable = scene->take_audio_command();
	expect(
		stop && stop->type == ClassicAudioCommandType::stop_song &&
			stop->cue == ClassicMusicCue::none && disable &&
			disable->type == ClassicAudioCommandType::set_bgm_active &&
			disable->parameter == 0U,
		"FM-to-Off emits Stop before disabling the source driver"
	);
	expect(!scene->take_audio_command(), "FM-to-Off audio queue drains");

	const auto rand_before_shrink = scene->resident().resident_rand;
	for(std::uint16_t frame = 0U; frame < 14U; ++frame) {
		const auto tick = scene->advance(
			frame == 0U ? ClassicOpInput{.cancel = true} : ClassicOpInput{}
		);
		expect(tick.has_value(), "Options shrink frame advances");
		if(tick) {
			expect(
				tick->input_sampled == (frame == 0U),
				"Options shrink samples only before the blocking animation"
			);
		}
		expect(
			scene->resident().resident_rand == rand_before_shrink,
			"Options shrink VSyncs do not advance resident RNG"
		);
	}
	expect(
		scene->phase() == ClassicOpOptionsPhase::return_main,
		"14 source shrink VSyncs reach main-menu redraw"
	);
	expect(
		scene->raster().pixel(Page::zero, {280, 256}) == 2U &&
			scene->raster().pixel(Page::zero, {384, 256}) == 4U,
		"shrink restores TL02 and returns the right edge to x=280"
	);
	const auto returned = scene->advance({});
	expect(
		returned && returned->ready &&
			returned->phase == ClassicOpOptionsPhase::ready,
		"main-menu redraw and common loop tail publish completion"
	);
	auto completion = scene->take_completion();
	expect(completion.has_value(), "Options completion is available once");
	if(completion) {
		expect(
			completion->config().rank == ClassicRank::hard &&
				completion->config().bgm_mode == ClassicBgmMode::off,
			"Options completion retains all in-memory configuration edits"
		);
		expect(
			!completion->main_input_allowed(),
			"held pre-shrink Cancel keeps returned main input locked"
		);
		expect_gaiji(
			completion->tram(), 25, 21, 0x3DU,
			th03::present::tram_white,
			"returned main menu redraws Option as selected"
		);
		expect(
			completion->resident().resident_rand == rand_before_shrink + 1,
			"returned main loop tail increments resident RNG once"
		);
	}
	expect(
		!scene->take_completion() &&
			scene->phase() == ClassicOpOptionsPhase::consumed,
		"Options completion is single-consumer"
	);
}

void select_bgm(ClassicOpOptionsScene &scene)
{
	grow_and_enter(scene, {});
	expect(scene.advance({.down = true}).has_value(), "BGM row selects");
	expect(scene.advance().has_value(), "BGM row release advances");
}

void test_bgm_modes_and_disabled_selection()
{
	ClassicConfigOptions off{};
	off.bgm_mode = ClassicBgmMode::off;
	auto from_off = make_scene(off);
	if(from_off) {
		select_bgm(*from_off);
		const auto changed = from_off->advance({.right = true});
		expect(
			changed && changed->config_changed &&
				from_off->config().bgm_mode == ClassicBgmMode::fm,
			"Off toggles to FM in either horizontal direction"
		);
		const auto stop = from_off->take_audio_command();
		const auto enable = from_off->take_audio_command();
		const auto play = from_off->take_audio_command();
		expect(
			stop && stop->type == ClassicAudioCommandType::stop_song &&
				enable && enable->type ==
					ClassicAudioCommandType::set_bgm_active &&
				enable->parameter == 1U &&
				play && play->type == ClassicAudioCommandType::play_song &&
				play->cue == ClassicMusicCue::menu_main,
			"Off-to-FM emits Stop, enables the driver, then plays"
		);
	}

	ClassicConfigOptions midi{};
	midi.bgm_mode = ClassicBgmMode::midi;
	auto from_midi = make_scene(midi);
	if(from_midi) {
		select_bgm(*from_midi);
		expect(
			from_midi->advance({.left = true}).has_value() &&
				from_midi->config().bgm_mode == ClassicBgmMode::off,
			"MIDI collapses to Off instead of ringing to FM"
		);
		const auto midi_stop = from_midi->take_audio_command();
		const auto midi_disable = from_midi->take_audio_command();
		expect(
			midi_stop && midi_stop->type == ClassicAudioCommandType::stop_song &&
				midi_disable && midi_disable->type ==
					ClassicAudioCommandType::set_bgm_active &&
				midi_disable->parameter == 0U &&
				!from_midi->take_audio_command(),
			"MIDI-to-Off emits Stop before disabling the source driver"
		);
	}

	auto disabled = make_scene(off, false);
	if(disabled) {
		select_bgm(*disabled);
		const auto redraw = disabled->advance({.left = true});
		expect(
			redraw && redraw->tram_changed && !redraw->config_changed &&
				disabled->config().bgm_mode == ClassicBgmMode::off &&
				!disabled->take_audio_command(),
			"disabled BGM input redraws but cannot mutate or emit audio"
		);
	}
}

void test_key_ring_and_rejections()
{
	auto scene = make_scene();
	if(scene) {
		grow_and_enter(*scene, {});
		expect(scene->advance({.down = true}).has_value(), "BGM selects");
		expect(scene->advance().has_value(), "first release advances");
		expect(scene->advance({.down = true}).has_value(), "Key Mode selects");
		expect(scene->advance().has_value(), "second release advances");
		const auto wrapped = scene->advance({.left = true, .right = true});
		expect(
			wrapped && wrapped->config_changed &&
				scene->config().key_mode == ClassicKeyMode::key_joy &&
				scene->resident().key_mode == 2U,
			"Left takes precedence and wraps Key-vs-Key to Key-vs-Joy"
		);
	}

	ClassicConfigOptions fm{};
	GameInit resident{};
	resident.rank = static_cast<std::uint8_t>(fm.rank);
	resident.key_mode = static_cast<std::uint8_t>(fm.key_mode);
	auto unavailable = ClassicOpOptionsScene::create(
		resident, fm, false, main_menu_raster(), {}, menu_background(),
		window_sheet()
	);
	expect(
		!unavailable && unavailable.error().code ==
			ClassicOpErrorCode::invalid_state,
		"unavailable FM rejects the source-unreachable enabled BGM state"
	);

	resident.rank = 0U;
	auto mismatch = ClassicOpOptionsScene::create(
		resident, fm, true, main_menu_raster(), {}, menu_background(),
		window_sheet()
	);
	expect(
		!mismatch && mismatch.error().code == ClassicOpErrorCode::invalid_state,
		"resident and persisted Rank mismatch is rejected"
	);

	auto bad_window = window_sheet();
	bad_window.header.pixel_width = 8U;
	resident.rank = static_cast<std::uint8_t>(fm.rank);
	auto geometry = ClassicOpOptionsScene::create(
		resident, fm, true, main_menu_raster(), {}, menu_background(),
		std::move(bad_window)
	);
	expect(
		!geometry && geometry.error().code ==
			ClassicOpErrorCode::invalid_asset_geometry,
		"invalid retained OPWIN geometry is rejected"
	);
}

const th03::app::LoadedMainResource *find_resource(
	std::span<const th03::app::LoadedMainResource> resources,
	th03::app::MainResourceRole role
)
{
	for(const auto &resource : resources) {
		if(resource.spec.key.role == role) {
			return &resource;
		}
	}
	return nullptr;
}

void local_original_gate(const std::filesystem::path &content_root)
{
	th03::app::MainResourceLoader loader(content_root);
	const auto closure = loader.load_op_title_menu();
	expect(
		closure.has_value(),
		"local title closure authenticates retained Options resources"
	);
	if(!closure) {
		return;
	}
	const auto *background_resource = find_resource(
		*closure, th03::app::MainResourceRole::op_menu_background
	);
	const auto *window_resource = find_resource(
		*closure, th03::app::MainResourceRole::op_window_sheet
	);
	const auto *gaiji_resource = find_resource(
		*closure, th03::app::MainResourceRole::op_gaiji_font
	);
	expect(
		background_resource && background_resource->bytes &&
			window_resource && window_resource->bytes &&
			gaiji_resource && gaiji_resource->bytes,
		"local retained TL02, OPWIN, and MIKOFT payloads are present"
	);
	if(!background_resource || !background_resource->bytes ||
		!window_resource || !window_resource->bytes ||
		!gaiji_resource || !gaiji_resource->bytes) {
		return;
	}
	auto background = th03::assets::decode_pi(*background_resource->bytes);
	auto window = th03::assets::decode_bfnt(
		*window_resource->bytes, BfntConsumerPolicy::superimpose
	);
	auto gaiji = th03::assets::decode_bfnt(
		*gaiji_resource->bytes, BfntConsumerPolicy::gaiji
	);
	expect(
		background && window && gaiji,
		"local retained Options assets decode at source geometry"
	);
	if(!background || !window || !gaiji || !background->palette) {
		return;
	}

	th03::present::Raster raster{};
	for(std::uint8_t index = 0U; index < background->palette->colors.size();
		++index) {
		expect(
			raster.set_pending_palette_entry(
				index, background->palette->colors[index]
			),
			"local TL02 palette entry prepares"
		);
	}
	raster.apply_pending_palette();
	expect(
		th03::present::pi_put_8(raster, 0, 0, *background),
		"local TL02 draws before retained main-window reconstruction"
	);
	auto prepared_window = th03::present::prepare_superimpose_sheet(*window);
	expect(
		prepared_window.has_value(),
		"local OPWIN prepares for retained source window reconstruction"
	);
	if(!prepared_window) {
		return;
	}
	expect(
		th03::present::superimpose_put(
			raster, *prepared_window, {160, 256}, 0U
		),
		"local OPWIN left edge reconstructs"
	);
	for(std::int32_t left = 176; left <= 280; left += 8) {
		const auto offset =
			(static_cast<std::size_t>(256) * background->image.width) +
			static_cast<std::size_t>(left);
		expect(
			raster.blit({
				.pixels = std::span<const th03::present::Index4>(
					background->image.indices
				).subspan(offset),
				.width = 16U,
				.height = 128U,
				.stride = background->image.width,
			}, {left, 256}) &&
			th03::present::superimpose_put(
				raster, *prepared_window, {left, 256}, 2U
			),
			"local OPWIN main-width source step reconstructs"
		);
	}

	auto prepared_gaiji = th03::app::PreparedGameftGaiji::prepare(*gaiji);
	expect(prepared_gaiji.has_value(), "local MIKOFT gaiji prepare");
	if(!prepared_gaiji) {
		return;
	}
	th03::present::Tram tram{};
	prepared_gaiji->install(tram);
	ClassicConfigOptions config{};
	GameInit resident{};
	resident.rank = static_cast<std::uint8_t>(config.rank);
	resident.key_mode = static_cast<std::uint8_t>(config.key_mode);
	auto created = ClassicOpOptionsScene::create(
		resident, config, true, std::move(raster), std::move(tram),
		std::move(*background), std::move(*window)
	);
	expect(
		created.has_value(),
		"local retained title state adopts into Options"
	);
	if(!created) {
		return;
	}
	grow_and_enter(**created, {.ok = true});
	expect_gaiji(
		(*created)->tram(), 25, 17, 0x6EU, th03::present::tram_white,
		"local MIKOFT renders the selected Rank label"
	);
	expect(
		(*created)->raster().pixel(Page::zero, {384, 256}) !=
			(*created)->raster().pixel(Page::zero, {400, 256}),
		"local OPWIN expanded edge remains distinct from TL02"
	);

	GameInit title_resident{};
	title_resident.rank = static_cast<std::uint8_t>(config.rank);
	title_resident.key_mode = static_cast<std::uint8_t>(config.key_mode);
	title_resident.fast_op_animation = 1U;
	auto title = th03::app::ClassicOpTitleMenuScene::create(
		title_resident, th03::app::ClassicOpTitleMode::fast_fade, *closure
	);
	expect(title.has_value(), "local fast title creates for typed adoption");
	if(!title) {
		return;
	}
	expect((*title)->advance().has_value(), "typed title black frame advances");
	const auto title_stop = (*title)->take_audio_command();
	const auto title_load = (*title)->take_audio_command();
	expect(
		title_stop && title_stop->type == ClassicAudioCommandType::stop_song &&
			title_load && title_load->type == ClassicAudioCommandType::load_song,
		"typed title startup audio drains"
	);
	expect((*title)->advance().has_value(), "typed title initial frame advances");
	expect(
		(*title)->take_audio_command().has_value(),
		"typed title play command drains"
	);
	for(std::uint16_t frame = 0U; frame < 25U; ++frame) {
		expect((*title)->advance().has_value(), "typed title fade advances");
	}
	expect(
		(*title)->advance({.ok = true}).has_value(),
		"typed title accepts the pre-menu input"
	);
	for(std::uint16_t frame = 0U; frame < 14U; ++frame) {
		expect((*title)->advance().has_value(), "typed title box step advances");
	}
	expect((*title)->advance().has_value(), "typed title main menu draws");
	for(std::uint8_t row = 0U; row < 4U; ++row) {
		expect(
			(*title)->advance({.down = true}).has_value(),
			"typed title selection advances toward Option"
		);
		expect(
			(*title)->advance().has_value(),
			"typed title selection release advances"
		);
	}
	expect(
		(*title)->advance({.ok = true}).has_value(),
		"typed title confirms Option"
	);
	auto title_completion = (*title)->take_completion();
	expect(
		title_completion &&
			title_completion->route() == th03::app::ClassicOpRoute::options,
		"typed title completion authorizes only the Options route"
	);
	if(!title_completion) {
		return;
	}
	auto mismatched = config;
	mismatched.rank = ClassicRank::easy;
	auto rejected_adoption = ClassicOpOptionsScene::create_from_op(
		*title_completion, mismatched, true, *closure
	);
	expect(
		!rejected_adoption && rejected_adoption.error().code ==
			ClassicOpErrorCode::invalid_state,
		"fallible typed adoption rejects resident/config mismatch"
	);
	expect(
		title_completion->route() == th03::app::ClassicOpRoute::options,
		"failed typed adoption leaves the title completion retryable"
	);
	auto adopted = ClassicOpOptionsScene::create_from_op(
		*title_completion, config, true, *closure
	);
	expect(
		adopted.has_value() &&
			(*adopted)->phase() == ClassicOpOptionsPhase::grow_window,
		"authenticated title completion adopts into Options retryably"
	);
	if(!adopted) {
		return;
	}
	grow_and_enter(**adopted, {.ok = true});
	expect((*adopted)->advance().has_value(),
		"authenticated Options releases the transition input");
	expect((*adopted)->advance({.cancel = true}).has_value(),
		"authenticated Options begins its return");
	for(std::uint16_t frame = 0U; frame < 14U; ++frame) {
		expect((*adopted)->advance(
			frame == 0U ? ClassicOpInput{.cancel = true} : ClassicOpInput{}
		).has_value(), "authenticated Options shrink advances");
	}
	expect((*adopted)->advance().has_value(),
		"authenticated Options redraws the retained main menu");
	auto options_completion = (*adopted)->take_completion();
	expect(options_completion.has_value(),
		"authenticated Options publishes its typed return");
	if(!options_completion) {
		return;
	}
	auto resumed = th03::app::ClassicOpTitleMenuScene::resume_from_options(
		*options_completion, *closure, {}
	);
	expect(
		resumed && (*resumed)->phase() == th03::app::ClassicOpPhase::main_menu &&
			(*resumed)->selected_choice() ==
				th03::app::ClassicOpMenuChoice::options &&
			!(*resumed)->take_audio_command(),
		"Options return resumes the retained title without fade or audio"
	);
	if(resumed) {
		const auto held = (*resumed)->advance({.ok = true});
		expect(
			held && (*resumed)->phase() == th03::app::ClassicOpPhase::main_menu &&
				!held->surface_changed,
			"Options return preserves the held-input lock without a redundant redraw"
		);
		expect((*resumed)->advance().has_value(),
			"Options return unlocks only after exact input release");
		const auto repressed = (*resumed)->advance({.ok = true});
		auto completion = (*resumed)->take_completion();
		expect(repressed && repressed->ready && completion &&
			completion->route() == th03::app::ClassicOpRoute::options,
			"Options return accepts a fresh confirm after release");
	}
}

void local_replay_patch_return_gate(const std::filesystem::path &content_root)
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
	const auto *path = std::getenv("TH03_AZINN_DAT");
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
	if(!path || *path == '\0') {
		return;
	}
	auto loaded = th03::app::load_new_classic_replay_patch_assets(path);
	expect(loaded.has_value(),
		"authenticated Replay Patch assets load for the Options return");
	if(!loaded) {
		return;
	}
	auto patch_assets =
		std::make_shared<const th03::app::NewClassicReplayPatchAssets>(
			std::move(*loaded)
		);
	th03::app::MainResourceLoader loader(content_root);
	const auto closure = loader.load_op_title_menu();
	expect(closure.has_value(),
		"authenticated title closure loads for the Replay Patch return");
	if(!closure) {
		return;
	}

	ClassicConfigOptions config{};
	GameInit resident{};
	resident.rank = static_cast<std::uint8_t>(config.rank);
	resident.key_mode = static_cast<std::uint8_t>(config.key_mode);
	resident.fast_op_animation = 1U;
	auto title = th03::app::ClassicOpTitleMenuScene::create(
		resident, th03::app::ClassicOpTitleMode::fast_fade, *closure,
		patch_assets
	);
	expect(title.has_value(),
		"authenticated Replay Patch title creates for the Options return");
	if(!title) {
		return;
	}
	expect((*title)->advance().has_value(),
		"Replay Patch title black frame advances");
	expect((*title)->take_audio_command().has_value() &&
		(*title)->take_audio_command().has_value(),
		"Replay Patch title startup audio drains");
	expect((*title)->advance().has_value(),
		"Replay Patch title initial frame advances");
	expect((*title)->take_audio_command().has_value(),
		"Replay Patch title play command drains");
	for(std::uint16_t frame = 0U; frame < 25U; ++frame) {
		expect((*title)->advance().has_value(),
			"Replay Patch title fade advances");
	}
	expect((*title)->advance({.ok = true}).has_value(),
		"Replay Patch title accepts the pre-menu input");
	for(std::uint16_t frame = 0U; frame < 14U; ++frame) {
		expect((*title)->advance().has_value(),
			"Replay Patch title box step advances");
	}
	expect((*title)->advance().has_value(),
		"Replay Patch title main menu draws");
	for(std::uint8_t row = 0U; row < 5U; ++row) {
		expect((*title)->advance({.down = true}).has_value(),
			"Replay Patch title selection advances toward Option");
		expect((*title)->advance().has_value(),
			"Replay Patch title selection releases toward Option");
	}
	expect((*title)->advance({.ok = true}).has_value(),
		"Replay Patch title confirms Option");
	auto title_completion = (*title)->take_completion();
	expect(title_completion && title_completion->route() ==
		th03::app::ClassicOpRoute::options,
		"Replay Patch title publishes the Option route");
	if(!title_completion) {
		return;
	}

	EmptyAnk font{};
	auto options = ClassicOpOptionsScene::create_from_op(
		*title_completion, config, true, *closure, patch_assets, &font, true
	);
	expect(options.has_value(),
		"authenticated Replay Patch Options adopts the title");
	if(!options) {
		return;
	}
	for(std::uint16_t frame = 0U; frame < 17U; ++frame) {
		expect((*options)->advance(
			frame == 0U ? ClassicOpInput{.ok = true} : ClassicOpInput{}
		).has_value(), "Replay Patch Options grow advances");
	}
	expect((*options)->advance().has_value(),
		"Replay Patch Options labels draw");
	expect((*options)->advance().has_value(),
		"Replay Patch Options transition input releases");
	for(std::uint8_t row = 0U; row < 2U; ++row) {
		expect((*options)->advance({.down = true}).has_value(),
			"Replay Patch Options advances toward Input");
		expect((*options)->advance().has_value(),
			"Replay Patch Options category movement releases");
	}
	expect((*options)->advance({.ok = true}).has_value() &&
		(*options)->page() == ClassicOpOptionsPage::input,
		"Replay Patch Options enters Input");
	expect((*options)->advance().has_value(),
		"Replay Patch Input entry releases");
	expect((*options)->advance({.down = true}).has_value() &&
		(*options)->selected_choice() == ClassicOpOptionsChoice::key_config,
		"Replay Patch Input selects KeyConfig");
	expect((*options)->advance().has_value(),
		"Replay Patch KeyConfig selection releases");
	expect((*options)->advance({.ok = true}).has_value(),
		"Replay Patch Input hands off to KeyConfig");
	auto key_config_completion = (*options)->take_completion();
	expect(key_config_completion && key_config_completion->outcome() ==
		ClassicOpOptionsOutcome::key_config,
		"Replay Patch KeyConfig handoff is typed");
	if(!key_config_completion) {
		return;
	}
	auto resumed_options = ClassicOpOptionsScene::resume_after_key_config(
		*key_config_completion, true, *closure, patch_assets, &font, true
	);
	expect(resumed_options &&
		(*resumed_options)->page() == ClassicOpOptionsPage::input &&
		(*resumed_options)->selected_choice() ==
			ClassicOpOptionsChoice::key_config &&
		!(*resumed_options)->input_allowed(),
		"KeyConfig return restores the Input page, cursor, and release lock");
	if(!resumed_options) {
		return;
	}
	options = std::move(resumed_options);
	const auto held_key_config = (*options)->advance({.ok = true});
	expect(held_key_config && !held_key_config->surface_changed &&
		(*options)->page() == ClassicOpOptionsPage::input,
		"KeyConfig return ignores the held confirm boundary");
	expect((*options)->advance().has_value(),
		"KeyConfig return unlocks after release");
	expect((*options)->advance({.down = true}).has_value() &&
		(*options)->selected_choice() == ClassicOpOptionsChoice::joy_config,
		"Replay Patch Input exposes JoyConfig below KeyConfig");
	expect((*options)->advance().has_value(),
		"Replay Patch JoyConfig selection releases");
	expect((*options)->advance({.ok = true}).has_value(),
		"Replay Patch Input hands off to JoyConfig");
	auto joy_config_completion = (*options)->take_completion();
	expect(joy_config_completion && joy_config_completion->outcome() ==
		ClassicOpOptionsOutcome::joy_config,
		"Replay Patch JoyConfig handoff is typed");
	if(!joy_config_completion) {
		return;
	}
	auto resumed_joy = ClassicOpOptionsScene::resume_after_joy_config(
		*joy_config_completion, true, *closure, patch_assets, &font, true
	);
	expect(resumed_joy &&
		(*resumed_joy)->page() == ClassicOpOptionsPage::input &&
		(*resumed_joy)->selected_choice() ==
			ClassicOpOptionsChoice::joy_config &&
		!(*resumed_joy)->input_allowed(),
		"JoyConfig return restores the Input page, cursor, and release lock");
	if(!resumed_joy) {
		return;
	}
	options = std::move(resumed_joy);
	expect((*options)->advance().has_value(),
		"JoyConfig return unlocks after release");
	expect((*options)->advance({.down = true}).has_value() &&
		(*options)->selected_choice() ==
			ClassicOpOptionsChoice::device_assignment,
		"Replay Patch Input exposes Device Assignment below JoyConfig");
	expect((*options)->advance().has_value(),
		"Device Assignment selection releases");
	expect((*options)->advance({.ok = true}).has_value(),
		"Replay Patch Input hands off to Device Assignment");
	auto device_completion = (*options)->take_completion();
	expect(device_completion && device_completion->outcome() ==
		ClassicOpOptionsOutcome::device_assignment,
		"Replay Patch Device Assignment handoff is typed");
	if(!device_completion) {
		return;
	}
	auto resumed_device = ClassicOpOptionsScene::resume_after_device_scene(
		*device_completion, true, *closure, patch_assets, &font, true
	);
	expect(resumed_device &&
		(*resumed_device)->page() == ClassicOpOptionsPage::input &&
		(*resumed_device)->selected_choice() ==
			ClassicOpOptionsChoice::device_assignment &&
		!(*resumed_device)->input_allowed(),
		"Device Assignment return restores its Input cursor and release lock");
	if(!resumed_device) {
		return;
	}
	options = std::move(resumed_device);
	expect((*options)->advance().has_value(),
		"Device Assignment return unlocks after release");
	expect((*options)->advance({.down = true}).has_value() &&
		(*options)->selected_choice() == ClassicOpOptionsChoice::input_test,
		"Replay Patch Input exposes Input Test below Device Assignment");
	expect((*options)->advance().has_value(),
		"Input Test selection releases");
	expect((*options)->advance({.ok = true}).has_value(),
		"Replay Patch Input hands off to Input Test");
	auto input_test_completion = (*options)->take_completion();
	expect(input_test_completion && input_test_completion->outcome() ==
		ClassicOpOptionsOutcome::input_test,
		"Replay Patch Input Test handoff is typed");
	if(!input_test_completion) {
		return;
	}
	auto resumed_test = ClassicOpOptionsScene::resume_after_device_scene(
		*input_test_completion, true, *closure, patch_assets, &font, true
	);
	expect(resumed_test &&
		(*resumed_test)->page() == ClassicOpOptionsPage::input &&
		(*resumed_test)->selected_choice() ==
			ClassicOpOptionsChoice::input_test,
		"Input Test return restores its exact Input cursor");
	if(!resumed_test) {
		return;
	}
	options = std::move(resumed_test);
	expect((*options)->advance().has_value(),
		"Input Test return unlocks after release");
	expect((*options)->advance({.cancel = true}).has_value() &&
		(*options)->page() == ClassicOpOptionsPage::categories &&
		(*options)->selected_choice() ==
			ClassicOpOptionsChoice::category_input,
		"KeyConfig return Cancel restores its exact Input parent");
	expect((*options)->advance().has_value(),
		"Replay Patch Input parent return releases");
	expect((*options)->advance({.cancel = true}).has_value(),
		"Replay Patch Options cancel starts its return");
	for(std::uint16_t frame = 0U; frame < 18U; ++frame) {
		expect((*options)->advance(
			frame == 0U ? ClassicOpInput{.cancel = true} : ClassicOpInput{}
		).has_value(), "Replay Patch Options shrink advances");
	}
	expect((*options)->advance().has_value(),
		"Replay Patch Options redraws the main menu");
	auto options_completion = (*options)->take_completion();
	expect(options_completion.has_value(),
		"Replay Patch Options publishes its typed return");
	if(!options_completion) {
		return;
	}

	auto resumed = th03::app::ClassicOpTitleMenuScene::resume_from_options(
		*options_completion, *closure, patch_assets
	);
	expect(resumed.has_value(),
		"Replay Patch Options return rebuilds the retained title owner");
	if(!resumed) {
		return;
	}
	const auto held = (*resumed)->advance({.ok = true});
	expect(held && (*resumed)->phase() ==
		th03::app::ClassicOpPhase::main_menu && !held->surface_changed,
		"Replay Patch Options return preserves the held-input lock");
	expect((*resumed)->advance().has_value(),
		"Replay Patch Options return unlocks on release");
	const auto up = (*resumed)->advance({.up = true});
	expect(up && up->selection_changed &&
		(*resumed)->selected_choice() ==
			th03::app::ClassicOpMenuChoice::replay,
		"New Classic Options return accepts Up to Replay without losing its base snapshot");
	expect((*resumed)->advance().has_value(),
		"New Classic Replay selection releases");
	for(std::uint8_t row = 0U; row < 3U; ++row) {
		expect((*resumed)->advance({.up = true}).has_value(),
			"Replay Patch return selection advances toward VS Start");
		expect((*resumed)->advance().has_value(),
			"Replay Patch return selection releases toward VS Start");
	}
	expect((*resumed)->selected_choice() ==
		th03::app::ClassicOpMenuChoice::versus,
		"Replay Patch return reaches VS Start");
	expect((*resumed)->advance({.ok = true}).has_value() &&
		(*resumed)->phase() == th03::app::ClassicOpPhase::vs_grow_window,
		"Replay Patch return opens VS Start without losing its base snapshot");
	for(std::uint16_t frame = 0U; frame < 13U; ++frame) {
		expect((*resumed)->advance().has_value(),
			"Replay Patch VS Start grow advances");
	}
	expect((*resumed)->phase() == th03::app::ClassicOpPhase::vs_menu,
		"Replay Patch VS Start reaches its menu");
	expect((*resumed)->advance().has_value(),
		"Replay Patch VS Start transition input releases");
	const auto vs_down = (*resumed)->advance({.down = true});
	expect(vs_down && vs_down->selection_changed &&
		(*resumed)->selected_vs_choice() ==
			th03::app::ClassicOpVsChoice::one_player_two_player,
		"Replay Patch VS Start accepts its first selection change");
}

} // namespace

int main(int argc, char **argv)
{
	test_replay_patch_option_unput_if_assets_supplied();
	test_replay_patch_option_hierarchy_if_assets_supplied();
	test_localized_option_snapshot_if_assets_supplied();
	test_exact_rows_and_transition_lock();
	test_config_audio_and_return_cadence();
	test_bgm_modes_and_disabled_selection();
	test_key_ring_and_rejections();
	if(argc == 2) {
		local_original_gate(argv[1]);
		local_replay_patch_return_gate(argv[1]);
	} else {
		expect(argc == 1, "optional local gate accepts one content root");
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
