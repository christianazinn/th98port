#include "th02/core/midbossx.hpp"
#include "th02/core/stage_state.hpp"
#include <bit>
namespace th02::core {
namespace {
std::int16_t word(std::int32_t n) noexcept
{ return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(n)); }
void add(MidbossxState &m, std::int16_t x, std::int16_t y, std::uint8_t group) noexcept
{
	if (y < 0) y = word(y + 400); else if (y >= 400) y = word(y - 400);
	for (auto &b : m.bursts) if (!b.cel) { b = {1, 0, x, y, group, 0, 100}; return; }
}
void cross(MidbossxState &m, std::int16_t x, std::int16_t y, std::uint8_t group) noexcept
{
	const auto d = m.cross_distance;
	if (!d) add(m, x, y, group);
	else {
		add(m, word(x - d), y, group); add(m, x, word(y - d), group);
		add(m, word(x + d), y, group); add(m, x, word(y + d), group);
	}
	m.cross_distance = static_cast<std::uint8_t>(d + 16);
}
} // namespace
EnemyBulletAddResult midbossx_update_and_render(StageState &s, MidbossxCallbacks &p) noexcept
{
	auto &m = s.midbossx;
	auto &f = s.boss.phase_frame; f = word(f + 1);
	if (f == 1) { m.cross_distance = 0; m.active = true; }
	else if (f < 96) { if (f % 16 == 0) cross(m, 128, 128, enemy_bullet_group::ring_4); }
	else if (f == 96) m.cross_distance = 0;
	else if (f < 192) { if (f % 16 == 0) cross(m, 304, 0, enemy_bullet_group::ring_8); }
	else if (f == 192) m.cross_distance = 0;
	else if (f < 288) { if (f % 16 == 0) cross(m, 192, 336, enemy_bullet_group::ring_16); }
	if (f > 420) { f = 0; m.active = false; }
	constexpr std::array<std::uint8_t, 7> images{0, 1, 2, 16, 17, 18, 9};
	for (auto &b : m.bursts) {
		if (!b.cel) continue;
		b.frame = static_cast<std::uint8_t>(b.frame + 1);
		if ((b.frame & 7) != 0) continue;
		p.tile_ring_set_and_put_both_8(b.left, b.vram_y, images[b.cel]);
		if (++b.cel < 7) continue;
		b.vram_y = word(b.vram_y - s.scroll.line);
		if (b.vram_y < 0) b.vram_y = word(b.vram_y + 400);
		const auto result = bullets_add_pellet(s.bullets, s.random, s.frame,
			s.player_miss.playperf, s.rank, s.bomb.bombing,
			{word(b.left + 4), word(b.vram_y + 4)}, b.angle, b.group, b.speed);
		if (result != EnemyBulletAddResult::completed) return result;
		b.cel = 0;
	}
	return EnemyBulletAddResult::completed;
}
} // namespace th02::core
