#include "th02/core/marisa.hpp"

#include "pc98/masterlib_vector.hpp"

#include <bit>
#include <cstdint>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_top = 16;
constexpr std::int16_t playfield_right = 416;
constexpr std::int16_t marisa_center_offset = 40;
constexpr std::int16_t orb_patnum = 138;
constexpr std::int16_t star_patnum = 85;
constexpr std::int16_t boss_patnum_rest = 128;
constexpr std::int16_t boss_patnum_cast_1 = 130;
constexpr std::int16_t boss_patnum_cast_2 = 132;
constexpr std::int16_t boss_patnum_cast_3 = 134;
constexpr std::int16_t boss_patnum_cast_4 = 136;
constexpr std::int16_t special_one = 0x00FF;
constexpr std::int8_t pattern_unstarted = 0x7F;
constexpr std::uint8_t boss_defeat_invincibility_frames = 200U;
constexpr std::array<std::uint8_t, 6U> marisa_bg_particle_colors{
	10U, 12U, 13U, 13U, 12U, 10U,
};

[[nodiscard]] std::int16_t word_add(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t word_subtract(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	return word_add(lhs, std::bit_cast<std::int16_t>(
		static_cast<std::uint16_t>(0U - std::bit_cast<std::uint16_t>(rhs))));
}

[[nodiscard]] std::int32_t dword_add(std::int32_t lhs,
	std::int32_t rhs) noexcept
{
	const auto bits = static_cast<std::uint32_t>(
		static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(lhs)) +
		static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(rhs))
	);
	return std::bit_cast<std::int32_t>(bits);
}

[[nodiscard]] std::int16_t arithmetic_shift_right(std::int16_t value,
	std::uint8_t count) noexcept
{
	auto bits = static_cast<std::uint16_t>(
		std::bit_cast<std::uint16_t>(value) >> count);
	if(value < 0) {
		bits = static_cast<std::uint16_t>(bits |
			(static_cast<std::uint16_t>(0xFFFFU) << (16U - count)));
	}
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] bool orb_is(const MarisaState &marisa, std::size_t orb,
	MarisaOrbFlag flag) noexcept
{
	return marisa.orb_flag[orb] == static_cast<std::int16_t>(flag);
}

void set_orb(MarisaState &marisa, std::size_t orb,
	MarisaOrbFlag flag) noexcept
{
	marisa.orb_flag[orb] = static_cast<std::int16_t>(flag);
}

[[nodiscard]] MarisaUpdateResult map_bullet_result(
	EnemyBulletAddResult result
) noexcept
{
	switch(result) {
	case EnemyBulletAddResult::completed:
		return MarisaUpdateResult::boss;
	case EnemyBulletAddResult::invalid_page:
		return MarisaUpdateResult::bullet_invalid_page;
	case EnemyBulletAddResult::source_iatan2_fault:
		return MarisaUpdateResult::bullet_source_iatan2_fault;
	}
	return MarisaUpdateResult::bullet_source_iatan2_fault;
}

[[nodiscard]] MarisaUpdateResult add_pellet(MarisaCombatContext &context,
	ScreenPoint origin, std::uint8_t angle, std::uint8_t group,
	std::int16_t speed
) noexcept
{
	return map_bullet_result(bullets_add_pellet(context.bullets, context.random,
		context.player_frame, context.playperf, context.rank, context.bombing,
		origin, angle, group, speed));
}

[[nodiscard]] MarisaUpdateResult add_star(MarisaCombatContext &context,
	ScreenPoint origin, std::uint8_t angle, std::uint8_t special,
	std::int16_t speed
) noexcept
{
	return map_bullet_result(bullets_add_16x16(context.bullets, context.random,
		context.player_frame, context.playperf, context.rank, context.bombing,
		origin, angle, special, star_patnum, speed));
}

void cast_cels(BossState &boss, std::int16_t frame,
	MarisaCallbacks &callbacks) noexcept
{
	if(boss.phase_frame < frame) return;
	if(boss.phase_frame == frame) {
		callbacks.sound_effect(9U);
		boss.patnum = boss_patnum_cast_1;
	} else if(boss.phase_frame == word_add(frame, 30)) {
		boss.patnum = boss_patnum_cast_1;
	} else if(boss.phase_frame == word_add(frame, 36)) {
		boss.patnum = boss_patnum_cast_2;
	} else if(boss.phase_frame == word_add(frame, 42)) {
		boss.patnum = boss_patnum_cast_3;
	} else if(boss.phase_frame == word_add(frame, 50)) {
		boss.patnum = boss_patnum_cast_4;
	} else if(boss.phase_frame == word_add(frame, 60)) {
		boss.patnum = boss_patnum_rest;
	}
}

void early_cast_cels(BossState &boss, MarisaCallbacks &callbacks) noexcept
{
	if(boss.phase_frame == 50) {
		callbacks.sound_effect(9U);
		boss.patnum = boss_patnum_cast_1;
	} else if(boss.phase_frame == 100) {
		callbacks.sound_effect(10U);
		boss.patnum = boss_patnum_cast_2;
	} else if(boss.phase_frame == 108) {
		callbacks.sound_effect(10U);
		boss.patnum = boss_patnum_cast_3;
	} else if(boss.phase_frame == 116) {
		callbacks.sound_effect(10U);
		boss.patnum = boss_patnum_cast_4;
	} else if(boss.phase_frame == 130) {
		boss.patnum = boss_patnum_rest;
	}
}

void set_rank_parameters(BossState &boss, std::int8_t rank) noexcept
{
	if(rank != 0) {
		boss.rank_param[0U] = enemy_bullet_group::spread_5_medium_aimed;
		boss.rank_param[1U] = enemy_bullet_group::spread_4_medium_aimed;
		boss.rank_param[2U] = enemy_bullet_group::spread_2_medium;
		boss.rank_param[3U] = 4U;
	} else {
		boss.rank_param[0U] = enemy_bullet_group::spread_5_wide_aimed;
		boss.rank_param[1U] = enemy_bullet_group::spread_4_wide_aimed;
		boss.rank_param[2U] = enemy_bullet_group::one;
		boss.rank_param[3U] = 2U;
	}
}

