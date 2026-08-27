#include "../src/app/classic/op/classic_op_options_scene_internal.hpp"

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

#include "th03/app/content/authentic_text.hpp"
#include "th03/app/content/resources.hpp"
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
using th03::app::ClassicLanguage;
using th03::app::ClassicMusicCue;
using th03::app::ClassicOpErrorCode;
using th03::app::ClassicOpInput;
using th03::app::ClassicOpOptionsChoice;
using th03::app::ClassicOpOptionsOutcome;
using th03::app::ClassicOpOptionsOpenFolder;
using th03::app::ClassicOpOptionsPage;
using th03::app::ClassicOpOptionsPhase;
using th03::app::ClassicOpOptionsScene;
using th03::app::ClassicRank;
using th03::app::NewClassicMessageId;
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
	ClassicConfigOptions config{};
	resident.rank = static_cast<std::uint8_t>(config.rank);
	EmptyAnk font{};
	auto created = ClassicOpOptionsScene::create(
		resident, config, true, std::move(raster), {}, menu_background(),
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

void enter_patch_options(ClassicOpOptionsScene &scene);

std::unique_ptr<ClassicOpOptionsScene> make_patch_scene_if_assets_supplied(
	bool localized = false,
	bool online_identity_reset_available = false,
	bool update_available = false
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
		if(!messages) {
			std::cerr << "width-stressing-options-pack-error=" <<
				static_cast<unsigned>(messages.error().code) <<
				" message=" << static_cast<unsigned>(
					messages.error().message
				) << '\n';
		}
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
	ClassicConfigOptions config{};
	if(localized) {
		config.language = ClassicLanguage::english;
	}
	resident.rank = static_cast<std::uint8_t>(config.rank);
	EmptyAnk font{};
	auto created = ClassicOpOptionsScene::create(
		resident, config, true, std::move(raster), {}, menu_background(),
		window_sheet(), std::move(assets), &font, true, 35U, {}, 100U,
		100U, false, online_identity_reset_available, {
			.available = update_available,
		}
	);
	expect(created.has_value(), "hierarchical Options scene creates");
	return created ? std::move(*created) : nullptr;
}

void test_update_page_if_assets_supplied()
{
	auto scene = make_patch_scene_if_assets_supplied(false, false, true);
	if(!scene) {
		return;
	}
	enter_patch_options(*scene);
	for(std::uint8_t row = 0U; row < 5U; ++row) {
		expect(scene->advance({.down = true}).has_value() &&
			scene->advance().has_value(),
			"update test reaches the System category");
	}
	expect(scene->advance({.ok = true}).has_value() &&
		scene->page() == ClassicOpOptionsPage::system,
		"update-capable System page opens");
	expect(scene->advance().has_value(), "System page releases for update test");
	for(std::uint8_t row = 0U; row < 5U; ++row) {
		expect(scene->advance({.down = true}).has_value() &&
			scene->advance().has_value(),
			"update test advances through System actions");
	}
	expect(scene->selected_choice() == ClassicOpOptionsChoice::updates,
		"update-capable System inserts Updates before Return");
	expect(scene->advance({.ok = true}).has_value() &&
		scene->page() == ClassicOpOptionsPage::updates &&
		scene->selected_choice() == ClassicOpOptionsChoice::automatic_updates &&
		scene->automatic_updates(),
		"Updates opens on its enabled automatic policy");
	expect(scene->advance().has_value(), "Updates entry releases");
	expect(scene->advance({.ok = true}).has_value() &&
		!scene->automatic_updates(),
		"Automatic toggles off through the ordinary Option input contract");
	expect(scene->advance().has_value() &&
		scene->advance({.right = true}).has_value() &&
		scene->automatic_updates(),
		"Automatic toggles on through horizontal Option input");
	expect(scene->advance().has_value() &&
		scene->advance({.down = true}).has_value() &&
		scene->selected_choice() == ClassicOpOptionsChoice::check_for_updates,
		"Updates retains a separate explicit check action");
	expect(scene->advance().has_value(), "explicit check selection releases");
	const auto requested = scene->advance({.ok = true});
	expect(requested && requested->check_for_updates,
		"Check for Updates emits one typed host request");
	expect(scene->advance().has_value(), "update request releases");
	const auto duplicate = scene->advance({.ok = true});
	expect(duplicate && !duplicate->check_for_updates,
		"an in-flight update check cannot be duplicated");
	expect(scene->report_update_check(
		th03::app::NewClassicUpdateCheckOutcome::up_to_date
	).has_value(), "host reports a fixed up-to-date outcome");
	const auto published = scene->advance();
	expect(published && published->surface_changed,
		"host update completion owns one source presentation boundary");
	expect(!scene->report_update_check(
		th03::app::NewClassicUpdateCheckOutcome::failed
	).has_value(), "an update result cannot complete twice");
	expect(scene->begin_update_check().has_value(),
		"a host-owned automatic check can begin on the retained page");
	const auto automatic_published = scene->advance();
	expect(automatic_published && automatic_published->surface_changed,
		"automatic checking status owns one source presentation boundary");
	expect(scene->report_update_check(
		th03::app::NewClassicUpdateCheckOutcome::restart_to_apply
	).has_value(), "host reports the next-launch activation outcome");
	expect(scene->advance({.cancel = true}).has_value() &&
		scene->page() == ClassicOpOptionsPage::system &&
		scene->selected_choice() == ClassicOpOptionsChoice::updates,
		"Updates Cancel restores its retained System selection");

	auto hidden = make_patch_scene_if_assets_supplied(false, false, false);
	if(!hidden) {
		return;
	}
	enter_patch_options(*hidden);
	for(std::uint8_t row = 0U; row < 5U; ++row) {
		expect(hidden->advance({.down = true}).has_value() &&
			hidden->advance().has_value(),
			"default-capability test reaches System");
	}
	expect(hidden->advance({.ok = true}).has_value() &&
		hidden->advance().has_value(), "default System opens and releases");
	for(std::uint8_t row = 0U; row < 5U; ++row) {
		expect(hidden->advance({.down = true}).has_value() &&
			hidden->advance().has_value(),
			"default System navigation remains compact");
	}
	expect(hidden->selected_choice() == ClassicOpOptionsChoice::return_page,
		"a build without updater capability exposes no placeholder row");
}

void test_patch_option_label_widths_if_assets_supplied()
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
	expect(loaded.has_value(), "Option label-width assets load");
	if(!loaded) {
		return;
	}
	constexpr std::array labels{
		NewClassicMessageId::option_rank,
		NewClassicMessageId::option_master_volume,
		NewClassicMessageId::option_window_mode,
		NewClassicMessageId::option_window_scale,
		NewClassicMessageId::option_aspect_ratio,
		NewClassicMessageId::option_filtering,
		NewClassicMessageId::option_music,
		NewClassicMessageId::option_key_mode,
		NewClassicMessageId::option_sfx,
		NewClassicMessageId::option_mute_unfocused,
		NewClassicMessageId::option_language,
		NewClassicMessageId::option_flash_fx,
	};
	constexpr std::int32_t restored_left = 162;
	constexpr std::int32_t label_center = 228;
	constexpr std::int32_t label_width_budget =
		(label_center - restored_left) * 2;
	for(const auto label : labels) {
		expect(
			loaded->menu_font.width(loaded->messages().text(label)) <=
				label_width_budget,
			"every built-in Option label fits the restored left column"
		);
	}
}

