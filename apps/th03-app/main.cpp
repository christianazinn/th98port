#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_keyboard.h>

#include "default_bindings.hpp"

#include "th03/app/classic_cli.hpp"
#include "th03/app/classic_audio_session.hpp"
#include "th03/app/classic_config_store.hpp"
#include "th03/app/classic_runner.hpp"
#include "th03/app/direct_vs_cli.hpp"
#include "th03/app/direct_vs_launch.hpp"
#include "th03/app/main_direct_vs_runner.hpp"
#include "th03/app/new_classic_netplay_datagram.hpp"
#include "th03/app/new_classic_netplay_lobby.hpp"
#include "th03/app/new_classic_netplay_match_contract.hpp"
#include "th03/app/new_classic_netplay_main_session.hpp"
#include "th03/app/new_classic_netplay_post_match.hpp"
#include "th03/app/new_classic_netplay_replay_capture.hpp"
#include "th03/app/main_compiled_content.hpp"
#include "th03/app/new_classic_frontend.hpp"
#include "th03/app/new_classic_language.hpp"
#include "th03/app/new_classic_online_client.hpp"
#include "th03/app/new_classic_online_store.hpp"
#include "th03/app/new_classic_online_task.hpp"
#include "th03/app/new_classic_playback.hpp"
#include "th03/app/new_classic_settings_store.hpp"
#include "th03/app/replay_slot_store.hpp"
#include "th03/app/startup_sequence.hpp"
#include "th03/app/tick_input.hpp"
#include "th03/audio/output_gain.hpp"
#include "th03/formats/v13.hpp"
#include "th03/platform/sdl_input.hpp"
#include "th03/platform/sdl_audio_device.hpp"
#include "th03/platform/sdl_platform.hpp"
#include "th03/platform/udp_socket.hpp"
#if defined(_WIN32)
#include "th03/platform/windows_online.hpp"
#endif

namespace {

constexpr std::uint64_t maximum_idle_delay_ns = 1'000'000U;
constexpr const char *pixel_oracle_log_environment = "TH03_PIXEL_ORACLE_LOG";
constexpr const char *pixel_oracle_rgb_environment = "TH03_PIXEL_ORACLE_RGB_DIR";
constexpr const char *pixel_oracle_identity_environment =
	"TH03_PIXEL_ORACLE_IDENTITY_LOG";
constexpr const char *pixel_oracle_fixture_environment =
	"TH03_PIXEL_ORACLE_FIXTURE_SHA256";
constexpr const char *pixel_oracle_selector_environment =
	"TH03_PIXEL_ORACLE_SELECTOR";
constexpr const char *online_origin_environment = "TH03_ONLINE_ORIGIN";

struct PixelOracleIdentityOutput final {
	std::ofstream stream{};
	std::string fixture_sha256{};

	[[nodiscard]] bool enabled() const noexcept { return stream.is_open(); }
};

[[nodiscard]] std::optional<std::string> environment_value(const char *name)
{
#if defined(_MSC_VER)
	char *value = nullptr;
	std::size_t value_bytes = 0U;
	if(_dupenv_s(&value, &value_bytes, name) != 0 || value == nullptr) {
		std::free(value);
		return std::nullopt;
	}
	std::string result(value);
	std::free(value);
	return result;
#else
	const char *const value = std::getenv(name);
	return value ? std::optional<std::string>(value) : std::nullopt;
#endif
}

[[nodiscard]] std::expected<PixelOracleIdentityOutput, const char *>
pixel_oracle_identity_output(bool publication_oracle_enabled)
{
	const auto path = environment_value(pixel_oracle_identity_environment);
	const auto fixture = environment_value(pixel_oracle_fixture_environment);
	if(!path && !fixture) {
		return PixelOracleIdentityOutput{};
	}
	if(!path || !fixture || path->empty() || !publication_oracle_enabled ||
		!th03::app::valid_pixel_oracle_fixture_sha256(*fixture)) {
		return std::unexpected("invalid-identity-oracle-config");
	}
	std::error_code error;
	if(std::filesystem::exists(*path, error) || error) {
		return std::unexpected("identity-oracle-path-used");
	}
	PixelOracleIdentityOutput output{
		.stream = std::ofstream(*path, std::ios::out | std::ios::binary),
		.fixture_sha256 = *fixture,
	};
	std::ranges::transform(
		output.fixture_sha256, output.fixture_sha256.begin(),
		[](char digit) { return static_cast<char>(std::tolower(
			static_cast<unsigned char>(digit)
		)); }
	);
	if(!output.stream || !th03::app::write_t3ali1_header(output.stream)) {
		return std::unexpected("identity-oracle-open-failed");
	}
	return output;
}

[[nodiscard]] th03::platform::SdlPlatformConfig platform_config(
	bool force_hidden = false
)
{
	th03::platform::SdlPlatformConfig config{};
	config.title = th03::platform::th03_product_window_title;
#if defined(_MSC_VER)
	char *path = nullptr;
	std::size_t path_bytes = 0U;
	if(_dupenv_s(&path, &path_bytes, pixel_oracle_log_environment) == 0 &&
		path != nullptr) {
		config.oracle_log_path = path;
	}
	std::free(path);
	path = nullptr;
	path_bytes = 0U;
	if(_dupenv_s(&path, &path_bytes, pixel_oracle_rgb_environment) == 0 &&
		path != nullptr) {
		config.oracle_rgb_directory = path;
	}
	std::free(path);
#else
	if(const char *const path = std::getenv(pixel_oracle_log_environment);
		path != nullptr) {
		config.oracle_log_path = path;
	}
	if(const char *const path = std::getenv(pixel_oracle_rgb_environment);
		path != nullptr) {
		config.oracle_rgb_directory = path;
	}
#endif
	// Developer identity captures are unattended evidence runs. Hide the native
	// window from creation onward so capture cannot steal focus.
	const auto identity_path = environment_value(
		pixel_oracle_identity_environment
	);
	config.hidden = force_hidden || th03::app::pixel_oracle_identity_capture_hidden(
		identity_path.value_or("")
	);
	return config;
}

enum class ExperienceRunDisposition : std::uint8_t {
	return_to_menu,
	switch_to_classic,
	application_quit,
	smoke_complete,
	replay_completed,
	failed,
};

struct ExperienceRunResult final {
	int exit_code = 0;
	ExperienceRunDisposition disposition =
		ExperienceRunDisposition::failed;
	std::optional<th03::formats::ReplayDocument> replay{};
};

struct StartupRunResult final {
	int exit_code = 0;
	bool application_quit = false;
};

struct HostUiStep final {
	bool surface_changed = false;
	bool done = false;
};

struct ReplaySaveRunResult final {
	int exit_code = 0;
	bool application_quit = false;
};

struct ReplaySaveAudioContext final {
	th03::app::ClassicAudioSession *session = nullptr;
	std::optional<th03::platform::SdlAudioDevice> *device = nullptr;
	bool *started = nullptr;
	const std::uint8_t *master_volume_percent = nullptr;
	const bool *mute_when_unfocused = nullptr;
};

[[nodiscard]] std::expected<
	std::span<const th03::audio::PmdCanonicalStereoSample>,
	const char *
> output_gain_frames(
	std::span<const th03::audio::PmdCanonicalStereoSample> source,
	std::uint8_t percent,
	std::vector<th03::audio::PmdCanonicalStereoSample> &storage
)
{
	if(!th03::audio::valid_output_gain_percent(percent)) {
		return std::unexpected("invalid-master-volume");
	}
	if(percent == 100U) {
		return source;
	}
	storage.assign(source.begin(), source.end());
	if(!th03::audio::apply_output_gain(storage, percent)) {
		return std::unexpected("master-volume-apply-failed");
	}
	return std::span<const th03::audio::PmdCanonicalStereoSample>(storage);
}

[[nodiscard]] std::uint16_t current_dos_date(void *) noexcept
{
	const auto now = std::time(nullptr);
	std::tm local{};
#if defined(_WIN32)
	if(localtime_s(&local, &now) != 0) {
		return 0U;
	}
#else
	if(!localtime_r(&now, &local)) {
		return 0U;
	}
#endif
	const auto year = std::clamp(local.tm_year + 1900, 1980, 2107) - 1980;
	return static_cast<std::uint16_t>(
		(static_cast<std::uint16_t>(year) << 9U) |
		(static_cast<std::uint16_t>(local.tm_mon + 1) << 5U) |
		static_cast<std::uint16_t>(local.tm_mday)
	);
}

ReplaySaveRunResult run_replay_save(
	th03::formats::ReplayDocument document,
	const std::filesystem::path &replay_root,
	th03::app::RationalTickRate tick_rate,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper,
	std::shared_ptr<const th03::app::NewClassicReplayPatchAssets>
		patch_assets = {},
	std::shared_ptr<const th03::app::NewClassicReplaySaveArt> save_art = {},
	ReplaySaveAudioContext audio = {}
);

enum class RhythmRomLoadErrorCode : std::uint8_t {
	file_missing,
	file_status_failed,
	file_not_regular,
	file_size_failed,
	file_size_mismatch,
	file_open_failed,
	file_read_failed,
	allocation_failed,
};

struct RhythmRomLoadError final {
	RhythmRomLoadErrorCode code = RhythmRomLoadErrorCode::file_missing;
	std::uintmax_t observed_size = 0U;
};

enum class RhythmPcmLoadErrorCode : std::uint8_t {
	file_missing,
	file_status_failed,
	file_not_regular,
	file_size_failed,
	file_size_mismatch,
	file_open_failed,
	file_read_failed,
	digest_mismatch,
	format_mismatch,
	allocation_failed,
};

struct RhythmPcmLoadError final {
	RhythmPcmLoadErrorCode code = RhythmPcmLoadErrorCode::file_missing;
	std::uintmax_t observed_size = 0U;
};

struct LoadedRhythmPcm final {
	std::array<
		std::vector<std::int16_t>, th03::audio::ym2608_rhythm_voice_count
	> voices{};

	[[nodiscard]] th03::audio::PmdOpnaRhythmPcm view() const noexcept
	{
		th03::audio::PmdOpnaRhythmPcm result{};
		if(std::ranges::any_of(voices, [](const auto &voice) {
			return !voice.empty();
		})) {
			result.sample_rate_hz = 44'100U;
		}
		for(std::size_t index = 0U; index < voices.size(); ++index) {
			result.voices[index] = voices[index];
		}
		return result;
	}
};

constexpr std::size_t rhythm_pcm_header_bytes = 36U;
constexpr std::size_t rhythm_pcm_profile_bytes = 178'730U;
constexpr std::array<std::uint8_t, 32U> rhythm_pcm_profile_sha256{
	0x22U, 0x5FU, 0xD0U, 0x7BU, 0x20U, 0xC5U, 0x1AU, 0xC1U,
	0x43U, 0x8EU, 0x42U, 0x6FU, 0x92U, 0x25U, 0x2CU, 0x41U,
	0xACU, 0xE9U, 0x5FU, 0x03U, 0x9CU, 0xA5U, 0x2AU, 0x59U,
	0xA3U, 0x04U, 0x96U, 0x16U, 0x9DU, 0x0CU, 0x89U, 0x53U,
};
constexpr std::array<std::uint32_t, 6U> rhythm_pcm_voice_lengths{
	0x2590U, 0x3B10U, 0x6EEDU, 0x3E00U, 0x2F5EU, 0x2018U,
};

std::uint32_t read_le32(std::span<const std::byte> bytes, std::size_t offset)
{
	return std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U) |
		(static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 2U])
		) << 16U) |
		(static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 3U])
		) << 24U);
}

[[nodiscard]] std::expected<std::vector<std::byte>, RhythmRomLoadError>
load_rhythm_rom(const std::filesystem::path &path)
{
	std::error_code error{};
	const auto status = std::filesystem::status(path, error);
	if(error) {
		if(error == std::errc::no_such_file_or_directory) {
			return std::unexpected(RhythmRomLoadError{
				.code = RhythmRomLoadErrorCode::file_missing,
			});
		}
		return std::unexpected(RhythmRomLoadError{
			.code = RhythmRomLoadErrorCode::file_status_failed,
		});
	}
	if(!std::filesystem::exists(status)) {
		return std::unexpected(RhythmRomLoadError{
			.code = RhythmRomLoadErrorCode::file_missing,
		});
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(RhythmRomLoadError{
			.code = RhythmRomLoadErrorCode::file_not_regular,
		});
	}
	const auto size = std::filesystem::file_size(path, error);
	if(error) {
		return std::unexpected(RhythmRomLoadError{
			.code = RhythmRomLoadErrorCode::file_size_failed,
		});
	}
	if(size != th03::audio::ym2608_rhythm_rom_bytes) {
		return std::unexpected(RhythmRomLoadError{
			.code = RhythmRomLoadErrorCode::file_size_mismatch,
			.observed_size = size,
		});
	}
	try {
		std::vector<std::byte> bytes(th03::audio::ym2608_rhythm_rom_bytes);
		std::ifstream input(path, std::ios::binary);
		if(!input.is_open()) {
			return std::unexpected(RhythmRomLoadError{
				.code = RhythmRomLoadErrorCode::file_open_failed,
			});
		}
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
		if(!input ||
			input.gcount() != static_cast<std::streamsize>(bytes.size())) {
			return std::unexpected(RhythmRomLoadError{
				.code = RhythmRomLoadErrorCode::file_read_failed,
			});
		}
		return bytes;
	} catch(const std::bad_alloc &) {
		return std::unexpected(RhythmRomLoadError{
			.code = RhythmRomLoadErrorCode::allocation_failed,
		});
	}
}

[[nodiscard]] std::expected<LoadedRhythmPcm, RhythmPcmLoadError>
load_rhythm_pcm(const std::filesystem::path &path)
{
	std::error_code error{};
	const auto status = std::filesystem::status(path, error);
	if(error) {
		return std::unexpected(RhythmPcmLoadError{
			.code = error == std::errc::no_such_file_or_directory ?
				RhythmPcmLoadErrorCode::file_missing :
				RhythmPcmLoadErrorCode::file_status_failed,
		});
	}
	if(!std::filesystem::exists(status)) {
		return std::unexpected(RhythmPcmLoadError{
			.code = RhythmPcmLoadErrorCode::file_missing,
		});
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(RhythmPcmLoadError{
			.code = RhythmPcmLoadErrorCode::file_not_regular,
		});
	}
	const auto size = std::filesystem::file_size(path, error);
	if(error) {
		return std::unexpected(RhythmPcmLoadError{
			.code = RhythmPcmLoadErrorCode::file_size_failed,
		});
	}
	if(size != rhythm_pcm_profile_bytes) {
		return std::unexpected(RhythmPcmLoadError{
			.code = RhythmPcmLoadErrorCode::file_size_mismatch,
			.observed_size = size,
		});
	}
	try {
		std::vector<std::byte> bytes(rhythm_pcm_profile_bytes);
		std::ifstream input(path, std::ios::binary);
		if(!input.is_open()) {
			return std::unexpected(RhythmPcmLoadError{
				.code = RhythmPcmLoadErrorCode::file_open_failed,
			});
		}
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
		if(!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
			return std::unexpected(RhythmPcmLoadError{
				.code = RhythmPcmLoadErrorCode::file_read_failed,
			});
		}
		if(th03::app::main_executable_sha256(bytes) !=
			rhythm_pcm_profile_sha256) {
			return std::unexpected(RhythmPcmLoadError{
				.code = RhythmPcmLoadErrorCode::digest_mismatch,
			});
		}
		constexpr std::array<std::byte, 8U> magic{
			std::byte{'T'}, std::byte{'9'}, std::byte{'8'}, std::byte{'R'},
			std::byte{'H'}, std::byte{'Y'}, std::byte{'0'}, std::byte{0U},
		};
		if(!std::equal(magic.begin(), magic.end(), bytes.begin()) ||
			read_le32(bytes, 8U) != 44'100U) {
			return std::unexpected(RhythmPcmLoadError{
				.code = RhythmPcmLoadErrorCode::format_mismatch,
			});
		}
		LoadedRhythmPcm result{};
		std::size_t cursor = rhythm_pcm_header_bytes;
		for(std::size_t voice = 0U; voice < result.voices.size(); ++voice) {
			const auto length = rhythm_pcm_voice_lengths[voice];
			if(read_le32(bytes, 12U + (voice * 4U)) != length) {
				return std::unexpected(RhythmPcmLoadError{
					.code = RhythmPcmLoadErrorCode::format_mismatch,
				});
			}
			auto &samples = result.voices[voice];
			samples.resize(length);
			for(auto &sample : samples) {
				sample = static_cast<std::int16_t>(
					std::to_integer<std::uint8_t>(bytes[cursor]) |
					(static_cast<std::uint16_t>(
						std::to_integer<std::uint8_t>(bytes[cursor + 1U])
					) << 8U)
				);
				cursor += 2U;
			}
		}
		if(cursor != bytes.size()) {
			return std::unexpected(RhythmPcmLoadError{
				.code = RhythmPcmLoadErrorCode::format_mismatch,
			});
		}
		return result;
	} catch(const std::bad_alloc &) {
		return std::unexpected(RhythmPcmLoadError{
			.code = RhythmPcmLoadErrorCode::allocation_failed,
		});
	}
}

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
			"[--rhythm-rom <YM2608.ROM> | --rhythm-pcm <T98RHYTHM.PCM>] "
			"[--tick-rate <ticks>/<seconds>] "
			"[--smoke-ticks n] [--capture-replay <V13.RPY> "
			"[--capture-checkpoint n]]\n"
		<< "   or: th03-app --vs --data <root> --main <MAIN.EXE> "
			"--font <FONT.ROM>\n"
		<< "       [--p1 <character[:first|second]>] "
			"[--p2 <character[:first|second]>] [--rank 0..3] [--seed n] "
			"[--tick-rate <ticks>/<seconds>] "
			"[--smoke-ticks n] [--netplay-host <port> | "
			"--netplay-join <IPv4-or-hostname:port>] "
			"[--netplay-replay-output <V13.RPY>]\n"
		<< "characters: reimu mima marisa ellen kotohime kana rikako "
			"chiyuri yumemi\n"
		<< "P1: T/F/H/B, diagonals R/Y/V/N, Z shot, X bomb\n"
		<< "P2: keypad 8/4/6/2, diagonals 7/9/1/3, Left shot, Right bomb\n"
		<< "TH03 UI: arrows/keypad, Z/X/Space, Return OK, Escape Cancel\n"
		<< "--shell runs startup and boots directly into New Classic. "
			"New Classic Quit exits; Option -> System can temporarily launch "
			"sealed Classic, whose Quit returns to a fresh New Classic title. "
			"In New Classic gameplay, Escape opens "
			"Pause, R restarts, and Q exits without saving. During New Classic "
			"replay playback, Escape aborts and held physical Z fast-forwards; "
			"after a prior clear, physical Z also accelerates Staff. In --vs, "
			"Escape exits. In Classic, Escape remains source Cancel and "
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
			"authenticated Replay Patch 0.4.14 TH3EN.DAT is supplied with "
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
	case th03::app::ClassicCliOption::data: return "data";
	case th03::app::ClassicCliOption::main_executable: return "main";
	case th03::app::ClassicCliOption::mainl_executable: return "mainl";
	case th03::app::ClassicCliOption::op_executable: return "op";
	case th03::app::ClassicCliOption::font_rom: return "font";
	case th03::app::ClassicCliOption::language_overlay:
		return "language-overlay";
	case th03::app::ClassicCliOption::language_pack:
		return "language-pack";
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

