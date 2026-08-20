#ifndef TH03_PLATFORM_SDL_UPDATE_PROCESS_HPP
#define TH03_PLATFORM_SDL_UPDATE_PROCESS_HPP

#include "th03/app/update_process_host.hpp"

namespace th03::platform {

inline constexpr char update_activation_state_environment[] =
	"TH03_UPDATE_STATE";
inline constexpr char update_trial_token_environment[] =
	"TH03_UPDATE_TRIAL_TOKEN";

[[nodiscard]] th03::app::UpdateProcessExecutor
sdl_update_process_executor() noexcept;

} // namespace th03::platform

#endif
