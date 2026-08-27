#include "rhythm_assets.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

#include "th03/app/content/main_compiled_content.hpp"

namespace th03::app_executable {
namespace {

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

} // namespace

audio::PmdOpnaRhythmPcm LoadedRhythmPcm::view() const noexcept
{
	audio::PmdOpnaRhythmPcm result{};
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

std::expected<std::vector<std::byte>, RhythmRomLoadError>
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
	if(size != audio::ym2608_rhythm_rom_bytes) {
		return std::unexpected(RhythmRomLoadError{
			.code = RhythmRomLoadErrorCode::file_size_mismatch,
			.observed_size = size,
		});
	}
	try {
		std::vector<std::byte> bytes(audio::ym2608_rhythm_rom_bytes);
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

std::expected<LoadedRhythmPcm, RhythmPcmLoadError>
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
		if(!input ||
			input.gcount() != static_cast<std::streamsize>(bytes.size())) {
			return std::unexpected(RhythmPcmLoadError{
				.code = RhythmPcmLoadErrorCode::file_read_failed,
			});
		}
		if(app::main_executable_sha256(bytes) != rhythm_pcm_profile_sha256) {
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

int report_rhythm_load_failure(
	const char *stage,
	const RhythmRomLoadError &error
)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< name(error.code) << '\n';
	if(error.code == RhythmRomLoadErrorCode::file_size_mismatch) {
		std::cerr << "expected-size=" << audio::ym2608_rhythm_rom_bytes
			<< "\nobserved-size=" << error.observed_size << '\n';
	}
	return 1;
}

int report_rhythm_load_failure(
	const char *stage,
	const RhythmPcmLoadError &error
)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode="
		<< name(error.code) << '\n';
	if(error.code == RhythmPcmLoadErrorCode::file_size_mismatch) {
		std::cerr << "expected-size=" << rhythm_pcm_profile_bytes
			<< "\nobserved-size=" << error.observed_size << '\n';
	}
	return 1;
}

} // namespace th03::app_executable
