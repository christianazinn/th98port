#include "th03/app/new_classic_device_store.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <ranges>
#include <span>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace th03::app {
namespace {

constexpr std::array<std::byte, 8U> magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'N'}, std::byte{'D'},
	std::byte{'E'}, std::byte{'V'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::uint16_t version = 1U;
constexpr std::size_t mode_offset = 12U;
constexpr std::size_t identity_offset = 16U;
constexpr std::size_t reserved_offset = 48U;
constexpr std::size_t digest_offset = 60U;

[[nodiscard]] constexpr std::uint8_t byte(std::byte value) noexcept
{
	return std::to_integer<std::uint8_t>(value);
}

[[nodiscard]] constexpr std::uint16_t u16(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(byte(bytes[offset])) |
		static_cast<std::uint16_t>(
			static_cast<std::uint16_t>(byte(bytes[offset + 1U])) << 8U
		);
}

[[nodiscard]] constexpr std::uint32_t u32(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	std::uint32_t result = 0U;
	for(std::size_t index = 0U; index < 4U; ++index) {
		result |= static_cast<std::uint32_t>(byte(bytes[offset + index])) <<
			(index * 8U);
	}
	return result;
}

constexpr void put_u16(
	std::span<std::byte> bytes,
	std::size_t offset,
	std::uint16_t value
) noexcept
{
	bytes[offset] = static_cast<std::byte>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

constexpr void put_u32(
	std::span<std::byte> bytes,
	std::size_t offset,
	std::uint32_t value
) noexcept
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
	}
}

[[nodiscard]] constexpr std::uint32_t fnv1a32(
	std::span<const std::byte> bytes
) noexcept
{
	std::uint32_t hash = 2'166'136'261U;
	for(const auto value : bytes) {
		hash ^= byte(value);
		hash *= 16'777'619U;
	}
	return hash;
}

[[nodiscard]] constexpr NewClassicDeviceCodecError codec_error(
	NewClassicDeviceCodecErrorCode code,
	std::size_t offset
) noexcept
{
	return {.code = code, .offset = offset};
}

[[nodiscard]] NewClassicDeviceStoreError store_error(
	NewClassicDeviceStoreErrorCode code,
	std::error_code system = {},
	NewClassicDeviceCodecError codec = {}
) noexcept
{
	return {.code = code, .system = system, .codec = codec};
}

[[nodiscard]] std::filesystem::path temporary_path(
	const std::filesystem::path &target
)
{
	static std::atomic<std::uint64_t> sequence{0U};
	auto result = target;
	result += std::filesystem::path(
		".tmp-" + std::to_string(sequence.fetch_add(1U))
	).native();
	return result;
}

} // namespace

std::expected<NewClassicDeviceAssignment, NewClassicDeviceCodecError>
decode_new_classic_device_assignment(std::span<const std::byte> bytes) noexcept
{
	if(bytes.size() != new_classic_device_file_bytes) {
		return std::unexpected(codec_error(
			NewClassicDeviceCodecErrorCode::input_size, bytes.size()
		));
	}
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		if(bytes[index] != magic[index]) {
			return std::unexpected(codec_error(
				NewClassicDeviceCodecErrorCode::magic, index
			));
		}
	}
	if(u16(bytes, 8U) != version) {
		return std::unexpected(codec_error(
			NewClassicDeviceCodecErrorCode::version, 8U
		));
	}
	if(u16(bytes, 10U) != new_classic_device_file_bytes) {
		return std::unexpected(codec_error(
			NewClassicDeviceCodecErrorCode::declared_size, 10U
		));
	}
	if(u32(bytes, digest_offset) != fnv1a32(bytes.first(digest_offset))) {
		return std::unexpected(codec_error(
			NewClassicDeviceCodecErrorCode::digest, digest_offset
		));
	}
	for(std::size_t index = 14U; index < identity_offset; ++index) {
		if(bytes[index] != std::byte{}) {
			return std::unexpected(codec_error(
				NewClassicDeviceCodecErrorCode::reserved, index
			));
		}
	}
	for(std::size_t index = reserved_offset; index < digest_offset; ++index) {
		if(bytes[index] != std::byte{}) {
			return std::unexpected(codec_error(
				NewClassicDeviceCodecErrorCode::reserved, index
			));
		}
	}
	NewClassicDeviceAssignment result{};
	for(std::size_t player = 0U; player < 2U; ++player) {
		result.mode[player] = static_cast<NewClassicControllerAssignmentMode>(
			byte(bytes[mode_offset + player])
		);
		for(std::size_t index = 0U;
			index < new_classic_controller_identity_bytes;
			++index) {
			result.identity[player].bytes[index] = byte(bytes[
				identity_offset + (player * new_classic_controller_identity_bytes) +
				index
			]);
		}
	}
	if(result.mode[0] == NewClassicControllerAssignmentMode::specific &&
		result.mode[1] == NewClassicControllerAssignmentMode::specific &&
		result.identity[0] == result.identity[1]) {
		return std::unexpected(codec_error(
			NewClassicDeviceCodecErrorCode::duplicate_identity,
			identity_offset + new_classic_controller_identity_bytes
		));
	}
	if(!valid_new_classic_device_assignment(result)) {
		const bool mode_valid = std::ranges::all_of(result.mode, [](auto mode) {
			return mode == NewClassicControllerAssignmentMode::automatic ||
				mode == NewClassicControllerAssignmentMode::specific ||
				mode == NewClassicControllerAssignmentMode::disabled;
		});
		return std::unexpected(codec_error(
			mode_valid ? NewClassicDeviceCodecErrorCode::identity :
				NewClassicDeviceCodecErrorCode::mode,
			mode_valid ? identity_offset : mode_offset
		));
	}
	return result;
}

