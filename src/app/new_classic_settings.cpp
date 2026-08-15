#include "th03/app/new_classic_settings.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace th03::app {
namespace {

constexpr std::array<std::byte, 8U> magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'N'}, std::byte{'S'},
	std::byte{'E'}, std::byte{'T'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::size_t digest_offset = new_classic_settings_file_bytes - 4U;
constexpr std::uint8_t known_flags = 0x0FU;

[[nodiscard]] constexpr NewClassicSettingsError error(
	NewClassicSettingsErrorCode code,
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
	std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(
		byte(bytes[offset]) |
		(static_cast<std::uint16_t>(byte(bytes[offset + 1U])) << 8U)
	);
}

[[nodiscard]] constexpr std::uint32_t u32(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return static_cast<std::uint32_t>(byte(bytes[offset])) |
		(static_cast<std::uint32_t>(byte(bytes[offset + 1U])) << 8U) |
		(static_cast<std::uint32_t>(byte(bytes[offset + 2U])) << 16U) |
		(static_cast<std::uint32_t>(byte(bytes[offset + 3U])) << 24U);
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
		bytes[offset + index] = static_cast<std::byte>(
			value >> (index * 8U)
		);
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

} // namespace

std::expected<void, NewClassicSettingsError> validate_new_classic_settings(
	const NewClassicSettings &settings
) noexcept
{
	if(static_cast<std::uint8_t>(settings.options.bgm_mode) >
		static_cast<std::uint8_t>(ClassicBgmMode::midi)) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::invalid_bgm_mode, 12U
		));
	}
	if(static_cast<std::uint8_t>(settings.options.key_mode) >
		static_cast<std::uint8_t>(ClassicKeyMode::key_joy)) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::invalid_key_mode, 13U
		));
	}
	if(static_cast<std::uint8_t>(settings.options.rank) >
		static_cast<std::uint8_t>(ClassicRank::lunatic)) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::invalid_rank, 14U
		));
	}
	if(settings.master_volume_percent > 100U ||
		(settings.master_volume_percent % 5U) != 0U) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::invalid_master_volume, 16U
		));
	}
	if(static_cast<std::uint8_t>(settings.window.mode) >
		static_cast<std::uint8_t>(
			NewClassicWindowMode::borderless_fullscreen
		)) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::invalid_window_mode, 17U
		));
	}
	if(static_cast<std::uint8_t>(settings.window.scale) >
		static_cast<std::uint8_t>(NewClassicWindowScale::automatic)) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::invalid_window_scale, 18U
		));
	}
	if(static_cast<std::uint8_t>(settings.window.aspect) >
		static_cast<std::uint8_t>(NewClassicAspectRatio::display_4_3)) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::invalid_aspect_ratio, 19U
		));
	}
	if(static_cast<std::uint8_t>(settings.window.filtering) >
		static_cast<std::uint8_t>(NewClassicFiltering::linear)) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::invalid_filtering, 19U
		));
	}
	return {};
}

std::expected<NewClassicSettings, NewClassicSettingsError>
decode_new_classic_settings(std::span<const std::byte> bytes) noexcept
{
	if(bytes.size() != new_classic_settings_file_bytes) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::input_size, bytes.size()
		));
	}
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		if(bytes[index] != magic[index]) {
			return std::unexpected(error(
				NewClassicSettingsErrorCode::magic, index
			));
		}
	}
	if(u16(bytes, 8U) != new_classic_settings_format_version) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::version, 8U
		));
	}
	if(u16(bytes, 10U) != new_classic_settings_file_bytes) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::declared_size, 10U
		));
	}
	const auto flags = byte(bytes[15U]);
	if((flags & static_cast<std::uint8_t>(~known_flags)) != 0U) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::invalid_flags, 15U
		));
	}
	if((byte(bytes[19U]) & 0xF0U) != 0U) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::reserved_nonzero, 19U
		));
	}
	if(u32(bytes, digest_offset) != fnv1a32(bytes.first(digest_offset))) {
		return std::unexpected(error(
			NewClassicSettingsErrorCode::digest, digest_offset
		));
	}
	NewClassicSettings settings{
		.options = {
			.bgm_mode = static_cast<ClassicBgmMode>(byte(bytes[12U])),
			.key_mode = static_cast<ClassicKeyMode>(byte(bytes[13U])),
			.rank = static_cast<ClassicRank>(byte(bytes[14U])),
			.sfx_enabled = (flags & 0x01U) != 0U,
			.legacy_autofire = (flags & 0x02U) != 0U,
			.language = (flags & 0x04U) != 0U ?
				ClassicLanguage::english : ClassicLanguage::japanese,
			.reduced_flash = (flags & 0x08U) != 0U,
		},
		.master_volume_percent = byte(bytes[16U]),
		.window = {
			.mode = static_cast<NewClassicWindowMode>(byte(bytes[17U])),
			.scale = static_cast<NewClassicWindowScale>(byte(bytes[18U])),
			.aspect = static_cast<NewClassicAspectRatio>(
				byte(bytes[19U]) & 0x03U
			),
			.filtering = static_cast<NewClassicFiltering>(
				(byte(bytes[19U]) >> 2U) & 0x03U
			),
		},
	};
	if(auto valid = validate_new_classic_settings(settings); !valid) {
		return std::unexpected(valid.error());
	}
	return settings;
}

std::expected<
	std::array<std::byte, new_classic_settings_file_bytes>,
	NewClassicSettingsError
> encode_new_classic_settings(const NewClassicSettings &settings) noexcept
{
	if(auto valid = validate_new_classic_settings(settings); !valid) {
		return std::unexpected(valid.error());
	}
	std::array<std::byte, new_classic_settings_file_bytes> bytes{};
	std::copy(magic.begin(), magic.end(), bytes.begin());
	put_u16(bytes, 8U, new_classic_settings_format_version);
	put_u16(bytes, 10U, new_classic_settings_file_bytes);
	bytes[12U] = static_cast<std::byte>(settings.options.bgm_mode);
	bytes[13U] = static_cast<std::byte>(settings.options.key_mode);
	bytes[14U] = static_cast<std::byte>(settings.options.rank);
	bytes[15U] = static_cast<std::byte>(
		(settings.options.sfx_enabled ? 0x01U : 0U) |
		(settings.options.legacy_autofire ? 0x02U : 0U) |
		(settings.options.language == ClassicLanguage::english ? 0x04U : 0U) |
		(settings.options.reduced_flash ? 0x08U : 0U)
	);
	bytes[16U] = static_cast<std::byte>(settings.master_volume_percent);
	bytes[17U] = static_cast<std::byte>(settings.window.mode);
	bytes[18U] = static_cast<std::byte>(settings.window.scale);
	bytes[19U] = static_cast<std::byte>(
		static_cast<std::uint8_t>(settings.window.aspect) |
		(static_cast<std::uint8_t>(settings.window.filtering) << 2U)
	);
	put_u32(bytes, digest_offset, fnv1a32(
		std::span<const std::byte>(bytes).first(digest_offset)
	));
	return bytes;
}

} // namespace th03::app