[[nodiscard]] MarisaUpdateResult update_background_particles(
	MarisaState &marisa, MarisaCombatContext &context
) noexcept
{
	if(((context.stage_frame & 1U) != 0U) &&
		(!context.reduce_effects || ((context.stage_frame & 3U) != 0U))) {
		const auto left = static_cast<std::int16_t>(
			(randring_next16(context.random) % 384U) +
			static_cast<std::uint16_t>(playfield_left)
		);
		const auto angle = static_cast<std::uint8_t>(
			(static_cast<std::int32_t>(224) - left) / 3 + 0x40
		);
		(void)bg_particles_add(context.background_particles, left,
			playfield_top, angle);
	}
	if((context.stage_frame & 31U) == 0U) {
		marisa.bg_particle_col_i = static_cast<std::int8_t>(
			marisa.bg_particle_col_i + 1
		);
		if(marisa.bg_particle_col_i >= static_cast<std::int8_t>(
			marisa_bg_particle_colors.size())) {
			marisa.bg_particle_col_i = 0;
		}
		context.background_particles.color = marisa_bg_particle_colors[
			static_cast<std::size_t>(marisa.bg_particle_col_i)
		];
	}
	return bg_particles_update_and_render(context.background_particles,
		context.player_frame.page_back, context.reduce_effects,
		context.background_particle_callbacks) == BgParticlePageResult::completed ?
		MarisaUpdateResult::boss : MarisaUpdateResult::invalid_page;
}

[[nodiscard]] MarisaUpdateResult marisa_hittest(BossState &boss,
	MarisaState &marisa, MarisaCombatContext &context,
	MarisaCallbacks &callbacks
) noexcept
{
	if(boss.phase != 0U) return MarisaUpdateResult::boss;
	PlayerShotSparkAllocatorCallbacks sparks(context.sparks,
		context.player_frame.page_back);
	const auto damage = shots_hittest(context.shots, context.score,
		context.player_frame.page_back, context.shot_level, context.stage_frame,
		context.bombing, context.bomb_damage_frame_mask,
		{{word_add(marisa.topleft.x, 24), word_add(marisa.topleft.y, 24)}, 48, 48},
		sparks);
	if(!damage) return MarisaUpdateResult::invalid_shot_hittest;
	if(!sparks.all_allocations_succeeded()) {
		return MarisaUpdateResult::shot_spark_allocation_refused;
	}
	if(*damage != 0) {
		if(marisa.orb_flag_sum >= 8) {
			callbacks.sound_effect(4U);
			marisa.hit_flash = true;
			const auto increased = static_cast<std::int16_t>(
				static_cast<std::int32_t>(marisa.damage_multiplier) * *damage);
			boss.damage = word_add(boss.damage, increased);
		} else {
			callbacks.sound_effect(11U);
		}
		if(boss.damage >= 900) {
			boss.phase = 1U;
			context.score.score_delta = dword_add(context.score.score_delta, 20000);
			context.player_hit.invincibility_time = boss_defeat_invincibility_frames;
		}
	}
	const auto player = context.player_frame.player.on_page[context.player_frame.page_front];
	if((player.x > word_subtract(marisa.topleft.x, 16)) &&
		(player.x < word_add(marisa.topleft.x, 48)) &&
		(player.y > word_subtract(marisa.topleft.y, 16)) &&
		(player.y < word_add(marisa.topleft.y, 48))) {
		context.player_hit.raw_hit = 1U;
	}
	return MarisaUpdateResult::boss;
}

[[nodiscard]] MarisaUpdateResult orb_hittest(MarisaState &marisa,
	MarisaCombatContext &context, MarisaCallbacks &callbacks
) noexcept
{
	context.bomb_damage_frame_mask = 3U;
	for(std::size_t orb = 0U; orb < marisa_orb_count; ++orb) {
		if(!orb_is(marisa, orb, MarisaOrbFlag::alive)) continue;
		const auto point = marisa.orb_on_page[context.player_frame.page_back][orb];
		PlayerShotSparkAllocatorCallbacks sparks(context.sparks,
			context.player_frame.page_back);
		const auto damage = shots_hittest(context.shots, context.score,
			context.player_frame.page_back, context.shot_level, context.stage_frame,
			context.bombing, context.bomb_damage_frame_mask,
			{{word_add(point.x, 4), point.y}, 24, 32}, sparks);
		if(!damage) return MarisaUpdateResult::invalid_shot_hittest;
		if(!sparks.all_allocations_succeeded()) {
			return MarisaUpdateResult::shot_spark_allocation_refused;
		}
		if(*damage != 0) {
			marisa.orb_hit_flash[orb] = 1;
			marisa.orb_damage[orb] = word_add(marisa.orb_damage[orb], 1);
			if(marisa.orb_damage[orb] >= 110) {
				if((randring_next8(context.random) % 80U) >= context.player_power) {
					items_add(context.items, context.score, word_add(point.x, 8),
						word_add(point.y, 8), static_cast<std::int16_t>(ItemType::big_power));
				}
				callbacks.sound_effect(2U);
				set_orb(marisa, orb, MarisaOrbFlag::kill_anim);
				context.score.score_delta = dword_add(context.score.score_delta, 5000);
			}
		}
		const auto player = context.player_frame.player.on_page[context.player_frame.page_front];
		if((player.x > word_subtract(point.x, 16)) &&
			(player.x < word_add(point.x, 16)) &&
			(player.y > word_subtract(point.y, 16)) &&
			(player.y < word_add(point.y, 16))) {
			context.player_hit.raw_hit = 1U;
		}
	}
	context.bomb_damage_frame_mask = 1U;
	return MarisaUpdateResult::boss;
}

