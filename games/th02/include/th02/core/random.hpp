#pragma once

#include <array>
#include <cstdint>

namespace th02::core {

struct RandomState {
	std::uint32_t seed{};
	std::array<std::uint8_t, 256> ring{};
	std::uint8_t cursor{};
	bool operator==(const RandomState &) const = default;
};

// Source: th02/main/randfill.cpp. Fills forward; does not reset the cursor.
void randring_fill(RandomState &state) noexcept;

// Both original code-segment instances use this same ring and cursor.
[[nodiscard]] std::uint8_t randring_next8(RandomState &state) noexcept;
[[nodiscard]] std::uint16_t randring_next16(RandomState &state) noexcept;

} // namespace th02::core
