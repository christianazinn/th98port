#ifndef TH03_APP_NEW_CLASSIC_SETTINGS_HPP
#define TH03_APP_NEW_CLASSIC_SETTINGS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "th03/app/classic_config.hpp"

namespace th03::app {

inline constexpr std::size_t new_classic_settings_file_bytes = 24U;
inline constexpr std::uint16_t new_classic_settings_format_version = 1U;

struct NewClassicSettings final {
	ClassicConfigOptions options{};
	std::uint8_t master_volume_percent = 100U;

	friend constexpr bool operator==(
		const NewClassicSettings &,
		const NewClassicSettings &
	) = default;
};

enum class NewClassicSettingsErrorCode : std::uint8_t {
	input_size,
	magic,
	version,
	declared_size,
	invalid_bgm_mode,
	invalid_key_mode,
	invalid_rank,
	invalid_flags,
	invalid_master_volume,
	reserved_nonzero,
	digest,
};

struct NewClassicSettingsError final {
	NewClassicSettingsErrorCode code =
		NewClassicSettingsErrorCode::input_size;
	std::size_t offset = 0U;
};

[[nodiscard]] constexpr NewClassicSettings
new_classic_settings_defaults() noexcept
{
	return {};
}

[[nodiscard]] constexpr NewClassicSettings
new_classic_settings_from_classic(ClassicConfigOptions options) noexcept
{
	return {.options = options};
}

[[nodiscard]] std::expected<void, NewClassicSettingsError>
validate_new_classic_settings(const NewClassicSettings &settings) noexcept;

[[nodiscard]] std::expected<
	NewClassicSettings,
	NewClassicSettingsError
> decode_new_classic_settings(std::span<const std::byte> bytes) noexcept;

[[nodiscard]] std::expected<
	std::array<std::byte, new_classic_settings_file_bytes>,
	NewClassicSettingsError
> encode_new_classic_settings(const NewClassicSettings &settings) noexcept;

} // namespace th03::app

#endif
