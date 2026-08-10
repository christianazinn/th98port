#ifndef TH03_APP_DEFAULT_BINDINGS_HPP
#define TH03_APP_DEFAULT_BINDINGS_HPP

#include "th03/platform/sdl_input.hpp"

namespace th03::app::detail {

[[nodiscard]] platform::SdlInputBindings classic_direct_vs_bindings() noexcept;

} // namespace th03::app::detail

#endif
