#ifndef TH03_APP_UPDATE_PACKAGE_HPP
#define TH03_APP_UPDATE_PACKAGE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace th03::app {

inline constexpr std::size_t update_package_header_bytes = 32U;
inline constexpr std::size_t update_package_entry_bytes = 48U;
inline constexpr std::size_t update_package_max_bytes =
	256U * 1024U * 1024U;
inline constexpr std::size_t update_package_max_files = 512U;
inline constexpr std::size_t update_package_max_path_bytes = 240U;

struct UpdatePackageEntry final {
	std::string path{};
	std::uint64_t payload_offset = 0U;
	std::uint64_t size = 0U;
	std::array<std::uint8_t, 32U> sha256{};
	bool executable = false;

	friend bool operator==(
		const UpdatePackageEntry &,
		const UpdatePackageEntry &
	) = default;
};

struct UpdatePackage final {
	std::vector<UpdatePackageEntry> entries{};
	std::uint64_t payload_offset = 0U;

	friend bool operator==(const UpdatePackage &, const UpdatePackage &) =
		default;
};

enum class UpdatePackageErrorCode : std::uint8_t {
	too_short,
	too_large,
	invalid_magic,
	unsupported_version,
	invalid_header_size,
	invalid_file_count,
	invalid_size,
	unsupported_flags,
	nonzero_reserved,
	invalid_path_length,
	invalid_path,
	unsorted_path,
	duplicate_path,
	invalid_payload_size,
};

struct UpdatePackageError final {
	UpdatePackageErrorCode code = UpdatePackageErrorCode::too_short;
	std::size_t offset = 0U;

	friend constexpr bool operator==(
		const UpdatePackageError &,
		const UpdatePackageError &
	) = default;
};

// Parses one exact T3PKG001 byte span. Member paths are portable relative
// paths; payload offsets always refer back to the supplied complete span.
// Parsing performs no hashing, filesystem mutation, or update authorization.
[[nodiscard]] std::expected<UpdatePackage, UpdatePackageError>
parse_update_package(std::span<const std::byte> bytes);

} // namespace th03::app

#endif
