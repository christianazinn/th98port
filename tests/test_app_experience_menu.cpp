#include "th03/app/experience_menu.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

std::uint64_t surface_hash(const th03::present::SourceSurface &surface)
{
	std::uint64_t hash = 14695981039346656037ULL;
	for(const auto &pixel : surface.pixels) {
		hash ^= static_cast<std::uint8_t>(pixel.layer);
		hash *= 1099511628211ULL;
		hash ^= pixel.value;
		hash *= 1099511628211ULL;
	}
	return hash;
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
	const auto at = [&surface](std::size_t x, std::size_t y) {
		return surface.pixels[(y * th03::present::graphics_width) + x].value;
	};
	expect(
		at(242U, 68U) == 3U && at(242U, 76U) == 15U,
		"the shell heading renders the exact mixed-case PoDD Port title"
	);
	expect(
		surface_hash(surface) == 3737366376350595403ULL,
		"the complete initial shell publication locks PoDD Port and removes the superseded heading"
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
