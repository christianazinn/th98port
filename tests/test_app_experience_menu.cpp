#include "th03/app/experience_menu.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>

namespace {

using th03::app::ExperienceMenu;
using th03::app::ExperienceMenuChoice;
using th03::app::ExperienceMenuStatus;
using th03::app::ExperienceProfile;

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

void release(ExperienceMenu &menu)
{
	(void)menu.consume({});
}

void test_surface_and_profiles()
{
	ExperienceMenu menu;
	const auto surface = menu.surface();
	expect(
		surface.pixels.size() == th03::present::graphics_page_pixels,
		"the host menu publishes one complete 640x400 source surface"
	);
	expect(
		std::any_of(
			surface.pixels.begin(), surface.pixels.end(),
			[](const auto &pixel) { return pixel.value == 14U; }
		),
		"the initial Classic choice has visible selected-label ink"
	);
	expect(
		th03::app::experience_profile(ExperienceMenuChoice::classic) ==
			ExperienceProfile::classic &&
		th03::app::experience_profile(ExperienceMenuChoice::new_classic) ==
			ExperienceProfile::new_classic,
		"visible choices map to distinct host experience identities"
	);
}

void test_release_lock_navigation_and_selection()
{
	ExperienceMenu menu;
	const auto held = menu.consume({.down = true});
	expect(
		held.choice == ExperienceMenuChoice::classic && !held.surface_changed,
		"initially held input cannot skip the default choice"
	);
	release(menu);
	const auto moved = menu.consume({.down = true});
	expect(
		moved.choice == ExperienceMenuChoice::new_classic &&
		moved.surface_changed,
		"one released Down edge selects New Classic and redraws"
	);
	const auto repeated = menu.consume({.down = true});
	expect(
		repeated.choice == ExperienceMenuChoice::new_classic &&
		!repeated.surface_changed,
		"held Down cannot repeat before a release"
	);
	release(menu);
	const auto selected = menu.consume({.confirm = true});
	expect(
		selected.status == ExperienceMenuStatus::experience_selected &&
		selected.choice == ExperienceMenuChoice::new_classic,
		"confirm returns the selected New Classic profile to the shell"
	);
}

void test_wrap_exit_cancel_and_app_quit()
{
	ExperienceMenu wrap;
	release(wrap);
	const auto up = wrap.consume({.up = true});
	expect(up.choice == ExperienceMenuChoice::exit,
		"Up wraps from Classic to the explicit Exit row");
	release(wrap);
	const auto exit = wrap.consume({.confirm = true});
	expect(exit.status == ExperienceMenuStatus::quit_requested,
		"confirming Exit terminates the host shell");

	ExperienceMenu cancel;
	release(cancel);
	const auto cancelled = cancel.consume({.cancel = true});
	expect(
		cancelled.status == ExperienceMenuStatus::quit_requested &&
		cancelled.choice == ExperienceMenuChoice::exit,
		"Cancel is a root-level shortcut to Exit"
	);

	ExperienceMenu app_quit;
	const auto closed = app_quit.consume({.app_quit_requested = true});
	expect(
		closed.status == ExperienceMenuStatus::quit_requested &&
		!closed.surface_changed,
		"window close terminates without inventing a menu redraw"
	);
}

} // namespace

int main()
{
	test_surface_and_profiles();
	test_release_lock_navigation_and_selection();
	test_wrap_exit_cancel_and_app_quit();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return (failures == 0U) ? 0 : 1;
}
