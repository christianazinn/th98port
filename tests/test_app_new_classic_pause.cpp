#include <array>
#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

#include "th03/app/new_classic_assets.hpp"
#include "th03/app/new_classic_pause.hpp"

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

void release(th03::app::NewClassicPauseMenu &menu)
{
	expect(menu.consume({}) == th03::app::NewClassicPauseStatus::running,
		"release boundary remains in the pause menu");
}

[[nodiscard]] std::shared_ptr<th03::app::NewClassicReplayPatchAssets>
synthetic_patch_assets()
{
	auto assets = std::make_shared<
		th03::app::NewClassicReplayPatchAssets
	>();
	for(const auto tile : std::array<std::size_t, 6U>{0U, 5U, 10U, 15U, 28U, 39U}) {
		assets->pause_gaiji[tile][0U] = 0x80U;
	}
	return assets;
}

void test_navigation_and_actions()
{
	using namespace th03::app;
	NewClassicPauseMenu menu(true);
	expect(menu.choice() == NewClassicPauseChoice::resume,
		"pause menu starts on Resume");
	expect(menu.consume({.down = true, .raw_any = true}) ==
		NewClassicPauseStatus::running,
		"opening held input is release-locked");
	release(menu);
	static_cast<void>(menu.consume({.down = true, .raw_any = true}));
	expect(menu.choice() == NewClassicPauseChoice::restart,
		"Down selects Restart");
	release(menu);
	static_cast<void>(menu.consume({.down = true, .raw_any = true}));
	expect(menu.choice() == NewClassicPauseChoice::save_exit,
		"second Down selects save-and-exit when recording");
	release(menu);
	expect(menu.consume({.confirm = true, .raw_any = true}) ==
		NewClassicPauseStatus::save_exit,
		"confirm returns the save-and-exit action");

	NewClassicPauseMenu direct(true);
	expect(direct.consume({.restart = true}) ==
		NewClassicPauseStatus::running,
		"physical R cannot bypass the opening interface-word release sample");
	expect(direct.consume({.restart = true}) ==
		NewClassicPauseStatus::restart,
		"held physical R is polled separately after the interface-word release");
	NewClassicPauseMenu quit(true);
	expect(quit.consume({.quit = true, .raw_any = true}) ==
		NewClassicPauseStatus::running,
		"held Q remains behind the opening release lock");
	release(quit);
	expect(quit.consume({.quit = true, .raw_any = true}) ==
		NewClassicPauseStatus::discard_exit,
		"Q discards only after release and repress");
}

void test_disabled_save_and_resume_release()
{
	using namespace th03::app;
	NewClassicPauseMenu menu(false);
	release(menu);
	static_cast<void>(menu.consume({.up = true, .raw_any = true}));
	expect(menu.choice() == NewClassicPauseChoice::discard_exit,
		"Up wraps past disabled Save to Exit Without Saving");
	release(menu);
	static_cast<void>(menu.consume({.down = true, .raw_any = true}));
	expect(menu.choice() == NewClassicPauseChoice::resume,
		"Down wraps from discard exit to Resume");
	release(menu);
	expect(menu.consume({.cancel = true, .raw_any = true}) ==
		NewClassicPauseStatus::running,
		"Escape selects Resume but waits for release");
	expect(menu.consume({.cancel = true, .raw_any = true}) ==
		NewClassicPauseStatus::running,
		"held Escape does not close the pause menu");
	expect(menu.consume({}) == NewClassicPauseStatus::resume,
		"released Escape completes Resume");

	NewClassicPauseMenu rejected(false);
	release(rejected);
	NewClassicPauseTestAccess::select(
		rejected, NewClassicPauseChoice::save_exit
	);
	expect(rejected.choice() == NewClassicPauseChoice::save_exit,
		"disabled Save can still be reached through the donor's rejection path");
	expect(
		rejected.consume({.confirm = true, .raw_any = true}) ==
			NewClassicPauseStatus::running &&
		rejected.choice() == NewClassicPauseChoice::discard_exit &&
		rejected.take_rejected_save_effect() &&
		!rejected.take_rejected_save_effect(),
		"disabled Save moves to Exit and emits exactly one forced SE 21 request"
	);
}

