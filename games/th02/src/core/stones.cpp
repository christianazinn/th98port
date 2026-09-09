#include "th02/core/stones.hpp"

#include "pc98/masterlib_iatan2.hpp"

#include <bit>
#include <cstdint>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_top = 16;
constexpr std::int16_t playfield_width = 384;
constexpr std::int16_t screen_height = 400;
constexpr std::int16_t stone_width = 32;
constexpr std::int16_t stone_hit_height = 40;
constexpr std::int16_t stone_patnum_rest = 148;
constexpr std::int16_t stone_patnum_inner_active = 155;
constexpr std::int16_t stone_patnum_outer_transition = 152;
constexpr std::int16_t stone_patnum_outer_active = 159;
constexpr std::int16_t stone_patnum_north_transition = 160;
constexpr std::int16_t stone_patnum_north_active = 163;
constexpr std::int16_t bullet_patnum_red = 81;
constexpr std::int16_t bullet_patnum_green = 82;
constexpr std::int16_t bullet_patnum_ball = 86;
constexpr std::uint8_t boss_defeat_invincibility_frames = 200U;
constexpr std::int16_t stones_laser_mirror = 432;

[[nodiscard]] std::int16_t word_add(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t word_subtract(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	return word_add(lhs, std::bit_cast<std::int16_t>(
		static_cast<std::uint16_t>(0U - std::bit_cast<std::uint16_t>(rhs))));
}

[[nodiscard]] std::int32_t dword_add(
	std::int32_t lhs, std::int32_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint32_t>(
		static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(lhs)) +
		static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(rhs))
	);
	return std::bit_cast<std::int32_t>(bits);
}

