#include "th03/app/update_generation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>

namespace th03::app {
namespace {

constexpr std::array<std::byte, 8U> marker_magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'G'}, std::byte{'E'},
	std::byte{'N'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::uint16_t marker_version = 1U;
constexpr std::size_t sequence_offset = 16U;
constexpr std::size_t digest_offset = 24U;
constexpr std::size_t reserved_offset = 56U;

void put_u16(
	std::span<std::byte> bytes,
	std::size_t offset,
	std::uint16_t value
) noexcept
{
	bytes[offset] = static_cast<std::byte>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void put_u64(
	std::span<std::byte> bytes,
	std::size_t offset,
	std::uint64_t value
) noexcept
{
	for(std::size_t index = 0U; index < 8U; ++index) {
		bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
	}
}

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

[[nodiscard]] UpdateGenerationMarkerError marker_error(
	UpdateGenerationMarkerErrorCode code,
	std::size_t offset
) noexcept
{
	return {.code = code, .offset = offset};
}

} // namespace

std::expected<
	std::array<std::byte, update_generation_marker_bytes>,
	UpdateGenerationMarkerError
> encode_update_generation_marker(
	const UpdateGenerationIdentity &identity
) noexcept
{
	if(!valid_update_generation_identity(identity)) {
		return std::unexpected(marker_error(
			UpdateGenerationMarkerErrorCode::identity, sequence_offset
		));
	}
	std::array<std::byte, update_generation_marker_bytes> result{};
	std::ranges::copy(marker_magic, result.begin());
	put_u16(result, 8U, marker_version);
	put_u16(
		result, 10U,
		static_cast<std::uint16_t>(update_generation_marker_bytes)
	);
	put_u64(result, sequence_offset, identity.sequence);
	std::ranges::transform(
		identity.artifact_sha256,
		result.begin() + static_cast<std::ptrdiff_t>(digest_offset),
		[](std::uint8_t value) { return static_cast<std::byte>(value); }
	);
	return result;
}

std::expected<UpdateGenerationIdentity, UpdateGenerationMarkerError>
decode_update_generation_marker(std::span<const std::byte> bytes) noexcept
{
	if(bytes.size() != update_generation_marker_bytes) {
		return std::unexpected(marker_error(
			UpdateGenerationMarkerErrorCode::input_size, bytes.size()
		));
	}
	if(!std::ranges::equal(marker_magic, bytes.first(marker_magic.size()))) {
		return std::unexpected(marker_error(
			UpdateGenerationMarkerErrorCode::magic, 0U
		));
	}
	if(u16(bytes, 8U) != marker_version) {
		return std::unexpected(marker_error(
			UpdateGenerationMarkerErrorCode::version, 8U
		));
	}
	if(u16(bytes, 10U) != update_generation_marker_bytes) {
		return std::unexpected(marker_error(
			UpdateGenerationMarkerErrorCode::declared_size, 10U
		));
	}
	if(std::ranges::any_of(
		bytes.subspan(12U, 4U), [](std::byte value) { return value != std::byte{}; }
	) || std::ranges::any_of(
		bytes.subspan(reserved_offset),
		[](std::byte value) { return value != std::byte{}; }
	)) {
		return std::unexpected(marker_error(
			UpdateGenerationMarkerErrorCode::reserved, 12U
		));
	}
	UpdateGenerationIdentity result{.sequence = u64(bytes, sequence_offset)};
	std::ranges::transform(
		bytes.subspan(digest_offset, result.artifact_sha256.size()),
		result.artifact_sha256.begin(),
		[](std::byte value) { return std::to_integer<std::uint8_t>(value); }
	);
	if(!valid_update_generation_identity(result)) {
		return std::unexpected(marker_error(
			UpdateGenerationMarkerErrorCode::identity, sequence_offset
		));
	}
	return result;
}

} // namespace th03::app
