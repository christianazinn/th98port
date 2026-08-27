#include "th03/app/new_classic/input/new_classic_joy_config_store.hpp"

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
	std::byte{'T'}, std::byte{'3'}, std::byte{'N'}, std::byte{'J'},
	std::byte{'O'}, std::byte{'Y'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::uint16_t format_version = 1U;
constexpr std::size_t button_offset = 12U;
constexpr std::size_t horizontal_axis_offset = 17U;
constexpr std::size_t vertical_axis_offset = 18U;
constexpr std::size_t direction_offset = 19U;
constexpr std::size_t dead_zone_offset = 20U;
constexpr std::size_t reserved_offset = 21U;
constexpr std::size_t digest_offset = new_classic_joy_config_file_bytes - 4U;

[[nodiscard]] constexpr NewClassicJoyConfigCodecError codec_error(
	NewClassicJoyConfigCodecErrorCode code,
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
		byte(bytes[at]) |
		(static_cast<std::uint16_t>(byte(bytes[at + 1U])) << 8U)
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

[[nodiscard]] constexpr bool valid_button(NewClassicJoyButton button) noexcept
{
	return button == NewClassicJoyButton::unbound ||
		static_cast<std::uint8_t>(button) <=
			static_cast<std::uint8_t>(NewClassicJoyButton::misc);
}

[[nodiscard]] constexpr bool horizontal_axis(NewClassicJoyAxis axis) noexcept
{
	return axis == NewClassicJoyAxis::left_x ||
		axis == NewClassicJoyAxis::right_x;
}

[[nodiscard]] constexpr bool vertical_axis(NewClassicJoyAxis axis) noexcept
{
	return axis == NewClassicJoyAxis::left_y ||
		axis == NewClassicJoyAxis::right_y;
}

[[nodiscard]] NewClassicJoyConfigStoreError store_error(
	NewClassicJoyConfigStoreErrorCode code,
	std::error_code system = {},
	NewClassicJoyConfigCodecError codec = {}
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

std::expected<void, NewClassicJoyConfigCodecError>
validate_new_classic_joy_config(const NewClassicJoyConfig &config) noexcept
{
	for(std::size_t index = 0U; index < config.buttons.size(); ++index) {
		const auto button = config.buttons[index];
		if(!valid_button(button)) {
			return std::unexpected(codec_error(
				NewClassicJoyConfigCodecErrorCode::button,
				button_offset + index
			));
		}
		for(std::size_t earlier = 0U; earlier < index; ++earlier) {
			if(button != NewClassicJoyButton::unbound &&
				config.buttons[earlier] == button) {
				return std::unexpected(codec_error(
					NewClassicJoyConfigCodecErrorCode::duplicate_button,
					button_offset + index
				));
			}
		}
	}
	if(!horizontal_axis(config.horizontal_axis)) {
		return std::unexpected(codec_error(
			NewClassicJoyConfigCodecErrorCode::axis, horizontal_axis_offset
		));
	}
	if(!vertical_axis(config.vertical_axis)) {
		return std::unexpected(codec_error(
			NewClassicJoyConfigCodecErrorCode::axis, vertical_axis_offset
		));
	}
	if(config.dead_zone_percent < 5U || config.dead_zone_percent > 95U ||
		(config.dead_zone_percent % 5U) != 0U) {
		return std::unexpected(codec_error(
			NewClassicJoyConfigCodecErrorCode::dead_zone, dead_zone_offset
		));
	}
	return {};
}

std::expected<NewClassicJoyConfig, NewClassicJoyConfigCodecError>
decode_new_classic_joy_config(std::span<const std::byte> bytes) noexcept
{
	if(bytes.size() != new_classic_joy_config_file_bytes) {
		return std::unexpected(codec_error(
			NewClassicJoyConfigCodecErrorCode::input_size, bytes.size()
		));
	}
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		if(bytes[index] != magic[index]) {
			return std::unexpected(codec_error(
				NewClassicJoyConfigCodecErrorCode::magic, index
			));
		}
	}
	if(u16(bytes, 8U) != format_version) {
		return std::unexpected(codec_error(
			NewClassicJoyConfigCodecErrorCode::version, 8U
		));
	}
	if(u16(bytes, 10U) != new_classic_joy_config_file_bytes) {
		return std::unexpected(codec_error(
			NewClassicJoyConfigCodecErrorCode::declared_size, 10U
		));
	}
	if(u32(bytes, digest_offset) != fnv1a32(bytes.first(digest_offset))) {
		return std::unexpected(codec_error(
			NewClassicJoyConfigCodecErrorCode::digest, digest_offset
		));
	}
	for(std::size_t index = reserved_offset; index < digest_offset; ++index) {
		if(bytes[index] != std::byte{}) {
			return std::unexpected(codec_error(
				NewClassicJoyConfigCodecErrorCode::reserved, index
			));
		}
	}
	NewClassicJoyConfig result{};
	for(std::size_t index = 0U; index < result.buttons.size(); ++index) {
		result.buttons[index] = static_cast<NewClassicJoyButton>(
			byte(bytes[button_offset + index])
		);
	}
	result.horizontal_axis = static_cast<NewClassicJoyAxis>(
		byte(bytes[horizontal_axis_offset])
	);
	result.vertical_axis = static_cast<NewClassicJoyAxis>(
		byte(bytes[vertical_axis_offset])
	);
	const auto directions = byte(bytes[direction_offset]);
	if((directions & ~0x03U) != 0U) {
		return std::unexpected(codec_error(
			NewClassicJoyConfigCodecErrorCode::direction, direction_offset
		));
	}
	result.horizontal_reversed = (directions & 0x01U) != 0U;
	result.vertical_reversed = (directions & 0x02U) != 0U;
	result.dead_zone_percent = byte(bytes[dead_zone_offset]);
	if(auto valid = validate_new_classic_joy_config(result); !valid) {
		return std::unexpected(valid.error());
	}
	return result;
}

std::expected<
	std::array<std::byte, new_classic_joy_config_file_bytes>,
	NewClassicJoyConfigCodecError
> encode_new_classic_joy_config(const NewClassicJoyConfig &config) noexcept
{
	if(auto valid = validate_new_classic_joy_config(config); !valid) {
		return std::unexpected(valid.error());
	}
	std::array<std::byte, new_classic_joy_config_file_bytes> result{};
	std::copy(magic.begin(), magic.end(), result.begin());
	put_u16(result, 8U, format_version);
	put_u16(result, 10U, new_classic_joy_config_file_bytes);
	for(std::size_t index = 0U; index < config.buttons.size(); ++index) {
		result[button_offset + index] = static_cast<std::byte>(
			config.buttons[index]
		);
	}
	result[horizontal_axis_offset] = static_cast<std::byte>(
		config.horizontal_axis
	);
	result[vertical_axis_offset] = static_cast<std::byte>(config.vertical_axis);
	result[direction_offset] = static_cast<std::byte>(
		(config.horizontal_reversed ? 0x01U : 0U) |
		(config.vertical_reversed ? 0x02U : 0U)
	);
	result[dead_zone_offset] = static_cast<std::byte>(config.dead_zone_percent);
	put_u32(result, digest_offset, fnv1a32(
		std::span<const std::byte>(result).first(digest_offset)
	));
	return result;
}

std::expected<NewClassicJoyConfigLoadResult, NewClassicJoyConfigStoreError>
load_new_classic_joy_config(
	const std::filesystem::path &path,
	const NewClassicJoyConfig &defaults
)
{
	if(path.empty()) {
		return std::unexpected(store_error(
			NewClassicJoyConfigStoreErrorCode::invalid_path
		));
	}
	if(auto valid = validate_new_classic_joy_config(defaults); !valid) {
		return std::unexpected(store_error(
			NewClassicJoyConfigStoreErrorCode::invalid_defaults, {}, valid.error()
		));
	}
	std::error_code status_error;
	const bool present = std::filesystem::exists(path, status_error);
	if(status_error) {
		return std::unexpected(store_error(
			NewClassicJoyConfigStoreErrorCode::open_failed, status_error
		));
	}
	if(!present) {
		return NewClassicJoyConfigLoadResult{.config = defaults};
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(store_error(
			NewClassicJoyConfigStoreErrorCode::open_failed,
			std::make_error_code(std::errc::io_error)
		));
	}
	std::array<std::byte, new_classic_joy_config_file_bytes> bytes{};
	input.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
	if(input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
		input.peek() != std::ifstream::traits_type::eof()) {
		return NewClassicJoyConfigLoadResult{
			.config = defaults,
			.file_present = true,
			.recovered_defaults = true,
		};
	}
	auto decoded = decode_new_classic_joy_config(bytes);
	if(!decoded) {
		return NewClassicJoyConfigLoadResult{
			.config = defaults,
			.file_present = true,
			.recovered_defaults = true,
		};
	}
	return NewClassicJoyConfigLoadResult{
		.config = std::move(*decoded),
		.file_present = true,
	};
}

std::expected<void, NewClassicJoyConfigStoreError>
store_new_classic_joy_config(
	const std::filesystem::path &path,
	const NewClassicJoyConfig &config
)
{
	if(path.empty() || !path.has_filename()) {
		return std::unexpected(store_error(
			NewClassicJoyConfigStoreErrorCode::invalid_path
		));
	}
	auto encoded = encode_new_classic_joy_config(config);
	if(!encoded) {
		return std::unexpected(store_error(
			NewClassicJoyConfigStoreErrorCode::codec, {}, encoded.error()
		));
	}
	std::error_code directory_error;
	if(!path.parent_path().empty()) {
		std::filesystem::create_directories(path.parent_path(), directory_error);
	}
	if(directory_error) {
		return std::unexpected(store_error(
			NewClassicJoyConfigStoreErrorCode::open_failed, directory_error
		));
	}
	const auto temporary = temporary_path(path);
	{
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		if(!output) {
			return std::unexpected(store_error(
				NewClassicJoyConfigStoreErrorCode::open_failed,
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
				NewClassicJoyConfigStoreErrorCode::write_failed,
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
			NewClassicJoyConfigStoreErrorCode::publish_failed, publish_error
		));
	}
	auto readback = load_new_classic_joy_config(path, config);
	if(!readback || readback->recovered_defaults || readback->config != config) {
		return std::unexpected(store_error(
			NewClassicJoyConfigStoreErrorCode::readback_failed
		));
	}
	return {};
}

} // namespace th03::app
