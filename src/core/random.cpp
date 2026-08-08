#include "game_internal.hpp"

#include <cstddef>

namespace th03::detail {

std::uint16_t irand(std::uint32_t &seed) noexcept
{
	seed = (seed * 0x015A4E35U) + 1U;
	return static_cast<std::uint16_t>((seed >> 16U) & 0x7FFFU);
}

void randring_fill(DeterministicState &state) noexcept
{
	for(std::size_t index = state.randring.size(); index-- > 0U;) {
		state.randring[index] = static_cast<std::uint8_t>(irand(state.random_seed));
	}
}

std::uint8_t randring_next8(DeterministicState &state) noexcept
{
	const auto index = state.randring_cursor;
	state.randring_cursor = static_cast<std::uint8_t>(state.randring_cursor + 1U);
	return state.randring[index];
}

std::uint16_t randring_next16(DeterministicState &state) noexcept
{
	const auto index = state.randring_cursor;
	state.randring_cursor = static_cast<std::uint8_t>(state.randring_cursor + 1U);
	const auto low = state.randring[index];

	// ZUN's 256-byte ring is immediately followed by randring_p in the
	// original data segment. The 16-bit read at index 255 therefore observes
	// the already-incremented (and wrapped) cursor as its high byte.
	const auto high = (index == 0xFFU)
		? state.randring_cursor
		: state.randring[static_cast<std::size_t>(index) + 1U];
	return static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(low) |
		(static_cast<std::uint16_t>(high) << 8U)
	);
}

} // namespace th03::detail