[[nodiscard]] std::int16_t arithmetic_shift_right(
	std::int16_t value, std::uint8_t count
) noexcept
{
	auto bits = static_cast<std::uint16_t>(
		std::bit_cast<std::uint16_t>(value) >> count);
	if(value < 0) {
		bits = static_cast<std::uint16_t>(bits |
			(static_cast<std::uint16_t>(0xFFFFU) << (16U - count)));
	}
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t scroll_y(std::int16_t top,
	std::int16_t scroll_line
) noexcept
{
	top = word_add(top, scroll_line);
	if(top >= screen_height) top = word_add(top, -screen_height);
	return top;
}

[[nodiscard]] bool is_flag(const StonesState &stones, std::size_t stone,
	StoneFlag value) noexcept
{
	return stones.flag[stone] == static_cast<std::uint8_t>(value);
}

void set_flag(StonesState &stones, std::size_t stone, StoneFlag value) noexcept
{
	stones.flag[stone] = static_cast<std::uint8_t>(value);
}

[[nodiscard]] StonesUpdateResult map_bullet_result(
	EnemyBulletAddResult result
) noexcept
{
	switch(result) {
	case EnemyBulletAddResult::completed:
		return StonesUpdateResult::boss;
	case EnemyBulletAddResult::invalid_page:
		return StonesUpdateResult::bullet_invalid_page;
	case EnemyBulletAddResult::source_iatan2_fault:
		return StonesUpdateResult::bullet_source_iatan2_fault;
	}
	return StonesUpdateResult::bullet_source_iatan2_fault;
}

[[nodiscard]] StonesUpdateResult add_pellet(StonesCombatContext &context,
	ScreenPoint origin, std::uint8_t angle, std::uint8_t group,
	std::int16_t speed
) noexcept
{
	return map_bullet_result(bullets_add_pellet(context.bullets, context.random,
		context.player_frame, context.playperf, context.rank, context.bombing,
		origin, angle, group, speed));
}

[[nodiscard]] StonesUpdateResult add_ball(StonesCombatContext &context,
	ScreenPoint origin, std::uint8_t angle, std::uint8_t group,
	std::int16_t patnum, std::int16_t speed
) noexcept
{
	return map_bullet_result(bullets_add_16x16(context.bullets, context.random,
		context.player_frame, context.playperf, context.rank, context.bombing,
		origin, angle, group, patnum, speed));
}

void render_living_stones(StonesState &stones,
	StonesCombatContext &context, StonesCallbacks &callbacks
) noexcept
{
	for(std::size_t stone = 0U; stone < stone_count; ++stone) {
		if(stones.flag[stone] >= static_cast<std::uint8_t>(StoneFlag::kill_anim)) {
			continue;
		}
		const auto topleft = ScreenPoint{stones.topleft[stone].x,
			scroll_y(stones.topleft[stone].y, context.scroll_line)};
		if(stones.hit_flash[stone] != 0U) {
			callbacks.sound_effect(4U);
			callbacks.render_stone({StonesSpriteKind::hit_white, stone, topleft,
				stones.patnum[stone]});
			stones.hit_flash[stone] = 0U;
		} else {
			callbacks.render_stone({StonesSpriteKind::normal, stone, topleft,
				stones.patnum[stone]});
		}
	}
}

[[nodiscard]] StonesUpdateResult kill_stone(StonesState &stones,
	std::size_t stone, BossState &boss, StonesCombatContext &context,
	StonesCallbacks &callbacks
) noexcept
{
	if(!sparks_add(context.sparks, context.player_frame.page_back,
		word_add(stones.topleft[stone].x, 16),
		static_cast<std::uint16_t>(word_add(stones.topleft[stone].y, 12)),
		80, 2, false)) {
		return StonesUpdateResult::kill_spark_allocation_refused;
	}

	auto kill_frames = static_cast<std::int16_t>(64);
	const auto topleft = ScreenPoint{stones.topleft[stone].x,
		scroll_y(stones.topleft[stone].y, context.scroll_line)};
	auto patnum = static_cast<std::int16_t>(10);
	if(stone == stone_north) {
		kill_frames = word_add(kill_frames, 32);
		const auto center = ScreenPoint{word_add(stones.topleft[stone_north].x, 8),
			word_add(stones.topleft[stone_north].y, 8)};
		boss_explode_render(center, stones.kill_frame[stone_north],
			boss.explode_angle_offset, context.scroll_line, context.slowdown_factor,
			context.palette_tone, callbacks);
		if(stones.kill_frame[stone_north] >= 24) {
			boss_explode_render(center,
				word_subtract(stones.kill_frame[stone_north], 24),
				boss.explode_angle_offset, context.scroll_line,
				context.slowdown_factor, context.palette_tone, callbacks);
		}
		if(stones.kill_frame[stone_north] < 32) {
			callbacks.render_stone({StonesSpriteKind::normal, stone_north, topleft,
				stones.patnum[stone_north]});
			stones.kill_frame[stone_north] = word_add(
				stones.kill_frame[stone_north], 1);
			return StonesUpdateResult::boss;
		}
		if(stones.kill_frame[stone_north] == 56) {
			boss.explode_angle_offset = 0xE0U;
		}
		patnum = word_add(patnum, static_cast<std::int16_t>(
			word_subtract(stones.kill_frame[stone_north], 32) / 8));
	} else {
		patnum = word_add(patnum, static_cast<std::int16_t>(
			stones.kill_frame[stone] / 8));
	}

	stones.kill_frame[stone] = word_add(stones.kill_frame[stone], 1);
	if(stones.kill_frame[stone] >= kill_frames) {
		stones.kill_frame[stone] = 0;
		set_flag(stones, stone, StoneFlag::remove);
		return StonesUpdateResult::boss;
	}
	callbacks.render_stone({StonesSpriteKind::kill, stone, topleft, patnum});
	return StonesUpdateResult::boss;
}

[[nodiscard]] StonesUpdateResult stones_housekeeping(BossState &boss,
	StonesState &stones, StonesCombatContext &context,
	StonesCallbacks &callbacks
) noexcept
{
	stones.phase_frame_unused = word_add(stones.phase_frame_unused, 1);
	for(std::size_t stone = 0U; stone < stone_count; ++stone) {
		if(stones.flag[stone] > static_cast<std::uint8_t>(StoneFlag::active)) {
			continue;
		}
		PlayerShotSparkAllocatorCallbacks shot_sparks(context.sparks,
			context.player_frame.page_back);
		const auto damage = shots_hittest(context.shots, context.score,
			context.player_frame.page_back, context.shot_level, context.stage_frame,
			context.bombing, context.bomb_damage_frame_mask,
			{{stones.topleft[stone].x, stones.topleft[stone].y}, stone_width,
				stone_hit_height}, shot_sparks);
		if(!damage) return StonesUpdateResult::invalid_shot_hittest;
		if(!shot_sparks.all_allocations_succeeded()) {
			return StonesUpdateResult::shot_spark_allocation_refused;
		}
		if((*damage == 0) || !is_flag(stones, stone, StoneFlag::active)) continue;

		stones.hit_flash[stone] = 1U;
		stones.damage[stone] = word_add(stones.damage[stone], *damage);
		const auto damage_max = (stone == stone_north) ? 930 :
			((stone <= stone_inner_east) ? 140 : 120);
		if(stones.damage[stone] < damage_max) continue;

		set_flag(stones, stone, StoneFlag::kill_anim);
		context.score.score_delta = dword_add(context.score.score_delta, 30000);
		if(stone <= stone_outer_east) {
			const auto type = (stone == stone_inner_west)
				? static_cast<std::int16_t>(ItemType::bomb)
				: ((stone <= static_cast<std::size_t>(ItemType::bomb))
					? static_cast<std::int16_t>(ItemType::big_power)
					: static_cast<std::int16_t>(ItemType::bomb));
			items_add(context.items, context.score,
				word_add(stones.topleft[stone].x, 8),
				word_add(stones.topleft[stone].y, 12), type);
			callbacks.sound_effect(2U);
		} else {
			context.player_hit.invincibility_time = boss_defeat_invincibility_frames;
		}
	}

	for(std::size_t stone = 0U; stone < stone_count; ++stone) {
		if(!is_flag(stones, stone, StoneFlag::active)) continue;
		boss.target = {word_add(stones.topleft[stone].x, 8),
			word_add(stones.topleft[stone].y, 8)};
		break;
	}
	if(context.player_frame.player.current.y < 96) {
		const auto angle = static_cast<std::uint8_t>(static_cast<std::int16_t>(
			randring_next8(context.random) & 0x0FU) - 7);
		return add_pellet(context, stones.muzzle, angle,
			enemy_bullet_group::one_aimed, 77);
	}
	return StonesUpdateResult::boss;
}

[[nodiscard]] StonesUpdateResult phase_1_pattern(BossState &boss,
	const StonesState &stones, StonesCombatContext &context
) noexcept
{
	if(boss.phase_frame < 30) return StonesUpdateResult::boss;
	if(boss.phase_frame < 180) {
		if((boss.phase_frame % 24) != 0) return StonesUpdateResult::boss;
		for(std::size_t stone = stone_inner_west; stone <= stone_inner_east;
			++stone) {
			if(!is_flag(stones, stone, StoneFlag::active)) continue;
			const auto result = add_pellet(context,
				{word_add(stones.topleft[stone].x, 12),
					word_add(stones.topleft[stone].y, 8)}, 0U,
				boss.rank_param[0U], 38);
			if(result != StonesUpdateResult::boss) return result;
		}
		return StonesUpdateResult::boss;
	}
	if(boss.phase_frame < 200) {
		return StonesUpdateResult::boss;
	}
	if(boss.phase_frame >= 310) {
		if(boss.phase_frame > 350) {
			boss.phase_frame = 0;
		}
		return StonesUpdateResult::boss;
	}
	if((static_cast<std::uint16_t>(boss.phase_frame) & 0x0FU) != 0U) {
		return StonesUpdateResult::boss;
	}
	for(std::size_t stone = stone_inner_west; stone < stone_outer_west; ++stone) {
		if(!is_flag(stones, stone, StoneFlag::active)) continue;
		const auto result = add_ball(context,
			{word_add(stones.topleft[stone].x, 12),
				word_add(stones.topleft[stone].y, 8)}, 0U,
			enemy_bullet_group::one_aimed, bullet_patnum_red, 80);
		if(result != StonesUpdateResult::boss) return result;
	}
	return StonesUpdateResult::boss;
}

[[nodiscard]] StonesUpdateResult phase_2_pattern(const BossState &boss,
	const StonesState &stones, StonesCombatContext &context, int removed
) noexcept
{
	if(boss.phase_frame < 30) return StonesUpdateResult::boss;
	const auto interval = static_cast<std::int16_t>(
		((boss.phase_frame < 200) ? 110 : 90) - (removed * 20));
	if((boss.phase_frame % interval) != 0) return StonesUpdateResult::boss;
	for(std::size_t stone = stone_outer_west; stone < stone_north; ++stone) {
		if(!is_flag(stones, stone, StoneFlag::active)) continue;
		for(std::uint8_t i = 0U; i < 6U; ++i) {
			const auto result = add_ball(context,
				{word_add(stones.topleft[stone].x, 12),
					word_add(stones.topleft[stone].y, 8)},
				static_cast<std::uint8_t>(i * 0x2AU),
				enemy_bullet_special_motion::decelerate_then_turn_aimed,
				bullet_patnum_green, 56);
			if(result != StonesUpdateResult::boss) return result;
		}
	}
	return StonesUpdateResult::boss;
}

void phase_4_lasers(BossState &boss, StonesCombatContext &context,
	StonesCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame < 50) return;
	if(boss.phase_frame == 50) {
		context.lasers.wait_frames = 36U;
		(void)lasers_add(context.lasers, {48, 96}, 16, 103, callbacks);
		(void)lasers_add(context.lasers, {192, 96}, 16, 103, callbacks);
		(void)lasers_add(context.lasers, {240, 96}, 16, 103, callbacks);
		(void)lasers_add(context.lasers, {384, 96}, 16, 103, callbacks);
		return;
	}
	if(boss.phase_frame == 80) {
		(void)lasers_add(context.lasers, {80, 96}, 16, 103, callbacks);
		(void)lasers_add(context.lasers, {160, 96}, 16, 103, callbacks);
		(void)lasers_add(context.lasers, {272, 96}, 16, 103, callbacks);
		(void)lasers_add(context.lasers, {352, 96}, 16, 103, callbacks);
		boss.phase_frame = 0;
	}
}

