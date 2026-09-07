#pragma once

#include "pc98/random.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace pc98::th0405 {

struct RandomState {
	std::uint32_t seed{};
	std::array<std::uint8_t, 256> ring{};
	std::uint16_t cursor{};
	bool operator==(const RandomState &) const = default;
};

// Both games link th03/math/randring_fill.cpp: reverse fill, then WORD reset.
constexpr void randring_fill(RandomState &state) noexcept
{
	for(std::size_t index = state.ring.size(); index-- > 0U;) {
		state.ring[index] = static_cast<std::uint8_t>(pc98::irand(state.seed));
	}
	state.cursor = 0;
}

// th04/math/randring.inc reads the word BEFORE incrementing the low cursor
// byte. At slot 255, the adjacent cursor supplies 0xFF as the high byte.
// A nonzero cursor high byte addresses unrelated DOS memory: refuse that
// unsupported state rather than normalize it or read outside the native ring.
[[nodiscard]] constexpr std::optional<std::uint16_t>
randring_next16(RandomState &state) noexcept
{
	if(state.cursor >= state.ring.size()) return std::nullopt;
	const auto index = state.cursor;
	const auto low = state.ring[index];
	const auto high = (index == 255U)
		? static_cast<std::uint8_t>(state.cursor)
		: state.ring[static_cast<std::size_t>(index) + 1U];
	const auto value = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(low) |
		(static_cast<std::uint16_t>(high) << 8U));
	state.cursor = static_cast<std::uint8_t>(state.cursor + 1U);
	return value;
}

[[nodiscard]] constexpr std::optional<std::uint16_t>
randring_next16_and(RandomState &state, std::uint16_t mask) noexcept
{
	const auto value = randring_next16(state);
	if(!value) return std::nullopt;
	return static_cast<std::uint16_t>(*value & mask);
}

// DIV by zero has no successful DOS result. Preserve its preceding draw and
// cursor advance, but report refusal instead of invoking host undefined behavior.
[[nodiscard]] constexpr std::optional<std::uint16_t>
randring_next16_mod(RandomState &state, std::uint16_t divisor) noexcept
{
	const auto value = randring_next16(state);
	if(!value || divisor == 0) return std::nullopt;
	return static_cast<std::uint16_t>(*value % divisor);
}

} // namespace pc98::th0405
