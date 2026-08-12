#include "th03/platform/sdl_platform.hpp"

#include "sdl_platform_internal.hpp"
#include "sdl_runtime_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <SDL3/SDL.h>

namespace th03::platform {
namespace {

constexpr int source_width = static_cast<int>(present::graphics_width);
constexpr int source_height = static_cast<int>(present::graphics_height);
constexpr std::size_t rgba_row_bytes = present::graphics_width * 4U;
constexpr std::uint64_t fnv_offset_basis = 14695981039346656037ULL;
constexpr std::uint64_t fnv_prime = 1099511628211ULL;
constexpr std::uint8_t source_surface_hash_schema = 2U;

bool platform_runtime_active = false;
std::size_t platform_runtime_leases = 0U;

[[nodiscard]] SdlPlatformError error_with_sdl_detail(
	SdlPlatformErrorCode code
) noexcept
{
	SdlPlatformError error{.code = code};
	const char *const detail = SDL_GetError();
	if(detail == nullptr) {
		return error;
	}
	const auto text = std::string_view{detail};
	const auto count = std::min(text.size(), error.sdl_detail.size() - 1U);
	std::copy_n(text.begin(), count, error.sdl_detail.begin());
	error.sdl_detail[count] = '\0';
	return error;
}

[[nodiscard]] SdlPlatformError surface_error(
	SdlPlatformErrorCode code,
	app::HostSurfaceError cause
) noexcept
{
	return SdlPlatformError{
		.code = code,
		.surface_error = cause,
	};
}

[[nodiscard]] bool valid_dimension(std::uint32_t value) noexcept
{
	return value <= static_cast<std::uint32_t>(std::numeric_limits<int>::max());
}

void hash_byte(std::uint64_t &hash, std::uint8_t value) noexcept
{
	hash ^= value;
	hash *= fnv_prime;
}

void hash_u16_le(std::uint64_t &hash, std::uint16_t value) noexcept
{
	hash_byte(hash, static_cast<std::uint8_t>(value & 0x00FFU));
	hash_byte(hash, static_cast<std::uint8_t>(value >> 8U));
}

void hash_palette(
	std::uint64_t &hash,
	const assets::Palette16Rgb4 &palette
) noexcept
{
	for(const auto &color : palette.colors) {
		hash_byte(hash, color.r);
		hash_byte(hash, color.g);
		hash_byte(hash, color.b);
	}
}

} // namespace

struct SdlPlatform::Impl {
	SDL_Window *window = nullptr;
	SDL_Renderer *renderer = nullptr;
	SDL_Texture *texture = nullptr;
	bool runtime_lease = false;
	std::ofstream oracle_log{};
	std::uint64_t oracle_publication = 0U;
	bool oracle_hash_lease = false;

	[[nodiscard]] bool record_oracle(
		const present::SourceSurface &surface,
		SdlPresentationOracleIdentity identity
	)
	{
		if(!oracle_log.is_open()) {
			return true;
		}
		oracle_log << "T3PXH\t2\t" << std::dec << oracle_publication++ << '\t';
		if(identity.logical_tick) {
			oracle_log << *identity.logical_tick;
		} else {
			oracle_log << "na";
		}
		oracle_log << '\t' << identity.coalesced_ticks << '\t'
			<< static_cast<unsigned>(surface.host_overlay) << '\t'
			<< static_cast<unsigned>(surface.reduced_flash_applied) << '\t'
			<< std::hex << std::setfill('0') << std::setw(16)
			<< detail::source_surface_state_hash(surface) << '\t';
		if(surface.oracle_hashes) {
			oracle_log << std::setw(16)
				<< surface.oracle_hashes->raster_display << '\t'
				<< std::setw(16) << surface.oracle_hashes->tram_visible;
		} else {
			oracle_log << "na\tna";
		}
		oracle_log << std::dec << '\t'
			<< static_cast<unsigned>(surface.shown_page) << '\t'
			<< static_cast<unsigned>(surface.raster_mode) << '\t'
			<< surface.effective_scroll_y << '\t'
			<< surface.palette_tone << '\n';
		oracle_log.flush();
		return static_cast<bool>(oracle_log);
	}

