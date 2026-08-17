#include "th03/platform/sdl_platform.hpp"
#include "th03/platform/sdl_input.hpp"

#include "sdl_platform_internal.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

namespace {

using th03::platform::SdlPlatform;
using th03::platform::SdlPlatformConfig;
using th03::platform::SdlPlatformErrorCode;
using th03::platform::SdlPlatformTestAccess;
using th03::platform::SdlWindowMode;
using th03::platform::SdlWindowPresentation;
using th03::present::ComposedPixel;
using th03::present::ComposedPixelLayer;

constexpr std::size_t width = th03::present::graphics_width;
constexpr std::size_t height = th03::present::graphics_height;
constexpr std::size_t pixel_count = th03::present::graphics_page_pixels;
constexpr std::size_t rgba_row_bytes = width * 4U;
constexpr std::byte untouched{0xA5U};

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

static_assert(
	th03::platform::th03_product_window_title ==
		"The Phantasmagoria of Dim. Dream" &&
	SdlPlatformConfig{}.title == th03::platform::th03_product_window_title,
	"every SDL launch profile uses the product title"
);

struct SurfaceFixture {
	std::vector<ComposedPixel> pixels = std::vector<ComposedPixel>(
		pixel_count,
		ComposedPixel{ComposedPixelLayer::graphics, 0U}
	);
	th03::assets::Palette16Rgb4 palette{};

	SurfaceFixture()
	{
		palette.colors[0U] = {1U, 2U, 3U};
		palette.colors[1U] = {15U, 0U, 8U};
		pixels[0U] = {ComposedPixelLayer::graphics, 1U};
		pixels[width - 1U] = {ComposedPixelLayer::text, 7U};
		pixels[pixel_count - 1U] = {ComposedPixelLayer::graphics, 1U};
	}