[[nodiscard]] MarisaUpdateResult defeat_update(BossState &boss,
	MarisaState &marisa, MarisaCombatContext &context,
	MarisaCallbacks &callbacks
) noexcept
{
	const auto center = ScreenPoint{word_add(marisa.topleft.x, marisa_center_offset),
		word_add(marisa.topleft.y, marisa_center_offset)};
	boss_explode_render(center, marisa.defeat_frame, boss.explode_angle_offset,
		context.scroll_line, context.slowdown_factor, context.palette_tone, callbacks);
	if(marisa.defeat_frame >= 24) {
		if(marisa.defeat_frame == 56) boss.explode_angle_offset = 32U;
		boss_explode_render(center, word_subtract(marisa.defeat_frame, 24),
			boss.explode_angle_offset, context.scroll_line, context.slowdown_factor,
			context.palette_tone, callbacks);
		if(marisa.defeat_frame >= 48) {
			if(marisa.defeat_frame == 80) boss.explode_angle_offset = 0U;
			boss_explode_render(center, word_subtract(marisa.defeat_frame, 48),
				boss.explode_angle_offset, context.scroll_line,
				context.slowdown_factor, context.palette_tone, callbacks);
			if((marisa.defeat_frame >= 64) &&
				((static_cast<std::uint16_t>(marisa.defeat_frame) & 15U) == 0U)) {
				if(!sparks_add(context.sparks, context.player_frame.page_back,
					word_add(marisa.topleft.x, 48),
					static_cast<std::uint16_t>(word_add(marisa.topleft.y, 48)),
					136, 24, true)) {
					return MarisaUpdateResult::defeat_spark_allocation_refused;
				}
			}
		}
	}
	const auto zoom = word_add(10, arithmetic_shift_right(
		word_subtract(marisa.defeat_frame, 32), 4U));
	marisa.defeat_frame = word_add(marisa.defeat_frame, 1);
	if(marisa.defeat_frame < 32) {
		callbacks.render_marisa({MarisaSpriteKind::normal, marisa.topleft,
			boss.patnum, 0});
		callbacks.render_marisa({MarisaSpriteKind::normal,
			{word_add(marisa.topleft.x, 48), marisa.topleft.y},
			word_add(boss.patnum, 1), 0});
	} else {
		callbacks.render_marisa({MarisaSpriteKind::defeat_zoom, marisa.topleft,
			0, zoom});
	}
	if(marisa.defeat_frame >= 100) {
		marisa.defeat_frame = 0;
		boss.phase = 3U;
		return MarisaUpdateResult::stage_clear;
	}
	return MarisaUpdateResult::boss;
}

[[nodiscard]] MarisaUpdateResult orb_kill_update(MarisaState &marisa,
	std::size_t orb, MarisaCombatContext &context, MarisaCallbacks &callbacks
) noexcept
{
	const auto point = marisa.orb_on_page[context.player_frame.page_back][orb];
	if(!sparks_add(context.sparks, context.player_frame.page_back,
		word_add(point.x, 16), static_cast<std::uint16_t>(word_add(point.y, 16)),
		60, 2, false)) {
		return MarisaUpdateResult::orb_spark_allocation_refused;
	}
	const auto patnum = word_add(10, static_cast<std::int16_t>(
		marisa.orb_kill_frame[orb] / 6));
	marisa.orb_kill_frame[orb] = word_add(marisa.orb_kill_frame[orb], 1);
	if(marisa.orb_kill_frame[orb] >= 48) {
		marisa.orb_kill_frame[orb] = 0;
		set_orb(marisa, orb, MarisaOrbFlag::removed);
		callbacks.clear_orb_on_front_page(orb,
			marisa.orb_on_page[context.player_frame.page_front][orb]);
		marisa.orb_damage[orb] = 0;
		return MarisaUpdateResult::boss;
	}
	callbacks.render_orb({MarisaOrbSpriteKind::kill, orb, point, patnum});
	return MarisaUpdateResult::boss;
}

void render_marisa(BossState &boss, MarisaState &marisa,
	MarisaCallbacks &callbacks) noexcept
{
	const auto kind = marisa.hit_flash ? MarisaSpriteKind::hit_white :
		MarisaSpriteKind::normal;
	callbacks.render_marisa({kind, marisa.topleft, boss.patnum, 0});
	callbacks.render_marisa({kind, {word_add(marisa.topleft.x, 48),
		marisa.topleft.y}, word_add(boss.patnum, 1), 0});
	marisa.hit_flash = false;
}

void update_orb_positions(MarisaState &marisa,
	const MarisaCombatContext &context
) noexcept
{
	for(std::size_t orb = 0U; orb < marisa_orb_count; ++orb) {
		if(!orb_is(marisa, orb, MarisaOrbFlag::alive)) continue;
		marisa.orb_angle[orb] = static_cast<std::uint8_t>(marisa.orb_angle[orb] +
			static_cast<std::uint8_t>(marisa.orb_angle_delta[orb]));
		const auto offset = pc98::masterlib::vector2(marisa.orb_angle[orb],
			marisa.orb_radius[orb]);
		marisa.orb_on_page[context.player_frame.page_back][orb] = {
			word_add(word_add(marisa.topleft.x, 32), offset.x),
			word_add(word_add(marisa.topleft.y, 32), offset.y),
		};
	}
}

void render_orbs(MarisaState &marisa, const MarisaCombatContext &context,
	MarisaCallbacks &callbacks) noexcept
{
	std::array<std::size_t, marisa_orb_count> alive{};
	std::size_t alive_count{};
	for(std::size_t orb = 0U; orb < marisa_orb_count; ++orb) {
		if(orb_is(marisa, orb, MarisaOrbFlag::alive)) alive[alive_count++] = orb;
	}
	if(alive_count != 0U) {
		for(std::size_t i = 0U; i < alive_count; ++i) {
			const auto from = marisa.orb_on_page[context.player_frame.page_back][alive[i]];
			const auto to = marisa.orb_on_page[context.player_frame.page_back][
				alive[(i + 1U) % alive_count]];
			callbacks.render_orb_line({word_add(from.x, 16), word_add(from.y, 16)},
				{word_add(to.x, 16), word_add(to.y, 16)});
		}
	}
	for(std::size_t orb = 0U; orb < marisa_orb_count; ++orb) {
		if(marisa.orb_flag[orb] >= static_cast<std::int16_t>(MarisaOrbFlag::kill_anim)) {
			continue;
		}
		const auto point = marisa.orb_on_page[context.player_frame.page_back][orb];
		if(marisa.orb_hit_flash[orb] != 0) {
			if((point.x > 0) && (point.x < playfield_right) &&
				(point.y >= 0) && (point.y <= 368)) {
				callbacks.render_orb({MarisaOrbSpriteKind::hit_white, orb, point,
					word_add(orb_patnum, static_cast<std::int16_t>(orb))});
			}
			marisa.orb_hit_flash[orb] = 0;
		} else {
			callbacks.render_orb({MarisaOrbSpriteKind::normal, orb, point,
				word_add(orb_patnum, static_cast<std::int16_t>(orb))});
		}
	}
}

