#include "th03/app/update_package.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

namespace th03::app {
namespace {

constexpr std::array<std::byte, 8U> package_magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'P'}, std::byte{'K'},
	std::byte{'G'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::uint16_t package_version = 1U;
constexpr std::uint16_t executable_flag = 0x0001U;

[[nodiscard]] constexpr std::uint16_t u16(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(
		std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U)
	);
}

[[nodiscard]] constexpr std::uint32_t u32(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	std::uint32_t result = 0U;
	for(std::size_t index = 0U; index < 4U; ++index) {
		result |= static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + index])
		) << (index * 8U);
	}
	return result;
}

[[nodiscard]] constexpr std::uint64_t u64(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	std::uint64_t result = 0U;
	for(std::size_t index = 0U; index < 8U; ++index) {
		result |= static_cast<std::uint64_t>(
			std::to_integer<std::uint8_t>(bytes[offset + index])
		) << (index * 8U);
	}
	return result;
}

[[nodiscard]] UpdatePackageError error(
	UpdatePackageErrorCode code,
	std::size_t offset
) noexcept
{
	return {code, offset};
}

[[nodiscard]] char ascii_lower(char value) noexcept
{
	return value >= 'A' && value <= 'Z' ?
		static_cast<char>(value + ('a' - 'A')) : value;
}

[[nodiscard]] bool reserved_windows_component(std::string_view component)
{
	const auto dot = component.find('.');
	auto stem = component.substr(0U, dot);
	std::string lower;
	lower.reserve(stem.size());
	std::ranges::transform(stem, std::back_inserter(lower), ascii_lower);
	if(lower == "con" || lower == "prn" || lower == "aux" ||
		lower == "nul") {
		return true;
	}
	if(lower.size() == 4U && lower[3U] >= '1' && lower[3U] <= '9') {
		return lower.starts_with("com") || lower.starts_with("lpt");
	}
	return false;
}

[[nodiscard]] bool valid_component(std::string_view component)
{
	if(component.empty() || component == "." || component == ".." ||
		component.back() == '.' || component.back() == ' ' ||
		reserved_windows_component(component)) {
		return false;
	}
	return std::ranges::all_of(component, [](char character) {
		const auto byte = static_cast<unsigned char>(character);
		return byte >= 0x21U && byte <= 0x7EU && character != '\\' &&
			character != ':' && character != '*' && character != '?' &&
			character != '"' && character != '<' && character != '>' &&
			character != '|';
	});
}

[[nodiscard]] bool valid_path(std::string_view path)
{
	if(path.empty() || path.front() == '/' || path.back() == '/') {
		return false;
	}
	std::size_t begin = 0U;
	while(begin < path.size()) {
		const auto end = path.find('/', begin);
		const auto component = path.substr(
			begin,
			end == std::string_view::npos ? path.size() - begin : end - begin
		);
		if(!valid_component(component)) {
			return false;
		}
		if(end == std::string_view::npos) {
			break;
		}
		begin = end + 1U;
	}
	return true;
}

[[nodiscard]] std::string path_key(std::string_view path)
{
	std::string result;
	result.reserve(path.size());
	std::ranges::transform(path, std::back_inserter(result), ascii_lower);
	return result;
}

} // namespace

