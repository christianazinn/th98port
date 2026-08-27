#include "th03/app/classic/persistence/classic_config.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace th03::app {
namespace {

[[nodiscard]] constexpr ClassicConfigError config_error(
	ClassicConfigErrorCode code,
	std::size_t offset
) noexcept
{
	return {.code = code, .offset = offset};
}

[[nodiscard]] constexpr std::uint8_t byte_value(std::byte value) noexcept
{
	return std::to_integer<std::uint8_t>(value);
}

[[nodiscard]] constexpr std::uint16_t little_u16(
	std::byte low,
	std::byte high
) noexcept
{
	return static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(byte_value(low)) |
		static_cast<std::uint16_t>(
			static_cast<std::uint16_t>(byte_value(high)) << 8U
		)
	);
}

[[nodiscard]] constexpr std::expected<void, ClassicConfigError>
validate(ClassicConfigOptions options) noexcept
{
	if(static_cast<std::uint8_t>(options.bgm_mode) >
		static_cast<std::uint8_t>(ClassicBgmMode::midi)) {
		return std::unexpected(config_error(
			ClassicConfigErrorCode::invalid_bgm_mode, 0U
		));
	}
	if(static_cast<std::uint8_t>(options.key_mode) >
		static_cast<std::uint8_t>(ClassicKeyMode::key_joy)) {
		return std::unexpected(config_error(
			ClassicConfigErrorCode::invalid_key_mode, 1U
		));
	}
	if(static_cast<std::uint8_t>(options.rank) >
		static_cast<std::uint8_t>(ClassicRank::lunatic)) {
		return std::unexpected(config_error(
			ClassicConfigErrorCode::invalid_rank, 2U
		));
	}
	if(static_cast<std::uint8_t>(options.language) >
		static_cast<std::uint8_t>(ClassicLanguage::spanish_test)) {
		return std::unexpected(config_error(
			ClassicConfigErrorCode::invalid_language, 4U
		));
	}
	return {};
}

} // namespace

std::expected<ClassicConfigDocument, ClassicConfigError>
decode_classic_config_file(std::span<const std::byte> bytes) noexcept
{
	if(bytes.size() != classic_config_file_bytes) {
		return std::unexpected(config_error(
			ClassicConfigErrorCode::input_size, bytes.size()
		));
	}
	ClassicConfigDocument document{
		.options = {
			.bgm_mode = static_cast<ClassicBgmMode>(
				byte_value(bytes[0U]) & 0x7FU
			),
			.key_mode = static_cast<ClassicKeyMode>(byte_value(bytes[1U])),
			.rank = static_cast<ClassicRank>(byte_value(bytes[2U])),
			.sfx_enabled = (byte_value(bytes[0U]) & 0x80U) == 0U,
			.legacy_autofire = byte_value(bytes[3U]) != 0U,
			.language = static_cast<ClassicLanguage>(
				byte_value(bytes[4U]) & 0x7FU
			),
			.reduced_flash = (byte_value(bytes[4U]) & 0x80U) != 0U,
		},
		.metadata = {
			.resident_segment = little_u16(bytes[5U], bytes[6U]),
			.debug = byte_value(bytes[7U]),
		},
	};
	if(byte_value(bytes[3U]) > 1U) {
		return std::unexpected(config_error(
			ClassicConfigErrorCode::invalid_autofire, 3U
		));
	}
	const auto valid = validate(document.options);
	if(!valid) {
		return std::unexpected(valid.error());
	}
	return document;
}

std::expected<
	std::array<std::byte, classic_config_file_bytes>,
	ClassicConfigError
> encode_classic_config_file(const ClassicConfigDocument &document) noexcept
{
	const auto valid = validate(document.options);
	if(!valid) {
		return std::unexpected(valid.error());
	}
	auto bytes = std::array<std::byte, classic_config_file_bytes>{};
	bytes[0U] = static_cast<std::byte>(
		static_cast<std::uint8_t>(document.options.bgm_mode) |
		(document.options.sfx_enabled ? 0U : 0x80U)
	);
	bytes[1U] = static_cast<std::byte>(document.options.key_mode);
	bytes[2U] = static_cast<std::byte>(document.options.rank);
	bytes[3U] = static_cast<std::byte>(document.options.legacy_autofire ? 1U : 0U);
	bytes[4U] = static_cast<std::byte>(
		static_cast<std::uint8_t>(document.options.language) |
		(document.options.reduced_flash ? 0x80U : 0U)
	);
	bytes[5U] = static_cast<std::byte>(
		document.metadata.resident_segment & 0xFFU
	);
	bytes[6U] = static_cast<std::byte>(
		document.metadata.resident_segment >> 8U
	);
	bytes[7U] = static_cast<std::byte>(document.metadata.debug);
	return bytes;
}

} // namespace th03::app
