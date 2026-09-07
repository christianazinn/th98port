#include "th03/platform/sdl_platform.hpp"

#include "sdl_platform_internal.hpp"
#include "sdl_runtime_internal.hpp"

#include "th03/app/runtime/display_presentability.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include <SDL3/SDL.h>

namespace th03::platform {
namespace {

constexpr int source_width = static_cast<int>(present::graphics_width);
constexpr int source_height = static_cast<int>(present::graphics_height);
constexpr int display_4_3_height = 480;
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

[[nodiscard]] constexpr int logical_height(
	SdlAspectRatio aspect
) noexcept
{
	return aspect == SdlAspectRatio::pixels_8_5 ? source_height :
		display_4_3_height;
}

[[nodiscard]] std::optional<app::IntegerLetterboxRect> window_destination(
	std::size_t drawable_width,
	std::size_t drawable_height,
	SdlAspectRatio aspect
) noexcept
{
	if(aspect == SdlAspectRatio::pixels_8_5) {
		return app::integer_letterbox_8x5(drawable_width, drawable_height);
	}
	const auto scale = std::min(
		drawable_width / present::graphics_width,
		drawable_height / static_cast<std::size_t>(display_4_3_height)
	);
	if(scale == 0U) {
		return std::nullopt;
	}
	const auto width = scale * present::graphics_width;
	const auto height = scale * static_cast<std::size_t>(display_4_3_height);
	return app::IntegerLetterboxRect{
		(drawable_width - width) / 2U,
		(drawable_height - height) / 2U,
		width,
		height,
		scale,
	};
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
	struct HostOutputState {
		bool presentable = false;
		std::optional<app::IntegerLetterboxRect> destination{};
	};

	struct PreparedRgbFrame {
		std::filesystem::path temporary{};
		std::filesystem::path final{};
		std::string filename{};
		std::uint64_t hash = 0U;
	};

	SDL_Window *window = nullptr;
	SDL_Renderer *renderer = nullptr;
	SDL_Texture *texture = nullptr;
	bool runtime_lease = false;
	std::ofstream oracle_log{};
	std::uint64_t oracle_publication = 0U;
	std::filesystem::path oracle_rgb_directory{};
	std::ofstream oracle_rgb_manifest{};
	std::uint64_t oracle_rgb_publication = 0U;
	bool oracle_hash_lease = false;
	SdlWindowPresentation window_presentation{};
	app::SourcePresentationMailbox presentation_mailbox{};
	std::optional<bool> test_presentability_override{};

	[[nodiscard]] std::expected<HostOutputState, SdlPlatformError>
	host_output_state() const noexcept
	{
		const auto flags = SDL_GetWindowFlags(window);
		const bool flag_presentable =
			(flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED |
				SDL_WINDOW_OCCLUDED)) == 0U;
		if(!test_presentability_override.value_or(flag_presentable)) {
			return HostOutputState{};
		}
		int output_width = 0;
		int output_height = 0;
		if(!SDL_GetRenderOutputSize(renderer, &output_width, &output_height) ||
			output_width < 0 || output_height < 0) {
			return std::unexpected(error_with_sdl_detail(
				SdlPlatformErrorCode::render_output_failed
			));
		}
		if(output_width == 0 || output_height == 0) {
			return HostOutputState{};
		}
		return HostOutputState{
			.presentable = true,
			.destination = window_destination(
				static_cast<std::size_t>(output_width),
				static_cast<std::size_t>(output_height),
				window_presentation.aspect
			),
		};
	}

	[[nodiscard]] std::expected<void, SdlPlatformError> submit_texture(
		std::optional<app::IntegerLetterboxRect> destination
	) noexcept
	{
		if(!SDL_SetRenderDrawColor(renderer, 0U, 0U, 0U, 0xFFU) ||
			!SDL_RenderClear(renderer) ||
			(destination && !SDL_RenderTexture(
				renderer, texture, nullptr, nullptr
			)) || !SDL_RenderPresent(renderer)) {
			return std::unexpected(error_with_sdl_detail(
				SdlPlatformErrorCode::render_failed
			));
		}
		return {};
	}