std::expected<UpdatePackage, UpdatePackageError> parse_update_package(
	std::span<const std::byte> bytes
)
{
	if(bytes.size() < update_package_header_bytes) {
		return std::unexpected(error(
			UpdatePackageErrorCode::too_short, bytes.size()
		));
	}
	if(bytes.size() > update_package_max_bytes) {
		return std::unexpected(error(
			UpdatePackageErrorCode::too_large, bytes.size()
		));
	}
	if(!std::ranges::equal(package_magic, bytes.first(package_magic.size()))) {
		return std::unexpected(error(UpdatePackageErrorCode::invalid_magic, 0U));
	}
	if(u16(bytes, 8U) != package_version) {
		return std::unexpected(error(
			UpdatePackageErrorCode::unsupported_version, 8U
		));
	}
	if(u16(bytes, 10U) != update_package_header_bytes) {
		return std::unexpected(error(
			UpdatePackageErrorCode::invalid_header_size, 10U
		));
	}
	const auto file_count = static_cast<std::size_t>(u32(bytes, 12U));
	if(file_count == 0U || file_count > update_package_max_files) {
		return std::unexpected(error(
			UpdatePackageErrorCode::invalid_file_count, 12U
		));
	}
	const auto table_size = u64(bytes, 16U);
	const auto payload_size = u64(bytes, 24U);
	if(table_size > bytes.size() - update_package_header_bytes ||
		payload_size > bytes.size() - update_package_header_bytes - table_size ||
		update_package_header_bytes + table_size + payload_size != bytes.size()) {
		return std::unexpected(error(UpdatePackageErrorCode::invalid_size, 16U));
	}

	UpdatePackage result{};
	result.entries.reserve(file_count);
	result.payload_offset = update_package_header_bytes + table_size;
	std::unordered_set<std::string> path_keys;
	path_keys.reserve(file_count);
	std::size_t cursor = update_package_header_bytes;
	std::uint64_t member_payload_offset = result.payload_offset;
	std::uint64_t members_size = 0U;
	std::string previous_path;
	for(std::size_t index = 0U; index < file_count; ++index) {
		const auto table_end = result.payload_offset;
		if(cursor > table_end || table_end - cursor < update_package_entry_bytes) {
			return std::unexpected(error(
				UpdatePackageErrorCode::invalid_size, cursor
			));
		}
		const auto path_size = static_cast<std::size_t>(u16(bytes, cursor));
		const auto flags = u16(bytes, cursor + 2U);
		if((flags & ~executable_flag) != 0U) {
			return std::unexpected(error(
				UpdatePackageErrorCode::unsupported_flags, cursor + 2U
			));
		}
		if(u32(bytes, cursor + 4U) != 0U) {
			return std::unexpected(error(
				UpdatePackageErrorCode::nonzero_reserved, cursor + 4U
			));
		}
		const auto member_size = u64(bytes, cursor + 8U);
		if(path_size == 0U || path_size > update_package_max_path_bytes) {
			return std::unexpected(error(
				UpdatePackageErrorCode::invalid_path_length, cursor
			));
		}
		if(table_end - cursor - update_package_entry_bytes < path_size) {
			return std::unexpected(error(
				UpdatePackageErrorCode::invalid_size, cursor
			));
		}
		UpdatePackageEntry entry{};
		entry.size = member_size;
		entry.payload_offset = member_payload_offset;
		entry.executable = (flags & executable_flag) != 0U;
		std::ranges::transform(
			bytes.subspan(cursor + 16U, entry.sha256.size()),
			entry.sha256.begin(),
			[](std::byte value) { return std::to_integer<std::uint8_t>(value); }
		);
		const auto path_offset = cursor + update_package_entry_bytes;
		const auto path_bytes = bytes.subspan(path_offset, path_size);
		entry.path.assign(
			reinterpret_cast<const char *>(path_bytes.data()), path_bytes.size()
		);
		if(!valid_path(entry.path)) {
			return std::unexpected(error(
				UpdatePackageErrorCode::invalid_path, path_offset
			));
		}
		if(!previous_path.empty() && previous_path >= entry.path) {
			return std::unexpected(error(
				UpdatePackageErrorCode::unsorted_path, path_offset
			));
		}
		auto [unused, inserted] = path_keys.insert(path_key(entry.path));
		(void)unused;
		if(!inserted) {
			return std::unexpected(error(
				UpdatePackageErrorCode::duplicate_path, path_offset
			));
		}
		if(member_size > payload_size - members_size) {
			return std::unexpected(error(
				UpdatePackageErrorCode::invalid_payload_size, cursor + 8U
			));
		}
		members_size += member_size;
		member_payload_offset += member_size;
		previous_path = entry.path;
		result.entries.push_back(std::move(entry));
		cursor = path_offset + path_size;
	}
	if(cursor != result.payload_offset || members_size != payload_size) {
		return std::unexpected(error(
			UpdatePackageErrorCode::invalid_payload_size, cursor
		));
	}
	return result;
}

} // namespace th03::app
