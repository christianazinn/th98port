#include "th04/core/stage.hpp"
#include "th04/core/state.hpp"
#include "th04/core/scroll.hpp"

namespace th04::core {
StageResult stage1_tick(State &s, std::uint16_t input, bool shift, Presentation &p) noexcept
{
	if (s.stage_clear_requested) return StageResult::cleared;
	if (s.gameover_pending) return StageResult::gameover;
	if (s.dialog_pending) return StageResult::dialog;
	if (input & 0x1000) return StageResult::pause;
	if (!s.boss_active) {
		if (s.std_data.scroll_speed == 0 && s.page_back == 1) {
			s.dialog_pending = true;
			return StageResult::dialog;
		}
		++s.total_std_frames;
	}
	stage1_midboss_activate(s);
	if (!std_run(s, p)) return StageResult::unsupported;
	stage1_circles_update(s, p);
	if (!pc98::th0405::sparks_update({s.sparks, s.spark_ring_offset})) return StageResult::unsupported;
	if (!player_update(s, input, shift, p)) return StageResult::unsupported;
	// The source blocks in its continue flow at this point, before shots.
	if (s.gameover_pending) return StageResult::gameover;
	shots_update(s.shots, s.option_cur);
	if (!bullets_update(s, p) || !enemies_update(s, p) ||
		!stage1_fights_update(s, p)) return StageResult::unsupported;
	if (s.stage_clear_requested) return StageResult::cleared;
	if (!items_update(s, p) ||
		!gathers_update(s) || !bomb_update(s, p)) return StageResult::unsupported;
	stage1_fights_render_tail(s, p);
	if (s.stage_clear_requested) return StageResult::cleared;
	if (s.playfield_shake_anim_time != 0) {
		s.playfield_shake_x = (s.stage_frame & 1) == 0 ? -2 : 2;
		s.playfield_shake_y = (s.stage_frame & 3) <= 1 ? -2 : 2;
		s.playfield_shake_anim_time = std::bit_cast<std::int16_t>(
			static_cast<std::uint16_t>(s.playfield_shake_anim_time - 1));
	}
	if (s.playfield_shake_x || s.playfield_shake_y) {
		p.playfield_shake({s.playfield_shake_x, s.playfield_shake_y});
		s.playfield_shake_redraw_time = 2;
	}
	if (s.playfield_shake_redraw_time != 0) {
		s.playfield_shake_redraw_time = std::bit_cast<std::int8_t>(
			static_cast<std::uint8_t>(s.playfield_shake_redraw_time - 1));
		s.playfield_shake_x = s.playfield_shake_y = 0;
	}
	s.palette_changed = false;
	if (!scroll_update(s, p)) return StageResult::unsupported;
	s.page_back ^= 1;
	++s.stage_frame;
	const unsigned interval = s.resources.rem_lives >= 10
		? 1000U : 6000U - s.resources.rem_lives * 500U;
	if (s.stage_frame % interval == 0) playperf_raise(s, 1);
	score_update(s, p);
	return StageResult::running;
}
bool stage1_dialog_complete(State &s) noexcept
{
	if (!s.dialog_pending) return false;
	s.dialog_pending = false;
	stage1_boss_activate(s);
	return true;
}
}
