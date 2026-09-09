#include "th02/core/boss_explosion.hpp"
#include "pc98/masterlib_vector.hpp"

#include <bit>

namespace th02::core {
namespace {
std::int16_t word(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

ScreenPoint ring_point(ScreenPoint center, std::int16_t radius,
	std::uint8_t angle, std::uint8_t offset) noexcept
{
	using namespace pc98::masterlib;
	return {word(center.x + vector_component(radius, cosine8(angle))),
		word(center.y + vector_component(radius,
			sine8(static_cast<std::uint8_t>(angle + offset))))};
}
}

void boss_explode_render(ScreenPoint center, std::int16_t frame,
	std::uint8_t angle_offset, std::int16_t scroll_line,
	std::uint8_t &slowdown_factor, std::uint16_t &palette_tone,
	BossExplosionCallbacks &callbacks) noexcept
{
	if (frame < 0) return;
	if (frame == 0) callbacks.explosion_margins_clear();
	if (frame < 32) {
		if (frame == 1) callbacks.sound_effect(18);
		center.x = word(center.x + 7);
		if (frame >= 2) {
			callbacks.explosion_egc_begin();
			callbacks.explosion_ring(center, word(240 - (frame - 2) * 8), true);
			callbacks.explosion_egc_end();
		}
		if (frame < 30) {
			callbacks.explosion_color(static_cast<std::uint8_t>((frame & 1) * 11 + 4));
			callbacks.explosion_ring(center, word(240 - frame * 8), false);
			callbacks.explosion_grcg_off();
		}
		return;
	}
	frame = static_cast<std::int16_t>(frame - 32);
	if (frame >= 26) return;
	if (frame == 0) callbacks.sound_effect(14);
	if (frame >= 2) {
		const auto old_frame = static_cast<std::int16_t>(frame - 2);
		callbacks.explosion_egc_begin();
		for (unsigned i = 0; i < 256; i += 4) {
			const auto point = ring_point(center, word(old_frame * 16),
				static_cast<std::uint8_t>(i + static_cast<unsigned>(old_frame)), angle_offset);
			if (point.x >= 16 && point.x < 416 && point.y > 0 && point.y < 384) {
				callbacks.explosion_invalidate(point);
			}
		}
		callbacks.explosion_tiles_render();
		callbacks.explosion_egc_end();
	}
	slowdown_factor = 1;
	if (frame >= 24) return;
	const auto patnum = static_cast<std::int16_t>(frame / 6 + 88);
	for (unsigned i = 0; i < 256; i += 4) {
		const auto point = ring_point(center, word(frame * 16),
			static_cast<std::uint8_t>(i + static_cast<unsigned>(frame)), angle_offset);
		auto top = word(point.y + scroll_line);
		if (top >= 400) top = static_cast<std::int16_t>(top - 400);
		if (point.x > 16 && point.x < 416 && point.y > 0 && point.y < 384) {
			callbacks.explosion_sprite({point.x, top}, patnum);
		}
	}
	slowdown_factor = 2;
	if (frame <= 8) {
		palette_tone = static_cast<std::uint16_t>((frame & 1) * 70 + 100);
		callbacks.explosion_palette_show();
	}
	if (frame <= 20) {
		const auto odd = static_cast<std::uint16_t>(frame & 1);
		callbacks.explosion_graph_scroll(static_cast<std::uint16_t>(400 - scroll_line),
			static_cast<std::uint16_t>(scroll_line * 40 + odd), odd);
		slowdown_factor = 3;
	}
}

} // namespace th02::core