const char *name(RhythmRomLoadErrorCode code) noexcept
{
	switch(code) {
	case RhythmRomLoadErrorCode::file_missing: return "file-missing";
	case RhythmRomLoadErrorCode::file_status_failed: return "file-status-failed";
	case RhythmRomLoadErrorCode::file_not_regular: return "file-not-regular";
	case RhythmRomLoadErrorCode::file_size_failed: return "file-size-failed";
	case RhythmRomLoadErrorCode::file_size_mismatch: return "file-size-mismatch";
	case RhythmRomLoadErrorCode::file_open_failed: return "file-open-failed";
	case RhythmRomLoadErrorCode::file_read_failed: return "file-read-failed";
	case RhythmRomLoadErrorCode::allocation_failed: return "allocation-failed";
	}
	return "unknown";
}

const char *name(RhythmPcmLoadErrorCode code) noexcept
{
	switch(code) {
	case RhythmPcmLoadErrorCode::file_missing: return "file-missing";
	case RhythmPcmLoadErrorCode::file_status_failed: return "file-status-failed";
	case RhythmPcmLoadErrorCode::file_not_regular: return "file-not-regular";
	case RhythmPcmLoadErrorCode::file_size_failed: return "file-size-failed";
	case RhythmPcmLoadErrorCode::file_size_mismatch: return "file-size-mismatch";
	case RhythmPcmLoadErrorCode::file_open_failed: return "file-open-failed";
	case RhythmPcmLoadErrorCode::file_read_failed: return "file-read-failed";
	case RhythmPcmLoadErrorCode::digest_mismatch: return "digest-mismatch";
	case RhythmPcmLoadErrorCode::format_mismatch: return "format-mismatch";
	case RhythmPcmLoadErrorCode::allocation_failed: return "allocation-failed";
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

int fail(const char *stage, const RhythmRomLoadError &error)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< name(error.code) << '\n';
	if(error.code == RhythmRomLoadErrorCode::file_size_mismatch) {
		std::cerr << "expected-size=" << th03::audio::ym2608_rhythm_rom_bytes
			<< "\nobserved-size=" << error.observed_size << '\n';
	}
	return 1;
}

int fail(const char *stage, const RhythmPcmLoadError &error)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< name(error.code) << '\n';
	if(error.code == RhythmPcmLoadErrorCode::file_size_mismatch) {
		std::cerr << "expected-size=" << rhythm_pcm_profile_bytes
			<< "\nobserved-size=" << error.observed_size << '\n';
	}
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

[[nodiscard]] constexpr th03::platform::SdlWindowPresentation
platform_window_presentation(
	th03::app::NewClassicWindowPresentation presentation
) noexcept
{
	return {
		.mode = presentation.mode ==
			th03::app::NewClassicWindowMode::windowed ?
				th03::platform::SdlWindowMode::windowed :
				th03::platform::SdlWindowMode::borderless_fullscreen,
		.integer_scale = static_cast<std::uint8_t>(
			presentation.scale ==
				th03::app::NewClassicWindowScale::automatic ? 0U :
				static_cast<unsigned>(presentation.scale) + 1U
		),
		.aspect = presentation.aspect ==
			th03::app::NewClassicAspectRatio::pixels_8_5 ?
				th03::platform::SdlAspectRatio::pixels_8_5 :
				th03::platform::SdlAspectRatio::display_4_3,
		.filtering = presentation.filtering ==
			th03::app::NewClassicFiltering::nearest ?
				th03::platform::SdlFiltering::nearest :
				th03::platform::SdlFiltering::linear,
	};
}

} // namespace

namespace {

[[nodiscard]] th03::app::NewClassicNetplayHostLock netplay_host_lock(
	const th03::app::DirectVsCliRequest &request
) noexcept
{
	return {
		.player_one = {
			.character = static_cast<std::uint8_t>(
				request.launch.process.player_one.character
			),
			.palette = static_cast<std::uint8_t>(
				request.launch.process.player_one.palette
			),
		},
		.rank = request.launch.process.rank,
		.seed = request.launch.process.seed,
		.cadence_ticks = request.tick_rate.ticks,
		.cadence_seconds = request.tick_rate.seconds,
	};
}

[[nodiscard]] th03::app::NewClassicNetplayJoinLock netplay_join_lock(
	const th03::app::DirectVsCliRequest &request
) noexcept
{
	return {
		.player_two = {
			.character = static_cast<std::uint8_t>(
				request.launch.process.player_two.character
			),
			.palette = static_cast<std::uint8_t>(
				request.launch.process.player_two.palette
			),
		},
	};
}

void apply_netplay_contract(
	th03::app::DirectVsCliRequest &request,
	const th03::app::NewClassicNetplayMatchContract &contract
) noexcept
{
	request.launch.process.player_one = {
		.character = static_cast<th03::app::MainCharacter>(
			contract.player_one.character
		),
		.palette = static_cast<th03::app::MainPaletteVariant>(
			contract.player_one.palette
		),
	};
	request.launch.process.player_two = {
		.character = static_cast<th03::app::MainCharacter>(
			contract.player_two.character
		),
		.palette = static_cast<th03::app::MainPaletteVariant>(
			contract.player_two.palette
		),
	};
	request.launch.process.rank = contract.rank;
	request.launch.process.seed = contract.seed;
	request.tick_rate = {
		.ticks = contract.cadence_ticks,
		.seconds = contract.cadence_seconds,
	};
	request.launch.process.game_options.input_protocol =
		th03::InputProtocol::live;
	request.launch.process.game_options.three_digit_combos = true;
}

[[nodiscard]] th03::app::NewClassicNetplayOwnedInput netplay_owned_input(
	const th03::CoreInput &input
) noexcept
{
	// Each peer uses its ordinary local-player bindings. The negotiated role
	// decides which logical lane receives this value; it must not force the
	// joining player onto the local P2/numpad binding set.
	return {
		.word = input.p1,
		.charge = static_cast<std::uint8_t>(input.charge_mask & 1U),
	};
}

[[nodiscard]] int fail_netplay(const char *stage, const char *code)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode=" << code
		<< '\n';
	return 1;
}

void log_netplay_session_error(
	const th03::app::NewClassicNetplayMainSessionError &error
)
{
	std::cerr << "netplay-session-code=" <<
		static_cast<unsigned>(error.code) << '\n';
	if(error.timeline) {
		std::cerr << "netplay-timeline-code=" <<
			static_cast<unsigned>(error.timeline->code) << '\n';
	}
	if(error.rollback) {
		std::cerr << "netplay-rollback-code=" <<
			static_cast<unsigned>(error.rollback->code) << '\n';
		if(error.rollback->rollback) {
			std::cerr << "netplay-snapshot-code=" << static_cast<unsigned>(
				error.rollback->rollback->code
			) << '\n';
		}
	}
}

[[nodiscard]] bool send_netplay_datagram(
	th03::platform::UdpSocket &socket,
	th03::platform::UdpIpv4Endpoint peer,
	const th03::app::NewClassicNetplayDatagram &datagram
)
{
	const auto encoded = th03::app::encode_new_classic_netplay_datagram(
		datagram
	);
	return encoded && socket.send(peer, *encoded).has_value();
}

[[nodiscard]] bool send_netplay_lobby_datagram(
	th03::platform::UdpSocket &socket,
	th03::platform::UdpIpv4Endpoint peer,
	const th03::app::NewClassicNetplayLobbyDatagram &datagram
)
{
	const auto encoded =
		th03::app::encode_new_classic_netplay_lobby_datagram(datagram);
	return encoded && socket.send(peer, *encoded).has_value();
}

struct DirectNetplayLobbyResult final {
	std::unique_ptr<th03::platform::UdpSocket> socket{};
	th03::platform::UdpIpv4Endpoint peer{};
	th03::app::NewClassicNetplayLobby lobby{};
	th03::app::NewClassicNetplayMatchContract contract{};
};

[[nodiscard]] std::expected<DirectNetplayLobbyResult, int>
negotiate_direct_netplay_lobby(
	const th03::app::DirectVsCliRequest &request
)
{
	using th03::app::DirectVsNetplayMode;
	using th03::app::NewClassicNetplayLobby;
	using th03::app::NewClassicNetplayLobbyDatagramType;
	using th03::app::NewClassicNetplayLobbyStatus;
	const auto mode = request.netplay_mode;
	auto socket = th03::platform::UdpSocket::bind(
		mode == DirectVsNetplayMode::host ? request.netplay_host_port : 0U
	);
	if(!socket) {
		return std::unexpected(fail_netplay("netplay-socket", "bind-failed"));
	}
	std::optional<th03::platform::UdpIpv4Endpoint> peer{};
	if(mode == DirectVsNetplayMode::join) {
		const auto endpoint = th03::platform::resolve_udp_ipv4_endpoint(
			request.netplay_join_endpoint
		);
		if(!endpoint) {
			return std::unexpected(fail_netplay(
				"netplay-endpoint", "resolve-ipv4-endpoint"
			));
		}
		peer = *endpoint;
		std::cout << "resolved-peer=" <<
			th03::platform::format_udp_ipv4_endpoint(*peer) << '\n';
	}
	const auto clock_seed = static_cast<std::uint64_t>(
		std::chrono::steady_clock::now().time_since_epoch().count()
	);
	auto created_lobby = mode == DirectVsNetplayMode::host ?
		NewClassicNetplayLobby::create_host(
			netplay_host_lock(request),
			(clock_seed ^ 0xA35C91D27B48E60FULL) == 0U ? 1U :
				(clock_seed ^ 0xA35C91D27B48E60FULL)
		) :
		NewClassicNetplayLobby::create_join(netplay_join_lock(request));
	if(!created_lobby) {
		return std::unexpected(fail_netplay(
			"netplay-lobby", "invalid-local-lock"
		));
	}
	auto lobby = std::move(*created_lobby);
	std::cout << "status=waiting\nrole=" <<
		(mode == DirectVsNetplayMode::host ? "host-p1" : "join-p2") << '\n';
	if(mode == DirectVsNetplayMode::host) {
		std::cout << "listen-port=" << request.netplay_host_port << '\n';
	} else {
		std::cout << "peer=" << request.netplay_join_endpoint << '\n';
	}
	std::cout.flush();

	const auto started = std::chrono::steady_clock::now();
	auto last_send = started - std::chrono::milliseconds(100);
	std::array<std::byte, th03::app::new_classic_netplay_datagram_bytes>
		receive_buffer{};
	while(lobby.status() != NewClassicNetplayLobbyStatus::ready) {
		const auto now = std::chrono::steady_clock::now();
		if(now - started > std::chrono::seconds(30)) {
			return std::unexpected(fail_netplay(
				"netplay-lobby", "timeout"
			));
		}
		for(;;) {
			const auto received = (*socket)->receive(receive_buffer);
			if(!received) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "receive-failed"
				));
			}
			if(!*received) {
				break;
			}
			if((*received)->size !=
				th03::app::new_classic_netplay_lobby_bytes) {
				continue;
			}
			const auto packet =
				th03::app::decode_new_classic_netplay_lobby_datagram(
					std::span(receive_buffer).first((*received)->size)
				);
			if(!packet || (peer && *peer != (*received)->sender)) {
				continue;
			}
			if(mode == DirectVsNetplayMode::host && !peer &&
				packet->type != NewClassicNetplayLobbyDatagramType::join_request) {
				continue;
			}
			if(const auto accepted = lobby.receive(*packet); !accepted) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "peer-lock-conflict"
				));
			}
			if(!peer) {
				peer = (*received)->sender;
			}
			if(const auto outbound = lobby.outbound(); outbound &&
				!send_netplay_lobby_datagram(**socket, *peer, *outbound)) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "send-failed"
				));
			}
			last_send = now;
		}
		if(lobby.status() == NewClassicNetplayLobbyStatus::ready) {
			break;
		}
		if(peer && now - last_send >= std::chrono::milliseconds(100)) {
			if(const auto outbound = lobby.outbound(); outbound &&
				!send_netplay_lobby_datagram(**socket, *peer, *outbound)) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "send-failed"
				));
			}
			last_send = now;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	const auto contract = lobby.contract();
	if(!peer || !contract || lobby.session_id() == 0U) {
		return std::unexpected(fail_netplay(
			"netplay-lobby", "incomplete-ready-state"
		));
	}
	return DirectNetplayLobbyResult{
		.socket = std::move(*socket),
		.peer = *peer,
		.lobby = std::move(lobby),
		.contract = *contract,
	};
}

[[nodiscard]] bool send_netplay_input_history(
	th03::platform::UdpSocket &socket,
	th03::platform::UdpIpv4Endpoint peer,
	const th03::app::NewClassicNetplayMainSession &session,
	th03::app::NewClassicNetplayPlayer local_player,
	std::uint64_t session_id,
	std::uint64_t contract_hash,
	std::uint64_t newest_frame
)
{
	th03::app::NewClassicNetplayDatagram datagram{
		.type = th03::app::NewClassicNetplayDatagramType::input,
		.sender = local_player,
		.session_id = session_id,
		.contract_hash = contract_hash,
		.latest_frame = newest_frame,
		.confirmed_frame_count = session.confirmed_frame_count(),
	};
	for(std::size_t index = 0U;
		index < datagram.inputs.size() && index <= newest_frame; ++index) {
		const auto input = session.local_input(newest_frame - index);
		if(!input) {
			break;
		}
		datagram.inputs[index] = *input;
		++datagram.input_count;
	}
	return datagram.input_count != 0U &&
		send_netplay_datagram(socket, peer, datagram);
}

