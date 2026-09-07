#pragma once

#include <bit>
#include <cstdint>

namespace pc98::th0405 {

struct SubpixelPoint {
	std::int16_t x{}, y{};
	bool operator==(const SubpixelPoint &) const = default;
};

struct Motion {
	SubpixelPoint cur{}, prev{}, velocity{};
	bool operator==(const Motion &) const = default;
};

// Both near code-segment instances of th04/math/motion.inc perform the
// same word additions, saving the pre-update position before either clamp.
constexpr SubpixelPoint update_motion(Motion &motion) noexcept
{
	const auto add = [](std::int16_t position, std::int16_t velocity) {
		return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
			static_cast<std::int32_t>(position) + velocity));
	};
	motion.prev.x = motion.cur.x;
	motion.cur.x = add(motion.cur.x, motion.velocity.x);
	motion.prev.y = motion.cur.y;
	motion.cur.y = add(motion.cur.y, motion.velocity.y);
	return motion.cur;
}

// th04/main/player/pos.cpp, linked by both TH04 and TH05 via player_p.cpp.
// Values are raw Q12.4 units: x=8..376 pixels, y=8..352 pixels. Clamping
// does not reset velocity or replace the saved pre-update position.
constexpr void player_pos_update_and_clamp(Motion &motion) noexcept
{
	update_motion(motion);
	const auto clamp = [](std::int16_t value, std::int16_t maximum) {
		return value < 128 ? std::int16_t{128} : (value > maximum ? maximum : value);
	};
	motion.cur.x = clamp(motion.cur.x, 6016);
	motion.cur.y = clamp(motion.cur.y, 5632);
}

} // namespace pc98::th0405