void enter_patch_options(ClassicOpOptionsScene &scene);

bool patch_option_left_exterior_matches_bare(
	const ClassicOpOptionsScene &scene
) noexcept
{
	for(std::int32_t y = 272; y < 384; ++y) {
		for(std::int32_t x = 0; x < 160; ++x) {
			if(scene.raster().pixel(Page::zero, {x, y}) !=
				scene.raster().pixel(Page::one, {x, y})) {
				return false;
			}
		}
	}
	return true;
}

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

constexpr std::int32_t patch_option_rows_left = 162;
constexpr std::int32_t patch_option_rows_top = 272;
constexpr std::int32_t patch_option_rows_right = 430;
constexpr std::int32_t patch_option_rows_bottom = 384;

bool page_zero_matches(
	const th03::present::Raster &left,
	const th03::present::Raster &right
) noexcept
{
	const auto left_pixels = left.graphics(Page::zero);
	const auto right_pixels = right.graphics(Page::zero);
	if(left_pixels.size() != right_pixels.size()) {
		return false;
	}
	std::size_t difference_count = 0U;
	std::size_t first_difference = 0U;
	for(std::size_t index = 0U; index < left_pixels.size(); ++index) {
		if(left_pixels[index] != right_pixels[index]) {
			if(difference_count == 0U) {
				first_difference = index;
			}
			++difference_count;
		}
	}
	if(difference_count != 0U) {
		std::cerr << "page-diff count=" << difference_count
			<< " x=" << (first_difference % th03::present::graphics_width)
			<< " y=" << (first_difference / th03::present::graphics_width)
			<< " left=" << static_cast<unsigned>(
				left_pixels[first_difference]
			) << " right=" << static_cast<unsigned>(
				right_pixels[first_difference]
			) << '\n';
	}
	return difference_count == 0U;
}