int run_direct_netplay(th03::app::DirectVsCliRequest parsed)
{
	using th03::app::DirectVsNetplayMode;
	using th03::app::NewClassicNetplayDatagram;
	using th03::app::NewClassicNetplayDatagramType;
	using th03::app::NewClassicNetplayMainSessionStatus;
	using th03::app::NewClassicNetplayPlayer;
	using th03::app::NewClassicNetplayPostMatchStatus;
	parsed.launch.process.classic_audio_lifecycle = true;
	const auto mode = parsed.netplay_mode;
	const auto local_player = mode == DirectVsNetplayMode::host ?
		NewClassicNetplayPlayer::player_one :
		NewClassicNetplayPlayer::player_two;
	const auto remote_player = mode == DirectVsNetplayMode::host ?
		NewClassicNetplayPlayer::player_two :
		NewClassicNetplayPlayer::player_one;
	auto negotiated = negotiate_direct_netplay_lobby(parsed);
	if(!negotiated) {
		return negotiated.error();
	}
	apply_netplay_contract(parsed, negotiated->contract);
	const auto created_contract_hash =
		th03::app::new_classic_netplay_match_contract_hash(
			negotiated->contract
		);
	if(!created_contract_hash) {
		return fail_netplay("netplay-contract", "invalid-match-lock");
	}
	const auto contract_hash = *created_contract_hash;
	auto socket = std::move(negotiated->socket);
	std::optional<th03::platform::UdpIpv4Endpoint> peer{negotiated->peer};
	auto lobby = std::move(negotiated->lobby);
	std::uint64_t session_id = lobby.session_id();
	const auto audio_data_root = parsed.launch.content.data_root;
	const auto post_match_content = parsed.launch.content;

	auto process = th03::app::load_direct_vs_process(std::move(parsed.launch));
	if(!process) {
		return fail_content(process.error());
	}
	const auto process_round_start = (*process)->round_start_observation();
	const auto process_start = process_round_start.start;
	auto created_replay_capture =
		th03::app::NewClassicNetplayReplayCapture::create(process_round_start);
	if(!created_replay_capture) {
		return fail_netplay("netplay-replay", "capture-create-failed");
	}
	auto replay_capture = std::move(*created_replay_capture);
	auto session = th03::app::NewClassicNetplayMainSession::create(
		local_player, std::move(*process)
	);
	if(!session) {
		return fail_netplay("netplay-session", "session-create-failed");
	}
	const auto initial_simulation_digest = (*session)->simulation_digest(0U);
	if(!initial_simulation_digest) {
		log_netplay_session_error(initial_simulation_digest.error());
		return fail_netplay("netplay-identity", "digest-create-failed");
	}
	auto created_audio = th03::app::ClassicAudioSession::create({
		.data_root = audio_data_root,
		.tick_rate = parsed.tick_rate,
		.bgm_active = true,
		.effects_active = true,
		.effects_follow_bgm = false,
	});
	if(!created_audio) {
		return fail("netplay-audio-create", created_audio.error());
	}
	auto audio_session = std::move(*created_audio);
	while(const auto command = (*session)->take_audio_command()) {
		if(auto applied = audio_session->apply(*command); !applied) {
			return fail("netplay-audio-command", applied.error());
		}
	}
	auto platform = th03::platform::SdlPlatform::create(platform_config());
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
	std::optional<th03::platform::SdlAudioDevice> audio_device{};
	auto created_device = th03::platform::SdlAudioDevice::create({
		.start_paused = true,
	});
	if(created_device) {
		audio_device.emplace(std::move(*created_device));
	} else {
		warn_audio_device("netplay-audio-device-create", created_device.error());
	}
	bool audio_started = false;
	std::vector<th03::audio::PmdCanonicalStereoSample> startup_silence{};
	const auto advance_audio_tick = [&]() -> std::expected<void, int> {
		auto tick = audio_session->advance_tick();
		if(!tick) {
			return std::unexpected(fail(
				"netplay-audio-tick", tick.error()
			));
		}
		if(!audio_device) {
			return {};
		}
		if(!audio_started && !tick->canonical_frames.empty() &&
			tick->canonical_frames.size() <=
				audio_device->maximum_queued_frames() / 3U) {
			startup_silence.assign(
				tick->canonical_frames.size() * 2U, {}
			);
			const auto prefilled = audio_device->submit(startup_silence);
			if(!prefilled) {
				warn_audio_device("netplay-audio-prefill", prefilled.error());
				audio_device.reset();
				return {};
			}
		}
		const auto submitted = audio_device->submit(tick->canonical_frames);
		if(!submitted) {
			warn_audio_device("netplay-audio-submit", submitted.error());
			audio_device.reset();
		} else if(!audio_started && !tick->canonical_frames.empty()) {
			const auto resumed = audio_device->resume();
			if(!resumed) {
				warn_audio_device("netplay-audio-resume", resumed.error());
				audio_device.reset();
			} else {
				audio_started = true;
			}
		}
		return {};
	};
	const auto publish_confirmed_audio = [&]() -> std::expected<void, int> {
		for(;;) {
			auto confirmed = (*session)->take_confirmed_audio_frame();
			if(!confirmed) {
				log_netplay_session_error(confirmed.error());
				return std::unexpected(fail_netplay(
					"netplay-audio", "journal-failed"
				));
			}
			if(!*confirmed) {
				return {};
			}
			for(std::size_t index = 0U;
				index < (**confirmed).command_count; ++index) {
				if(auto applied = audio_session->apply(
					(**confirmed).commands[index]
				); !applied) {
					return std::unexpected(fail(
						"netplay-audio-command", applied.error()
					));
				}
			}
			for(std::uint8_t vsync = 0U;
				vsync < (**confirmed).source_vsyncs; ++vsync) {
				if(const auto advanced = advance_audio_tick(); !advanced) {
					return std::unexpected(advanced.error());
				}
			}
		}
	};
	auto scheduler = th03::app::TickInputScheduler::create(
		parsed.tick_rate,
		th03::platform::sdl_ticks_now_ns(),
		th03::app::TickInputCatchUpPolicy::preserve_all
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
	auto surface = (*session)->compose_shown_surface(pixel_span);
	if(!surface) {
		return fail("compose", surface.error());
	}
	if(const auto presented = (*platform)->present(*surface); !presented) {
		return fail("present", presented.error());
	}

	const auto start_ns = th03::platform::sdl_ticks_now_ns();
	auto last_send_ns = start_ns - 250'000'000U;
	auto last_initial_digest_send_ns = start_ns - 250'000'000U;
	auto last_runtime_digest_send_ns = start_ns - 250'000'000U;
	auto last_receive_ns = start_ns;
	bool established = false;
	bool peer_started_input = false;
	bool retry_blocked_event = false;
	std::array<std::byte,
		th03::app::new_classic_netplay_datagram_bytes> receive_buffer{};
	std::optional<th03::app::NewClassicNetplayOwnedInput> stalled_input{};
	std::optional<std::uint64_t> newest_local_frame{};
	using ConfirmedSimulationDigest =
		th03::app::NewClassicNetplayConfirmedSimulationDigest;
	std::optional<ConfirmedSimulationDigest> local_runtime_digest{};
	std::optional<ConfirmedSimulationDigest> remote_runtime_digest{};
	std::optional<ConfirmedSimulationDigest> remote_runtime_digest_ack{};
	std::optional<ConfirmedSimulationDigest> verified_runtime_digest{};
	std::uint64_t verified_periodic_gameplay_frame = 0U;
	std::vector<std::uint8_t> local_deadline_missed{};
	std::unique_ptr<th03::app::NewClassicNetplayPostMatch> post_match{};
	bool runtime_digest_peer_ack = false;
	bool presentation_pending = false;
	std::uint64_t local_frames = 0U;
	std::cout << "status=match-locked\ncontract=" << contract_hash << '\n';
	std::cout.flush();
	const auto send_simulation_digest = [&](
		NewClassicNetplayDatagramType type,
		std::uint64_t frame,
		std::uint64_t confirmed_gameplay_frame_count,
		th03::app::NewClassicNetplaySimulationDigestKind kind,
		th03::app::NewClassicNetplayBoundaryProof boundary,
		const th03::app::AppSha256 &digest
	) -> bool {
		if(!peer || session_id == 0U) {
			return false;
		}
		return send_netplay_datagram(*socket, *peer, {
			.type = type,
			.sender = local_player,
			.session_id = session_id,
			.contract_hash = contract_hash,
			.latest_frame = frame,
			.confirmed_frame_count = confirmed_gameplay_frame_count,
			.simulation_digest = digest,
			.simulation_digest_kind = kind,
			.boundary_proof = boundary,
		});
	};
	const auto same_proof_identity = [](
		const ConfirmedSimulationDigest &left,
		const ConfirmedSimulationDigest &right
	) noexcept {
		return left.frame == right.frame &&
			left.confirmed_gameplay_frame_count ==
				right.confirmed_gameplay_frame_count &&
			left.kind == right.kind && left.boundary == right.boundary;
	};
	const auto same_proof = [&](const ConfirmedSimulationDigest &left,
		const ConfirmedSimulationDigest &right) noexcept {
		return same_proof_identity(left, right) &&
			left.digest == right.digest;
	};
	const auto leave_preview = [&]() -> int {
		if(peer && session_id != 0U) {
			const NewClassicNetplayDatagram departure{
				.type = NewClassicNetplayDatagramType::disconnect,
				.sender = local_player,
				.session_id = session_id,
				.contract_hash = contract_hash,
			};
			// UDP has no delivery acknowledgement. Three bounded copies make an
			// intentional local leave prompt without turning shutdown into a wait.
			for(std::uint8_t attempt = 0U; attempt < 3U; ++attempt) {
				(void)send_netplay_datagram(*socket, *peer, departure);
			}
		}
		std::cout << "status=left\n";
		return 0;
	};
	const auto match_runtime_digests = [&]() -> std::expected<void, int> {
		if(!local_runtime_digest || !remote_runtime_digest) {
			return {};
		}
		if(!same_proof_identity(
			*local_runtime_digest, *remote_runtime_digest
		)) {
			return std::unexpected(fail_netplay(
				"netplay-identity", "runtime-proof-order"
			));
		}
		if(local_runtime_digest->digest != remote_runtime_digest->digest) {
			return std::unexpected(fail_netplay(
				"netplay-identity", "runtime-simulation-mismatch"
			));
		}
		if(remote_runtime_digest_ack && !same_proof(
			*remote_runtime_digest_ack, *local_runtime_digest
		)) {
			return std::unexpected(fail_netplay(
				"netplay-identity", "runtime-proof-order"
			));
		}
		if(!send_simulation_digest(
			NewClassicNetplayDatagramType::simulation_digest_ack,
			local_runtime_digest->frame,
			local_runtime_digest->confirmed_gameplay_frame_count,
			local_runtime_digest->kind,
			local_runtime_digest->boundary,
			local_runtime_digest->digest
		)) {
			return std::unexpected(fail_netplay(
				"netplay-identity", "send-failed"
			));
		}
		verified_runtime_digest = *local_runtime_digest;
		verified_periodic_gameplay_frame = std::max(
			verified_periodic_gameplay_frame,
			(verified_runtime_digest->confirmed_gameplay_frame_count /
				th03::app::new_classic_netplay_simulation_digest_interval) *
				th03::app::new_classic_netplay_simulation_digest_interval
		);
		runtime_digest_peer_ack = remote_runtime_digest_ack.has_value();
		std::cout << "status=identity-confirmed\nidentity-frame=" <<
			verified_runtime_digest->frame << '\n';
		std::cout.flush();
		local_runtime_digest.reset();
		remote_runtime_digest.reset();
		remote_runtime_digest_ack.reset();
		return {};
	};
	const auto drain_local_runtime_digests = [&]() -> std::expected<void, int> {
		if(local_runtime_digest ||
			(verified_runtime_digest && !runtime_digest_peer_ack)) {
			return match_runtime_digests();
		}
		for(;;) {
			auto digest = (*session)->take_confirmed_simulation_digest();
			if(!digest) {
				log_netplay_session_error(digest.error());
				return std::unexpected(fail_netplay(
					"netplay-identity", "digest-journal-failed"
				));
			}
			if(!*digest) {
				return match_runtime_digests();
			}
			const bool periodic = (**digest).kind ==
				th03::app::NewClassicNetplaySimulationDigestKind::periodic;
			const bool boundary = (**digest).kind ==
				th03::app::NewClassicNetplaySimulationDigestKind::boundary;
			const auto expected_periodic = verified_periodic_gameplay_frame +
				th03::app::new_classic_netplay_simulation_digest_interval;
			if((periodic && ((**digest).boundary !=
					th03::app::NewClassicNetplayBoundaryProof::none ||
				(**digest).confirmed_gameplay_frame_count != expected_periodic)) ||
				(boundary && (**digest).boundary ==
					th03::app::NewClassicNetplayBoundaryProof::none) ||
				(!periodic && !boundary) ||
				(verified_runtime_digest &&
				 ((**digest).frame <= verified_runtime_digest->frame ||
				  (**digest).confirmed_gameplay_frame_count <
					verified_runtime_digest->confirmed_gameplay_frame_count))) {
				return std::unexpected(fail_netplay(
					"netplay-identity", "runtime-proof-lag"
				));
			}
			local_runtime_digest = **digest;
			if(!send_simulation_digest(
				NewClassicNetplayDatagramType::simulation_digest,
				local_runtime_digest->frame,
				local_runtime_digest->confirmed_gameplay_frame_count,
				local_runtime_digest->kind,
				local_runtime_digest->boundary,
				local_runtime_digest->digest
			)) {
				return std::unexpected(fail_netplay(
					"netplay-identity", "send-failed"
				));
			}
		}
	};
	const auto drain_confirmed_transcript = [&]() -> std::expected<void, int> {
		for(;;) {
			auto frame = (*session)->take_confirmed_input_frame();
			if(!frame) {
				log_netplay_session_error(frame.error());
				return std::unexpected(fail_netplay(
					"netplay-replay", "transcript-journal-failed"
				));
			}
			if(!*frame) {
				return {};
			}
			if((**frame).frame != replay_capture.frame_count() ||
				(**frame).frame >= local_deadline_missed.size()) {
				return std::unexpected(fail_netplay(
					"netplay-replay", "transcript-order-failed"
				));
			}
			if(auto appended = replay_capture.append_confirmed_frame(
				**frame,
				local_deadline_missed[static_cast<std::size_t>(
					(**frame).frame
				)] != 0U
			); !appended) {
				return std::unexpected(fail_netplay(
					"netplay-replay", "capture-append-failed"
				));
			}
		}
	};
	const auto classify_local_frame = [&](
		std::optional<std::uint64_t> frame
	) -> std::expected<void, int> {
		if(!frame) {
			return {};
		}
		if(*frame != local_deadline_missed.size()) {
			return std::unexpected(fail_netplay(
				"netplay-replay", "deadline-order-failed"
			));
		}
		const auto next_deadline = scheduler->next_deadline_ns();
		local_deadline_missed.push_back(static_cast<std::uint8_t>(
			next_deadline &&
			th03::platform::sdl_ticks_now_ns() >= *next_deadline
		));
		return {};
	};
	const auto drain_post_match_audio = [&]() -> std::expected<void, int> {
		while(post_match) {
			const auto command = post_match->take_audio_command();
			if(!command) {
				break;
			}
			if(auto applied = audio_session->apply(*command); !applied) {
				return std::unexpected(fail(
					"netplay-post-match-audio", applied.error()
				));
			}
		}
		return {};
	};

	const auto submit_input_datagram = [&](const NewClassicNetplayDatagram &packet)
		-> bool {
		if(packet.type != NewClassicNetplayDatagramType::input ||
			packet.sender != remote_player || packet.session_id != session_id ||
			packet.contract_hash != contract_hash) {
			return false;
		}
		for(std::size_t offset = packet.input_count; offset > 0U; --offset) {
			const auto index = offset - 1U;
			const auto frame = packet.latest_frame - index;
			const auto submitted = (*session)->submit_remote(
				frame, packet.inputs[index]
			);
			if(!submitted) {
				return false;
			}
			presentation_pending = presentation_pending ||
				submitted->surface_publishable;
		}
		return true;
	};
	const auto settle_confirmed_boundary = [&]() -> std::expected<void, int> {
		if((*session)->status() !=
			NewClassicNetplayMainSessionStatus::boundary_confirmed) {
			return {};
		}
		if(const auto digests = drain_local_runtime_digests(); !digests) {
			return digests;
		}
		const auto expected_boundary = (*session)->boundary() ?
			th03::app::new_classic_netplay_boundary_proof(
				*(*session)->boundary()
			) :
			th03::app::NewClassicNetplayBoundaryProof::none;
		if(local_runtime_digest || remote_runtime_digest ||
			remote_runtime_digest_ack || !verified_runtime_digest ||
			!runtime_digest_peer_ack) {
			return {};
		}
		if(verified_runtime_digest->kind !=
				th03::app::NewClassicNetplaySimulationDigestKind::boundary ||
			verified_runtime_digest->boundary != expected_boundary ||
			verified_runtime_digest->frame != (*session)->next_frame()) {
			return std::unexpected(fail_netplay(
				"netplay-boundary", "identity-missing"
			));
		}
		const auto continued = (*session)->continue_confirmed_boundary();
		if(!continued) {
			std::cerr << "netplay-boundary-status=" <<
				((*session)->boundary() ? static_cast<unsigned>(
					*(*session)->boundary()
				) : 255U) << " netplay-input-context=" <<
				static_cast<unsigned>((*session)->input_context()) << '\n';
			log_netplay_session_error(continued.error());
			return std::unexpected(fail_netplay(
				"netplay-boundary", "lifecycle-failed"
			));
		}
		presentation_pending = presentation_pending ||
			continued->surface_publishable;
		if(const auto transcript = drain_confirmed_transcript(); !transcript) {
			return std::unexpected(transcript.error());
		}
		if(*continued->boundary == th03::StepStatus::round_transition) {
			if(auto started = replay_capture.round_started(
				(*session)->process().round_start_observation()
			); !started) {
				return std::unexpected(fail_netplay(
					"netplay-replay", "round-start-failed"
				));
			}
		}
		std::cout << "status=boundary-confirmed\nboundary=" <<
			static_cast<unsigned>(*continued->boundary) << "\nframe=" <<
			*continued->frame << '\n';
		std::cout.flush();
		return {};
	};

	for(;;) {
		const auto now_ns = th03::platform::sdl_ticks_now_ns();
		if(now_ns - start_ns > 30'000'000'000ULL && !established) {
			return fail_netplay("netplay-handshake", "timeout");
		}
		if(established && !post_match &&
			now_ns - last_receive_ns > 5'000'000'000ULL) {
			return fail_netplay("netplay-peer", "peer-timeout");
		}

		if(!retry_blocked_event) {
			auto events = mapper->pump_events(*scheduler, true);
			if(!events) {
				return fail("input-pump", events.error());
			}
			retry_blocked_event = events->queue_blocked;
		}

		for(;;) {
			const auto received = socket->receive(receive_buffer);
			if(!received) {
				return fail_netplay("netplay-receive", "socket-failed");
			}
			if(!*received) {
				break;
			}
			if((*received)->size ==
				th03::app::new_classic_netplay_lobby_bytes) {
				const auto lobby_packet =
					th03::app::decode_new_classic_netplay_lobby_datagram(
						std::span(receive_buffer).first((*received)->size)
					);
				if(mode == DirectVsNetplayMode::host && lobby_packet && peer &&
					*peer == (*received)->sender &&
					(lobby_packet->type == th03::app::
						NewClassicNetplayLobbyDatagramType::join_request ||
					 lobby_packet->type == th03::app::
						NewClassicNetplayLobbyDatagramType::join_ready) &&
					lobby.receive(*lobby_packet)) {
					if(const auto response = lobby.outbound(); response) {
						(void)send_netplay_lobby_datagram(
							*socket, *peer, *response
						);
					}
				}
				continue;
			}
			const auto packet = th03::app::decode_new_classic_netplay_datagram(
				std::span(receive_buffer).first((*received)->size)
			);
			if(!packet || packet->contract_hash != contract_hash) {
				continue;
			}
			if(mode == DirectVsNetplayMode::host &&
				packet->type == NewClassicNetplayDatagramType::hello &&
				packet->sender == remote_player &&
				(!peer || *peer == (*received)->sender)) {
				peer = (*received)->sender;
				const NewClassicNetplayDatagram accepted{
					.type = NewClassicNetplayDatagramType::accept,
					.sender = local_player,
					.session_id = session_id,
					.contract_hash = contract_hash,
				};
				if(!send_netplay_datagram(*socket, *peer, accepted)) {
					return fail_netplay("netplay-accept", "send-failed");
				}
				last_receive_ns = now_ns;
				continue;
			}
			if(!peer || *peer != (*received)->sender) {
				continue;
			}
			if(mode == DirectVsNetplayMode::join &&
				packet->type == NewClassicNetplayDatagramType::accept &&
				packet->sender == remote_player && packet->session_id != 0U) {
				if(session_id != 0U && session_id != packet->session_id) {
					continue;
				}
				session_id = packet->session_id;
				last_receive_ns = now_ns;
				continue;
			}
			if(packet->type ==
					NewClassicNetplayDatagramType::simulation_digest &&
				packet->sender == remote_player &&
				packet->session_id == session_id) {
				last_receive_ns = now_ns;
				if(packet->simulation_digest_kind ==
					th03::app::NewClassicNetplaySimulationDigestKind::initial) {
					if(packet->simulation_digest != *initial_simulation_digest) {
						return fail_netplay(
							"netplay-identity", "initial-simulation-mismatch"
						);
					}
					if(!established) {
						established = true;
						if(const auto rebased = scheduler->rebase_after_modal(
							now_ns
						); !rebased) {
							return fail_tick(rebased.error());
						}
						std::cout << "status=connected\nsession=" << session_id <<
							"\nidentity-frame=0\n";
						std::cout.flush();
					}
					continue;
				}
				if(!established) {
					return fail_netplay(
						"netplay-identity", "runtime-proof-order"
					);
				}
				const ConfirmedSimulationDigest received_digest{
					.frame = packet->latest_frame,
					.confirmed_gameplay_frame_count =
						packet->confirmed_frame_count,
					.kind = packet->simulation_digest_kind,
					.boundary = packet->boundary_proof,
					.digest = packet->simulation_digest,
				};
				if(verified_runtime_digest &&
					received_digest.frame == verified_runtime_digest->frame) {
					if(!same_proof(
						received_digest, *verified_runtime_digest
					)) {
						return fail_netplay(
							"netplay-identity", "runtime-simulation-mismatch"
						);
					}
					if(!send_simulation_digest(
						NewClassicNetplayDatagramType::simulation_digest_ack,
						verified_runtime_digest->frame,
						verified_runtime_digest->
							confirmed_gameplay_frame_count,
						verified_runtime_digest->kind,
						verified_runtime_digest->boundary,
						verified_runtime_digest->digest
					)) {
						return fail_netplay(
							"netplay-identity", "send-failed"
						);
					}
					continue;
				}
				if(verified_runtime_digest &&
					received_digest.frame < verified_runtime_digest->frame) {
					continue;
				}
				const bool periodic = received_digest.kind ==
					th03::app::NewClassicNetplaySimulationDigestKind::periodic;
				const bool boundary = received_digest.kind ==
					th03::app::NewClassicNetplaySimulationDigestKind::boundary;
				const auto expected_periodic =
					verified_periodic_gameplay_frame +
					th03::app::new_classic_netplay_simulation_digest_interval;
				if((periodic && (received_digest.boundary !=
						th03::app::NewClassicNetplayBoundaryProof::none ||
					received_digest.confirmed_gameplay_frame_count !=
						expected_periodic)) ||
					(boundary && received_digest.boundary ==
						th03::app::NewClassicNetplayBoundaryProof::none) ||
					(!periodic && !boundary) ||
					(verified_runtime_digest &&
					 received_digest.confirmed_gameplay_frame_count <
						verified_runtime_digest->confirmed_gameplay_frame_count) ||
					(remote_runtime_digest &&
					 !same_proof(*remote_runtime_digest, received_digest))) {
					return fail_netplay(
						"netplay-identity", "runtime-proof-order"
					);
				}
				remote_runtime_digest = received_digest;
				if(const auto matched = match_runtime_digests(); !matched) {
					return matched.error();
				}
				continue;
			}
			if(packet->type ==
					NewClassicNetplayDatagramType::simulation_digest_ack &&
				packet->sender == remote_player &&
				packet->session_id == session_id) {
				last_receive_ns = now_ns;
				if(!established) {
					return fail_netplay(
						"netplay-identity", "runtime-proof-order"
					);
				}
				const ConfirmedSimulationDigest received_ack{
					.frame = packet->latest_frame,
					.confirmed_gameplay_frame_count =
						packet->confirmed_frame_count,
					.kind = packet->simulation_digest_kind,
					.boundary = packet->boundary_proof,
					.digest = packet->simulation_digest,
				};
				if(verified_runtime_digest &&
					received_ack.frame == verified_runtime_digest->frame) {
					if(!same_proof(received_ack, *verified_runtime_digest)) {
						return fail_netplay(
							"netplay-identity", "runtime-simulation-mismatch"
						);
					}
					const bool peer_was_already_acknowledged =
						runtime_digest_peer_ack;
					runtime_digest_peer_ack = true;
					// A peer that repeats an acknowledgement after we already
					// observed theirs is still waiting for ours: the prior reply
					// was lost. Reply once; the peer's first observation does not
					// echo, so this cannot become an acknowledgement loop.
					if(peer_was_already_acknowledged &&
						!send_simulation_digest(
							NewClassicNetplayDatagramType::
								simulation_digest_ack,
							verified_runtime_digest->frame,
							verified_runtime_digest->
								confirmed_gameplay_frame_count,
							verified_runtime_digest->kind,
							verified_runtime_digest->boundary,
							verified_runtime_digest->digest
						)) {
						return fail_netplay(
							"netplay-identity", "send-failed"
						);
					}
					continue;
				}
				if(!local_runtime_digest ||
					!same_proof(received_ack, *local_runtime_digest) ||
					(remote_runtime_digest_ack &&
					 !same_proof(*remote_runtime_digest_ack, received_ack))) {
					return fail_netplay(
						"netplay-identity", "runtime-proof-order"
					);
				}
				remote_runtime_digest_ack = received_ack;
				if(remote_runtime_digest &&
					!same_proof(*remote_runtime_digest, received_ack)) {
					return fail_netplay(
						"netplay-identity", "runtime-simulation-mismatch"
					);
				}
				remote_runtime_digest = received_ack;
				if(const auto matched = match_runtime_digests(); !matched) {
					return matched.error();
				}
				continue;
			}
			if(packet->type == NewClassicNetplayDatagramType::disconnect &&
				packet->sender == remote_player &&
				packet->session_id == session_id) {
				std::cout << "status=peer-left\n";
				return 0;
			}
			if(established &&
				packet->type == NewClassicNetplayDatagramType::input &&
				submit_input_datagram(*packet)) {
				last_receive_ns = now_ns;
				peer_started_input = true;
				if(const auto transcript = drain_confirmed_transcript();
					!transcript) {
					return transcript.error();
				}
			}
		}
		if(established) {
			if(const auto digests = drain_local_runtime_digests(); !digests) {
				return digests.error();
			}
		}
		if(const auto settled = settle_confirmed_boundary(); !settled) {
			return settled.error();
		}
		if(const auto published = publish_confirmed_audio(); !published) {
			return published.error();
		}

		if(!established && mode == DirectVsNetplayMode::join &&
			session_id == 0U &&
			now_ns - last_send_ns >= 100'000'000U) {
			const NewClassicNetplayDatagram hello{
				.type = NewClassicNetplayDatagramType::hello,
				.sender = local_player,
				.contract_hash = contract_hash,
			};
			if(!send_netplay_datagram(*socket, *peer, hello)) {
				return fail_netplay("netplay-hello", "send-failed");
			}
			last_send_ns = now_ns;
		}
		if(peer && session_id != 0U && !peer_started_input &&
			now_ns - last_initial_digest_send_ns >= 100'000'000U) {
			if(!send_simulation_digest(
				NewClassicNetplayDatagramType::simulation_digest,
				0U, 0U,
				th03::app::NewClassicNetplaySimulationDigestKind::initial,
				th03::app::NewClassicNetplayBoundaryProof::none,
				*initial_simulation_digest
			)) {
				return fail_netplay("netplay-identity", "send-failed");
			}
			last_initial_digest_send_ns = now_ns;
		}

		if(established && stalled_input && (*session)->status() !=
			NewClassicNetplayMainSessionStatus::complete) {
			const auto frame = (*session)->next_frame();
			const auto advanced = (*session)->advance_local(*stalled_input);
			if(!advanced) {
				log_netplay_session_error(advanced.error());
				return fail_netplay("netplay-advance", "session-failed");
			}
			if(advanced->status !=
					NewClassicNetplayMainSessionStatus::prediction_stalled &&
				advanced->status !=
					NewClassicNetplayMainSessionStatus::remote_waiting) {
				stalled_input.reset();
				if(advanced->frame) {
					newest_local_frame = frame;
					++local_frames;
				}
				if(const auto classified = classify_local_frame(advanced->frame);
					!classified) {
					return classified.error();
				}
				presentation_pending = presentation_pending ||
					advanced->surface_publishable;
				if(const auto transcript = drain_confirmed_transcript();
					!transcript) {
					return transcript.error();
				}
			}
			if(const auto settled = settle_confirmed_boundary(); !settled) {
				return settled.error();
			}
		}

		if(established && !stalled_input && (*session)->status() !=
			NewClassicNetplayMainSessionStatus::complete) {
			for(;;) {
				const auto due = scheduler->pop_due(now_ns);
				if(!due) {
					return fail_tick(due.error());
				}
				if(!*due) {
					break;
				}
				if((**due).app_quit_requested ||
					(((**due).input.system & 0x1000U) != 0U)) {
					return leave_preview();
				}
				const auto input = netplay_owned_input((**due).input);
				const auto frame = (*session)->next_frame();
				const auto advanced = (*session)->advance_local(input);
				if(!advanced) {
					log_netplay_session_error(advanced.error());
					return fail_netplay("netplay-advance", "session-failed");
				}
				const bool input_staged = advanced->frame.has_value() ||
					advanced->status ==
						NewClassicNetplayMainSessionStatus::prediction_stalled ||
					advanced->status ==
						NewClassicNetplayMainSessionStatus::remote_waiting;
				if(input_staged) {
					newest_local_frame = frame;
					if(!send_netplay_input_history(
						*socket, *peer, **session, local_player, session_id,
						contract_hash, frame
					)) {
						return fail_netplay("netplay-input", "send-failed");
					}
					last_send_ns = now_ns;
				}
				presentation_pending = presentation_pending ||
					advanced->surface_publishable;
				if(const auto classified = classify_local_frame(advanced->frame);
					!classified) {
					return classified.error();
				}
				if(const auto transcript = drain_confirmed_transcript();
					!transcript) {
					return transcript.error();
				}
				if(advanced->status ==
						NewClassicNetplayMainSessionStatus::prediction_stalled ||
					advanced->status ==
						NewClassicNetplayMainSessionStatus::remote_waiting) {
					stalled_input = input;
					break;
				}
				if(advanced->frame) {
					++local_frames;
				}
				if(const auto settled = settle_confirmed_boundary(); !settled) {
					return settled.error();
				}
				if(advanced->status !=
					NewClassicNetplayMainSessionStatus::running) {
					break;
				}
				if(retry_blocked_event) {
					auto retried = mapper->pump_events(*scheduler, true);
					if(!retried) {
						return fail("input-pump", retried.error());
					}
					retry_blocked_event = retried->queue_blocked;
				}
			}
		}
		if(post_match && post_match->status() ==
			NewClassicNetplayPostMatchStatus::running) {
			for(;;) {
				const auto due = scheduler->pop_due(now_ns);
				if(!due) {
					return fail_tick(due.error());
				}
				if(!*due) {
					break;
				}
				const bool dismiss_pressed = (**due).input.system != 0U ||
					(**due).input.p1 != 0U || (**due).input.p2 != 0U;
				auto advanced = post_match->advance(
					audio_session->status(), dismiss_pressed,
					(**due).app_quit_requested
				);
				if(!advanced) {
					return fail("netplay-post-match", advanced.error());
				}
				if(const auto drained = drain_post_match_audio(); !drained) {
					return drained.error();
				}
				if(const auto audio = advance_audio_tick(); !audio) {
					return audio.error();
				}
				presentation_pending = presentation_pending ||
					advanced->surface_changed;
				if(advanced->status ==
					NewClassicNetplayPostMatchStatus::quit_requested) {
					return leave_preview();
				}
				if(advanced->status ==
					NewClassicNetplayPostMatchStatus::complete) {
					break;
				}
			}
		}

		if(established && !post_match && newest_local_frame && peer &&
			now_ns - last_send_ns >= 50'000'000U) {
			if(!send_netplay_input_history(
				*socket, *peer, **session, local_player, session_id,
				contract_hash, *newest_local_frame
			)) {
				return fail_netplay("netplay-resend", "send-failed");
			}
			last_send_ns = now_ns;
		}
		if(established) {
			if(const auto digests = drain_local_runtime_digests(); !digests) {
				return digests.error();
			}
			const auto &outbound_digest = local_runtime_digest ?
				local_runtime_digest : verified_runtime_digest;
			if(outbound_digest && now_ns - last_runtime_digest_send_ns >=
				100'000'000U &&
				(local_runtime_digest || !runtime_digest_peer_ack)) {
				if(!send_simulation_digest(
					local_runtime_digest ?
						NewClassicNetplayDatagramType::simulation_digest :
						NewClassicNetplayDatagramType::simulation_digest_ack,
					outbound_digest->frame,
					outbound_digest->confirmed_gameplay_frame_count,
					outbound_digest->kind,
					outbound_digest->boundary,
					outbound_digest->digest
				)) {
					return fail_netplay("netplay-identity", "send-failed");
				}
				last_runtime_digest_send_ns = now_ns;
			}
		}
		if(const auto published = publish_confirmed_audio(); !published) {
			return published.error();
		}
		const bool runtime_proof_pending = local_runtime_digest ||
			remote_runtime_digest || remote_runtime_digest_ack ||
			(verified_runtime_digest && !runtime_digest_peer_ack);
		if((*session)->status() ==
				NewClassicNetplayMainSessionStatus::complete &&
			!runtime_proof_pending && !post_match) {
			if(!(*session)->transition()) {
				return fail_netplay(
					"netplay-post-match", "transition-missing"
				);
			}
			auto created = th03::app::NewClassicNetplayPostMatch::create({
				.content = post_match_content,
				.process_start = process_start,
				.terminal = (*session)->process().canonical_state(),
				.transition = *(*session)->transition(),
			});
			if(!created) {
				return fail("netplay-post-match-create", created.error());
			}
			post_match = std::move(*created);
			if(const auto drained = drain_post_match_audio(); !drained) {
				return drained.error();
			}
			const auto resumed_at = th03::platform::sdl_ticks_now_ns();
			if(auto rebased = scheduler->rebase_after_modal(resumed_at);
				!rebased) {
				return fail_tick(rebased.error());
			}
			if(auto reset = mapper->reset_after_modal(*scheduler, resumed_at);
				!reset) {
				return fail("netplay-post-match-rebase", reset.error());
			}
			retry_blocked_event = false;
			presentation_pending = true;
			std::cout << "status=post-match\n";
			std::cout.flush();
		}

		if(presentation_pending) {
			if(post_match) {
				auto composed = post_match->compose_shown_surface(pixel_span);
				if(!composed) {
					return fail("netplay-post-match-compose", composed.error());
				}
				surface = *composed;
			} else {
				surface = (*session)->compose_shown_surface(pixel_span);
				if(!surface) {
					return fail("compose", surface.error());
				}
			}
			if(const auto presented = (*platform)->present(*surface); !presented) {
				return fail("present", presented.error());
			}
			presentation_pending = false;
		}
		if(post_match && post_match->status() ==
			NewClassicNetplayPostMatchStatus::complete) {
			if(!(*session)->transition()) {
				return fail_netplay("netplay-replay", "transition-missing");
			}
			auto completed = replay_capture.finish_mainl(
				(*session)->transition()->resident
			);
			if(!completed) {
				return fail_netplay("netplay-replay", "finalize-failed");
			}
			auto encoded = th03::formats::write_v13_replay(*completed);
			if(!encoded) {
				return fail_netplay("netplay-replay", "encode-failed");
			}
			if(parsed.netplay_replay_output) {
				auto published = th03::app::commit_exact_replay_file(
					*parsed.netplay_replay_output, *encoded
				);
				if(!published) {
					std::cerr << "netplay-replay-store-code=" <<
						static_cast<unsigned>(published.error().code) << '\n';
					return fail_netplay(
						"netplay-replay", "publish-failed"
					);
				}
			}
			std::cout << "status=match-complete\nframes=" <<
				(*session)->confirmed_frame_count() << "\nreplay-frames=" <<
				replay_capture.frame_count() << "\nreplay-ready=1\nreplay-bytes=" <<
				encoded->size() << "\nreplay-published=" <<
				(parsed.netplay_replay_output ? 1 : 0) << '\n';
			return 0;
		}
		if(parsed.smoke_ticks && local_frames >= *parsed.smoke_ticks &&
			(*session)->confirmed_frame_count() >= *parsed.smoke_ticks &&
			!runtime_proof_pending) {
			std::cout << "status=smoke-complete\nframes=" << local_frames
				<< "\nreplay-frames=" << replay_capture.frame_count()
				<< "\nreplay-ready=0\n";
			return 0;
		}

		if(!established) {
			// Discard pre-match source deadlines while still applying/retaining
			// physical state; the connected rebase starts frame zero cleanly.
			for(;;) {
				const auto due = scheduler->pop_due(now_ns);
				if(!due) {
					return fail_tick(due.error());
				}
				if(!*due) {
					break;
				}
				if((**due).app_quit_requested ||
					(((**due).input.system & 0x1000U) != 0U)) {
					return leave_preview();
				}
			}
		}
		SDL_DelayPrecise(500'000U);
	}
}

int run_direct_vs(th03::app::DirectVsCliRequest parsed)
{
	if(parsed.netplay_mode != th03::app::DirectVsNetplayMode::none) {
		return run_direct_netplay(std::move(parsed));
	}
	auto process = th03::app::load_direct_vs_process(std::move(parsed.launch));
	if(!process) {
		return fail_content(process.error());
	}
	auto runner = th03::app::MainDirectVsRunner::create(std::move(*process));
	if(!runner) {
		return fail("runner-create", runner.error());
	}

	auto platform = th03::platform::SdlPlatform::create(platform_config());
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
		th03::platform::sdl_ticks_now_ns(),
		parsed.smoke_ticks ?
			th03::app::TickInputCatchUpPolicy::preserve_all :
			th03::app::TickInputCatchUpPolicy::discard_missed_deadlines
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
		if(!presentation_pending) {
			if(auto refreshed = refresh_host_presentability(
				**platform, "presentability-refresh"
			); !refreshed) {
				return refreshed.error();
			}
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

ExperienceRunResult run_classic_session(
	th03::app::ClassicCliRequest parsed,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper,
	std::shared_ptr<const th03::app::NewClassicReplayPatchAssets>
		patch_assets = {}
)
{
	const auto data_root = parsed.launch.content.data_root;
	const auto config_path = parsed.launch.content.config;
	const auto new_classic_settings_path =
		parsed.launch.content.new_classic_settings;
	const bool new_classic =
		parsed.mode == th03::app::ClassicLaunchMode::new_classic;
	if(parsed.rhythm_rom && parsed.rhythm_pcm) {
		std::cerr << "status=error\nstage=audio-profile\n"
			"code=conflicting-rhythm-profiles\n";
		return {.exit_code = 1};
	}
	std::vector<std::byte> rhythm_rom{};
	if(parsed.rhythm_rom) {
		auto loaded = load_rhythm_rom(*parsed.rhythm_rom);
		if(!loaded) {
			return {.exit_code = fail("rhythm-rom", loaded.error())};
		}
		rhythm_rom = std::move(*loaded);
	}
	LoadedRhythmPcm rhythm_pcm{};
	if(parsed.rhythm_pcm) {
		auto loaded = load_rhythm_pcm(*parsed.rhythm_pcm);
		if(!loaded) {
			return {.exit_code = fail("rhythm-pcm", loaded.error())};
		}
		rhythm_pcm = std::move(*loaded);
	}
	parsed.launch.fm_available = true;
	parsed.launch.experience =
		new_classic ?
			th03::app::ExperienceProfile::new_classic :
			th03::app::ExperienceProfile::classic;
	parsed.launch.content.replay_root = parsed.replay_root;
	if(parsed.capture_replay) {
		auto replay = th03::app::load_replay_file(*parsed.capture_replay);
		if(!replay) {
			return {.exit_code = fail("capture-replay-load", replay.error())};
		}
		if(!replay->document) {
			std::cerr << "status=error\nstage=capture-replay-load\n"
				"code=file-missing\n";
			return {.exit_code = 1};
		}
		parsed.launch.initial_route =
			th03::app::ClassicRunnerInitialRoute::new_classic_replay;
		parsed.launch.initial_replay = th03::app::ClassicRunnerInitialReplay{
			.document = std::move(*replay->document),
			.checkpoint_index = parsed.capture_checkpoint,
		};
	}
	auto active_key_config = parsed.launch.new_classic_key_config;
	auto active_joy_config = parsed.launch.new_classic_joy_config;
	auto active_device_assignment =
		parsed.launch.new_classic_device_assignment;
	const auto online_root =
		new_classic_settings_path.parent_path() / "ONLINE";
#if defined(_WIN32)
	std::optional<th03::platform::WindowsOnlineTransport> online_transport{};
#endif
	// The platform transport must outlive the worker that refers to it.
	std::optional<th03::app::NewClassicOnlineBoardRefreshTask>
		online_refresh_task{};
	std::optional<th03::app::NewClassicOnlineReplayDownloadTask>
		online_replay_download_task{};
	std::optional<th03::app::NewClassicOnlineReplayId>
		online_replay_download_id{};
	const auto overlay_online_downloads = [&](auto &document) {
		for(auto &record : document.replay_records) {
			record.availability =
				th03::app::NewClassicOnlineReplayAvailability::metadata_only;
			auto downloaded =
				th03::app::probe_new_classic_online_replay_download(
					record, parsed.replay_root
				);
			if(downloaded && *downloaded) {
				record.availability = th03::app::
					NewClassicOnlineReplayAvailability::downloaded;
			} else if(!downloaded) {
				std::cerr << "status=warning\n"
					"stage=online-replay-probe\nrank="
					<< static_cast<unsigned>(record.category.rank)
					<< "\ncharacter="
					<< static_cast<unsigned>(record.category.character)
					<< "\ncode="
					<< static_cast<unsigned>(downloaded.error().code) << '\n';
			}
		}
	};
	if(new_classic) {
		for(std::uint8_t rank = 0U; rank < 4U; ++rank) {
			for(std::uint8_t character = 0U; character < 9U; ++character) {
				const th03::app::NewClassicOnlineCategory category{
					.mode = th03::app::NewClassicOnlineMode::story,
					.rank = rank,
					.character = character,
				};
				const auto path =
					th03::app::new_classic_online_board_cache_path(
						online_root, category
					);
				auto loaded = th03::app::load_new_classic_online_store(path);
				if(!loaded) {
					std::cerr << "status=warning\nstage=online-cache-load\n"
						"rank=" << static_cast<unsigned>(rank) << "\ncharacter="
						<< static_cast<unsigned>(character) << "\ncode="
						<< static_cast<unsigned>(loaded.error().code) << '\n';
					continue;
				}
				if(loaded->document.board_category) {
					overlay_online_downloads(loaded->document);
					parsed.launch.new_classic_online_boards.push_back(
						std::move(loaded->document)
					);
				}
			}
		}
	}
	auto runner = th03::app::ClassicRunner::load(std::move(parsed.launch));
	if(!runner) {
		return {.exit_code = fail("th03-load", runner.error())};
	}
	auto identity_output_result = pixel_oracle_identity_output(
		platform.oracle_enabled()
	);
	if(!identity_output_result) {
		std::cerr << "status=error\nstage=pixel-oracle-identity\ncode="
			<< identity_output_result.error() << '\n';
		return {.exit_code = 1};
	}
	auto identity_output = std::move(*identity_output_result);
	std::optional<th03::app::PixelOracleCaptureSelector>
		pixel_oracle_selector{};
	if(const auto selector = environment_value(
		pixel_oracle_selector_environment
	)) {
		if(!parsed.capture_replay || !identity_output.enabled()) {
			std::cerr << "status=error\nstage=pixel-oracle-selector\n"
				"code=selector-requires-replay-identity-capture\n";
			return {.exit_code = 1};
		}
		auto parsed_selector =
			th03::app::parse_pixel_oracle_capture_selector(*selector);
		if(!parsed_selector) {
			std::cerr << "status=error\nstage=pixel-oracle-selector\ncode="
				<< parsed_selector.error() << '\n';
			return {.exit_code = 1};
		}
		pixel_oracle_selector = *parsed_selector;
	}
	// Authenticate the selected game corpus before creating or recovering a
	// persistence file. A misspelled/incomplete content root must remain a
	// read-only startup failure rather than leaving a stray YUME.CFG behind.
	th03::app::ClassicConfigOptions persisted_options{};
	std::uint8_t master_volume_percent = 100U;
	std::uint8_t music_volume_percent = 100U;
	std::uint8_t effect_volume_percent = 100U;
	bool mute_when_unfocused = false;
	th03::app::NewClassicWindowPresentation window_presentation{};
	if(new_classic) {
		auto persisted = th03::app::load_new_classic_settings_store(
			new_classic_settings_path, config_path
		);
		if(!persisted) {
			return {.exit_code = fail("settings-load", persisted.error())};
		}
		persisted_options = persisted->settings.options;
		master_volume_percent = persisted->settings.master_volume_percent;
		music_volume_percent = persisted->settings.music_volume_percent;
		effect_volume_percent = persisted->settings.effect_volume_percent;
		mute_when_unfocused = persisted->settings.mute_when_unfocused;
		window_presentation = persisted->settings.window;
		if(auto applied = platform.apply_window_presentation(
			platform_window_presentation(window_presentation)
		); !applied) {
			return {.exit_code = fail("window-presentation", applied.error())};
		}
	} else {
		auto persisted = th03::app::load_classic_config_store(config_path);
		if(!persisted) {
			return {.exit_code = fail("config-load", persisted.error())};
		}
		persisted_options = persisted->options;
	}
	if(new_classic) {
		if(const auto origin = environment_value(online_origin_environment)) {
			if(platform.oracle_enabled() || parsed.capture_replay) {
				std::cerr << "status=warning\nstage=online-refresh-start\n"
					"code=disabled-during-capture\n";
#if defined(_WIN32)
			} else if(auto created =
				th03::platform::create_windows_online_transport(*origin)) {
				std::error_code directory_error{};
				std::filesystem::create_directories(
					online_root, directory_error
				);
				if(directory_error) {
					std::cerr << "status=warning\nstage=online-refresh-start\n"
						"code=cache-directory\nsystem="
						<< directory_error.value() << '\n';
				} else {
					online_transport.emplace(std::move(*created));
					std::vector<th03::app::NewClassicOnlineCategory> categories{};
					categories.reserve(9U);
					const auto rank = static_cast<std::uint8_t>(
						persisted_options.rank
					);
					for(std::uint8_t character = 0U; character < 9U; ++character) {
						categories.push_back({
							.mode = th03::app::NewClassicOnlineMode::story,
							.rank = rank,
							.character = character,
						});
					}
					auto started =
						th03::app::start_new_classic_online_board_refresh(
							std::move(categories), online_root,
							online_transport->transport()
						);
					if(started) {
						online_refresh_task.emplace(std::move(*started));
					} else {
						std::cerr << "status=warning\n"
							"stage=online-refresh-start\ncode="
							<< static_cast<unsigned>(started.error().code) << '\n';
						online_transport.reset();
					}
				}
			} else {
				std::cerr << "status=warning\nstage=online-refresh-start\ncode="
					<< static_cast<unsigned>(created.error().code) << '\n';
#else
			} else {
				std::cerr << "status=warning\nstage=online-refresh-start\n"
					"code=platform-unavailable\n";
#endif
			}
		}
	}
	auto created_session = th03::app::ClassicAudioSession::create({
		.data_root = data_root,
		.tick_rate = parsed.tick_rate,
		.bgm_active = persisted_options.bgm_mode ==
			th03::app::ClassicBgmMode::fm,
		.effects_active = !new_classic || persisted_options.sfx_enabled,
		.effects_follow_bgm = !new_classic,
		.music_volume_percent = new_classic ? music_volume_percent :
			std::uint8_t{100},
		.effect_volume_percent = new_classic ? effect_volume_percent :
			std::uint8_t{100},
		.rhythm_rom = std::span<const std::byte>(rhythm_rom),
		.rhythm_pcm = rhythm_pcm.view(),
	});
	if(!created_session) {
		return {
			.exit_code = fail("audio-session-create", created_session.error())
		};
	}
	auto audio_session = std::move(*created_session);
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
		return {.exit_code = fail("audio-command", drained.error())};
	}
	if(new_classic && !parsed.capture_replay) {
		if(auto discarded = th03::app::discard_replay_fallback_on_startup(
			parsed.replay_root
		); !discarded) {
			return {
				.exit_code = fail("replay-fallback-remove", discarded.error())
			};
		}
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
		return {.exit_code = fail("compose", surface.error())};
	}
	const auto bootstrap_identity = (*runner)->pixel_oracle_bootstrap_identity();
	const bool bootstrap_selected = !pixel_oracle_selector ||
		pixel_oracle_selector->matches(bootstrap_identity);
	if(bootstrap_selected) {
		auto presented = platform.present(*surface);
		if(!presented) {
			return {.exit_code = fail("present", presented.error())};
		}
		if(identity_output.enabled() &&
			(!presented->oracle_publication || !th03::app::write_t3ali1_event(
				identity_output.stream, identity_output.fixture_sha256,
				bootstrap_identity, presented->oracle_publication
			))) {
			std::cerr << "status=error\nstage=pixel-oracle-identity\n"
				"code=identity-oracle-write-failed\n";
			return {.exit_code = 1};
		}
		if(pixel_oracle_selector) {
			return {.disposition = ExperienceRunDisposition::smoke_complete};
		}
	} else if(identity_output.enabled() &&
		!th03::app::write_t3ali1_event(
			identity_output.stream, identity_output.fixture_sha256,
			bootstrap_identity, std::nullopt
		)) {
		std::cerr << "status=error\nstage=pixel-oracle-identity\n"
			"code=identity-oracle-write-failed\n";
		return {.exit_code = 1};
	}

	std::optional<th03::platform::SdlAudioDevice> audio_device{};
	if(!pixel_oracle_selector) {
		auto created_device = th03::platform::SdlAudioDevice::create({
			.start_paused = true,
		});
		if(created_device) {
			audio_device.emplace(std::move(*created_device));
		} else {
			warn_audio_device("audio-device-create", created_device.error());
		}
	}
	bool audio_started = false;
	bool audio_focus_muted = false;
	std::vector<th03::audio::PmdCanonicalStereoSample> startup_silence{};
	std::vector<th03::audio::PmdCanonicalStereoSample> gain_frames{};
	auto scheduler = th03::app::TickInputScheduler::create(
		parsed.tick_rate,
		th03::platform::sdl_ticks_now_ns(),
		th03::app::classic_interactive_catch_up_policy
	);
	if(!scheduler) {
		return {.exit_code = fail_tick(scheduler.error())};
	}
	bool retry_blocked_event = false;
	bool presentation_pending = false;
	std::optional<std::uint64_t> pending_publication_tick{};
	std::uint32_t pending_publication_ticks = 0U;
	std::optional<th03::app::PixelOracleIdentity>
		pending_publication_identity{};
	std::uint64_t logical_ticks = 0U;
	std::optional<th03::formats::ReplayDocument> completed_replay{};
	auto sync_audio_focus_mute = [&]() noexcept {
		const bool desired = new_classic && mute_when_unfocused &&
			!mapper.main_window_focused();
		if(desired && !audio_focus_muted && audio_device) {
			if(auto cleared = audio_device->clear(); !cleared) {
				warn_audio_device("audio-device-focus-clear", cleared.error());
				audio_device.reset();
			}
		}
		audio_focus_muted = desired;
	};
	while((*runner)->status() == th03::app::ClassicRunnerStatus::running) {
		if(online_refresh_task) {
			while(auto refreshed = online_refresh_task->take_completed()) {
				if(refreshed->error) {
					std::cerr << "status=warning\nstage=online-refresh\nrank="
						<< static_cast<unsigned>(refreshed->category.rank)
						<< "\ncharacter="
						<< static_cast<unsigned>(refreshed->category.character)
						<< "\ncode="
						<< static_cast<unsigned>(refreshed->error->code) << '\n';
					continue;
				}
				if(!refreshed->document) {
					std::cerr << "status=warning\nstage=online-refresh\n"
						"code=missing-result\n";
					continue;
				}
				overlay_online_downloads(*refreshed->document);
				if(auto merged = (*runner)->merge_new_classic_online_board(
					std::move(*refreshed->document)
				); !merged) {
					std::cerr << "status=warning\nstage=online-refresh-merge\n"
						"code=" << static_cast<unsigned>(merged.error().code)
						<< '\n';
				}
			}
			if(online_refresh_task->finished()) {
				if(online_refresh_task->worker_failed()) {
					std::cerr << "status=warning\nstage=online-refresh\n"
						"code=worker-failed\n";
				}
				online_refresh_task.reset();
			}
		}
		if(online_replay_download_task) {
			bool completion_reported = false;
			if(auto downloaded =
				online_replay_download_task->take_completed()) {
				const auto outcome = downloaded->path && !downloaded->error ?
					th03::app::NewClassicOnlineReplayDownloadOutcome::downloaded :
					th03::app::NewClassicOnlineReplayDownloadOutcome::failed;
				if(auto reported =
					(*runner)->report_new_classic_online_replay_download(
						downloaded->replay_id, outcome
					); !reported) {
					return {.exit_code = fail(
						"online-replay-download-report", reported.error()
					)};
				}
				if(downloaded->error) {
					std::cerr << "status=warning\n"
						"stage=online-replay-download\ncode="
						<< static_cast<unsigned>(downloaded->error->code)
						<< '\n';
				}
				completion_reported = true;
				online_replay_download_id.reset();
			}
			if(online_replay_download_task->finished()) {
				if(!completion_reported && online_replay_download_id) {
					if(auto reported =
						(*runner)->report_new_classic_online_replay_download(
							*online_replay_download_id,
							th03::app::NewClassicOnlineReplayDownloadOutcome::failed
						); !reported) {
						return {.exit_code = fail(
							"online-replay-download-report", reported.error()
						)};
					}
					std::cerr << "status=warning\n"
						"stage=online-replay-download\ncode=worker-failed\n";
					online_replay_download_id.reset();
				}
				online_replay_download_task.reset();
			}
		}
		// Sparse replay evidence advances one exact source boundary per host loop
		// without waiting for wall time. Replay samples still own all deterministic
		// input; no audio is submitted and discarded surfaces are never composed.
		const auto cutoff = pixel_oracle_selector ?
			scheduler->next_deadline_ns().value_or(
				th03::platform::sdl_ticks_now_ns()
			) : th03::platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto result = mapper.pump_events(*scheduler, false);
			if(!result) {
				return {.exit_code = fail("input-pump", result.error())};
			}
			retry_blocked_event = result->queue_blocked;
		}
		sync_audio_focus_mute();

		bool smoke_complete = false;
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return {.exit_code = fail_tick(due.error())};
			}
			if(!*due) {
				break;
			}
			(*due)->restart_requested =
				parsed.mode == th03::app::ClassicLaunchMode::new_classic &&
				mapper.keyboard_scancode_held(SDL_SCANCODE_R);
			(*due)->fast_forward_requested =
				parsed.mode == th03::app::ClassicLaunchMode::new_classic &&
				mapper.keyboard_scancode_held(SDL_SCANCODE_Z);
			if(new_classic) {
				const auto scancode = mapper.take_keyboard_scancode_press();
				(*due)->scancode_pressed = scancode ?
					std::optional<std::uint16_t>{static_cast<std::uint16_t>(
						*scancode
					)} : std::nullopt;
				(*due)->any_scancode_held =
					mapper.any_keyboard_scancode_held();
			}
			auto consumed = (*runner)->consume(**due, audio_session->status());
			if(!consumed) {
				std::cerr << "logical-tick=" << logical_ticks << '\n';
				return {
					.exit_code = fail("th03-runner", consumed.error())
				};
			}
			if(auto folder =
				(*runner)->take_new_classic_open_folder_request()) {
				std::error_code folder_error{};
				std::filesystem::create_directories(*folder, folder_error);
				if(folder_error) {
					std::cerr << "status=warning\n"
						"stage=open-folder-create\ncode="
						<< folder_error.value() << '\n';
				} else if(auto opened = platform.open_folder(*folder); !opened) {
					std::cerr << "status=warning\n"
						"stage=open-folder\ncode="
						<< static_cast<unsigned>(opened.error().code) << '\n';
				}
			}
			if(auto request =
				(*runner)->take_new_classic_online_replay_download_request()) {
				auto report_immediate = [&](auto outcome) {
					return (*runner)->report_new_classic_online_replay_download(
						request->replay_id, outcome
					);
				};
#if defined(_WIN32)
				if(!online_transport) {
					if(auto reported = report_immediate(
						th03::app::NewClassicOnlineReplayDownloadOutcome::unavailable
					); !reported) {
						return {.exit_code = fail(
							"online-replay-download-report", reported.error()
						)};
					}
				} else if(online_replay_download_task ||
					online_replay_download_id) {
					if(auto reported = report_immediate(
						th03::app::NewClassicOnlineReplayDownloadOutcome::failed
					); !reported) {
						return {.exit_code = fail(
							"online-replay-download-report", reported.error()
						)};
					}
				} else {
					auto started =
						th03::app::start_new_classic_online_replay_download(
							*request, parsed.replay_root,
							online_transport->transport()
						);
					if(!started) {
						if(auto reported = report_immediate(
							th03::app::NewClassicOnlineReplayDownloadOutcome::failed
						); !reported) {
							return {.exit_code = fail(
								"online-replay-download-report", reported.error()
							)};
						}
						std::cerr << "status=warning\n"
							"stage=online-replay-download-start\ncode="
							<< static_cast<unsigned>(started.error().code)
							<< '\n';
					} else {
						online_replay_download_id = request->replay_id;
						online_replay_download_task.emplace(std::move(*started));
					}
				}
#else
				if(auto reported = report_immediate(
					th03::app::NewClassicOnlineReplayDownloadOutcome::unavailable
				); !reported) {
					return {.exit_code = fail(
						"online-replay-download-report", reported.error()
					)};
				}
#endif
			}
			if(auto request =
				(*runner)->take_new_classic_online_replay_watch_request()) {
				auto loaded = th03::app::load_new_classic_online_replay(
					*request, parsed.replay_root
				);
				std::optional<th03::formats::ReplayDocument> document{};
				if(loaded) {
					document.emplace(std::move(*loaded));
				} else {
					std::cerr << "status=warning\n"
						"stage=online-replay-watch-load\ncode="
						<< static_cast<unsigned>(loaded.error().code) << '\n';
				}
				if(auto reported =
					(*runner)->report_new_classic_online_replay_watch(
						request->replay_id, std::move(document)
					); !reported) {
					return {.exit_code = fail(
						"online-replay-watch-report", reported.error()
					)};
				}
			}
			if(auto update = (*runner)->take_new_classic_key_config_update()) {
				active_key_config = *update;
				if(!active_joy_config || !active_device_assignment) {
					std::cerr << "status=error\nstage=key-config-apply\n"
						"code=input-config-missing\n";
					return {.exit_code = 1};
				}
				if(auto replaced = mapper.replace_bindings(
					th03::app::detail::new_classic_bindings(
						*active_key_config, *active_joy_config,
						*active_device_assignment
					)
				); !replaced) {
					return {.exit_code = fail("key-config-apply", replaced.error())};
				}
			}
			if(auto update = (*runner)->take_new_classic_joy_config_update()) {
				active_joy_config = *update;
				if(!active_key_config || !active_device_assignment) {
					std::cerr << "status=error\nstage=joy-config-apply\n"
						"code=input-config-missing\n";
					return {.exit_code = 1};
				}
				if(auto replaced = mapper.replace_bindings(
					th03::app::detail::new_classic_bindings(
						*active_key_config, *active_joy_config,
						*active_device_assignment
					)
				); !replaced) {
					return {.exit_code = fail("joy-config-apply", replaced.error())};
				}
			}
			if(auto update =
				(*runner)->take_new_classic_device_assignment_update()) {
				active_device_assignment = *update;
				if(!active_key_config || !active_joy_config) {
					std::cerr << "status=error\n"
						"stage=device-assignment-apply\n"
						"code=input-config-missing\n";
					return {.exit_code = 1};
				}
				if(auto replaced = mapper.replace_bindings(
					th03::app::detail::new_classic_bindings(
						*active_key_config, *active_joy_config,
						*active_device_assignment
					)
				); !replaced) {
					return {.exit_code = fail(
						"device-assignment-apply", replaced.error()
					)};
				}
			}
			if(consumed->clock_rebase_requested) {
				const auto resumed_at = th03::platform::sdl_ticks_now_ns();
				if(auto rebased = scheduler->rebase_after_modal(resumed_at);
					!rebased) {
					return {.exit_code = fail_tick(rebased.error())};
				}
				if(auto reset = mapper.reset_after_modal(*scheduler, resumed_at);
					!reset) {
					return {.exit_code = fail("input-rebase", reset.error())};
				}
				retry_blocked_event = false;
			}
			if(auto update =
				(*runner)->take_new_classic_master_volume_update()) {
				master_volume_percent = *update;
			}
			if(auto update =
				(*runner)->take_new_classic_mute_when_unfocused_update()) {
				mute_when_unfocused = *update;
				sync_audio_focus_mute();
			}
			if(auto update =
				(*runner)->take_new_classic_window_presentation_update()) {
				if(auto applied = platform.apply_window_presentation(
					platform_window_presentation(*update)
				); !applied) {
					return {.exit_code = fail(
						"window-presentation", applied.error()
					)};
				}
				window_presentation = *update;
			}
			const bool clock_rebased_this_tick =
				consumed->clock_rebase_requested;
			const auto next_source_deadline = scheduler->next_deadline_ns();
			(*runner)->classify_pending_replay_frame(
				next_source_deadline &&
				th03::platform::sdl_ticks_now_ns() >= *next_source_deadline
			);
			if(auto drained = drain_audio(); !drained) {
				std::cerr << "logical-tick=" << logical_ticks << '\n';
				return {.exit_code = fail("audio-command", drained.error())};
			}
			sync_audio_focus_mute();
			auto audio_tick = audio_session->advance_tick();
			if(!audio_tick) {
				std::cerr << "logical-tick=" << logical_ticks << '\n';
				return {.exit_code = fail("audio-tick", audio_tick.error())};
			}
			if(audio_device && !audio_focus_muted) {
				if(!audio_started && !audio_tick->canonical_frames.empty() &&
					audio_tick->canonical_frames.size() <=
						audio_device->maximum_queued_frames() / 3U) {
					// Two silent source intervals plus the first rendered interval
					// keep a 50 Hz blocking present from starving the 56.4 Hz
					// source clock before its first two-tick catch-up.
					startup_silence.assign(
						audio_tick->canonical_frames.size() * 2U, {}
					);
					const auto prefilled = audio_device->submit(startup_silence);
					if(!prefilled) {
						warn_audio_device(
							"audio-device-prefill", prefilled.error()
						);
						audio_device.reset();
					}
				}
			}
			if(audio_device && !audio_focus_muted) {
				auto output = output_gain_frames(
					audio_tick->canonical_frames,
					new_classic ? master_volume_percent : std::uint8_t{100U},
					gain_frames
				);
				if(!output) {
					return {.exit_code = fail_output_gain(
						"audio-output-gain", output.error()
					)};
				}
				const auto submitted = audio_device->submit(*output);
				if(!submitted) {
					// Device loss is final-output state. Keep PMD and scene
					// timelines advancing, but stop attempting delivery.
					warn_audio_device(
						"audio-device-submit", submitted.error()
					);
					audio_device.reset();
				} else if(!audio_started && !audio_tick->canonical_frames.empty()) {
					const auto resumed = audio_device->resume();
					if(!resumed) {
						warn_audio_device("audio-device-resume", resumed.error());
						audio_device.reset();
					} else {
						audio_started = true;
					}
				}
			}
			if(consumed->surface_changed) {
				if(pixel_oracle_selector &&
					!pixel_oracle_selector->matches(
						consumed->pixel_oracle_identity
					)) {
					if(!th03::app::write_t3ali1_event(
						identity_output.stream, identity_output.fixture_sha256,
						consumed->pixel_oracle_identity, std::nullopt
					)) {
						std::cerr << "status=error\n"
							"stage=pixel-oracle-identity\n"
							"code=identity-oracle-write-failed\n";
						return {.exit_code = 1};
					}
				} else {
					presentation_pending = true;
					if(identity_output.enabled() && pending_publication_identity) {
						std::cerr << "status=error\n"
							"stage=pixel-oracle-identity\n"
							"code=identity-oracle-coalesced-publication\n";
						return {.exit_code = 1};
					}
					pending_publication_tick = logical_ticks;
					++pending_publication_ticks;
					pending_publication_identity =
						consumed->pixel_oracle_identity;
				}
			} else if(identity_output.enabled() &&
				!th03::app::write_t3ali1_event(
					identity_output.stream, identity_output.fixture_sha256,
					consumed->pixel_oracle_identity, std::nullopt
				)) {
				std::cerr << "status=error\nstage=pixel-oracle-identity\n"
					"code=identity-oracle-write-failed\n";
				return {.exit_code = 1};
			}
			if(auto replay = (*runner)->take_completed_replay()) {
				completed_replay = std::move(*replay);
			}
			logical_ticks++;
			if(retry_blocked_event) {
				auto retried = mapper.pump_events(*scheduler, false);
				if(!retried) {
					return {.exit_code = fail("input-pump", retried.error())};
				}
				retry_blocked_event = retried->queue_blocked;
			}
			if(parsed.smoke_ticks && logical_ticks >= *parsed.smoke_ticks) {
				smoke_complete = true;
				break;
			}
			if(completed_replay || consumed->status !=
				th03::app::ClassicRunnerStatus::running) {
				break;
			}
			if(clock_rebased_this_tick) {
				// cutoff was sampled before synchronous replay reconstruction
				// established the new scheduler baseline. Publish the selected
				// checkpoint now, then begin a fresh host iteration before asking
				// the rebased scheduler for another logical tick.
				break;
			}
			if(platform.oracle_enabled() && presentation_pending) {
				// Oracle captures are logical-tick aligned: never let wall-clock
				// catch-up coalesce two source publications into one row.
				break;
			}
		}

		const bool selected_publication_pending =
			pixel_oracle_selector && presentation_pending;
		if(pixel_oracle_selector && !selected_publication_pending &&
			(completed_replay || (*runner)->status() !=
				th03::app::ClassicRunnerStatus::running)) {
			std::cerr << "status=error\nstage=pixel-oracle-selector\n"
				"code=selector-not-reached\n";
			return {.exit_code = 1};
		}
		if(!selected_publication_pending && (*runner)->status() ==
			th03::app::ClassicRunnerStatus::quit_requested) {
			return {.disposition = ExperienceRunDisposition::application_quit};
		}
		if(!selected_publication_pending && (*runner)->status() ==
			th03::app::ClassicRunnerStatus::switch_to_classic) {
			return {.disposition = ExperienceRunDisposition::switch_to_classic};
		}
		const bool session_complete = (*runner)->status() ==
			th03::app::ClassicRunnerStatus::session_complete;
		// Replay Patch finalizes and offers the recording before constructing or
		// showing Continue. The typed owner may already have prepared that surface,
		// so retain its pending publication across the host-owned save transaction.
		if(!selected_publication_pending && completed_replay && new_classic) {
			auto saved = run_replay_save(
				std::move(*completed_replay), parsed.replay_root,
				parsed.tick_rate, platform, mapper, patch_assets,
				(*runner)->new_classic_replay_save_art(),
				{
					.session = audio_session.get(),
					.device = &audio_device,
					.started = &audio_started,
					.master_volume_percent = &master_volume_percent,
					.mute_when_unfocused = &mute_when_unfocused,
				}
			);
			completed_replay.reset();
			if(saved.exit_code != 0) {
				return {.exit_code = saved.exit_code};
			}
			if(saved.application_quit) {
				return {
					.disposition = ExperienceRunDisposition::application_quit
				};
			}
			const auto resumed_at = th03::platform::sdl_ticks_now_ns();
			if(auto rebased = scheduler->rebase_after_modal(
				resumed_at
			); !rebased) {
				return {.exit_code = fail_tick(rebased.error())};
			}
			if(auto reset = mapper.reset_after_modal(*scheduler, resumed_at);
				!reset) {
				return {.exit_code = fail("input-rebase", reset.error())};
			}
			retry_blocked_event = false;
			if(session_complete) {
				return {.disposition = ExperienceRunDisposition::return_to_menu};
			}
			continue;
		}
		bool presented_this_iteration = false;
		if(presentation_pending &&
			(!retry_blocked_event || session_complete || smoke_complete ||
			 completed_replay)) {
			surface = (*runner)->compose_shown_surface(pixel_span);
			if(!surface) {
				return {.exit_code = fail("compose", surface.error())};
			}
			auto presented = platform.present(*surface, {
				.logical_tick = pending_publication_tick,
				.coalesced_ticks = pending_publication_ticks,
			});
			if(!presented) {
				return {.exit_code = fail("present", presented.error())};
			}
			if(identity_output.enabled() &&
				(!pending_publication_identity ||
				 !presented->oracle_publication ||
				 !th03::app::write_t3ali1_event(
					identity_output.stream, identity_output.fixture_sha256,
					*pending_publication_identity,
					presented->oracle_publication
				 ))) {
				std::cerr << "status=error\nstage=pixel-oracle-identity\n"
					"code=identity-oracle-write-failed\n";
				return {.exit_code = 1};
			}
			presentation_pending = false;
			pending_publication_tick.reset();
			pending_publication_ticks = 0U;
			pending_publication_identity.reset();
			presented_this_iteration = true;
			if(pixel_oracle_selector) {
				return {.disposition = ExperienceRunDisposition::smoke_complete};
			}
		}
		if(!presentation_pending) {
			if(auto refreshed = refresh_host_presentability(
				platform, "presentability-refresh"
			); !refreshed) {
				return {.exit_code = refreshed.error()};
			}
		}
		if(session_complete) {
			return {.disposition = ExperienceRunDisposition::return_to_menu};
		}
		if(completed_replay) {
			return {
				.disposition = ExperienceRunDisposition::replay_completed,
				.replay = std::move(completed_replay),
			};
		}
		if(smoke_complete) {
			if(pixel_oracle_selector) {
				std::cerr << "status=error\nstage=pixel-oracle-selector\n"
					"code=selector-not-reached\n";
				return {.exit_code = 1};
			}
			return {.disposition = ExperienceRunDisposition::smoke_complete};
		}
		if(presented_this_iteration) {
			continue;
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = th03::platform::sdl_ticks_now_ns();
		if(!pixel_oracle_selector && deadline && *deadline > now) {
			SDL_DelayPrecise(std::min(*deadline - now, maximum_idle_delay_ns));
		}
	}
	return {.disposition = ExperienceRunDisposition::return_to_menu};
}

StartupRunResult run_startup_sequence(
	th03::app::RationalTickRate tick_rate,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper,
	th03::app::StartupSequence sequence
)
{
	auto presented = platform.present(sequence.surface());
	if(!presented) {
		return {.exit_code = fail("startup-present", presented.error())};
	}
	auto scheduler = th03::app::TickInputScheduler::create(
		tick_rate,
		th03::platform::sdl_ticks_now_ns(),
		th03::app::TickInputCatchUpPolicy::discard_missed_deadlines
	);
	if(!scheduler) {
		return {.exit_code = fail_tick(scheduler.error())};
	}

	bool retry_blocked_event = false;
	while(sequence.status() == th03::app::StartupSequenceStatus::running) {
		const auto cutoff = th03::platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto result = mapper.pump_events(*scheduler, false);
			if(!result) {
				return {.exit_code = fail("startup-input-pump", result.error())};
			}
			retry_blocked_event = result->queue_blocked;
		}

		bool presentation_pending = false;
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return {.exit_code = fail_tick(due.error())};
			}
			if(!*due) {
				break;
			}
			const auto &sample = **due;
			const auto consumed = sequence.consume({
				.any_input = sample.input.system != 0U ||
					sample.input.p1 != 0U || sample.input.p2 != 0U ||
					mapper.any_keyboard_scancode_held(),
				.app_quit_requested = sample.app_quit_requested,
			});
			presentation_pending =
				presentation_pending || consumed.surface_changed;
			if(retry_blocked_event) {
				auto retried = mapper.pump_events(*scheduler, false);
				if(!retried) {
					return {
						.exit_code = fail("startup-input-pump", retried.error())
					};
				}
				retry_blocked_event = retried->queue_blocked;
			}
			if(consumed.status != th03::app::StartupSequenceStatus::running) {
				break;
			}
		}

		if(presentation_pending) {
			presented = platform.present(sequence.surface());
			if(!presented) {
				return {.exit_code = fail("startup-present", presented.error())};
			}
		}
		if(auto refreshed = refresh_host_presentability(
			platform, "startup-presentability-refresh"
		); !refreshed) {
			return {.exit_code = refreshed.error()};
		}
		if(sequence.status() ==
			th03::app::StartupSequenceStatus::quit_requested) {
			return {.application_quit = true};
		}
		if(sequence.status() == th03::app::StartupSequenceStatus::failed) {
			return {.exit_code = fail(
				"startup-render",
				th03::app::StartupSequenceError{
					.code = th03::app::StartupSequenceErrorCode::draw_failed,
				}
			)};
		}
		if(sequence.status() == th03::app::StartupSequenceStatus::complete) {
			return {};
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = th03::platform::sdl_ticks_now_ns();
		if(deadline && *deadline > now) {
			SDL_DelayPrecise(std::min(*deadline - now, maximum_idle_delay_ns));
		}
	}
	return {};
}

