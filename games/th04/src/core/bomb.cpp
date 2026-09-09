#include "th04/core/state.hpp"

namespace th04::core {
namespace {
std::int16_t word(int value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
void tone(State &s, std::uint16_t value) noexcept
{
	s.palette_tone = value;
	s.palette_changed = true;
}
bool stars(State &s, Presentation &p) noexcept
{
	if (s.bomb_frame == 48) {
		for (auto &star : s.bomb_stars) {
			const auto x = pc98::th0405::randring_next16_mod(s.random, 6144);
			const auto y = pc98::th0405::randring_next16_mod(s.random, 5888);
			if (!x || !y) return false;
			star.center = {word(*x), word(*y)};
			if (s.playchar == 0) {
				star.angle = 0xC0;
				unsigned attempts = 0;
				while (star.center.x >= 2048 && star.center.x <= 4096) {
					// A full cursor cycle repeats forever for this source loop.
					if (attempts++ == 256) return false;
					const auto next = pc98::th0405::randring_next16_mod(s.random, 6144);
					if (!next) return false;
					star.center.x = word(*next);
				}
				const auto distance = star.center.x <= 3072
					? 2080 - star.center.x : star.center.x - 4064;
				star.speed = static_cast<std::uint8_t>(distance / 9);
			} else {
				star.angle = 0xE0;
				const auto speed = pc98::th0405::randring_next16_and(s.random, 127);
				if (!speed) return false;
				star.speed = static_cast<std::uint8_t>(*speed + 160);
			}
		}
	}
	for (std::size_t i = 0; i < s.bomb_stars.size(); ++i) {
		auto &star = s.bomb_stars[i];
		const auto v = pc98::masterlib::vector2(star.angle, star.speed);
		star.center.x = word(star.center.x + v.x);
		star.center.y = word(star.center.y + v.y);
		if (star.center.x <= -128 || star.center.x >= 6272 ||
			star.center.y <= -128 || star.center.y >= 6144) {
			if (s.playchar == 0) {
				star.center.y = 6144;
			} else if ((i & 1) != 0) {
				star.center.x = -128;
				const auto y = pc98::th0405::randring_next16_mod(s.random, 5888);
				if (!y) return false;
				star.center.y = word(*y);
			} else {
				const auto x = pc98::th0405::randring_next16_mod(s.random, 6144);
				if (!x) return false;
				star.center.x = word(*x);
				star.center.y = 6016;
			}
		}
		p.bomb_star(star.center, s.playchar == 0 ? 14 : 8);
	}
	return true;
}
bool character_bomb(State &s, Presentation &p) noexcept
{
	p.bomb_background_put(s.playchar);
	if (s.bomb_frame <= 80) {
		s.circles_color = s.playchar == 0 ? 9 : 15;
		tone(s, static_cast<std::uint16_t>(196 - (s.bomb_frame - 48) * 3));
	} else if (s.bomb_frame <= 160 && (s.stage_frame & 3) == 0) {
		if (s.playchar == 0) {
			const auto angle = static_cast<std::uint8_t>(s.stage_frame << 2);
			for (const auto a : {angle, static_cast<std::uint8_t>(0x80 - angle)}) {
				const auto v = pc98::masterlib::vector2(a, 2048);
				p.growing_circle({word(3072 + v.x), word(2944 + v.y)});
			}
		} else {
			auto x = (s.bomb_frame - 80) * 4;
			const auto y = (161 - s.bomb_frame) * 3 + 40;
			if (s.bomb_frame < 120) {
				const auto offset = pc98::th0405::randring_next16_mod(s.random,
					static_cast<std::uint16_t>((s.bomb_frame - 80) * 8));
				if (!offset) return false;
				x += *offset - (s.bomb_frame - 64) * 4;
			} else {
				const auto offset = pc98::th0405::randring_next16_mod(s.random,
					static_cast<std::uint16_t>((161 - s.bomb_frame) * 8));
				if (!offset) return false;
				x += *offset - (161 - s.bomb_frame) * 4;
			}
			p.growing_circle({word(x * 16), word(y * 16)});
		}
		p.sound(9);
	}
	return stars(s, p);
}
} // namespace

bool bomb_update(State &s, Presentation &p) noexcept
{
	if (!s.bombing) return true;
	if (s.playchar > 1) return false;
	if (s.bomb_frame < 32) p.bomb_cel(static_cast<std::uint8_t>(s.bomb_frame / 4));
	else if (s.bomb_frame < 48) p.bomb_cel(static_cast<std::uint8_t>(s.bomb_frame / 2 - 8));
	else if (s.bomb_frame < 176) {
		if (s.bomb_frame == 48) {
			s.scroll_active = false;
			p.graph_scroll(0);
			s.bomb_background = false;
			s.bomb_col14_backup = s.palette[14];
			s.palette[14] = {240, 176, 192};
		}
		if (!character_bomb(s, p)) return false;
	} else if (s.bomb_frame < 226) {
		if (s.bomb_frame == 176) {
			p.sound(15);
			s.scroll_active = true;
			s.items.items_pull_to_player = false;
		}
		s.palette[14] = s.bomb_col14_backup;
		s.bomb_background = true;
		tone(s, static_cast<std::uint16_t>(200 - (s.bomb_frame - 176) * 2));
		if (s.bomb_frame == 177) p.graph_scroll(s.scroll_line);
	} else {
		s.bombing = false;
		tone(s, 100);
		s.circles_color = 13;
	}
	++s.bomb_frame;
	return true;
}
} // namespace th04::core
