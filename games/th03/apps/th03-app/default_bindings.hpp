#ifndef TH03_APP_DEFAULT_BINDINGS_HPP
#define TH03_APP_DEFAULT_BINDINGS_HPP

#include "th03/app/new_classic/input/new_classic_controller.hpp"
#include "th03/app/new_classic/input/new_classic_joy_config_store.hpp"
#include "th03/app/new_classic/input/new_classic_key_config_store.hpp"
#include "th03/platform/sdl_input.hpp"

namespace th03::app::detail {

[[nodiscard]] platform::SdlInputBindings classic_direct_vs_bindings() noexcept;
[[nodiscard]] platform::SdlInputBindings
new_classic_netplay_story_bindings(bool autofire_enabled) noexcept;
[[nodiscard]] platform::SdlInputBindings new_classic_netplay_story_bindings(
	const NewClassicKeyConfig &key_config,
	const NewClassicJoyConfig &joy_config,
	const NewClassicDeviceAssignment &device_assignment,
	bool autofire_enabled
) noexcept;
[[nodiscard]] NewClassicKeyConfig new_classic_default_key_config() noexcept;
[[nodiscard]] NewClassicJoyConfig new_classic_default_joy_config() noexcept;
[[nodiscard]] platform::SdlInputBindings new_classic_bindings(
	const NewClassicKeyConfig &key_config,
	const NewClassicJoyConfig &joy_config
) noexcept;
[[nodiscard]] platform::SdlInputBindings new_classic_bindings(
	const NewClassicKeyConfig &key_config,
	const NewClassicJoyConfig &joy_config,
	const NewClassicDeviceAssignment &device_assignment
) noexcept;
[[nodiscard]] platform::SdlInputBindings new_classic_bindings(
	const NewClassicKeyConfig &key_config
) noexcept;

} // namespace th03::app::detail

#endif
