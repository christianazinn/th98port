#include "th02/core/stage2_entities.hpp"
#include "th02/core/stage_state.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

using namespace th02::core;

void require(bool value,
	std::source_location at = std::source_location::current()
)
{
	if(!value) {
		std::fprintf(stderr, "stage2 entities: %u\n", at.line());
		std::abort();
	}
}

struct Sink final : EnemyUpdateCallbacks, MeiraCallbacks, Midboss2Callbacks,
	PlayerMissPresentationCallbacks, ItemLifecyclePresentationCallbacks,
	LaserUpdateCallbacks, EnemyBulletPresentationCallbacks {
	unsigned item_updates{};

	void sound_effect(std::uint8_t) noexcept override {}
	void tile_ring_set_and_put_both_8(
		std::int16_t, std::int16_t, std::int16_t
	) noexcept override {}
	void render_enemy(EnemySprite) noexcept override {}
	void render_enemy_hit_white(EnemySprite) noexcept override {}
	void invalidate_meira(MeiraInvalidation) noexcept override {}
	void render_meira(MeiraSprite) noexcept override {}
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
	void explosion_graph_scroll(
		std::uint16_t, std::uint16_t, std::uint16_t
	) noexcept override {}
	void invalidate_midboss(Midboss2Invalidation) noexcept override {}
	void render_midboss(Midboss2Sprite) noexcept override {}
	void hud_power(std::uint8_t, std::uint8_t) noexcept override {}
	void hud_lives(std::int8_t) noexcept override {}
	void hud_bombs(std::int8_t) noexcept override {}
	void render_player(PlayerMissSprite) noexcept override {}
	void pointnum_add(
		std::int16_t, std::int16_t, std::uint16_t
	) noexcept override {}
	void render_item(ItemSprite) noexcept override {}
	void pointnums_update_and_render() noexcept override { ++item_updates; }
	void render_laser(LaserPresentation) noexcept override {}
	void present(EnemyBulletPresentationEvent) noexcept override {}
};

[[nodiscard]] Stage2EntitiesPresentation presentation(Sink &sink) noexcept
{
	return {sink, sink, sink, sink, sink, sink, sink};
}

[[nodiscard]] StageState state_with_pages() noexcept
{
	StageState state{};
	state.frame.page_front = 0U;
	state.frame.page_back = 1U;
	state.frame.player.current = {300, 300};
	state.frame.player.on_page = {{{300, 300}, {300, 300}}};
	state.boss.topleft_on_page = {{{100, 48}, {100, 48}}};
	for(std::size_t i = 0U; i < state.random.ring.size(); ++i) {
		state.random.ring[i] = static_cast<std::uint8_t>(i);
	}
	return state;
}

void test_meira_target_and_private_persistence() noexcept
{
	auto state = state_with_pages();
	state.boss_enabled = true;
	state.meira.defeat_frame = 19;
	state.meira.slash_burst_i = 7U;
	Sink sink{};
	auto callbacks = presentation(sink);
	auto result = stage2_entities_update(state, callbacks);
	require(result.result == StageEntitiesResult::completed);
	require(result.meira == MeiraUpdateResult::boss);
	require(result.midboss == Midboss2UpdateResult::completed);
	require(state.stage_progression == StageProgression::boss);
	require(state.boss.phase_frame == 1);
	require(state.boss.target == ScreenPoint{140, 88});
	require(state.enemy_target_projection.boss_position == state.boss.target);
	require(state.meira.defeat_frame == 19 && state.meira.slash_burst_i == 7U);
	require(state.item_lifecycle.frame_unused == 1 && sink.item_updates == 1U);
}

void test_midboss_overwrites_boss_target_before_shared_tail() noexcept
{
	auto state = state_with_pages();
	state.midboss_active = -1;
	state.meira.dash_step = 31;
	Sink sink{};
	auto callbacks = presentation(sink);
	auto result = stage2_entities_update(state, callbacks);
	require(result.result == StageEntitiesResult::completed);
	require(result.midboss == Midboss2UpdateResult::completed);
	require(state.stage_progression == StageProgression::stage);
	require(state.midboss2.active == 1 && state.stage_bg_flash.frame == 1);
	require(state.boss.target == ScreenPoint{216, 72});
	require(state.enemy_target_projection.boss_position == state.boss.target);
	require(state.meira.dash_step == 31);
	require(state.item_lifecycle.frame_unused == 1 && sink.item_updates == 1U);
}

void test_meira_and_midboss_refusal_prefixes() noexcept
{
	{
		auto state = state_with_pages();
		state.boss_enabled = true;
		state.frame.page_back = 2U;
		state.boss.phase_frame = 4;
		state.boss.target = {50, 60};
		state.item_lifecycle.frame_unused = 9;
		Sink sink{};
		auto callbacks = presentation(sink);
		auto result = stage2_entities_update(state, callbacks);
		require(result.result == StageEntitiesResult::meira_refused);
		require(result.meira == MeiraUpdateResult::invalid_page);
		require(state.boss.phase_frame == 5);
		require(state.enemy_target_projection.boss_position == ScreenPoint{50, 60});
		require(state.midboss2.active == 0 && state.item_lifecycle.frame_unused == 9);
	}
	{
		auto state = state_with_pages();
		state.midboss_active = -1;
		state.frame.page_back = 2U;
		state.boss.target = {9, 17};
		state.item_lifecycle.frame_unused = 19;
		Sink sink{};
		auto callbacks = presentation(sink);
		auto result = stage2_entities_update(state, callbacks);
		require(result.result == StageEntitiesResult::midboss_refused);
		require(result.midboss == Midboss2UpdateResult::invalid_page);
		require(state.boss.target == ScreenPoint{216, 17});
		require(state.enemy_target_projection.boss_position == state.boss.target);
		require(state.item_lifecycle.frame_unused == 19);
	}
}

void test_enemy_refusal_stops_before_stage2_actors() noexcept
{
	auto state = state_with_pages();
	state.enemies_enabled = true;
	state.enemies.enemies_loop_bound = 26U;
	state.boss_enabled = true;
	state.boss.phase_frame = 7;
	state.boss.target = {50, 60};
	state.item_lifecycle.frame_unused = 23;
	Sink sink{};
	auto callbacks = presentation(sink);
	auto result = stage2_entities_update(state, callbacks);
	require(result.result == StageEntitiesResult::enemy_refused);
	require(result.enemy == EnemyUpdateResult::invalid_loop_bound);
	require(state.boss.target == ScreenPoint{-1, -1});
	require(state.enemy_target_projection.boss_position == state.boss.target);
	require(state.boss.phase_frame == 7 && state.item_lifecycle.frame_unused == 23);
}

} // namespace

int main()
{
	test_meira_target_and_private_persistence();
	test_midboss_overwrites_boss_target_before_shared_tail();
	test_meira_and_midboss_refusal_prefixes();
	test_enemy_refusal_stops_before_stage2_actors();
}
