#ifndef TH01_PLATFORM_SDL_RUNTIME_INTERNAL_HPP
#define TH01_PLATFORM_SDL_RUNTIME_INTERNAL_HPP

namespace th01::platform::detail {

enum class SdlPlatformRuntimeAcquire {
	acquired,
	already_initialized,
	initialization_failed,
};

// One platform owns SDL initialization, while native input and audio owners
// take subordinate leases so their SDL handles remain valid if their window
// owner is destroyed first. All calls are confined to the SDL main thread.
[[nodiscard]] SdlPlatformRuntimeAcquire acquire_platform_runtime() noexcept;
void release_platform_runtime() noexcept;
[[nodiscard]] bool acquire_platform_runtime_lease() noexcept;
void release_platform_runtime_lease() noexcept;

} // namespace th01::platform::detail

#endif
