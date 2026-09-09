#include "th02/core/midboss1.hpp"

#include <bit>
#include <cstdint>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_height = 400;
constexpr std::int16_t midboss_width = 64;
constexpr std::int16_t midboss_height = 96;
constexpr std::int16_t defeat_width = 96;
constexpr std::int16_t defeat_left_offset = -16;
constexpr std::int16_t midboss_patnum = 148;
constexpr std::int16_t midboss_patnum_alt = 149;

[[nodiscard]] std::int16_t add_word(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int32_t add_dword(
	std::int32_t lhs, std::int32_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint32_t>(
		static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(lhs)) +
		static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(rhs))
	);
	return std::bit_cast<std::int32_t>(bits);
}

[[nodiscard]] Midboss1UpdateResult map_bullet_result(
	EnemyBulletAddResult result
) noexcept
{
	switch(result) {
	case EnemyBulletAddResult::completed:
		return Midboss1UpdateResult::completed;
	case EnemyBulletAddResult::invalid_page:
		return Midboss1UpdateResult::bullet_invalid_page;
	case EnemyBulletAddResult::source_iatan2_fault:
		return Midboss1UpdateResult::bullet_source_iatan2_fault;
	}
	return Midboss1UpdateResult::bullet_source_iatan2_fault;
}

[[nodiscard]] std::int16_t scroll_top(
	std::int16_t top, std::int16_t scroll_line, bool wrap_negative
) noexcept
{
	top = add_word(top, scroll_line);
	if(wrap_negative && top < 0) {
		top = add_word(top, playfield_height);
	} else if(top >= playfield_height) {
		top = add_word(top, -playfield_height);
	}
	return top;
}

[[nodiscard]] Midboss1UpdateResult fire_pattern(
	const BossState &boss, ScreenPoint back_topleft,
	Midboss1Context &context
) noexcept
{
	const auto phase = static_cast<std::uint16_t>(boss.phase_frame) & 0x7FU;
	std::uint8_t group{};
	std::int16_t speed{};
	if(phase == 0U || phase == 96U) {
		group = enemy_bullet_group::spread_4_wide_aimed;
		speed = 50;
	} else if(phase == 48U) {
		group = enemy_bullet_group::spread_3_medium_aimed;
		speed = 30;
	} else {
		return Midboss1UpdateResult::completed;
	}
	return map_bullet_result(bullets_add_pellet(context.bullets, context.random,
		context.player_frame, context.playperf, context.rank, context.bombing,
		{add_word(back_topleft.x, 28), add_word(back_topleft.y, 88)}, 0U,
		group, speed));
}

[[nodiscard]] Midboss1UpdateResult defeat_update_and_render(
	BossState &boss, Midboss1State &state, ScreenPoint &back_topleft,
	Midboss1Context &context, Midboss1Callbacks &callbacks
) noexcept
{
	const auto zoom = add_word(10, static_cast<std::int16_t>(state.defeat_frame >> 3));
	state.defeat_frame = add_word(state.defeat_frame, 1);
	if(!sparks_add(context.sparks, context.player_frame.page_back,
		add_word(back_topleft.x, 32),
		static_cast<std::uint16_t>(add_word(back_topleft.y, 48)), 60, 1, false)) {
		return Midboss1UpdateResult::defeat_spark_allocation_refused;
	}
	if(state.defeat_frame >= 64) {
		if(!sparks_add(context.sparks, context.player_frame.page_back,
			add_word(back_topleft.x, 32),
			static_cast<std::uint16_t>(add_word(back_topleft.y, 48)), 128, 48, true)) {
			return Midboss1UpdateResult::defeat_spark_allocation_refused;
		}
		state.defeat_frame = 0;
		boss.phase_frame = 0;
		boss.target = {-1, -1};
		state.active = 0;
		return Midboss1UpdateResult::completed;
	}
	if((state.defeat_frame & 3) == 0) {
		back_topleft.y = add_word(back_topleft.y, 1);
	}
	callbacks.render_midboss({Midboss1SpriteKind::defeat_zoom,
		{add_word(back_topleft.x, defeat_left_offset),
			scroll_top(back_topleft.y, context.scroll_line, false)}, 3, zoom});
	return Midboss1UpdateResult::completed;
}

} // namespace

