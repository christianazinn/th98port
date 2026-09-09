#include "th04/core/stage.hpp"
#include "th04/core/state.hpp"

namespace th04::core {
bool gameplay_init(State &s, std::uint8_t rank, std::uint8_t playchar,
	std::uint8_t shottype, std::uint8_t lives, std::uint8_t bombs, bool demo, bool turbo) noexcept
{
	if (rank > 4 || playchar > 1 || shottype > 1) return false;
	s.miss_count = s.bombs_used = 0;
	s.playchar = playchar;
	s.shottype = shottype;
	s.score.score.fill(0);
	s.power = 1;
	s.bombs = s.credit_bombs = bombs;
	s.credit_lives = lives;
	s.resources.rem_lives = lives;
	s.rank = demo ? 2 : rank;
	s.resources.playperf = demo ? 28 : 16;
	s.bullets.turbo_mode = demo || rank == 4 || turbo;
	score_reset(s.score);
	constexpr std::array<std::uint16_t, 5> graze{100, 250, 400, 500, 2560};
	constexpr std::array<std::uint8_t, 5> minimum{4, 11, 20, 22, 16};
	constexpr std::array<std::uint8_t, 5> maximum{16, 24, 32, 34, 20};
	s.bullets.graze_score = graze[s.rank];
	s.playperf_min = minimum[s.rank];
	s.resources.playperf_max = maximum[s.rank];
	if (s.rank == 2) s.resources.playperf = 20;
	if (s.rank == 3) s.resources.playperf = 22;
	return true;
}
void stage_init(State &s, Presentation &p) noexcept
{
	// Reset only the source arrays and named globals, not their adjacent state.
	s.stage_frame = 0;
	s.bullets.slowdown_factor = 1;
	s.gameover_pending = s.stage_clear_requested = false;
	s.dialog_pending = false;
	s.palette_changed = false;
	s.bullets.zap.set_active(false);
	s.bullets.stage_graze = 0;
	s.circles_color = 13;
	s.shots.shots.fill({});
	s.enemies.enemies.fill({});
	s.boss.kurumi_spawnrays.fill({}); // Source custom_entities reset.
	s.boss.elly_boomerang = {};
	s.sparks.fill({});
	s.bullets.pool.fill({});
	s.circles.pool.fill({});
	s.items.items.fill({});
	s.bullets.gathers.fill({});
	auto &g = s.bullets.gather_template;
	g.ring_points = 8;
	g.col = 9;
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
	s.items.stage_point_items_collected = s.items.dream_items_collected = 0;
	// TH04 resets cadence through shot_reset(), rather than an inline store.
	s.shots.shot_laser_time = 0;
	s.shots.shot_laser_style = 0;
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
	thicklasers_reset(s);
}
}