bool page_zero_matches_outside_option_rows(
	const th03::present::Raster &left,
	const th03::present::Raster &right
) noexcept
{
	for(std::int32_t y = 0; y < th03::present::graphics_height; ++y) {
		for(std::int32_t x = 0; x < th03::present::graphics_width; ++x) {
			if(x >= patch_option_rows_left && x < patch_option_rows_right &&
				y >= patch_option_rows_top && y < patch_option_rows_bottom) {
				continue;
			}
			if(left.pixel(Page::zero, {x, y}) !=
				right.pixel(Page::zero, {x, y})) {
				return false;
			}
		}
	}
	return true;
}

void test_patch_option_reciprocal_shadow_if_assets_supplied()
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
	expect(loaded.has_value(), "reciprocal Option shadow assets load");
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
	}, {0, 0}) && raster.set_access_page(Page::zero),
		"reciprocal Option shadow fixture prepares its bare title page");
	ClassicConfigOptions config{};
	config.language = ClassicLanguage::english;
	GameInit resident{};
	resident.rank = static_cast<std::uint8_t>(config.rank);
	EmptyAnk font{};
	auto created = ClassicOpOptionsScene::create(
		resident, config, true, std::move(raster), {}, menu_background(),
		window_sheet(), assets, &font, true
	);
	expect(created.has_value(), "reciprocal Option shadow scene creates");
	if(!created) {
		return;
	}
	auto &scene = **created;
	for(std::uint16_t frame = 0U; frame < 17U; ++frame) {
		expect(scene.advance(
			frame == 0U ? ClassicOpInput{.ok = true} : ClassicOpInput{}
		).has_value(), "reciprocal Option shadow grow advances");
	}
	const auto clean_rows = scene.raster();
	auto expected = clean_rows;
	bool expected_drawn = expected.set_access_page(Page::zero);
	const auto bare = expected.graphics(Page::one);
	for(std::int32_t y = patch_option_rows_top;
		y < patch_option_rows_bottom; ++y) {
		for(std::int32_t x = patch_option_rows_left;
			x < patch_option_rows_right; ++x) {
			const auto offset = static_cast<std::size_t>(y) *
				th03::present::graphics_width + static_cast<std::size_t>(x);
			expected_drawn = expected_drawn &&
				expected.plot({x, y}, bare[offset]);
			if(((x + (y - 264)) & 1) == 0) {
				expected_drawn = expected_drawn &&
					expected.plot({x, y}, 3U);
			}
		}
	}
	constexpr std::array messages{
		NewClassicMessageId::option_category_gameplay,
		NewClassicMessageId::option_category_input,
		NewClassicMessageId::option_category_video,
		NewClassicMessageId::option_category_audio,
		NewClassicMessageId::option_category_language,
		NewClassicMessageId::option_category_system,
		NewClassicMessageId::option_return,
	};
	const auto &catalog = assets->messages(
		th03::app::NewClassicLanguageAssetProfile::english
	);
	for(std::size_t line = 0U; line < messages.size(); ++line) {
		const auto center_color = static_cast<th03::present::Index4>(
			line == 0U ? 15U : 0U
		);
		const auto shadow_color = static_cast<th03::present::Index4>(
			line == 0U ? 0U : 15U
		);
		const auto y = patch_option_rows_top +
			static_cast<std::int32_t>(line * 16U);
		const auto text = catalog.text(messages[line]);
		expected_drawn = expected_drawn &&
			assets->menu_font.put_centered_clipped(
				expected, 297, y + 1, text, shadow_color,
				{162, y, 268U, 16U}
			) && assets->menu_font.put_centered(
				expected, 296, y, text, center_color
			);
	}
	expect(expected_drawn,
		"reciprocal Option shadow reference composes every category row");
	expect(scene.advance().has_value(),
		"reciprocal Option shadow category page publishes");
	expect(page_zero_matches(scene.raster(), expected),
		"every category row draws shadow at (+1,+1) before its exact center");
	expect(page_zero_matches_outside_option_rows(clean_rows, scene.raster()),
		"category shadows stay inside the existing restoration row domain");
	bool selected_black_shadow = false;
	bool unselected_white_shadow = false;
	for(std::int32_t y = 272; y < 288; ++y) {
		for(std::int32_t x = 162; x < 430; ++x) {
			selected_black_shadow = selected_black_shadow ||
				(scene.raster().pixel(Page::zero, {x, y}) == 0U &&
				 clean_rows.pixel(Page::zero, {x, y}) != 0U);
		}
	}
	for(std::int32_t y = 288; y < 304; ++y) {
		for(std::int32_t x = 162; x < 430; ++x) {
			unselected_white_shadow = unselected_white_shadow ||
				(scene.raster().pixel(Page::zero, {x, y}) == 15U &&
				 clean_rows.pixel(Page::zero, {x, y}) != 15U);
		}
	}
	expect(selected_black_shadow,
		"selected white Option text visibly introduces its black shadow");
	expect(unselected_white_shadow,
		"unselected black Option text visibly introduces its white shadow");
}

