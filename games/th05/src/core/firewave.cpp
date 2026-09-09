#include "th05/core/firewave.hpp"
#include "th05/core/state.hpp"
#include "pc98/masterlib_vector.hpp"
#include <bit>

namespace th05::core {
namespace {
std::int16_t word(int value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
}
void firewaves_add(State &s, std::int16_t amplitude, bool is_right) noexcept
{
	for (auto &wave : s.firewaves) {
		if (wave.alive) continue;
		wave.alive = true;
		wave.is_right = is_right;
		wave.bottom = 16;
		wave.amplitude = amplitude;
		return;
	}
}
void firewaves_update(State &s) noexcept
{
	for (auto &wave : s.firewaves) {
		if (!wave.alive) continue;
		wave.bottom = word(wave.bottom + 4);
		if (wave.bottom >= 608) wave.alive = false;
	}
}
void firewaves_render_hittest(State &s) noexcept
{
	// Source to_pixel_slow() is division, truncating negative coordinates
	// toward zero. It does not add the playfield's screen-space origin.
	const int player_x = s.player_pos.cur.x / 16;
	const int player_y = s.player_pos.cur.y / 16;
	for (const auto &wave : s.firewaves) {
		if (!wave.alive) continue;
		int top = wave.bottom;
		int angle = (top & 15) / 2;
		top &= ~15;
		for (; top >= 16 && angle < 128; top -= 16, angle += 8) {
			if (top >= 384) continue;
			int x = pc98::masterlib::vector_component(wave.amplitude,
				pc98::masterlib::sine8(static_cast<std::uint8_t>(angle)));
			x = wave.is_right ? word(416 - x + 16) : word(x + 16);
			if (static_cast<std::uint16_t>(player_y - top) < 16U &&
				(wave.is_right ? player_x > word(x - 32) : player_x < word(x - 32)))
				s.player_is_hit = true;
		}
	}
}
} // namespace th05::core
