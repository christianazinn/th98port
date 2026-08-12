#ifndef TH03_APP_DEFAULT_BINDINGS_HPP
#define TH03_APP_DEFAULT_BINDINGS_HPP

#include "th03/app/new_classic_key_config_store.hpp"
#include "th03/platform/sdl_input.hpp"

namespace th03::app::detail {

[[nodiscard]] platform::SdlInputBindings classic_direct_vs_bindings() noexcept;
[[nodiscard]] NewClassicKeyConfig new_classic_default_key_config() noexcept;
[[nodiscard]] platform::SdlInputBindings new_classic_bindings(
	const NewClassicKeyConfig &config
) noexcept;

} // namespace th03::app::detail

#endif