void test_overlay_preserves_base_and_palette()
{
	using namespace th03::app;
	std::vector<th03::present::ComposedPixel> base_pixels(
		th03::present::graphics_page_pixels,
		{th03::present::ComposedPixelLayer::graphics, 3U}
	);
	th03::present::SourceSurface original{
		.pixels = std::span<const th03::present::ComposedPixel,
			th03::present::graphics_page_pixels>(
			base_pixels.data(), base_pixels.size()
		),
	};
	original.shown_page = th03::present::Page::one;
	original.raster_mode = th03::present::RasterMode::zoomed_200_lower;
	original.effective_scroll_y = 184U;
	original.palette_tone = 42;
	original.oracle_hashes = th03::present::SourceSurfaceOracleHashes{11U, 22U};
	NewClassicPauseMenu menu(true, synthetic_patch_assets());
	auto before_release = menu.compose_over(original);
	expect(before_release.pixels.data() == original.pixels.data(),
		"pause stays invisible during its opening release lock");
	release(menu);
	auto overlay = menu.compose_over(original);
	expect(overlay.graphics_palette == original.graphics_palette,
		"pause overlay preserves the complete gameplay palette");
	expect(overlay.pixels[0U] == original.pixels[0U],
		"pause overlay leaves pixels outside its modal untouched");
	const auto modal_top_left =
		(128U * th03::present::graphics_width) + 192U;
	expect(
		overlay.pixels[modal_top_left] == th03::present::ComposedPixel{
			th03::present::ComposedPixelLayer::text, 0U
		},
		"pause overlay starts at exact TRAM cell (24,8) in true black"
	);
	const auto below_modal =
		(240U * th03::present::graphics_width) + 192U;
	expect(overlay.pixels[below_modal] == original.pixels[below_modal],
		"pause overlay ends after exact 32x7-cell geometry");
	const auto title_bit =
		(144U * th03::present::graphics_width) + 280U;
	expect(
		overlay.pixels[title_bit] == th03::present::ComposedPixel{
			th03::present::ComposedPixelLayer::graphics, 10U
		},
		"PAUSED gaiji exposes the cyan graphics backing at cell (35,9)"
	);
	const auto cursor_bit =
		(160U * th03::present::graphics_width) + 208U;
	const auto resume_bit =
		(160U * th03::present::graphics_width) + 224U;
	const auto second_choice_bit =
		(176U * th03::present::graphics_width) + 224U;
	expect(
		overlay.pixels[cursor_bit].layer ==
			th03::present::ComposedPixelLayer::graphics &&
		overlay.pixels[cursor_bit].value == 12U &&
		overlay.pixels[resume_bit].value == 12U,
		"selected cursor and Resume gaiji expose bright-yellow backing"
	);
	expect(
		overlay.pixels[second_choice_bit].layer ==
			th03::present::ComposedPixelLayer::graphics &&
		overlay.pixels[second_choice_bit].value == 15U,
		"unselected Restart gaiji exposes white backing"
	);
	expect(
		overlay.host_overlay == original.host_overlay &&
		overlay.shown_page == original.shown_page &&
		overlay.raster_mode == original.raster_mode &&
		overlay.effective_scroll_y == original.effective_scroll_y &&
		overlay.palette_tone == original.palette_tone &&
		!overlay.oracle_hashes.has_value(),
		"pause overlay retains guest selectors but drops stale frozen fingerprints"
	);

	NewClassicPauseMenu unavailable(true);
	release(unavailable);
	expect(unavailable.compose_over(original).pixels.data() == original.pixels.data(),
		"missing authenticated PAUSE.GFT never substitutes a host-font placeholder");

	NewClassicPauseMenu disabled(false, synthetic_patch_assets());
	release(disabled);
	const auto disabled_overlay = disabled.compose_over(original);
	const auto save_bit =
		(192U * th03::present::graphics_width) + 224U;
	expect(
		disabled_overlay.pixels[save_bit].layer ==
			th03::present::ComposedPixelLayer::graphics &&
		disabled_overlay.pixels[save_bit].value == 9U,
		"disabled Save gaiji exposes Replay Patch blue backing"
	);

	NewClassicPauseMenu network(
		false, synthetic_patch_assets(),
		NewClassicPausePresentation::netplay_wait,
		[] {
			auto text = std::make_shared<NewClassicPauseNetplayText>();
			text->forfeit[NewClassicPauseNetplayText::choice_text_left] = 1U;
			text->resume_countdown[0U][80U] = 1U;
			return text;
		}()
	);
	release(network);
	const auto network_overlay = network.compose_over(original);
	expect(
		network_overlay.pixels[resume_bit].layer ==
			th03::present::ComposedPixelLayer::graphics &&
		network_overlay.pixels[resume_bit].value == 12U,
		"network Pause uses the authenticated selected Resume gaiji"
	);
	expect(
		network_overlay.pixels[second_choice_bit] ==
			th03::present::ComposedPixel{
				th03::present::ComposedPixelLayer::graphics, 15U
			},
		"network Pause left-aligns Forfeit with Resume at exact x=224"
	);
	const auto unavailable_third_row =
		(192U * th03::present::graphics_width) + 224U;
	expect(
		network_overlay.pixels[unavailable_third_row] ==
			th03::present::ComposedPixel{
				th03::present::ComposedPixelLayer::text, 0U
			},
		"network Pause does not advertise Restart or Save"
	);
	static_cast<void>(network.consume({.down = true, .raw_any = true}));
	const auto selected_forfeit = network.compose_over(original);
	const auto second_cursor_bit =
		(176U * th03::present::graphics_width) + 208U;
	expect(network.choice() == NewClassicPauseChoice::discard_exit &&
		selected_forfeit.pixels[second_cursor_bit].value == 12U &&
		selected_forfeit.pixels[second_choice_bit].value == 12U,
		"network Pause navigation selects the visible Forfeit action");
	release(network);
	expect(network.consume({.confirm = true, .raw_any = true}) ==
		NewClassicPauseStatus::discard_exit,
		"network Pause confirms Forfeit without exposing local Save or Restart");

	NewClassicPauseMenu countdown(
		false, synthetic_patch_assets(),
		NewClassicPausePresentation::netplay_wait,
		[] {
			auto text = std::make_shared<NewClassicPauseNetplayText>();
			text->resume_countdown[0U][80U] = 1U;
			return text;
		}()
	);
	release(countdown);
	expect(countdown.consume({.cancel = true, .raw_any = true}) ==
		NewClassicPauseStatus::running && countdown.consume({}) ==
		NewClassicPauseStatus::resume,
		"local Resume reaches its terminal state before peer synchronization");
	countdown.set_resume_countdown(std::uint8_t{3U});
	const auto countdown_overlay = countdown.compose_over(original);
	const auto countdown_bit =
		(192U * th03::present::graphics_width) + 272U;
	expect(countdown.resume_countdown() == std::uint8_t{3U} &&
		countdown_overlay.pixels[countdown_bit] ==
			th03::present::ComposedPixel{
				th03::present::ComposedPixelLayer::graphics, 10U
			},
		"peer-ready countdown reopens the retained Pause overlay in cyan"
	);
	expect(new_classic_netplay_resume_countdown_remaining(0U) ==
			std::uint8_t{3U} &&
		new_classic_netplay_resume_countdown_remaining(999'999'999U) ==
			std::uint8_t{3U} &&
		new_classic_netplay_resume_countdown_remaining(1'000'000'000U) ==
			std::uint8_t{2U} &&
		new_classic_netplay_resume_countdown_remaining(2'999'999'999U) ==
			std::uint8_t{1U} &&
		!new_classic_netplay_resume_countdown_remaining(3'000'000'000U),
		"network Pause countdown has exact three-second boundaries");
}

void test_input_projection()
{
	using namespace th03;
	const auto projected = app::project_new_classic_pause_input(
		CoreInput{
			.system = static_cast<std::uint16_t>(
				0x0001U | 0x0002U | 0x2020U | 0x1000U | 0x4000U
			),
		},
		true,
		true
	);
	expect(projected.up && projected.down,
		"projection preserves both vertical direction bits");
	expect(projected.confirm && projected.cancel && projected.quit,
		"projection maps Shot/OK, Escape, and Q");
	expect(projected.restart && projected.application_quit,
		"projection carries host-only restart and application-quit requests");
	expect(projected.raw_any,
		"projection retains the complete interface-word presence contract");
	expect(
		app::project_new_classic_pause_input(CoreInput{.p1 = 0x0001U}).raw_any &&
		app::project_new_classic_pause_input(CoreInput{.p2 = 0x0002U}).raw_any,
		"either player's interface word preserves the pause release lock"
	);
	expect(!app::project_new_classic_pause_input({}).raw_any,
		"empty interface input projects to an exact release boundary");
	const auto netplay = app::project_new_classic_netplay_pause_input({
		.p1 = 0x0023U,
	});
	expect(netplay.up && netplay.down && netplay.confirm && netplay.raw_any,
		"netplay Pause projects ordinary local Story movement and Shot");
	const auto netplay_system = app::project_new_classic_netplay_pause_input({
		.system = 0x5000U,
	});
	expect(netplay_system.cancel && netplay_system.quit,
		"netplay Pause retains source-global Esc and Q");
}

} // namespace

int main()
{
	test_navigation_and_actions();
	test_disabled_save_and_resume_release();
	test_overlay_preserves_base_and_palette();
	test_input_projection();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