void test_patch_option_shadow_transitions_if_assets_supplied(bool localized)
{
	auto scene = make_patch_scene_if_assets_supplied(localized);
	if(!scene) {
		return;
	}
	for(std::uint16_t frame = 0U; frame < 17U; ++frame) {
		expect(scene->advance(
			frame == 0U ? ClassicOpInput{.ok = true} : ClassicOpInput{}
		).has_value(), "shadow-transition Option grow advances");
	}
	const auto clean_rows = scene->raster();
	expect(scene->advance().has_value(),
		"shadow-transition category page publishes");
	expect(page_zero_matches_outside_option_rows(clean_rows, scene->raster()),
		localized ?
			"maximum-width localized category shadows stay inside restored rows" :
			"built-in category shadows stay inside restored rows");
	expect(scene->advance().has_value(),
		"shadow-transition category input releases");
	struct Route final {
		ClassicOpOptionsChoice category;
		ClassicOpOptionsPage child;
	};
	constexpr std::array routes{
		Route{ClassicOpOptionsChoice::category_gameplay,
			ClassicOpOptionsPage::gameplay},
		Route{ClassicOpOptionsChoice::category_input,
			ClassicOpOptionsPage::input},
		Route{ClassicOpOptionsChoice::category_video,
			ClassicOpOptionsPage::video},
		Route{ClassicOpOptionsChoice::category_audio,
			ClassicOpOptionsPage::audio},
		Route{ClassicOpOptionsChoice::category_language,
			ClassicOpOptionsPage::language},
		Route{ClassicOpOptionsChoice::category_system,
			ClassicOpOptionsPage::system},
	};
	for(std::size_t index = 0U; index < routes.size(); ++index) {
		if(index != 0U) {
			expect(scene->advance({.down = true}).has_value() &&
				scene->advance().has_value(),
				"shadow-transition category navigation advances and releases");
		}
		expect(scene->selected_choice() == routes[index].category,
			"shadow-transition reaches the expected category");
		const auto parent = scene->raster();
		const auto opened = scene->advance({.ok = true});
		expect(opened.has_value() && scene->page() == routes[index].child,
			"shadow-transition opens the expected child page");
		expect(page_zero_matches_outside_option_rows(clean_rows, scene->raster()),
			localized ?
				"localized child labels and values stay inside restored rows" :
				"built-in child labels and values stay inside restored rows");
		expect(scene->advance().has_value(),
			"shadow-transition child input releases");
		if(routes[index].child == ClassicOpOptionsPage::system) {
			const auto system = scene->raster();
			expect(scene->advance({.ok = true}).has_value() &&
				scene->page() == ClassicOpOptionsPage::switch_confirmation,
				"shadow-transition opens Switch-to-Classic confirmation");
			expect(page_zero_matches_outside_option_rows(
				clean_rows, scene->raster()
			), "confirmation prompt and choices stay inside restored rows");
			expect(scene->advance().has_value() &&
				scene->advance({.cancel = true}).has_value() &&
				page_zero_matches(system, scene->raster()),
				"confirmation Cancel exactly restores the shadowed System page");
			expect(scene->advance().has_value(),
				"shadow-transition restored System input releases");
		}
		expect(scene->advance({.cancel = true}).has_value() &&
			page_zero_matches(parent, scene->raster()),
			"child Cancel exactly restores its shadowed parent category");
		expect(scene->advance().has_value(),
			"shadow-transition restored category input releases");
	}
}