std::expected<
	std::array<std::byte, new_classic_device_file_bytes>,
	NewClassicDeviceCodecError
> encode_new_classic_device_assignment(
	const NewClassicDeviceAssignment &assignment
) noexcept
{
	if(assignment.mode[0] == NewClassicControllerAssignmentMode::specific &&
		assignment.mode[1] == NewClassicControllerAssignmentMode::specific &&
		assignment.identity[0] == assignment.identity[1]) {
		return std::unexpected(codec_error(
			NewClassicDeviceCodecErrorCode::duplicate_identity,
			identity_offset + new_classic_controller_identity_bytes
		));
	}
	if(!valid_new_classic_device_assignment(assignment)) {
		return std::unexpected(codec_error(
			NewClassicDeviceCodecErrorCode::identity, identity_offset
		));
	}
	std::array<std::byte, new_classic_device_file_bytes> result{};
	std::copy(magic.begin(), magic.end(), result.begin());
	put_u16(result, 8U, version);
	put_u16(result, 10U, new_classic_device_file_bytes);
	for(std::size_t player = 0U; player < 2U; ++player) {
		result[mode_offset + player] = static_cast<std::byte>(
			assignment.mode[player]
		);
		for(std::size_t index = 0U;
			index < new_classic_controller_identity_bytes;
			++index) {
			result[identity_offset +
				(player * new_classic_controller_identity_bytes) + index] =
				static_cast<std::byte>(assignment.identity[player].bytes[index]);
		}
	}
	put_u32(result, digest_offset, fnv1a32(
		std::span<const std::byte>(result).first(digest_offset)
	));
	return result;
}

std::expected<NewClassicDeviceLoadResult, NewClassicDeviceStoreError>
load_new_classic_device_assignment(
	const std::filesystem::path &path,
	const NewClassicDeviceAssignment &defaults
)
{
	if(path.empty()) {
		return std::unexpected(store_error(
			NewClassicDeviceStoreErrorCode::invalid_path
		));
	}
	if(!valid_new_classic_device_assignment(defaults)) {
		return std::unexpected(store_error(
			NewClassicDeviceStoreErrorCode::invalid_defaults
		));
	}
	std::error_code error;
	const bool present = std::filesystem::exists(path, error);
	if(error) {
		return std::unexpected(store_error(
			NewClassicDeviceStoreErrorCode::open_failed, error
		));
	}
	if(!present) {
		return NewClassicDeviceLoadResult{.assignment = defaults};
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(store_error(
			NewClassicDeviceStoreErrorCode::open_failed,
			std::make_error_code(std::errc::io_error)
		));
	}
	std::array<std::byte, new_classic_device_file_bytes> bytes{};
	input.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
	if(input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
		input.peek() != std::ifstream::traits_type::eof()) {
		return NewClassicDeviceLoadResult{
			.assignment = defaults,
			.file_present = true,
			.recovered_defaults = true,
		};
	}
	auto decoded = decode_new_classic_device_assignment(bytes);
	if(!decoded) {
		return NewClassicDeviceLoadResult{
			.assignment = defaults,
			.file_present = true,
			.recovered_defaults = true,
		};
	}
	return NewClassicDeviceLoadResult{
		.assignment = *decoded,
		.file_present = true,
	};
}

std::expected<void, NewClassicDeviceStoreError>
store_new_classic_device_assignment(
	const std::filesystem::path &path,
	const NewClassicDeviceAssignment &assignment
)
{
	if(path.empty() || !path.has_filename()) {
		return std::unexpected(store_error(
			NewClassicDeviceStoreErrorCode::invalid_path
		));
	}
	auto encoded = encode_new_classic_device_assignment(assignment);
	if(!encoded) {
		return std::unexpected(store_error(
			NewClassicDeviceStoreErrorCode::codec, {}, encoded.error()
		));
	}
	std::error_code error;
	if(!path.parent_path().empty()) {
		std::filesystem::create_directories(path.parent_path(), error);
	}
	if(error) {
		return std::unexpected(store_error(
			NewClassicDeviceStoreErrorCode::open_failed, error
		));
	}
	const auto temporary = temporary_path(path);
	{
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		if(!output) {
			return std::unexpected(store_error(
				NewClassicDeviceStoreErrorCode::open_failed,
				std::make_error_code(std::errc::io_error)
			));
		}
		output.write(reinterpret_cast<const char *>(encoded->data()),
			static_cast<std::streamsize>(encoded->size()));
		output.flush();
		if(!output) {
			std::filesystem::remove(temporary, error);
			return std::unexpected(store_error(
				NewClassicDeviceStoreErrorCode::write_failed,
				std::make_error_code(std::errc::io_error)
			));
		}
	}
#if defined(_WIN32)
	std::error_code publish_error;
	if(MoveFileExW(temporary.c_str(), path.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
		publish_error = {
			static_cast<int>(GetLastError()), std::system_category()
		};
	}
#else
	std::error_code publish_error;
	std::filesystem::rename(temporary, path, publish_error);
#endif
	if(publish_error) {
		std::filesystem::remove(temporary, error);
		return std::unexpected(store_error(
			NewClassicDeviceStoreErrorCode::publish_failed, publish_error
		));
	}
	auto readback = load_new_classic_device_assignment(path, assignment);
	if(!readback || readback->recovered_defaults ||
		readback->assignment != assignment) {
		return std::unexpected(store_error(
			NewClassicDeviceStoreErrorCode::readback_failed
		));
	}
	return {};
}

} // namespace th03::app