Midboss1InvalidateResult midboss1_invalidate(
	BossState &boss, const PlayerFrameState &frame,
	const Midboss1State &state, Midboss1Callbacks &callbacks
) noexcept
{
	if(frame.page_back >= boss.topleft_on_page.size() ||
		frame.page_front >= boss.topleft_on_page.size()) {
		return Midboss1InvalidateResult::invalid_page;
	}
	const auto old_back = boss.topleft_on_page[frame.page_back];
	callbacks.invalidate_midboss({
		{add_word(old_back.x, (boss.phase != 0U) ? defeat_left_offset : 0),
			old_back.y},
		(boss.phase != 0U) ? defeat_width : midboss_width, midboss_height,
	});
	boss.topleft_on_page[frame.page_back] = boss.topleft_on_page[frame.page_front];
	return (state.active != 0) ? Midboss1InvalidateResult::active :
		Midboss1InvalidateResult::inactive;
}

Midboss1UpdateResult midboss1_update_and_render(
	BossState &boss, Midboss1State &state, Midboss1Context &context,
	Midboss1Callbacks &callbacks
) noexcept
{
	boss.target = {216, 64};
	boss.phase_frame = add_word(boss.phase_frame, 1);
	if(boss.phase_frame == 1) {
		boss.topleft_on_page[0U] = {192, -16};
		boss.topleft_on_page[1U] = {192, -16};
		boss.damage = 0;
		state.patnum = midboss_patnum;
		state.active = 1;
		state.scenery_frame = 1;
		return Midboss1UpdateResult::completed;
	}
	if(context.player_frame.page_back >= boss.topleft_on_page.size()) {
		return Midboss1UpdateResult::invalid_page;
	}
	auto &back_topleft = boss.topleft_on_page[context.player_frame.page_back];

	std::int16_t scrolled_y{};
	if(boss.phase_frame < 18) {
		back_topleft.y = add_word(back_topleft.y, 2);
		scrolled_y = scroll_top(back_topleft.y, context.scroll_line, true);
	} else {
		state.patnum = ((context.scroll_line & 1) != 0) ? midboss_patnum_alt :
			midboss_patnum;
		if(boss.phase != 0U) {
			return defeat_update_and_render(boss, state, back_topleft, context,
				callbacks);
		}

		const auto fired = fire_pattern(boss, back_topleft, context);
		if(fired != Midboss1UpdateResult::completed) return fired;
		if((context.scroll_line & 0x0F) == 0) {
			auto tile_top = scroll_top(48, context.scroll_line, false);
			for(std::int16_t i = 0; i < 4; ++i) {
				callbacks.tile_ring_set_and_put_both_8(
					add_word(back_topleft.x, static_cast<std::int16_t>(i * 16)),
					tile_top, add_word(60, i));
			}
		}
		scrolled_y = scroll_top(back_topleft.y, context.scroll_line, false);
		PlayerShotSparkAllocatorCallbacks shot_sparks(context.sparks,
			context.player_frame.page_back);
		const auto damage = shots_hittest(context.shots, context.score,
			context.player_frame.page_back, context.shot_level, context.stage_frame,
			context.bombing, context.bomb_damage_frame_mask,
			{{back_topleft.x, back_topleft.y}, midboss_width, midboss_height},
			shot_sparks);
		if(!damage) return Midboss1UpdateResult::invalid_shot_hittest;
		if(!shot_sparks.all_allocations_succeeded()) {
			return Midboss1UpdateResult::shot_spark_allocation_refused;
		}
		if(*damage != 0) {
			boss.damage = add_word(boss.damage, *damage);
			if(boss.damage <= 300 || scrolled_y >= 304) {
				callbacks.sound_effect(4U);
				callbacks.render_midboss({Midboss1SpriteKind::hit_white,
					{back_topleft.x, scrolled_y}, midboss_patnum, 0});
			} else {
				callbacks.sound_effect(2U);
				boss.phase = 1U;
				context.score.score_delta = add_dword(context.score.score_delta, 10000);
			}
			return Midboss1UpdateResult::completed;
		}
		if(context.scroll_step >= 184 && back_topleft.y <= 304) {
			callbacks.sound_effect(2U);
			boss.phase = 1U;
			return Midboss1UpdateResult::completed;
		}
	}

	callbacks.render_midboss({Midboss1SpriteKind::normal,
		{back_topleft.x, scrolled_y}, state.patnum, 0});
	return Midboss1UpdateResult::completed;
}

} // namespace th02::core
