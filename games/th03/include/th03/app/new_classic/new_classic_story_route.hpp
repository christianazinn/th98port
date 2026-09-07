#ifndef TH03_APP_NEW_CLASSIC_STORY_ROUTE_HPP
#define TH03_APP_NEW_CLASSIC_STORY_ROUTE_HPP

#include <array>
#include <cstdint>
#include <optional>

namespace th03::app {

// Reconstructs the exact nine-opponent route chosen by Story selection from
// its resident RNG seed and selected paletted character (1..18).
[[nodiscard]] std::optional<std::array<std::int8_t, 9U>>
new_classic_story_opponents(
	std::int8_t paletted_character,
	std::int32_t resident_rand
) noexcept;

} // namespace th03::app

#endif
