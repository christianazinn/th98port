#ifndef TH03_APP_NEW_CLASSIC_KEY_CONFIG_STORE_HPP
#define TH03_APP_NEW_CLASSIC_KEY_CONFIG_STORE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <system_error>

namespace th03::app {

inline constexpr std::size_t new_classic_player_action_count = 11U;
inline constexpr std::size_t new_classic_story_action_count = 4U;
inline constexpr std::size_t new_classic_key_binding_count =
	(2U * new_classic_player_action_count) + new_classic_story_action_count;
inline constexpr std::size_t new_classic_key_config_file_bytes = 70U;
inline constexpr std::uint16_t new_classic_host_key_unbound = 0xFFFFU;

enum class NewClassicPlayerAction : std::uint8_t {
	up_left,
	up,
	up_right,
	left,
	right,
	down_left,
	down,
	down_right,
	shot,
	bomb,
	charge,
};

enum class NewClassicStoryAction : std::uint8_t {
	up,
	left,
	right,
	down,
};

struct NewClassicKeyConfig final {
	std::uint8_t autofire = 0U;
	std::array<std::uint16_t, new_classic_key_binding_count> bindings{};

	friend constexpr bool operator==(
		const NewClassicKeyConfig &,
		const NewClassicKeyConfig &
	) = default;
};

enum class NewClassicKeyConfigCodecErrorCode : std::uint8_t {
	input_size,
	magic,
	version,
	declared_size,
	autofire,
	binding,
	duplicate_binding,
	digest,
};

struct NewClassicKeyConfigCodecError final {
	NewClassicKeyConfigCodecErrorCode code =
		NewClassicKeyConfigCodecErrorCode::input_size;
	std::size_t offset = 0U;
};

[[nodiscard]] std::expected<void, NewClassicKeyConfigCodecError>
validate_new_classic_key_config(
	const NewClassicKeyConfig &config,
	std::uint16_t maximum_host_key
) noexcept;

[[nodiscard]] std::expected<
	NewClassicKeyConfig,
	NewClassicKeyConfigCodecError
> decode_new_classic_key_config(
	std::span<const std::byte> bytes,
	std::uint16_t maximum_host_key
) noexcept;

[[nodiscard]] std::expected<
	std::array<std::byte, new_classic_key_config_file_bytes>,
	NewClassicKeyConfigCodecError
> encode_new_classic_key_config(
	const NewClassicKeyConfig &config,
	std::uint16_t maximum_host_key
) noexcept;

enum class NewClassicKeyConfigStoreErrorCode : std::uint8_t {
	invalid_path,
	invalid_defaults,
	open_failed,
	read_failed,
	write_failed,
	flush_failed,
	publish_failed,
	readback_failed,
	codec,
};

struct NewClassicKeyConfigStoreError final {
	NewClassicKeyConfigStoreErrorCode code =
		NewClassicKeyConfigStoreErrorCode::invalid_path;
	std::error_code system{};
	NewClassicKeyConfigCodecError codec{};
};

struct NewClassicKeyConfigLoadResult final {
	NewClassicKeyConfig config{};
	bool file_present = false;
	bool recovered_defaults = false;
};

[[nodiscard]] std::expected<
	NewClassicKeyConfigLoadResult,
	NewClassicKeyConfigStoreError
> load_new_classic_key_config(
	const std::filesystem::path &path,
	const NewClassicKeyConfig &defaults,
	std::uint16_t maximum_host_key
);

[[nodiscard]] std::expected<void, NewClassicKeyConfigStoreError>
store_new_classic_key_config(
	const std::filesystem::path &path,
	const NewClassicKeyConfig &config,
	std::uint16_t maximum_host_key
);

} // namespace th03::app

#endif
