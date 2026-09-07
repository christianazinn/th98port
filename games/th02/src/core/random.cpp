#include "th02/core/random.hpp"
#include "pc98/random.hpp"

#include <cstddef>

namespace th02::core {

void randring_fill(RandomState &state) noexcept
{
	for(auto &value : state.ring) {
		value = static_cast<std::uint8_t>(pc98::irand(state.seed));
	}
}

std::uint8_t randring_next8(RandomState &state) noexcept
{
	const auto index = state.cursor;
	state.cursor = static_cast<std::uint8_t>(state.cursor + 1U);
	return state.ring[index];
}

std::uint16_t randring_next16(RandomState &state) noexcept
{
	const auto index = state.cursor;
	state.cursor = static_cast<std::uint8_t>(state.cursor + 1U);
	// th02/math/randring[bss].asm places the cursor immediately after the
	// ring. At index 255 the word read observes the now-wrapped cursor, not
	// ring[0]. One word read consumes one cursor position, not two.
	const auto high = (index == 255U) ? state.cursor
		: state.ring[static_cast<std::size_t>(index) + 1U];
	return static_cast<std::uint16_t>(static_cast<std::uint16_t>(state.ring[index]) |
		(static_cast<std::uint16_t>(high) << 8U));
}

} // namespace th02::core
