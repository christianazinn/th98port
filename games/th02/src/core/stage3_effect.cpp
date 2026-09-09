#include "th02/core/stage3_effect.hpp"
#include "pc98/masterlib_vector.hpp"
#include <bit>

namespace th02::core {
namespace {
std::int16_t word(int value) noexcept { return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value)); }
}
bool stage3_invalidate(const Stage3EffectState &s, std::size_t back,
	StageSceneryCallbacks &p) noexcept
{
	if (back >= s.dots.size()) return false;
	for (auto point : s.dots[back]) p.invalidate_scenery({point, 1, 2});
	return true;
}
bool stage3_update_and_render(Stage3EffectState &s, std::size_t back,
	std::int16_t scroll, std::uint8_t &slowdown, std::uint16_t &tone,
	Stage3EffectPresentation &p) noexcept
{
	++s.frame;
	if (s.frame < 168) return true;
	if (s.frame == 168) p.sound_effect(14);
	else if (s.frame < 200) {
		if (s.frame <= 180) {
			tone = static_cast<std::uint16_t>((s.frame & 1) * 70 + 100);
			p.palette_tone_set(tone);
		}
		const auto parity = static_cast<std::uint16_t>(s.frame & 1);
		p.graph_scroll(static_cast<std::uint16_t>(400 - scroll), static_cast<std::uint16_t>(scroll * 40 + parity), parity);
		slowdown = 3;
		s.center = {224, 200};
		s.radius = 0;
		s.angle = 0;
	} else if (s.frame < 400) {
		if (s.frame == 200) slowdown = 1;
		p.palette_set(0, {static_cast<std::uint8_t>(200 - (s.frame >> 1)), 0, 0});
		s.radius = word(s.frame - 200);
		++s.angle;
	} else if (s.frame < 600) {
		p.palette_set(0, {0, static_cast<std::uint8_t>(44 - (s.frame >> 1)), 0});
		s.radius = word(s.frame - 400);
		--s.angle;
	} else if (s.frame < 800) {
		p.palette_set(0, {0, 0, static_cast<std::uint8_t>(144 - (s.frame >> 1))});
		s.radius = word(s.frame - 600);
		++s.angle;
	} else if (s.frame < 1000) {
		const auto c = static_cast<std::uint8_t>(244 - (s.frame >> 1));
		p.palette_set(0, {c, 0, c});
		s.radius = word(s.frame - 800);
		--s.angle;
	} else s.frame = 200;
	p.palette_show();
	p.egc_off();
	p.grcg_set_color(7);
	if (back >= s.dots.size()) return false;
	auto angle = s.angle;
	for (auto &point : s.dots[back]) {
		using namespace pc98::masterlib;
		point = {word(s.center.x + vector_component(s.radius, cosine8(angle))),
			word(s.center.y + vector_component(s.radius, sine8(angle)))};
		auto y = word(point.y + scroll);
		if (y >= 400) y = word(y - 400);
		p.dot_square({point.x, y}, 1);
		angle = static_cast<std::uint8_t>(angle + 4);
	}
	p.grcg_off();
	return true;
}
} // namespace th02::core