[[nodiscard]] th03::app::NewClassicUiInput project_new_classic_ui(
	const th03::app::LogicalTickInput &sample
) noexcept
{
	constexpr std::uint16_t input_up = 0x0001U;
	constexpr std::uint16_t input_down = 0x0002U;
	constexpr std::uint16_t input_left = 0x0004U;
	constexpr std::uint16_t input_right = 0x0008U;
	constexpr std::uint16_t input_bomb = 0x0010U;
	constexpr std::uint16_t input_shot = 0x0020U;
	constexpr std::uint16_t input_cancel = 0x1000U;
	constexpr std::uint16_t input_ok = 0x2000U;
	const auto word = sample.input.system;
	const auto pressed = [word](std::uint16_t bit) noexcept {
		return (word & bit) != 0U;
	};
	return {
		.up = pressed(input_up),
		.down = pressed(input_down),
		.left = pressed(input_left),
		.right = pressed(input_right),
		.confirm = pressed(input_shot) || pressed(input_ok),
		.bomb = pressed(input_bomb),
		.cancel = pressed(input_cancel),
		.app_quit_requested = sample.app_quit_requested,
	};
}

template <typename Surface, typename Consume>
std::expected<void, int> run_host_ui(
	th03::app::RationalTickRate tick_rate,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper,
	Surface surface,
	Consume consume,
	ReplaySaveAudioContext audio = {}
)
{
	auto presented = platform.present(surface());
	if(!presented) {
		return std::unexpected(fail("new-classic-present", presented.error()));
	}
	auto scheduler = th03::app::TickInputScheduler::create(
		tick_rate,
		th03::platform::sdl_ticks_now_ns(),
		// Replay Patch menus and a retained SCORE.M still live on the same
		// 56.423 Hz source timeline while the host owns this modal transaction.
		th03::app::classic_interactive_catch_up_policy
	);
	if(!scheduler) {
		return std::unexpected(fail_tick(scheduler.error()));
	}
	bool retry_blocked_event = false;
	bool audio_focus_muted = false;
	std::vector<th03::audio::PmdCanonicalStereoSample> gain_frames{};
	auto sync_audio_focus_mute = [&]() noexcept {
		const bool desired = audio.mute_when_unfocused &&
			*audio.mute_when_unfocused && !mapper.main_window_focused();
		if(desired && !audio_focus_muted && audio.device && *audio.device) {
			if(auto cleared = (*audio.device)->clear(); !cleared) {
				warn_audio_device(
					"replay-save-audio-focus-clear", cleared.error()
				);
				audio.device->reset();
			}
		}
		audio_focus_muted = desired;
	};
	for(;;) {
		const auto cutoff = th03::platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto pumped = mapper.pump_events(*scheduler, false);
			if(!pumped) {
				return std::unexpected(fail(
					"new-classic-input-pump", pumped.error()
				));
			}
			retry_blocked_event = pumped->queue_blocked;
		}
		sync_audio_focus_mute();
		bool presentation_pending = false;
		bool done = false;
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return std::unexpected(fail_tick(due.error()));
			}
			if(!*due) {
				break;
			}
			auto step = consume(project_new_classic_ui(**due));
			if(!step) {
				return std::unexpected(step.error());
			}
			presentation_pending =
				presentation_pending || step->surface_changed;
			if(audio.session) {
				sync_audio_focus_mute();
				auto rendered = audio.session->advance_tick();
				if(!rendered) {
					return std::unexpected(fail(
						"replay-save-audio-tick", rendered.error()
					));
				}
				if(audio.device && *audio.device && !audio_focus_muted) {
					const std::uint8_t percent = audio.master_volume_percent ?
						*audio.master_volume_percent : std::uint8_t{100U};
					auto output = output_gain_frames(
						rendered->canonical_frames, percent, gain_frames
					);
					if(!output) {
						return std::unexpected(fail_output_gain(
							"replay-save-audio-output-gain", output.error()
						));
					}
					auto submitted = (*audio.device)->submit(*output);
					if(!submitted) {
						warn_audio_device(
							"replay-save-audio-submit", submitted.error()
						);
						audio.device->reset();
					} else if(audio.started && !*audio.started &&
						!rendered->canonical_frames.empty()) {
						auto resumed = (*audio.device)->resume();
						if(!resumed) {
							warn_audio_device(
								"replay-save-audio-resume", resumed.error()
							);
							audio.device->reset();
						} else {
							*audio.started = true;
						}
					}
				}
			}
			done = step->done;
			if(retry_blocked_event) {
				auto retried = mapper.pump_events(*scheduler, false);
				if(!retried) {
					return std::unexpected(fail(
						"new-classic-input-pump", retried.error()
					));
				}
				retry_blocked_event = retried->queue_blocked;
			}
			if(done) {
				break;
			}
		}
		if(presentation_pending) {
			presented = platform.present(surface());
			if(!presented) {
				return std::unexpected(fail(
					"new-classic-present", presented.error()
				));
			}
		}
		if(auto refreshed = refresh_host_presentability(
			platform, "new-classic-presentability-refresh"
		); !refreshed) {
			return std::unexpected(refreshed.error());
		}
		if(done) {
			return {};
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = th03::platform::sdl_ticks_now_ns();
		if(deadline && *deadline > now) {
			SDL_DelayPrecise(std::min(*deadline - now, maximum_idle_delay_ns));
		}
	}
}

