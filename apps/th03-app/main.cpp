#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL_timer.h>

#include "default_bindings.hpp"

#include "th03/app/classic_cli.hpp"
#include "th03/app/classic_audio_session.hpp"
#include "th03/app/classic_runner.hpp"
#include "th03/app/direct_vs_cli.hpp"
#include "th03/app/direct_vs_launch.hpp"
#include "th03/app/main_direct_vs_runner.hpp"
#include "th03/app/tick_input.hpp"
#include "th03/platform/sdl_input.hpp"
#include "th03/platform/sdl_audio_device.hpp"
#include "th03/platform/sdl_platform.hpp"

namespace {

constexpr std::uint64_t maximum_idle_delay_ns = 1'000'000U;

bool kotohime_zero_red(
	void *,
	const th03::BombFrameStep &,
	std::uint8_t,
	std::uint8_t &red_out
) noexcept
{
	// The donor reads an uninitialized source-local byte here. Zero is an
	// explicit deterministic developer compatibility policy, not a claim about
	// the original runtime value; VS-3 captures must validate any release policy.
	red_out = 0U;
	return true;
}

void print_usage()
{
	std::cout
		<< "usage: th03-app --classic --data <root> --main <MAIN.EXE> "
			"--mainl <MAINL.EXE> --op <OP.EXE> --font <FONT.ROM> "
			"--tick-rate <ticks>/<seconds>\n"
		<< "       [--config <YUME.CFG>] [--score <YUME.NEM>] [--seed n] "
			"[--kotohime-zero-red] [--smoke-ticks n]\n"
		<< "   or: th03-app --vs --data <root> --main <MAIN.EXE> "
			"--font <FONT.ROM> --tick-rate <ticks>/<seconds>\n"
		<< "       [--p1 <character[:first|second]>] "
			"[--p2 <character[:first|second]>] [--rank 0..3] [--seed n] "
			"[--kotohime-zero-red] [--smoke-ticks n]\n"
		<< "characters: reimu mima marisa ellen kotohime kana rikako "
			"chiyuri yumemi\n"
		<< "P1: T/F/H/B, diagonals R/Y/V/N, Z shot, X bomb\n"
		<< "P2: keypad 8/4/6/2, diagonals 7/9/1/3, Left shot, Right bomb\n"
		<< "Classic UI: arrows/keypad, Z/X/Space, Return OK, Escape Cancel\n"
		<< "In --vs, Escape exits. In --classic, Escape is source Cancel and "
			"window close exits. Tick rate is explicit host policy; exact PC-98 "
			"cadence is not yet claimed.\n"
		<< "Classic synthesizes external PMD/EFC content and delivers canonical "
			"PCM when SDL playback is available. OPNA rhythm is deterministically "
			"silent until a caller-supplied ROM seam is exposed.\n"
		<< "Kotohime requires --kotohime-zero-red, an unresolved developer "
			"compatibility policy rather than an authenticity claim.\n";
}

const char *name(th03::app::ClassicRunnerErrorCode code) noexcept
{
	switch(code) {
	case th03::app::ClassicRunnerErrorCode::invalid_request: return "invalid-request";
	case th03::app::ClassicRunnerErrorCode::file_missing: return "file-missing";
	case th03::app::ClassicRunnerErrorCode::file_not_regular: return "file-not-regular";
	case th03::app::ClassicRunnerErrorCode::file_size_failed: return "file-size-failed";
	case th03::app::ClassicRunnerErrorCode::file_size_mismatch: return "file-size-mismatch";
	case th03::app::ClassicRunnerErrorCode::file_open_failed: return "file-open-failed";
	case th03::app::ClassicRunnerErrorCode::file_read_failed: return "file-read-failed";
	case th03::app::ClassicRunnerErrorCode::compiled_content_failed: return "compiled-content-failed";
	case th03::app::ClassicRunnerErrorCode::session_failed: return "session-failed";
	case th03::app::ClassicRunnerErrorCode::invalid_sample_order: return "invalid-sample-order";
	case th03::app::ClassicRunnerErrorCode::runner_not_running: return "runner-not-running";
	case th03::app::ClassicRunnerErrorCode::unexpected_session_status: return "unexpected-session-status";
	case th03::app::ClassicRunnerErrorCode::surface_failed: return "surface-failed";
	}
	return "unknown";
}

const char *name(th03::app::ClassicAudioSessionErrorCode code) noexcept
{
	switch(code) {
	case th03::app::ClassicAudioSessionErrorCode::invalid_config:
		return "invalid-config";
	case th03::app::ClassicAudioSessionErrorCode::invalid_cue:
		return "invalid-cue";
	case th03::app::ClassicAudioSessionErrorCode::invalid_parameter:
		return "invalid-parameter";
	case th03::app::ClassicAudioSessionErrorCode::command_order:
		return "command-order";
	case th03::app::ClassicAudioSessionErrorCode::resource_load_failed:
		return "resource-load-failed";
	case th03::app::ClassicAudioSessionErrorCode::engine_failed:
		return "engine-failed";
	case th03::app::ClassicAudioSessionErrorCode::service_failed:
		return "service-failed";
	case th03::app::ClassicAudioSessionErrorCode::resampler_failed:
		return "resampler-failed";
	case th03::app::ClassicAudioSessionErrorCode::allocation_failed:
		return "allocation-failed";
	}
	return "unknown";
}

const char *name(th03::app::ClassicContentFile file) noexcept
{
	switch(file) {
	case th03::app::ClassicContentFile::none: return "none";
	case th03::app::ClassicContentFile::main_executable: return "main-executable";
	case th03::app::ClassicContentFile::mainl_executable: return "mainl-executable";
	case th03::app::ClassicContentFile::op_executable: return "op-executable";
	case th03::app::ClassicContentFile::font_rom: return "font-rom";
	}
	return "unknown";
}

const char *name(th03::app::ClassicCliErrorCode code) noexcept
{
	switch(code) {
	case th03::app::ClassicCliErrorCode::unknown_option: return "unknown-option";
	case th03::app::ClassicCliErrorCode::missing_value: return "missing-value";
	case th03::app::ClassicCliErrorCode::duplicate_option: return "duplicate-option";
	case th03::app::ClassicCliErrorCode::missing_required_option: return "missing-required-option";
	case th03::app::ClassicCliErrorCode::invalid_tick_rate: return "invalid-tick-rate";
	case th03::app::ClassicCliErrorCode::invalid_seed: return "invalid-seed";
	case th03::app::ClassicCliErrorCode::invalid_smoke_ticks: return "invalid-smoke-ticks";
	}
	return "unknown";
}

const char *name(th03::app::ClassicCliOption option) noexcept
{
	switch(option) {
	case th03::app::ClassicCliOption::none: return "none";
	case th03::app::ClassicCliOption::help: return "help";
	case th03::app::ClassicCliOption::classic: return "classic";
	case th03::app::ClassicCliOption::data: return "data";
	case th03::app::ClassicCliOption::main_executable: return "main";
	case th03::app::ClassicCliOption::mainl_executable: return "mainl";
	case th03::app::ClassicCliOption::op_executable: return "op";
	case th03::app::ClassicCliOption::font_rom: return "font";
	case th03::app::ClassicCliOption::config: return "config";
	case th03::app::ClassicCliOption::score: return "score";
	case th03::app::ClassicCliOption::tick_rate: return "tick-rate";
	case th03::app::ClassicCliOption::seed: return "seed";
	case th03::app::ClassicCliOption::kotohime_zero_red: return "kotohime-zero-red";
	case th03::app::ClassicCliOption::smoke_ticks: return "smoke-ticks";
	}
	return "unknown";
}

const char *name(th03::app::DirectVsLaunchErrorCode code) noexcept
{
	switch(code) {
	case th03::app::DirectVsLaunchErrorCode::invalid_request: return "invalid-request";
	case th03::app::DirectVsLaunchErrorCode::file_missing: return "file-missing";
	case th03::app::DirectVsLaunchErrorCode::file_not_regular: return "file-not-regular";
	case th03::app::DirectVsLaunchErrorCode::file_size_failed: return "file-size-failed";
	case th03::app::DirectVsLaunchErrorCode::file_size_mismatch: return "file-size-mismatch";
	case th03::app::DirectVsLaunchErrorCode::file_open_failed: return "file-open-failed";
	case th03::app::DirectVsLaunchErrorCode::file_read_failed: return "file-read-failed";
	case th03::app::DirectVsLaunchErrorCode::resource_load_failed: return "resource-load-failed";
	case th03::app::DirectVsLaunchErrorCode::compiled_content_failed: return "compiled-content-failed";
	case th03::app::DirectVsLaunchErrorCode::process_failed: return "process-failed";
	}
	return "unknown";
}

const char *name(th03::app::DirectVsFile file) noexcept
{
	switch(file) {
	case th03::app::DirectVsFile::none: return "none";
	case th03::app::DirectVsFile::main_executable: return "main-executable";
	case th03::app::DirectVsFile::font_rom: return "font-rom";
	}
	return "unknown";
}

const char *name(th03::app::ResourceLoadErrorCode code) noexcept
{
	switch(code) {
	case th03::app::ResourceLoadErrorCode::invalid_resource_key: return "invalid-resource-key";
	case th03::app::ResourceLoadErrorCode::duplicate_logical_role: return "duplicate-logical-role";
	case th03::app::ResourceLoadErrorCode::invalid_content_root: return "invalid-content-root";
	case th03::app::ResourceLoadErrorCode::path_escape: return "path-escape";
	case th03::app::ResourceLoadErrorCode::case_mismatch: return "case-mismatch";
	case th03::app::ResourceLoadErrorCode::missing: return "missing";
	case th03::app::ResourceLoadErrorCode::not_directory: return "not-directory";
	case th03::app::ResourceLoadErrorCode::not_regular_file: return "not-regular-file";
	case th03::app::ResourceLoadErrorCode::size_mismatch: return "size-mismatch";
	case th03::app::ResourceLoadErrorCode::checksum_mismatch: return "checksum-mismatch";
	case th03::app::ResourceLoadErrorCode::read_failed: return "read-failed";
	}
	return "unknown";
}

const char *name(th03::app::DirectVsCliErrorCode code) noexcept
{
	switch(code) {
	case th03::app::DirectVsCliErrorCode::unknown_option: return "unknown-option";
	case th03::app::DirectVsCliErrorCode::missing_value: return "missing-value";
	case th03::app::DirectVsCliErrorCode::duplicate_option: return "duplicate-option";
	case th03::app::DirectVsCliErrorCode::missing_required_option: return "missing-required-option";
	case th03::app::DirectVsCliErrorCode::invalid_player_selection: return "invalid-player-selection";
	case th03::app::DirectVsCliErrorCode::invalid_tick_rate: return "invalid-tick-rate";
	case th03::app::DirectVsCliErrorCode::invalid_rank: return "invalid-rank";
	case th03::app::DirectVsCliErrorCode::invalid_seed: return "invalid-seed";
	case th03::app::DirectVsCliErrorCode::kotohime_policy_required: return "kotohime-policy-required";
	case th03::app::DirectVsCliErrorCode::invalid_smoke_ticks: return "invalid-smoke-ticks";
	}
	return "unknown";
}

const char *name(th03::app::DirectVsCliOption option) noexcept
{
	switch(option) {
	case th03::app::DirectVsCliOption::none: return "none";
	case th03::app::DirectVsCliOption::help: return "help";
	case th03::app::DirectVsCliOption::vs: return "vs";
	case th03::app::DirectVsCliOption::data: return "data";
	case th03::app::DirectVsCliOption::main_executable: return "main";
	case th03::app::DirectVsCliOption::font_rom: return "font";
	case th03::app::DirectVsCliOption::player_one: return "p1";
	case th03::app::DirectVsCliOption::player_two: return "p2";
	case th03::app::DirectVsCliOption::tick_rate: return "tick-rate";
	case th03::app::DirectVsCliOption::rank: return "rank";
	case th03::app::DirectVsCliOption::seed: return "seed";
	case th03::app::DirectVsCliOption::kotohime_zero_red: return "kotohime-zero-red";
	case th03::app::DirectVsCliOption::smoke_ticks: return "smoke-ticks";
	}
	return "unknown";
}

const char *name(th03::app::MainDirectVsRunnerErrorCode code) noexcept
{
	switch(code) {
	case th03::app::MainDirectVsRunnerErrorCode::invalid_process: return "invalid-process";
	case th03::app::MainDirectVsRunnerErrorCode::unsupported_input_protocol: return "unsupported-input-protocol";
	case th03::app::MainDirectVsRunnerErrorCode::invalid_sample_order: return "invalid-sample-order";
	case th03::app::MainDirectVsRunnerErrorCode::runner_not_running: return "runner-not-running";
	case th03::app::MainDirectVsRunnerErrorCode::main_process_failed: return "main-process-failed";
	case th03::app::MainDirectVsRunnerErrorCode::unexpected_step_status: return "unexpected-step-status";
	case th03::app::MainDirectVsRunnerErrorCode::unexpected_session_action: return "unexpected-session-action";
	}
	return "unknown";
}

const char *name(th03::app::MainProcessErrorCode code) noexcept
{
	switch(code) {
	case th03::app::MainProcessErrorCode::invalid_config: return "invalid-config";
	case th03::app::MainProcessErrorCode::invalid_runtime_argument: return "invalid-runtime-argument";
	case th03::app::MainProcessErrorCode::resource_set_mismatch: return "resource-set-mismatch";
	case th03::app::MainProcessErrorCode::resource_bytes_missing: return "resource-bytes-missing";
	case th03::app::MainProcessErrorCode::resource_integrity_failed: return "resource-integrity-failed";
	case th03::app::MainProcessErrorCode::asset_decode_failed: return "asset-decode-failed";
	case th03::app::MainProcessErrorCode::enedat_decode_failed: return "enedat-decode-failed";
	case th03::app::MainProcessErrorCode::font_rom_failed: return "font-rom-failed";
	case th03::app::MainProcessErrorCode::gameft_prepare_failed: return "gameft-prepare-failed";
	case th03::app::MainProcessErrorCode::pi_palette_missing: return "pi-palette-missing";
	case th03::app::MainProcessErrorCode::pi_staging_failed: return "pi-staging-failed";
	case th03::app::MainProcessErrorCode::static_startup_failed: return "static-startup-failed";
	case th03::app::MainProcessErrorCode::background_initialization_failed: return "background-initialization-failed";
	case th03::app::MainProcessErrorCode::presenter_setup_failed: return "presenter-setup-failed";
	case th03::app::MainProcessErrorCode::game_create_failed: return "game-create-failed";
	case th03::app::MainProcessErrorCode::presentation_failed: return "presentation-failed";
	case th03::app::MainProcessErrorCode::repeat_startup_failed: return "repeat-startup-failed";
	case th03::app::MainProcessErrorCode::core_operation_failed: return "core-operation-failed";
	}
	return "unknown";
}

const char *name(th03::present::MainRoundStatus status) noexcept
{
	switch(status) {
	case th03::present::MainRoundStatus::ready: return "ready";
	case th03::present::MainRoundStatus::invalid_setup: return "invalid-setup";
	case th03::present::MainRoundStatus::invalid_observation: return "invalid-observation";
	case th03::present::MainRoundStatus::background_failed: return "background-failed";
	case th03::present::MainRoundStatus::bosses_failed: return "bosses-failed";
	case th03::present::MainRoundStatus::entities_failed: return "entities-failed";
	case th03::present::MainRoundStatus::effects_failed: return "effects-failed";
	case th03::present::MainRoundStatus::extra_charge_failed: return "extra-charge-failed";
	case th03::present::MainRoundStatus::players_bullets_failed: return "players-bullets-failed";
	case th03::present::MainRoundStatus::bombs_failed: return "bombs-failed";
	case th03::present::MainRoundStatus::warning_gauge_failed: return "warning-gauge-failed";
	case th03::present::MainRoundStatus::hud_result_failed: return "hud-result-failed";
	case th03::present::MainRoundStatus::page_state_mismatch: return "page-state-mismatch";
	case th03::present::MainRoundStatus::page_transaction_failed: return "page-transaction-failed";
	case th03::present::MainRoundStatus::repeat_startup_failed: return "repeat-startup-failed";
	}
	return "unknown";
}

const char *name(th03::platform::SdlPlatformErrorCode code) noexcept
{
	switch(code) {
	case th03::platform::SdlPlatformErrorCode::invalid_config: return "invalid-config";
	case th03::platform::SdlPlatformErrorCode::already_initialized: return "already-initialized";
	case th03::platform::SdlPlatformErrorCode::initialization_failed: return "initialization-failed";
	case th03::platform::SdlPlatformErrorCode::window_renderer_failed: return "window-renderer-failed";
	case th03::platform::SdlPlatformErrorCode::window_setup_failed: return "window-setup-failed";
	case th03::platform::SdlPlatformErrorCode::logical_presentation_failed: return "logical-presentation-failed";
	case th03::platform::SdlPlatformErrorCode::texture_failed: return "texture-failed";
	case th03::platform::SdlPlatformErrorCode::render_output_failed: return "render-output-failed";
	case th03::platform::SdlPlatformErrorCode::texture_lock_failed: return "texture-lock-failed";
	case th03::platform::SdlPlatformErrorCode::invalid_texture_pitch: return "invalid-texture-pitch";
	case th03::platform::SdlPlatformErrorCode::surface_conversion_failed: return "surface-conversion-failed";
	case th03::platform::SdlPlatformErrorCode::render_failed: return "render-failed";
	}
	return "unknown";
}

const char *name(th03::platform::SdlAudioDeviceErrorCode code) noexcept
{
	switch(code) {
	case th03::platform::SdlAudioDeviceErrorCode::invalid_config:
		return "invalid-config";
	case th03::platform::SdlAudioDeviceErrorCode::runtime_unavailable:
		return "runtime-unavailable";
	case th03::platform::SdlAudioDeviceErrorCode::audio_initialization_failed:
		return "audio-initialization-failed";
	case th03::platform::SdlAudioDeviceErrorCode::stream_open_failed:
		return "stream-open-failed";
	case th03::platform::SdlAudioDeviceErrorCode::queue_query_failed:
		return "queue-query-failed";
	case th03::platform::SdlAudioDeviceErrorCode::invalid_queued_bytes:
		return "invalid-queued-bytes";
	case th03::platform::SdlAudioDeviceErrorCode::queue_clear_failed:
		return "queue-clear-failed";
	case th03::platform::SdlAudioDeviceErrorCode::invalid_frame_count:
		return "invalid-frame-count";
	case th03::platform::SdlAudioDeviceErrorCode::submit_failed:
		return "submit-failed";
	case th03::platform::SdlAudioDeviceErrorCode::pause_failed:
		return "pause-failed";
	case th03::platform::SdlAudioDeviceErrorCode::resume_failed:
		return "resume-failed";
	case th03::platform::SdlAudioDeviceErrorCode::allocation_failed:
		return "allocation-failed";
	}
	return "unknown";
}

const char *name(th03::platform::SdlInputMapperErrorCode code) noexcept
{
	switch(code) {
	case th03::platform::SdlInputMapperErrorCode::invalid_bindings: return "invalid-bindings";
	case th03::platform::SdlInputMapperErrorCode::runtime_unavailable: return "runtime-unavailable";
	case th03::platform::SdlInputMapperErrorCode::malformed_event: return "malformed-event";
	case th03::platform::SdlInputMapperErrorCode::gamepad_capacity: return "gamepad-capacity";
	case th03::platform::SdlInputMapperErrorCode::gamepad_open_failed: return "gamepad-open-failed";
	case th03::platform::SdlInputMapperErrorCode::tick_input: return "tick-input";
	}
	return "unknown";
}

const char *name(th03::app::TickInputError code) noexcept
{
	switch(code) {
	case th03::app::TickInputError::invalid_rate: return "invalid-rate";
	case th03::app::TickInputError::timestamp_regressed: return "timestamp-regressed";
	case th03::app::TickInputError::event_already_due: return "event-already-due";
	case th03::app::TickInputError::event_queue_full: return "event-queue-full";
	case th03::app::TickInputError::invalid_event_kind: return "invalid-event-kind";
	case th03::app::TickInputError::invalid_source: return "invalid-source";
	case th03::app::TickInputError::invalid_input_bits: return "invalid-input-bits";
	case th03::app::TickInputError::deadline_overflow: return "deadline-overflow";
	}
	return "unknown";
}

int fail(const char *stage, const th03::app::DirectVsCliError &error)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< name(error.code) << "\noption=" << name(error.option)
		<< "\nargument-index="
		<< error.argument_index << '\n';
	return 1;
}