[[nodiscard]] StonesUpdateResult swept_pellets(BossState &boss,
	StonesState &stones, StonesCombatContext &context
) noexcept
{
	if((static_cast<std::uint16_t>(boss.phase_frame) & 3U) != 0U) {
		return StonesUpdateResult::boss;
	}
	const auto result = add_pellet(context, stones.muzzle, stones.sweep_angle,
		enemy_bullet_group::spread_2_horizontally_symmetric, 70);
	if(result != StonesUpdateResult::boss) return result;
	stones.sweep_angle = static_cast<std::uint8_t>(stones.sweep_angle + 8U);
	if(stones.sweep_angle > 0x82U) {
		boss.phase_frame = 0;
		stones.sweep_angle = 0U;
	}
	return StonesUpdateResult::boss;
}

void phase_4_player_laser(BossState &boss, StonesCombatContext &context,
	StonesCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame < 24) return;
	const auto interval = static_cast<std::int16_t>(
		28 - (static_cast<std::int16_t>(context.rank) * 3));
	if((boss.phase_frame % interval) == 0) {
		context.lasers.wait_frames = 30U;
		const auto player_column = static_cast<std::uint16_t>(
			std::bit_cast<std::uint16_t>(word_add(
				context.player_frame.player.current.x, 16)) & 0xFFF0U);
		const auto left = std::bit_cast<std::int16_t>(player_column);
		(void)lasers_add(context.lasers, {left, 96}, 16, 103, callbacks);
	}
	if(boss.phase_frame >= 140) boss.phase_frame = 0;
}

