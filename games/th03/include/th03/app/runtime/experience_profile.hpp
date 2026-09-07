#ifndef TH03_APP_EXPERIENCE_PROFILE_HPP
#define TH03_APP_EXPERIENCE_PROFILE_HPP

#include <cstdint>

namespace th03::app {

// Host-owned product identities. This value stays outside the deterministic
// gameplay core; adapters may only translate it into explicit presentation,
// persistence, or recorded-input policies.
enum class ExperienceProfile : std::uint8_t {
	classic,
	new_classic,
};

enum class ProductLifecycleSignal : std::uint8_t {
	return_to_menu,
	switch_to_classic,
	application_quit,
};

struct ProductLifecycleTransition final {
	ExperienceProfile next = ExperienceProfile::new_classic;
	bool exit_application = false;
};

// The normal product has one root: New Classic. Its Quit terminates the
// application, while its explicit System action temporarily enters sealed
// Classic. Classic's exact Quit returns to a fresh New Classic title.
[[nodiscard]] constexpr ProductLifecycleTransition product_lifecycle_transition(
	ExperienceProfile current,
	ProductLifecycleSignal signal
) noexcept
{
	if(signal == ProductLifecycleSignal::application_quit) {
		return {.next = current, .exit_application = true};
	}
	if(current == ExperienceProfile::new_classic) {
		return signal == ProductLifecycleSignal::switch_to_classic ?
			ProductLifecycleTransition{.next = ExperienceProfile::classic} :
			ProductLifecycleTransition{
				.next = ExperienceProfile::new_classic,
				.exit_application = true,
			};
	}
	return signal == ProductLifecycleSignal::return_to_menu ?
		ProductLifecycleTransition{.next = ExperienceProfile::new_classic} :
		ProductLifecycleTransition{
			.next = ExperienceProfile::classic,
			.exit_application = true,
		};
}

} // namespace th03::app

#endif
