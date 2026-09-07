#ifndef TH03_APP_NEW_CLASSIC_DEVICE_STORE_HPP
#define TH03_APP_NEW_CLASSIC_DEVICE_STORE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <system_error>

#include "th03/app/new_classic/input/new_classic_controller.hpp"

namespace th03::app {

inline constexpr std::size_t new_classic_device_file_bytes = 64U;

enum class NewClassicDeviceCodecErrorCode : std::uint8_t {
	input_size,
	magic,
	version,
	declared_size,
	mode,
	identity,
	duplicate_identity,
	reserved,
	digest,
};

struct NewClassicDeviceCodecError final {
	NewClassicDeviceCodecErrorCode code =
		NewClassicDeviceCodecErrorCode::input_size;
	std::size_t offset = 0U;
};

[[nodiscard]] std::expected<
	NewClassicDeviceAssignment,
	NewClassicDeviceCodecError
> decode_new_classic_device_assignment(std::span<const std::byte> bytes) noexcept;

[[nodiscard]] std::expected<
	std::array<std::byte, new_classic_device_file_bytes>,
	NewClassicDeviceCodecError
> encode_new_classic_device_assignment(
	const NewClassicDeviceAssignment &assignment
) noexcept;

enum class NewClassicDeviceStoreErrorCode : std::uint8_t {
	invalid_path,
	invalid_defaults,
	open_failed,
	write_failed,
	publish_failed,
	readback_failed,
	codec,
};

struct NewClassicDeviceStoreError final {
	NewClassicDeviceStoreErrorCode code =
		NewClassicDeviceStoreErrorCode::invalid_path;
	std::error_code system{};
	NewClassicDeviceCodecError codec{};
};

struct NewClassicDeviceLoadResult final {
	NewClassicDeviceAssignment assignment{};
	bool file_present = false;
	bool recovered_defaults = false;
};

[[nodiscard]] std::expected<
	NewClassicDeviceLoadResult,
	NewClassicDeviceStoreError
> load_new_classic_device_assignment(
	const std::filesystem::path &path,
	const NewClassicDeviceAssignment &defaults
);

[[nodiscard]] std::expected<void, NewClassicDeviceStoreError>
store_new_classic_device_assignment(
	const std::filesystem::path &path,
	const NewClassicDeviceAssignment &assignment
);

} // namespace th03::app

#endif
