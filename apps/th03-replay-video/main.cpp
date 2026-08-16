#include "th03/app/classic_audio_session.hpp"
#include "th03/app/host_surface.hpp"
#include "th03/app/main_compiled_content.hpp"
#include "th03/app/new_classic_playback.hpp"
#include "th03/app/new_classic_settings.hpp"
#include "th03/audio/output_gain.hpp"
#include "th03/formats/v13.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <vector>

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

constexpr std::uint32_t source_rate_numerator = 526'315U;
constexpr std::uint32_t source_rate_denominator = 9'328U;
constexpr std::size_t source_rgba_bytes =
	static_cast<std::size_t>(th03::present::graphics_width) *
	th03::present::graphics_height * 4U;
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

struct Options final {
	fs::path replay{};
	fs::path output{};
	fs::path data{};
	fs::path main{};
	fs::path mainl{};
	fs::path font{};
	fs::path rhythm_pcm{};
	fs::path rhythm_rom{};
	fs::path settings{};
	fs::path ffmpeg{"ffmpeg"};
	std::uint32_t width = 1'920U;
	std::uint32_t height = 1'200U;
	std::size_t checkpoint = 0U;
	std::optional<std::uint64_t> maximum_frames{};
	std::string encoder{"auto"};
	std::string encoder_preset{"p4"};
	std::uint8_t master_volume = 100U;
	std::uint8_t music_volume = 100U;
	std::uint8_t effect_volume = 100U;
	th03::app::ClassicLanguage language =
		th03::app::ClassicLanguage::japanese;
	bool reduced_flash = false;
	bool overwrite = false;
	bool master_volume_overridden = false;
	bool music_volume_overridden = false;
	bool effect_volume_overridden = false;
	bool language_overridden = false;
	bool reduced_flash_overridden = false;
	bool use_installed_settings = false;
};

struct LoadedRhythmPcm final {
	std::array<
		std::vector<std::int16_t>, th03::audio::ym2608_rhythm_voice_count
	> voices{};

	[[nodiscard]] th03::audio::PmdOpnaRhythmPcm view() const noexcept
	{
		th03::audio::PmdOpnaRhythmPcm result{};
		result.sample_rate_hz = 44'100U;
		for(std::size_t index = 0U; index < voices.size(); ++index) {
			result.voices[index] = voices[index];
		}
		return result;
	}
};

struct SimulationResult final {
	std::uint64_t frames = 0U;
	std::uint64_t audio_samples = 0U;
	bool replay_complete = false;
};

enum class PassKind : std::uint8_t {
	audio,
	video,
};

void usage()
{
	std::cout <<
		"Usage: th03-replay-video --replay <TH3Rxx.RPY> --data <content> "
		"--output <video.mkv> [options]\n\n"
		"Renders exact 56.423 Hz New Classic replay output with synchronized "
		"PMD/OPNA audio.\n\n"
		"Options:\n"
		"  --resolution <WxH>       Output size (default 1920x1200)\n"
		"  --encoder <name>         auto, h264_nvenc, hevc_nvenc, "
		"av1_nvenc, or libx264\n"
		"  --encoder-preset <p1..p7> NVENC speed/quality (default p4; p1 fastest)\n"
		"  --ffmpeg <path>          FFmpeg executable (default: ffmpeg)\n"
		"  --checkpoint <index>     Begin at a replay checkpoint (default 0)\n"
		"  --language <ja|en>       Presentation language (default ja)\n"
		"  --reduced-flash          Apply New Classic reduced flashes\n"
		"  --full-flash             Override inherited reduced flashes\n"
		"  --settings <PODD.CFG>    Inherit New Classic presentation/audio values\n"
		"  --use-installed-settings Inherit the installed standalone's PODD.CFG\n"
		"  --master-volume <0..100> Final gain in 5% steps\n"
		"  --music-volume <0..100>  Music gain in 5% steps\n"
		"  --sfx-volume <0..100>    Effect gain in 5% steps\n"
		"  --main/--mainl/--font    Override content files\n"
		"  --rhythm-pcm <path>      T98RHYTHM.PCM (auto-detected under data)\n"
		"  --rhythm-rom <path>      8 KiB YM2608 rhythm ROM instead\n"
		"  --max-frames <count>     Diagnostic partial render\n"
		"  --overwrite              Replace an existing output\n";
}

template<typename Integer>
[[nodiscard]] bool parse_integer(std::string_view text, Integer &value)
{
	if(text.empty()) {
		return false;
	}
	Integer parsed{};
	const auto result = std::from_chars(
		text.data(), text.data() + text.size(), parsed
	);
	if(result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
		return false;
	}
	value = parsed;
	return true;
}

