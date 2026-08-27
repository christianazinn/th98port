#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/new_classic_practice.hpp"
#include "th03/app/new_classic/replay/new_classic_replay.hpp"
#include "th03/formats/v14.hpp"

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const std::string &message)
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

std::shared_ptr<const th03::app::NewClassicReplayPatchAssets> patch_assets()
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
	auto font = th03::app::detail::decode_new_classic_menu_font(bytes);
	check(font.has_value(), "synthetic Replay Patch MNUFONT decodes");
	if(!font) {
		return {};
	}
	auto assets = std::make_shared<th03::app::NewClassicReplayPatchAssets>();
	assets->menu_font = std::move(*font);
	return assets;
}

struct RetainedSelection final {
	std::vector<th03::present::ComposedPixel> pixels{
		th03::present::graphics_page_pixels,
		{th03::present::ComposedPixelLayer::graphics, 3U},
	};

	[[nodiscard]] th03::present::SourceSurface surface() const noexcept
	{
		return {
			.pixels = std::span<
				const th03::present::ComposedPixel,
				th03::present::graphics_page_pixels
			>(pixels.data(), pixels.size()),
			.shown_page = th03::present::Page::zero,
			.palette_tone = 100,
			.host_overlay = false,
		};
	}
};

const th03::present::ComposedPixel &pixel(
	th03::present::SourceSurface surface,
	std::size_t x,
	std::size_t y
)
{
	return surface.pixels[(y * th03::present::graphics_width) + x];
}

th03::GameInit selected(std::int8_t p1 = 1, std::int8_t cpu = 3)
{
	th03::GameInit resident{};
	resident.resident_rand = 37;
	resident.random_seed = 37U;
	resident.game_mode = th03::game_mode::vs_1p_cpu;
	resident.rank = 1U;
	resident.key_mode = 0U;
	resident.is_cpu = {0U, 1U};
	resident.paletted_character = {p1, cpu};
	return resident;
}

th03::GameContent content()
{
	th03::GameContent value{};
	value.enemy_formations.resize(2U);
	for(auto &formation : value.enemy_formations) {
		formation.enemy_scripts.push_back({0U});
	}
	return value;
}

void release(th03::app::NewClassicPracticeMenu &menu)
{
	static_cast<void>(menu.consume({}));
}

void press(
	th03::app::NewClassicPracticeMenu &menu,
	th03::app::NewClassicPracticeInput input
)
{
	static_cast<void>(menu.consume(input));
	release(menu);
}

th03::app::NewClassicPracticeStatus finish_start_fade(
	th03::app::NewClassicPracticeMenu &menu
)
{
	auto status = menu.status();
	for(std::size_t boundary = 0U; boundary < 19U; ++boundary) {
		status = menu.consume({});
	}
	return status;
}

void test_source_menu()
{
	th03::app::NewClassicPracticeMenu menu(selected());
	check(
		menu.config().preset == 1U && menu.config().stage == 6U &&
		menu.config().round == 0U && menu.config().p1_spell == 7U &&
		menu.config().cpu_spell == 7U && menu.config().stock == 2U,
		"Practice defaults use the selected Reimu-vs-Mima fixed Story stage"
	);
	check(
		menu.config().initial_cpu_safety_frames == 1000U,
		"Story-native stage 7 round 1 safety comes from the shared source table"
	);

	release(menu);
	for(int row = 0; row < 3; ++row) {
		press(menu, {.raw_any = true, .down = true});
	}
	press(menu, {.raw_any = true, .confirm = true});
	press(menu, {.raw_any = true, .right = true});
	check(
		menu.config().cpu_timer == 2U &&
		menu.config().initial_cpu_safety_frames == 0xFFFFU,
		"advanced CPU Timer steps Story to Infinite and refreshes the anchor"
	);
	press(menu, {.raw_any = true, .cancel = true});
	press(menu, {.raw_any = true, .down = true});
	press(menu, {.raw_any = true, .down = true});
	check(
		menu.consume({.raw_any = true, .confirm = true}) ==
			th03::app::NewClassicPracticeStatus::start_fade &&
		finish_start_fade(menu) ==
			th03::app::NewClassicPracticeStatus::started &&
		menu.practice_active(),
		"main Start completes a customized Practice after black-out"
	);

}