[[nodiscard]] MarisaUpdateResult pattern_minus_1(BossState &boss,
	MarisaState &marisa, MarisaCombatContext &context,
	MarisaCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame < 50) return MarisaUpdateResult::boss;
	early_cast_cels(boss, callbacks);
	if(boss.phase_frame == 50) {
		marisa.spray_angle = 0x18U;
	} else if(boss.phase_frame == 130) {
		boss.phase_frame = 0;
		marisa.spray_is_first_run = false;
	}
	if((boss.phase_frame <= 100) || ((boss.phase_frame % 3) != 0)) {
		return MarisaUpdateResult::boss;
	}
	if(marisa.spray_is_first_run) {
		auto result = add_pellet(context,
			{word_add(marisa.topleft.x, 36), word_add(marisa.topleft.y, 64)},
			marisa.spray_angle, enemy_bullet_group::one, 60);
		if(result != MarisaUpdateResult::boss) return result;
		result = add_pellet(context,
			{word_add(marisa.topleft.x, 52), word_add(marisa.topleft.y, 64)},
			marisa.spray_angle, enemy_bullet_group::one, 60);
		if(result != MarisaUpdateResult::boss) return result;
	} else {
		const auto result = add_pellet(context,
			{word_add(marisa.topleft.x, 44), word_add(marisa.topleft.y, 64)},
			marisa.spray_angle, enemy_bullet_group::one, 50);
		if(result != MarisaUpdateResult::boss) return result;
	}
	marisa.spray_angle = static_cast<std::uint8_t>(marisa.spray_angle + 10U);
	return MarisaUpdateResult::boss;
}

[[nodiscard]] MarisaUpdateResult pattern_minus_2(BossState &boss,
	MarisaState &marisa, MarisaCombatContext &context,
	MarisaCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame < 50) return MarisaUpdateResult::boss;
	if(boss.phase_frame == 50) {
		callbacks.sound_effect(9U);
		boss.patnum = boss_patnum_cast_1;
	}
	if((boss.phase_frame % 24) == 0) {
		auto result = add_pellet(context,
			{word_add(marisa.topleft.x, 64), word_add(marisa.topleft.y, 16)},
			0U, boss.rank_param[0U], 54);
		if(result != MarisaUpdateResult::boss) return result;
		result = add_pellet(context,
			{word_add(marisa.topleft.x, 64), word_add(marisa.topleft.y, 16)},
			0U, boss.rank_param[1U], 60);
		if(result != MarisaUpdateResult::boss) return result;
	}
	if(boss.phase_frame > 192) {
		boss.phase_frame = 0;
		boss.patnum = boss_patnum_rest;
	}
	return MarisaUpdateResult::boss;
}

[[nodiscard]] MarisaUpdateResult pattern_minus_3(BossState &boss,
	MarisaState &marisa, MarisaCombatContext &context,
	MarisaCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame < 50) return MarisaUpdateResult::boss;
	early_cast_cels(boss, callbacks);
	if(boss.phase_frame == 50) {
		marisa.drift_star_angle = randring_next8(context.random);
		context.bullet_special.chase_gravity_or_drift_angle =
			static_cast<std::int16_t>(marisa.star_drift_angle);
		context.bullet_special.homing_frames_or_drift_speed = 2;
		context.bullet_special.turns_or_drift_frames = 128;
		marisa.star_drift_angle = static_cast<std::uint8_t>(
			0U - marisa.star_drift_angle);
	} else if(boss.phase_frame == 130) {
		boss.phase_frame = 0;
	}
	if((boss.phase_frame < 100) ||
		((static_cast<std::uint16_t>(boss.phase_frame) & 3U) != 0U)) {
		return MarisaUpdateResult::boss;
	}
	const auto result = add_star(context,
		{word_add(marisa.topleft.x, 44), word_add(marisa.topleft.y, 64)},
		marisa.drift_star_angle, enemy_bullet_special_motion::drift_angle_and_speed,
		40);
	if(result != MarisaUpdateResult::boss) return result;
	marisa.drift_star_angle = static_cast<std::uint8_t>(marisa.drift_star_angle + 0x2BU);
	return MarisaUpdateResult::boss;
}

void spawn_orbs(BossState &boss, MarisaState &marisa,
	MarisaCallbacks &callbacks) noexcept
{
	if(boss.phase_frame < 100) return;
	if(boss.phase_frame == 100) {
		callbacks.sound_effect(9U);
		boss.patnum = boss_patnum_cast_1;
	} else if(boss.phase_frame == 130) {
		const auto center = ScreenPoint{word_add(marisa.topleft.x, 32),
			word_add(marisa.topleft.y, 32)};
		for(std::size_t orb = 0U; orb < marisa_orb_count; ++orb) {
			set_orb(marisa, orb, MarisaOrbFlag::alive);
			marisa.orb_angle[orb] = static_cast<std::uint8_t>(orb << 6U);
			marisa.orb_radius[orb] = 8;
			marisa.orb_angle_delta[orb] = 2;
			marisa.orb_on_page[0U][orb] = center;
			marisa.orb_on_page[1U][orb] = center;
		}
	} else if(boss.phase_frame == 154) {
		boss.patnum = boss_patnum_rest;
		boss.phase_frame = 0;
	}
	if(boss.phase_frame <= 130) return;
	for(auto &radius : marisa.orb_radius) radius = word_add(radius, 2);
}

