#ifndef TH03_APP_EXECUTABLE_RHYTHM_ASSETS_HPP
#define TH03_APP_EXECUTABLE_RHYTHM_ASSETS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <vector>

#include "th03/audio/pmd_opna.hpp"

namespace th03::app_executable {

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
		std::vector<std::int16_t>, audio::ym2608_rhythm_voice_count
	> voices{};

	[[nodiscard]] audio::PmdOpnaRhythmPcm view() const noexcept;
};

[[nodiscard]] std::expected<std::vector<std::byte>, RhythmRomLoadError>
load_rhythm_rom(const std::filesystem::path &path);

[[nodiscard]] std::expected<LoadedRhythmPcm, RhythmPcmLoadError>
load_rhythm_pcm(const std::filesystem::path &path);

int report_rhythm_load_failure(
	const char *stage,
	const RhythmRomLoadError &error
);

int report_rhythm_load_failure(
	const char *stage,
	const RhythmPcmLoadError &error
);

} // namespace th03::app_executable

#endif