void test_exact_patch_surface()
{
	auto assets = patch_assets();
	if(!assets) {
		return;
	}
	RetainedSelection base{};
	th03::app::NewClassicPracticeMenu menu(selected(), assets);
	check(menu.has_patch_assets(), "Practice exact renderer owns patch assets");
	release(menu);
	check(
		menu.compose_over(base.surface()).pixels.data() == base.surface().pixels.data(),
		"Practice overlay remains hidden until the retained curves frame publishes"
	);
	menu.mark_background_published();
	auto main = menu.compose_over(base.surface());
	check(
		pixel(main, 288U, 32U) == th03::present::ComposedPixel{
			th03::present::ComposedPixelLayer::graphics, 10U,
		},
		"VS SETUP is centered at source y=32 in header color 10"
	);
	check(
		pixel(main, 224U, 96U).value == 14U &&
		pixel(main, 240U, 96U).value == 14U &&
		pixel(main, 328U, 96U).value == 14U,
		"selected Mode cursor, label, and value use exact source geometry/color"
	);
	check(
		pixel(main, 240U, 112U).value == 13U &&
		pixel(main, 328U, 112U).value == 11U,
		"fixed Stage keeps the normal label and gray disabled value"
	);
	check(
		pixel(main, 0U, 0U) == th03::present::ComposedPixel{
			th03::present::ComposedPixelLayer::graphics, 3U,
		},
		"MNUFONT preserves retained selector graphics outside its glyph masks"
	);
	check(
		main.graphics_palette.colors[14U] == th03::assets::Rgb4{2U, 14U, 15U} &&
		main.graphics_palette.colors[11U] == th03::assets::Rgb4{8U, 8U, 8U} &&
		main.palette_tone == 100 && !main.host_overlay &&
		main.shown_page == th03::present::Page::one,
		"Practice applies its two palette overrides and publishes the hidden page"
	);
	th03::app::NewClassicPracticeMenu retained_page_menu(selected(), assets);
	release(retained_page_menu);
	retained_page_menu.mark_background_published();
	check(
		retained_page_menu.compose_over(base.surface(), true).shown_page ==
			base.surface().shown_page,
		"an already-flipped retained selector page remains the published Practice page"
	);

	for(int row = 0; row < 3; ++row) {
		press(menu, {.raw_any = true, .down = true});
	}
	press(menu, {.raw_any = true, .confirm = true});
	auto advanced = menu.compose_over(base.surface());
	check(
		pixel(advanced, 252U, 32U).value == 10U &&
		pixel(advanced, 96U, 64U).value == 14U &&
		pixel(advanced, 112U, 64U).value == 14U &&
		pixel(advanced, 336U, 64U).value == 14U,
		"Advanced header and selected CPU Timer row match patch coordinates"
	);
	check(
		pixel(advanced, 336U, 80U).value == 15U &&
		pixel(advanced, 348U, 80U).value == 15U &&
		pixel(advanced, 356U, 80U).value == 15U,
		"Q4 values retain Replay Patch fixed numeric-cell spacing"
	);
	check(
		pixel(advanced, 336U, 176U).value == 15U &&
		advanced.shown_page == th03::present::Page::zero,
		"Boss Rank includes its source value/raw suffix and pages alternate"
	);

	th03::app::NewClassicPracticeMenu vs_menu(selected(1, 5), assets);
	release(vs_menu);
	vs_menu.mark_background_published();
	press(vs_menu, {.raw_any = true, .right = true});
	for(int row = 0; row < 3; ++row) {
		press(vs_menu, {.raw_any = true, .down = true});
	}
	press(vs_menu, {.raw_any = true, .confirm = true});
	auto vs_advanced = vs_menu.compose_over(base.surface());
	check(
		pixel(vs_advanced, 336U, 208U).value == 11U &&
		pixel(vs_advanced, 336U, 224U).value == 11U,
		"VS-default Stock and Extends values render disabled gray"
	);
}

void test_localized_patch_surface()
{
	auto base_assets = patch_assets();
	check(static_cast<bool>(base_assets), "Practice localization font decodes");
	if(!base_assets) {
		return;
	}
	auto localized_assets =
		std::make_shared<th03::app::NewClassicReplayPatchAssets>(*base_assets);
	localized_assets->custom_messages =
		std::make_shared<const th03::app::NewClassicMessageCatalog>(
			th03::app::detail::new_classic_width_test_messages()
		);
	th03::app::NewClassicPracticeMenu base(selected(), base_assets);
	th03::app::NewClassicPracticeMenu localized(selected(), localized_assets);
	base.mark_background_published();
	localized.mark_background_published();
	RetainedSelection retained{};
	const auto base_surface = base.compose_over(retained.surface(), true);
	const auto localized_surface = localized.compose_over(retained.surface(), true);
	check(
		!std::ranges::equal(base_surface.pixels, localized_surface.pixels),
		"Practice pseudo-locale changes catalog-owned semantic pixels"
	);
	check(
		base_surface.shown_page == localized_surface.shown_page &&
		base_surface.palette_tone == localized_surface.palette_tone &&
		base_surface.graphics_palette == localized_surface.graphics_palette,
		"Practice localization preserves retained page and palette ownership"
	);
}

