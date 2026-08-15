#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "th03/app/new_classic_joy_config_scene.hpp"

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
	auto &background = assets->key_config_background;
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
	background.image.indices.assign(256'000U, 2U);
	return assets;
}

using Scene = th03::app::NewClassicJoyConfigScene;

void finish_fade(Scene &scene)
{
	for(std::size_t boundary = 0U; boundary < 19U; ++boundary) {
		static_cast<void>(scene.consume({}));
	}
}

void release(Scene &scene) { static_cast<void>(scene.consume({})); }
void press(Scene &scene, th03::app::NewClassicUiInput input)
{
	static_cast<void>(scene.consume(input));
}

void move_down(Scene &scene, std::size_t count)
{
	for(std::size_t step = 0U; step < count; ++step) {
		press(scene, {.down = true});
		release(scene);
	}
}

void test_geometry_navigation_and_values()
{
	auto assets = assets_fixture();
	Scene scene({}, {}, assets);
	expect(scene.valid(), "JoyConfig source-style scene initializes");
	expect(scene.surface().shown_page == th03::present::Page::one &&
		scene.surface().palette_tone == 0 && !scene.surface().host_overlay,
		"JoyConfig first BG1 frame is hidden-page composed and black");
	expect(th03::app::NewClassicJoyConfigSceneTestAccess::page(scene) == 0U &&
		th03::app::NewClassicJoyConfigSceneTestAccess::selected_row(scene) == 0U,
		"JoyConfig begins on Buttons/Shot");
	finish_fade(scene);
	expect(scene.surface().palette_tone == 100,
		"JoyConfig runs the exact palette black-in boundary count");

	press(scene, {.right = true});
	expect(scene.config().buttons[0U] == th03::app::NewClassicJoyButton::east &&
		scene.config().buttons[1U] == th03::app::NewClassicJoyButton::south,
		"button-ring duplicate swaps the previous assignment");
	release(scene);
	move_down(scene, 5U);
	press(scene, {.confirm = true});
	expect(th03::app::NewClassicJoyConfigSceneTestAccess::page(scene) == 1U &&
		th03::app::NewClassicJoyConfigSceneTestAccess::selected_row(scene) == 0U,
		"Axis and Dead Zone command enters the second page");
	release(scene);
	press(scene, {.confirm = true});
	expect(scene.config().horizontal_axis ==
		th03::app::NewClassicJoyAxis::right_x,
		"Confirm cycles the selected horizontal axis");
	release(scene);
	move_down(scene, 4U);
	press(scene, {.left = true});
	expect(scene.config().dead_zone_percent == 45U,
		"dead zone changes in five-percent steps");
}

void test_defaults_apply_discard_and_failure()
{
	auto assets = assets_fixture();
	auto defaults = th03::app::NewClassicJoyConfig{};
	auto initial = defaults;
	initial.dead_zone_percent = 60U;
	Scene scene(initial, defaults, assets);
	finish_fade(scene);
	move_down(scene, 6U);
	press(scene, {.confirm = true});
	expect(scene.config() == defaults, "Restore Defaults resets the full policy");
	release(scene);
	press(scene, {.down = true});
	release(scene);
	auto apply = scene.consume({.confirm = true});
	expect(apply.status ==
		th03::app::NewClassicJoyConfigSceneStatus::apply_requested,
		"Apply requests the external atomic transaction");
	scene.report_commit(false);
	expect(scene.status() ==
		th03::app::NewClassicJoyConfigSceneStatus::running,
		"store failure is retryable without losing edits");
	release(scene);
	apply = scene.consume({.confirm = true});
	expect(apply.status ==
		th03::app::NewClassicJoyConfigSceneStatus::apply_requested,
		"failed JoyConfig store can be retried");
	scene.report_commit(true);
	finish_fade(scene);
	expect(scene.status() == th03::app::NewClassicJoyConfigSceneStatus::applied &&
		scene.surface().palette_tone == 0,
		"successful JoyConfig store fades out before return");

	Scene discard(initial, defaults, assets);
	finish_fade(discard);
	press(discard, {.right = true});
	release(discard);
	press(discard, {.cancel = true});
	release(discard);
	press(discard, {.confirm = true});
	finish_fade(discard);
	expect(discard.config() == initial && discard.status() ==
		th03::app::NewClassicJoyConfigSceneStatus::back,
		"discard confirmation restores the entry policy and returns");
}

void test_localized_surface_contract()
{
	auto base_assets = assets_fixture();
	expect(static_cast<bool>(base_assets), "JoyConfig localization assets decode");
	if(!base_assets) {
		return;
	}
	auto localized_assets =
		std::make_shared<th03::app::NewClassicReplayPatchAssets>(*base_assets);
	localized_assets->custom_messages =
		std::make_shared<const th03::app::NewClassicMessageCatalog>(
			th03::app::detail::new_classic_width_test_messages()
		);
	Scene base({}, {}, base_assets);
	Scene localized({}, {}, localized_assets);
	expect(
		!std::ranges::equal(base.surface().pixels, localized.surface().pixels),
		"JoyConfig pseudo-locale changes catalog-owned semantic pixels"
	);
	expect(
		base.surface().shown_page == localized.surface().shown_page &&
		base.surface().palette_tone == localized.surface().palette_tone &&
		base.surface().graphics_palette == localized.surface().graphics_palette,
		"JoyConfig localization preserves page and palette cadence"
	);
}

} // namespace

int main()
{
	test_geometry_navigation_and_values();
	test_defaults_apply_discard_and_failure();
	test_localized_surface_contract();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