int fail(const char *stage, const th03::app::ClassicCliError &error)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< name(error.code) << "\noption=" << name(error.option)
		<< "\nargument-index=" << error.argument_index << '\n';
	return 1;
}

int fail(const char *stage, const th03::app::MainDirectVsRunnerError &error)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< name(error.code) << '\n';
	if(error.main_process_error) {
		std::cerr << "main-process-code="
			<< name(error.main_process_error->code) << '\n';
		if(error.main_process_error->code ==
			th03::app::MainProcessErrorCode::presentation_failed) {
			std::cerr << "presentation-status="
				<< name(error.main_process_error->presentation_status) << '\n';
		}
	}
	return 1;
}

int fail(const char *stage, const th03::platform::SdlPlatformError &error)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< name(error.code) << '\n';
	if(error.sdl_detail[0] != '\0') {
		std::cerr << "sdl-detail=" << error.sdl_detail.data() << '\n';
	}
	return 1;
}

int fail(const char *stage, const th03::platform::SdlInputMapperError &error)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< name(error.code) << '\n';
	if(error.tick_input) {
		std::cerr << "tick-input-code=" << name(*error.tick_input) << '\n';
	}
	return 1;
}

int fail_content(const th03::app::DirectVsLaunchError &error)
{
	std::cerr << "status=error\nstage=content\ncode=" << name(error.code)
		<< "\nfile=" << name(error.file) << '\n';
	if(error.resource_error) {
		std::cerr << "resource-code=" << name(error.resource_error->code) << '\n';
		const auto spec = th03::app::main_resource_spec(
			error.resource_error->key
		);
		std::cerr << "resource-role="
			<< (spec ? spec->logical_role : std::string{"invalid-key"}) << '\n';
	}
	if(error.compiled_content_error) {
		std::cerr << "compiled-code="
			<< static_cast<unsigned int>(error.compiled_content_error->code)
			<< "\ncompiled-item="
			<< static_cast<unsigned int>(error.compiled_content_error->item)
			<< "\ncompiled-offset=" << error.compiled_content_error->offset << '\n';
	}
	if(error.process_error) {
		std::cerr << "process-code="
			<< static_cast<unsigned int>(error.process_error->code) << '\n';
	}
	return 1;
}