	~Impl()
	{
		if(oracle_hash_lease) {
			present::set_source_surface_oracle_hashes_enabled(false);
		}
		if(texture != nullptr) {
			SDL_DestroyTexture(texture);
		}
		if(renderer != nullptr) {
			SDL_DestroyRenderer(renderer);
		}
		if(window != nullptr) {
			SDL_DestroyWindow(window);
		}
		if(runtime_lease) {
			detail::release_platform_runtime();
		}
	}
};

namespace detail {

std::uint64_t source_surface_state_hash(
	const present::SourceSurface &surface
) noexcept
{
	std::uint64_t hash = fnv_offset_basis;
	hash_byte(hash, source_surface_hash_schema);
	hash_byte(hash, static_cast<std::uint8_t>(surface.shown_page));
	hash_byte(hash, static_cast<std::uint8_t>(surface.raster_mode));
	hash_u16_le(hash, surface.effective_scroll_y);
	hash_u16_le(hash, static_cast<std::uint16_t>(surface.palette_tone));
	hash_palette(hash, surface.graphics_palette);
	hash_palette(hash, surface.reduced_flash_palette);
	hash_byte(hash, static_cast<std::uint8_t>(surface.host_overlay));
	hash_byte(hash, static_cast<std::uint8_t>(surface.reduced_flash_applied));
	for(const auto pixel : surface.pixels) {
		hash_byte(hash, static_cast<std::uint8_t>(pixel.layer));
		hash_byte(hash, pixel.value);
	}
	return hash;
}

SdlPlatformRuntimeAcquire acquire_platform_runtime() noexcept
{
	if(platform_runtime_active || platform_runtime_leases != 0U ||
		SDL_WasInit(0U) != 0U) {
		return SdlPlatformRuntimeAcquire::already_initialized;
	}
	if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		return SdlPlatformRuntimeAcquire::initialization_failed;
	}
	platform_runtime_active = true;
	platform_runtime_leases = 1U;
	return SdlPlatformRuntimeAcquire::acquired;
}

void release_platform_runtime() noexcept
{
	platform_runtime_active = false;
	if(platform_runtime_leases != 0U) {
		--platform_runtime_leases;
	}
	if(platform_runtime_leases == 0U) {
		SDL_Quit();
	}
}

bool acquire_platform_runtime_lease() noexcept
{
	if(!platform_runtime_active || platform_runtime_leases == 0U) {
		return false;
	}
	++platform_runtime_leases;
	return true;
}

void release_platform_runtime_lease() noexcept
{
	if(platform_runtime_leases != 0U) {
		--platform_runtime_leases;
	}
	if(platform_runtime_leases == 0U) {
		SDL_Quit();
	}
}

std::expected<void, SdlPlatformError> write_locked_rgba(
	void *pixels,
	int pitch_bytes,
	const present::SourceSurface &surface
) noexcept
{
	if(pixels == nullptr || pitch_bytes < 0 ||
		static_cast<std::size_t>(pitch_bytes) < rgba_row_bytes) {
		return std::unexpected(SdlPlatformError{
			.code = SdlPlatformErrorCode::invalid_texture_pitch,
		});
	}
	const auto pitch = static_cast<std::size_t>(pitch_bytes);
	constexpr auto preceding_rows = present::graphics_height - 1U;
	if(pitch > ((std::numeric_limits<std::size_t>::max() - rgba_row_bytes) /
		preceding_rows)) {
		return std::unexpected(SdlPlatformError{
			.code = SdlPlatformErrorCode::invalid_texture_pitch,
		});
	}
	const auto extent = (preceding_rows * pitch) + rgba_row_bytes;
	auto *const bytes = static_cast<std::byte *>(pixels);
	const auto converted = app::source_surface_to_host_rgba(
		surface,
		app::HostRgbaTarget{
			std::span<std::byte>{bytes, extent},
			pitch,
			app::HostRgbaByteOrder::rgba,
		}
	);
	if(!converted) {
		return std::unexpected(surface_error(
			SdlPlatformErrorCode::surface_conversion_failed,
			converted.error()
		));
	}
	return {};
}

} // namespace detail