[[nodiscard]] bool parse_volume(std::string_view text, std::uint8_t &value)
{
	unsigned parsed = 0U;
	if(!parse_integer(text, parsed) || parsed > 100U || (parsed % 5U) != 0U) {
		return false;
	}
	value = static_cast<std::uint8_t>(parsed);
	return true;
}

[[nodiscard]] std::expected<Options, std::string> parse_options(
	int argc,
	char **argv
)
{
	Options options{};
	for(int index = 1; index < argc; ++index) {
		const std::string_view key{argv[index]};
		if(key == "--help" || key == "-h") {
			usage();
			std::exit(0);
		}
		if(key == "--reduced-flash") {
			options.reduced_flash = true;
			options.reduced_flash_overridden = true;
			continue;
		}
		if(key == "--full-flash") {
			options.reduced_flash = false;
			options.reduced_flash_overridden = true;
			continue;
		}
		if(key == "--use-installed-settings") {
			options.use_installed_settings = true;
			continue;
		}
		if(key == "--overwrite") {
			options.overwrite = true;
			continue;
		}
		if(index + 1 >= argc) {
			return std::unexpected("missing value after " + std::string(key));
		}
		const std::string_view value{argv[++index]};
		if(key == "--replay") options.replay = value;
		else if(key == "--output") options.output = value;
		else if(key == "--data") options.data = value;
		else if(key == "--main") options.main = value;
		else if(key == "--mainl") options.mainl = value;
		else if(key == "--font") options.font = value;
		else if(key == "--rhythm-pcm") options.rhythm_pcm = value;
		else if(key == "--rhythm-rom") options.rhythm_rom = value;
		else if(key == "--settings") options.settings = value;
		else if(key == "--ffmpeg") options.ffmpeg = value;
		else if(key == "--encoder") options.encoder = value;
		else if(key == "--encoder-preset") options.encoder_preset = value;
		else if(key == "--checkpoint") {
			if(!parse_integer(value, options.checkpoint)) {
				return std::unexpected("invalid checkpoint index");
			}
		} else if(key == "--max-frames") {
			std::uint64_t parsed = 0U;
			if(!parse_integer(value, parsed) || parsed == 0U) {
				return std::unexpected("invalid maximum frame count");
			}
			options.maximum_frames = parsed;
		} else if(key == "--master-volume") {
			if(!parse_volume(value, options.master_volume)) {
				return std::unexpected("invalid master volume");
			}
			options.master_volume_overridden = true;
		} else if(key == "--music-volume") {
			if(!parse_volume(value, options.music_volume)) {
				return std::unexpected("invalid music volume");
			}
			options.music_volume_overridden = true;
		} else if(key == "--sfx-volume") {
			if(!parse_volume(value, options.effect_volume)) {
				return std::unexpected("invalid SFX volume");
			}
			options.effect_volume_overridden = true;
		} else if(key == "--language") {
			if(value == "ja") {
				options.language = th03::app::ClassicLanguage::japanese;
			} else if(value == "en") {
				options.language = th03::app::ClassicLanguage::english;
			} else {
				return std::unexpected("language must be ja or en");
			}
			options.language_overridden = true;
		} else if(key == "--resolution") {
			const auto separator = value.find_first_of("xX");
			if(separator == std::string_view::npos ||
				!parse_integer(value.substr(0U, separator), options.width) ||
				!parse_integer(value.substr(separator + 1U), options.height) ||
				options.width == 0U || options.height == 0U ||
				static_cast<std::uint64_t>(options.width) * 5U !=
					static_cast<std::uint64_t>(options.height) * 8U) {
				return std::unexpected("resolution must be a nonzero 8:5 size");
			}
		} else {
			return std::unexpected("unknown option " + std::string(key));
		}
	}
	if(options.replay.empty() || options.output.empty() || options.data.empty()) {
		return std::unexpected("--replay, --data, and --output are required");
	}
	if(options.output.extension() != ".mkv" &&
		options.output.extension() != ".MKV") {
		return std::unexpected("output must use the .mkv extension");
	}
	if(options.use_installed_settings) {
		if(!options.settings.empty()) {
			return std::unexpected(
				"--settings and --use-installed-settings are mutually exclusive"
			);
		}
#if defined(_WIN32)
#if defined(_MSC_VER)
		char *local_app_data_raw = nullptr;
		std::size_t local_app_data_size = 0U;
		if(_dupenv_s(
			&local_app_data_raw, &local_app_data_size, "LOCALAPPDATA"
		) != 0) {
			return std::unexpected("LOCALAPPDATA cannot be read");
		}
		const std::unique_ptr<char, decltype(&std::free)> local_app_data(
			local_app_data_raw, &std::free
		);
		if(!local_app_data || local_app_data_size <= 1U) {
			return std::unexpected("LOCALAPPDATA is unavailable");
		}
		options.settings = fs::path(local_app_data.get()) /
			"PoDD Port" / "user" / "PODD.CFG";
#else
		const auto *local_app_data = std::getenv("LOCALAPPDATA");
		if(local_app_data == nullptr || *local_app_data == '\0') {
			return std::unexpected("LOCALAPPDATA is unavailable");
		}
		options.settings = fs::path(local_app_data) /
			"PoDD Port" / "user" / "PODD.CFG";
#endif
#else
		return std::unexpected(
			"installed settings discovery is only available on Windows"
		);
#endif
	}
	if(options.main.empty()) options.main = options.data / "MAIN.EXE";
	if(options.mainl.empty()) options.mainl = options.data / "MAINL.EXE";
	if(options.font.empty()) options.font = options.data / "FONT.ROM";
	if(options.rhythm_pcm.empty() && options.rhythm_rom.empty()) {
		const auto candidate = options.data / "T98RHYTHM.PCM";
		if(fs::is_regular_file(candidate)) options.rhythm_pcm = candidate;
	}
	if(!options.rhythm_pcm.empty() && !options.rhythm_rom.empty()) {
		return std::unexpected("rhythm PCM and rhythm ROM are mutually exclusive");
	}
	constexpr std::array<std::string_view, 5U> encoders{
		"auto", "h264_nvenc", "hevc_nvenc", "av1_nvenc", "libx264",
	};
	if(std::ranges::find(encoders, options.encoder) == encoders.end()) {
		return std::unexpected("unsupported encoder");
	}
	constexpr std::array<std::string_view, 7U> encoder_presets{
		"p1", "p2", "p3", "p4", "p5", "p6", "p7",
	};
	if(std::ranges::find(encoder_presets, options.encoder_preset) ==
		encoder_presets.end()) {
		return std::unexpected("encoder preset must be between p1 and p7");
	}
	return options;
}

