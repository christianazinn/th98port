#pragma once

#include <cstdint>

namespace pc98 {

// ReC98 libs/master.lib/random.asm::IRAND. The seed retains all 32 bits;
// only the returned high word loses its sign bit.
[[nodiscard]] constexpr std::uint16_t irand(std::uint32_t &seed) noexcept
{
	seed = (seed * 0x015A4E35U) + 1U;
	return static_cast<std::uint16_t>((seed >> 16U) & 0x7FFFU);
}

} // namespace pc98