[[nodiscard]] StonesUpdateResult phase_8_converging_pellets(BossState &boss,
	const StonesState &stones, StonesCombatContext &context
) noexcept
{
	if(boss.phase_frame >= 102 || (boss.phase_frame % 6) != 0) {
		if(boss.phase_frame >= 102) boss.phase_frame = 0;
		return StonesUpdateResult::boss;
	}
	const auto aimed = pc98::masterlib::iatan2_source(
		word_subtract(context.player_frame.player.current.y, 32),
		word_subtract(context.player_frame.player.current.x,
			stones.topleft[stone_north].x));
	if(!aimed) return StonesUpdateResult::source_iatan2_fault;
	const auto delta = (boss.phase_frame < 40)
		? static_cast<std::uint8_t>((randring_next8(context.random) & 0x1FU) + 0x10U)
		: static_cast<std::uint8_t>(0x5F - boss.phase_frame);
	auto result = add_pellet(context, stones.muzzle,
		static_cast<std::uint8_t>(*aimed + delta), enemy_bullet_group::one, 80);
	if(result != StonesUpdateResult::boss) return result;
	return add_pellet(context, stones.muzzle,
		static_cast<std::uint8_t>(*aimed - delta), enemy_bullet_group::one, 80);
}

void phase_8_laser_wall(BossState &boss, StonesState &stones,
	StonesCombatContext &context, StonesCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame < 16) return;
	if(boss.phase_frame == 16) {
		stones.laser_left = 32;
		stones.laser_return_delay = 0U;
	}
	if((stones.laser_return_delay == 0U) &&
		((static_cast<std::uint16_t>(boss.phase_frame) & 7U) == 0U)) {
		context.lasers.wait_frames = 16U;
		(void)lasers_add(context.lasers, {stones.laser_left, 96}, 3, 111, callbacks);
		(void)lasers_add(context.lasers,
			{word_subtract(stones_laser_mirror, stones.laser_left), 96}, 3, 111,
			callbacks);
		stones.laser_left = word_add(stones.laser_left, 16);
		if(stones.laser_left > 176) {
			stones.laser_return_delay = 1U;
			stones.laser_left = word_add(stones.laser_left, 16);
		}
	}
	if(stones.laser_return_delay > 0x24U) {
		if((static_cast<std::uint16_t>(boss.phase_frame) & 7U) == 0U) {
			(void)lasers_add(context.lasers, {stones.laser_left, 96}, 1, 111,
				callbacks);
			(void)lasers_add(context.lasers,
				{word_subtract(stones_laser_mirror, stones.laser_left), 96}, 1, 111,
				callbacks);
			stones.laser_left = word_subtract(stones.laser_left, 16);
			if(stones.laser_left < 64) {
				stones.laser_return_delay = 0U;
				boss.phase_frame = 0;
				stones.laser_left = 32;
			}
		}
	} else if(stones.laser_return_delay != 0U) {
		stones.laser_return_delay = static_cast<std::uint8_t>(
			stones.laser_return_delay + 1U);
	}
}

[[nodiscard]] StonesUpdateResult gravity_balls(BossState &boss,
	const StonesState &stones, StonesCombatContext &context
) noexcept
{
	if((static_cast<std::uint16_t>(boss.phase_frame) & 0x0FU) == 0U) {
		for(std::size_t stone = boss.rank_param[1U]; stone < stone_north; ++stone) {
			const auto result = add_ball(context, stones.topleft[stone],
				randring_next8(context.random), enemy_bullet_special_motion::gravity,
				bullet_patnum_green, 32);
			if(result != StonesUpdateResult::boss) return result;
		}
	}
	if(boss.phase_frame > 110) boss.phase_frame = 0;
	return StonesUpdateResult::boss;
}

[[nodiscard]] StonesUpdateResult random_pellet(BossState &boss,
	const StonesState &stones, StonesCombatContext &context
) noexcept
{
	if((static_cast<std::uint16_t>(boss.phase_frame) & 3U) == 0U) {
		const auto speed = word_add(arithmetic_shift_right(boss.phase_frame, 1U),
			30);
		const auto result = add_pellet(context, stones.muzzle,
			randring_next8(context.random), boss.rank_param[3U], speed);
		if(result != StonesUpdateResult::boss) return result;
	}
	if(boss.phase_frame > 130) boss.phase_frame = 0;
	return StonesUpdateResult::boss;
}

