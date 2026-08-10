#include "th03/platform/sdl_platform.hpp"
#include "th03/platform/sdl_input.hpp"

#include "sdl_platform_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

namespace {

using th03::platform::SdlPlatform;
using th03::platform::SdlPlatformConfig;
using th03::platform::SdlPlatformErrorCode;
using th03::platform::SdlPlatformTestAccess;
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
			std::span<const ComposedPixel, pixel_count>{
				pixels.data(), pixels.size()
			},
			palette,
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

void test_locked_texture_adapter()
{
	SurfaceFixture fixture;
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

	{
		auto created = SdlPlatform::create(SdlPlatformConfig{
			.title = "th03 SDL dummy test",
			.initial_width = 640U,
			.initial_height = 400U,
			.hidden = true,
		});
		expect(static_cast<bool>(created),
			"pinned dummy/software SDL platform is created");
		if(!created) {
			return;
		}
		auto &platform = **created;
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
				metrics->renderer_name == "software",
			"test gate uses the requested dummy video and software renderer without fallback"
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
		const auto first = platform.present(fixture.source());
		expect(
			first && first->destination == th03::app::IntegerLetterboxRect{
				0U, 0U, 640U, 400U, 1U
			},
			"native-size source surface uploads and presents at integer scale one"
		);
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
	}
	expect(SDL_WasInit(0U) == 0U,
		"platform destruction releases all SDL subsystems it owns");

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
		platform.reset();
		expect(
			SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) != 0U,
			"native mapper lease keeps SDL alive after its window owner is destroyed"
		);
	}
	expect(SDL_WasInit(0U) == 0U,
		"last native mapper lease performs final SDL teardown safely");
}

} // namespace

int main()
{
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
