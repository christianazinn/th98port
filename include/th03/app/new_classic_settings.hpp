#ifndef TH03_APP_NEW_CLASSIC_SETTINGS_HPP
#define TH03_APP_NEW_CLASSIC_SETTINGS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "th03/app/classic_config.hpp"

namespace th03::app {

inline constexpr std::size_t new_classic_settings_file_bytes = 28U;
inline constexpr std::size_t new_classic_settings_v1_file_bytes = 24U;
inline constexpr std::uint16_t new_classic_settings_format_version = 3U;

enum class NewClassicWindowMode : std::uint8_t {
	windowed,
	borderless_fullscreen,
};

enum class NewClassicWindowScale : std::uint8_t {
	x1,
	x2,
	x3,
	automatic,
};

enum class NewClassicAspectRatio : std::uint8_t {
	pixels_8_5,
	display_4_3,
};

enum class NewClassicFiltering : std::uint8_t {
	nearest,
	linear,
};

struct NewClassicWindowPresentation final {
	NewClassicWindowMode mode = NewClassicWindowMode::windowed;
	NewClassicWindowScale scale = NewClassicWindowScale::x1;
	NewClassicAspectRatio aspect = NewClassicAspectRatio::pixels_8_5;
	NewClassicFiltering filtering = NewClassicFiltering::nearest;

	friend constexpr bool operator==(
		const NewClassicWindowPresentation &,
		const NewClassicWindowPresentation &
	) = default;
};

struct NewClassicSettings final {
	ClassicConfigOptions options{};
	std::uint8_t master_volume_percent = 100U;
	std::uint8_t music_volume_percent = 100U;
	std::uint8_t effect_volume_percent = 100U;
	bool mute_when_unfocused = false;
	NewClassicWindowPresentation window{};

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
	invalid_music_volume,
	invalid_effect_volume,
	invalid_window_mode,
	invalid_window_scale,
	invalid_aspect_ratio,
	invalid_filtering,
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
	return {
		.options = options,
		.music_volume_percent = static_cast<std::uint8_t>(
			options.bgm_mode == ClassicBgmMode::off ? 0U : 100U
		),
		.effect_volume_percent = static_cast<std::uint8_t>(
			options.sfx_enabled ? 100U : 0U
		),
	};
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
