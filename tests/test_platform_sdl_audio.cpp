#include "th03/platform/sdl_audio_device.hpp"
#include "th03/platform/sdl_platform.hpp"

#include "sdl_audio_device_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

#include <SDL3/SDL.h>

namespace {

using th03::audio::PmdCanonicalStereoSample;
using th03::platform::SdlAudioDevice;
using th03::platform::SdlAudioDeviceConfig;
using th03::platform::SdlAudioDeviceErrorCode;
using th03::platform::SdlAudioDeviceTestAccess;
using th03::platform::SdlPlatform;
using th03::platform::SdlPlatformConfig;

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

void test_runtime_preconditions()
{
	expect(SDL_WasInit(0U) == 0U, "SDL starts uninitialized");
	auto unavailable = SdlAudioDevice::create();
	expect(!unavailable, "audio requires the platform runtime");
	expect(
		!unavailable && unavailable.error().code ==
			SdlAudioDeviceErrorCode::runtime_unavailable,
		"missing-runtime error is typed"
	);
	expect(SDL_WasInit(0U) == 0U, "rejected audio leaves SDL untouched");

	auto zero = SdlAudioDevice::create({.maximum_queued_frames = 0U});
	expect(!zero, "zero queue capacity is invalid");
	expect(
		!zero && zero.error().code == SdlAudioDeviceErrorCode::invalid_config,
		"zero-capacity error is typed"
	);
	const auto too_large =
		static_cast<std::size_t>(std::numeric_limits<int>::max()) /
		sizeof(PmdCanonicalStereoSample) + 1U;
	auto oversized = SdlAudioDevice::create({
		.maximum_queued_frames = too_large,
	});
	expect(!oversized, "queue capacity beyond SDL int bytes is invalid");
	expect(SDL_WasInit(0U) == 0U, "invalid config performs no SDL work");
}

void test_dummy_queue_and_lifetime()
{
	auto platform_result = SdlPlatform::create(SdlPlatformConfig{
		.title = "TH03 SDL audio test",
		.hidden = true,
	});
	expect(static_cast<bool>(platform_result), "dummy platform creates");
	if(!platform_result) {
		return;
	}
	auto platform = std::move(*platform_result);
	expect(SDL_WasInit(SDL_INIT_AUDIO) == 0U, "platform does not pre-own audio");

	auto *const environment = SDL_GetEnvironment();
	expect(
		SDL_SetEnvironmentVariable(
			environment, "SDL_AUDIO_DRIVER", "th03-invalid-driver", true
		),
		"test installs an invalid audio driver"
	);
	auto failed_audio = SdlAudioDevice::create({
		.maximum_queued_frames = 4U,
	});
	expect(!failed_audio, "audio initialization failure is reported");
	expect(
		!failed_audio && failed_audio.error().code ==
			SdlAudioDeviceErrorCode::audio_initialization_failed,
		"audio initialization error is typed"
	);
	expect(SDL_WasInit(SDL_INIT_AUDIO) == 0U,
		"failed audio initialization releases its subsystem state");
	expect(SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) != 0U,
		"failed audio initialization preserves the platform runtime");
	expect(
		SDL_SetEnvironmentVariable(environment, "SDL_AUDIO_DRIVER", "dummy", true),
		"test restores the authenticated dummy audio driver"
	);

	auto device_result = SdlAudioDevice::create({
		.maximum_queued_frames = 4U,
	});
	expect(static_cast<bool>(device_result), "dummy playback stream creates");
	if(!device_result) {
		return;
	}
	auto device = std::move(*device_result);
	expect(SDL_WasInit(SDL_INIT_AUDIO) != 0U, "audio subsystem is initialized");
	expect(!device.paused(), "created playback stream is resumed");
	expect(device.maximum_queued_frames() == 4U, "queue cap is retained");