[[nodiscard]] std::string_view sdl_scancode_name(
	void *,
	std::uint16_t key
) noexcept
{
	if(key == 0U || key >= static_cast<std::uint16_t>(SDL_SCANCODE_COUNT)) {
		return {};
	}
	// Keep the Replay Patch's visible PC-98 vocabulary for native keys with a
	// direct host equivalent. SDL's platform-local names are only a fallback
	// for host-only extensions that could not exist in TH3KEY.CFG.
	switch(static_cast<SDL_Scancode>(key)) {
	case SDL_SCANCODE_1: return "1";
	case SDL_SCANCODE_2: return "2";
	case SDL_SCANCODE_3: return "3";
	case SDL_SCANCODE_4: return "4";
	case SDL_SCANCODE_5: return "5";
	case SDL_SCANCODE_6: return "6";
	case SDL_SCANCODE_7: return "7";
	case SDL_SCANCODE_8: return "8";
	case SDL_SCANCODE_9: return "9";
	case SDL_SCANCODE_0: return "0";
	case SDL_SCANCODE_A: return "A";
	case SDL_SCANCODE_B: return "B";
	case SDL_SCANCODE_C: return "C";
	case SDL_SCANCODE_D: return "D";
	case SDL_SCANCODE_E: return "E";
	case SDL_SCANCODE_F: return "F";
	case SDL_SCANCODE_G: return "G";
	case SDL_SCANCODE_H: return "H";
	case SDL_SCANCODE_I: return "I";
	case SDL_SCANCODE_J: return "J";
	case SDL_SCANCODE_K: return "K";
	case SDL_SCANCODE_L: return "L";
	case SDL_SCANCODE_M: return "M";
	case SDL_SCANCODE_N: return "N";
	case SDL_SCANCODE_O: return "O";
	case SDL_SCANCODE_P: return "P";
	case SDL_SCANCODE_Q: return "Q";
	case SDL_SCANCODE_R: return "R";
	case SDL_SCANCODE_S: return "S";
	case SDL_SCANCODE_T: return "T";
	case SDL_SCANCODE_U: return "U";
	case SDL_SCANCODE_V: return "V";
	case SDL_SCANCODE_W: return "W";
	case SDL_SCANCODE_X: return "X";
	case SDL_SCANCODE_Y: return "Y";
	case SDL_SCANCODE_Z: return "Z";
	case SDL_SCANCODE_MINUS: return "Minus";
	case SDL_SCANCODE_EQUALS: return "Plus";
	case SDL_SCANCODE_BACKSPACE: return "Backspace";
	case SDL_SCANCODE_TAB: return "Tab";
	case SDL_SCANCODE_LEFTBRACKET: return "LBracket";
	case SDL_SCANCODE_RIGHTBRACKET: return "RBracket";
	case SDL_SCANCODE_RETURN: return "Return";
	case SDL_SCANCODE_COMMA: return "Comma";
	case SDL_SCANCODE_PERIOD: return "Period";
	case SDL_SCANCODE_SLASH: return "Slash";
	case SDL_SCANCODE_SPACE: return "Space";
	case SDL_SCANCODE_PAGEUP: return "Roll Up";
	case SDL_SCANCODE_PAGEDOWN: return "Roll Down";
	case SDL_SCANCODE_INSERT: return "Insert";
	case SDL_SCANCODE_DELETE: return "Delete";
	case SDL_SCANCODE_UP: return "Up Arrow";
	case SDL_SCANCODE_LEFT: return "Left Arrow";
	case SDL_SCANCODE_RIGHT: return "Right Arrow";
	case SDL_SCANCODE_DOWN: return "Down Arrow";
	case SDL_SCANCODE_HOME: return "Home/Clear";
	case SDL_SCANCODE_END: return "End";
	case SDL_SCANCODE_KP_0: return "Num 0";
	case SDL_SCANCODE_KP_1: return "Num 1";
	case SDL_SCANCODE_KP_2: return "Num 2";
	case SDL_SCANCODE_KP_3: return "Num 3";
	case SDL_SCANCODE_KP_4: return "Num 4";
	case SDL_SCANCODE_KP_5: return "Num 5";
	case SDL_SCANCODE_KP_6: return "Num 6";
	case SDL_SCANCODE_KP_7: return "Num 7";
	case SDL_SCANCODE_KP_8: return "Num 8";
	case SDL_SCANCODE_KP_9: return "Num 9";
	case SDL_SCANCODE_KP_MINUS: return "Num Minus";
	case SDL_SCANCODE_KP_DIVIDE: return "Num Divide";
	case SDL_SCANCODE_KP_MULTIPLY: return "Num Multiply";
	case SDL_SCANCODE_KP_PLUS: return "Num Plus";
	case SDL_SCANCODE_KP_EQUALS: return "Num Equals";
	case SDL_SCANCODE_KP_COMMA: return "Num Comma";
	case SDL_SCANCODE_KP_PERIOD: return "Num Period";
	case SDL_SCANCODE_F1: return "F1";
	case SDL_SCANCODE_F2: return "F2";
	case SDL_SCANCODE_F3: return "F3";
	case SDL_SCANCODE_F4: return "F4";
	case SDL_SCANCODE_F5: return "F5";
	case SDL_SCANCODE_F6: return "F6";
	case SDL_SCANCODE_F7: return "F7";
	case SDL_SCANCODE_F8: return "F8";
	case SDL_SCANCODE_F9: return "F9";
	case SDL_SCANCODE_F10: return "F10";
	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT: return "Shift";
	case SDL_SCANCODE_LALT:
	case SDL_SCANCODE_RALT: return "GRPH";
	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL: return "Ctrl";
	default: break;
	}
	const auto *name = SDL_GetScancodeName(static_cast<SDL_Scancode>(key));
	return name != nullptr ? std::string_view{name} : std::string_view{};
}