int fail(const char *stage, const th03::app::ClassicRunnerError &error)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< name(error.code) << "\nfile=" << name(error.file)
		<< "\ndetail-code=" << static_cast<unsigned int>(error.detail_code)
		<< '\n';
	if(error.resource_error) {
		std::cerr << "resource-code=" << name(error.resource_error->code) << '\n';
		const auto spec = th03::app::main_resource_spec(error.resource_error->key);
		std::cerr << "resource-role="
			<< (spec ? spec->logical_role : std::string{"invalid-key"}) << '\n';
	}
	if(error.compiled_content_error) {
		std::cerr << "compiled-code=" << static_cast<unsigned int>(
			error.compiled_content_error->code
		) << "\ncompiled-item=" << static_cast<unsigned int>(
			error.compiled_content_error->item
		) << "\ncompiled-offset=" << error.compiled_content_error->offset << '\n';
	}
	if(error.process_error) {
		std::cerr << "process-code=" << static_cast<unsigned int>(
			error.process_error->code
		) << "\npresentation-status=" << static_cast<unsigned int>(
			error.process_error->presentation_status
		) << '\n';
		if(error.process_error->core_error) {
			std::cerr << "core-code=" << static_cast<unsigned int>(
				error.process_error->core_error->code
			) << '\n';
		}
	}
	return 1;
}