	[[nodiscard]] std::optional<PreparedRgbFrame> prepare_rgb_frame(
		const void *pixels,
		int pitch_bytes
	)
	{
		if(!oracle_rgb_manifest.is_open()) {
			return PreparedRgbFrame{};
		}
		std::ostringstream filename;
		filename << "frame-" << std::setfill('0') << std::setw(8)
			<< std::dec << oracle_rgb_publication << ".ppm";
		PreparedRgbFrame frame{};
		frame.filename = filename.str();
		frame.final = oracle_rgb_directory / frame.filename;
		frame.temporary = frame.final;
		frame.temporary += ".tmp";
		std::ofstream output{frame.temporary, std::ios::binary | std::ios::trunc};
		output << "P6\n" << source_width << ' ' << source_height << "\n255\n";
		if(!output) {
			output.close();
			std::error_code ignored;
			std::filesystem::remove(frame.temporary, ignored);
			return std::nullopt;
		}
		const auto *const bytes = static_cast<const std::uint8_t *>(pixels);
		frame.hash = fnv_offset_basis;
		std::array<std::uint8_t, present::graphics_width * 3U> rgb_row{};
		for(int y = 0; y < source_height; ++y) {
			const auto *const row = bytes +
				(static_cast<std::size_t>(y) * static_cast<std::size_t>(pitch_bytes));
			for(int x = 0; x < source_width; ++x) {
				const auto *const pixel = row + (static_cast<std::size_t>(x) * 4U);
				for(std::size_t channel = 0U; channel < 3U; ++channel) {
					hash_byte(frame.hash, pixel[channel]);
					rgb_row[(static_cast<std::size_t>(x) * 3U) + channel] =
						pixel[channel];
				}
			}
			output.write(
				reinterpret_cast<const char *>(rgb_row.data()),
				static_cast<std::streamsize>(rgb_row.size())
			);
		}
		output.close();
		if(!output) {
			std::error_code ignored;
			std::filesystem::remove(frame.temporary, ignored);
			return std::nullopt;
		}
		return frame;
	}

	[[nodiscard]] bool publish_rgb_frame(
		const present::SourceSurface &surface,
		SdlPresentationOracleIdentity identity,
		const PreparedRgbFrame &frame
	)
	{
		if(!oracle_rgb_manifest.is_open()) {
			return true;
		}
		std::error_code error;
		std::filesystem::rename(frame.temporary, frame.final, error);
		if(error) {
			std::filesystem::remove(frame.temporary, error);
			return false;
		}
		oracle_rgb_manifest << "T3RGB\t1\t" << std::dec
			<< oracle_rgb_publication++ << '\t';
		if(identity.logical_tick) {
			oracle_rgb_manifest << *identity.logical_tick;
		} else {
			oracle_rgb_manifest << "na";
		}
		oracle_rgb_manifest << '\t' << identity.coalesced_ticks << '\t'
			<< static_cast<unsigned>(surface.host_overlay) << '\t'
			<< static_cast<unsigned>(surface.reduced_flash_applied) << '\t'
			<< std::hex << std::setfill('0') << std::setw(16)
			<< detail::source_surface_state_hash(surface) << '\t';
		if(surface.oracle_hashes) {
			oracle_rgb_manifest << std::setw(16)
				<< surface.oracle_hashes->raster_display << '\t'
				<< std::setw(16) << surface.oracle_hashes->tram_visible;
		} else {
			oracle_rgb_manifest << "na\tna";
		}
		oracle_rgb_manifest << std::dec << '\t'
			<< static_cast<unsigned>(surface.shown_page) << '\t'
			<< static_cast<unsigned>(surface.raster_mode) << '\t'
			<< surface.effective_scroll_y << '\t'
			<< surface.palette_tone << '\t'
			<< std::hex << std::setw(16) << frame.hash << '\t'
			<< frame.filename << '\n';
		oracle_rgb_manifest.flush();
		return static_cast<bool>(oracle_rgb_manifest);
	}

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
	if(config.initial_width % present::graphics_width == 0U &&
		config.initial_height % present::graphics_height == 0U) {
		const auto width_scale = config.initial_width / present::graphics_width;
		const auto height_scale = config.initial_height / present::graphics_height;
		if(width_scale == height_scale && width_scale >= 1U && width_scale <= 3U) {
			impl->window_presentation.integer_scale =
				static_cast<std::uint8_t>(width_scale);
		}
	}

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
	}
	if(!config.oracle_rgb_directory.empty()) {
		impl->oracle_rgb_directory = std::filesystem::path{
			config.oracle_rgb_directory
		};
		std::error_code error;
		std::filesystem::create_directories(impl->oracle_rgb_directory, error);
		const auto manifest = impl->oracle_rgb_directory / "T3RGB1.tsv";
		const bool directory_empty = !error &&
			(std::filesystem::directory_iterator{
				impl->oracle_rgb_directory, error
			} == std::filesystem::directory_iterator{});
		if(error || !directory_empty) {
			return std::unexpected(SdlPlatformError{
				.code = SdlPlatformErrorCode::oracle_rgb_open_failed,
			});
		}
		impl->oracle_rgb_manifest.open(
			manifest, std::ios::out | std::ios::trunc
		);
		impl->oracle_rgb_manifest <<
			"stream\tschema\tpublication\tlogical_tick\tcoalesced_ticks\t"
			"overlay\treduced_flash\tsource_fnv64\traster_fnv64\t"
			"tram_fnv64\tshown_page\traster_mode\tscroll_y\ttone\t"
			"rgb_fnv64\tfile\n";
		impl->oracle_rgb_manifest.flush();
		if(!impl->oracle_rgb_manifest) {
			return std::unexpected(SdlPlatformError{
				.code = SdlPlatformErrorCode::oracle_rgb_open_failed,
			});
		}
	}
	if(impl->oracle_log.is_open() || impl->oracle_rgb_manifest.is_open()) {
		present::set_source_surface_oracle_hashes_enabled(true);
		impl->oracle_hash_lease = true;
	}

