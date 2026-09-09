#include "th02/core/stage3_effect.hpp"
#include <cstdlib>

using namespace th02::core;
namespace {
void require(bool v) { if (!v) std::abort(); }
struct Sink final : Stage3EffectPresentation {
	unsigned dots{}, shows{}, sounds{}, colors{}, scrolls{};
	StageSceneryRgb color{};
	void sound_effect(std::uint8_t id) noexcept override { require(id == 14); ++sounds; }
	void graph_scroll(std::uint16_t, std::uint16_t, std::uint16_t) noexcept override { ++scrolls; }
	void invalidate_scenery(StageSceneryInvalidation) noexcept override {}
	void palette_set(std::uint8_t index, StageSceneryRgb c) noexcept override { require(index == 0); color = c; ++colors; }
	void palette_tone_set(std::uint16_t) noexcept override {}
	void palette_show() noexcept override { ++shows; }
	void grc_set_clip(std::int16_t, std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void egc_off() noexcept override {}
	void grcg_set_color(std::uint8_t c) noexcept override { require(c == 7); }
	void dot_square(ScreenPoint, std::uint8_t edge) noexcept override { require(edge == 1); ++dots; }
	void grcg_off() noexcept override {}
};
}
int main()
{
	Stage3EffectState s{};
	std::uint8_t slowdown = 1;
	std::uint16_t tone = 100;
	Sink p;
	for (unsigned i = 0; i < 167; ++i) require(stage3_update_and_render(s, 0, 37, slowdown, tone, p));
	require(p.dots == 0);
	require(stage3_update_and_render(s, 0, 37, slowdown, tone, p));
	require(p.sounds == 1 && p.dots == 64 && s.center == ScreenPoint{});
	for (unsigned i = 169; i <= 200; ++i) require(stage3_update_and_render(s, 0, 37, slowdown, tone, p));
	require(slowdown == 1 && tone == 100 && s.center == ScreenPoint{224, 200});
	require(p.color == StageSceneryRgb{100, 0, 0} && s.radius == 0 && s.angle == 1 && p.scrolls == 31);
	for (unsigned i = 201; i <= 999; ++i) require(stage3_update_and_render(s, 0, 37, slowdown, tone, p));
	const auto dots = s.dots[0];
	const auto colors = p.colors;
	require(stage3_update_and_render(s, 0, 37, slowdown, tone, p));
	require(s.frame == 200 && s.radius == 199 && s.dots[0] == dots && p.colors == colors);
	require(stage3_invalidate(s, 0, p) && !stage3_invalidate(s, 2, p));
}