int fail(const char *stage, const th03::app::ClassicAudioSessionError &error)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< name(error.code) << "\ncommand="
		<< static_cast<unsigned int>(error.command.type) << "\ncue="
		<< static_cast<unsigned int>(error.command.cue) << "\nparameter="
		<< error.command.parameter << '\n';
	if(error.resource_error) {
		std::cerr << "resource-code=" << name(error.resource_error->code) << '\n';
		const auto spec = th03::app::main_resource_spec(error.resource_error->key);
		std::cerr << "resource-role="
			<< (spec ? spec->logical_role : std::string{"invalid-key"}) << '\n';
	}
	if(error.engine_error) {
		std::cerr << "engine-code=" << static_cast<unsigned int>(
			error.engine_error->code
		) << '\n';
	}
	if(error.service_error) {
		std::cerr << "service-code=" << static_cast<unsigned int>(
			error.service_error->code
		) << '\n';
	}
	if(error.resampler_error) {
		std::cerr << "resampler-code=" << static_cast<unsigned int>(
			error.resampler_error->code
		) << '\n';
	}
	return 1;
}

void warn_audio_device(
	const char *stage,
	const th03::platform::SdlAudioDeviceError &error
)
{
	std::cerr << "status=warning\nstage=" << stage << "\ncode="
		<< name(error.code) << '\n';
	if(error.sdl_detail[0] != '\0') {
		std::cerr << "sdl-detail=" << error.sdl_detail.data() << '\n';
	}
}