[[nodiscard]] StonesUpdateResult random_lasers(BossState &boss,
	const StonesState &stones, StonesCombatContext &context,
	StonesCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame < 16) return StonesUpdateResult::boss;
	if(boss.phase_frame < 80) {
		if((boss.phase_frame % 36) == 0) {
			context.lasers.wait_frames = 30U;
			const auto col = static_cast<std::int16_t>(
				(randring_next8(context.random) % 24U) + 2U);
			(void)lasers_add(context.lasers, {static_cast<std::int16_t>(col << 4U), 96},
				12, 111, callbacks);
		}
		if(((boss.phase_frame % 40) == 0) && (context.rank != 0)) {
			return add_pellet(context, stones.muzzle, 0U,
				enemy_bullet_group::one_aimed, 70);
		}
	} else if(boss.phase_frame < 120) {
		if(boss.phase_frame == 100) {
			context.lasers.wait_frames = 30U;
			for(std::size_t i = 0U; i < 3U; ++i) {
				const auto col = static_cast<std::int16_t>(
					(randring_next8(context.random) % 24U) + 2U);
				(void)lasers_add(context.lasers,
					{static_cast<std::int16_t>(col << 4U), 96}, 12, 111, callbacks);
			}
		}
	} else {
		boss.phase_frame = 0;
	}
	return StonesUpdateResult::boss;
}

[[nodiscard]] StonesUpdateResult phase_6_mirrored_balls(BossState &boss,
	StonesState &stones, StonesCombatContext &context
) noexcept
{
	if(boss.phase_frame < 16) return StonesUpdateResult::boss;
	if(boss.phase_frame == 16) stones.phase6_pair_angle = 0x29U;
	if((boss.phase_frame % 30) == 0) {
		for(std::size_t stone = 0U; stone < stone_north; ++stone) {
			auto result = add_ball(context, stones.topleft[stone],
				stones.phase6_pair_angle, enemy_bullet_group::one_aimed,
				bullet_patnum_green, 60);
			if(result != StonesUpdateResult::boss) return result;
			result = add_ball(context, stones.topleft[stone],
				static_cast<std::uint8_t>(0U - stones.phase6_pair_angle),
				enemy_bullet_group::one_aimed, bullet_patnum_green, 60);
			if(result != StonesUpdateResult::boss) return result;
		}
		stones.phase6_pair_angle = static_cast<std::uint8_t>(
			stones.phase6_pair_angle - 3U);
	}
	if(stones.phase6_pair_angle >= 0xC8U) boss.phase_frame = 0;
	return StonesUpdateResult::boss;
}

[[nodiscard]] StonesUpdateResult phase_6_mirrored_pellets(BossState &boss,
	StonesState &stones, StonesCombatContext &context
) noexcept
{
	if(boss.phase_frame == 2) stones.phase6_pellet_angle = 0x3EU;
	if((static_cast<std::uint16_t>(boss.phase_frame) & 7U) != 0U) {
		return StonesUpdateResult::boss;
	}
	auto result = add_pellet(context, stones.muzzle,
		static_cast<std::uint8_t>(0U - stones.phase6_pellet_angle),
		boss.rank_param[2U], 64);
	if(result != StonesUpdateResult::boss) return result;
	result = add_pellet(context, stones.muzzle, stones.phase6_pellet_angle,
		boss.rank_param[2U], 64);
	if(result != StonesUpdateResult::boss) return result;
	stones.phase6_pellet_angle = static_cast<std::uint8_t>(
		stones.phase6_pellet_angle - 3U);
	if(stones.phase6_pellet_angle > 0xC8U) boss.phase_frame = 0;
	return StonesUpdateResult::boss;
}

[[nodiscard]] StonesUpdateResult phase_6_long_pattern(BossState &boss,
	StonesState &stones, StonesCombatContext &context, StonesCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame < 60) return StonesUpdateResult::boss;
	if((boss.phase_frame % 64) == 0) {
		for(std::size_t stone = 0U; stone < stone_count; ++stone) {
			const auto aimed = pc98::masterlib::iatan2_source(
				word_subtract(context.player_frame.player.current.y, 32),
				word_subtract(context.player_frame.player.current.x,
					stones.topleft[stone].x));
			if(!aimed) return StonesUpdateResult::source_iatan2_fault;
			stones.aimed_angle[stone] = *aimed;
		}
	}
	if((static_cast<std::uint16_t>(boss.phase_frame) & 3U) == 0U) {
		if(boss.phase_frame < 384) {
			if(((boss.phase_frame % 64) >> 2) <= 8) {
				for(std::size_t stone = 0U; stone < stone_count; ++stone) {
					const auto result = add_pellet(context,
						{word_add(stones.topleft[stone].x, 12),
							word_add(stones.topleft[stone].y, 12)},
						stones.aimed_angle[stone], enemy_bullet_group::one, 112);
					if(result != StonesUpdateResult::boss) return result;
				}
				callbacks.sound_effect(3U);
			}
		} else {
			bullets_set_stack_multiplier(context.bullets, 0U);
			for(std::size_t stone = 0U; stone < stone_count; ++stone) {
				const auto result = add_ball(context,
					{word_add(stones.topleft[stone].x, 8),
						word_add(stones.topleft[stone].y, 8)},
					stones.aimed_angle[stone_north], enemy_bullet_group::ring_4,
					bullet_patnum_ball, 112);
				if(result != StonesUpdateResult::boss) return result;
			}
			callbacks.sound_effect(3U);
		}
	}
	if(boss.phase_frame >= 447) {
		boss.phase_frame = 0;
		bullets_set_stack_multiplier(context.bullets, 1U);
	}
	return StonesUpdateResult::boss;
}