[[nodiscard]] bool sdl_controller_catalog(
	void *context,
	th03::app::NewClassicControllerCatalog &catalog
) noexcept
{
	if(context == nullptr) {
		return false;
	}
	catalog = static_cast<th03::platform::SdlInputMapper *>(context)->
		controller_catalog();
	return true;
}

ReplaySaveRunResult run_replay_save(
	th03::formats::ReplayDocument document,
	const std::filesystem::path &replay_root,
	th03::app::RationalTickRate tick_rate,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper,
	std::shared_ptr<const th03::app::NewClassicReplayPatchAssets> patch_assets,
	std::shared_ptr<const th03::app::NewClassicReplaySaveArt> save_art,
	ReplaySaveAudioContext audio
)
{
	auto catalog = th03::app::scan_new_classic_replays(replay_root);
	if(!catalog) {
		return {.exit_code = fail("replay-catalog", catalog.error())};
	}
	th03::app::NewClassicReplaySaveMenu menu(
		std::move(document), std::move(patch_assets), std::move(save_art),
		&current_dos_date
	);
	bool application_quit = false;
	auto driven = run_host_ui(
		tick_rate, platform, mapper,
		[&]() { return menu.surface(); },
		[&](th03::app::NewClassicUiInput input) ->
			std::expected<HostUiStep, int> {
			auto result = menu.consume(input, *catalog);
			if(result.status ==
				th03::app::NewClassicReplaySaveStatus::save_requested) {
				auto committed = th03::app::commit_replay_slot(
					replay_root, menu.selected_slot(), menu.document()
				);
				if(committed) {
					auto refreshed = th03::app::scan_new_classic_replays(replay_root);
					if(!refreshed) {
						return std::unexpected(fail(
							"replay-catalog-refresh", refreshed.error()
						));
					}
					catalog = std::move(refreshed);
				}
				menu.report_commit(committed.has_value(), *catalog);
				result.surface_changed = true;
				result.status = th03::app::NewClassicReplaySaveStatus::running;
			}
			application_quit = result.status ==
				th03::app::NewClassicReplaySaveStatus::application_quit;
			return HostUiStep{
				.surface_changed = result.surface_changed,
				.done = result.status !=
					th03::app::NewClassicReplaySaveStatus::running,
			};
		},
		audio
	);
	if(!driven) {
		return {.exit_code = driven.error()};
	}
	return {.application_quit = application_quit};
}

