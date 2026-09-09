#include "th05/core/stage.hpp"
#include "th05/core/state.hpp"
#include "th05/core/enemy.hpp"

namespace th05::core {
void stage_init(State &s, Presentation &p) noexcept
{
	// Reset the source arrays, not every subsystem's persistent surrounding data.
	s.stage_frame = 0;
	s.slowdown_factor = 1;
	s.slowdown_caused_by_bullets = false;
	s.gameover_pending = s.stage_clear_requested = s.dialog_pending = false;
	// stage_setup calls midboss_reset before the stage-specific setup callback.
	s.midboss_callback_active = false;
	s.midboss.hp = 0;
	s.palette_changed = false;
	s.bullets.zap.stop();
	s.bullets.stage_graze = 0;
	s.circles_color = 13;
	s.shots.hitshot_state.hitshots.fill({});
	s.shots.hitshot_state.next_free_id = 0;
	s.lasers.pool.fill({});
	s.shots.shots.fill({});
	s.enemies.enemies.fill({});
	s.stage2_scenery.particles.fill({}); // Source custom_entities reset.
	s.alice.puppets.fill({});
	s.stage4.custom = {};
	s.swords = {};
	s.b6balls = {};
	s.sparks.fill({});
	s.bullets.pool.pellets.fill({});
	s.bullets.pool.bullets16.fill({});
	s.items.items.fill({});
	s.bullets.gathers.fill({});
	auto &g = s.bullets.gather_template;
	g.ring_points = 8;
	g.color = 9;
	g.radius = 64 * 16;
	g.angle_delta = 2;
	g.velocity = {};
	s.bombing_disabled = false;
	s.scroll_line = s.tile_ring_row_filled = 0;
	s.scroll_line_on_page.fill(0);
	s.scroll_subpixel_line = s.scroll_lines_pending = s.scroll_lines_prev_frame = 0;
	s.playfield_shake_x = s.playfield_shake_y = 0;
	s.player_pos.cur = s.player_pos.prev = {192 * 16, 320 * 16};
	s.miss_move_lock_time = s.miss_time = 0;
	s.player_is_hit = false;
	s.player_invincibility_time = 64;
	s.items.stage_point_items_collected = 0;
	s.shot_spawn.shot_time = 0;
	player_shot_level_update(s, p);
	pc98::th0405::randring_fill(s.random);
	items_init(s, p);
	s.bombing = s.bomb_background = false;
	for (auto &spark : s.sparks) {
		spark.angle = static_cast<std::uint16_t>((spark.angle & 0xFF00U) |
			static_cast<std::uint8_t>(pc98::irand(s.random.seed)));
	}
	s.spark_ring_offset &= 0xFF00;
	p.hud(HudField::score);
}
}