	auto metrics = SdlAudioDeviceTestAccess::metrics(device);
	expect(static_cast<bool>(metrics), "audio metrics query succeeds");
	if(metrics) {
		expect(metrics->device_id != 0U, "playback stream owns a device");
		expect(
			metrics->source_format == static_cast<std::uint32_t>(SDL_AUDIO_S32),
			"source format is native S32"
		);
		expect(metrics->source_channels == 2, "source format is stereo");
		expect(metrics->source_frequency == 48'000, "source rate is canonical");
		expect(metrics->audio_driver == "dummy", "test never falls back from dummy audio");
	}

	auto paused = device.pause();
	expect(static_cast<bool>(paused), "playback pause succeeds");
	expect(device.paused(), "pause state is retained");
	expect(static_cast<bool>(device.pause()), "repeated pause is idempotent");

	const auto telemetry_before_empty = device.telemetry();
	auto empty = device.submit({});
	expect(empty && empty->frames_submitted == 0U, "empty submission is a no-op");
	expect(device.telemetry().frames_submitted ==
		telemetry_before_empty.frames_submitted, "empty submission preserves telemetry");

	const std::array<PmdCanonicalStereoSample, 3U> first{{
		{.left = 1, .right = -1},
		{.left = 2, .right = -2},
		{.left = 3, .right = -3},
	}};
	auto first_submit = device.submit(first);
	expect(first_submit && first_submit->frames_submitted == first.size(),
		"first block is submitted whole");
	expect(first_submit && !first_submit->discontinuity,
		"within-cap submission is continuous");
	auto queued = device.queued_frames();
	expect(queued && *queued == 3U, "paused stream reports exact queued frames");

	const std::array<PmdCanonicalStereoSample, 2U> second{{
		{.left = 4, .right = -4},
		{.left = 5, .right = -5},
	}};
	auto second_submit = device.submit(second);
	expect(second_submit && second_submit->frames_submitted == second.size(),
		"newest block survives stale-queue clear");
	expect(second_submit && second_submit->frames_discarded == 3U,
		"stale queued frames are counted");
	expect(second_submit && second_submit->discontinuity,
		"stale-queue clear is explicit");
	queued = device.queued_frames();
	expect(queued && *queued == 2U, "only newest block remains queued");

	const std::array<PmdCanonicalStereoSample, 6U> oversized{{
		{.left = 10, .right = -10}, {.left = 11, .right = -11},
		{.left = 12, .right = -12}, {.left = 13, .right = -13},
		{.left = 14, .right = -14}, {.left = 15, .right = -15},
	}};
	auto trimmed = device.submit(oversized);
	expect(trimmed && trimmed->frames_submitted == 4U,
		"oversized block retains exactly the newest queue cap");
	expect(trimmed && trimmed->frames_discarded == 4U,
		"old queue and oversized prefix are both counted");
	expect(trimmed && trimmed->discontinuity,
		"oversized replacement is a discontinuity");
	queued = device.queued_frames();
	expect(queued && *queued == 4U, "replacement fills the exact queue cap");

	const auto telemetry = device.telemetry();
	expect(telemetry.frames_submitted == 9U, "submitted-frame telemetry accumulates");
	expect(telemetry.frames_discarded == 7U, "discard telemetry accumulates");
	expect(telemetry.discontinuities == 2U, "discontinuity telemetry accumulates");
	expect(telemetry.failures == 0U, "successful path reports no failures");

	expect(static_cast<bool>(device.resume()), "resume succeeds");
	expect(!device.paused(), "resume state is retained");
	expect(static_cast<bool>(device.resume()), "repeated resume is idempotent");
	expect(static_cast<bool>(device.pause()), "device can pause again before move");

	auto moved = std::move(device);
	expect(device.paused(), "moved-from device is inert");
	expect(device.maximum_queued_frames() == 0U, "moved-from cap is zero");
	auto moved_rejection = device.submit(first);
	expect(!moved_rejection, "moved-from submission is rejected");
	expect(
		!moved_rejection && moved_rejection.error().code ==
			SdlAudioDeviceErrorCode::runtime_unavailable,
		"moved-from error is stable"
	);

	platform.reset();
	expect(SDL_WasInit(SDL_INIT_AUDIO) != 0U,
		"audio lease survives destruction of its window owner");
	expect(SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) != 0U,
		"subordinate lease retains the shared SDL runtime");
	expect(static_cast<bool>(moved.submit({})),
		"audio owner remains usable after platform destruction");
}

void test_sequential_recreation()
{
	expect(SDL_WasInit(0U) == 0U, "final audio lease tears SDL down");
	auto platform = SdlPlatform::create(SdlPlatformConfig{
		.title = "TH03 SDL audio recreate",
		.hidden = true,
	});
	expect(static_cast<bool>(platform), "platform recreates after full teardown");
	if(!platform) {
		return;
	}
	auto device = SdlAudioDevice::create();
	expect(static_cast<bool>(device), "audio recreates after full teardown");
}

} // namespace

int main()
{
	test_runtime_preconditions();
	test_dummy_queue_and_lifetime();
	test_sequential_recreation();

	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " SDL audio checks failed\n";
		return 1;
	}
	std::cout << "all " << checks << " SDL audio checks passed\n";
	return 0;
}
