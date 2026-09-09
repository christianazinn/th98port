#include "th02/core/stage1_state.hpp"

#include <cstdlib>

namespace {

using namespace th02::core;

class Trace final : public EnemyUpdateCallbacks, public RikaCallbacks,
	public Midboss1Callbacks, public EnemyInvalidationCallbacks,
	public PlayerFrameInvalidationCallbacks,
	public LaserInvalidationCallbacks, public ItemScheduleCallbacks,
	public SparkInvalidationCallbacks, public BombAnimationCallbacks,
	public BombPresentationCallbacks, public PlayerMissPresentationCallbacks,
	public ItemLifecyclePresentationCallbacks, public LaserUpdateCallbacks,
	public EnemyBulletPresentationCallbacks {
public:
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

} // namespace

int main()
{
	if(!test_invalidation_overload_borrows_one_state() ||
		!test_player_bomb_overload_uses_canonical_boss_and_frame() ||
		!test_entities_overload_builds_local_adapters() ||
		!test_enabled_enemy_rejects_invalid_projection_before_context()) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