void test_patch_option_japanese_language_and_shadow_if_assets_supplied()
{
	auto scene = make_patch_scene_if_assets_supplied();
	if(!scene) {
		return;
	}
	enter_patch_options(*scene);
	for(std::uint8_t row = 0U; row < 4U; ++row) {
		expect(scene->advance({.down = true}).has_value() &&
			scene->advance().has_value(),
			"Japanese language test advances to Language");
	}
	expect(scene->selected_choice() ==
		ClassicOpOptionsChoice::category_language &&
		scene->advance({.ok = true}).has_value() &&
		scene->page() == ClassicOpOptionsPage::language,
		"Japanese language test opens the Language child");
	expect(scene->advance().has_value(),
		"Japanese language child releases its transition input");
	const auto return_row_hash = [](const th03::present::Raster &raster) {
		std::uint64_t hash = 1'469'598'103'934'665'603ULL;
		for(std::int32_t y = 288; y < 304; ++y) {
			for(std::int32_t x = patch_option_rows_left;
				x < patch_option_rows_right; ++x) {
				hash ^= raster.pixel(Page::zero, {x, y}).value_or(0U);
				hash *= 1'099'511'628'211ULL;
			}
		}
		return hash;
	};
	const auto japanese = scene->raster();
	expect(scene->advance({.right = true}).has_value() &&
		scene->config().language == ClassicLanguage::english,
		"available localized assets switch Japanese to English");
	const auto english = scene->raster();
	expect(!page_zero_matches(japanese, english) &&
		return_row_hash(japanese) != return_row_hash(english),
		"language switch redraws both the selected value and unselected Return row");
	expect(scene->advance().has_value() &&
		scene->advance({.right = true}).has_value() &&
		scene->config().language == ClassicLanguage::spanish_test,
		"bundled Spanish test locale is a distinct third selector choice");
	const auto spanish = scene->raster();
	expect(!page_zero_matches(english, spanish) &&
		return_row_hash(english) != return_row_hash(spanish),
		"Spanish fallback redraws the complete retained Language child");
	expect(scene->advance().has_value() &&
		scene->advance({.right = true}).has_value() &&
		scene->config().language == ClassicLanguage::japanese,
		"language ring wraps Spanish back to Japanese");
	expect(return_row_hash(japanese) == return_row_hash(scene->raster()),
		"Japanese redraw restores the unselected localized Return row exactly");
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
	expect(scene->advance({.down = true}).has_value() &&
		scene->advance().has_value() &&
		scene->selected_choice() == ClassicOpOptionsChoice::arrange,
		"Gameplay navigation reaches the Arrange ruleset selector");
	expect(scene->advance({.right = true}).has_value() &&
		scene->resident().gameplay_ruleset == th03::dopamine_test_v1_ruleset,
		"Gameplay Arrange selects the complete Dopamine test ruleset");
	expect(scene->advance().has_value(), "Gameplay Arrange releases");
	expect(scene->advance({.cancel = true}).has_value() &&
		scene->page() == ClassicOpOptionsPage::categories &&
		scene->selected_choice() ==
			ClassicOpOptionsChoice::category_gameplay,
		"child Cancel restores the exact parent category");
	expect(scene->advance().has_value(), "category return releases");
	for(std::uint8_t row = 0U; row < 3U; ++row) {
		expect(scene->advance({.down = true}).has_value() &&
			scene->advance().has_value(),
			"Audio category navigation advances and releases");
	}
	expect(scene->advance({.ok = true}).has_value() &&
		scene->page() == ClassicOpOptionsPage::audio &&
		scene->selected_choice() == ClassicOpOptionsChoice::master_volume,
		"Audio opens from the category page");
	expect(scene->advance().has_value(), "Audio entry releases");
	expect(patch_option_left_exterior_matches_bare(*scene),
		"Audio labels stay inside the retained Option window");
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
		scene->page() == ClassicOpOptionsPage::categories,
		"Audio Cancel restores the category page");
	expect(patch_option_left_exterior_matches_bare(*scene),
		"Audio return leaves no proportional text outside the restored rows");
	expect(scene->advance().has_value(), "Audio return input releases");
	for(std::uint8_t row = 0U; row < 2U; ++row) {
		expect(scene->advance({.up = true}).has_value() &&
			scene->advance().has_value(),
			"Input category navigation advances and releases");
	}
	expect(scene->advance({.ok = true}).has_value() &&
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
	for(std::uint8_t row = 0U; row < 2U; ++row) {
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

	auto system = make_patch_scene_if_assets_supplied(false, true);
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
	expect(system->advance({.down = true}).has_value() &&
		system->advance().has_value() &&
		system->selected_choice() == ClassicOpOptionsChoice::data_storage,
		"System exposes the Data & Storage child");
	expect(system->advance({.ok = true}).has_value() &&
		system->page() == ClassicOpOptionsPage::data_storage &&
		system->selected_choice() == ClassicOpOptionsChoice::open_user_data,
		"Data & Storage opens on the user-data action");
	expect(system->advance().has_value(), "Data & Storage entry releases");
	const auto open_user_data = system->advance({.ok = true});
	expect(open_user_data && open_user_data->open_folder ==
		ClassicOpOptionsOpenFolder::user_data &&
		system->page() == ClassicOpOptionsPage::data_storage,
		"Open User Data emits a typed host intent without leaving Options");
	expect(system->advance().has_value(), "Open User Data releases");
	expect(system->advance({.down = true}).has_value() &&
		system->advance().has_value() &&
		system->selected_choice() ==
			ClassicOpOptionsChoice::open_replay_folder,
		"Data & Storage exposes the replay-folder action");
	const auto open_replays = system->advance({.ok = true});
	expect(open_replays && open_replays->open_folder ==
		ClassicOpOptionsOpenFolder::replays,
		"Open Replay Folder emits its distinct typed host intent");
	expect(system->advance().has_value(), "Open Replay Folder releases");
	expect(system->advance({.down = true}).has_value() &&
		system->advance().has_value() &&
		system->selected_choice() == ClassicOpOptionsChoice::return_page &&
		system->advance({.ok = true}).has_value() &&
		system->page() == ClassicOpOptionsPage::system &&
		system->selected_choice() == ClassicOpOptionsChoice::data_storage,
		"Data & Storage Return restores the retained System selection");
	expect(system->advance().has_value() &&
		system->advance({.down = true}).has_value() &&
		system->advance().has_value() &&
		system->selected_choice() == ClassicOpOptionsChoice::online_privacy,
		"System exposes the Online & Privacy child");
	expect(system->advance({.ok = true}).has_value() &&
		system->page() == ClassicOpOptionsPage::online_privacy &&
		system->selected_choice() ==
			ClassicOpOptionsChoice::forget_room_identity,
		"protected identity page exposes its reset action");
	expect(system->advance().has_value() &&
		system->advance({.ok = true}).has_value() &&
		system->page() ==
			ClassicOpOptionsPage::forget_identity_confirmation,
		"room identity reset requires explicit confirmation");
	expect(system->advance().has_value() &&
		system->advance({.down = true}).has_value() &&
		system->advance().has_value() &&
		system->selected_choice() ==
			ClassicOpOptionsChoice::cancel_forget_room_identity &&
		system->advance({.ok = true}).has_value() &&
		system->page() == ClassicOpOptionsPage::online_privacy,
		"reset No restores the retained Online & Privacy owner");
	expect(system->advance().has_value() &&
		system->advance({.ok = true}).has_value() &&
		system->advance().has_value(),
		"second reset confirmation opens and releases");
	const auto forgotten = system->advance({.ok = true});
	expect(forgotten && forgotten->forget_online_identity &&
		system->page() == ClassicOpOptionsPage::online_privacy,
		"reset Yes emits one typed host request and restores its parent");
	expect(system->advance().has_value() &&
		system->advance({.down = true}).has_value() &&
		system->advance().has_value() &&
		system->selected_choice() == ClassicOpOptionsChoice::return_page &&
		system->advance({.ok = true}).has_value() &&
		system->page() == ClassicOpOptionsPage::system &&
		system->selected_choice() == ClassicOpOptionsChoice::online_privacy,
		"Online & Privacy Return restores its retained System selection");
	expect(system->advance().has_value() &&
		system->advance({.down = true}).has_value() &&
		system->advance().has_value() &&
		system->selected_choice() == ClassicOpOptionsChoice::credits,
		"System exposes the factual Credits child");
	expect(system->advance({.ok = true}).has_value() &&
		system->page() == ClassicOpOptionsPage::credits &&
		system->selected_choice() == ClassicOpOptionsChoice::return_page,
		"Credits opens with a single retained Return action");
	expect(system->advance().has_value(), "Credits entry releases");
	expect(system->advance({.cancel = true}).has_value() &&
		system->page() == ClassicOpOptionsPage::system &&
		system->selected_choice() == ClassicOpOptionsChoice::credits,
		"Credits Cancel restores its retained System selection");
	expect(system->advance().has_value() &&
		system->advance({.down = true}).has_value() &&
		system->advance().has_value() &&
		system->selected_choice() == ClassicOpOptionsChoice::diagnostics,
		"System exposes the Diagnostics child");
	expect(system->advance({.ok = true}).has_value() &&
		system->page() == ClassicOpOptionsPage::diagnostics &&
		system->selected_choice() == ClassicOpOptionsChoice::open_log_folder,
		"Diagnostics opens on the log-folder action");
	expect(system->advance().has_value(), "Diagnostics entry releases");
	const auto open_log = system->advance({.ok = true});
	expect(open_log && open_log->open_folder ==
		ClassicOpOptionsOpenFolder::user_data &&
		system->page() == ClassicOpOptionsPage::diagnostics,
		"Open Log Folder emits the authoritative user-data directory intent");
	expect(system->advance().has_value(), "Open Log Folder releases");
	expect(system->advance({.down = true}).has_value() &&
		system->advance().has_value() &&
		system->selected_choice() == ClassicOpOptionsChoice::return_page &&
		system->advance({.ok = true}).has_value() &&
		system->page() == ClassicOpOptionsPage::system &&
		system->selected_choice() == ClassicOpOptionsChoice::diagnostics,
		"Diagnostics Return restores its retained System selection");
	expect(system->advance().has_value() &&
		system->advance({.up = true}).has_value() &&
		system->advance().has_value() &&
		system->advance({.up = true}).has_value() &&
		system->advance().has_value() &&
		system->advance({.up = true}).has_value() &&
		system->advance().has_value() &&
		system->advance({.up = true}).has_value() &&
		system->advance().has_value() &&
		system->selected_choice() == ClassicOpOptionsChoice::switch_classic,
		"System navigation returns to Switch to Classic");
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

void test_environment_online_identity_if_assets_supplied()
{
	auto scene = make_patch_scene_if_assets_supplied(false, false);
	if(!scene) {
		return;
	}
	enter_patch_options(*scene);
	for(std::uint8_t row = 0U; row < 5U; ++row) {
		expect(scene->advance({.down = true}).has_value() &&
			scene->advance().has_value(),
			"environment identity test reaches System");
	}
	expect(scene->advance({.ok = true}).has_value() &&
		scene->advance().has_value(),
		"environment identity test enters and releases System");
	for(std::uint8_t row = 0U; row < 2U; ++row) {
		expect(scene->advance({.down = true}).has_value() &&
			scene->advance().has_value(),
			"environment identity test reaches Online & Privacy");
	}
	expect(scene->selected_choice() ==
		ClassicOpOptionsChoice::online_privacy &&
		scene->advance({.ok = true}).has_value() &&
		scene->page() == ClassicOpOptionsPage::online_privacy &&
		scene->selected_choice() == ClassicOpOptionsChoice::return_page,
		"environment-owned identity exposes no misleading reset action");
	const auto release_tick = scene->advance();
	expect(release_tick && !release_tick->forget_online_identity,
		"environment identity page emits no reset request");
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
		th03::app::ClassicOpPhase::main_menu && !held->surface_changed &&
		!(*resumed)->take_completion(),
		"Replay Patch Options return redraws every shadowed row without releasing the held-input lock");
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
	test_patch_option_label_widths_if_assets_supplied();
	test_patch_option_reciprocal_shadow_if_assets_supplied();
	test_patch_option_japanese_language_and_shadow_if_assets_supplied();
	test_patch_option_shadow_transitions_if_assets_supplied(false);
	test_patch_option_shadow_transitions_if_assets_supplied(true);
	test_replay_patch_option_hierarchy_if_assets_supplied();
	test_update_page_if_assets_supplied();
	test_environment_online_identity_if_assets_supplied();
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