template <typename Error>
int fail(const char *stage, const Error &error)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< static_cast<unsigned int>(error.code) << '\n';
	return 1;
}

int fail_tick(th03::app::TickInputError error)
{
	std::cerr << "status=error\nstage=tick-input\ncode="
		<< name(error) << '\n';
	return 1;
}

} // namespace

namespace {

int run_direct_vs(th03::app::DirectVsCliRequest parsed)
{
	if(parsed.kotohime_zero_red) {
		parsed.launch.process.kotohime_compatibility = {
			.red_provider = &kotohime_zero_red,
		};
	}
	auto process = th03::app::load_direct_vs_process(std::move(parsed.launch));
	if(!process) {
		return fail_content(process.error());
	}
	auto runner = th03::app::MainDirectVsRunner::create(std::move(*process));
	if(!runner) {
		return fail("runner-create", runner.error());
	}

	auto platform = th03::platform::SdlPlatform::create({});
	if(!platform) {
		return fail("platform-create", platform.error());
	}
	auto mapper = th03::platform::SdlInputMapper::create(
		th03::app::detail::classic_direct_vs_bindings(),
		static_cast<SDL_WindowID>((*platform)->window_id()),
		th03::platform::native_sdl_gamepad_backend()
	);
	if(!mapper) {
		return fail("input-create", mapper.error());
	}
	auto scheduler = th03::app::TickInputScheduler::create(
		parsed.tick_rate,
		th03::platform::sdl_ticks_now_ns()
	);
	if(!scheduler) {
		return fail_tick(scheduler.error());
	}

	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	const auto pixel_span = std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(pixels.data(), pixels.size());
	auto surface = (*runner)->compose_shown_surface(pixel_span);
	if(!surface) {
		return fail("compose", surface.error());
	}
	auto presented = (*platform)->present(*surface);
	if(!presented) {
		return fail("present", presented.error());
	}

	bool retry_blocked_event = false;
	bool presentation_pending = false;
	std::uint64_t process_ticks = 0U;
	while((*runner)->status() ==
		th03::app::MainDirectVsRunnerStatus::running) {
		const auto cutoff = th03::platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto result = mapper->pump_events(*scheduler, true);
			if(!result) {
				return fail("input-pump", result.error());
			}
			retry_blocked_event = result->queue_blocked;
		}

		bool smoke_complete = false;
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return fail_tick(due.error());
			}
			if(!*due) {
				break;
			}
			auto consumed = (*runner)->consume(**due);
			if(!consumed) {
				return fail("runner", consumed.error());
			}
			presentation_pending =
				presentation_pending || consumed->surface_changed;
			if(consumed->process_advanced) {
				process_ticks++;
			}
			// A retained SDL event must be retried as soon as this logical
			// boundary frees scheduler history, before any frame is presented.
			if(retry_blocked_event) {
				auto retried = mapper->pump_events(*scheduler, true);
				if(!retried) {
					return fail("input-pump", retried.error());
				}
				retry_blocked_event = retried->queue_blocked;
			}
			if(parsed.smoke_ticks && process_ticks >= *parsed.smoke_ticks) {
				smoke_complete = true;
				break;
			}
			if(consumed->status !=
				th03::app::MainDirectVsRunnerStatus::running) {
				break;
			}
		}

		if((*runner)->status() ==
			th03::app::MainDirectVsRunnerStatus::quit_requested) {
			return 0;
		}
		const bool session_complete = (*runner)->status() ==
			th03::app::MainDirectVsRunnerStatus::session_complete;
		bool presented_this_iteration = false;
		if(presentation_pending &&
			(!retry_blocked_event || session_complete || smoke_complete)) {
			surface = (*runner)->compose_shown_surface(pixel_span);
			if(!surface) {
				return fail("compose", surface.error());
			}
			presented = (*platform)->present(*surface);
			if(!presented) {
				return fail("present", presented.error());
			}
			presentation_pending = false;
			presented_this_iteration = true;
		}
		if(session_complete) {
			return 0;
		}
		if(smoke_complete) {
			return 0;
		}

		if(presented_this_iteration) {
			continue;
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = th03::platform::sdl_ticks_now_ns();
		if(deadline && *deadline > now) {
			SDL_DelayPrecise(std::min(*deadline - now, maximum_idle_delay_ns));
		}
	}
	return 0;
}