std::expected<
	std::pair<
		std::unique_ptr<th03::platform::SdlPlatform>,
		th03::platform::SdlInputMapper
	>,
	int
> create_product_platform(
	bool force_hidden = false,
	std::optional<th03::app::NewClassicWindowPresentation>
		initial_presentation = std::nullopt
)
{
	auto config = platform_config(force_hidden);
	if(initial_presentation) {
		config.initial_window_presentation = platform_window_presentation(
			*initial_presentation
		);
	}
	auto platform = th03::platform::SdlPlatform::create(
		config
	);
	if(!platform) {
		return std::unexpected(fail("platform-create", platform.error()));
	}
	auto mapper = th03::platform::SdlInputMapper::create(
		th03::app::detail::classic_direct_vs_bindings(),
		static_cast<SDL_WindowID>((*platform)->window_id()),
		th03::platform::native_sdl_gamepad_backend()
	);
	if(!mapper) {
		return std::unexpected(fail("input-create", mapper.error()));
	}
	return std::pair{std::move(*platform), std::move(*mapper)};
}

ExperienceRunResult run_new_classic_experience(
	th03::app::ClassicCliRequest parsed,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper
)
{
	constexpr auto maximum_key = static_cast<std::uint16_t>(
		SDL_SCANCODE_COUNT
	);
	const auto key_defaults = th03::app::detail::new_classic_default_key_config();
	auto loaded_settings = th03::app::load_new_classic_settings_store(
		parsed.launch.content.new_classic_settings,
		parsed.launch.content.config
	);
	if(!loaded_settings) {
		return {.exit_code = fail("settings-load", loaded_settings.error())};
	}
	auto settings = loaded_settings->settings;
	auto config_options = settings.options;
	auto patch_asset_path = parsed.launch.content.replay_patch_assets;
	if(patch_asset_path.empty()) {
		patch_asset_path = parsed.launch.content.data_root / "AZINN.DAT";
	}
	auto loaded_patch_assets =
		th03::app::load_new_classic_replay_patch_assets(patch_asset_path);
	if(!loaded_patch_assets) {
		return {.exit_code = fail("replay-patch-assets", loaded_patch_assets.error())};
	}
	auto patch_assets = std::make_shared<
		const th03::app::NewClassicReplayPatchAssets
	>(std::move(*loaded_patch_assets));
	bool english_available = false;
	if(!parsed.launch.content.language_overlay.empty()) {
		auto overlay = th03::app::load_new_classic_language_overlay(
			parsed.launch.content.language_overlay
		);
		if(!overlay) {
			return {.exit_code = fail("language-overlay", overlay.error())};
		}
		english_available = true;
	}
	if(!english_available &&
		config_options.language == th03::app::ClassicLanguage::english) {
		config_options.language = th03::app::ClassicLanguage::japanese;
		settings.options = config_options;
		if(auto stored = th03::app::commit_new_classic_settings_store(
			parsed.launch.content.new_classic_settings, settings
		); !stored) {
			return {.exit_code = fail("settings-language-fallback", stored.error())};
		}
	}
	const auto key_path = parsed.launch.content.config.parent_path() /
		"TH3KEY.CFG";
	const auto joy_defaults = th03::app::detail::new_classic_default_joy_config();
	const auto joy_path = parsed.launch.content.config.parent_path() /
		"TH3JOY.CFG";
	const th03::app::NewClassicDeviceAssignment device_defaults{};
	const auto device_path = parsed.launch.content.config.parent_path() /
		"TH3DEV.CFG";
	auto loaded_keys = th03::app::load_new_classic_key_config(
		key_path, key_defaults, maximum_key, config_options.legacy_autofire
	);
	if(!loaded_keys) {
		return {.exit_code = fail("key-config-load", loaded_keys.error())};
	}
	auto key_config = loaded_keys->config;
	if(loaded_keys->recovered_defaults) {
		std::cerr << "status=warning\nstage=key-config-load\n"
			"code=recovered-defaults\n";
	}
	auto loaded_joy = th03::app::load_new_classic_joy_config(
		joy_path, joy_defaults
	);
	if(!loaded_joy) {
		return {.exit_code = fail("joy-config-load", loaded_joy.error())};
	}
	auto joy_config = loaded_joy->config;
	if(loaded_joy->recovered_defaults) {
		std::cerr << "status=warning\nstage=joy-config-load\n"
			"code=recovered-defaults\n";
	}
	auto loaded_device = th03::app::load_new_classic_device_assignment(
		device_path, device_defaults
	);
	if(!loaded_device) {
		return {
			.exit_code = fail("device-assignment-load", loaded_device.error())
		};
	}
	auto device_assignment = loaded_device->assignment;
	if(loaded_device->recovered_defaults) {
		std::cerr << "status=warning\nstage=device-assignment-load\n"
			"code=recovered-defaults\n";
	}
	if(auto replaced = mapper.replace_bindings(
		th03::app::detail::new_classic_bindings(
			key_config, joy_config, device_assignment
		)
	); !replaced) {
		return {.exit_code = fail("key-config-apply", replaced.error())};
	}
	parsed.mode = th03::app::ClassicLaunchMode::new_classic;
	parsed.launch.autofire_mask = key_config.autofire;
	parsed.launch.reduced_flash = config_options.reduced_flash;
	parsed.launch.new_classic_key_config = key_config;
	parsed.launch.new_classic_key_defaults = key_defaults;
	parsed.launch.new_classic_joy_config = joy_config;
	parsed.launch.new_classic_joy_defaults = joy_defaults;
	parsed.launch.new_classic_device_assignment = device_assignment;
	parsed.launch.new_classic_device_defaults = device_defaults;
	parsed.launch.new_classic_controller_catalog = &sdl_controller_catalog;
	parsed.launch.new_classic_controller_context = &mapper;
	parsed.launch.new_classic_maximum_key = maximum_key;
	parsed.launch.new_classic_capture_cancel_key = static_cast<std::uint16_t>(
		SDL_SCANCODE_ESCAPE
	);
	parsed.launch.new_classic_key_name = &sdl_scancode_name;
	parsed.launch.initial_route = th03::app::ClassicRunnerInitialRoute::title;
	// Replay, Practice, KeyConfig, Options, and replay playback are children of
	// the patched OP session. Its title is the normal product root; no Replay
	// Patch screen is substituted by a host route.
	return run_classic_session(parsed, platform, mapper, patch_assets);
}

