#include "th02/core/rika.hpp"

#include <bit>
#include <cstdint>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_top = 16;
constexpr std::int16_t playfield_width = 384;
constexpr std::int16_t rika_center_left =
	playfield_left + (playfield_width / 2) - 32;
constexpr std::int16_t rika_top = 48;
constexpr std::int16_t muzzle_top = playfield_top + 96;
constexpr std::int16_t defeat_spark_top = playfield_left + 96;
constexpr std::int16_t defeat_spark_speed = playfield_top + 34;
constexpr std::int16_t rika_patnum = 150;
constexpr std::int16_t rika_patnum_alt = 151;
constexpr std::int16_t rika_patnum_defeat_1 = 152;
constexpr std::int16_t rika_patnum_defeat_2 = 153;
constexpr std::uint8_t rank_hard = 2U;
constexpr std::uint8_t boss_defeat_invincibility_frames = 200U;

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

[[nodiscard]] std::int16_t subtract_word(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	return add_word(lhs, std::bit_cast<std::int16_t>(
		static_cast<std::uint16_t>(0U - std::bit_cast<std::uint16_t>(rhs))));
}

[[nodiscard]] std::int16_t shift_left_2_word(std::int16_t value) noexcept
{
	const auto bits = static_cast<std::uint16_t>(std::bit_cast<std::uint16_t>(value));
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits << 2U));
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

[[nodiscard]] RikaUpdateResult map_bullet_result(
	EnemyBulletAddResult result
) noexcept
{
	switch(result) {
	case EnemyBulletAddResult::completed:
		return RikaUpdateResult::boss;
	case EnemyBulletAddResult::invalid_page:
		return RikaUpdateResult::bullet_invalid_page;
	case EnemyBulletAddResult::source_iatan2_fault:
		return RikaUpdateResult::bullet_source_iatan2_fault;
	}
	return RikaUpdateResult::bullet_source_iatan2_fault;
}

[[nodiscard]] RikaUpdateResult add_pellet(RikaCombatContext &context,
	ScreenPoint origin, std::uint8_t angle, std::uint8_t group,
	std::int16_t speed) noexcept
{
	return map_bullet_result(bullets_add_pellet(context.bullets, context.random,
		context.player_frame, context.playperf, context.rank, context.bombing,
		origin, angle, group, speed));
}

[[nodiscard]] RikaUpdateResult add_ball(RikaCombatContext &context,
	ScreenPoint origin, std::uint8_t angle, std::uint8_t group,
	std::int16_t patnum, std::int16_t speed) noexcept
{
	return map_bullet_result(bullets_add_16x16(context.bullets, context.random,
		context.player_frame, context.playperf, context.rank, context.bombing,
		origin, angle, group, patnum, speed));
}

[[nodiscard]] RikaUpdateResult rika_move(BossState &boss, RikaState &rika,
	ScreenPoint &back_topleft, RikaCombatContext &context
) noexcept
{
	if(rika.move_state == 0) {
		if(boss.phase_frame < 16) {
			if((boss.phase_frame & 3) != 0) return RikaUpdateResult::boss;
			const auto angle = static_cast<std::uint8_t>(
				(context.rank >= static_cast<std::int8_t>(rank_hard))
				? static_cast<std::uint8_t>(boss.phase_frame) : 0U
			);
			return add_pellet(context, {228, 112}, angle,
				enemy_bullet_group::ring_16,
				add_word(shift_left_2_word(boss.phase_frame), 40));
		}
		if(boss.phase_frame < 116) {
			back_topleft.x = add_word(back_topleft.x, -1);
		} else if(boss.phase_frame < 316) {
			back_topleft.x = add_word(back_topleft.x, 1);
		} else {
			if(boss.phase_frame >= 416) boss.phase_frame = 16;
			back_topleft.x = add_word(back_topleft.x, -1);
		}
	} else if(rika.move_state == 1) {
		if(back_topleft.x < rika_center_left) {
			back_topleft.x = add_word(back_topleft.x, 1);
		} else if(boss.phase_frame > 192) {
			back_topleft.x = add_word(back_topleft.x, -1);
		} else {
			rika.move_state = 2;
		}
	} else if(rika.move_state == 2) {
		boss.phase_frame = 0;
	}
	return RikaUpdateResult::boss;
}

