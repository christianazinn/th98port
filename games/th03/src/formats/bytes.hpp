#ifndef TH03_FORMATS_BYTES_HPP
#define TH03_FORMATS_BYTES_HPP

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace th03::formats::detail {

inline std::uint8_t u8(std::span<const std::byte> bytes, std::size_t offset)
{
	return std::to_integer<std::uint8_t>(bytes[offset]);
}

inline std::int8_t i8(std::span<const std::byte> bytes, std::size_t offset)
{
	return std::bit_cast<std::int8_t>(u8(bytes, offset));
}

inline std::uint16_t u16(std::span<const std::byte> bytes, std::size_t offset)
{
	return static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(u8(bytes, offset)) |
		(static_cast<std::uint16_t>(u8(bytes, offset + 1U)) << 8U)
	);
}

inline std::uint32_t u32(std::span<const std::byte> bytes, std::size_t offset)
{
	return (
		static_cast<std::uint32_t>(u8(bytes, offset)) |
		(static_cast<std::uint32_t>(u8(bytes, offset + 1U)) << 8U) |
		(static_cast<std::uint32_t>(u8(bytes, offset + 2U)) << 16U) |
		(static_cast<std::uint32_t>(u8(bytes, offset + 3U)) << 24U)
	);
}

inline std::int32_t i32(std::span<const std::byte> bytes, std::size_t offset)
{
	return std::bit_cast<std::int32_t>(u32(bytes, offset));
}

inline void put_u8(std::span<std::byte> bytes, std::size_t offset, std::uint8_t value)
{
	bytes[offset] = static_cast<std::byte>(value);
}

inline void put_u16(std::span<std::byte> bytes, std::size_t offset, std::uint16_t value)
{
	put_u8(bytes, offset, static_cast<std::uint8_t>(value));
	put_u8(bytes, offset + 1U, static_cast<std::uint8_t>(value >> 8U));
}

inline void put_u32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value)
{
	put_u8(bytes, offset, static_cast<std::uint8_t>(value));
	put_u8(bytes, offset + 1U, static_cast<std::uint8_t>(value >> 8U));
	put_u8(bytes, offset + 2U, static_cast<std::uint8_t>(value >> 16U));
	put_u8(bytes, offset + 3U, static_cast<std::uint8_t>(value >> 24U));
}

inline void put_i32(std::span<std::byte> bytes, std::size_t offset, std::int32_t value)
{
	put_u32(bytes, offset, std::bit_cast<std::uint32_t>(value));
}

template <std::size_t Size>
bool has_magic(
	std::span<const std::byte> bytes,
	const std::array<std::uint8_t, Size> &magic
)
{
	if(bytes.size() < Size) {
		return false;
	}
	for(std::size_t index = 0; index < Size; ++index) {
		if(u8(bytes, index) != magic[index]) {
			return false;
		}
	}
	return true;
}

} // namespace th03::formats::detail

#endif
