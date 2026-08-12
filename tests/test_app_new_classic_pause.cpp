#include <cstddef>
#include <iostream>

#include "th03/app/host_menu_canvas.hpp"
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
	expect(direct.consume({.restart = true, .raw_any = true}) ==
		NewClassicPauseStatus::restart,
		"physical R bypasses the release lock and requests restart");
	NewClassicPauseMenu quit(true);
	expect(quit.consume({.quit = true, .raw_any = true}) ==
		NewClassicPauseStatus::discard_exit,
		"Q bypasses the release lock and discards");
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
}

void test_overlay_preserves_base_and_palette()
{
	using namespace th03::app;
	HostMenuCanvas base;
	base.clear(3U);
	auto original = base.surface();
	NewClassicPauseMenu menu(true);
	auto overlay = menu.compose_over(original);
	expect(overlay.graphics_palette == original.graphics_palette,
		"pause overlay preserves the complete gameplay palette");
	expect(overlay.pixels[0U] == original.pixels[0U],
		"pause overlay leaves pixels outside its modal untouched");
	const auto center = (200U * th03::present::graphics_width) + 320U;
	expect(overlay.pixels[center] != original.pixels[center],
		"pause overlay replaces the modal center over the frozen frame");
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
	expect(!app::project_new_classic_pause_input({}).raw_any,
		"empty interface input projects to an exact release boundary");
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
