#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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
#include "th03/app/main_compiled_content.hpp"
#include "th03/app/new_classic_frontend.hpp"
#include "th03/app/new_classic_language.hpp"
#include "th03/app/new_classic_playback.hpp"
#include "th03/app/new_classic_settings_store.hpp"
#include "th03/app/replay_slot_store.hpp"
#include "th03/app/startup_sequence.hpp"
#include "th03/app/tick_input.hpp"
#include "th03/audio/output_gain.hpp"
#include "th03/platform/sdl_input.hpp"
#include "th03/platform/sdl_audio_device.hpp"
#include "th03/platform/sdl_platform.hpp"

namespace {

constexpr std::uint64_t maximum_idle_delay_ns = 1'000'000U;
constexpr const char *pixel_oracle_log_environment = "TH03_PIXEL_ORACLE_LOG";
constexpr const char *pixel_oracle_rgb_environment = "TH03_PIXEL_ORACLE_RGB_DIR";
constexpr const char *pixel_oracle_identity_environment =
	"TH03_PIXEL_ORACLE_IDENTITY_LOG";
constexpr const char *pixel_oracle_fixture_environment =
	"TH03_PIXEL_ORACLE_FIXTURE_SHA256";

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
			"[--smoke-ticks n]\n"
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

int run_direct_vs(th03::app::DirectVsCliRequest parsed)
{
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
	bool recovered_replay_modal = false;
	if(new_classic && !parsed.capture_replay) {
		auto fallback = th03::app::load_replay_fallback(parsed.replay_root);
		if(!fallback) {
			if(fallback.error().code !=
				th03::app::ReplaySlotStoreErrorCode::format) {
				return {
					.exit_code = fail("replay-fallback-load", fallback.error())
				};
			}
			// Replay Patch replay_save_pending() treats an unreadable TH3LAST as
			// no pending save and lets the next recording replace it. Mirror that
			// availability policy at startup; I/O and removal failures remain loud.
			if(auto removed =
				th03::app::remove_replay_fallback(parsed.replay_root); !removed) {
				return {
					.exit_code = fail("replay-fallback-remove", removed.error())
				};
			}
			fallback = th03::app::ReplaySlotLoadResult{};
		}
		if(fallback->document) {
			auto saved = run_replay_save(
				std::move(*fallback->document), parsed.replay_root,
				parsed.tick_rate, platform, mapper, patch_assets,
				(*runner)->new_classic_replay_save_art()
			);
			if(saved.exit_code != 0) {
				return {.exit_code = saved.exit_code};
			}
			if(saved.application_quit) {
				return {
					.disposition = ExperienceRunDisposition::application_quit
				};
			}
			recovered_replay_modal = true;
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

	std::optional<th03::platform::SdlAudioDevice> audio_device{};
	auto created_device = th03::platform::SdlAudioDevice::create({
		.start_paused = true,
	});
	if(created_device) {
		audio_device.emplace(std::move(*created_device));
	} else {
		warn_audio_device("audio-device-create", created_device.error());
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
	if(recovered_replay_modal) {
		const auto resumed_at = th03::platform::sdl_ticks_now_ns();
		if(auto reset = mapper.reset_after_modal(*scheduler, resumed_at);
			!reset) {
			return {.exit_code = fail("input-rebase", reset.error())};
		}
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
		const auto cutoff = th03::platform::sdl_ticks_now_ns();
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
			presentation_pending =
				presentation_pending || consumed->surface_changed;
			if(consumed->surface_changed) {
				if(identity_output.enabled() && pending_publication_identity) {
					std::cerr << "status=error\nstage=pixel-oracle-identity\n"
						"code=identity-oracle-coalesced-publication\n";
					return {.exit_code = 1};
				}
				pending_publication_tick = logical_ticks;
				++pending_publication_ticks;
				pending_publication_identity = consumed->pixel_oracle_identity;
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

		if((*runner)->status() ==
			th03::app::ClassicRunnerStatus::quit_requested) {
			return {.disposition = ExperienceRunDisposition::application_quit};
		}
		if((*runner)->status() ==
			th03::app::ClassicRunnerStatus::switch_to_classic) {
			return {.disposition = ExperienceRunDisposition::switch_to_classic};
		}
		const bool session_complete = (*runner)->status() ==
			th03::app::ClassicRunnerStatus::session_complete;
		// Replay Patch finalizes and offers the recording before constructing or
		// showing Continue. The typed owner may already have prepared that surface,
		// so retain its pending publication across the host-owned save transaction.
		if(completed_replay && new_classic) {
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
			presented = platform.present(*surface, {
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
			return {.disposition = ExperienceRunDisposition::smoke_complete};
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
				auto committed = th03::app::commit_replay_fallback_to_slot(
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
			if(result.status ==
				th03::app::NewClassicReplaySaveStatus::discarded) {
				if(auto removed = th03::app::remove_replay_fallback(replay_root);
					!removed) {
					return std::unexpected(fail(
						"replay-fallback-remove", removed.error()
					));
				}
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
> create_product_platform(bool force_hidden = false)
{
	auto platform = th03::platform::SdlPlatform::create(
		platform_config(force_hidden)
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
	auto product = create_product_platform(parsed.capture_replay.has_value());
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
	auto product = create_product_platform();
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
