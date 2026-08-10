#ifndef TH03_PLATFORM_SDL_RUNTIME_INTERNAL_HPP
#define TH03_PLATFORM_SDL_RUNTIME_INTERNAL_HPP

namespace th03::platform::detail {

enum class SdlPlatformRuntimeAcquire {
	acquired,
	already_initialized,
	initialization_failed,
};

// One platform owns SDL initialization, while native input mappers take
// subordinate leases so their gamepad handles remain valid if their window
// owner is destroyed first. All calls are confined to the SDL main thread.
[[nodiscard]] SdlPlatformRuntimeAcquire acquire_platform_runtime() noexcept;
void release_platform_runtime() noexcept;
[[nodiscard]] bool acquire_native_mapper_runtime() noexcept;
void release_native_mapper_runtime() noexcept;

} // namespace th03::platform::detail

#endif