	[[nodiscard]] th03::present::SourceSurface source() const
	{
		return {
			.pixels = std::span<const ComposedPixel, pixel_count>{
				pixels.data(), pixels.size()
			},
			.graphics_palette = palette,
			.oracle_hashes = th03::present::SourceSurfaceOracleHashes{
				0x1122334455667788ULL,
				0x8877665544332211ULL,
			},
		};
	}
};

[[nodiscard]] bool bytes_equal(
	std::span<const std::byte> bytes,
	std::size_t offset,
	std::initializer_list<std::uint8_t> expected
)
{
	std::size_t index = 0U;
	for(const auto value : expected) {
		if(bytes[offset + index] != static_cast<std::byte>(value)) {
			return false;
		}
		++index;
	}
	return true;
}

void test_folder_url_projection()
{
	expect(!SdlPlatformTestAccess::folder_file_url({}),
		"empty folder actions fail closed before reaching the desktop shell");
	const auto encoded = SdlPlatformTestAccess::folder_file_url(
		std::filesystem::current_path() / "Replay Folder#1"
	);
	expect(encoded && encoded->starts_with("file://") &&
		encoded->find("Replay%20Folder%231") != std::string::npos &&
		encoded->find(' ') == std::string::npos &&
		encoded->find('#') == std::string::npos,
		"folder actions produce an absolute escaped file URL");
}

void test_locked_texture_adapter()
{
	SurfaceFixture fixture;
	const auto original_hash =
		th03::platform::detail::source_surface_state_hash(fixture.source());
	fixture.pixels[1U] = {ComposedPixelLayer::text, 2U};
	expect(
		th03::platform::detail::source_surface_state_hash(fixture.source()) !=
			original_hash,
		"oracle host-input hash covers the complete composed pixel domain"
	);
	fixture.pixels[1U] = {ComposedPixelLayer::graphics, 0U};
	for(const std::size_t pitch : {2567U, 2573U}) {
		const auto extent = ((height - 1U) * pitch) + rgba_row_bytes;
		std::vector<std::byte> output(extent + 11U, untouched);
		const auto result = th03::platform::detail::write_locked_rgba(
			output.data(), static_cast<int>(pitch), fixture.source()
		);
		expect(static_cast<bool>(result),
			"locked texture adapter accepts an arbitrary positive padded pitch");
		expect(bytes_equal(output, 0U, {255U, 0U, 136U, 255U}),
			"locked texture adapter writes RGBA32 channel bytes");
		expect(bytes_equal(output, (width - 1U) * 4U,
			{255U, 255U, 255U, 255U}),
			"locked texture adapter preserves the text color domain");
		expect(bytes_equal(output, ((height - 1U) * pitch) +
			((width - 1U) * 4U), {255U, 0U, 136U, 255U}),
			"locked texture adapter reaches the final pixel at the supplied pitch");
		expect(std::all_of(
			output.begin() + static_cast<std::ptrdiff_t>(rgba_row_bytes),
			output.begin() + static_cast<std::ptrdiff_t>(pitch),
			[](std::byte value) { return value == untouched; }
		), "locked texture adapter does not overwrite row padding");
		expect(std::all_of(
			output.end() - 11, output.end(),
			[](std::byte value) { return value == untouched; }
		), "locked texture adapter does not overwrite trailing storage");
	}

	std::vector<std::byte> output(height * rgba_row_bytes, untouched);
	const auto before = output;
	expect(
		!th03::platform::detail::write_locked_rgba(
			nullptr, static_cast<int>(rgba_row_bytes), fixture.source()
		) && !th03::platform::detail::write_locked_rgba(
			output.data(), -1, fixture.source()
		) && !th03::platform::detail::write_locked_rgba(
			output.data(), static_cast<int>(rgba_row_bytes - 1U), fixture.source()
		),
		"null, negative, and short locked texture pitches are rejected"
	);
	expect(output == before,
		"invalid locked texture geometry leaves output storage unchanged");

	fixture.pixels[pixel_count - 1U] = {
		static_cast<ComposedPixelLayer>(0xFFU), 0U
	};
	const auto malformed = th03::platform::detail::write_locked_rgba(
		output.data(), static_cast<int>(rgba_row_bytes), fixture.source()
	);
	expect(
		!malformed && malformed.error().code ==
			SdlPlatformErrorCode::surface_conversion_failed &&
			malformed.error().surface_error && output == before,
		"late invalid source pixels propagate a nested error before any write"
	);
}

void expect_geometry(
	SdlPlatform &platform,
	std::uint32_t window_width,
	std::uint32_t window_height,
	const th03::app::IntegerLetterboxRect &expected,
	const char *message
)
{
	const auto resized = SdlPlatformTestAccess::resize(
		platform, window_width, window_height
	);
	const auto metrics = SdlPlatformTestAccess::metrics(platform);
	if(expected.scale == 0U) {
		expect(
			resized && metrics && metrics->output_width == window_width &&
				metrics->output_height == window_height,
			message
		);
		return;
	}
	expect(
		resized && metrics && metrics->output_width == window_width &&
			metrics->output_height == window_height &&
			static_cast<std::size_t>(std::floor(metrics->logical_x)) == expected.x &&
			static_cast<std::size_t>(std::floor(metrics->logical_y)) == expected.y &&
			static_cast<std::size_t>(metrics->logical_width) == expected.width &&
			static_cast<std::size_t>(metrics->logical_height) == expected.height,
		message
	);
}

void test_dummy_platform_lifecycle_and_present()
{
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto oracle_root = std::filesystem::temp_directory_path() /
		("th03-platform-pixel-oracle-test-" + std::to_string(nonce));
	const auto oracle_path = oracle_root / "capture.tsv";
	const auto rgb_root = oracle_root / "rgb";
	std::error_code remove_error;
	std::filesystem::remove_all(oracle_root, remove_error);
	const auto oracle_path_text = oracle_path.string();
	const auto rgb_root_text = rgb_root.string();
	expect(SDL_VERSION == SDL_VERSIONNUM(3, 4, 10),
		"platform tests compile against exactly SDL 3.4.10");
	expect(SDL_GetVersion() == SDL_VERSIONNUM(3, 4, 10),
		"platform tests load exactly the pinned SDL 3.4.10 runtime");
	expect(SDL_WasInit(0U) == 0U,
		"SDL subsystems are initially unowned by the test process");

	expect(!SdlPlatform::create(SdlPlatformConfig{.title = {}}),
		"an empty copied window title is rejected before SDL initialization");
	expect(!SdlPlatform::create(SdlPlatformConfig{
		.title = "small",
		.initial_width = 639U,
		.initial_height = 400U,
	}), "a sub-native initial drawable is rejected before SDL initialization");
	expect(SDL_WasInit(0U) == 0U,
		"invalid platform configuration mutates no SDL subsystem state");
	expect(SDL_Init(SDL_INIT_EVENTS),
		"test establishes an externally owned SDL subsystem");
	const auto externally_owned = SdlPlatform::create(SdlPlatformConfig{
		.title = "must not steal external SDL lifetime",
		.hidden = true,
	});
	expect(
		!externally_owned && externally_owned.error().code ==
			SdlPlatformErrorCode::already_initialized &&
			SDL_WasInit(SDL_INIT_EVENTS) != 0U,
		"platform refuses external SDL ownership without shutting it down"
	);
	SDL_Quit();
	expect(SDL_WasInit(0U) == 0U,
		"external SDL test owner performs its own teardown");
	expect(SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0"),
		"test establishes a conflicting ambient renderer VSync hint");

	{
		auto created = SdlPlatform::create(SdlPlatformConfig{
			.title = "persisted startup presentation",
			.initial_window_presentation = SdlWindowPresentation{
				.mode = SdlWindowMode::windowed,
				.integer_scale = 2U,
				.filtering = th03::platform::SdlFiltering::linear,
			},
			.hidden = true,
		});
		expect(static_cast<bool>(created),
			"persisted presentation applies during hidden window construction");
		if(created) {
			const auto metrics = SdlPlatformTestAccess::metrics(**created);
			expect(
				metrics && metrics->output_width == 1280U &&
					metrics->output_height == 800U &&
					metrics->texture_scale_mode == SDL_SCALEMODE_LINEAR &&
					(*created)->window_presentation() == SdlWindowPresentation{
						.mode = SdlWindowMode::windowed,
						.integer_scale = 2U,
						.filtering = th03::platform::SdlFiltering::linear,
					},
				"the first visible frame inherits saved size, aspect, and filtering"
			);
		}
	}
	expect(SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0"),
		"test restores the conflicting renderer VSync hint after startup-policy teardown");

	{
		auto created = SdlPlatform::create(SdlPlatformConfig{
			.title = "th03 SDL dummy test",
			.initial_width = 640U,
			.initial_height = 400U,
			.hidden = true,
			.oracle_log_path = oracle_path_text,
			.oracle_rgb_directory = rgb_root_text,
		});
		expect(static_cast<bool>(created),
			"pinned dummy/software SDL platform is created");
		if(!created) {
			return;
		}
		auto &platform = **created;
		SdlPlatformTestAccess::set_presentability_override(platform, true);
		expect(platform.oracle_enabled(),
			"state and RGB capture make publication-preserving oracle policy observable");
		expect(platform.window_id() != 0U,
			"opaque platform exposes the stable numeric window identity needed by its input mapper");
		const auto duplicate = SdlPlatform::create(SdlPlatformConfig{
			.title = "forbidden concurrent owner",
			.hidden = true,
		});
		expect(
			!duplicate && duplicate.error().code ==
				SdlPlatformErrorCode::already_initialized,
			"a second process-global SDL platform owner is rejected"
		);
		const auto metrics = SdlPlatformTestAccess::metrics(platform);
		expect(
			metrics && metrics->video_driver == "dummy" &&
				metrics->renderer_name == "software" &&
				metrics->renderer_vsync == 1,
			"platform overrides ambient policy with interval-one VSync on the requested dummy/software renderer"
		);
		expect(
			metrics && metrics->texture_width == 640.0F &&
				metrics->texture_height == 400.0F &&
				metrics->texture_format == static_cast<std::uint32_t>(SDL_PIXELFORMAT_RGBA32) &&
				metrics->texture_access == SDL_TEXTUREACCESS_STREAMING &&
				metrics->texture_scale_mode == SDL_SCALEMODE_NEAREST &&
				metrics->texture_blend_mode == SDL_BLENDMODE_NONE,
			"platform retains one exact RGBA32 streaming nearest/no-blend texture"
		);

		SurfaceFixture fixture;
		const auto first = platform.present(fixture.source(), {
			.logical_tick = 17U,
			.coalesced_ticks = 1U,
		});
		expect(
			first && first->destination == th03::app::IntegerLetterboxRect{
				0U, 0U, 640U, 400U, 1U
			} && first->submitted_to_host && !first->retained_for_restore &&
				first->oracle_publication == 0U,
			"native-size source surface uploads and presents at integer scale one"
		);
		const auto before_invalid = platform.window_presentation();
		expect(
			!platform.apply_window_presentation({
				.integer_scale = 4U,
			}) && platform.window_presentation() == before_invalid,
			"invalid window presentation is rejected before mutating SDL state"
		);
		expect(
			platform.apply_window_presentation({
				.mode = SdlWindowMode::windowed,
				.integer_scale = 2U,
			}).has_value(),
			"windowed 2x presentation applies live"
		);
		const auto doubled = SdlPlatformTestAccess::metrics(platform);
		expect(
			doubled && doubled->output_width == 1280U &&
				doubled->output_height == 800U &&
				platform.window_presentation() == SdlWindowPresentation{
					.mode = SdlWindowMode::windowed,
					.integer_scale = 2U,
				},
			"windowed preset changes only the physical output extent"
		);
		expect(
			platform.apply_window_presentation({
				.mode = SdlWindowMode::borderless_fullscreen,
				.integer_scale = 2U,
			}).has_value() && platform.window_presentation().mode ==
				SdlWindowMode::borderless_fullscreen,
			"borderless desktop fullscreen applies as a complete mode"
		);
		expect(
			platform.apply_window_presentation({
				.mode = SdlWindowMode::windowed,
				.integer_scale = 1U,
			}).has_value(),
			"leaving borderless mode restores the requested windowed preset"
		);
		expect(
			platform.apply_window_presentation({
				.mode = SdlWindowMode::windowed,
				.integer_scale = 0U,
			}).has_value() &&
				platform.window_presentation().integer_scale == 0U,
			"automatic scale resolves against the current display while retaining its typed policy"
		);
		expect(platform.apply_window_presentation({}).has_value(),
			"explicit 1x restores the canonical default after automatic scale");
		expect(
			platform.apply_window_presentation({
				.aspect = th03::platform::SdlAspectRatio::display_4_3,
				.filtering = th03::platform::SdlFiltering::linear,
			}).has_value(),
			"4:3 display aspect and linear filtering apply as one live policy"
		);
		const auto display_aspect = SdlPlatformTestAccess::metrics(platform);
		expect(
			display_aspect && display_aspect->output_width == 640U &&
				display_aspect->output_height == 480U &&
				display_aspect->logical_width == 640.0F &&
				display_aspect->logical_height == 480.0F &&
				display_aspect->texture_width == 640.0F &&
				display_aspect->texture_height == 400.0F &&
				display_aspect->texture_scale_mode == SDL_SCALEMODE_LINEAR,
			"display aspect changes only output geometry and texture sampling"
		);
		const auto aspect_frame = platform.present(fixture.source());
		expect(
			aspect_frame && aspect_frame->destination ==
				th03::app::IntegerLetterboxRect{0U, 0U, 640U, 480U, 1U},
			"4:3 display policy reports its exact physical destination"
		);
		expect(platform.apply_window_presentation({}).has_value(),
			"default 8:5 nearest policy restores atomically");
		fixture.pixels[0U] = {ComposedPixelLayer::text, 4U};
		expect(static_cast<bool>(platform.present(fixture.source())),
			"persistent texture accepts a distinct second complete frame");

		expect_geometry(
			platform, 1280U, 900U,
			{0U, 50U, 1280U, 800U, 2U},
			"wide dummy output has centered integer logical presentation"
		);
		const auto wide = platform.present(fixture.source());
		expect(wide && wide->destination == th03::app::IntegerLetterboxRect{
			0U, 50U, 1280U, 800U, 2U
		}, "present reports the same integer destination as the output geometry");
		expect_geometry(
			platform, 800U, 600U,
			{80U, 100U, 640U, 400U, 1U},
			"tall dummy output has centered integer logical presentation"
		);
		expect_geometry(
			platform, 642U, 401U,
			{1U, 0U, 640U, 400U, 1U},
			"odd dummy output matches integer centering after flooring SDL's half-pixel offset"
		);
		expect_geometry(
			platform, 320U, 200U,
			{0U, 0U, 0U, 0U, 0U},
			"test seam reaches a sub-native dummy output without stretching"
		);
		const auto black_only = platform.present(fixture.source());
		expect(black_only && !black_only->destination,
			"sub-native output takes the successful black-only presentation path");

		fixture.pixels[pixel_count - 1U] = {
			static_cast<ComposedPixelLayer>(0xFFU), 0U
		};
		const auto malformed = platform.present(fixture.source());
		expect(
			!malformed && malformed.error().code ==
				SdlPlatformErrorCode::surface_conversion_failed &&
				malformed.error().surface_error,
			"actual SDL texture lock path rejects a late malformed source before rendering"
		);
		fixture.pixels[pixel_count - 1U] = {
			ComposedPixelLayer::graphics, 1U
		};
		expect(static_cast<bool>(platform.present(fixture.source())),
			"platform recovers with a valid frame after a failed conversion");

		const auto before_suppression = SdlPlatformTestAccess::metrics(platform);
		SdlPlatformTestAccess::set_presentability_override(platform, false);
		fixture.pixels[0U] = {ComposedPixelLayer::text, 5U};
		const auto suppressed_first = platform.present(fixture.source(), {
			.logical_tick = 90U,
			.coalesced_ticks = 1U,
		});
		fixture.pixels[0U] = {ComposedPixelLayer::text, 6U};
		const auto suppressed_newest = platform.present(fixture.source(), {
			.logical_tick = 91U,
			.coalesced_ticks = 1U,
		});
		const auto suppressed_metrics = SdlPlatformTestAccess::metrics(platform);
		expect(
			before_suppression && suppressed_first && suppressed_newest &&
				!suppressed_first->submitted_to_host &&
				suppressed_first->retained_for_restore &&
				!suppressed_newest->submitted_to_host &&
				suppressed_newest->retained_for_restore &&
				suppressed_first->oracle_publication == 6U &&
				suppressed_newest->oracle_publication == 7U &&
				suppressed_metrics && suppressed_metrics->source_publications ==
					before_suppression->source_publications + 2U &&
				suppressed_metrics->host_submissions ==
					before_suppression->host_submissions &&
				suppressed_metrics->restore_submission_pending,
			"minimized publications retain only the newest texture while every oracle source row remains complete"
		);
		const auto still_suppressed = platform.refresh_presentability();
		SdlPlatformTestAccess::set_presentability_override(platform, true);
		const auto restored = platform.refresh_presentability();
		const auto restored_again = platform.refresh_presentability();
		const auto restored_metrics = SdlPlatformTestAccess::metrics(platform);
		expect(
			still_suppressed && !still_suppressed->presentable &&
				!still_suppressed->submitted_retained_surface && restored &&
				restored->presentable && restored->submitted_retained_surface &&
				restored_again && restored_again->presentable &&
				!restored_again->submitted_retained_surface && restored_metrics &&
				restored_metrics->host_submissions ==
					before_suppression->host_submissions + 1U &&
				!restored_metrics->restore_submission_pending,
			"restore submits the newest retained texture exactly once without another source publication"
		);
	}
	expect(SDL_WasInit(0U) == 0U,
		"platform destruction releases all SDL subsystems it owns");
	{
		std::ifstream oracle{oracle_path};
		std::vector<std::string> rows{};
		for(std::string row; std::getline(oracle, row);) {
			rows.push_back(std::move(row));
		}
		expect(
			rows.size() == 9U && rows[0U] ==
				"stream\tschema\tpublication\tlogical_tick\tcoalesced_ticks\t"
				"overlay\treduced_flash\tsource_fnv64\traster_fnv64\t"
				"tram_fnv64\tshown_page\traster_mode\tscroll_y\ttone" &&
				rows[1U].starts_with("T3PXH\t2\t0\t17\t1\t0\t0\t") &&
				rows[1U].find("\t1122334455667788\t8877665544332211\t") !=
					std::string::npos &&
				rows[6U].starts_with("T3PXH\t2\t5\tna\t0\t") &&
				rows[7U].starts_with("T3PXH\t2\t6\t90\t1\t") &&
				rows[8U].starts_with("T3PXH\t2\t7\t91\t1\t"),
			"T3PXH2 records every successful source publication independent of host presentability"
		);
	}
	{
		std::ifstream manifest{rgb_root / "T3RGB1.tsv"};
		std::vector<std::string> rows{};
		for(std::string row; std::getline(manifest, row);) {
			rows.push_back(std::move(row));
		}
		expect(
			rows.size() == 9U && rows[0U] ==
				"stream\tschema\tpublication\tlogical_tick\tcoalesced_ticks\t"
				"overlay\treduced_flash\tsource_fnv64\traster_fnv64\t"
				"tram_fnv64\tshown_page\traster_mode\tscroll_y\ttone\t"
				"rgb_fnv64\tfile" &&
				rows[1U].starts_with("T3RGB\t1\t0\t17\t1\t0\t0\t") &&
				rows[1U].ends_with("\tframe-00000000.ppm") &&
				rows[6U].starts_with("T3RGB\t1\t5\tna\t0\t") &&
				rows[7U].starts_with("T3RGB\t1\t6\t90\t1\t") &&
				rows[8U].starts_with("T3RGB\t1\t7\t91\t1\t"),
			"T3RGB1 records one exact frame identity for every successful publication"
		);
		std::ifstream first{
			rgb_root / "frame-00000000.ppm", std::ios::binary
		};
		const std::vector<std::uint8_t> bytes{
			std::istreambuf_iterator<char>(first),
			std::istreambuf_iterator<char>()
		};
		constexpr std::string_view ppm_header{"P6\n640 400\n255\n"};
		expect(
			bytes.size() == ppm_header.size() + (pixel_count * 3U) &&
			std::equal(ppm_header.begin(), ppm_header.end(), bytes.begin()) &&
			bytes[ppm_header.size() + 0U] == 255U &&
			bytes[ppm_header.size() + 1U] == 0U &&
			bytes[ppm_header.size() + 2U] == 136U &&
			bytes[bytes.size() - 3U] == 255U &&
			bytes[bytes.size() - 2U] == 0U &&
			bytes[bytes.size() - 1U] == 136U,
			"T3RGB1 PPM preserves every exact RGB byte from first through final pixel"
		);
		expect(
			std::filesystem::file_size(rgb_root / "frame-00000001.ppm") ==
				bytes.size() &&
				std::filesystem::file_size(rgb_root / "frame-00000007.ppm") ==
					bytes.size() &&
				!std::filesystem::exists(rgb_root / "frame-00000008.ppm") &&
				!std::filesystem::exists(rgb_root / "frame-00000008.ppm.tmp"),
			"failed source conversion emits no raw frame and later publication stays contiguous"
		);
	}
	{
		const auto rgb_only_root = oracle_root / "rgb-only";
		auto created = SdlPlatform::create(SdlPlatformConfig{
			.title = "th03 SDL RGB-only oracle test",
			.initial_width = 640U,
			.initial_height = 400U,
			.hidden = true,
			.oracle_rgb_directory = rgb_only_root.string(),
		});
		expect(created && (*created)->oracle_enabled(),
			"RGB-only capture preserves every source publication instead of host catch-up coalescing");
	}
	{
		const auto reused = SdlPlatform::create(SdlPlatformConfig{
			.title = "th03 SDL reused RGB oracle test",
			.initial_width = 640U,
			.initial_height = 400U,
			.hidden = true,
			.oracle_rgb_directory = (oracle_root / "rgb-only").string(),
		});
		expect(
			!reused && reused.error().code ==
				SdlPlatformErrorCode::oracle_rgb_open_failed,
			"RGB capture rejects a nonempty directory instead of mixing independent runs"
		);
	}
	std::filesystem::remove_all(oracle_root, remove_error);

	{
		auto recreated = SdlPlatform::create(SdlPlatformConfig{
			.title = "th03 SDL dummy recreate",
			.hidden = true,
		});
		expect(static_cast<bool>(recreated),
			"platform can be created again after complete teardown");
	}
	expect(SDL_WasInit(0U) == 0U,
		"second platform destruction also releases SDL completely");
}

void test_native_mapper_runtime_lease()
{
	auto created = SdlPlatform::create(SdlPlatformConfig{
		.title = "th03 native mapper lifetime",
		.hidden = true,
	});
	expect(static_cast<bool>(created),
		"platform is created for the native mapper lifetime gate");
	if(!created) {
		return;
	}
	auto platform = std::move(*created);
	th03::platform::SdlInputBindings bindings{};
	bindings.keyboard[0U] = {
		.scancode = SDL_SCANCODE_Z,
		.player = th03::platform::SdlInputPlayer::p1,
		.action = th03::platform::SdlLogicalAction::shot,
	};
	bindings.keyboard_count = 1U;
	for(auto &player : bindings.gamepad_players) {
		player = th03::platform::SdlInputPlayer::p1;
	}
	auto mapper_created = th03::platform::SdlInputMapper::create(
		bindings,
		static_cast<SDL_WindowID>(platform->window_id()),
		th03::platform::native_sdl_gamepad_backend()
	);
	expect(static_cast<bool>(mapper_created),
		"native mapper composes directly with the owning platform window identity");
	if(!mapper_created) {
		return;
	}
	{
		auto mapper = std::move(*mapper_created);
		bool lease_checked = false;
		const auto timestamp = th03::platform::sdl_ticks_now_ns();
		auto scheduler = th03::app::TickInputScheduler::create(
			{1U, 1U}, timestamp
		);
		expect(static_cast<bool>(scheduler),
			"native event-pump scheduler is created");
		if(scheduler) {
			SDL_Event escape{};
			escape.type = SDL_EVENT_KEY_DOWN;
			escape.key.timestamp = timestamp;
			escape.key.windowID = static_cast<SDL_WindowID>(
				platform->window_id()
			);
			escape.key.scancode = SDL_SCANCODE_ESCAPE;
			escape.key.down = true;
			expect(SDL_PushEvent(&escape),
				"Escape event is injected into the real SDL queue");
			const auto pumped = mapper.pump_events(*scheduler, true);
			const auto due = scheduler->pop_due(timestamp);
			expect(pumped && pumped->events_consumed >= 1U &&
				pumped->physical_events_queued == 1U &&
				!pumped->queue_blocked && due && *due &&
				(**due).app_quit_requested &&
				(**due).input.system == 0U,
				"native pump maps Escape to timestamped app quit without gameplay Cancel");

			const auto rapid_timestamp = timestamp + 10U;
			auto rapid_scheduler = th03::app::TickInputScheduler::create(
				{100'000'000U, 1U}, rapid_timestamp
			);
			expect(static_cast<bool>(rapid_scheduler),
				"rapid-tap pump scheduler is created");
			if(rapid_scheduler) {
				const auto already_emitted = rapid_scheduler->pop_due(
					rapid_timestamp
				);
				SDL_Event down{};
				down.type = SDL_EVENT_KEY_DOWN;
				down.key.timestamp = rapid_timestamp;
				down.key.windowID = static_cast<SDL_WindowID>(
					platform->window_id()
				);
				down.key.scancode = SDL_SCANCODE_Z;
				down.key.down = true;
				SDL_Event up = down;
				up.type = SDL_EVENT_KEY_UP;
				up.key.down = false;
				expect(SDL_PushEvent(&down) && SDL_PushEvent(&up),
					"rapid down/up edges enter the real SDL queue after their timestamp boundary");
				const auto rapid = mapper.pump_events(*rapid_scheduler, true);
				const auto next = rapid_scheduler->pop_due(rapid_timestamp + 10U);
				expect(
					already_emitted && *already_emitted &&
						(**already_emitted).input.p1 == 0U && rapid &&
						rapid->physical_events_queued == 2U && next && *next &&
						(**next).input.p1 == 0U,
					"late rapid down/up is deferred atomically to the next unemitted tick"
				);
			}

			const auto burst_timestamp = timestamp + 1U;
			auto burst_scheduler = th03::app::TickInputScheduler::create(
				{1U, 1U}, burst_timestamp
			);
			expect(static_cast<bool>(burst_scheduler),
				"same-boundary burst scheduler is created");
			if(burst_scheduler) {
				for(std::size_t index = 0U;
					index < th03::app::pending_physical_event_capacity + 1U;
					++index) {
					SDL_Event repeated_escape = escape;
					repeated_escape.key.timestamp = burst_timestamp;
					expect(SDL_PushEvent(&repeated_escape),
						"same-boundary Escape burst enters the real SDL queue");
				}
				const auto burst = mapper.pump_events(*burst_scheduler, true);
				const auto burst_due = burst_scheduler->pop_due(burst_timestamp);
				expect(
					burst && burst->physical_events_queued ==
						th03::app::pending_physical_event_capacity + 1U &&
						!burst->queue_blocked && burst_due && *burst_due &&
						(**burst_due).app_quit_requested,
					"capacity-plus-one SDL burst is folded into its one exact logical boundary"
				);
			}

			const auto blocked_timestamp = burst_timestamp + 1U;
			auto blocked_scheduler = th03::app::TickInputScheduler::create(
				{1U, 1U}, blocked_timestamp
			);
			expect(static_cast<bool>(blocked_scheduler),
				"future-history retained-event scheduler is created");
			if(blocked_scheduler) {
				for(std::size_t index = 0U;
					index < th03::app::pending_physical_event_capacity;
					++index) {
					const auto queued = blocked_scheduler->push_event({
						.timestamp_ns = blocked_timestamp + 1U + index,
						.kind = th03::app::PhysicalInputEventKind::source_state_changed,
						.source = 0U,
						.state = {
							.p1 = static_cast<std::uint16_t>(
								(index % 2U) == 0U ? 0x0020U : 0U
							),
						},
					});
					expect(static_cast<bool>(queued),
						"future scheduler history fills every fixed slot");
				}
				SDL_Event retained_escape = escape;
				retained_escape.key.timestamp = blocked_timestamp +
					th03::app::pending_physical_event_capacity + 1U;
				expect(SDL_PushEvent(&retained_escape),
					"one later Escape edge enters the SDL queue");
				const auto blocked = mapper.pump_events(*blocked_scheduler, true);
				expect(blocked && blocked->queue_blocked &&
					blocked->physical_events_queued == 0U,
					"full future history retains the exact polled SDL edge");

				auto moved_mapper = std::move(mapper);
				const auto earlier = blocked_scheduler->pop_due(blocked_timestamp);
				expect(earlier && *earlier && !(**earlier).app_quit_requested,
					"earlier deadline is emitted before retained future input");
				const auto retried = moved_mapper.pump_events(
					*blocked_scheduler, true
				);
				expect(retried && !retried->queue_blocked &&
					retried->events_consumed == 1U &&
					retried->physical_events_queued == 1U,
					"move-constructed mapper retries the retained edge exactly once");
				const auto retained_due = blocked_scheduler->pop_due(
					blocked_timestamp + 1'000'000'000ULL
				);
				expect(retained_due && *retained_due &&
					(**retained_due).app_quit_requested,
					"retained edge reaches its later logical boundary after retry");
				platform.reset();
				expect(
					SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) != 0U,
					"move-constructed native mapper lease keeps SDL alive after its window owner is destroyed"
				);
				lease_checked = true;
			}
		}
		if(!lease_checked) {
			platform.reset();
			expect(
				SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) != 0U,
				"native mapper lease keeps SDL alive after its window owner is destroyed"
			);
		}
	}
	expect(SDL_WasInit(0U) == 0U,
		"last native mapper lease performs final SDL teardown safely");
}

} // namespace

int main()
{
	test_folder_url_projection();
	test_locked_texture_adapter();
	test_dummy_platform_lifecycle_and_present();
	test_native_mapper_runtime_lease();

	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << "All " << checks << " SDL platform checks passed\n";
	return 0;
}