	auto platform = std::unique_ptr<SdlPlatform>{
		new SdlPlatform{std::move(impl)}
	};
	if(config.initial_window_presentation) {
		if(auto applied = platform->apply_window_presentation(
			*config.initial_window_presentation
		); !applied) {
			return std::unexpected(std::move(applied.error()));
		}
	}
	if(!config.hidden && !SDL_ShowWindow(platform->impl_->window)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::window_setup_failed
		));
	}
	return platform;
}

[[nodiscard]] bool file_url_byte_unescaped(unsigned char value) noexcept
{
	return (value >= static_cast<unsigned char>('a') &&
		value <= static_cast<unsigned char>('z')) ||
		(value >= static_cast<unsigned char>('A') &&
		value <= static_cast<unsigned char>('Z')) ||
		(value >= static_cast<unsigned char>('0') &&
		value <= static_cast<unsigned char>('9')) ||
		value == static_cast<unsigned char>('-') ||
		value == static_cast<unsigned char>('_') ||
		value == static_cast<unsigned char>('.') ||
		value == static_cast<unsigned char>('~') ||
		value == static_cast<unsigned char>('/') ||
		value == static_cast<unsigned char>(':');
}

[[nodiscard]] std::optional<std::string> build_folder_file_url(
	const std::filesystem::path &path
)
{
	if(path.empty()) {
		return std::nullopt;
	}
	std::error_code error{};
	const auto absolute = std::filesystem::absolute(path, error);
	if(error || absolute.empty()) {
		return std::nullopt;
	}
	const auto generic = absolute.generic_u8string();
	std::string bytes{};
	bytes.reserve(generic.size());
	for(const auto value : generic) {
		bytes.push_back(static_cast<char>(value));
	}
	std::string url = bytes.starts_with("//") ? "file:" :
		(bytes.starts_with('/') ? "file://" : "file:///");
	constexpr char hex[] = "0123456789ABCDEF";
	for(const auto value : bytes) {
		const auto byte = static_cast<unsigned char>(value);
		if(file_url_byte_unescaped(byte)) {
			url.push_back(value);
		} else {
			url.push_back('%');
			url.push_back(hex[byte >> 4U]);
			url.push_back(hex[byte & 0x0FU]);
		}
	}
	return url;
}

