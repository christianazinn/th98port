#include "diagnostics.hpp"

#include <iostream>
#include <string>

#include "th03/app/content/main_compiled_content.hpp"
#include "th03/app/new_classic/beta_features.hpp"

namespace th03::app::launcher_support {

void print_usage()
{
	std::cout
		<< "usage: th03-app --shell|--classic|--new-classic --data <root> "
			"--main <MAIN.EXE> "
			"--mainl <MAINL.EXE> --op <OP.EXE> --font <FONT.ROM>\n"
		<< "       [--config <YUME.CFG>] [--score <YUME.NEM>] "
			"[--replay-root <REPLAY>] [--seed n] "
			"[--language-overlay <TH3EN.DAT>] "
			"[--language-pack <locale.t3lang>] "
			"[--game-text-pack <locale.t3gtl>] "
			"[--cutscene-text-pack <locale.t3ctp>] "
			"[--picture-patch-pack <locale.t3ptp>] "
			"[--rhythm-rom <YM2608.ROM> | --rhythm-pcm <T98RHYTHM.PCM>] "
			"[--tick-rate <ticks>/<seconds>] "
			"[--smoke-ticks n] [--capture-replay <V14.RPY> "
			"[--capture-checkpoint n]]"

#if TH03_BETA_FEATURES
			" [--arrange dopamine-test-v1]"
#endif
			"\n"
		<< "   or: th03-app --vs --data <root> --main <MAIN.EXE> "
			"--font <FONT.ROM>\n"
		<< "       [--p1 <character[:first|second]>] "
			"[--p2 <character[:first|second]>] [--rank 0..3] [--seed n] "
			"[--tick-rate <ticks>/<seconds>] "
			"[--smoke-ticks n] [--netplay-host <port> | "
			"--netplay-join <IPv4-or-hostname:port>] "
			"[--netplay-input-delay <0..6>] "
			"[--replay-root <directory>] "
			"[--netplay-replay-output <V14.RPY>]\n"
		<< "characters: reimu mima marisa ellen kotohime kana rikako "
			"chiyuri yumemi\n"
		<< "P1: T/F/H/B, diagonals R/Y/V/N, Z shot, X bomb\n"
		<< "P2: keypad 8/4/6/2, diagonals 7/9/1/3, Left shot, Right bomb\n"
		<< "Netplay local player: arrows, Z shot, X bomb, Left Shift charge\n"
		<< "TH03 UI: arrows/keypad, Z/X/Space, Return OK, Escape Cancel\n"
		<< "--shell runs startup and boots directly into New Classic. "
			"New Classic Quit exits; Option -> System can temporarily launch "
			"sealed Classic, whose Quit returns to a fresh New Classic title. "
			"In New Classic gameplay, Escape opens "
			"Pause, R restarts, and Q exits without saving. During New Classic "
			"replay playback, Escape aborts and held physical Z fast-forwards; "
			"after a prior clear, physical Z also accelerates Staff. In local "
			"--vs, Escape exits; in netplay it requests synchronized Pause. "
			"In Classic, Escape remains source Cancel and "
			"window close exits. The default cadence is nominal PC-9801 24 kHz "
			"(526315/9328 Hz); --tick-rate overrides it.\n"
		<< "PoDD Port synthesizes external PMD/EFC content and delivers canonical "
			"PCM when SDL playback is available. Supply an exact 8 KiB YM2608 "
			"ADPCM-A image with --rhythm-rom, or an authenticated decoded T98 "
			"profile with --rhythm-pcm; otherwise rhythm is deterministically "
			"suppressed.\n"
		<< "PoDD Port stores sealed YUME.CFG/YUME.NEM, New Classic PODD.CFG, "
			"TH3KEY.CFG, TH3JOY.CFG, TH3DEV.CFG, YUME.NEW.NEM, and REPLAY "
			"directory under --data by default; --config, --score, and "
			"--replay-root override them for read-only installs.\n"
		<< "New Classic enables its persistent English option only when the "
			"authenticated Replay Patch 0.4.18 TH3EN.DAT is supplied with "
			"--language-overlay. Classic remains Japanese.\n"
		<< "The normal product launch also requires the original root TOUHOU.DAT "
			"ZUN Soft logo resource under --data.\n"
		<< "Kotohime's source-uninitialized late Bomb local is deterministically "
			"normalized from the adjacent Kana fade routine.\n";
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
	case th03::app::ClassicRunnerErrorCode::replay_patch_assets_failed: return "replay-patch-assets-failed";
	case th03::app::ClassicRunnerErrorCode::language_pack_failed: return "language-pack-failed";
	case th03::app::ClassicRunnerErrorCode::game_text_pack_failed: return "game-text-pack-failed";
	case th03::app::ClassicRunnerErrorCode::cutscene_text_pack_failed: return "cutscene-text-pack-failed";
	case th03::app::ClassicRunnerErrorCode::picture_patch_pack_failed: return "picture-patch-pack-failed";
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
	case th03::app::ClassicContentFile::replay_patch_assets: return "azinn-dat";
	case th03::app::ClassicContentFile::language_pack: return "language-pack";
	case th03::app::ClassicContentFile::game_text_pack: return "game-text-pack";
	case th03::app::ClassicContentFile::cutscene_text_pack:
		return "cutscene-text-pack";
	case th03::app::ClassicContentFile::picture_patch_pack:
		return "picture-patch-pack";
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
	case th03::app::ClassicCliErrorCode::invalid_capture_checkpoint:
		return "invalid-capture-checkpoint";
	case th03::app::ClassicCliErrorCode::invalid_capture_route:
		return "invalid-capture-route";
	case th03::app::ClassicCliErrorCode::invalid_arrange:
		return "invalid-arrange";
	}
	return "unknown";
}