[[nodiscard]] RikaUpdateResult rika_pattern(BossState &boss, RikaState &rika,
	RikaCombatContext &context, RikaCallbacks &callbacks
) noexcept
{
	auto left = add_word(rika.topleft.x, 28);
	if(boss.damage < 700) {
		rika.move_state = 0;
		if((static_cast<std::uint16_t>(boss.phase_frame) & 0x3FU) != 0U) {
			return RikaUpdateResult::boss;
		}
		callbacks.sound_effect(3U);
		auto result = add_pellet(context, {left, muzzle_top}, 0U,
			boss.rank_param[0U], 50);
		if(result != RikaUpdateResult::boss) return result;
		return add_pellet(context, {left, muzzle_top}, 0x40U,
			boss.rank_param[1U], 30);
	}
	if(boss.damage < 1400) {
		if((context.stage_frame & 0x1FU) != 0U) return RikaUpdateResult::boss;
		rika.pattern_angle = static_cast<std::uint8_t>(
			rika.pattern_angle + 0x18U);
		rika.pattern_angle = static_cast<std::uint8_t>(rika.pattern_angle % 0x80U);
		left = add_word(rika.topleft.x, 8);
		callbacks.sound_effect(3U);
		auto result = add_pellet(context, {left, muzzle_top}, rika.pattern_angle,
			boss.rank_param[2U], 54);
		if(result != RikaUpdateResult::boss) return result;
		result = add_pellet(context, {left, muzzle_top}, rika.pattern_angle,
			boss.rank_param[3U], 44);
		if(result != RikaUpdateResult::boss) return result;
		left = add_word(left, 44);
		const auto mirrored_angle = static_cast<std::uint8_t>(
			0x80U - rika.pattern_angle);
		result = add_pellet(context, {left, muzzle_top}, mirrored_angle,
			boss.rank_param[2U], 54);
		if(result != RikaUpdateResult::boss) return result;
		return add_pellet(context, {left, muzzle_top}, mirrored_angle,
			boss.rank_param[3U], 44);
	}

	if(rika.move_state == 0) {
		rika.move_state = 1;
		rika.pattern_frame = 0U;
		rika.pattern_angle = 0U;
	}
	rika.pattern_frame = static_cast<std::uint16_t>(rika.pattern_frame + 1U);
	if(rika.pattern_frame < 100U) {
		boss.patnum = ((rika.pattern_frame & 7U) < 4U) ? rika_patnum :
			rika_patnum_alt;
		if((rika.pattern_frame & 0x1FU) == 0U) callbacks.sound_effect(9U);
		return RikaUpdateResult::boss;
	}
	if(rika.pattern_frame == 100U) {
		if((rika.pattern_frame & 0x1FU) == 0U) callbacks.sound_effect(9U);
		context.lasers.wait_frames = 0x40U;
		(void)lasers_add(context.lasers, {add_word(rika.topleft.x, 2), 112},
			160, 0x6F, callbacks);
		(void)lasers_add(context.lasers, {add_word(rika.topleft.x, 46), 112},
			160, 0x6F, callbacks);
		return RikaUpdateResult::boss;
	}
	if(rika.pattern_frame > 164U && rika.pattern_frame < 280U) {
		if(rika.pattern_frame < 250U) {
			boss.patnum = ((rika.pattern_frame & 7U) < 4U) ? rika_patnum :
				rika_patnum_alt;
		} else if(rika.pattern_frame == 260U) {
			boss.patnum = rika_patnum_defeat_1;
		} else if(rika.pattern_frame == 270U) {
			boss.patnum = rika_patnum_defeat_2;
		}
		if((rika.pattern_frame & 0x0FU) != 0U) return RikaUpdateResult::boss;
		callbacks.sound_effect(6U);
		const auto group = ((rika.pattern_frame & 0x3FU) == 0x10U ||
			(rika.pattern_frame & 0x3FU) == 0x30U)
			? enemy_bullet_group::spread_4_medium
			: enemy_bullet_group::spread_4_wide;
		return add_pellet(context, {left, muzzle_top}, 0x40U, group, 58);
	}
	if(rika.pattern_frame >= 280U &&
		((rika.pattern_frame & boss.rank_param[4U]) == 0U)) {
		return add_ball(context, {left, playfield_top + 56}, 0x01U,
			enemy_bullet_group::random_angle_and_speed, 19, 44);
	}
	return RikaUpdateResult::boss;
}