std::expected<SdlPresentResult, SdlPlatformError> SdlPlatform::present(
	const present::SourceSurface &surface,
	SdlPresentationOracleIdentity identity
) noexcept
{
	std::optional<std::uint64_t> publication{};
	if(impl_->oracle_log.is_open()) {
		publication = impl_->oracle_publication;
	}
	if(impl_->oracle_rgb_manifest.is_open()) {
		if(publication && *publication != impl_->oracle_rgb_publication) {
			return std::unexpected(SdlPlatformError{
				.code = SdlPlatformErrorCode::oracle_rgb_write_failed,
			});
		}
		publication = impl_->oracle_rgb_publication;
	}
	void *pixels = nullptr;
	int pitch = 0;
	if(!SDL_LockTexture(impl_->texture, nullptr, &pixels, &pitch)) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::texture_lock_failed
		));
	}
	const auto converted = detail::write_locked_rgba(pixels, pitch, surface);
	std::optional<Impl::PreparedRgbFrame> rgb_frame{};
	if(converted && impl_->oracle_rgb_manifest.is_open()) {
		rgb_frame = impl_->prepare_rgb_frame(pixels, pitch);
	}
	SDL_UnlockTexture(impl_->texture);
	if(!converted) {
		return std::unexpected(converted.error());
	}
	if(impl_->oracle_rgb_manifest.is_open() && !rgb_frame) {
		return std::unexpected(SdlPlatformError{
			.code = SdlPlatformErrorCode::oracle_rgb_write_failed,
		});
	}
	const auto discard_rgb_frame = [&rgb_frame]() noexcept {
		if(rgb_frame) {
			std::error_code ignored;
			std::filesystem::remove(rgb_frame->temporary, ignored);
		}
	};

	const auto output = impl_->host_output_state();
	if(!output) {
		discard_rgb_frame();
		return std::unexpected(output.error());
	}
	const auto requested = impl_->presentation_mailbox.publish_source(
		output->presentable
	);
	bool submitted_to_host = false;
	if(requested) {
		if(auto submitted = impl_->submit_texture(output->destination); !submitted) {
			discard_rgb_frame();
			return std::unexpected(submitted.error());
		}
		submitted_to_host = impl_->presentation_mailbox
			.acknowledge_host_submission(*requested);
	}
	if(!impl_->record_oracle(surface, identity)) {
		discard_rgb_frame();
		return std::unexpected(SdlPlatformError{
			.code = SdlPlatformErrorCode::oracle_log_write_failed,
		});
	}
	if(rgb_frame && !impl_->publish_rgb_frame(surface, identity, *rgb_frame)) {
		return std::unexpected(SdlPlatformError{
			.code = SdlPlatformErrorCode::oracle_rgb_write_failed,
		});
	}
	return SdlPresentResult{
		.destination = submitted_to_host ? output->destination : std::nullopt,
		.submitted_to_host = submitted_to_host,
		.retained_for_restore =
			impl_->presentation_mailbox.submission_pending(),
		.oracle_publication = publication,
	};
}

std::expected<SdlPresentabilityRefreshResult, SdlPlatformError>
SdlPlatform::refresh_presentability() noexcept
{
	const auto output = impl_->host_output_state();
	if(!output) {
		return std::unexpected(output.error());
	}
	const auto requested = impl_->presentation_mailbox.refresh_host(
		output->presentable
	);
	if(!requested) {
		return SdlPresentabilityRefreshResult{
			.presentable = output->presentable,
		};
	}
	if(auto submitted = impl_->submit_texture(output->destination); !submitted) {
		return std::unexpected(submitted.error());
	}
	const bool acknowledged = impl_->presentation_mailbox
		.acknowledge_host_submission(*requested);
	return SdlPresentabilityRefreshResult{
		.presentable = output->presentable,
		.submitted_retained_surface = acknowledged,
		.destination = acknowledged ? output->destination : std::nullopt,
	};
}

bool SdlPlatform::oracle_enabled() const noexcept
{
	return impl_->oracle_log.is_open() || impl_->oracle_rgb_manifest.is_open();
}