[[nodiscard]] MarisaUpdateResult pattern_1(BossState &boss,
	MarisaState &marisa, MarisaCombatContext &context
) noexcept
{
	if(boss.phase_frame < 10) return MarisaUpdateResult::boss;
	if(boss.phase_frame == 10) {
		marisa.swoop_direction = (word_add(marisa.topleft.x, 32) <
			context.player_frame.player.current.x) ? 1 : -1;
		marisa.swoop_center.x = marisa.topleft.x;
		marisa.swoop_center.y = word_add(marisa.topleft.y, 112);
		marisa.swoop_angle = 0xC0U;
	}
	marisa.swoop_angle = static_cast<std::uint8_t>(marisa.swoop_angle +
		static_cast<std::uint8_t>(marisa.swoop_direction));
	const auto offset = pc98::masterlib::vector2(marisa.swoop_angle, 112);
	auto &back = boss.topleft_on_page[context.player_frame.page_back];
	back = {word_add(marisa.swoop_center.x, offset.x),
		word_add(marisa.swoop_center.y, offset.y)};
	marisa.topleft = back;
	if((static_cast<std::uint16_t>(boss.phase_frame) & 15U) == 0U) {
		const auto orb = static_cast<std::size_t>(randring_next8(context.random) & 3U);
		if(orb_is(marisa, orb, MarisaOrbFlag::alive)) {
			const auto point = marisa.orb_on_page[context.player_frame.page_back][orb];
			const auto result = add_pellet(context,
				{word_add(point.x, 12), word_add(point.y, 12)}, 0U,
				enemy_bullet_group::one_aimed, 80);
			if(result != MarisaUpdateResult::boss) return result;
		}
		if((marisa.orb_flag_sum == 8) && (orb < 1U)) {
			const auto result = add_pellet(context,
				{word_add(marisa.topleft.x, 64), word_add(marisa.topleft.y, 16)},
				0U, enemy_bullet_group::spread_3_narrow_aimed, 80);
			if(result != MarisaUpdateResult::boss) return result;
		}
	}
	if(boss.phase_frame >= 266) boss.phase_frame = 0;
	return MarisaUpdateResult::boss;
}

[[nodiscard]] MarisaUpdateResult pattern_2(BossState &boss,
	MarisaState &marisa, MarisaCombatContext &context,
	MarisaCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame < 50) return MarisaUpdateResult::boss;
	if(boss.phase_frame < 280) {
		for(auto &radius : marisa.orb_radius) radius = word_add(radius, 1);
	} else if(boss.phase_frame < 510) {
		for(auto &radius : marisa.orb_radius) radius = word_subtract(radius, 1);
	} else {
		boss.phase_frame = 0;
		boss.patnum = boss_patnum_rest;
	}
	cast_cels(boss, 220, callbacks);
	if(boss.phase_frame == 220) marisa.stream_angle = 0U;
	if(marisa.orb_flag_sum == 8) boss.patnum = boss_patnum_cast_1;
	if(((boss.phase_frame >= 250) && (boss.phase_frame < 270)) ||
		(marisa.orb_flag_sum == 8)) {
		if(marisa.orb_flag_sum != 8) {
			marisa.stream_angle = static_cast<std::uint8_t>(marisa.stream_angle + 13U);
			if(context.rank >= static_cast<std::int8_t>(
				static_cast<std::uint16_t>(boss.phase_frame) & 1U)) {
				return add_pellet(context,
					{word_add(marisa.topleft.x, 44), word_add(marisa.topleft.y, 64)},
					marisa.stream_angle, enemy_bullet_group::one, 50);
			}
		} else {
			marisa.stream_angle = static_cast<std::uint8_t>(marisa.stream_angle + 10U);
			const auto result = add_pellet(context,
				{word_add(marisa.topleft.x, 64), word_add(marisa.topleft.y, 16)},
				marisa.stream_angle, enemy_bullet_group::one, 60);
			if(result != MarisaUpdateResult::boss) return result;
			if(boss.phase_frame > 270) {
				boss.phase_frame = 0;
				boss.patnum = boss_patnum_rest;
			}
		}
	}
	return MarisaUpdateResult::boss;
}

[[nodiscard]] MarisaUpdateResult pattern_5(BossState &boss,
	MarisaState &marisa, MarisaCombatContext &context,
	MarisaCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame == 10) {
		marisa.orb_angle_delta = {3, -2, 3, -2};
	}
	if(boss.phase_frame < 50) return MarisaUpdateResult::boss;
	if(boss.phase_frame < 130) {
		for(std::size_t orb = 0U; orb < marisa_orb_count; orb += 2U) {
			marisa.orb_radius[orb] = word_add(marisa.orb_radius[orb], 2);
		}
	} else if(boss.phase_frame >= 260) {
		if(boss.phase_frame < 340) {
			for(std::size_t orb = 0U; orb < marisa_orb_count; orb += 2U) {
				marisa.orb_radius[orb] = word_subtract(marisa.orb_radius[orb], 2);
			}
			if(context.rank != 0) {
				for(std::size_t orb = 1U; orb < marisa_orb_count; orb += 2U) {
					marisa.orb_radius[orb] = word_add(marisa.orb_radius[orb], 1);
				}
			}
		} else if(boss.phase_frame < 420) {
			if(context.rank != 0) {
				for(std::size_t orb = 1U; orb < marisa_orb_count; orb += 2U) {
					marisa.orb_radius[orb] = word_subtract(marisa.orb_radius[orb], 1);
				}
			}
		} else {
			marisa.orb_angle_delta = {2, -2, 2, -2};
			boss.phase_frame = 0;
			boss.patnum = boss_patnum_rest;
		}
	}
	cast_cels(boss, 140, callbacks);
	if(boss.phase_frame == 140) marisa.orb_pair_angle = 0U;
	if(marisa.orb_flag_sum == 8) {
		boss.patnum = boss_patnum_cast_1;
		marisa.orb_pair_angle = static_cast<std::uint8_t>(marisa.orb_pair_angle -
			static_cast<std::uint8_t>(0x18 - context.rank));
		return add_star(context,
			{word_add(marisa.topleft.x, 64), word_add(marisa.topleft.y, 16)},
			marisa.orb_pair_angle, static_cast<std::uint8_t>(special_one), 60);
	}
	if((boss.phase_frame < 150) || (boss.phase_frame >= 250) ||
		((static_cast<std::uint16_t>(boss.phase_frame) & 3U) != 0U)) {
		return MarisaUpdateResult::boss;
	}
	marisa.orb_pair_angle = static_cast<std::uint8_t>(marisa.orb_pair_angle + 3U);
	callbacks.sound_effect(10U);
	for(std::size_t orb = 0U; orb < marisa_orb_count; orb += 2U) {
		if(!orb_is(marisa, orb, MarisaOrbFlag::alive)) continue;
		const auto point = marisa.orb_on_page[context.player_frame.page_back][orb];
		if((point.x > 0) && (point.x < playfield_right) &&
			(point.y >= 0) && (point.y <= 368)) {
			const auto result = add_pellet(context,
				{word_add(point.x, 12), word_add(point.y, 12)},
				static_cast<std::uint8_t>(marisa.orb_angle[orb] + marisa.orb_pair_angle),
				enemy_bullet_group::one, 70);
			if(result != MarisaUpdateResult::boss) return result;
		}
	}
	return MarisaUpdateResult::boss;
}

