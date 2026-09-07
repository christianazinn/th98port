#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th01::core {

struct Shot {
	std::int16_t left{};
	std::int16_t top{};
	std::int16_t unknown{};
	bool moving{};
	std::uint8_t decay_frame{};
	bool operator==(const Shot &) const = default;
};

// REIIDEN's global CShots is zero-initialized before its constructor sets the
// first three unknown fields. These fields are retained for checkpoints.
struct Shots {
	std::array<Shot, 8> slots{};
	constexpr Shots() noexcept
	{
		slots[0].unknown = 0x50;
		slots[1].unknown = 0x60;
		slots[2].unknown = 0x70;
	}
	bool operator==(const Shots &) const = default;
};

// Source: th01/main/player/shot.cpp (CShots::add/hittest_pellet/hittest_boss).
// A returned slot identifies the only changed shot. The host plays SFX 1 for
// spawn; on a hit it unblits the old shot and plays SFX 16. This is not the
// shot update loop: movement, decay animation, and Orb hits remain separate.
[[nodiscard]] std::optional<std::size_t> add_shot(
	Shots &shots, std::int16_t left, std::int16_t top
) noexcept;
[[nodiscard]] std::optional<std::size_t> hit_pellet(
	Shots &shots, std::int16_t left, std::int16_t top
) noexcept;
// Width/height already exclude the 16x16 shot sprite, like the donor helper.
[[nodiscard]] std::optional<std::size_t> hit_boss(
	Shots &shots, std::int16_t left, std::int16_t top,
	std::int16_t width_minus_shot, std::int16_t height_minus_shot
) noexcept;

} // namespace th01::core
