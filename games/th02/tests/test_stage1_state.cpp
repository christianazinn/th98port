#include "th02/core/stage1_state.hpp"
#include "th02/core/stage1_prepare.hpp"
#include "th02/core/stage1_setup.hpp"

#include <cstdlib>
#include <vector>

namespace {

using namespace th02::core;

class Trace final : public EnemyUpdateCallbacks, public RikaCallbacks,
	public Midboss1Callbacks, public EnemyInvalidationCallbacks,
	public PlayerFrameInvalidationCallbacks,
	public LaserInvalidationCallbacks, public ItemScheduleCallbacks,
	public SparkInvalidationCallbacks, public BombAnimationCallbacks,
	public BombPresentationCallbacks, public PlayerMissPresentationCallbacks,
	public ItemLifecyclePresentationCallbacks, public LaserUpdateCallbacks,
	public EnemyBulletPresentationCallbacks, public Stage1PreparationPresentation,
	public Stage1SetupPresentation, public Stage1FramePresentation, public TileInitialCallbacks {
public:
	unsigned initial_rows{};
	std::int16_t last_initial_top{};
	void put_initial_row(std::int16_t top, const TileSectionRow &) noexcept override {
		++initial_rows;
		last_initial_top = top;
	}
	void copy_line(const TileLineCopy &) noexcept override {}
	void egc_end() noexcept override {}
	void graph_scroll(ScrollPresentation) noexcept override {}
	void wait_for_presentation(std::uint8_t) noexcept override {}
	void page_show(std::size_t) noexcept override {}
	void page_access(std::size_t) noexcept override {}
	void score_events(const ScoreEvents &) noexcept override {}
	void background_particles_reset() noexcept override {}
	void sound_effects_reset() noexcept override {}
	const Stage1State *observed_state{};
	std::int16_t tile_line{};
	std::int16_t tile_midboss{};
	std::int16_t tile_spawn_row{};
	bool tile_render_all{};
	unsigned tile_calls{};
	ScreenPoint scenery_dot{};
	void egc_begin() noexcept override {}
	void egc_off() noexcept override {}
	void invalidate_scenery(StageSceneryInvalidation) noexcept override {}
	void palette_set(std::uint8_t, StageSceneryRgb) noexcept override {}
	void palette_tone_set(std::uint16_t) noexcept override {}
	void grc_set_clip(std::int16_t, std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void dot_square(ScreenPoint point, std::uint8_t) noexcept override { scenery_dot = point; }
	void tiles_render(const Stage1State &state) noexcept override {
		observed_state = &state;
		tile_line = state.scroll.line;
		tile_midboss = state.midboss_active;
		tile_spawn_row = state.enemies.spawn_row_cur;
		tile_render_all = state.tiles_egc_render_all;
		++tile_calls;
	}
	void sound_effect(std::uint8_t) noexcept override {}
	void tile_ring_set_and_put_both_8(
		std::int16_t, std::int16_t, std::int16_t
	) noexcept override {}
	void render_enemy(EnemySprite) noexcept override {}
	void render_enemy_hit_white(EnemySprite) noexcept override {}
	void invalidate_rika(RikaInvalidation) noexcept override {}
	void render_rika(RikaSprite) noexcept override {}
	void explosion_margins_clear() noexcept override {}
	void explosion_egc_begin() noexcept override {}
	void explosion_egc_end() noexcept override {}
	void explosion_ring(ScreenPoint, std::int16_t, bool) noexcept override {}
	void explosion_color(std::uint8_t) noexcept override {}
	void explosion_grcg_off() noexcept override {}
	void explosion_invalidate(ScreenPoint) noexcept override {}
	void explosion_tiles_render() noexcept override {}
	void explosion_sprite(ScreenPoint, std::int16_t) noexcept override {}
	void explosion_palette_show() noexcept override {}
	void explosion_graph_scroll(std::uint16_t, std::uint16_t,
		std::uint16_t) noexcept override {}
	void invalidate_midboss(Midboss1Invalidation) noexcept override {}
	void render_midboss(Midboss1Sprite) noexcept override {}
	void invalidate_enemy(EnemyInvalidation) noexcept override {}
	void invalidate_shot(PlayerShotInvalidation) noexcept override {}
	void invalidate_player(PlayerFrameInvalidation) noexcept override {}
	void invalidate_option(PlayerFrameInvalidation) noexcept override {}
	void invalidate_laser(LaserInvalidation) noexcept override {}
	void pointnums_init_for_rank_and_reset() noexcept override {}
	void invalidate_item(ItemInvalidation) noexcept override {}
	void pointnums_invalidate() noexcept override {}
	void invalidate_spark(SparkInvalidation) noexcept override {}
	void invalidate_bullet(EnemyBulletInvalidation) noexcept override {}
	void hud_bombs(std::int8_t) noexcept override {}
	void invalidate_bomb(BombInvalidation) noexcept override {}
	void render_all() noexcept override {}
	BombRgb palette_get(std::uint8_t) noexcept override { return {}; }
	void palette_set(std::uint8_t, BombRgb) noexcept override {}
	void palette_show() noexcept override {}
	std::uint8_t tile_mode_get() noexcept override { return 0U; }
	void tile_mode_set(std::uint8_t) noexcept override {}
	void boxfill(std::int16_t, std::int16_t, std::int16_t,
		std::int16_t) noexcept override {}
	void bomb_bft(std::int16_t) noexcept override {}
	void bomb_b_tram(std::uint8_t) noexcept override {}
	void bomb_smear(std::uint8_t, std::int16_t) noexcept override {}
	void pi_put(std::int16_t, std::int16_t, std::uint8_t) noexcept override {}
	void overlay_wipe() noexcept override {}
	void grcg_set_color(std::uint8_t) noexcept override {}
	void circle_point(ScreenPoint) noexcept override {}
	void particle(ScreenPoint, std::int16_t) noexcept override {}
	void grcg_off() noexcept override {}
	void hud_power(std::uint8_t, std::uint8_t) noexcept override {}
	void hud_lives(std::int8_t) noexcept override {}
	void render_player(PlayerMissSprite) noexcept override {}
	void pointnum_add(std::int16_t, std::int16_t, std::uint16_t) noexcept override {}
	void render_item(ItemSprite) noexcept override {}
	void pointnums_update_and_render() noexcept override {}
	void render_laser(LaserPresentation) noexcept override {}
	void present(EnemyBulletPresentationEvent) noexcept override {}
};

[[nodiscard]] Stage1InvalidationPresentation invalidation_presentation(
	Trace &trace
) noexcept
{
	auto &bullet_invalidation = static_cast<EnemyBulletInvalidationCallbacks &>(
		static_cast<BombPresentationCallbacks &>(trace)
	);
	return {trace, trace, trace, trace, bullet_invalidation, trace, trace,
		trace, trace};
}

[[nodiscard]] Stage1EntitiesPresentation entities_presentation(
	Trace &trace
) noexcept
{
	return {trace, trace, trace, trace, trace, trace, trace};
}

void set_valid_pages(Stage1State &state) noexcept
{
	state.frame.page_front = 0U;
	state.frame.page_back = 1U;
	state.frame.player.current = {100, 200};
	state.frame.player.on_page = {{{100, 200}, {100, 200}}};
	state.frame.option_left_on_page = {{{84, 216}, {84, 216}}};
}

[[nodiscard]] bool test_invalidation_overload_borrows_one_state() noexcept
{
	Stage1State state{};
	set_valid_pages(state);
	state.boss_enabled = false;
	state.enemies_enabled = false;
	state.lasers_enabled = false;
	state.midboss_active = 0;
	state.shottype = 2U;
	state.stage_frame = 123U;
	Trace trace{};
	auto presentation = invalidation_presentation(trace);
	return stage1_invalidate(state, presentation) ==
		Stage1InvalidationResult::completed && state.frame.page_front == 0U &&
		state.frame.page_back == 1U && state.shottype == 2U &&
		state.stage_frame == 123U;
}

[[nodiscard]] bool test_player_bomb_overload_uses_canonical_boss_and_frame() noexcept
{
	Stage1State state{};
	set_valid_pages(state);
	state.boss.target = {180, 100};
	state.player_hit.raw_hit = 1U;
	state.player_volley.type = PlayerShotType::b;
	state.bomb_selection = BombSelection::reimu_c;
	Trace trace{};
	const auto result = player_move_and_bomb_update(state, trace, trace);
	return result.player_result == PlayerFrameResult::hit_blocked &&
		result.bomb_result == BombUpdateResult::inactive &&
		result.bomb_update_called && !result.bomb_request_result.has_value() &&
		state.player_volley.boss_position == ShotPixelPoint{180, 100} &&
		state.frame.page_back == 1U;
}

[[nodiscard]] bool test_entities_overload_builds_local_adapters() noexcept
{
	Stage1State state{};
	set_valid_pages(state);
	state.boss.target = {77, 88};
	state.enemy_target_projection.boss_position = {11, 12};
	state.enemies_enabled = false;
	state.boss_enabled = false;
	state.lasers_enabled = false;
	state.midboss_active = 0;
	state.max_enemy_vm_steps = 64U;
	state.stage_progression = StageProgression::clear;
	Trace trace{};
	auto presentation = entities_presentation(trace);
	const auto result = stage1_entities_update(state, presentation);
	return result.result == Stage1EntitiesResult::completed &&
		state.stage_progression == StageProgression::stage &&
		state.boss.target == ScreenPoint{77, 88} &&
		state.enemy_target_projection.boss_position == ScreenPoint{11, 12} &&
		state.item_lifecycle.frame_unused == 1 && state.frame.page_back == 1U &&
		state.stage_frame == 0U && state.resident_frame == 0;
}

[[nodiscard]] bool test_enabled_enemy_rejects_invalid_projection_before_context() noexcept
{
	Stage1State state{};
	set_valid_pages(state);
	state.enemies_enabled = true;
	state.stage_data.template_count = -1;
	state.boss.target = {77, 88};
	Trace trace{};
	auto presentation = entities_presentation(trace);
	const auto result = stage1_entities_update(state, presentation);
	return result.result == Stage1EntitiesResult::enemy_refused &&
		result.enemy == EnemyUpdateResult::invalid_template &&
		state.boss.target == ScreenPoint{77, 88} &&
		state.item_lifecycle.frame_unused == 0;
}

[[nodiscard]] bool test_prepare_old_page_and_activation() noexcept
{
	Stage1State state{};
	set_valid_pages(state);
	scroll_reset(state.scroll);
	state.scroll.line = 200;
	state.scroll_line_on_page = {20, 37};
	state.scroll.step = 116;
	state.scroll.step_advanced = 1;
	state.tiles_egc_render_all = true;
	state.midboss.scenery_frame = 1000;
	state.scenery.particles[0].flag = stage_scenery_alive;
	state.scenery.particles[0].left = {100, 300};
	state.scenery.particles[0].top = {10, 200};
	state.scenery.particles[0].speed = 3;
	state.stage_data.template_count = 1;
	state.stage_data.spawn_row_count = 1;
	state.stage_data.spawn_rows.resize(1);
	auto &row = state.stage_data.spawn_rows[0].column;
	row.fill(-1);
	row[0] = 116;
	row[1] = 0;
	Trace trace{};
	auto invalidation = invalidation_presentation(trace);
	const auto result = stage1_prepare(state, invalidation, trace);
	return result == Stage1PreparationResult::input_pending &&
		trace.tile_line == 37 && trace.tile_midboss == 0 && trace.tile_spawn_row == 1 &&
		trace.tile_render_all && !state.tiles_egc_render_all &&
		state.enemies.enemies[0].flag == enemy_alive &&
		state.random.cursor == 1 && trace.scenery_dot == ScreenPoint{97, 50} &&
		state.midboss_active == 1 && state.scroll.line == 199 &&
		state.scroll_line_on_page[1] == 199 && state.scroll_line_on_page[0] == 20 &&
		state.scroll.delta == 1 && state.scroll.step_advanced == 0 &&
		state.stage_frame == 0 && state.resident_frame == 0 && state.frame.page_back == 1;
}

[[nodiscard]] bool test_prepare_interval_refusal_prefix() noexcept
{
	Stage1State state{};
	set_valid_pages(state);
	state.scroll.line = 200;
	state.scroll_line_on_page[1] = 37;
	state.scroll.step = 116;
	state.scroll.delta = 9;
	Trace trace{};
	auto invalidation = invalidation_presentation(trace);
	return stage1_prepare(state, invalidation, trace) == Stage1PreparationResult::scroll_interval_zero &&
		trace.tile_calls == 1 && trace.tile_line == 37 && state.midboss_active == 1 &&
		state.scroll.line == 200 && state.scroll.delta == 0 &&
		state.scroll_line_on_page[1] == 37;
}

[[nodiscard]] bool test_notifications() noexcept
{
	struct Notifications final : Stage1NotificationPresentation {
		std::vector<int> calls;
		void sound_effects_update() noexcept override { calls.push_back(1); }
		void stage_title_clear() noexcept override { calls.push_back(2); }
		void bgm_title_show(std::uint8_t title) noexcept override { calls.push_back(10 + title); }
		void bgm_title_clear() noexcept override { calls.push_back(3); }
	} sink;
	Stage1State state{};
	bool title_enabled = true;
	state.stage_frame = 160;
	state.bgm_show_timer = 1;
	state.bgm_title_id = 4;
	stage1_notifications(state, title_enabled, false, sink);
	if (title_enabled || state.bgm_show_timer != 2 || sink.calls != std::vector<int>({1, 2, 14})) return false;
	sink.calls.clear();
	state.bgm_show_timer = 159;
	stage1_notifications(state, title_enabled, false, sink);
	if (state.bgm_show_timer != 0 || sink.calls != std::vector<int>({1, 3})) return false;
	sink.calls.clear();
	title_enabled = true;
	state.bgm_show_timer = 255;
	stage1_notifications(state, title_enabled, true, sink);
	return !title_enabled && state.bgm_show_timer == 0 && sink.calls == std::vector<int>({1});
}

[[nodiscard]] bool test_setup_retains_and_sequences() noexcept
{
	Stage1State state{};
	state.random.cursor = 7;
	state.rank = 3;
	state.shottype = 1;
	state.frame.player.current = {90, 91};
	state.midboss.scenery_frame = 1002;
	state.midboss.defeat_frame = 9;
	state.rika.defeat_frame = 12;
	state.sparks.ring_index = 23;
	state.spark_parameters.acceleration_x = -1;
	state.resident_frame = 500;
	state.player_stream.stream_a_phase = 17;
	state.boss.target = {70, 80};
	Trace trace{};
	if (!stage1_setup_before_assets(state, false, trace)) return false;
	// Independent arithmetic from master.lib's LCG, 128 spark draws followed
	// by 256 ring draws. Literals catch swapped or omitted initializer calls.
	if (state.random.seed != 2418387584U || state.random.ring[0] != 180 ||
		state.random.ring[7] != 7 || state.random.ring[255] != 37 || state.random.cursor != 7) return false;
	if (state.frame.player.on_page[0] != ScreenPoint{208, 336} ||
		state.frame.player.current != ScreenPoint{90, 91} ||
		state.midboss.scenery_frame != 1002 || state.midboss.defeat_frame != 9 ||
		state.rika.defeat_frame != 12 || state.sparks.ring_index != 23 ||
		state.resident_frame != 500 || state.spark_parameters.acceleration_x != -1 ||
		state.spark_parameters.age_max != 48 || state.spark_sprite_interval != 1 ||
		state.bullets.rank_base_speed != 12 || state.bullets.stack != 1) return false;
	Stage1FrameSchedule schedule{};
	bool title_enabled = false;
	stage1_setup_after_assets(state, schedule, title_enabled);
	if (state.boss.target != ScreenPoint{-1, -1} || state.player_stream.stream_a_phase != 17 ||
		!title_enabled || !state.enemies_enabled || state.boss_enabled || state.lasers_enabled) return false;
	if (!stage1_setup_items_and_score(state, trace)) return false;
	return state.random.cursor == 8 && state.items.semirandom_ring_p == 7 &&
		state.midboss.scenery_frame == 1002 && state.resident_frame == 500;
}

[[nodiscard]] bool test_setup_options_and_demo() noexcept
{
	Stage1State state{};
	state.score.score = 123;
	state.items.bigpower_override = 8;
	gameplay_options_apply(state, true, 0);
	if (state.score.score != 123 || state.items.bigpower_override != 8 || state.playperf_max != 4) return false;
	state.stage_id = 4;
	state.rank = 1;
	gameplay_options_apply(state, true, 256);
	if (state.score.score != 9 || state.items.bigpower_override != 6 || state.playperf_max != 16) return false;
	Trace trace{};
	if (stage1_setup_before_assets(state, true, trace) || state.random.seed != 0) return false;
	state.stage_id = 0;
	state.bgm_title_id = 33;
	state.boss_bgm_title_id = 34;
	return stage1_setup_before_assets(state, true, trace) && state.bgm_show_timer == 0 &&
		state.bgm_title_id == 33 && state.boss_bgm_title_id == 34 &&
		state.spark_sprite_interval == 7 && state.spark_parameters.age_max == 24;
}

[[nodiscard]] bool test_connected_frame() noexcept
{
	// A minimal decoded empty-map fixture, not a replay or original asset set.
	// Every gameplay call below is concrete; only output sinks are inert.
	Stage1State state{};
	Stage1FrameSchedule schedule{};
	Trace trace{};
	bool title_enabled = false;
	state.max_enemy_vm_steps = 64;
	if (!stage1_setup_before_assets(state, false, trace)) return false;
	state.stage_data.map_length = 4;
	tiles_stuff_reset(state.tiles, state.frame.page_back, state.tile_mode, state.tiles_egc_render_all);
	stage1_setup_after_assets(state, schedule, title_enabled);
	if (!stage1_setup_items_and_score(state, trace)) return false;
	if (tiles_fill_and_put_initial(state.tiles, state.stage_data, state.tile_sections, trace) != TileScrollResult::completed) return false;
	state.frame.page_front = 1;
	state.frame.page_back = 0;
	auto invalidation = invalidation_presentation(trace);
	auto entities = entities_presentation(trace);
	if (stage1_prepare(state, invalidation, trace) != Stage1PreparationResult::input_pending) return false;
	state.input = 0;
	const auto player = player_move_and_bomb_update(state, trace, trace);
	if (!player.bomb_update_called || player.player_result != PlayerFrameResult::completed) return false;
	if (stage1_entities_update(state, entities).result != Stage1EntitiesResult::completed) return false;
	if (stage1_frame_finish(state, schedule, trace) != Stage1FramePhase::complete) return false;
	return state.stage_frame == 1 && state.resident_frame == 1 && state.scroll.line == 399 &&
		state.scroll.cycle == 1 && state.frame.page_front == 0 && state.frame.page_back == 1 &&
		state.frame.player.current == ScreenPoint{208, 336} && state.random.cursor == 1 &&
		state.item_lifecycle.frame_unused == 1;
}

[[nodiscard]] bool test_initial_tile_ring() noexcept
{
	Stage1State state{};
	Trace trace{};
	state.stage_data.map_length = 3;
	state.stage_data.map[0] = 0;
	state.stage_data.map[1] = 1;
	state.stage_data.map[2] = 2;
	state.tiles.tile_ring[24].fill(99);
	for (std::size_t section = 0; section < 3; ++section)
		for (std::size_t row = 0; row < 8; ++row)
			state.tile_sections[section][row].fill(static_cast<std::uint8_t>(section * 8 + row));
	if (tiles_fill_and_put_initial(state.tiles, state.stage_data, state.tile_sections, trace) != TileScrollResult::completed) return false;
	if (trace.initial_rows != 24 || trace.last_initial_top != 0 || state.tiles.tile_ring[24][0] != 99) return false;
	for (std::size_t row = 0; row < 24; ++row)
		if (state.tiles.tile_ring[row][0] != 23U - row) return false;
	state.stage_data.map[1] = 255;
	trace.initial_rows = 0;
	return tiles_fill_and_put_initial(state.tiles, state.stage_data, state.tile_sections, trace) ==
		TileScrollResult::invalid_native_reference && trace.initial_rows == 8;
}

} // namespace

int main()
{
	if(!test_invalidation_overload_borrows_one_state() ||
		!test_player_bomb_overload_uses_canonical_boss_and_frame() ||
		!test_entities_overload_builds_local_adapters() ||
		!test_enabled_enemy_rejects_invalid_projection_before_context() ||
		!test_prepare_old_page_and_activation() ||
		!test_prepare_interval_refusal_prefix() || !test_notifications() ||
		!test_setup_retains_and_sequences() || !test_setup_options_and_demo() ||
		!test_connected_frame() || !test_initial_tile_ring()) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