[[nodiscard]] MarisaUpdateResult pattern_6(BossState &boss,
	MarisaState &marisa, MarisaCombatContext &context,
	MarisaCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame == 10) marisa.orb_angle_delta = {-3, -3, -3, -3};
	if(marisa.orb_flag_sum == 8) {
		boss.patnum = boss_patnum_rest;
		boss.phase_frame = 0;
	}
	if(boss.phase_frame < 50) return MarisaUpdateResult::boss;
	if(boss.phase_frame == 50) {
		marisa.orb_angle_delta = {-4, -4, -4, -4};
		callbacks.sound_effect(9U);
		boss.patnum = boss_patnum_cast_1;
	} else if((boss.phase_frame == 100) || (boss.phase_frame == 110)) {
		callbacks.sound_effect(3U);
		const auto start = (boss.phase_frame == 100) ? -0x40 : -0x46;
		const auto stop = (boss.phase_frame == 100) ? 0x40 : 0x46;
		const auto step = (boss.phase_frame == 100) ? 0x10 : 12;
		for(std::size_t orb = 0U; orb < marisa_orb_count; ++orb) {
			if(!orb_is(marisa, orb, MarisaOrbFlag::alive)) continue;
			const auto point = marisa.orb_on_page[context.player_frame.page_back][orb];
			for(auto angle = start; angle < stop; angle += step) {
				const auto speed = (boss.phase_frame == 100) ? std::int16_t{70} :
					static_cast<std::int16_t>((randring_next8(context.random) & 0x1FU) + 0x10U);
				const auto result = add_pellet(context,
					{word_add(point.x, 12), word_add(point.y, 12)},
					static_cast<std::uint8_t>(static_cast<std::int16_t>(
						marisa.orb_angle[orb]) + angle), enemy_bullet_group::one, speed);
				if(result != MarisaUpdateResult::boss) return result;
			}
			if(boss.phase_frame == 110) set_orb(marisa, orb, MarisaOrbFlag::kill_anim);
		}
	}
	return MarisaUpdateResult::boss;
}

[[nodiscard]] MarisaUpdateResult pattern_3(BossState &boss,
	MarisaState &marisa, MarisaCombatContext &context,
	MarisaCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame < 50) return MarisaUpdateResult::boss;
	if(boss.phase_frame >= 150) cast_cels(boss, 120, callbacks);
	if(boss.phase_frame == 50) {
		callbacks.sound_effect(9U);
		boss.patnum = boss_patnum_cast_1;
		marisa.volleys_fired = 0U;
		marisa.orb_volley_angle.fill(0U);
	} else if(boss.phase_frame == 300) {
		boss.phase_frame = 0;
		for(auto &angle : marisa.orb_angle_delta) {
			angle = word_subtract(0, angle);
		}
	}
	if((boss.phase_frame <= 70) || ((boss.phase_frame % 5) != 0)) {
		return MarisaUpdateResult::boss;
	}
	callbacks.sound_effect(10U);
	for(std::size_t orb = 0U; orb < marisa_orb_count; ++orb) {
		if(!orb_is(marisa, orb, MarisaOrbFlag::alive)) continue;
		const auto point = marisa.orb_on_page[context.player_frame.page_back][orb];
		const auto speed = word_add(arithmetic_shift_right(boss.phase_frame, 2U), 25);
		const auto result = add_pellet(context,
			{word_add(point.x, 12), word_add(point.y, 12)},
			static_cast<std::uint8_t>(marisa.orb_angle[orb] +
				marisa.orb_volley_angle[orb]), enemy_bullet_group::one, speed);
		if(result != MarisaUpdateResult::boss) return result;
		marisa.orb_volley_angle[orb] = static_cast<std::uint8_t>(
			marisa.orb_volley_angle[orb] + static_cast<std::uint8_t>(
				marisa.orb_angle_delta[orb] * 2));
	}
	if((marisa.orb_flag_sum == 8) && ((marisa.volleys_fired & 1U) == 0U)) {
		const auto result = add_pellet(context,
			{word_add(marisa.topleft.x, 44), word_add(marisa.topleft.y, 44)},
			static_cast<std::uint8_t>(boss.phase_frame), enemy_bullet_group::ring_16,
			50);
		if(result != MarisaUpdateResult::boss) return result;
	}
	marisa.volleys_fired = static_cast<std::uint8_t>(marisa.volleys_fired + 1U);
	return MarisaUpdateResult::boss;
}