[[nodiscard]] RikaUpdateResult rika_hittest_and_put(BossState &boss,
	RikaState &rika, RikaCombatContext &context, RikaCallbacks &callbacks,
	bool &hit) noexcept
{
	PlayerShotSparkAllocatorCallbacks shot_sparks(context.sparks,
		context.player_frame.page_back);
	const auto damage = shots_hittest(context.shots, context.score,
		context.player_frame.page_back, context.shot_level, context.stage_frame,
		context.bombing, context.bomb_damage_frame_mask,
		{{rika.topleft.x, rika_top}, 64, 96}, shot_sparks);
	if(!damage) return RikaUpdateResult::invalid_shot_hittest;
	if(!shot_sparks.all_allocations_succeeded()) {
		return RikaUpdateResult::shot_spark_allocation_refused;
	}
	hit = (*damage != 0);
	if(!hit) return RikaUpdateResult::boss;
	boss.damage = add_word(boss.damage, *damage);
	if(boss.damage <= 2260) {
		callbacks.sound_effect(4U);
		callbacks.render_rika({RikaSpriteKind::hit_white, rika.topleft,
			boss.patnum, 0});
	} else {
		boss.phase = 1U;
		context.score.score_delta = add_dword(context.score.score_delta, 20000);
		context.player_hit.invincibility_time = boss_defeat_invincibility_frames;
	}
	return RikaUpdateResult::boss;
}

[[nodiscard]] RikaUpdateResult rika_defeat_update_and_render(
	BossState &boss, RikaState &rika, RikaCombatContext &context,
	RikaCallbacks &callbacks
) noexcept
{
	if(!sparks_add(context.sparks, context.player_frame.page_back,
		add_word(rika.topleft.x, 32), static_cast<std::uint16_t>(defeat_spark_top),
		defeat_spark_speed, 2, false)) {
		return RikaUpdateResult::defeat_spark_allocation_refused;
	}
	const auto zoom = add_word(10,
		static_cast<std::int16_t>(subtract_word(rika.defeat_frame, 32) / 8));
	rika.defeat_frame = add_word(rika.defeat_frame, 1);
	if(rika.defeat_frame >= 80) {
		rika.defeat_frame = 0;
		return RikaUpdateResult::stage_clear;
	}
	if(boss.phase_frame < 32) {
		callbacks.render_rika({RikaSpriteKind::normal, rika.topleft,
			boss.patnum, 0});
	} else {
		callbacks.render_rika({RikaSpriteKind::defeat_zoom,
			{rika.topleft.x, add_word(rika.topleft.y, 16)}, 2, zoom});
	}
	boss_explode_render({add_word(rika.topleft.x, 24), muzzle_top},
		rika.defeat_frame, boss.explode_angle_offset, context.scroll_line,
		context.slowdown_factor, context.palette_tone, callbacks);
	return RikaUpdateResult::boss;
}

} // namespace

RikaBackgroundResult rika_bg_render(BossState &boss, const RikaState &rika,
	const PlayerFrameState &frame, RikaCallbacks &callbacks) noexcept
{
	if(frame.page_back >= boss.topleft_on_page.size() ||
		frame.page_front >= boss.topleft_on_page.size()) {
		return RikaBackgroundResult::invalid_page;
	}
	const auto old_back_left = boss.topleft_on_page[frame.page_back].x;
	callbacks.invalidate_rika({{old_back_left, rika_top}, 64, 96});
	boss.topleft_on_page[frame.page_back].x =
		boss.topleft_on_page[frame.page_front].x;
	if(rika.move_state == 0 && boss.phase_frame < 3) {
		callbacks.invalidate_rika({{208, 16}, 32, 32});
	}
	return RikaBackgroundResult::completed;
}

RikaUpdateResult rika_update(BossState &boss, RikaState &rika,
	RikaCombatContext &context, RikaCallbacks &callbacks) noexcept
{
	boss.target = {add_word(rika.topleft.x, 24), rika_top};
	boss.phase_frame = add_word(boss.phase_frame, 1);
	if(boss.phase != 0U) {
		return rika_defeat_update_and_render(boss, rika, context, callbacks);
	}
	if(context.player_frame.page_back >= boss.topleft_on_page.size()) {
		return RikaUpdateResult::invalid_page;
	}
	auto &back_topleft = boss.topleft_on_page[context.player_frame.page_back];
	const auto moved = rika_move(boss, rika, back_topleft, context);
	if(moved != RikaUpdateResult::boss) return moved;
	rika.topleft.x = back_topleft.x;
	bool hit{};
	const auto hittest = rika_hittest_and_put(boss, rika, context, callbacks,
		hit);
	if(hittest != RikaUpdateResult::boss) return hittest;
	if(!hit) {
		callbacks.render_rika({RikaSpriteKind::normal, rika.topleft,
			boss.patnum, 0});
	}
	return rika_pattern(boss, rika, context, callbacks);
}

} // namespace th02::core