SdlPlatform::SdlPlatform(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{
}

SdlPlatform::~SdlPlatform() = default;

std::expected<std::unique_ptr<SdlPlatform>, SdlPlatformError>
SdlPlatform::create(const SdlPlatformConfig &config)
{
	if(config.title.empty() ||
		config.initial_width < present::graphics_width ||
		config.initial_height < present::graphics_height ||
		!valid_dimension(config.initial_width) ||
		!valid_dimension(config.initial_height)) {
		return std::unexpected(SdlPlatformError{
			.code = SdlPlatformErrorCode::invalid_config,
		});
	}

	auto impl = std::make_unique<Impl>();
	const auto acquired = detail::acquire_platform_runtime();
	if(acquired == detail::SdlPlatformRuntimeAcquire::already_initialized) {
		return std::unexpected(SdlPlatformError{
			.code = SdlPlatformErrorCode::already_initialized,
		});
	}
	if(acquired == detail::SdlPlatformRuntimeAcquire::initialization_failed) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::initialization_failed
		));
	}
	impl->runtime_lease = true;

	const std::string title{config.title};
	SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
	if(!SDL_CreateWindowAndRenderer(
		title.c_str(),
		static_cast<int>(config.initial_width),
		static_cast<int>(config.initial_height),
		flags,
		&impl->window,
		&impl->renderer
	)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::window_renderer_failed
		));
	}
	if(!SDL_SetRenderVSync(impl->renderer, 1)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::present_vsync_failed
		));
	}
	if(!SDL_SetWindowMinimumSize(impl->window, source_width, source_height)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::window_setup_failed
		));
	}
	if(!SDL_SetRenderLogicalPresentation(
		impl->renderer,
		source_width,
		source_height,
		SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
	)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::logical_presentation_failed
		));
	}

	impl->texture = SDL_CreateTexture(
		impl->renderer,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STREAMING,
		source_width,
		source_height
	);
	if(impl->texture == nullptr ||
		!SDL_SetTextureScaleMode(impl->texture, SDL_SCALEMODE_NEAREST) ||
		!SDL_SetTextureBlendMode(impl->texture, SDL_BLENDMODE_NONE)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::texture_failed
		));
	}
	if(!config.hidden && !SDL_ShowWindow(impl->window)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::window_setup_failed
		));
	}
	if(!config.oracle_log_path.empty()) {
		const std::filesystem::path log_path{
			std::string{config.oracle_log_path}
		};
		const auto parent = log_path.parent_path();
		std::error_code error;
		if(!parent.empty()) {
			std::filesystem::create_directories(parent, error);
		}
		if(error) {
			return std::unexpected(SdlPlatformError{
				.code = SdlPlatformErrorCode::oracle_log_open_failed,
			});
		}
		impl->oracle_log.open(log_path, std::ios::out | std::ios::trunc);
		impl->oracle_log <<
			"stream\tschema\tpublication\tlogical_tick\tcoalesced_ticks\t"
			"overlay\treduced_flash\tsource_fnv64\traster_fnv64\t"
			"tram_fnv64\tshown_page\traster_mode\tscroll_y\ttone\n";
		impl->oracle_log.flush();
		if(!impl->oracle_log) {
			return std::unexpected(SdlPlatformError{
				.code = SdlPlatformErrorCode::oracle_log_open_failed,
			});
		}
		present::set_source_surface_oracle_hashes_enabled(true);
		impl->oracle_hash_lease = true;
	}

	return std::unique_ptr<SdlPlatform>{new SdlPlatform{std::move(impl)}};
}

std::expected<SdlPresentResult, SdlPlatformError> SdlPlatform::present(
	const present::SourceSurface &surface,
	SdlPresentationOracleIdentity identity
) noexcept
{
	void *pixels = nullptr;
	int pitch = 0;
	if(!SDL_LockTexture(impl_->texture, nullptr, &pixels, &pitch)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::texture_lock_failed
		));
	}
	const auto converted = detail::write_locked_rgba(pixels, pitch, surface);
	SDL_UnlockTexture(impl_->texture);
	if(!converted) {
		return std::unexpected(converted.error());
	}

	int output_width = 0;
	int output_height = 0;
	if(!SDL_GetRenderOutputSize(
		impl_->renderer, &output_width, &output_height
	) || output_width < 0 || output_height < 0) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::render_output_failed
		));
	}
	const auto destination = app::integer_letterbox_8x5(
		static_cast<std::size_t>(output_width),
		static_cast<std::size_t>(output_height)
	);
	if(!SDL_SetRenderDrawColor(impl_->renderer, 0U, 0U, 0U, 0xFFU) ||
		!SDL_RenderClear(impl_->renderer)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::render_failed
		));
	}
	if(destination &&
		!SDL_RenderTexture(impl_->renderer, impl_->texture, nullptr, nullptr)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::render_failed
		));
	}
	if(!SDL_RenderPresent(impl_->renderer)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::render_failed
		));
	}
	if(!impl_->record_oracle(surface, identity)) {
		return std::unexpected(SdlPlatformError{
			.code = SdlPlatformErrorCode::oracle_log_write_failed,
		});
	}
	return SdlPresentResult{destination};
}