[[nodiscard]] MarisaUpdateResult pattern_4(BossState &boss,
	MarisaState &marisa, MarisaCombatContext &context,
	MarisaCallbacks &callbacks
) noexcept
{
	if(boss.phase_frame < 50) return MarisaUpdateResult::boss;
	if(boss.phase_frame >= 150) cast_cels(boss, 120, callbacks);
	if(boss.phase_frame == 50) {
		callbacks.sound_effect(9U);
		boss.patnum = boss_patnum_cast_1;
		context.bullet_special.chase_gravity_or_drift_angle = marisa.pattern_side;
		context.bullet_special.homing_frames_or_drift_speed = 0;
		context.bullet_special.turns_or_drift_frames = 64;
		marisa.pattern_side = static_cast<std::int8_t>(-marisa.pattern_side);
		marisa.orb_angle_delta = {2, -2, 2, -2};
	} else if(boss.phase_frame == 70) {
		marisa.orb_angle_delta = {3, -3, 3, -3};
	} else if(boss.phase_frame == 80) {
		marisa.orb_angle_delta = {4, -4, 4, -4};
	} else if(boss.phase_frame == 200) {
		marisa.orb_angle_delta = {2, -2, 2, -2};
		boss.phase_frame = 0;
	}
	if((boss.phase_frame <= 70) || ((boss.phase_frame % 14) != 0)) {
		return MarisaUpdateResult::boss;
	}
	callbacks.sound_effect(10U);
	if(boss.phase_frame <= 120) {
		for(std::size_t orb = 0U; orb < marisa_orb_count; ++orb) {
			if(!orb_is(marisa, orb, MarisaOrbFlag::alive)) continue;
			const auto point = marisa.orb_on_page[context.player_frame.page_back][orb];
			const auto result = add_star(context,
				{word_add(point.x, 8), word_add(point.y, 8)}, 0xC0U,
				static_cast<std::uint8_t>(special_one), 70);
			if(result != MarisaUpdateResult::boss) return result;
		}
		const auto result = add_star(context,
			{word_add(marisa.topleft.x, marisa_center_offset),
				word_add(marisa.topleft.y, marisa_center_offset)}, 0xC0U,
			static_cast<std::uint8_t>(special_one), 70);
		if(result != MarisaUpdateResult::boss) return result;
	} else {
		const auto rain_left = (marisa.pattern_side == 1) ? std::int16_t{400} :
			playfield_left;
		for(std::size_t i = 0U; i < boss.rank_param[3U]; ++i) {
			const auto angle = static_cast<std::uint8_t>((randring_next8(context.random) & 7U) +
				static_cast<std::uint8_t>(marisa.pattern_side << 4) + 0x40U);
			auto result = add_star(context,
				{static_cast<std::int16_t>((randring_next16(context.random) % 368U) +
					static_cast<std::uint16_t>(playfield_left)), playfield_top}, angle,
				static_cast<std::uint8_t>(special_one),
				static_cast<std::int16_t>((randring_next8(context.random) & 0x1FU) + 0x10U));
			if(result != MarisaUpdateResult::boss) return result;
			result = add_star(context,
				{rain_left, static_cast<std::int16_t>((randring_next16(context.random) % 320U) +
					static_cast<std::uint16_t>(playfield_top))}, angle,
				static_cast<std::uint8_t>(special_one),
				static_cast<std::int16_t>((randring_next8(context.random) & 0x1FU) + 0x10U));
			if(result != MarisaUpdateResult::boss) return result;
		}
	}
	if(marisa.orb_flag_sum == 8) {
		for(std::size_t i = 0U; i < marisa_orb_count; ++i) {
			const auto result = add_star(context,
				{word_add(marisa.topleft.x, marisa_center_offset),
					word_add(marisa.topleft.y, marisa_center_offset)},
				static_cast<std::uint8_t>((i << 6U) +
					static_cast<std::uint16_t>(boss.phase_frame) * 2U),
				enemy_bullet_special_motion::drift_angle_chase, 70);
			if(result != MarisaUpdateResult::boss) return result;
		}
	}
	return MarisaUpdateResult::boss;
}

void drift(BossState &boss, MarisaState &marisa,
	MarisaCombatContext &context) noexcept
{
	if(boss.phase_frame == 1) return;
	if(boss.phase_frame == 2) {
		marisa.velocity_x = (word_add(marisa.topleft.x, 32) <
			context.player_frame.player.current.x) ? 1 : -1;
		marisa.velocity_y = (marisa.topleft.y < 72) ? 1 :
			((marisa.topleft.y > 108) ? -1 : static_cast<std::int16_t>(
				1 - (randring_next8(context.random) % 3U)));
	}
	if(boss.phase_frame < 50) {
		auto &back = boss.topleft_on_page[context.player_frame.page_back];
		back.x = word_add(back.x, marisa.velocity_x);
		back.y = word_add(back.y, marisa.velocity_y);
		marisa.topleft = back;
	}
}

} // namespace

void marisa_combat_reset(BossState &boss, MarisaState &marisa,
	std::int8_t rank) noexcept
{
	const auto topleft = ScreenPoint{176, 64};
	boss.topleft_on_page[0U] = topleft;
	boss.topleft_on_page[1U] = topleft;
	marisa.topleft = topleft;
	boss.patnum = boss_patnum_rest;
	boss.damage = 0;
	boss.phase = 0U;
	boss.phase_frame = 0;
	marisa.hit_flash = false;
	marisa.intro_step = 0;
	marisa.damage_multiplier = 0U;
	for(std::size_t orb = 0U; orb < marisa_orb_count; ++orb) {
		marisa.orb_damage[orb] = 0;
		set_orb(marisa, orb, MarisaOrbFlag::removed);
		marisa.orb_hit_flash[orb] = 0;
	}
	marisa.spray_is_first_run = true;
	marisa.intro_direction = 0;
	marisa.pattern = 0;
	marisa.orbless_patterns_seen = 0;
	marisa.rounds_done = 0U;
	boss.explode_angle_offset = 0xE0U;
	marisa.bg_particle_col_i = 0;
	set_rank_parameters(boss, rank);
}