int run_classic(th03::app::ClassicCliRequest parsed)
{
	if(parsed.kotohime_zero_red) {
		parsed.launch.kotohime_compatibility = {
			.red_provider = &kotohime_zero_red,
		};
	}
	const auto data_root = parsed.launch.content.data_root;
	parsed.launch.fm_available = true;
	auto runner = th03::app::ClassicRunner::load(std::move(parsed.launch));
	if(!runner) {
		return fail("classic-load", runner.error());
	}
	auto created_session = th03::app::ClassicAudioSession::create({
		.data_root = data_root,
		.tick_rate = parsed.tick_rate,
	});
	if(!created_session) {
		return fail("audio-session-create", created_session.error());
	}
	auto audio_session = std::move(*created_session);

	auto platform = th03::platform::SdlPlatform::create({});
	if(!platform) {
		return fail("platform-create", platform.error());
	}
	std::optional<th03::platform::SdlAudioDevice> audio_device{};
	auto created_device = th03::platform::SdlAudioDevice::create();
	if(created_device) {
		audio_device.emplace(std::move(*created_device));
	} else {
		warn_audio_device("audio-device-create", created_device.error());
	}
	auto mapper = th03::platform::SdlInputMapper::create(
		th03::app::detail::classic_direct_vs_bindings(),
		static_cast<SDL_WindowID>((*platform)->window_id()),
		th03::platform::native_sdl_gamepad_backend()
	);
	if(!mapper) {
		return fail("input-create", mapper.error());
	}
	auto scheduler = th03::app::TickInputScheduler::create(
		parsed.tick_rate,
		th03::platform::sdl_ticks_now_ns()
	);
	if(!scheduler) {
		return fail_tick(scheduler.error());
	}

	const auto drain_audio = [&]() -> std::expected<
		void,
		th03::app::ClassicAudioSessionError
	> {
		while(const auto command = (*runner)->take_audio_command()) {
			if(auto applied = audio_session->apply(*command); !applied) {
				return std::unexpected(applied.error());
			}
		}
		return {};
	};
	if(auto drained = drain_audio(); !drained) {
		return fail("audio-command", drained.error());
	}

	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	const auto pixel_span = std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(pixels.data(), pixels.size());
	auto surface = (*runner)->compose_shown_surface(pixel_span);
	if(!surface) {
		return fail("compose", surface.error());
	}
	auto presented = (*platform)->present(*surface);
	if(!presented) {
		return fail("present", presented.error());
	}

	bool retry_blocked_event = false;
	bool presentation_pending = false;
	std::uint64_t logical_ticks = 0U;
	while((*runner)->status() == th03::app::ClassicRunnerStatus::running) {
		const auto cutoff = th03::platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto result = mapper->pump_events(*scheduler, false);
			if(!result) {
				return fail("input-pump", result.error());
			}
			retry_blocked_event = result->queue_blocked;
		}

		bool smoke_complete = false;
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return fail_tick(due.error());
			}
			if(!*due) {
				break;
			}
			auto consumed = (*runner)->consume(**due, audio_session->status());
			if(!consumed) {
				std::cerr << "logical-tick=" << logical_ticks << '\n';
				return fail("classic-runner", consumed.error());
			}
			if(auto drained = drain_audio(); !drained) {
				std::cerr << "logical-tick=" << logical_ticks << '\n';
				return fail("audio-command", drained.error());
			}
			auto audio_tick = audio_session->advance_tick();
			if(!audio_tick) {
				std::cerr << "logical-tick=" << logical_ticks << '\n';
				return fail("audio-tick", audio_tick.error());
			}
			if(audio_device) {
				const auto submitted = audio_device->submit(
					audio_tick->canonical_frames
				);
				if(!submitted) {
					// Device loss is final-output state. Keep PMD and scene
					// timelines advancing, but stop attempting delivery.
					warn_audio_device(
						"audio-device-submit", submitted.error()
					);
					audio_device.reset();
				}
			}
			presentation_pending =
				presentation_pending || consumed->surface_changed;
			logical_ticks++;
			if(retry_blocked_event) {
				auto retried = mapper->pump_events(*scheduler, false);
				if(!retried) {
					return fail("input-pump", retried.error());
				}
				retry_blocked_event = retried->queue_blocked;
			}
			if(parsed.smoke_ticks && logical_ticks >= *parsed.smoke_ticks) {
				smoke_complete = true;
				break;
			}
			if(consumed->status != th03::app::ClassicRunnerStatus::running) {
				break;
			}
		}

		if((*runner)->status() ==
			th03::app::ClassicRunnerStatus::quit_requested) {
			return 0;
		}
		const bool session_complete = (*runner)->status() ==
			th03::app::ClassicRunnerStatus::session_complete;
		bool presented_this_iteration = false;
		if(presentation_pending &&
			(!retry_blocked_event || session_complete || smoke_complete)) {
			surface = (*runner)->compose_shown_surface(pixel_span);
			if(!surface) {
				return fail("compose", surface.error());
			}
			presented = (*platform)->present(*surface);
			if(!presented) {
				return fail("present", presented.error());
			}
			presentation_pending = false;
			presented_this_iteration = true;
		}
		if(session_complete || smoke_complete) {
			return 0;
		}
		if(presented_this_iteration) {
			continue;
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = th03::platform::sdl_ticks_now_ns();
		if(deadline && *deadline > now) {
			SDL_DelayPrecise(std::min(*deadline - now, maximum_idle_delay_ns));
		}
	}
	return 0;
}

} // namespace

int main(int argc, char **argv)
{
	std::vector<std::string_view> arguments;
	arguments.reserve(static_cast<std::size_t>(std::max(argc - 1, 0)));
	for(int index = 1; index < argc; index++) {
		arguments.emplace_back(argv[index]);
	}

	const bool classic = std::ranges::find(arguments, "--classic") !=
		arguments.end();
	if(classic) {
		auto parsed = th03::app::parse_classic_cli(arguments);
		if(!parsed) {
			return fail("arguments", parsed.error());
		}
		if(parsed->show_help) {
			print_usage();
			return 0;
		}
		return run_classic(std::move(*parsed));
	}

	auto parsed = th03::app::parse_direct_vs_cli(arguments);
	if(!parsed) {
		return fail("arguments", parsed.error());
	}
	if(parsed->show_help) {
		print_usage();
		return 0;
	}
	return run_direct_vs(std::move(*parsed));
}
