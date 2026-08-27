#include "new_classic_menu_preview.hpp"
#include "th03/app/new_classic/input/new_classic_device_scene.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

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

std::uint8_t pixel(
	th03::present::SourceSurface surface,
	std::size_t x,
	std::size_t y
)
{
	return surface.pixels[(y * 640U) + x].value;
}

th03::app::NewClassicControllerIdentity identity(std::uint8_t value)
{
	th03::app::NewClassicControllerIdentity result{};
	result.bytes[0] = value;
	return result;
}

th03::app::NewClassicControllerCatalog catalog()
{
	th03::app::NewClassicControllerCatalog result{};
	result.count = 2U;
	result.controllers[0].identity = identity(1U);
	result.controllers[0].name[0] = 'A';
	result.controllers[0].player = th03::app::NewClassicControllerPlayer::p1;
	result.controllers[1].identity = identity(2U);
	result.controllers[1].name[0] = 'B';
	result.controllers[1].player = th03::app::NewClassicControllerPlayer::p2;
	return result;
}

using Scene = th03::app::NewClassicDeviceScene;

void finish_fade(Scene &scene, const th03::app::NewClassicControllerCatalog &pads)
{
	for(std::size_t boundary = 0U; boundary < 19U; ++boundary) {
		static_cast<void>(scene.consume({}, pads, {}));
	}
}

void release(Scene &scene, const th03::app::NewClassicControllerCatalog &pads)
{
	static_cast<void>(scene.consume({}, pads, {}));
}

void test_assignment_apply_and_duplicate_ownership()
{
	const auto pads = catalog();
	Scene scene(
		th03::app::NewClassicDeviceSceneMode::assignment, {}, {},
		assets_fixture()
	);
	expect(scene.valid(), "device assignment scene initializes");
	expect(scene.surface().palette_tone == 0 && !scene.surface().host_overlay,
		"device assignment starts on the black neutral-background frame");
	expect(pixel(scene.surface(), 0U, 0U) == 3U,
		"Device Assignment uses the source-distinct project-owned neutral background");
	finish_fade(scene, pads);
	expect(scene.surface().palette_tone == 100,
		"device assignment runs the source-style black-in cadence");

	static_cast<void>(scene.consume({.right = true}, pads, {}));
	expect(scene.assignment().mode[0] ==
		th03::app::NewClassicControllerAssignmentMode::disabled,
		"right cycles Automatic to Disabled");
	release(scene, pads);
	static_cast<void>(scene.consume({.right = true}, pads, {}));
	expect(scene.assignment().mode[0] ==
		th03::app::NewClassicControllerAssignmentMode::specific &&
		scene.assignment().identity[0] == identity(1U),
		"next value selects the first stable controller identity");
	release(scene, pads);
	static_cast<void>(scene.consume({.down = true}, pads, {}));
	release(scene, pads);
	static_cast<void>(scene.consume({.right = true}, pads, {}));
	release(scene, pads);
	static_cast<void>(scene.consume({.right = true}, pads, {}));
	expect(scene.assignment().mode[1] ==
		th03::app::NewClassicControllerAssignmentMode::specific &&
		scene.assignment().identity[1] == identity(1U) &&
		scene.assignment().mode[0] ==
			th03::app::NewClassicControllerAssignmentMode::automatic,
		"selecting one identity for P2 atomically releases it from P1");

	release(scene, pads);
	for(std::size_t row = 0U; row < 2U; ++row) {
		static_cast<void>(scene.consume({.down = true}, pads, {}));
		release(scene, pads);
	}
	auto apply = scene.consume({.confirm = true}, pads, {});
	expect(apply.status == th03::app::NewClassicDeviceSceneStatus::apply_requested,
		"Apply requests the external atomic store transaction");
	scene.report_commit(true);
	finish_fade(scene, pads);
	expect(scene.status() == th03::app::NewClassicDeviceSceneStatus::applied,
		"successful assignment store fades back to Options");
}