MarisaUpdateResult marisa_update(BossState &boss, MarisaState &marisa,
	MarisaCombatContext &context, MarisaCallbacks &callbacks
) noexcept
{
	marisa.orb_flag_sum = 0;
	for(const auto flag : marisa.orb_flag) {
		marisa.orb_flag_sum = word_add(marisa.orb_flag_sum, flag);
	}
	if(marisa.orb_flag_sum == 8) {
		boss.target = {word_add(marisa.topleft.x, marisa_center_offset),
			word_add(marisa.topleft.y, marisa_center_offset)};
	} else {
		boss.target = {-1, -1};
	}
	boss.phase_frame = word_add(boss.phase_frame, 1);
	const auto background = update_background_particles(marisa, context);
	if(background != MarisaUpdateResult::boss) return background;
	if((context.player_frame.page_back >= boss.topleft_on_page.size()) ||
		(context.player_frame.page_front >= boss.topleft_on_page.size())) {
		return MarisaUpdateResult::invalid_page;
	}

	if(boss.phase == 0U) {
		MarisaUpdateResult pattern = MarisaUpdateResult::boss;
		if(marisa.intro_step == 0) {
			pattern = pattern_minus_1(boss, marisa, context, callbacks);
			if(pattern != MarisaUpdateResult::boss) return pattern;
			if(boss.phase_frame == 0) marisa.intro_step = word_add(marisa.intro_step, 1);
		} else if(marisa.intro_step == 1) {
			spawn_orbs(boss, marisa, callbacks);
			if(boss.phase_frame == 0) {
				marisa.intro_step = word_add(marisa.intro_step, 1);
				marisa.pattern = 1;
				marisa.patterns_seen = 0U;
			}
		} else if(marisa.intro_step == 2) {
			switch(marisa.pattern) {
			case -3:
				pattern = pattern_minus_3(boss, marisa, context, callbacks);
				if(pattern != MarisaUpdateResult::boss) return pattern;
				drift(boss, marisa, context);
				break;
			case -2:
				pattern = pattern_minus_2(boss, marisa, context, callbacks);
				if(pattern != MarisaUpdateResult::boss) return pattern;
				drift(boss, marisa, context);
				break;
			case -1:
				pattern = pattern_minus_1(boss, marisa, context, callbacks);
				if(pattern != MarisaUpdateResult::boss) return pattern;
				drift(boss, marisa, context);
				break;
			case 0:
				spawn_orbs(boss, marisa, callbacks);
				break;
			case 1:
				pattern = pattern_1(boss, marisa, context);
				if(pattern != MarisaUpdateResult::boss) return pattern;
				break;
			case 2:
				pattern = pattern_2(boss, marisa, context, callbacks);
				if(pattern != MarisaUpdateResult::boss) return pattern;
				drift(boss, marisa, context);
				break;
			case 3:
				pattern = pattern_3(boss, marisa, context, callbacks);
				if(pattern != MarisaUpdateResult::boss) return pattern;
				drift(boss, marisa, context);
				break;
			case 4:
				pattern = pattern_4(boss, marisa, context, callbacks);
				if(pattern != MarisaUpdateResult::boss) return pattern;
				drift(boss, marisa, context);
				break;
			case 5:
				pattern = pattern_5(boss, marisa, context, callbacks);
				if(pattern != MarisaUpdateResult::boss) return pattern;
				drift(boss, marisa, context);
				break;
			case 6:
				pattern = pattern_6(boss, marisa, context, callbacks);
				if(pattern != MarisaUpdateResult::boss) return pattern;
				drift(boss, marisa, context);
				break;
			case pattern_unstarted:
				drift(boss, marisa, context);
				if(boss.phase_frame > 30) {
					if(marisa.orb_flag_sum < 8) {
						if(marisa.patterns_seen >= 4U) {
							marisa.pattern = 6;
						} else {
							marisa.pattern = static_cast<std::int8_t>(
								(randring_next8(context.random) % 5U) + 1U);
							marisa.patterns_seen = static_cast<std::uint8_t>(
								marisa.patterns_seen + 1U);
						}
					} else if(marisa.orbless_patterns_seen >= 2) {
						marisa.patterns_seen = 0U;
						marisa.pattern = 0;
						marisa.orbless_patterns_seen = 0;
						marisa.rounds_done = static_cast<std::uint8_t>(marisa.rounds_done + 1U);
						if(marisa.rounds_done >= 2U) marisa.damage_multiplier = 1U;
						if(marisa.rounds_done >= 7U) boss.phase = 1U;
					} else {
						marisa.orbless_patterns_seen = static_cast<std::int8_t>(
							marisa.orbless_patterns_seen + 1);
						marisa.pattern = static_cast<std::int8_t>(255U -
							(randring_next8(context.random) % 3U));
					}
					boss.phase_frame = 1;
				}
				break;
			default:
				break;
			}
			if(boss.phase_frame == 0) marisa.pattern = pattern_unstarted;
		}
	}

	const auto hit = marisa_hittest(boss, marisa, context, callbacks);
	if(hit != MarisaUpdateResult::boss) return hit;
	const auto orb_hit = orb_hittest(marisa, context, callbacks);
	if(orb_hit != MarisaUpdateResult::boss) return orb_hit;
	if(boss.phase != 0U) {
		const auto defeat = defeat_update(boss, marisa, context, callbacks);
		if(defeat != MarisaUpdateResult::boss) return defeat;
	} else {
		render_marisa(boss, marisa, callbacks);
	}
	for(std::size_t orb = 0U; orb < marisa_orb_count; ++orb) {
		if(!orb_is(marisa, orb, MarisaOrbFlag::kill_anim)) continue;
		const auto killed = orb_kill_update(marisa, orb, context, callbacks);
		if(killed != MarisaUpdateResult::boss) return killed;
	}
	update_orb_positions(marisa, context);
	render_orbs(marisa, context, callbacks);
	return MarisaUpdateResult::boss;
}

} // namespace th02::core
