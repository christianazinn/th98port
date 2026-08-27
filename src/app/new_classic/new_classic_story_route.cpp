#include "th03/app/new_classic/new_classic_story_route.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace th03::app {
namespace {

[[nodiscard]] std::uint16_t irand(std::uint32_t &seed) noexcept
{
	seed = (seed * 0x015A4E35U) + 1U;
	return static_cast<std::uint16_t>((seed >> 16U) & 0x7FFFU);
}

[[nodiscard]] constexpr std::int8_t first_palette(
	std::uint8_t character
) noexcept
{
	return static_cast<std::int8_t>((character * 2U) + 1U);
}

} // namespace

std::optional<std::array<std::int8_t, 9U>> new_classic_story_opponents(
	std::int8_t paletted_character,
	std::int32_t resident_rand
) noexcept
{
	if(paletted_character < 1 || paletted_character > 18) {
		return std::nullopt;
	}
	constexpr std::array<std::uint8_t, 9U> fixed_stage_six{
		1U, 0U, 0U, 2U, 0U, 3U, 5U, 4U, 6U,
	};
	const auto player = static_cast<std::uint8_t>(paletted_character);
	const auto character = static_cast<std::uint8_t>((player - 1U) / 2U);
	const auto fixed = fixed_stage_six[character];
	std::array<std::int8_t, 9U> result{};
	std::array<bool, 7U> used{};
	used[fixed] = true;
	auto seed = std::bit_cast<std::uint32_t>(resident_rand);
	for(std::size_t stage = 0U; stage < 6U; ++stage) {
		std::uint8_t candidate = 0U;
		do {
			candidate = static_cast<std::uint8_t>(irand(seed) % 7U);
		} while(used[candidate]);
		used[candidate] = true;
		result[stage] = first_palette(candidate);
	}
	result[6U] = first_palette(fixed);
	result[7U] = 15;
	result[8U] = 17;
	for(auto &opponent : result) {
		if(opponent == paletted_character) {
			++opponent;
		}
	}
	return result;
}

} // namespace th03::app
