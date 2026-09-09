#pragma once

#include "th02/core/player.hpp"

namespace th02::core {

// Final drawing boundaries only. Ring requests represent the complete
// dot_square_ring_* helper with angle step2; margin clearing includes both
// pages. Implementations must not mutate borrowed effect globals.
class BossExplosionCallbacks {
public:
	virtual ~BossExplosionCallbacks() = default;
	virtual void explosion_margins_clear() noexcept = 0;
	virtual void sound_effect(std::uint8_t id) noexcept = 0;
	virtual void explosion_egc_begin() noexcept = 0;
	virtual void explosion_egc_end() noexcept = 0;
	virtual void explosion_ring(ScreenPoint center, std::int16_t radius,
		bool invalidate) noexcept = 0;
	virtual void explosion_color(std::uint8_t color) noexcept = 0;
	virtual void explosion_grcg_off() noexcept = 0;
	virtual void explosion_invalidate(ScreenPoint point) noexcept = 0;
	virtual void explosion_tiles_render() noexcept = 0;
	virtual void explosion_sprite(ScreenPoint point, std::int16_t patnum) noexcept = 0;
	virtual void explosion_palette_show() noexcept = 0;
	virtual void explosion_graph_scroll(std::uint16_t lines,
		std::uint16_t address, std::uint16_t lower_address) noexcept = 0;
};

// th02/main/explode.cpp::boss_explode_render. The caller owns the frame;
// this body writes the actual shared slowdown/tone globals at source points.
void boss_explode_render(ScreenPoint center, std::int16_t frame,
	std::uint8_t angle_offset, std::int16_t scroll_line,
	std::uint8_t &slowdown_factor, std::uint16_t &palette_tone,
	BossExplosionCallbacks &callbacks) noexcept;

} // namespace th02::core
