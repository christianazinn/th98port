#include "th04/core/midboss_extra.hpp"
#include "th04/core/state.hpp"
#include "th04/core/item.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_random.hpp"
#include <bit>

namespace th04::core {
namespace {
std::int16_t word(int value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
void orbit(MidbossState &m) noexcept
{
	m.pos.prev = m.pos.cur;
	const auto offset = pc98::masterlib::vector2(m.angle, m.hp);
	m.pos.cur = {word(3072 + offset.x), word(1536 + offset.y)};
	m.angle = static_cast<std::uint8_t>(m.angle - 2U);
}
void sweep(MidbossState &m) noexcept
{
	m.pos.prev = m.pos.cur;
	if (m.phase_frame == 1) { m.pos.velocity.x = 16; m.angle = 0; }
	m.pos.cur.x = word(m.pos.cur.x + m.pos.velocity.x);
	if (m.phase <= 5 && (m.pos.cur.x <= 256 || m.pos.cur.x >= 5888))
		m.pos.velocity.x = word(-m.pos.velocity.x);
	m.pos.cur.y = word(1536 + pc98::masterlib::vector_component(m.hp,
		pc98::masterlib::sine8(m.angle)));
	m.angle = static_cast<std::uint8_t>(m.angle + 2U);
}
bool random_ring(State &s, std::uint8_t count, std::uint8_t speed) noexcept
{
	auto &b = s.bullets.template_;
	b.group = BulletGroup::ring;
	b.count = count;
	const auto angle = pc98::th0405::randring_next16(s.random);
	if (!angle) return false;
	b.angle = static_cast<std::uint8_t>(*angle);
	b.speed = speed;
	return bullet_template_tune(s) && bullets_add(s);
}
bool pattern(State &s, Presentation &p, unsigned which) noexcept
{
	if ((s.stage_frame & 15U) != 0) return true;
	auto &b = s.bullets.template_;
	if (which == 0) return random_ring(s, 32, 48);
	if (which == 1) {
		b.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_backwards);
		b.patnum = 57;
		if (!random_ring(s, 32, static_cast<std::uint8_t>(s.midboss.phase_frame / 4 + 10))) return false;
		p.sound(3);
		return true;
	}
	if (which == 2) {
		b.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		b.special_motion = BulletSpecialMotion::bounce_left_right_top;
		b.group = BulletGroup::spread;
		b.count = 5;
		b.delta = 12;
		b.patnum = 59;
		b.speed = 32;
		const auto angle = pc98::th0405::randring_next16_and(s.random, 15);
		if (!angle) return false;
		b.angle = static_cast<std::uint8_t>(*angle + 0xB8U);
		if (!bullet_template_tune(s)) return false;
		s.bullets.special.value = 2;
		if (!bullets_add(s, true)) return false;
		p.sound(9);
		return true;
	}
	return random_ring(s, 32, 32) && random_ring(s, 16, 48);
}
bool gift_pattern(State &s, Presentation &p) noexcept
{
	auto &b = s.bullets.template_;
	switch (s.midboss.phase_frame) {
	case 250: case 258: case 266: case 274:
		p.sound(9); items_add(s, p, s.midboss.pos.cur, ItemType::dream); break;
	case 350: case 358: case 366: case 374:
		p.sound(9); items_add(s, p, s.midboss.pos.cur, ItemType::big_power); break;
	case 450:
		p.sound(9); items_add(s, p, s.midboss.pos.cur, ItemType::one_up); break;
	case 290: case 298: case 306: case 314:
	case 390: case 398: case 406: case 414:
	case 490: case 498: case 506: case 514:
		p.sound(3);
		b.group = BulletGroup::spread_aimed;
		b.count = 5; b.delta = 6; b.angle = 0; b.speed = 64;
		return bullet_template_tune(s) && bullets_add(s);
	}
	return true;
}
}
void extra_midboss_setup(State &s) noexcept
{
	s.midboss.frames_until = 5400;
	s.midboss.pos.cur = s.midboss.pos.prev = {-256, 4096};
	s.midboss.pos.velocity = {64, -64};
	s.midboss.hp = 4096;
	s.midboss.sprite = 0;
	s.midboss.angle = 0x60;
	s.midboss_active = false;
}
void extra_midboss_activate(State &s) noexcept
{
	if (s.midboss.frames_until != s.stage_frame) return;
	s.midboss.phase = 0;
	s.midboss.phase_frame = 0;
	s.midboss_active = true;
}
bool extra_midboss_update(State &s, Presentation &p) noexcept
{
	auto &m = s.midboss;
	s.bullets.template_.origin = m.pos.cur; // Before the source movement.
	s.bullets.template_.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
	switch (m.phase) {
	case 0: case 1: case 2:
		orbit(m);
		m.phase_frame = word(m.phase_frame + 1);
		if (m.phase == 1) { if (m.hp < 896) m.hp = word(m.hp + 8); }
		else if (m.hp > 128) m.hp = word(m.hp - 32);
		if (m.phase_frame < 128) break;
		if (!pattern(s, p, m.phase == 2 ? 1U : 0U)) return false;
		if (m.phase_frame < 320) break;
		m.phase_frame = 0; ++m.phase; break;
	case 3:
		if (m.phase_frame < 128) orbit(m);
		else {
			sweep(m);
			if (!pattern(s, p, 2)) return false;
			if (m.hp < 768) m.hp = word(m.hp + 8);
		}
		m.phase_frame = word(m.phase_frame + 1);
		if (m.phase_frame >= 640) { m.phase_frame = 0; ++m.phase; }
		break;
	case 4:
		sweep(m);
		if (m.phase_frame >= 160 && !pattern(s, p, 3)) return false;
		m.phase_frame = word(m.phase_frame + 1);
		if (m.phase_frame >= 440) { m.phase_frame = 2; ++m.phase; }
		break;
	case 5:
		sweep(m);
		if (!gift_pattern(s, p)) return false;
		m.phase_frame = word(m.phase_frame + 1);
		if (m.phase_frame >= 520) { m.phase_frame = 2; ++m.phase; }
		break;
	case 6:
		sweep(m);
		m.phase_frame = word(m.phase_frame + 1);
		if (m.pos.cur.x <= -256 || m.pos.cur.x >= 6400) {
			s.midboss_active = false; m.hp = 0;
		}
		break;
	}
	return true;
}
}
