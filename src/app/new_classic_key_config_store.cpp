#include "th03/app/new_classic_key_config_store.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace th03::app {
namespace {

constexpr std::array<std::byte, 8U> magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'N'}, std::byte{'K'},
	std::byte{'C'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::uint16_t format_version = 1U;
constexpr std::size_t header_bytes = 14U;
constexpr std::size_t digest_offset = new_classic_key_config_file_bytes - 4U;

[[nodiscard]] constexpr NewClassicKeyConfigCodecError codec_error(
	NewClassicKeyConfigCodecErrorCode code,
	std::size_t offset
) noexcept
{
	return {.code = code, .offset = offset};
}

[[nodiscard]] constexpr std::uint8_t byte(std::byte value) noexcept
{
	return std::to_integer<std::uint8_t>(value);
}

[[nodiscard]] constexpr std::uint16_t u16(
	std::span<const std::byte> bytes,
	std::size_t at
) noexcept
{
	return static_cast<std::uint16_t>(
		byte(bytes[at]) | (static_cast<std::uint16_t>(byte(bytes[at + 1U])) << 8U)
	);
}

[[nodiscard]] constexpr std::uint32_t u32(
	std::span<const std::byte> bytes,
	std::size_t at
) noexcept
{
	return static_cast<std::uint32_t>(byte(bytes[at])) |
		(static_cast<std::uint32_t>(byte(bytes[at + 1U])) << 8U) |
		(static_cast<std::uint32_t>(byte(bytes[at + 2U])) << 16U) |
		(static_cast<std::uint32_t>(byte(bytes[at + 3U])) << 24U);
}

constexpr void put_u16(
	std::span<std::byte> bytes,
	std::size_t at,
	std::uint16_t value
) noexcept
{
	bytes[at] = static_cast<std::byte>(value & 0xFFU);
	bytes[at + 1U] = static_cast<std::byte>(value >> 8U);
}

constexpr void put_u32(
	std::span<std::byte> bytes,
	std::size_t at,
	std::uint32_t value
) noexcept
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		bytes[at + index] = static_cast<std::byte>(value >> (index * 8U));
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

[[nodiscard]] NewClassicKeyConfigStoreError store_error(
	NewClassicKeyConfigStoreErrorCode code,
	std::error_code system = {},
	NewClassicKeyConfigCodecError codec = {}
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

std::expected<void, NewClassicKeyConfigCodecError>
validate_new_classic_key_config(
	const NewClassicKeyConfig &config,
	std::uint16_t maximum_host_key
) noexcept
{
	if(config.autofire > 3U) {
		return std::unexpected(codec_error(
			NewClassicKeyConfigCodecErrorCode::autofire, 12U
		));
	}
	for(std::size_t index = 0U; index < config.bindings.size(); ++index) {
		const auto binding = config.bindings[index];
		if(binding != new_classic_host_key_unbound &&
			(binding == 0U || binding >= maximum_host_key)) {
			return std::unexpected(codec_error(
				NewClassicKeyConfigCodecErrorCode::binding,
				header_bytes + (index * 2U)
			));
		}
		const auto first = index < 2U * new_classic_player_action_count ?
			0U : 2U * new_classic_player_action_count;
		for(std::size_t earlier = first; earlier < index; ++earlier) {
			if(binding != new_classic_host_key_unbound &&
				config.bindings[earlier] == binding) {
				return std::unexpected(codec_error(
					NewClassicKeyConfigCodecErrorCode::duplicate_binding,
					header_bytes + (index * 2U)
				));
			}
		}
	}
	return {};
}

std::expected<NewClassicKeyConfig, NewClassicKeyConfigCodecError>
decode_new_classic_key_config(
	std::span<const std::byte> bytes,
	std::uint16_t maximum_host_key
) noexcept
{
	if(bytes.size() != new_classic_key_config_file_bytes) {
		return std::unexpected(codec_error(
			NewClassicKeyConfigCodecErrorCode::input_size, bytes.size()
		));
	}
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		if(bytes[index] != magic[index]) {
			return std::unexpected(codec_error(
				NewClassicKeyConfigCodecErrorCode::magic, index
			));
		}
	}
	if(u16(bytes, 8U) != format_version) {
		return std::unexpected(codec_error(
			NewClassicKeyConfigCodecErrorCode::version, 8U
		));
	}
	if(u16(bytes, 10U) != new_classic_key_config_file_bytes) {
		return std::unexpected(codec_error(
			NewClassicKeyConfigCodecErrorCode::declared_size, 10U
		));
	}
	if(u32(bytes, digest_offset) != fnv1a32(bytes.first(digest_offset))) {
		return std::unexpected(codec_error(
			NewClassicKeyConfigCodecErrorCode::digest, digest_offset
		));
	}
	NewClassicKeyConfig result{.autofire = byte(bytes[12U])};
	for(std::size_t index = 0U; index < result.bindings.size(); ++index) {
		result.bindings[index] = u16(bytes, header_bytes + (index * 2U));
	}
	auto valid = validate_new_classic_key_config(result, maximum_host_key);
	if(!valid) {
		return std::unexpected(valid.error());
	}
	return result;
}

std::expected<
	std::array<std::byte, new_classic_key_config_file_bytes>,
	NewClassicKeyConfigCodecError
> encode_new_classic_key_config(
	const NewClassicKeyConfig &config,
	std::uint16_t maximum_host_key
) noexcept
{
	auto valid = validate_new_classic_key_config(config, maximum_host_key);
	if(!valid) {
		return std::unexpected(valid.error());
	}
	std::array<std::byte, new_classic_key_config_file_bytes> result{};
	std::copy(magic.begin(), magic.end(), result.begin());
	put_u16(result, 8U, format_version);
	put_u16(result, 10U, new_classic_key_config_file_bytes);
	result[12U] = static_cast<std::byte>(config.autofire);
	for(std::size_t index = 0U; index < config.bindings.size(); ++index) {
		put_u16(result, header_bytes + (index * 2U), config.bindings[index]);
	}
	put_u32(result, digest_offset, fnv1a32(
		std::span<const std::byte>(result).first(digest_offset)
	));
	return result;
}

std::expected<NewClassicKeyConfigLoadResult, NewClassicKeyConfigStoreError>
load_new_classic_key_config(
	const std::filesystem::path &path,
	const NewClassicKeyConfig &defaults,
	std::uint16_t maximum_host_key,
	bool legacy_autofire
)
{
	if(path.empty()) {
		return std::unexpected(store_error(
			NewClassicKeyConfigStoreErrorCode::invalid_path
		));
	}
	if(auto valid = validate_new_classic_key_config(defaults, maximum_host_key);
		!valid) {
		return std::unexpected(store_error(
			NewClassicKeyConfigStoreErrorCode::invalid_defaults, {}, valid.error()
		));
	}
	std::error_code status_error;
	const bool present = std::filesystem::exists(path, status_error);
	if(status_error) {
		return std::unexpected(store_error(
			NewClassicKeyConfigStoreErrorCode::open_failed, status_error
		));
	}
	if(!present) {
		auto migrated = defaults;
		if(legacy_autofire) {
			migrated.autofire = 3U;
		}
		return NewClassicKeyConfigLoadResult{.config = std::move(migrated)};
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(store_error(
			NewClassicKeyConfigStoreErrorCode::open_failed,
			std::make_error_code(std::errc::io_error)
		));
	}
	std::array<std::byte, new_classic_key_config_file_bytes> bytes{};
	input.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
	if(input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
		input.peek() != std::ifstream::traits_type::eof()) {
		return NewClassicKeyConfigLoadResult{
			.config = defaults,
			.file_present = true,
			.recovered_defaults = true,
		};
	}
	auto decoded = decode_new_classic_key_config(bytes, maximum_host_key);
	if(!decoded) {
		return NewClassicKeyConfigLoadResult{
			.config = defaults,
			.file_present = true,
			.recovered_defaults = true,
		};
	}
	return NewClassicKeyConfigLoadResult{
		.config = std::move(*decoded),
		.file_present = true,
	};
}

std::expected<void, NewClassicKeyConfigStoreError>
store_new_classic_key_config(
	const std::filesystem::path &path,
	const NewClassicKeyConfig &config,
	std::uint16_t maximum_host_key
)
{
	if(path.empty() || !path.has_filename()) {
		return std::unexpected(store_error(
			NewClassicKeyConfigStoreErrorCode::invalid_path
		));
	}
	auto encoded = encode_new_classic_key_config(config, maximum_host_key);
	if(!encoded) {
		return std::unexpected(store_error(
			NewClassicKeyConfigStoreErrorCode::codec, {}, encoded.error()
		));
	}
	std::error_code directory_error;
	if(!path.parent_path().empty()) {
		std::filesystem::create_directories(path.parent_path(), directory_error);
	}
	if(directory_error) {
		return std::unexpected(store_error(
			NewClassicKeyConfigStoreErrorCode::open_failed, directory_error
		));
	}
	const auto temporary = temporary_path(path);
	{
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		if(!output) {
			return std::unexpected(store_error(
				NewClassicKeyConfigStoreErrorCode::open_failed,
				std::make_error_code(std::errc::io_error)
			));
		}
		output.write(
			reinterpret_cast<const char *>(encoded->data()),
			static_cast<std::streamsize>(encoded->size())
		);
		output.flush();
		if(!output) {
			std::filesystem::remove(temporary, directory_error);
			return std::unexpected(store_error(
				NewClassicKeyConfigStoreErrorCode::write_failed,
				std::make_error_code(std::errc::io_error)
			));
		}
	}
#if defined(_WIN32)
	std::error_code publish_error;
	if(MoveFileExW(
		temporary.c_str(), path.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
	) == 0) {
		publish_error = {
			static_cast<int>(GetLastError()), std::system_category()
		};
	}
#else
	std::error_code publish_error;
	std::filesystem::rename(temporary, path, publish_error);
#endif
	if(publish_error) {
		std::filesystem::remove(temporary, directory_error);
		return std::unexpected(store_error(
			NewClassicKeyConfigStoreErrorCode::publish_failed, publish_error
		));
	}
	auto readback = load_new_classic_key_config(path, config, maximum_host_key);
	if(!readback || readback->recovered_defaults || readback->config != config) {
		return std::unexpected(store_error(
			NewClassicKeyConfigStoreErrorCode::readback_failed
		));
	}
	return {};
}

} // namespace th03::app