const char *name(th03::app::ClassicCliOption option) noexcept
{
	switch(option) {
	case th03::app::ClassicCliOption::none: return "none";
	case th03::app::ClassicCliOption::help: return "help";
	case th03::app::ClassicCliOption::classic: return "classic";
	case th03::app::ClassicCliOption::new_classic: return "new-classic";
	case th03::app::ClassicCliOption::shell: return "shell";
	case th03::app::ClassicCliOption::arrange: return "arrange";
	case th03::app::ClassicCliOption::data: return "data";
	case th03::app::ClassicCliOption::main_executable: return "main";
	case th03::app::ClassicCliOption::mainl_executable: return "mainl";
	case th03::app::ClassicCliOption::op_executable: return "op";
	case th03::app::ClassicCliOption::font_rom: return "font";
	case th03::app::ClassicCliOption::language_overlay:
		return "language-overlay";
	case th03::app::ClassicCliOption::language_pack:
		return "language-pack";
	case th03::app::ClassicCliOption::game_text_pack:
		return "game-text-pack";
	case th03::app::ClassicCliOption::cutscene_text_pack:
		return "cutscene-text-pack";
	case th03::app::ClassicCliOption::picture_patch_pack:
		return "picture-patch-pack";
	case th03::app::ClassicCliOption::rhythm_rom: return "rhythm-rom";
	case th03::app::ClassicCliOption::rhythm_pcm: return "rhythm-pcm";
	case th03::app::ClassicCliOption::config: return "config";
	case th03::app::ClassicCliOption::score: return "score";
	case th03::app::ClassicCliOption::replay_root: return "replay-root";
	case th03::app::ClassicCliOption::tick_rate: return "tick-rate";
	case th03::app::ClassicCliOption::seed: return "seed";
	case th03::app::ClassicCliOption::smoke_ticks: return "smoke-ticks";
	case th03::app::ClassicCliOption::capture_replay: return "capture-replay";
	case th03::app::ClassicCliOption::capture_checkpoint:
		return "capture-checkpoint";
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
	case th03::app::DirectVsCliErrorCode::invalid_smoke_ticks: return "invalid-smoke-ticks";
	case th03::app::DirectVsCliErrorCode::invalid_netplay_endpoint: return "invalid-netplay-endpoint";
	case th03::app::DirectVsCliErrorCode::invalid_netplay_input_delay: return "invalid-netplay-input-delay";
	case th03::app::DirectVsCliErrorCode::invalid_replay_output: return "invalid-replay-output";
	case th03::app::DirectVsCliErrorCode::conflicting_netplay_mode: return "conflicting-netplay-mode";
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
	case th03::app::DirectVsCliOption::smoke_ticks: return "smoke-ticks";
	case th03::app::DirectVsCliOption::netplay_host: return "netplay-host";
	case th03::app::DirectVsCliOption::netplay_join: return "netplay-join";
	case th03::app::DirectVsCliOption::netplay_input_delay:
		return "netplay-input-delay";
	case th03::app::DirectVsCliOption::replay_root: return "replay-root";
	case th03::app::DirectVsCliOption::netplay_replay_output: return "netplay-replay-output";
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
	case th03::app::MainProcessErrorCode::audio_command_pending: return "audio-command-pending";
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
	case th03::platform::SdlPlatformErrorCode::present_vsync_failed: return "present-vsync-failed";
	case th03::platform::SdlPlatformErrorCode::window_presentation_failed:
		return "window-presentation-failed";
	case th03::platform::SdlPlatformErrorCode::window_presentation_rollback_failed:
		return "window-presentation-rollback-failed";
	case th03::platform::SdlPlatformErrorCode::oracle_log_open_failed: return "oracle-log-open-failed";
	case th03::platform::SdlPlatformErrorCode::oracle_log_write_failed: return "oracle-log-write-failed";
	case th03::platform::SdlPlatformErrorCode::oracle_rgb_open_failed: return "oracle-rgb-open-failed";
	case th03::platform::SdlPlatformErrorCode::oracle_rgb_write_failed: return "oracle-rgb-write-failed";
	case th03::platform::SdlPlatformErrorCode::open_folder_failed: return "open-folder-failed";
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
	case th03::app::TickInputError::invalid_catch_up_policy:
		return "invalid-catch-up-policy";
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

const char *name(th03::app::StartupSequenceErrorCode code) noexcept
{
	switch(code) {
	case th03::app::StartupSequenceErrorCode::invalid_path: return "invalid-path";
	case th03::app::StartupSequenceErrorCode::file_status_failed:
		return "file-status-failed";
	case th03::app::StartupSequenceErrorCode::file_missing: return "file-missing";
	case th03::app::StartupSequenceErrorCode::file_not_regular:
		return "file-not-regular";
	case th03::app::StartupSequenceErrorCode::file_size_failed:
		return "file-size-failed";
	case th03::app::StartupSequenceErrorCode::file_size_mismatch:
		return "file-size-mismatch";
	case th03::app::StartupSequenceErrorCode::file_open_failed:
		return "file-open-failed";
	case th03::app::StartupSequenceErrorCode::file_read_failed:
		return "file-read-failed";
	case th03::app::StartupSequenceErrorCode::digest_mismatch:
		return "digest-mismatch";
	case th03::app::StartupSequenceErrorCode::asset_decode_failed:
		return "asset-decode-failed";
	case th03::app::StartupSequenceErrorCode::asset_profile_mismatch:
		return "asset-profile-mismatch";
	case th03::app::StartupSequenceErrorCode::asset_prepare_failed:
		return "asset-prepare-failed";
	case th03::app::StartupSequenceErrorCode::allocation_failed:
		return "allocation-failed";
	case th03::app::StartupSequenceErrorCode::draw_failed: return "draw-failed";
	}
	return "unknown";
}

const char *name(th03::app::NewClassicMessagePackErrorCode code) noexcept
{
	switch(code) {
	case th03::app::NewClassicMessagePackErrorCode::invalid_path:
		return "invalid-path";
	case th03::app::NewClassicMessagePackErrorCode::source_status:
		return "source-status";
	case th03::app::NewClassicMessagePackErrorCode::source_not_regular:
		return "source-not-regular";
	case th03::app::NewClassicMessagePackErrorCode::source_size:
		return "source-size";
	case th03::app::NewClassicMessagePackErrorCode::source_open:
		return "source-open";
	case th03::app::NewClassicMessagePackErrorCode::source_read:
		return "source-read";
	case th03::app::NewClassicMessagePackErrorCode::header_shape:
		return "header-shape";
	case th03::app::NewClassicMessagePackErrorCode::schema_mismatch:
		return "schema-mismatch";
	case th03::app::NewClassicMessagePackErrorCode::body_digest_mismatch:
		return "body-digest-mismatch";
	case th03::app::NewClassicMessagePackErrorCode::locale_shape:
		return "locale-shape";
	case th03::app::NewClassicMessagePackErrorCode::asset_profile:
		return "asset-profile";
	case th03::app::NewClassicMessagePackErrorCode::directory_shape:
		return "directory-shape";
	case th03::app::NewClassicMessagePackErrorCode::message_shape:
		return "message-shape";
	case th03::app::NewClassicMessagePackErrorCode::fallback_missing:
		return "fallback-missing";
	case th03::app::NewClassicMessagePackErrorCode::placeholder_mismatch:
		return "placeholder-mismatch";
	case th03::app::NewClassicMessagePackErrorCode::glyph_coverage:
		return "glyph-coverage";
	case th03::app::NewClassicMessagePackErrorCode::width_exceeded:
		return "width-exceeded";
	case th03::app::NewClassicMessagePackErrorCode::allocation_failed:
		return "allocation-failed";
	}
	return "unknown";
}

const char *name(th03::app::NewClassicGameTextPackErrorCode code) noexcept
{
	switch(code) {
	case th03::app::NewClassicGameTextPackErrorCode::invalid_path: return "invalid-path";
	case th03::app::NewClassicGameTextPackErrorCode::source_status: return "source-status";
	case th03::app::NewClassicGameTextPackErrorCode::source_not_regular: return "source-not-regular";
	case th03::app::NewClassicGameTextPackErrorCode::source_size: return "source-size";
	case th03::app::NewClassicGameTextPackErrorCode::source_open: return "source-open";
	case th03::app::NewClassicGameTextPackErrorCode::source_read: return "source-read";
	case th03::app::NewClassicGameTextPackErrorCode::header_shape: return "header-shape";
	case th03::app::NewClassicGameTextPackErrorCode::schema_mismatch: return "schema-mismatch";
	case th03::app::NewClassicGameTextPackErrorCode::body_digest_mismatch: return "body-digest-mismatch";
	case th03::app::NewClassicGameTextPackErrorCode::locale_shape: return "locale-shape";
	case th03::app::NewClassicGameTextPackErrorCode::encoding: return "encoding";
	case th03::app::NewClassicGameTextPackErrorCode::directory_shape: return "directory-shape";
	case th03::app::NewClassicGameTextPackErrorCode::message_shape: return "message-shape";
	case th03::app::NewClassicGameTextPackErrorCode::fallback_missing: return "fallback-missing";
	case th03::app::NewClassicGameTextPackErrorCode::malformed_shift_jis: return "malformed-shift-jis";
	case th03::app::NewClassicGameTextPackErrorCode::length_mismatch: return "length-mismatch";
	case th03::app::NewClassicGameTextPackErrorCode::allocation_failed: return "allocation-failed";
	}
	return "unknown";
}

const char *name(th03::app::NewClassicCutsceneTextPackErrorCode code) noexcept
{
	switch(code) {
	case th03::app::NewClassicCutsceneTextPackErrorCode::invalid_path:
		return "invalid-path";
	case th03::app::NewClassicCutsceneTextPackErrorCode::source_status:
		return "source-status";
	case th03::app::NewClassicCutsceneTextPackErrorCode::source_not_regular:
		return "source-not-regular";
	case th03::app::NewClassicCutsceneTextPackErrorCode::source_size:
		return "source-size";
	case th03::app::NewClassicCutsceneTextPackErrorCode::source_open:
		return "source-open";
	case th03::app::NewClassicCutsceneTextPackErrorCode::source_read:
		return "source-read";
	case th03::app::NewClassicCutsceneTextPackErrorCode::header_shape:
		return "header-shape";
	case th03::app::NewClassicCutsceneTextPackErrorCode::body_digest_mismatch:
		return "body-digest-mismatch";
	case th03::app::NewClassicCutsceneTextPackErrorCode::locale_shape:
		return "locale-shape";
	case th03::app::NewClassicCutsceneTextPackErrorCode::directory_shape:
		return "directory-shape";
	case th03::app::NewClassicCutsceneTextPackErrorCode::message_shape:
		return "message-shape";
	case th03::app::NewClassicCutsceneTextPackErrorCode::malformed_shift_jis:
		return "malformed-shift-jis";
	case th03::app::NewClassicCutsceneTextPackErrorCode::source_profile_mismatch:
		return "source-profile-mismatch";
	case th03::app::NewClassicCutsceneTextPackErrorCode::script_mismatch:
		return "script-mismatch";
	case th03::app::NewClassicCutsceneTextPackErrorCode::run_mismatch:
		return "run-mismatch";
	case th03::app::NewClassicCutsceneTextPackErrorCode::allocation_failed:
		return "allocation-failed";
	}
	return "unknown";
}

int fail(const char *stage, const th03::app::StartupSequenceError &error)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< name(error.code) << '\n';
	if(error.code ==
		th03::app::StartupSequenceErrorCode::file_size_mismatch) {
		std::cerr << "expected-size=8272\nobserved-size="
			<< error.observed_size << '\n';
	}
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

std::expected<void, int> refresh_host_presentability(
	th03::platform::SdlPlatform &platform,
	const char *stage
)
{
	if(auto refreshed = platform.refresh_presentability(); !refreshed) {
		return std::unexpected(fail(stage, refreshed.error()));
	}
	return {};
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
	if(error.language_pack_error) {
		std::cerr << "language-pack-code=" << name(
			error.language_pack_error->code
		) << "\nlanguage-pack-offset=" << error.language_pack_error->offset
			<< "\nlanguage-pack-message=" << static_cast<unsigned int>(
				error.language_pack_error->message
			) << '\n';
		if(error.language_pack_error->system) {
			std::cerr << "language-pack-system="
				<< error.language_pack_error->system.message() << '\n';
		}
	}
	if(error.game_text_pack_error) {
		std::cerr << "game-text-pack-code=" << name(
			error.game_text_pack_error->code
		) << "\ngame-text-pack-offset=" << error.game_text_pack_error->offset
			<< "\ngame-text-pack-message=" << static_cast<unsigned int>(
				error.game_text_pack_error->message
			) << '\n';
		if(error.game_text_pack_error->system) {
			std::cerr << "game-text-pack-system="
				<< error.game_text_pack_error->system.message() << '\n';
		}
	}
	if(error.cutscene_text_pack_error) {
		std::cerr << "cutscene-text-pack-code=" << name(
			error.cutscene_text_pack_error->code
		) << "\ncutscene-text-pack-offset="
			<< error.cutscene_text_pack_error->offset
			<< "\ncutscene-text-pack-script="
			<< error.cutscene_text_pack_error->script_index
			<< "\ncutscene-text-pack-run="
			<< error.cutscene_text_pack_error->run_index << '\n';
		if(error.cutscene_text_pack_error->system) {
			std::cerr << "cutscene-text-pack-system="
				<< error.cutscene_text_pack_error->system.message() << '\n';
		}
	}
	if(error.surface_error) {
		std::cerr << "surface-code=" << static_cast<unsigned int>(
			error.surface_error->code
		) << "\ntext-code=" << static_cast<unsigned int>(
			error.surface_error->text.code
		) << '\n';
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
	if(error.midi_engine_error) {
		std::cerr << "midi-engine-code=" << static_cast<unsigned int>(
			error.midi_engine_error->code
		) << "\nmidi-offset=" << error.midi_engine_error->offset
			<< "\nmidi-command=" << static_cast<unsigned int>(
				error.midi_engine_error->command
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

int fail_output_gain(const char *stage, const char *code)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode=" << code << '\n';
	return 1;
}

int fail_netplay(const char *stage, const char *code)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode=" << code
		<< '\n';
	return 1;
}

int fail_tick(th03::app::TickInputError error)
{
	std::cerr << "status=error\nstage=tick-input\ncode="
		<< name(error) << '\n';
	return 1;
}

} // namespace th03::app::launcher_support
