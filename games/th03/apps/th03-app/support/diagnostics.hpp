#pragma once

#include <expected>
#include <iostream>

#include "th03/app/classic/classic_audio_session.hpp"
#include "th03/app/classic/classic_cli.hpp"
#include "th03/app/classic/classic_runner.hpp"
#include "th03/app/direct_vs/direct_vs_cli.hpp"
#include "th03/app/direct_vs/direct_vs_launch.hpp"
#include "th03/app/main/main_direct_vs_runner.hpp"
#include "th03/app/new_classic/content/new_classic_language.hpp"
#include "th03/app/runtime/startup_sequence.hpp"
#include "th03/app/runtime/tick_input.hpp"
#include "th03/platform/sdl_audio_device.hpp"
#include "th03/platform/sdl_input.hpp"
#include "th03/platform/sdl_platform.hpp"

namespace th03::app::launcher_support {

void print_usage();

[[nodiscard]] const char *name(
	NewClassicCutsceneTextPackErrorCode code
) noexcept;

int fail(const char *stage, const DirectVsCliError &error);
int fail(const char *stage, const ClassicCliError &error);
int fail(const char *stage, const StartupSequenceError &error);
int fail(const char *stage, const MainDirectVsRunnerError &error);
int fail(const char *stage, const platform::SdlPlatformError &error);
int fail(const char *stage, const platform::SdlInputMapperError &error);
int fail_content(const DirectVsLaunchError &error);
int fail(const char *stage, const ClassicRunnerError &error);
int fail(const char *stage, const ClassicAudioSessionError &error);
void warn_audio_device(
	const char *stage,
	const platform::SdlAudioDeviceError &error
);
int fail_output_gain(const char *stage, const char *code);
int fail_netplay(const char *stage, const char *code);
int fail_tick(TickInputError error);

[[nodiscard]] std::expected<void, int> refresh_host_presentability(
	platform::SdlPlatform &platform,
	const char *stage
);

template <typename Error>
int fail(const char *stage, const Error &error)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< static_cast<unsigned int>(error.code) << '\n';
	return 1;
}

} // namespace th03::app::launcher_support