[[nodiscard]] bool take_over_stone(StonesState &stones,
	std::size_t stone
) noexcept
{
	if((stone == stone_inner_west) || (stone == stone_inner_east)) {
		stones.patnum[stone] = word_add(stones.patnum[stone], 1);
		if(stones.patnum[stone] >= stone_patnum_inner_active) {
			set_flag(stones, stone, StoneFlag::active);
			return true;
		}
	} else if((stone == stone_outer_west) || (stone == stone_outer_east)) {
		if(stones.patnum[stone] < stone_patnum_outer_transition) {
			stones.patnum[stone] = word_add(stones.patnum[stone], 1);
		}
		if(stones.patnum[stone] == stone_patnum_outer_transition) {
			stones.patnum[stone] = 156;
		} else {
			stones.patnum[stone] = word_add(stones.patnum[stone], 1);
		}
		if(stones.patnum[stone] >= stone_patnum_outer_active) {
			set_flag(stones, stone, StoneFlag::active);
			return true;
		}
	} else if(stone == stone_north) {
		if(stones.patnum[stone_north] < stone_patnum_outer_transition) {
			stones.patnum[stone_north] = word_add(stones.patnum[stone_north], 1);
		}
		if(stones.patnum[stone_north] == stone_patnum_outer_transition) {
			stones.patnum[stone_north] = stone_patnum_north_transition;
		} else {
			stones.patnum[stone_north] = word_add(stones.patnum[stone_north], 1);
		}
		if(stones.patnum[stone_north] >= stone_patnum_north_active) {
			set_flag(stones, stone_north, StoneFlag::active);
			return true;
		}
	}
	return false;
}

[[nodiscard]] std::uint8_t north_cooldown(StonesState &stones) noexcept
{
	if(stones.patnum[stone_north] >= stone_patnum_north_transition) {
		stones.patnum[stone_north] = word_subtract(stones.patnum[stone_north], 1);
	}
	if(stones.patnum[stone_north] == 159) {
		set_flag(stones, stone_north, StoneFlag::dormant);
		stones.patnum[stone_north] = 151;
	} else {
		stones.patnum[stone_north] = word_subtract(stones.patnum[stone_north], 1);
	}
	return (stones.patnum[stone_north] <= stone_patnum_rest) ? 1U : 0U;
}

void queue_takeover_columns(StonesState &stones, std::uint8_t image,
	std::int16_t phase_frame
) noexcept
{
	if((static_cast<std::uint16_t>(phase_frame) & 1U) == 0U) return;
	const auto west = static_cast<std::size_t>(stones.tile_pair);
	const auto east = static_cast<std::size_t>(23U - stones.tile_pair);
	stones.tile_pending[west] = image;
	stones.tile_pending[east] = image;
	stones.tile_pair = static_cast<std::uint8_t>(stones.tile_pair + 1U);
	if(stones.tile_pair >= 12U) {
		stones.tile_pass = static_cast<std::uint8_t>(stones.tile_pass + 1U);
		stones.tile_pair = 0U;
	}
}

void render_takeover_tiles(StonesState &stones, StonesCallbacks &callbacks) noexcept
{
	auto left = playfield_left;
	for(std::size_t column = 0U; column < stones.tile_pending.size(); ++column) {
		const auto pending = stones.tile_pending[column];
		if(pending == 1U) {
			callbacks.put_takeover_tile(left, stones.tile_row, stones.tile_images[0U]);
			stones.tile_pending[column] = 0U;
		} else if(pending == 2U) {
			callbacks.put_takeover_tile(left, stones.tile_row, stones.tile_images[1U]);
			stones.tile_pending[column] = 0U;
		} else if(pending == 3U) {
			callbacks.put_takeover_tile(left, stones.tile_row, stones.tile_images[2U]);
			stones.tile_pending[column] = 0U;
			stones.tile_cols_done = static_cast<std::uint8_t>(
				stones.tile_cols_done + 1U);
		}
		left = word_add(left, 16);
	}
}

[[nodiscard]] bool takeover_tiles(StonesState &stones,
	std::int16_t phase_frame, bool returning, StonesCallbacks &callbacks
) noexcept
{
	if(phase_frame < 49) return false;
	if(phase_frame == 49) {
		stones.tile_pending.fill(0U);
		stones.tile_pair = 0U;
		stones.tile_pass = 0U;
		stones.tile_cols_done = 0U;
		stones.tile_images = returning
			? std::array<std::int16_t, 3U>{42, 41, 40}
			: std::array<std::int16_t, 3U>{41, 42, 43};
	} else if(stones.tile_pass == 0U) {
		queue_takeover_columns(stones, 1U, phase_frame);
	} else if(stones.tile_pass == 1U) {
		queue_takeover_columns(stones, 2U, phase_frame);
	} else if(stones.tile_pass == 2U) {
		queue_takeover_columns(stones, 3U, phase_frame);
	}
	render_takeover_tiles(stones, callbacks);
	return stones.tile_cols_done >= 24U;
}

} // namespace