void test_input_priority_and_release_lock()
{
	th03::app::NewClassicPracticeMenu menu(selected(1, 5));
	release(menu);
	check(
		menu.consume({.raw_any = true, .up = true, .cancel = true}) ==
			th03::app::NewClassicPracticeStatus::running,
		"simultaneous Up and Cancel follows source Up-first priority"
	);
	release(menu);
	check(
		menu.consume({.raw_any = true, .confirm = true}) ==
			th03::app::NewClassicPracticeStatus::start_fade,
		"Up from Mode wrapped to Start before the lower-priority Cancel"
	);

	th03::app::NewClassicPracticeMenu held(selected(1, 5));
	release(held);
	static_cast<void>(held.consume({.raw_any = true}));
	static_cast<void>(held.consume({.raw_any = true, .right = true}));
	check(
		held.config().preset == 1U,
		"an arbitrary held interface bit locks a later mapped press"
	);
	release(held);
	press(held, {.raw_any = true, .right = true});
	check(held.config().preset == 0U, "release re-arms Practice input");

	th03::app::NewClassicPracticeMenu directions(selected(1, 5));
	release(directions);
	for(int row = 0; row < 3; ++row) {
		press(directions, {.raw_any = true, .down = true});
	}
	press(directions, {.raw_any = true, .confirm = true});
	press(directions, {.raw_any = true, .down = true});
	const auto before = directions.config().round_speed;
	static_cast<void>(directions.consume({
		.raw_any = true, .left = true, .right = true,
	}));
	check(
		directions.config().round_speed == before,
		"simultaneous Left/Right follows source Left-first priority"
	);
}

void test_exact_exit_transitions()
{
	auto assets = patch_assets();
	if(!assets) {
		return;
	}
	RetainedSelection base{};
	th03::app::NewClassicPracticeMenu cancel(selected(), assets);
	release(cancel);
	cancel.mark_background_published();
	check(
		cancel.consume({.raw_any = true, .cancel = true}) ==
			th03::app::NewClassicPracticeStatus::cancel_frame &&
		cancel.consume_surface_changed(),
		"Practice Esc enters the retained extra-frame phase"
	);
	const auto restored = cancel.compose_over(base.surface(), true);
	check(
		pixel(restored, 224U, 96U).value == 14U &&
		restored.graphics_palette.colors[14U] ==
			th03::assets::Rgb4{2U, 14U, 15U} &&
		restored.graphics_palette.colors[11U] ==
			th03::assets::Rgb4{8U, 8U, 8U},
		"the input-sampled Esc frame keeps the visible Practice menu and palette"
	);
	cancel.mark_background_published();
	check(
		cancel.status() ==
			th03::app::NewClassicPracticeStatus::cancel_wait_release &&
		cancel.compose_over(base.surface(), true).pixels.data() ==
			base.surface().pixels.data() &&
		cancel.compose_over(base.surface(), true).graphics_palette.colors[14U] ==
			base.surface().graphics_palette.colors[14U] &&
		cancel.compose_over(base.surface(), true).graphics_palette.colors[11U] ==
			base.surface().graphics_palette.colors[11U],
		"the extra curves-only publication removes Practice glyphs and restores TLSL colors"
	);
	check(
		cancel.consume({}) ==
			th03::app::NewClassicPracticeStatus::cancel_wait_release &&
		cancel.consume({}) == th03::app::NewClassicPracticeStatus::canceled,
		"Esc waits one mandatory release-poll VSync before returning"
	);

	th03::app::NewClassicPracticeMenu start(selected(), assets);
	release(start);
	start.mark_background_published();
	for(std::size_t row = 0U; row < 5U; ++row) {
		press(start, {.raw_any = true, .down = true});
	}
	check(
		start.consume({.raw_any = true, .confirm = true}) ==
			th03::app::NewClassicPracticeStatus::start_fade,
		"Start enters palette_black_out(1) before becoming ready"
	);
	bool exact_fade = true;
	for(std::uint8_t call = 0U; call < 19U; ++call) {
		const auto status = start.consume({});
		const auto surface = start.compose_over(base.surface(), true);
		const auto expected_tone = static_cast<std::int16_t>(
			call == 0U ? 100 : call <= 17U ? 100 - ((call - 1U) * 6U) : 0
		);
		exact_fade = exact_fade && surface.palette_tone == expected_tone &&
			start.consume_surface_changed() == (call != 0U) &&
			status == (call == 18U ?
				th03::app::NewClassicPracticeStatus::started :
				th03::app::NewClassicPracticeStatus::start_fade);
	}
	check(
		exact_fade,
		"Start black-out has one alignment, 17 tone writes, and final tone 0"
	);
}