bool SdlPlatform::oracle_enabled() const noexcept
{
	return impl_->oracle_log.is_open();
}

std::uint32_t SdlPlatform::window_id() const noexcept
{
	return static_cast<std::uint32_t>(SDL_GetWindowID(impl_->window));
}

std::expected<void, SdlPlatformError> SdlPlatformTestAccess::resize(
	SdlPlatform &platform,
	std::uint32_t width,
	std::uint32_t height
) noexcept
{
	if(width == 0U || height == 0U ||
		!valid_dimension(width) || !valid_dimension(height)) {
		return std::unexpected(SdlPlatformError{
			.code = SdlPlatformErrorCode::invalid_config,
		});
	}
	if(!SDL_SetWindowMinimumSize(platform.impl_->window, 1, 1) ||
		!SDL_SetWindowSize(
		platform.impl_->window,
		static_cast<int>(width),
		static_cast<int>(height)
	) || !SDL_SyncWindow(platform.impl_->window)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::window_setup_failed
		));
	}
	return {};
}

std::expected<detail::SdlPlatformMetrics, SdlPlatformError>
SdlPlatformTestAccess::metrics(const SdlPlatform &platform) noexcept
{
	int output_width = 0;
	int output_height = 0;
	SDL_FRect logical{};
	float texture_width = 0.0F;
	float texture_height = 0.0F;
	SDL_ScaleMode scale_mode = SDL_SCALEMODE_INVALID;
	SDL_BlendMode blend_mode = SDL_BLENDMODE_INVALID;
	int renderer_vsync = SDL_RENDERER_VSYNC_DISABLED;
	if(!SDL_GetRenderOutputSize(
		platform.impl_->renderer, &output_width, &output_height
	) || output_width < 0 || output_height < 0 ||
		!SDL_GetRenderLogicalPresentationRect(platform.impl_->renderer, &logical) ||
		!SDL_GetTextureSize(
			platform.impl_->texture, &texture_width, &texture_height
		) || !SDL_GetTextureScaleMode(platform.impl_->texture, &scale_mode) ||
		!SDL_GetTextureBlendMode(platform.impl_->texture, &blend_mode) ||
		!SDL_GetRenderVSync(platform.impl_->renderer, &renderer_vsync)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::render_output_failed
		));
	}
	const auto properties = SDL_GetTextureProperties(platform.impl_->texture);
	if(properties == 0U) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::render_output_failed
		));
	}
	const char *const video = SDL_GetCurrentVideoDriver();
	const char *const renderer = SDL_GetRendererName(platform.impl_->renderer);
	return detail::SdlPlatformMetrics{
		.output_width = static_cast<std::uint32_t>(output_width),
		.output_height = static_cast<std::uint32_t>(output_height),
		.logical_x = logical.x,
		.logical_y = logical.y,
		.logical_width = logical.w,
		.logical_height = logical.h,
		.texture_width = texture_width,
		.texture_height = texture_height,
		.texture_format = static_cast<std::uint32_t>(SDL_GetNumberProperty(
			properties, SDL_PROP_TEXTURE_FORMAT_NUMBER, 0
		)),
		.texture_access = static_cast<std::int32_t>(SDL_GetNumberProperty(
			properties, SDL_PROP_TEXTURE_ACCESS_NUMBER, -1
		)),
		.texture_scale_mode = static_cast<std::int32_t>(scale_mode),
		.texture_blend_mode = static_cast<std::uint32_t>(blend_mode),
		.renderer_vsync = renderer_vsync,
		.video_driver = (video == nullptr) ? std::string_view{} :
			std::string_view{video},
		.renderer_name = (renderer == nullptr) ? std::string_view{} :
			std::string_view{renderer},
	};
}

} // namespace th03::platform