void stones_combat_reset(BossState &boss, StonesState &stones,
	LaserPoolState &lasers, std::int8_t rank, std::int16_t scroll_line
) noexcept
{
	boss.phase_frame = 0;
	for(std::size_t stone = 0U; stone < stone_count; ++stone) {
		set_flag(stones, stone, StoneFlag::dormant);
		stones.damage[stone] = 0;
		stones.patnum[stone] = stone_patnum_rest;
		stones.hit_flash[stone] = 0U;
	}
	stones.topleft[stone_inner_west] = {128, 32};
	stones.topleft[stone_inner_east] = {288, 32};
	stones.topleft[stone_outer_west] = {48, 48};
	stones.topleft[stone_outer_east] = {368, 48};
	stones.topleft[stone_north] = {208, 32};
	stones.muzzle = {216, 40};
	lasers.wait_frames = 12U;
	stones.phase_frame_unused = 0;
	stones.timeout_frame = 0;
	stones.phase = 0U;
	stones.pattern = 0U;
	boss.explode_angle_offset = 0x20U;
	stones.tile_row = scroll_y(96, scroll_line);
	if(rank != 0) {
		boss.rank_param[0U] = enemy_bullet_group::spread_5_wide_aimed;
		boss.rank_param[1U] = 0U;
		boss.rank_param[2U] = enemy_bullet_group::ring_8;
		boss.rank_param[3U] = enemy_bullet_group::ring_4;
	} else {
		boss.rank_param[0U] = enemy_bullet_group::spread_3_wide_aimed;
		boss.rank_param[1U] = 2U;
		boss.rank_param[2U] = enemy_bullet_group::ring_4;
		boss.rank_param[3U] = enemy_bullet_group::ring_2;
	}
}