void test_live_input_test_and_return()
{
	auto pads = catalog();
	Scene scene(
		th03::app::NewClassicDeviceSceneMode::input_test, {}, {},
		assets_fixture()
	);
	finish_fade(scene, pads);
	const auto before = scene.surface().shown_page;
	pads.controllers[0].p1 = 0x0020U;
	th03::CoreInput logical{.p1 = 0x0020U, .system = 0x2000U};
	auto refreshed = scene.consume({}, pads, logical);
	expect(refreshed.surface_changed && scene.surface().shown_page != before,
		"input test redraws live logical and per-controller contributions");
	static_cast<void>(scene.consume({.cancel = true}, pads, logical));
	finish_fade(scene, pads);
	expect(scene.status() == th03::app::NewClassicDeviceSceneStatus::back,
		"input test Cancel fades back without persisting anything");
}

void test_localized_surface_contract()
{
	auto base_assets = assets_fixture();
	expect(static_cast<bool>(base_assets), "Device localization assets decode");
	if(!base_assets) {
		return;
	}
	auto localized_assets =
		std::make_shared<th03::app::NewClassicReplayPatchAssets>(*base_assets);
	localized_assets->custom_messages =
		std::make_shared<const th03::app::NewClassicMessageCatalog>(
			th03::app::detail::new_classic_width_test_messages()
		);
	Scene base(
		th03::app::NewClassicDeviceSceneMode::assignment, {}, {}, base_assets
	);
	Scene localized(
		th03::app::NewClassicDeviceSceneMode::assignment, {}, {}, localized_assets
	);
	Scene japanese(
		th03::app::NewClassicDeviceSceneMode::assignment, {}, {}, base_assets,
		th03::app::NewClassicLanguageAssetProfile::japanese
	);
	expect(
		!std::ranges::equal(base.surface().pixels, localized.surface().pixels),
		"Device pseudo-locale changes catalog-owned semantic pixels"
	);
	expect(
		base.surface().shown_page == localized.surface().shown_page &&
		base.surface().palette_tone == localized.surface().palette_tone &&
		base.surface().graphics_palette == localized.surface().graphics_palette,
		"Device localization preserves page and palette cadence"
	);
	expect(japanese.valid() &&
		!std::ranges::equal(base.surface().pixels, japanese.surface().pixels),
		"Device screens render the explicitly selected Japanese catalog");
}

void test_authentic_previews_if_requested()
{
	if(!th03::test::environment("TH03_MENU_PREVIEW_DIR")) {
		return;
	}
	const auto assets = th03::test::authentic_menu_assets();
	expect(static_cast<bool>(assets), "authentic Device assets load");
	if(!assets) {
		return;
	}
	Scene assignment(
		th03::app::NewClassicDeviceSceneMode::assignment, {}, {}, assets
	);
	Scene input_test(
		th03::app::NewClassicDeviceSceneMode::input_test, {}, {}, assets
	);
	Scene japanese_assignment(
		th03::app::NewClassicDeviceSceneMode::assignment, {}, {}, assets,
		th03::app::NewClassicLanguageAssetProfile::japanese
	);
	Scene japanese_input_test(
		th03::app::NewClassicDeviceSceneMode::input_test, {}, {}, assets,
		th03::app::NewClassicLanguageAssetProfile::japanese
	);
	expect(assignment.valid() && input_test.valid() &&
		japanese_assignment.valid() && japanese_input_test.valid(),
		"authentic English and Japanese Device scenes compose");
	const auto pads = catalog();
	finish_fade(assignment, pads);
	finish_fade(input_test, pads);
	finish_fade(japanese_assignment, pads);
	finish_fade(japanese_input_test, pads);
	th03::test::dump_menu_preview(
		assignment.surface(), "input-device-assignment-en.ppm"
	);
	th03::test::dump_menu_preview(
		input_test.surface(), "input-test-en.ppm"
	);
	th03::test::dump_menu_preview(
		japanese_assignment.surface(), "input-device-assignment-ja.ppm"
	);
	th03::test::dump_menu_preview(
		japanese_input_test.surface(), "input-test-ja.ppm"
	);
}

} // namespace

int main()
{
	test_assignment_apply_and_duplicate_ownership();
	test_live_input_test_and_return();
	test_localized_surface_contract();
	test_authentic_previews_if_requested();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
