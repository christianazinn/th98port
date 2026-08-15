#ifndef TH03_APP_NEW_CLASSIC_JOY_CONFIG_STORE_HPP
#define TH03_APP_NEW_CLASSIC_JOY_CONFIG_STORE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <system_error>

namespace th03::app {

inline constexpr std::size_t new_classic_joy_action_count = 5U;
inline constexpr std::size_t new_classic_joy_config_file_bytes = 32U;

// Stable host-neutral names for SDL's standardized gamepad positions. The
// persistence document does not store SDL enum values or device instance IDs.
enum class NewClassicJoyButton : std::uint8_t {
	south,
	east,
	west,
	north,
	back,
	start,
	left_stick,
	right_stick,
	left_shoulder,
	right_shoulder,
	dpad_up,
	dpad_down,
	dpad_left,
	dpad_right,
	misc,
	unbound = 0xFFU,
};

enum class NewClassicJoyAxis : std::uint8_t {
	left_x,
	left_y,
	right_x,
	right_y,
};

enum class NewClassicJoyAction : std::uint8_t {
	shot,
	bomb,
	charge,
	cancel,
	ok,
};

struct NewClassicJoyConfig final {
	std::array<NewClassicJoyButton, new_classic_joy_action_count> buttons{
		NewClassicJoyButton::south,
		NewClassicJoyButton::east,
		NewClassicJoyButton::west,
		NewClassicJoyButton::back,
		NewClassicJoyButton::start,
	};
	NewClassicJoyAxis horizontal_axis = NewClassicJoyAxis::left_x;
	NewClassicJoyAxis vertical_axis = NewClassicJoyAxis::left_y;
	bool horizontal_reversed = false;
	bool vertical_reversed = false;
	// Activation threshold in 5% steps. The SDL adapter derives a 3/4 release
	// threshold, preserving the existing 50%/37.5% hysteresis by default.
	std::uint8_t dead_zone_percent = 50U;

	friend constexpr bool operator==(
		const NewClassicJoyConfig &,
		const NewClassicJoyConfig &
	) = default;
};

enum class NewClassicJoyConfigCodecErrorCode : std::uint8_t {
	input_size,
	magic,
	version,
	declared_size,
	button,
	duplicate_button,
	axis,
	direction,
	dead_zone,
	reserved,
	digest,
};

struct NewClassicJoyConfigCodecError final {
	NewClassicJoyConfigCodecErrorCode code =
		NewClassicJoyConfigCodecErrorCode::input_size;
	std::size_t offset = 0U;
};

[[nodiscard]] std::expected<void, NewClassicJoyConfigCodecError>
validate_new_classic_joy_config(const NewClassicJoyConfig &config) noexcept;

[[nodiscard]] std::expected<
	NewClassicJoyConfig,
	NewClassicJoyConfigCodecError
> decode_new_classic_joy_config(std::span<const std::byte> bytes) noexcept;

[[nodiscard]] std::expected<
	std::array<std::byte, new_classic_joy_config_file_bytes>,
	NewClassicJoyConfigCodecError
> encode_new_classic_joy_config(const NewClassicJoyConfig &config) noexcept;

enum class NewClassicJoyConfigStoreErrorCode : std::uint8_t {
	invalid_path,
	invalid_defaults,
	open_failed,
	write_failed,
	publish_failed,
	readback_failed,
	codec,
};

struct NewClassicJoyConfigStoreError final {
	NewClassicJoyConfigStoreErrorCode code =
		NewClassicJoyConfigStoreErrorCode::invalid_path;
	std::error_code system{};
	NewClassicJoyConfigCodecError codec{};
};

struct NewClassicJoyConfigLoadResult final {
	NewClassicJoyConfig config{};
	bool file_present = false;
	bool recovered_defaults = false;
};

[[nodiscard]] std::expected<
	NewClassicJoyConfigLoadResult,
	NewClassicJoyConfigStoreError
> load_new_classic_joy_config(
	const std::filesystem::path &path,
	const NewClassicJoyConfig &defaults
);

[[nodiscard]] std::expected<void, NewClassicJoyConfigStoreError>
store_new_classic_joy_config(
	const std::filesystem::path &path,
	const NewClassicJoyConfig &config
);

} // namespace th03::app

#endif