void test_exact_vs_default()
{
	th03::app::NewClassicPracticeMenu menu(selected(1, 5));
	release(menu);
	press(menu, {.raw_any = true, .right = true});
	check(
		menu.config().preset == 0U && menu.config().stage == 0U &&
		menu.config().stock == 0xFFU && !menu.practice_active(),
		"Mode VS resets to the patch's exact ordinary-VS configuration"
	);
	for(int row = 0; row < 5; ++row) {
		press(menu, {.raw_any = true, .down = true});
	}
	check(
		menu.consume({.raw_any = true, .confirm = true}) ==
			th03::app::NewClassicPracticeStatus::start_fade &&
		finish_start_fade(menu) ==
			th03::app::NewClassicPracticeStatus::started &&
		!menu.practice_active(),
		"exact VS Start stays distinguishable after its exact black-out"
	);

	th03::app::NewClassicPracticeMenu chiyuri(selected(1, 15));
	th03::app::NewClassicPracticeMenu yumemi(selected(1, 17));
	check(
		chiyuri.config().stage == 7U && yumemi.config().stage == 8U,
		"Chiyuri and Yumemi force their source Story stages"
	);
}

void test_checkpoint_and_recording()
{
	th03::app::NewClassicPracticeMenu menu(selected());
	const auto config = menu.config();
	auto prepared = th03::app::prepare_new_classic_practice_init(
		selected(), config, content()
	);
	check(prepared.has_value(), "Practice first-round checkpoint prepares");
	if(!prepared) {
		return;
	}
	check(
		prepared->practice_rules && prepared->practice_rules->uses_stock &&
		prepared->round_start_snapshot &&
		prepared->round_start_snapshot->state.round_id == config.round &&
		prepared->round_start_snapshot->state.gauge_attack_level ==
			std::array<std::uint8_t, 2U>{7U, 7U} &&
		prepared->compact_runtime_snapshot &&
		prepared->compact_runtime_snapshot->players[0U].gauge_avail == 1024U,
		"checkpoint applies source stock, round, spell, and gauge fields"
	);
	auto game = th03::Game::create(
		*prepared, content(),
		th03::GameOptions{.input_protocol = th03::InputProtocol::recorded_replay}
	);
	check(game.has_value(), "the shared deterministic Game accepts Practice");
	if(!game) {
		return;
	}

	th03::app::NewClassicReplayRecorder recorder(0x5D0CU);
	check(
		recorder.configure_practice(config).has_value() &&
		recorder.round_started(game->round_start_observation()).has_value() &&
		recorder.sample(th03::replay::InputPhase::gameplay, {}).has_value(),
		"live recorder accepts the exact Practice anchor"
	);
	auto terminal = game->canonical_state();
	terminal.route = 0U;
	terminal.winner = 0;
	check(
		recorder.round_completed(terminal).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::main).has_value() &&
		recorder.finish(
			th03::formats::ReplayEndReason::menu_return, *prepared
		).has_value(),
		"Practice terminal finalizes through V14"
	);
	auto document = recorder.take_completed();
	check(
		document && document->practice && document->metadata->practice &&
		document->metadata->flags == 0x0007U &&
		document->checkpoints[0U].stage == config.stage &&
		document->expected_round_splits[0U].story_stage == config.stage &&
		th03::formats::write_v14_replay(*document).has_value(),
		"Practice V14 retains flag, header, checkpoint stage, and split identity"
	);

	auto bad = config;
	++bad.initial_cpu_safety_frames;
	check(
		!th03::app::prepare_new_classic_practice_init(
			selected(), bad, content()
		),
		"Practice checkpoint rejects a safety value outside its selected policy"
	);
}

} // namespace

int main()
{
	test_source_menu();
	test_exact_patch_surface();
	test_localized_patch_surface();
	test_input_priority_and_release_lock();
	test_exact_exit_transitions();
	test_exact_vs_default();
	test_checkpoint_and_recording();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