std::expected<void, SdlPlatformError>
SdlPlatform::apply_window_presentation(
	SdlWindowPresentation presentation
) noexcept
{
	if(!impl_ || presentation.integer_scale > 3U ||
		static_cast<std::uint8_t>(presentation.mode) >
			static_cast<std::uint8_t>(SdlWindowMode::borderless_fullscreen) ||
		static_cast<std::uint8_t>(presentation.aspect) >
			static_cast<std::uint8_t>(SdlAspectRatio::display_4_3) ||
		static_cast<std::uint8_t>(presentation.filtering) >
			static_cast<std::uint8_t>(SdlFiltering::linear)) {
		return std::unexpected(SdlPlatformError{
			.code = SdlPlatformErrorCode::invalid_config,
		});
	}
	if(presentation == impl_->window_presentation) {
		return {};
	}
	const auto apply = [this](SdlWindowPresentation value) noexcept {
		const auto height = logical_height(value.aspect);
		const auto filtering = value.filtering == SdlFiltering::nearest ?
			SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR;
		if(!SDL_SetTextureScaleMode(impl_->texture, filtering) ||
			!SDL_SetRenderLogicalPresentation(
				impl_->renderer,
				source_width,
				height,
				SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
			)) {
			return false;
		}
		if(value.mode == SdlWindowMode::borderless_fullscreen) {
			return SDL_SetWindowFullscreenMode(impl_->window, nullptr) &&
				SDL_SetWindowFullscreen(impl_->window, true) &&
				SDL_SyncWindow(impl_->window);
		}
		auto scale = value.integer_scale;
		if(scale == 0U) {
			SDL_Rect usable{};
			const auto display = SDL_GetDisplayForWindow(impl_->window);
			if(display == 0U || !SDL_GetDisplayUsableBounds(display, &usable) ||
				usable.w < source_width || usable.h < height) {
				return false;
			}
			scale = static_cast<std::uint8_t>(std::clamp(
				std::min(usable.w / source_width, usable.h / height),
				1, 3
			));
		}
		const auto width = static_cast<int>(present::graphics_width * scale);
		const auto window_height = height * scale;
		return SDL_SetWindowFullscreen(impl_->window, false) &&
			SDL_SetWindowSize(impl_->window, width, window_height) &&
			SDL_SetWindowPosition(
				impl_->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED
			) && SDL_SyncWindow(impl_->window);
	};
	const auto previous = impl_->window_presentation;
	if(!apply(presentation)) {
		const auto transition_error = error_with_sdl_detail(
			SdlPlatformErrorCode::window_presentation_failed
		);
		if(!apply(previous)) {
			return std::unexpected(error_with_sdl_detail(
				SdlPlatformErrorCode::window_presentation_rollback_failed
			));
		}
		return std::unexpected(transition_error);
	}
	impl_->window_presentation = presentation;
	return {};
}

SdlWindowPresentation SdlPlatform::window_presentation() const noexcept
{
	return impl_ ? impl_->window_presentation : SdlWindowPresentation{};
}

std::expected<void, SdlPlatformError> SdlPlatform::open_folder(
	const std::filesystem::path &path
) noexcept
{
	try {
		const auto url = build_folder_file_url(path);
		if(!url || !SDL_OpenURL(url->c_str())) {
			return std::unexpected(error_with_sdl_detail(
				SdlPlatformErrorCode::open_folder_failed
			));
		}
	} catch(...) {
		return std::unexpected(error_with_sdl_detail(
			SdlPlatformErrorCode::open_folder_failed
		));
	}
	return {};
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
		.source_publications = platform.impl_->presentation_mailbox
			.source_publication_count(),
		.host_submissions = platform.impl_->presentation_mailbox
			.host_submission_count(),
		.restore_submission_pending = platform.impl_->presentation_mailbox
			.submission_pending(),
		.video_driver = (video == nullptr) ? std::string_view{} :
			std::string_view{video},
		.renderer_name = (renderer == nullptr) ? std::string_view{} :
			std::string_view{renderer},
	};
}

void SdlPlatformTestAccess::set_presentability_override(
	SdlPlatform &platform,
	std::optional<bool> presentable
) noexcept
{
	platform.impl_->test_presentability_override = presentable;
}

std::optional<std::string> SdlPlatformTestAccess::folder_file_url(
	const std::filesystem::path &path
)
{
	try {
		return build_folder_file_url(path);
	} catch(...) {
		return std::nullopt;
	}
}

} // namespace th03::platform
