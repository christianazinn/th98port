#ifndef TH03_APP_EXPERIENCE_MENU_HPP
#define TH03_APP_EXPERIENCE_MENU_HPP

#include <cstdint>
#include <memory>

#include "th03/present/source_surface.hpp"

namespace th03::app {

// Host-owned product identities. This value stays outside the deterministic
// gameplay core; adapters may only translate it into explicit presentation,
// persistence, or recorded-input policies.
enum class ExperienceProfile : std::uint8_t {
	classic,
	new_classic,
};

enum class ExperienceMenuChoice : std::uint8_t {
	classic,
	new_classic,
	exit,
};

struct ExperienceMenuInput final {
	bool up = false;
	bool down = false;
	bool confirm = false;
	bool cancel = false;
	bool app_quit_requested = false;
};

enum class ExperienceMenuStatus : std::uint8_t {
	running,
	experience_selected,
	quit_requested,
};

struct ExperienceMenuResult final {
	bool surface_changed = false;
	ExperienceMenuStatus status = ExperienceMenuStatus::running;
	ExperienceMenuChoice choice = ExperienceMenuChoice::classic;
};

// Dependency-free root menu for the native product shell. It deliberately
// owns no game resources: both experiences share the authenticated owners
// selected after this boundary.
class ExperienceMenu final {
public:
	ExperienceMenu();
	~ExperienceMenu();
	ExperienceMenu(const ExperienceMenu &) = delete;
	ExperienceMenu &operator=(const ExperienceMenu &) = delete;
	ExperienceMenu(ExperienceMenu &&) noexcept;
	ExperienceMenu &operator=(ExperienceMenu &&) = delete;

	[[nodiscard]] ExperienceMenuResult consume(
		ExperienceMenuInput input
	) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] ExperienceMenuChoice choice() const noexcept;
	[[nodiscard]] ExperienceMenuStatus status() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

[[nodiscard]] constexpr ExperienceProfile experience_profile(
	ExperienceMenuChoice choice
) noexcept
{
	return (choice == ExperienceMenuChoice::new_classic) ?
		ExperienceProfile::new_classic : ExperienceProfile::classic;
}

} // namespace th03::app

#endif