int run_classic(th03::app::ClassicCliRequest parsed)
{
	std::optional<th03::app::NewClassicWindowPresentation>
		initial_presentation{};
	if(parsed.mode == th03::app::ClassicLaunchMode::new_classic) {
		auto settings = th03::app::load_new_classic_settings_store(
			parsed.launch.content.new_classic_settings,
			parsed.launch.content.config
		);
		if(!settings) {
			return fail("settings-load", settings.error());
		}
		initial_presentation = settings->settings.window;
	}
	auto product = create_product_platform(
		parsed.capture_replay.has_value(), initial_presentation
	);
	if(!product) {
		return product.error();
	}
	auto result = parsed.mode == th03::app::ClassicLaunchMode::new_classic ?
		run_new_classic_experience(
			parsed, *product->first, product->second
		) : run_classic_session(
			parsed, *product->first, product->second
		);
	if(result.exit_code == 0 && result.disposition ==
		ExperienceRunDisposition::switch_to_classic) {
		parsed.mode = th03::app::ClassicLaunchMode::classic;
		result = run_classic_session(
			std::move(parsed), *product->first, product->second
		);
	}
	return result.exit_code;
}

int run_product_shell(th03::app::ClassicCliRequest parsed)
{
	auto settings = th03::app::load_new_classic_settings_store(
		parsed.launch.content.new_classic_settings,
		parsed.launch.content.config
	);
	if(!settings) {
		return fail("settings-load", settings.error());
	}
	auto product = create_product_platform(
		false, settings->settings.window
	);
	if(!product) {
		return product.error();
	}
	// Automated packaging smoke exercises the normal direct New Classic root
	// without waiting for a physical title selection.
	if(parsed.smoke_ticks) {
		parsed.mode = th03::app::ClassicLaunchMode::new_classic;
		return run_new_classic_experience(
			std::move(parsed), *product->first, product->second
		).exit_code;
	}
	auto startup = th03::app::StartupSequence::load(
		parsed.launch.content.data_root / "TOUHOU.DAT"
	);
	if(!startup) {
		return fail("startup-logo-load", startup.error());
	}
	const auto startup_result = run_startup_sequence(
		parsed.tick_rate,
		*product->first,
		product->second,
		std::move(*startup)
	);
	if(startup_result.exit_code != 0 || startup_result.application_quit) {
		return startup_result.exit_code;
	}

	for(;;) {
		parsed.mode = th03::app::ClassicLaunchMode::new_classic;
		auto experience = run_new_classic_experience(
			parsed, *product->first, product->second
		);
		if(experience.exit_code != 0) {
			return experience.exit_code;
		}
		const auto new_classic_transition =
			th03::app::product_lifecycle_transition(
				th03::app::ExperienceProfile::new_classic,
				experience.disposition ==
					ExperienceRunDisposition::switch_to_classic ?
					th03::app::ProductLifecycleSignal::switch_to_classic :
					(experience.disposition ==
						ExperienceRunDisposition::application_quit ?
						th03::app::ProductLifecycleSignal::application_quit :
						th03::app::ProductLifecycleSignal::return_to_menu)
			);
		if(new_classic_transition.exit_application) {
			return 0;
		}

		if(auto restored = product->second.replace_bindings(
			th03::app::detail::classic_direct_vs_bindings()
		); !restored) {
			return fail("classic-input-policy", restored.error());
		}
		parsed.mode = th03::app::ClassicLaunchMode::classic;
		experience = run_classic_session(
			parsed, *product->first, product->second
		);
		if(experience.exit_code != 0 || experience.disposition !=
			ExperienceRunDisposition::return_to_menu) {
			return experience.exit_code;
		}
		const auto classic_transition =
			th03::app::product_lifecycle_transition(
				th03::app::ExperienceProfile::classic,
				th03::app::ProductLifecycleSignal::return_to_menu
			);
		if(classic_transition.exit_application || classic_transition.next !=
			th03::app::ExperienceProfile::new_classic) {
			std::cerr << "status=error\nstage=product-lifecycle\n"
				"code=invalid-classic-return\n";
			return 1;
		}
		// Classic's exact Quit has completed. Begin a fresh New Classic OP
		// owner on the next iteration without replaying startup.
	}
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
		arguments.end() ||
		std::ranges::find(arguments, "--new-classic") != arguments.end() ||
		std::ranges::find(arguments, "--shell") != arguments.end();
	if(classic) {
		auto parsed = th03::app::parse_classic_cli(arguments);
		if(!parsed) {
			return fail("arguments", parsed.error());
		}
		if(parsed->show_help) {
			print_usage();
			return 0;
		}
		if(parsed->mode == th03::app::ClassicLaunchMode::shell) {
			return run_product_shell(std::move(*parsed));
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
