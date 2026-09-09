#include "th02/core/boss_explosion.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
using namespace th02::core;
void require(bool condition)
{
	if (!condition) { std::fputs("boss explosion assertion failed\n", stderr); std::abort(); }
}
struct Sink final : BossExplosionCallbacks {
	unsigned calls{}, sprites{}, invalidations{}, tones{}, scrolls{};
	std::uint8_t sound{};
	ScreenPoint ring_center{}, first_sprite{};
	std::int16_t radius{}, patnum{};
	bool invalidating{};
	void explosion_margins_clear() noexcept override { ++calls; }
	void sound_effect(std::uint8_t id) noexcept override { ++calls; sound = id; }
	void explosion_egc_begin() noexcept override { ++calls; }
	void explosion_egc_end() noexcept override { ++calls; }
	void explosion_ring(ScreenPoint center, std::int16_t r, bool invalidate) noexcept override {
		++calls; ring_center = center; radius = r; invalidating = invalidate;
	}
	void explosion_color(std::uint8_t) noexcept override { ++calls; }
	void explosion_grcg_off() noexcept override { ++calls; }
	void explosion_invalidate(ScreenPoint) noexcept override { ++calls; ++invalidations; }
	void explosion_tiles_render() noexcept override { ++calls; }
	void explosion_sprite(ScreenPoint point, std::int16_t pattern) noexcept override {
		++calls;
		if (sprites++ == 0) { first_sprite = point; patnum = pattern; }
	}
	void explosion_palette_show() noexcept override { ++calls; ++tones; }
	void explosion_graph_scroll(std::uint16_t, std::uint16_t, std::uint16_t) noexcept override {
		++calls; ++scrolls;
	}
};
}

int main()
{
	std::uint8_t slowdown = 7;
	std::uint16_t tone = 83;
	Sink sink{};
	boss_explode_render({200, 100}, -1, 32, 0, slowdown, tone, sink);
	require(sink.calls == 0 && slowdown == 7 && tone == 83);
	boss_explode_render({200, 100}, 1, 32, 0, slowdown, tone, sink);
	require(sink.sound == 18 && sink.ring_center == ScreenPoint{207, 100});
	require(sink.radius == 232 && !sink.invalidating && slowdown == 7);
	sink = {};
	boss_explode_render({200, 100}, 30, 32, 0, slowdown, tone, sink);
	require(sink.invalidating && sink.radius == 16 && sink.sprites == 0);
	sink = {};
	boss_explode_render({200, 100}, 32, 32, 350, slowdown, tone, sink);
	require(sink.sound == 14 && sink.sprites == 64 && sink.invalidations == 0);
	require(sink.first_sprite == ScreenPoint{200, 50} && sink.patnum == 88);
	require(slowdown == 3 && tone == 100 && sink.tones == 1 && sink.scrolls == 1);
	sink = {};
	boss_explode_render({200, 100}, 33, 32, 0, slowdown, tone, sink);
	require(tone == 170 && slowdown == 3);
	require(sink.first_sprite == ScreenPoint{216, 111});
	sink = {};
	boss_explode_render({200, 100}, 53, 32, 0, slowdown, tone, sink);
	require(slowdown == 2 && tone == 170 && sink.tones == 0 && sink.scrolls == 0);
	sink = {};
	boss_explode_render({200, 100}, 56, 32, 0, slowdown, tone, sink);
	require(slowdown == 1 && sink.sprites == 0 && sink.tones == 0);
	sink = {};
	slowdown = 9;
	boss_explode_render({200, 100}, 58, 32, 0, slowdown, tone, sink);
	require(sink.calls == 0 && slowdown == 9 && tone == 170);
	sink = {};
	boss_explode_render({16, 100}, 32, 32, 0, slowdown, tone, sink);
	require(sink.sprites == 0);
	sink = {};
	boss_explode_render({16, 100}, 34, 32, 0, slowdown, tone, sink);
	require(sink.invalidations == 64);
}
