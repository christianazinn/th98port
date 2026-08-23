#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

#include "new_classic_menu_preview.hpp"
#include "th03/app/new_classic_key_config_scene.hpp"

namespace {

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

void put_u16(std::vector<std::byte> &bytes, std::size_t at, std::uint16_t value)
{
	bytes[at] = static_cast<std::byte>(value & 0xFFU);
	bytes[at + 1U] = static_cast<std::byte>(value >> 8U);
}

std::vector<std::byte> font_fixture()
{
	std::vector<std::byte> bytes(3'168U, std::byte{0U});
	constexpr std::array magic{'T', '3', 'P', 'F', 'N', 'T', '1'};
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		bytes[index] = static_cast<std::byte>(magic[index]);
	}
	put_u16(bytes, 8U, 3'168U);
	put_u16(bytes, 10U, 32U);
	put_u16(bytes, 12U, 32U);
	put_u16(bytes, 14U, 128U);
	put_u16(bytes, 16U, 95U);
	bytes[18U] = std::byte{0x20U};
	bytes[19U] = std::byte{0x7EU};
	bytes[20U] = std::byte{16U};
	bytes[21U] = std::byte{16U};
	bytes[22U] = std::byte{0x3FU};
	for(std::size_t glyph = 0U; glyph < 95U; ++glyph) {
		bytes[32U + glyph] = std::byte{8U};
		bytes[128U + (glyph * 32U)] = std::byte{0x80U};
	}
	std::uint16_t checksum = 0U;
	for(std::size_t index = 32U; index < bytes.size(); ++index) {
		checksum = static_cast<std::uint16_t>(
			checksum + std::to_integer<std::uint8_t>(bytes[index])
		);
	}
	put_u16(bytes, 24U, checksum);
	return bytes;
}

std::shared_ptr<const th03::app::NewClassicReplayPatchAssets> assets_fixture()
{
	auto decoded = th03::app::detail::decode_new_classic_menu_font(font_fixture());
	if(!decoded) {
		return {};
	}
	auto assets = std::make_shared<th03::app::NewClassicReplayPatchAssets>();
	assets->menu_font = std::move(*decoded);
	auto &background = assets->new_menu_backgrounds[static_cast<std::size_t>(
		th03::app::NewClassicMenuBackground::neutral
	)];
	background.palette.emplace();
	for(std::size_t index = 0U; index < 16U; ++index) {
		background.palette->colors[index] = {
			static_cast<std::uint8_t>(index),
			static_cast<std::uint8_t>(index),
			static_cast<std::uint8_t>(index),
		};
	}
	background.image.width = 640U;
	background.image.height = 400U;
	background.image.indices.assign(256'000U, 3U);
	return assets;
}

th03::app::NewClassicKeyConfig config_fixture()
{
	th03::app::NewClassicKeyConfig result{};
	for(std::size_t index = 0U; index < result.bindings.size(); ++index) {
		result.bindings[index] = static_cast<std::uint16_t>(index + 1U);
	}
	return result;
}

std::string_view key_name(void *, std::uint16_t key) noexcept
{
	constexpr std::array names{
		std::string_view{""}, std::string_view{"One"},
		std::string_view{"Two"}, std::string_view{"Three"},
		std::string_view{"Four"}, std::string_view{"Five"},
	};
	return key < names.size() ? names[key] : std::string_view{};
}

using Scene = th03::app::NewClassicKeyConfigScene;

void finish_fade(Scene &scene)
{
	for(std::size_t boundary = 0U; boundary < 19U; ++boundary) {
		static_cast<void>(scene.consume({}));
	}
}

void release(Scene &scene)
{
	static_cast<void>(scene.consume({}));
}

void press(Scene &scene, th03::app::NewClassicUiInput ui)
{
	static_cast<void>(scene.consume({.ui = ui}));
}

std::uint8_t pixel(
	th03::present::SourceSurface surface,
	std::size_t x,
	std::size_t y
)
{
	return surface.pixels[(y * 640U) + x].value;
}

void test_source_geometry_and_fade()
{
	auto assets = assets_fixture();
	expect(static_cast<bool>(assets), "font fixture decodes");
	if(!assets) {
		return;
	}
	Scene scene(config_fixture(), config_fixture(), 512U, 41U, key_name, assets);
	expect(scene.valid(), "authenticated-shape scene initializes");
	const auto initial = scene.surface();
	expect(
		initial.shown_page == th03::present::Page::one &&
		initial.palette_tone == 0 && !initial.host_overlay,
		"first complete neutral-background frame is composed hidden and kept black"
	);
	expect(pixel(initial, 0U, 0U) == 3U,
		"KeyConfig uses the source-distinct project-owned neutral background");
	expect(
		pixel(initial, 284U, 64U) == 14U &&
		pixel(initial, 248U, 96U) == 14U &&
		pixel(initial, 300U, 96U) == 14U &&
		pixel(initial, 200U, 128U) == 11U &&
		pixel(initial, 216U, 128U) == 11U &&
		pixel(initial, 312U, 128U) == 11U,
		"localized scene title, Story page carets, page title, cursor, label, and value use visible source positions and colors"
	);
	expect(
		th03::app::NewClassicKeyConfigSceneTestAccess::page(scene) == 2U &&
		th03::app::NewClassicKeyConfigSceneTestAccess::selected_row(scene) == 0U,
		"KeyConfig enters on Story Up"
	);
	finish_fade(scene);
	expect(
		scene.surface().palette_tone == 100 &&
		scene.status() == th03::app::NewClassicKeyConfigSceneStatus::running,
		"palette_black_in(1) reaches the exact final tone before input"
	);
}

void test_player_column_and_page_navigation()
{
	auto assets = assets_fixture();
	Scene scene(config_fixture(), config_fixture(), 512U, 41U, key_name, assets);
	finish_fade(scene);
	press(scene, {.right = true});
	expect(
		th03::app::NewClassicKeyConfigSceneTestAccess::page(scene) == 0U &&
		th03::app::NewClassicKeyConfigSceneTestAccess::selected_row(scene) == 9U &&
		!th03::app::NewClassicKeyConfigSceneTestAccess::movement_column(scene),
		"Story Right enters P1 on Shot in the action column"
	);
	expect(
		scene.surface().shown_page == th03::present::Page::zero,
		"navigation composes the neutral background on the hidden page before flipping"
	);
	release(scene);
	press(scene, {.right = true});
	expect(
		th03::app::NewClassicKeyConfigSceneTestAccess::selected_row(scene) == 3U &&
		th03::app::NewClassicKeyConfigSceneTestAccess::movement_column(scene),
		"P1 Shot Right crosses to the geometrically adjacent Up-Right row"
	);
	release(scene);
	press(scene, {.left = true});
	expect(
		th03::app::NewClassicKeyConfigSceneTestAccess::selected_row(scene) == 9U &&
		!th03::app::NewClassicKeyConfigSceneTestAccess::movement_column(scene),
		"movement-column Left maps back to the action column"
	);
	release(scene);
	press(scene, {.left = true});
	expect(
		th03::app::NewClassicKeyConfigSceneTestAccess::page(scene) == 2U &&
		th03::app::NewClassicKeyConfigSceneTestAccess::selected_row(scene) == 0U,
		"action-column Left wraps P1 to Story"
	);
}

void test_capture_release_barriers_and_swap()
{
	auto assets = assets_fixture();
	auto initial = config_fixture();
	Scene scene(initial, initial, 512U, 41U, key_name, assets);
	finish_fade(scene);
	const auto front = scene.surface().shown_page;
	auto capture = scene.consume({
		.ui = {.confirm = true},
		.any_scancode_held = true,
	});
	expect(
		capture.surface_changed && scene.surface().shown_page == front &&
		pixel(scene.surface(), 188U, 320U) == 14U,
		"capture prompt overlays the complete front page without a flip"
	);
	static_cast<void>(scene.consume({.any_scancode_held = false}));
	static_cast<void>(scene.consume({
		.scancode_pressed = std::uint16_t{24U},
		.any_scancode_held = true,
	}));
	expect(
		scene.config().bindings[22U] == 24U &&
		scene.config().bindings[23U] == 23U &&
		scene.surface().shown_page == front,
		"captured duplicate swaps within the Story binding group before redraw"
	);
	auto released = scene.consume({.any_scancode_held = false});
	expect(
		released.surface_changed && scene.surface().shown_page != front,
		"capture redraw and page flip wait for the captured key's final release"
	);
}

void test_discard_apply_and_save_failure()
{
	auto assets = assets_fixture();
	auto initial = config_fixture();
	Scene scene(initial, initial, 512U, 41U, key_name, assets);
	finish_fade(scene);
	for(std::size_t row = 0U; row < 7U; ++row) {
		press(scene, {.down = true});
		release(scene);
	}
	press(scene, {.confirm = true});
	expect(scene.config().autofire == 1U, "Story Autofire toggles P1 only");
	release(scene);
	auto prompt = scene.consume({.ui = {.cancel = true}});
	expect(prompt.surface_changed, "changed Cancel overlays discard confirmation");
	release(scene);
	auto no = scene.consume({.ui = {.cancel = true}});
	expect(
		no.surface_changed &&
		scene.status() == th03::app::NewClassicKeyConfigSceneStatus::running,
		"discard No redraws the current page and retains edits"
	);
	release(scene);
	press(scene, {.down = true});
	release(scene);
	press(scene, {.down = true});
	release(scene);
	auto apply = scene.consume({.ui = {.confirm = true}});
	expect(
		apply.status ==
			th03::app::NewClassicKeyConfigSceneStatus::apply_requested,
		"Apply publishes the retryable store transaction"
	);
	scene.report_commit(false);
	expect(
		scene.status() == th03::app::NewClassicKeyConfigSceneStatus::running,
		"save failure returns directly to the source menu loop"
	);
	release(scene);
	apply = scene.consume({.ui = {.confirm = true}});
	expect(
		apply.status ==
			th03::app::NewClassicKeyConfigSceneStatus::apply_requested,
		"Apply can be retried while the source failure overlay remains"
	);
	scene.report_commit(true);
	finish_fade(scene);
	expect(
		scene.status() == th03::app::NewClassicKeyConfigSceneStatus::applied &&
		scene.surface().palette_tone == 0,
		"successful store runs palette_black_out(1) before returning"
	);

	Scene discard(initial, initial, 512U, 41U, key_name, assets);
	finish_fade(discard);
	for(std::size_t row = 0U; row < 7U; ++row) {
		press(discard, {.down = true});
		release(discard);
	}
	press(discard, {.confirm = true});
	release(discard);
	press(discard, {.cancel = true});
	release(discard);
	press(discard, {.confirm = true});
	expect(
		discard.config() == initial,
		"discard confirmation retires the edited transaction without leaking it"
	);
}

void test_authentic_assets_if_supplied()
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
	const auto assets = th03::test::authentic_menu_assets();
	expect(static_cast<bool>(assets), "authentic Replay Patch assets load");
	if(!assets) {
		return;
	}
	auto config = config_fixture();
	Scene scene(config, config, 512U, 41U, key_name, assets);
	Scene japanese(
		config, config, 512U, 41U, key_name, assets, nullptr,
		th03::app::NewClassicLanguageAssetProfile::japanese
	);
	expect(
		scene.valid() && japanese.valid() && !scene.surface().host_overlay &&
		scene.surface().shown_page == th03::present::Page::one,
		"authentic menu assets compose the first neutral source-domain frame"
	);
	finish_fade(scene);
	finish_fade(japanese);
	th03::test::dump_menu_preview(scene.surface(), "input-key-config-en.ppm");
	th03::test::dump_menu_preview(
		japanese.surface(), "input-key-config-ja.ppm"
	);
}

void test_localized_surface_contract()
{
	auto base_assets = assets_fixture();
	expect(static_cast<bool>(base_assets), "KeyConfig localization assets decode");
	if(!base_assets) {
		return;
	}
	auto localized_assets =
		std::make_shared<th03::app::NewClassicReplayPatchAssets>(*base_assets);
	localized_assets->custom_messages =
		std::make_shared<const th03::app::NewClassicMessageCatalog>(
			th03::app::detail::new_classic_width_test_messages()
		);
	auto config = config_fixture();
	Scene base(config, config, 512U, 41U, key_name, base_assets);
	Scene localized(config, config, 512U, 41U, key_name, localized_assets);
	Scene japanese(
		config, config, 512U, 41U, key_name, base_assets, nullptr,
		th03::app::NewClassicLanguageAssetProfile::japanese
	);
	expect(
		!std::ranges::equal(base.surface().pixels, localized.surface().pixels),
		"KeyConfig pseudo-locale changes catalog-owned semantic pixels"
	);
	expect(
		base.surface().shown_page == localized.surface().shown_page &&
		base.surface().palette_tone == localized.surface().palette_tone &&
		base.surface().graphics_palette == localized.surface().graphics_palette,
		"KeyConfig localization preserves page and palette cadence"
	);
	expect(japanese.valid() &&
		!std::ranges::equal(base.surface().pixels, japanese.surface().pixels),
		"KeyConfig renders the explicitly selected Japanese catalog");
}

} // namespace

int main()
{
	test_source_geometry_and_fade();
	test_player_column_and_page_navigation();
	test_capture_release_barriers_and_swap();
	test_discard_apply_and_save_failure();
	test_authentic_assets_if_supplied();
	test_localized_surface_contract();
	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << "New Classic KeyConfig scene checks=" << checks << "\n";
	return 0;
}