[[nodiscard]] std::expected<std::vector<std::byte>, std::string> read_file(
	const fs::path &path
)
{
	std::error_code error{};
	const auto size = fs::file_size(path, error);
	if(error || size > std::numeric_limits<std::size_t>::max()) {
		return std::unexpected("cannot size " + path.string());
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	std::ifstream input(path, std::ios::binary);
	if(!input.is_open()) {
		return std::unexpected("cannot open " + path.string());
	}
	input.read(reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	if(!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
		return std::unexpected("cannot read " + path.string());
	}
	return bytes;
}

[[nodiscard]] std::expected<void, std::string> inherit_settings(
	Options &options
)
{
	if(options.settings.empty()) {
		return {};
	}
	auto bytes = read_file(options.settings);
	if(!bytes) {
		return std::unexpected(bytes.error());
	}
	auto decoded = th03::app::decode_new_classic_settings(*bytes);
	if(!decoded) {
		return std::unexpected("cannot decode " + options.settings.string() +
			" (code " + std::to_string(static_cast<unsigned>(
				decoded.error().code
			)) + ", offset " + std::to_string(decoded.error().offset) + ')');
	}
	if(!options.master_volume_overridden) {
		options.master_volume = decoded->master_volume_percent;
	}
	if(!options.music_volume_overridden) {
		options.music_volume = decoded->music_volume_percent;
	}
	if(!options.effect_volume_overridden) {
		options.effect_volume = decoded->effect_volume_percent;
	}
	if(!options.language_overridden) {
		options.language = decoded->options.language;
	}
	if(!options.reduced_flash_overridden) {
		options.reduced_flash = decoded->options.reduced_flash;
	}
	return {};
}

[[nodiscard]] std::uint32_t read_le32(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
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

[[nodiscard]] std::expected<LoadedRhythmPcm, std::string> load_rhythm_pcm(
	const fs::path &path
)
{
	auto loaded = read_file(path);
	if(!loaded || loaded->size() != rhythm_pcm_profile_bytes ||
		th03::app::main_executable_sha256(*loaded) !=
			rhythm_pcm_profile_sha256) {
		return std::unexpected("T98RHYTHM.PCM profile mismatch");
	}
	constexpr std::array<std::byte, 8U> magic{
		std::byte{'T'}, std::byte{'9'}, std::byte{'8'}, std::byte{'R'},
		std::byte{'H'}, std::byte{'Y'}, std::byte{'0'}, std::byte{0U},
	};
	if(!std::equal(magic.begin(), magic.end(), loaded->begin()) ||
		read_le32(*loaded, 8U) != 44'100U) {
		return std::unexpected("T98RHYTHM.PCM format mismatch");
	}
	LoadedRhythmPcm result{};
	std::size_t cursor = rhythm_pcm_header_bytes;
	for(std::size_t voice = 0U; voice < result.voices.size(); ++voice) {
		const auto length = rhythm_pcm_voice_lengths[voice];
		if(read_le32(*loaded, 12U + (voice * 4U)) != length) {
			return std::unexpected("T98RHYTHM.PCM voice table mismatch");
		}
		auto &samples = result.voices[voice];
		samples.resize(length);
		for(auto &sample : samples) {
			sample = static_cast<std::int16_t>(
				std::to_integer<std::uint8_t>((*loaded)[cursor]) |
				(static_cast<std::uint16_t>(
					std::to_integer<std::uint8_t>((*loaded)[cursor + 1U])
				) << 8U)
			);
			cursor += 2U;
		}
	}
	if(cursor != loaded->size()) {
		return std::unexpected("T98RHYTHM.PCM trailing data mismatch");
	}
	return result;
}

[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
load_rhythm_rom(const fs::path &path)
{
	auto loaded = read_file(path);
	if(!loaded || loaded->size() != th03::audio::ym2608_rhythm_rom_bytes) {
		return std::unexpected("YM2608 rhythm ROM must be exactly 8192 bytes");
	}
	return loaded;
}

[[nodiscard]] std::optional<std::string> quote(const fs::path &path)
{
	const auto value = path.string();
	if(value.find_first_of("\"\r\n&|<>^%();!`") != std::string::npos) {
		return std::nullopt;
	}
	if(value.find_first_of(" \t") == std::string::npos) {
		return value;
	}
	return '"' + value + '"';
}

[[nodiscard]] std::FILE *open_pipe(const std::string &command)
{
#if defined(_WIN32)
	return _popen(command.c_str(), "wb");
#else
	return popen(command.c_str(), "w");
#endif
}

[[nodiscard]] int close_pipe(std::FILE *pipe)
{
#if defined(_WIN32)
	return _pclose(pipe);
#else
	return pclose(pipe);
#endif
}

class Progress final {
public:
	Progress(
		std::string label,
		std::uint32_t first_sample,
		std::uint32_t total_samples
	) :
		label_(std::move(label)), first_sample_(first_sample),
		total_samples_(std::max(first_sample, total_samples)), started_(Clock::now())
	{
		update(0U, 0U, true);
	}

	void update(
		std::uint32_t sample,
		std::uint64_t frames,
		bool force = false
	)
	{
		const auto now = Clock::now();
		if(!force && now - last_ < std::chrono::milliseconds(250)) {
			return;
		}
		last_ = now;
		const auto completed_samples = sample > first_sample_ ?
			sample - first_sample_ : 0U;
		const auto remaining_samples = total_samples_ - first_sample_;
		const double ratio = remaining_samples == 0U ? 0.0 : std::min(
			1.0, static_cast<double>(completed_samples) / remaining_samples
		);
		const auto filled = static_cast<std::size_t>(ratio * 30.0);
		const double source_seconds =
			static_cast<double>(frames) * source_rate_denominator /
			source_rate_numerator;
		const double elapsed = std::chrono::duration<double>(now - started_).count();
		const double speed = elapsed > 0.0 ? source_seconds / elapsed : 0.0;
		const double eta = ratio > 0.0 && ratio < 1.0 ?
			elapsed * (1.0 - ratio) / ratio : 0.0;
		std::cerr << '\r' << label_ << " [";
		for(std::size_t index = 0U; index < 30U; ++index) {
			std::cerr << (index < filled ? '#' : '-');
		}
		std::cerr << "] " << std::fixed << std::setprecision(1)
			<< (ratio * 100.0) << "%  " << source_seconds << "s  "
			<< speed << "x";
		if(eta > 0.0) {
			std::cerr << "  ETA " << static_cast<unsigned long long>(eta) << 's';
		}
		std::cerr << "      " << std::flush;
	}

	void finish(std::uint32_t sample, std::uint64_t frames)
	{
		update(sample, frames, true);
		std::cerr << '\n';
	}

private:
	std::string label_{};
	std::uint32_t first_sample_ = 0U;
	std::uint32_t total_samples_ = 0U;
	Clock::time_point started_{};
	Clock::time_point last_{};
};

class AsyncFrameWriter final {
public:
	explicit AsyncFrameWriter(std::FILE *pipe) : pipe_(pipe)
	{
		for(auto &slot : slots_) {
			slot.pixels.resize(th03::present::graphics_page_pixels);
		}
		worker_ = std::thread([this]() { run(); });
	}

	~AsyncFrameWriter()
	{
		(void)finish();
	}

	AsyncFrameWriter(const AsyncFrameWriter &) = delete;
	AsyncFrameWriter &operator=(const AsyncFrameWriter &) = delete;

	[[nodiscard]] bool submit(const th03::present::SourceSurface &surface)
	{
		std::unique_lock lock(mutex_);
		available_.wait(lock, [this]() {
			return failed_ || count_ < slots_.size();
		});
		if(failed_ || closing_ ||
			surface.pixels.size() != th03::present::graphics_page_pixels) {
			return false;
		}
		auto &slot = slots_[write_];
		std::ranges::copy(surface.pixels, slot.pixels.begin());
		slot.palette = surface.graphics_palette;
		write_ = (write_ + 1U) % slots_.size();
		++count_;
		available_.notify_all();
		return true;
	}

	[[nodiscard]] bool finish()
	{
		{
			std::lock_guard lock(mutex_);
			closing_ = true;
			available_.notify_all();
		}
		if(worker_.joinable()) {
			worker_.join();
		}
		return !failed_;
	}

private:
	struct Slot final {
		std::vector<th03::present::ComposedPixel> pixels{};
		th03::assets::Palette16Rgb4 palette{};
	};

	void run()
	{
		std::vector<std::byte> rgba(source_rgba_bytes);
		for(;;) {
			std::size_t slot = 0U;
			{
				std::unique_lock lock(mutex_);
				available_.wait(lock, [this]() {
					return closing_ || count_ != 0U;
				});
				if(count_ == 0U) {
					return;
				}
				slot = read_;
			}
			const auto &frame = slots_[slot];
			const th03::present::SourceSurface source{
				.pixels = std::span<
					const th03::present::ComposedPixel,
					th03::present::graphics_page_pixels
				>(frame.pixels),
				.graphics_palette = frame.palette,
			};
			const auto converted = th03::app::source_surface_to_host_rgba(
				source,
				{
					.bytes = rgba,
					.pitch_bytes = th03::present::graphics_width * 4U,
					.byte_order = th03::app::HostRgbaByteOrder::rgba,
				}
			);
			const bool written = converted.has_value() && std::fwrite(
				rgba.data(), 1U, rgba.size(), pipe_
			) == rgba.size();
			{
				std::lock_guard lock(mutex_);
				if(!written) {
					failed_ = true;
					closing_ = true;
					count_ = 0U;
					available_.notify_all();
					return;
				}
				read_ = (read_ + 1U) % slots_.size();
				--count_;
				available_.notify_all();
			}
		}
	}

	static constexpr std::size_t capacity = 4U;
	std::FILE *pipe_ = nullptr;
	std::array<Slot, capacity> slots_{};
	std::thread worker_{};
	std::mutex mutex_{};
	std::condition_variable available_{};
	std::size_t read_ = 0U;
	std::size_t write_ = 0U;
	std::size_t count_ = 0U;
	bool closing_ = false;
	bool failed_ = false;
};

[[nodiscard]] std::expected<void, std::string> write_all(
	std::FILE *pipe,
	const void *data,
	std::size_t element_size,
	std::size_t count
)
{
	if(count != 0U && std::fwrite(data, element_size, count, pipe) != count) {
		return std::unexpected("FFmpeg pipe write failed");
	}
	return {};
}

[[nodiscard]] std::expected<SimulationResult, std::string> simulate(
	const Options &options,
	const th03::formats::ReplayDocument &document,
	const LoadedRhythmPcm *rhythm_pcm,
	std::span<const std::byte> rhythm_rom,
	PassKind pass,
	std::FILE *pipe,
	std::string progress_label,
	std::vector<th03::app::ClassicAudioStatus> *captured_audio_status,
	std::span<const th03::app::ClassicAudioStatus> replayed_audio_status
)
{
	const auto score_root = fs::temp_directory_path() /
		("th03-replay-video-" + std::to_string(
			Clock::now().time_since_epoch().count()
		));
	std::error_code filesystem_error{};
	fs::create_directories(score_root, filesystem_error);
	if(filesystem_error) {
		return std::unexpected("cannot create temporary replay directory");
	}
	const auto cleanup = [&]() { fs::remove_all(score_root, filesystem_error); };
	auto playback = th03::app::NewClassicReplayPlayback::load({
		.content = {
			.data_root = options.data,
			.main_executable = options.main,
			.mainl_executable = options.mainl,
			.font_rom = options.font,
			.language_overlay = options.data / "TH3EN.DAT",
			.score = score_root / "YUME.NEM",
		},
		.document = document,
		.checkpoint_index = options.checkpoint,
		.reduced_flash = options.reduced_flash,
		.language = options.language,
	});
	if(!playback) {
		cleanup();
		return std::unexpected("replay playback initialization failed (code " +
			std::to_string(static_cast<unsigned>(playback.error().code)) + ')');
	}
	std::unique_ptr<th03::app::ClassicAudioSession> audio{};
	if(pass == PassKind::audio) {
		auto created_audio = th03::app::ClassicAudioSession::create({
			.data_root = options.data,
			.tick_rate = th03::app::pc98_24khz_nominal_tick_rate,
			.bgm_active = true,
			.effects_active = true,
			.effects_follow_bgm = false,
			.music_volume_percent = options.music_volume,
			.effect_volume_percent = options.effect_volume,
			.rhythm_rom = rhythm_rom,
			.rhythm_pcm = rhythm_pcm ? rhythm_pcm->view() :
				th03::audio::PmdOpnaRhythmPcm{},
		});
		if(!created_audio) {
			cleanup();
			return std::unexpected("audio initialization failed (code " +
				std::to_string(static_cast<unsigned>(
					created_audio.error().code
				)) + ')');
		}
		audio = std::move(*created_audio);
	}
	const auto drain_audio = [&]() -> std::expected<void, std::string> {
		while(const auto command = (*playback)->take_audio_command()) {
			if(audio) {
				auto applied = audio->apply(*command);
				if(applied) {
					continue;
				}
				return std::unexpected("audio command failed (code " +
					std::to_string(static_cast<unsigned>(applied.error().code)) + ')');
			}
		}
		return {};
	};
	if(auto drained = drain_audio(); !drained) {
		cleanup();
		return std::unexpected(drained.error());
	}
	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	const auto pixel_span = std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(pixels.data(), pixels.size());
	std::vector<th03::audio::PmdCanonicalStereoSample> gained_audio{};
	std::optional<AsyncFrameWriter> video_writer{};
	std::optional<th03::present::SourceSurface> current_surface{};
	if(pass == PassKind::video) {
		video_writer.emplace(pipe);
	}
	const auto compose = [&]() -> std::expected<void, std::string> {
		auto surface = (*playback)->compose_shown_surface(pixel_span);
		if(!surface) {
			return std::unexpected("surface composition failed");
		}
		current_surface = *surface;
		return {};
	};
	if(pass == PassKind::video) {
		if(auto composed = compose(); !composed) {
			cleanup();
			return std::unexpected(composed.error());
		}
	}
	Progress progress(
		std::move(progress_label),
		document.checkpoints[options.checkpoint].sample_index,
		document.session.sample_count
	);
	SimulationResult result{};
	while((*playback)->status() == th03::app::NewClassicPlaybackStatus::running &&
		(!options.maximum_frames || result.frames < *options.maximum_frames)) {
		if(pass == PassKind::video) {
			if(!current_surface || !video_writer->submit(*current_surface)) {
				cleanup();
				return std::unexpected("FFmpeg frame queue write failed");
			}
		}
		th03::app::ClassicAudioStatus audio_status{};
		if(audio) {
			audio_status = audio->status();
			if(captured_audio_status) {
				captured_audio_status->push_back(audio_status);
			}
		} else {
			if(result.frames >= replayed_audio_status.size()) {
				cleanup();
				return std::unexpected("audio-status timeline ended before video");
			}
			audio_status = replayed_audio_status[
				static_cast<std::size_t>(result.frames)
			];
		}
		auto advanced = (*playback)->advance(audio_status, false, false);
		if(!advanced) {
			cleanup();
			return std::unexpected("replay advance failed (code " +
				std::to_string(static_cast<unsigned>(advanced.error().code)) + ')');
		}
		if(auto drained = drain_audio(); !drained) {
			cleanup();
			return std::unexpected(drained.error());
		}
		if(pass == PassKind::audio) {
			auto audio_tick = audio->advance_tick();
			if(!audio_tick) {
				cleanup();
				return std::unexpected("audio render failed (code " +
					std::to_string(static_cast<unsigned>(
						audio_tick.error().code
					)) + ')');
			}
			std::span<const th03::audio::PmdCanonicalStereoSample> frames =
				audio_tick->canonical_frames;
			if(options.master_volume != 100U) {
				gained_audio.assign(frames.begin(), frames.end());
				if(!th03::audio::apply_output_gain(
					gained_audio, options.master_volume
				)) {
					cleanup();
					return std::unexpected("invalid master volume");
				}
				frames = gained_audio;
			}
			if(auto written = write_all(
				pipe, frames.data(), sizeof(th03::audio::PmdCanonicalStereoSample),
				frames.size()
			); !written) {
				cleanup();
				return std::unexpected(written.error());
			}
			result.audio_samples += frames.size();
		}
		if(pass == PassKind::video && advanced->surface_changed &&
			advanced->status == th03::app::NewClassicPlaybackStatus::running) {
			if(auto composed = compose(); !composed) {
				cleanup();
				return std::unexpected(composed.error());
			}
		}
		++result.frames;
		progress.update((*playback)->sample_index(), result.frames);
	}
	result.replay_complete = (*playback)->status() ==
		th03::app::NewClassicPlaybackStatus::complete;
	if(video_writer && !video_writer->finish()) {
		cleanup();
		return std::unexpected("FFmpeg frame queue write failed");
	}
	progress.finish((*playback)->sample_index(), result.frames);
	cleanup();
	return result;
}

[[nodiscard]] std::expected<int, std::string> run_pipe_pass(
	const std::string &command,
	const Options &options,
	const th03::formats::ReplayDocument &document,
	const LoadedRhythmPcm *rhythm_pcm,
	std::span<const std::byte> rhythm_rom,
	PassKind pass,
	std::string label,
	SimulationResult &result,
	std::vector<th03::app::ClassicAudioStatus> *captured_audio_status = nullptr,
	std::span<const th03::app::ClassicAudioStatus> replayed_audio_status = {}
)
{
	std::FILE *pipe = open_pipe(command);
	if(!pipe) {
		return std::unexpected("cannot start FFmpeg");
	}
	auto simulated = simulate(
		options, document, rhythm_pcm, rhythm_rom, pass, pipe, std::move(label),
		captured_audio_status, replayed_audio_status
	);
	const int exit_code = close_pipe(pipe);
	if(!simulated) {
		return std::unexpected(simulated.error());
	}
	result = *simulated;
	return exit_code;
}

[[nodiscard]] bool encoder_available(
	const fs::path &ffmpeg,
	std::string_view encoder
)
{
	const auto executable = quote(ffmpeg);
	if(!executable) return false;
#if defined(_WIN32)
	const std::string sink = " >NUL 2>NUL";
#else
	const std::string sink = " >/dev/null 2>/dev/null";
#endif
	const std::string command = *executable + " -hide_banner -loglevel error "
		"-h encoder=" + std::string(encoder) + sink;
	return std::system(command.c_str()) == 0;
}

[[nodiscard]] std::string encoder_arguments(
	std::string_view encoder,
	std::string_view preset
)
{
	if(encoder == "libx264") {
		return "-c:v libx264 -preset medium -crf 18";
	}
	return "-c:v " + std::string(encoder) +
		" -preset " + std::string(preset) +
		" -tune hq -rc vbr -cq 18 -b:v 0";
}

[[nodiscard]] int run(const Options &options)
{
	static_assert(std::endian::native == std::endian::little);
	static_assert(sizeof(th03::audio::PmdCanonicalStereoSample) == 8U);
	std::error_code filesystem_error{};
	if(fs::exists(options.output, filesystem_error) && !options.overwrite) {
		std::cerr << "error: output exists; pass --overwrite to replace it\n";
		return 2;
	}
	if(!options.output.parent_path().empty()) {
		fs::create_directories(options.output.parent_path(), filesystem_error);
		if(filesystem_error) {
			std::cerr << "error: cannot create output directory\n";
			return 2;
		}
	}
	auto replay_bytes = read_file(options.replay);
	if(!replay_bytes) {
		std::cerr << "error: " << replay_bytes.error() << '\n';
		return 2;
	}
	auto document = th03::formats::parse_v13_replay(*replay_bytes);
	if(!document || !th03::formats::replay_can_be_verified(*document) ||
		options.checkpoint >= document->checkpoints.size()) {
		std::cerr << "error: replay is not a finalized selectable V13 document\n";
		return 2;
	}
	std::optional<LoadedRhythmPcm> rhythm_pcm{};
	std::vector<std::byte> rhythm_rom{};
	if(!options.rhythm_pcm.empty()) {
		auto loaded = load_rhythm_pcm(options.rhythm_pcm);
		if(!loaded) {
			std::cerr << "error: " << loaded.error() << '\n';
			return 2;
		}
		rhythm_pcm.emplace(std::move(*loaded));
	} else if(!options.rhythm_rom.empty()) {
		auto loaded = load_rhythm_rom(options.rhythm_rom);
		if(!loaded) {
			std::cerr << "error: " << loaded.error() << '\n';
			return 2;
		}
		rhythm_rom = std::move(*loaded);
	} else {
		std::cerr << "warning: no rhythm PCM/ROM; drums will be silent\n";
	}
	const auto executable = quote(options.ffmpeg);
	const fs::path audio_path = options.output.string() + ".th03-audio.flac";
	const auto audio_output = quote(audio_path);
	const auto video_output = quote(options.output);
	if(!executable || !audio_output || !video_output ||
		options.ffmpeg.string().find_first_of(" \t") != std::string::npos) {
		std::cerr << "error: shell metacharacters are unsupported in output "
			"paths, and the FFmpeg executable path must not contain whitespace "
			"(put it on PATH instead)\n";
		return 2;
	}
	fs::remove(audio_path, filesystem_error);
	const std::string audio_command = *executable +
		" -hide_banner -loglevel error -nostdin -y -f s32le -ar 48000 -ac 2 "
		"-i pipe:0 -c:a flac " + *audio_output;
	SimulationResult audio_result{};
	std::vector<th03::app::ClassicAudioStatus> audio_status_timeline{};
	auto audio_exit = run_pipe_pass(
		audio_command, options, *document,
		rhythm_pcm ? &*rhythm_pcm : nullptr, rhythm_rom,
		PassKind::audio, "Audio 1/2", audio_result, &audio_status_timeline
	);
	if(!audio_exit || *audio_exit != 0) {
		fs::remove(audio_path, filesystem_error);
		std::cerr << "error: " << (audio_exit ? "FFmpeg audio pass failed" :
			audio_exit.error()) << '\n';
		return 1;
	}
	std::string encoder = options.encoder;
	const bool automatic = encoder == "auto";
	if(automatic) {
		encoder = encoder_available(options.ffmpeg, "h264_nvenc") ?
			"h264_nvenc" : "libx264";
	}
	std::cerr << "Video encoder: " << encoder << '\n';
	const auto render_video = [&](std::string_view selected) {
		const bool nvenc = selected.ends_with("_nvenc");
		const std::string scale_filter = nvenc ?
			("format=yuv444p,hwupload_cuda,scale_cuda=" +
				std::to_string(options.width) + ':' +
				std::to_string(options.height) +
				":interp_algo=nearest:format=yuv444p") :
			("scale=" + std::to_string(options.width) + ':' +
				std::to_string(options.height) +
				":flags=neighbor,format=yuv444p");
		const std::string command = *executable +
			" -hide_banner -loglevel error -nostdin -y -f rawvideo -pix_fmt "
			"rgba -video_size 640x400 -framerate 526315/9328 -i pipe:0 -i " +
			*audio_output + " -vf " + scale_filter +
			" -map 0:v:0 -map 1:a:0 " +
			encoder_arguments(selected, options.encoder_preset) +
			" -c:a copy -shortest -f matroska " + *video_output;
		SimulationResult result{};
		auto exit = run_pipe_pass(
			command, options, *document,
			rhythm_pcm ? &*rhythm_pcm : nullptr, rhythm_rom,
			PassKind::video, "Video 2/2", result, nullptr,
			audio_status_timeline
		);
		return std::pair{std::move(exit), result};
	};
	auto [video_exit, video_result] = render_video(encoder);
	if((!video_exit || *video_exit != 0) && automatic && encoder != "libx264") {
		std::cerr << "NVENC failed; retrying with libx264\n";
		fs::remove(options.output, filesystem_error);
		std::tie(video_exit, video_result) = render_video("libx264");
		encoder = "libx264";
	}
	fs::remove(audio_path, filesystem_error);
	if(!video_exit || *video_exit != 0) {
		std::cerr << "error: " << (video_exit ? "FFmpeg video pass failed" :
			video_exit.error()) << '\n';
		return 1;
	}
	if(video_result.frames != audio_result.frames) {
		std::cerr << "error: deterministic A/V passes produced different lengths\n";
		return 1;
	}
	if(!options.maximum_frames &&
		(!audio_result.replay_complete || !video_result.replay_complete)) {
		std::cerr << "error: replay did not reach its terminal boundary\n";
		return 1;
	}
	const double seconds = static_cast<double>(video_result.frames) *
		source_rate_denominator / source_rate_numerator;
	std::cout << "status=ok\noutput=" << options.output.string()
		<< "\nencoder=" << encoder << "\nframes=" << video_result.frames
		<< "\naudio-samples=" << audio_result.audio_samples << "\nduration="
		<< std::fixed << std::setprecision(3) << seconds
		<< "\nlanguage=" << (options.language ==
			th03::app::ClassicLanguage::english ? "en" : "ja")
		<< "\nmaster-volume=" << static_cast<unsigned>(options.master_volume)
		<< "\nmusic-volume=" << static_cast<unsigned>(options.music_volume)
		<< "\nsfx-volume=" << static_cast<unsigned>(options.effect_volume)
		<< "\nreduced-flash=" << (options.reduced_flash ? 1 : 0)
		<< "\nencoder-preset=" << options.encoder_preset << "\n";
	return 0;
}

} // namespace

int main(int argc, char **argv)
{
	try {
		auto options = parse_options(argc, argv);
		if(!options) {
			std::cerr << "error: " << options.error() << "\n\n";
			usage();
			return 2;
		}
		if(auto inherited = inherit_settings(*options); !inherited) {
			std::cerr << "error: " << inherited.error() << '\n';
			return 2;
		}
		return run(*options);
	} catch(const std::bad_alloc &) {
		std::cerr << "error: allocation failed\n";
		return 1;
	} catch(const std::exception &error) {
		std::cerr << "error: " << error.what() << '\n';
		return 1;
	}
}