StonesUpdateResult stones_update(BossState &boss, StonesState &stones,
	StonesCombatContext &context, StonesCallbacks &callbacks
) noexcept
{
	std::size_t removed{};
	boss.phase_frame = word_add(boss.phase_frame, 1);
	stones.timeout_frame = dword_add(stones.timeout_frame, 1);
	const auto housekeeping = stones_housekeeping(boss, stones, context, callbacks);
	if(housekeeping != StonesUpdateResult::boss) return housekeeping;
	render_living_stones(stones, context, callbacks);
	for(std::size_t stone = 0U; stone < stone_count; ++stone) {
		if(is_flag(stones, stone, StoneFlag::kill_anim)) {
			const auto killed = kill_stone(stones, stone, boss, context, callbacks);
			if(killed != StonesUpdateResult::boss) return killed;
		} else if(is_flag(stones, stone, StoneFlag::removed)) {
			++removed;
		}
	}
	if(removed >= stone_count) return StonesUpdateResult::stage_clear;

	if(stones.phase == 0U) {
		if((boss.phase_frame == 50) || (boss.phase_frame == 52) ||
			(boss.phase_frame == 54) || (boss.phase_frame == 58) ||
			(boss.phase_frame == 60) || (boss.phase_frame == 62)) {
			stones.patnum[stone_inner_west] = word_add(
				stones.patnum[stone_inner_west], 1);
			stones.patnum[stone_inner_east] = word_add(
				stones.patnum[stone_inner_east], 1);
		} else if(boss.phase_frame == 64) {
			stones.patnum[stone_inner_west] = word_add(
				stones.patnum[stone_inner_west], 1);
			stones.patnum[stone_inner_east] = word_add(
				stones.patnum[stone_inner_east], 1);
			boss.phase_frame = 0;
			set_flag(stones, stone_inner_west, StoneFlag::active);
			set_flag(stones, stone_inner_east, StoneFlag::active);
			stones.phase = 1U;
			stones.phase_frame_unused = 0;
		}
	} else if(stones.phase == 1U) {
		const auto pattern = phase_1_pattern(boss, stones, context);
		if(pattern != StonesUpdateResult::boss) return pattern;
		if(stones.timeout_frame > 1300) {
			if(is_flag(stones, stone_inner_west, StoneFlag::active)) {
				set_flag(stones, stone_inner_west, StoneFlag::kill_anim);
			}
			if(is_flag(stones, stone_inner_east, StoneFlag::active)) {
				set_flag(stones, stone_inner_east, StoneFlag::kill_anim);
			}
		}
		if((removed >= 2U) && ((boss.phase_frame % 3) == 0)) {
			(void)take_over_stone(stones, stone_outer_west);
			if(take_over_stone(stones, stone_outer_east)) {
				stones.phase = 2U;
				boss.phase_frame = 0;
				stones.phase_frame_unused = 0;
			}
			stones.timeout_frame = 0;
		}
	} else if(stones.phase == 2U) {
		if(stones.timeout_frame > 1300) {
			if(is_flag(stones, stone_outer_west, StoneFlag::active)) {
				set_flag(stones, stone_outer_west, StoneFlag::kill_anim);
			}
			if(is_flag(stones, stone_outer_east, StoneFlag::active)) {
				set_flag(stones, stone_outer_east, StoneFlag::kill_anim);
			}
		}
		if(removed >= 4U) {
			stones.phase = 3U;
			boss.phase_frame = 0;
			stones.pattern = 0U;
			stones.phase_frame_unused = 0;
			boss.target = {word_add(stones.topleft[stone_north].x, 8), 32};
		} else {
			const auto pattern = phase_2_pattern(boss, stones, context,
				static_cast<int>(removed));
			if(pattern != StonesUpdateResult::boss) return pattern;
		}
	} else if(stones.phase == 3U) {
		if(takeover_tiles(stones, boss.phase_frame, false, callbacks) &&
			((boss.phase_frame % 3) == 0) && take_over_stone(stones, stone_north)) {
			stones.phase = 4U;
			boss.phase_frame = 0;
			stones.phase_frame_unused = 0;
			stones.timeout_frame = 0;
		}
	} else if(stones.phase == 4U) {
		StonesUpdateResult pattern = StonesUpdateResult::boss;
		if(stones.pattern == 0U) {
			phase_4_lasers(boss, context, callbacks);
		} else if(stones.pattern == 1U) {
			pattern = swept_pellets(boss, stones, context);
		} else if(stones.pattern == 2U) {
			phase_4_player_laser(boss, context, callbacks);
		} else if(stones.pattern == 3U) {
			pattern = gravity_balls(boss, stones, context);
		} else if(stones.pattern == 4U) {
			pattern = random_pellet(boss, stones, context);
		} else if(stones.pattern == 5U) {
			pattern = random_lasers(boss, stones, context, callbacks);
		}
		if(pattern != StonesUpdateResult::boss) return pattern;
		if(boss.phase_frame == 0) {
			stones.pattern = static_cast<std::uint8_t>(stones.pattern + 1U);
			stones.phase_frame_unused = 0;
			if(stones.pattern > 5U) {
				stones.phase = 5U;
				stones.pattern = 0U;
			}
		}
		if(stones.damage[stone_north] >= 500) stones.damage[stone_north] = 500;
	} else if(stones.phase == 5U) {
		if(((boss.phase_frame % 3) == 0) && (stones.pattern == 0U)) {
			stones.pattern = north_cooldown(stones);
		}
		if(takeover_tiles(stones, boss.phase_frame, true, callbacks)) {
			stones.phase = 6U;
			boss.phase_frame = 0;
			stones.phase_frame_unused = 0;
			stones.timeout_frame = 0;
			stones.pattern = 0U;
		}
	} else if(stones.phase == 6U) {
		StonesUpdateResult pattern = StonesUpdateResult::boss;
		if(stones.pattern == 0U) {
			pattern = phase_6_mirrored_pellets(boss, stones, context);
		} else if(stones.pattern == 1U) {
			pattern = phase_6_mirrored_balls(boss, stones, context);
		} else if(stones.pattern == 2U) {
			pattern = phase_6_mirrored_pellets(boss, stones, context);
		} else if(stones.pattern == 3U) {
			pattern = phase_6_long_pattern(boss, stones, context, callbacks);
		}
		if(pattern != StonesUpdateResult::boss) return pattern;
		if(boss.phase_frame == 0) {
			stones.pattern = static_cast<std::uint8_t>(stones.pattern + 1U);
			stones.phase_frame_unused = 0;
			if(stones.pattern > 3U) stones.phase = 7U;
		}
	} else if(stones.phase == 7U) {
		if(takeover_tiles(stones, boss.phase_frame, false, callbacks) &&
			((boss.phase_frame % 3) == 0) && take_over_stone(stones, stone_north)) {
			stones.phase = 8U;
			boss.phase_frame = 0;
			stones.phase_frame_unused = 0;
			stones.timeout_frame = 0;
			stones.pattern = 0U;
		}
	} else if(stones.phase == 8U) {
		StonesUpdateResult pattern = StonesUpdateResult::boss;
		if(stones.pattern == 0U) {
			phase_8_laser_wall(boss, stones, context, callbacks);
		} else if(stones.pattern == 1U) {
			pattern = swept_pellets(boss, stones, context);
		} else if(stones.pattern == 2U) {
			pattern = phase_8_converging_pellets(boss, stones, context);
		} else if(stones.pattern == 3U) {
			pattern = gravity_balls(boss, stones, context);
		} else if(stones.pattern == 4U) {
			pattern = random_pellet(boss, stones, context);
		} else if(stones.pattern == 5U) {
			pattern = random_lasers(boss, stones, context, callbacks);
		}
		if(pattern != StonesUpdateResult::boss) return pattern;
		if(boss.phase_frame == 0) {
			stones.pattern = static_cast<std::uint8_t>(stones.pattern + 1U);
			stones.phase_frame_unused = 0;
			if(stones.pattern > 5U) stones.pattern = 1U;
			if(stones.timeout_frame > 2000) {
				set_flag(stones, stone_north, StoneFlag::kill_anim);
			}
		}
	}
	return StonesUpdateResult::boss;
}

} // namespace th02::core
