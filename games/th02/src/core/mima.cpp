#include "th02/core/mima.hpp"

#include "th02/core/bullet.hpp"
#include "th02/core/item.hpp"
#include "th02/core/shot.hpp"
#include "th02/core/random.hpp"
#include "th02/core/score.hpp"
#include "th02/core/spark.hpp"
#include "th02/core/stage_state.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"

#include <bit>
#include <cstdint>
#include <optional>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_top = 16;
constexpr std::int16_t playfield_right = 416;
constexpr std::int16_t playfield_bottom = 384;
constexpr std::int16_t screen_bottom = 400;
constexpr ScreenPoint mima_ring_center{224, 200};
constexpr std::int16_t mima_pattern_width = 48;
constexpr std::int16_t mima_damage_max = 6000;
constexpr std::uint8_t boss_defeat_invincibility_frames = 200U;
constexpr std::int16_t mima_orb_reach = 16;
constexpr std::int16_t mima_orb_patnum = 137;
constexpr std::int16_t bullet_patnum_star = 85;
constexpr std::int16_t bullet_patnum_ball = 86;

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

[[nodiscard]] bool pages_valid(const StageState &state) noexcept
{
	return (state.frame.page_back < state.boss.topleft_on_page.size()) &&
		(state.frame.page_front < state.boss.topleft_on_page.size());
}

[[nodiscard]] MimaUpdateResult map_bullet(EnemyBulletAddResult result) noexcept
{
	switch(result) {
	case EnemyBulletAddResult::completed:
		return MimaUpdateResult::boss;
	case EnemyBulletAddResult::invalid_page:
		return MimaUpdateResult::bullet_invalid_page;
	case EnemyBulletAddResult::source_iatan2_fault:
		return MimaUpdateResult::bullet_source_iatan2_fault;
	}
	return MimaUpdateResult::bullet_source_iatan2_fault;
}

[[nodiscard]] MimaUpdateResult add_pellet(StageState &state,
	ScreenPoint origin, std::uint8_t angle, std::uint8_t group,
	std::int16_t speed) noexcept
{
	return map_bullet(bullets_add_pellet(state.bullets, state.random,
		state.frame, state.player_miss.playperf, state.rank, state.bomb.bombing,
		origin, angle, group, speed));
}

[[nodiscard]] MimaUpdateResult add_ball(StageState &state,
	ScreenPoint origin, std::uint8_t angle, std::uint8_t group,
	std::int16_t patnum, std::int16_t speed) noexcept
{
	return map_bullet(bullets_add_16x16(state.bullets, state.random,
		state.frame, state.player_miss.playperf, state.rank, state.bomb.bombing,
		origin, angle, group, patnum, speed));
}

[[nodiscard]] ScreenPoint polar(std::uint8_t angle,
	std::int16_t radius) noexcept
{
	const auto vector = pc98::masterlib::vector2(angle, radius);
	return {vector.x, vector.y};
}

[[nodiscard]] std::optional<ScreenPoint> vector_between(
	ScreenPoint from, ScreenPoint to, std::uint8_t plus_angle,
	std::int16_t length
) noexcept
{
	const auto angle = pc98::masterlib::iatan2_source(
		word_subtract(to.y, from.y), word_subtract(to.x, from.x));
	if(!angle) return std::nullopt;
	return polar(static_cast<std::uint8_t>(*angle + plus_angle), length);
}

[[nodiscard]] ScreenPoint &boss_back(StageState &state) noexcept
{
	return state.boss.topleft_on_page[state.frame.page_back];
}

[[nodiscard]] const ScreenPoint &player_front(const StageState &state) noexcept
{
	return state.frame.player.on_page[state.frame.page_front];
}

void set_rank_parameters(BossState &boss, std::int8_t rank) noexcept
{
	if(rank != 0) {
		boss.rank_param = {enemy_bullet_group::ring_16,
			enemy_bullet_group::spread_5_medium_aimed,
			enemy_bullet_group::ring_32, enemy_bullet_group::ring_4, 6U};
		return;
	}
	boss.rank_param = {enemy_bullet_group::ring_16,
		enemy_bullet_group::one_aimed,
		enemy_bullet_group::spread_2_medium_aimed,
		enemy_bullet_group::ring_2, 8U};
}

void ring_pass(MimaState &mima, MimaCallbacks &callbacks, std::uint8_t color,
	std::uint8_t angle_skew, std::uint8_t angle_step, bool with_circle,
	std::int16_t angle_start) noexcept
{
	const auto edge = static_cast<std::uint8_t>((mima.bg_ring_radius >> 6U) + 1U);
	callbacks.grcg_set_color(color);
	for(auto counter = angle_start; counter < angle_start + 0x80;
		counter = word_add(counter, static_cast<std::int16_t>(angle_step))) {
		const auto offset = polar(static_cast<std::uint8_t>(counter),
			static_cast<std::int16_t>(mima.bg_ring_radius));
		const auto offset_y = polar(static_cast<std::uint8_t>(
			static_cast<std::uint8_t>(counter) + angle_skew),
			static_cast<std::int16_t>(mima.bg_ring_radius));
		const auto point = ScreenPoint{word_add(mima_ring_center.x, offset.x),
			word_add(mima_ring_center.y, offset_y.y)};
		if((point.x < playfield_right) && (point.x > 24) &&
			(point.y > 8) && (point.y < playfield_bottom)) {
			callbacks.dot_square(point, edge);
		}
		if(with_circle && (counter == 256)) {
			callbacks.grcg_set_color(mima.bg_circle_col);
			callbacks.grcg_circle_fill(mima_ring_center,
				static_cast<std::int16_t>(mima.bg_circle_radius));
			callbacks.grcg_set_color(color);
		}
	}
	callbacks.grcg_off();
}

[[nodiscard]] MimaUpdateResult hit_test(StageState &state,
	MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	if(state.boss.phase != 0U) return MimaUpdateResult::boss;
	const auto &back = boss_back(state);
	PlayerShotSparkAllocatorCallbacks sparks(state.sparks, state.frame.page_back);
	const auto damage = shots_hittest(state.player_shots, state.score,
		state.frame.page_back, state.player_stream.shot_level, state.stage_frame,
		state.bomb.bombing, state.bomb_damage_frame_mask,
		{{word_add(back.x, 48), word_add(back.y, 32)}, 64, 64}, sparks);
	if(!damage) return MimaUpdateResult::invalid_shot_hittest;
	if(!sparks.all_allocations_succeeded()) {
		return MimaUpdateResult::shot_spark_allocation_refused;
	}
	if(*damage != 0) {
		mima.hit_flash = true;
		state.boss.damage = word_add(state.boss.damage, static_cast<std::int16_t>(
			static_cast<std::int32_t>(mima.damage_multiplier) * *damage));
		if(state.boss.damage >= mima_damage_max) {
			callbacks.sound_effect(2U);
			state.boss.phase = 1U;
			state.score.score_delta = dword_add(state.score.score_delta, 500000);
			state.player_hit.invincibility_time = boss_defeat_invincibility_frames;
		}
	}
	const auto &player = player_front(state);
	if((player.x > word_add(back.x, 16)) && (player.x < word_add(back.x, 112)) &&
		(player.y > back.y) && (player.y < word_add(back.y, 80))) {
		state.player_hit.raw_hit = 1U;
	}
	return MimaUpdateResult::boss;
}

[[nodiscard]] MimaUpdateResult render_orbs(StageState &state,
	MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	mima.orbs_gone_unused = 0;
	for(std::size_t orb = 0U; orb < mima_orb_slot_count; ++orb) {
		if(mima.orb_flag[orb] != 1) {
			mima.orbs_gone_unused = word_add(mima.orbs_gone_unused, 1);
			continue;
		}
		const auto point = mima.orb_on_page[state.frame.page_back][orb];
		PlayerShotSparkAllocatorCallbacks sparks(state.sparks, state.frame.page_back);
		const auto damage = shots_hittest(state.player_shots, state.score,
			state.frame.page_back, state.player_stream.shot_level, state.stage_frame,
			state.bomb.bombing, state.bomb_damage_frame_mask,
			{{word_add(point.x, 4), point.y}, 24, 32}, sparks);
		if(!damage) return MimaUpdateResult::invalid_shot_hittest;
		if(!sparks.all_allocations_succeeded()) {
			return MimaUpdateResult::shot_spark_allocation_refused;
		}
		const auto &player = player_front(state);
		if((word_subtract(point.x, mima_orb_reach) < player.x) &&
			(word_add(point.x, mima_orb_reach) > player.x) &&
			(word_subtract(point.y, mima_orb_reach) < player.y) &&
			(word_add(point.y, mima_orb_reach) > player.y)) {
			state.player_hit.raw_hit = 1U;
		}
		callbacks.render_mima_orb({orb, point, word_add(mima_orb_patnum,
			static_cast<std::int16_t>(orb & 3U))});
	}
	return MimaUpdateResult::boss;
}

void render_mima(StageState &state, MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	const auto &back = boss_back(state);
	if(mima.hit_flash) {
		mima.flash_frame = static_cast<std::uint8_t>(mima.flash_frame + 1U);
		if((mima.flash_frame & 3U) == 0U) {
			mima.hit_flash = false;
			for(std::int16_t part = 0; part < 3; ++part) {
				callbacks.render_mima({MimaSpriteKind::hit_flash,
					{word_add(back.x, static_cast<std::int16_t>(part * mima_pattern_width)),
						back.y}, word_add(state.boss.patnum, part)});
			}
			return;
		}
	}
	for(std::int16_t part = 0; part < 3; ++part) {
		callbacks.render_mima({MimaSpriteKind::normal,
			{word_add(back.x, static_cast<std::int16_t>(part * mima_pattern_width)),
				back.y}, word_add(state.boss.patnum, part)});
	}
}

[[nodiscard]] MimaUpdateResult horizontal_sweep(StageState &state) noexcept
{
	auto &mima = state.mima;
	auto &boss = state.boss;
	auto &back = boss_back(state);
	if(boss.phase_frame < 10) return MimaUpdateResult::boss;
	if(boss.phase_frame == 10) {
		bullets_set_stack_multiplier(state.bullets, 0U);
		boss.patnum = 131;
	}
	if(boss.phase_frame < 66) {
		back.x = word_subtract(back.x, 2);
		if(back.x <= 32) boss.phase_frame = 66;
	} else if(boss.phase_frame < 178) {
		back.x = word_add(back.x, 2);
		if(back.x >= 256) boss.phase_frame = 178;
	} else if(boss.phase_frame < 234) {
		back.x = word_subtract(back.x, 2);
		if(back.x <= 144) boss.phase_frame = 234;
	} else {
		boss.phase_frame = 0;
		boss.patnum = 128;
		bullets_set_stack_multiplier(state.bullets, 1U);
	}
	if((boss.phase_frame % 32) != 0) return MimaUpdateResult::boss;
	auto result = add_pellet(state, {mima.muzzle_left, mima.muzzle_top},
		static_cast<std::uint8_t>((randring_next8(state.random) & 0x0FU) + 0x38U),
		boss.rank_param[0U], 60);
	if(result != MimaUpdateResult::boss) return result;
	return add_pellet(state, {mima.muzzle_left, mima.muzzle_top}, 0U,
		boss.rank_param[static_cast<std::size_t>(1U + (mima.all_patterns ? 1U : 0U))],
		40);
}

[[nodiscard]] MimaUpdateResult ray_column(StageState &state,
	MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	auto &boss = state.boss;
	auto &back = boss_back(state);
	if(boss.phase_frame < 20) return MimaUpdateResult::boss;
	if(boss.phase_frame < 70) {
		boss.patnum = 128;
		back.x = word_add(back.x, word_add(back.x, 64) < state.frame.player.current.x ? 2 : -2);
		return MimaUpdateResult::boss;
	}
	if(boss.phase_frame == 70) {
		callbacks.sound_effect(9U);
		boss.patnum = 131;
		mima.ray_unused = 0x30U;
		mima.ray_angle = 0U;
		mima.ray_tone = 100U;
		return MimaUpdateResult::boss;
	}
	if(boss.phase_frame < 170) {
		callbacks.grcg_set_color(13U);
		auto angle_step = static_cast<std::uint8_t>(0x40U - mima.ray_angle);
		angle_step = static_cast<std::uint8_t>(angle_step >> 2U);
		for(std::uint8_t i = 0U, angle = mima.ray_angle; i < 9U;
			++i, angle = static_cast<std::uint8_t>(angle + angle_step)) {
			const auto offset = polar(angle, 400);
			callbacks.grcg_line({mima.muzzle_left, mima.muzzle_top},
				{word_add(mima.muzzle_left, offset.x), word_add(mima.muzzle_top, offset.y)});
		}
		mima.ray_angle = static_cast<std::uint8_t>(mima.ray_angle + 1U);
		callbacks.grcg_off();
		if((static_cast<std::uint16_t>(boss.phase_frame) & 1U) != 0U) {
			mima.ray_tone = static_cast<std::uint8_t>(mima.ray_tone + 1U);
		}
		callbacks.palette_tone_set(mima.ray_tone);
		return MimaUpdateResult::boss;
	}
	if(boss.phase_frame == 170) {
		callbacks.sound_effect(5U);
		boss.patnum = 134;
		return MimaUpdateResult::boss;
	}
	if(boss.phase_frame <= 198) {
		back.y = word_subtract(272, static_cast<std::int16_t>(
			(static_cast<std::uint16_t>(boss.phase_frame) & 3U) * 60U));
		callbacks.grcg_set_color(13U);
		const auto column_x = word_add(word_add(back.x, 8),
			static_cast<std::int16_t>(randring_next8(state.random) & 0x7FU));
		callbacks.grcg_line({column_x, playfield_top}, {column_x,
			word_subtract(playfield_bottom, 1)});
		callbacks.grcg_off();
		if((word_add(back.x, 16) < state.frame.player.current.x) &&
			(word_add(back.x, 96) > state.frame.player.current.x)) {
			state.player_hit.raw_hit = 1U;
		}
		mima.ray_tone = 100U;
		callbacks.palette_tone_set(mima.ray_tone);
		return MimaUpdateResult::boss;
	}
	if(boss.phase_frame <= 262) {
		boss.patnum = 128;
		back.y = word_subtract(back.y, 4);
		if(back.y <= 64) boss.phase_frame = 263;
		if(state.rank == 0) return MimaUpdateResult::boss;
		if((boss.phase_frame % 16) != 0) return MimaUpdateResult::boss;
		return add_pellet(state, {mima.left_26c56, mima.top_26c5e}, 0U,
			mima.all_patterns ? enemy_bullet_group::ring_32 : enemy_bullet_group::ring_16,
			60);
	}
	boss.phase_frame = 0;
	return MimaUpdateResult::boss;
}

[[nodiscard]] MimaUpdateResult orb_flight(StageState &state,
	ScreenPoint &velocity, bool variant, MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	auto &boss = state.boss;
	const auto page = state.frame.page_back;
	const auto &back = boss_back(state);
	if(boss.phase_frame == 40) {
		callbacks.sound_effect(9U);
		boss.patnum = 131;
		mima.orb_flight_center_x = static_cast<std::int16_t>(
			static_cast<std::uint16_t>(word_add(back.x, 80)) << 4U);
		mima.orb_flight_center_y = static_cast<std::int16_t>(
			static_cast<std::uint16_t>(word_add(back.y, 64)) << 4U);
		mima.orb_flight_radius = 4;
		mima.orb_flight_radius_step = -1;
		mima.orb_flight_detonated = false;
	} else if(boss.phase_frame < 80) {
		return MimaUpdateResult::boss;
	} else if(boss.phase_frame == 80) {
		callbacks.sound_effect(10U);
		boss.patnum = 134;
		for(std::size_t orb = 0U; orb < mima_orb_count; ++orb) {
			const auto offset = polar(static_cast<std::uint8_t>(mima.orb_flight_angle +
				static_cast<std::uint8_t>(orb << 6U)), mima.orb_flight_radius);
			const auto point = ScreenPoint{word_add(arithmetic_shift_right(
				mima.orb_flight_center_x, 4U), offset.x), word_add(arithmetic_shift_right(
				mima.orb_flight_center_y, 4U), offset.y)};
			mima.orb_on_page[0U][orb] = point;
			mima.orb_on_page[1U][orb] = point;
			mima.orb_flag[orb] = 1;
		}
	} else if(boss.phase_frame < 96) {
		mima.orb_flight_radius = word_add(mima.orb_flight_radius, 4);
		for(std::size_t orb = 0U; orb < mima_orb_count; ++orb) {
			const auto offset = polar(static_cast<std::uint8_t>(mima.orb_flight_angle +
				static_cast<std::uint8_t>(orb << 6U)), mima.orb_flight_radius);
			mima.orb_on_page[page][orb] = {word_add(arithmetic_shift_right(
				mima.orb_flight_center_x, 4U), offset.x), word_add(arithmetic_shift_right(
				mima.orb_flight_center_y, 4U), offset.y)};
		}
	} else {
		bool retire_now{};
		mima.orb_flight_center_x = word_add(mima.orb_flight_center_x, velocity.x);
		mima.orb_flight_center_y = word_add(mima.orb_flight_center_y, velocity.y);
		mima.orb_flight_radius = word_add(mima.orb_flight_radius,
			static_cast<std::int16_t>(mima.orb_flight_radius_step));
		if(variant) {
			if(mima.orb_flight_detonated) {
				mima.orb_flight_angle = static_cast<std::uint8_t>(mima.orb_flight_angle -
					(mima.all_patterns ? 3U : 5U));
			}
			if(mima.orb_flight_radius >= 400) retire_now = true;
			else if(mima.orb_flight_radius < 20) {
				velocity = {};
				mima.orb_flight_detonated = true;
				callbacks.sound_effect(3U);
				auto angle = static_cast<std::uint8_t>(mima.orb_flight_angle + 8U);
				for(std::size_t orb = 0U; orb < mima_orb_count; ++orb,
					angle = static_cast<std::uint8_t>(angle + 0x40U)) {
					const auto point = mima.orb_on_page[page][orb];
					for(std::int16_t speed = 0; speed < 48; speed = word_add(speed, 12)) {
						const auto result = add_pellet(state,
							{word_add(point.x, 12), word_add(point.y, 12)},
							static_cast<std::uint8_t>(angle + static_cast<std::uint8_t>(speed)),
							enemy_bullet_group::one, word_add(speed, 35));
						if(result != MimaUpdateResult::boss) return result;
					}
				}
				boss.patnum = 128;
				mima.orb_flight_radius_step = 5;
			}
		} else if(!mima.all_patterns) {
			if(mima.orb_flight_radius < 32) mima.orb_flight_radius_step = 2;
			else if(mima.orb_flight_radius > 128) mima.orb_flight_radius_step = -2;
		} else {
			if(mima.orb_flight_radius == 196) boss.phase_frame = 0;
			if(mima.orb_flight_radius < 20) {
				mima.orb_flight_detonated = false;
				callbacks.sound_effect(3U);
				bullets_set_stack_multiplier(state.bullets, 0U);
				for(std::size_t orb = 0U; orb < mima_orb_count; ++orb) {
					const auto point = mima.orb_on_page[page][orb];
					for(std::int16_t speed = 0, angle = static_cast<std::int16_t>(
						static_cast<std::int16_t>(orb << 6U) + mima.orb_flight_angle - 0x40);
						speed < 16; speed = word_add(speed, 1), angle = word_add(angle, 8)) {
						const auto result = add_pellet(state,
							{word_add(point.x, 12), word_add(point.y, 12)},
							static_cast<std::uint8_t>(angle), enemy_bullet_group::one, 35);
						if(result != MimaUpdateResult::boss) return result;
					}
					for(std::int16_t speed = 0, angle = static_cast<std::int16_t>(
						static_cast<std::int16_t>(orb << 6U) + mima.orb_flight_angle - 0x3C);
						speed < 16; speed = word_add(speed, 1), angle = word_add(angle, 8)) {
						const auto result = add_pellet(state,
							{word_add(point.x, 12), word_add(point.y, 12)},
							static_cast<std::uint8_t>(angle), enemy_bullet_group::one,
							word_subtract(35, speed));
						if(result != MimaUpdateResult::boss) return result;
					}
					mima.orb_flag[orb] = 2;
				}
				bullets_set_stack_multiplier(state.bullets, 1U);
				mima.orb_flight_radius = 320;
				boss.patnum = 128;
			}
		}
		{
			const auto left = arithmetic_shift_right(mima.orb_flight_center_x, 4U);
			const auto top = arithmetic_shift_right(mima.orb_flight_center_y, 4U);
			if(retire_now || (word_add(left, mima.orb_flight_radius) < 0) ||
				(word_subtract(left, mima.orb_flight_radius) > 416) ||
				(word_add(top, mima.orb_flight_radius) < -16) ||
				(word_subtract(top, mima.orb_flight_radius) > 386) ||
				(boss.phase_frame > 500)) {
				for(std::size_t orb = 0U; orb < mima_orb_count; ++orb) mima.orb_flag[orb] = 2;
				boss.phase_frame = 0;
				boss.patnum = 128;
			}
			for(std::size_t orb = 0U; orb < mima_orb_count; ++orb) {
				const auto offset = polar(static_cast<std::uint8_t>(mima.orb_flight_angle +
					static_cast<std::uint8_t>(orb << 6U)), mima.orb_flight_radius);
				mima.orb_on_page[page][orb] = {word_add(left, offset.x),
					word_add(top, offset.y)};
			}
		}
	}
	mima.orb_flight_angle = static_cast<std::uint8_t>(mima.orb_flight_angle + 5U);
	return MimaUpdateResult::boss;
}

[[nodiscard]] MimaUpdateResult flying_orbs(StageState &state, bool variant,
	MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	if(state.boss.phase_frame == 40) {
		const auto origin = ScreenPoint{word_add(boss_back(state).x, 80),
			word_add(boss_back(state).y, 64)};
		const auto velocity = vector_between(origin, state.frame.player.current, 0U,
			variant ? 52 : 48);
		if(!velocity) return MimaUpdateResult::source_iatan2_fault;
		mima.orb_variant = variant;
		(variant ? mima.orb_velocity_b : mima.orb_velocity_a) = *velocity;
	}
	return orb_flight(state, variant ? mima.orb_velocity_b : mima.orb_velocity_a,
		variant, callbacks);
}

[[nodiscard]] MimaUpdateResult pellet_stream(StageState &state,
	MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	auto &boss = state.boss;
	auto &back = boss_back(state);
	if(boss.phase_frame < 10) return MimaUpdateResult::boss;
	if(boss.phase_frame < 70) {
		boss.patnum = 128;
		back.x = word_add(back.x, word_add(back.x, 64) < state.frame.player.current.x ? 2 : -2);
	} else if(boss.phase_frame == 70) {
		callbacks.sound_effect(9U); boss.patnum = 131; mima.stream_speed = 20U;
	} else if(boss.phase_frame < 100) {
		return MimaUpdateResult::boss;
	} else if(boss.phase_frame == 100) {
		callbacks.sound_effect(10U); boss.patnum = 134; mima.stream_angle = randring_next8(state.random);
	} else if(boss.phase_frame <= 160) {
		mima.stream_angle = static_cast<std::uint8_t>(mima.stream_angle - 1U);
	} else if(boss.phase_frame <= 240) {
		mima.stream_angle = static_cast<std::uint8_t>(mima.stream_angle + 1U);
		mima.stream_speed = static_cast<std::uint8_t>(mima.stream_speed + 1U);
	} else if(mima.all_patterns) {
		if(boss.phase_frame <= 320) {
			mima.stream_angle = static_cast<std::uint8_t>(mima.stream_angle + 1U);
			mima.stream_speed = static_cast<std::uint8_t>(mima.stream_speed - 1U);
		} else if(boss.phase_frame <= 400) {
			mima.stream_angle = static_cast<std::uint8_t>(mima.stream_angle - 1U);
			mima.stream_speed = static_cast<std::uint8_t>(mima.stream_speed + 1U);
		} else { boss.phase_frame = 0; boss.patnum = 128; }
	} else { boss.phase_frame = 0; boss.patnum = 128; }
	if(boss.phase_frame <= 100) return MimaUpdateResult::boss;
	if((boss.phase_frame % (8 - state.rank)) != 0) return MimaUpdateResult::boss;
	const auto result = add_pellet(state, {mima.left_26c5a, mima.top_26c62},
		mima.stream_angle, boss.rank_param[3U], static_cast<std::int16_t>(mima.stream_speed));
	if(result == MimaUpdateResult::boss) callbacks.sound_effect(3U);
	return result;
}

[[nodiscard]] MimaUpdateResult pellet_pairs(StageState &state,
	MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	auto &boss = state.boss;
	auto &back = boss_back(state);
	if(boss.phase_frame < 10) return MimaUpdateResult::boss;
	if(boss.phase_frame < 70) {
		boss.patnum = 128;
		back.x = word_add(back.x, word_add(back.x, 64) < state.frame.player.current.x ? 2 : -2);
		return MimaUpdateResult::boss;
	}
	if(boss.phase_frame == 70) {
		callbacks.sound_effect(9U); boss.patnum = 131; mima.pair_angle = 0xD8U;
		return MimaUpdateResult::boss;
	}
	if(boss.phase_frame < 100) return MimaUpdateResult::boss;
	if(boss.phase_frame == 100) {
		callbacks.sound_effect(10U); boss.patnum = 134; return MimaUpdateResult::boss;
	}
	if(boss.phase_frame <= 132) {
		const auto result = add_pellet(state, {mima.left_26c5a, mima.top_26c62},
			mima.pair_angle, enemy_bullet_group::spread_2_horizontally_symmetric, 90);
		mima.pair_angle = static_cast<std::uint8_t>(mima.pair_angle + 3U);
		return result;
	}
	if(boss.phase_frame <= 150) {
		const auto result = add_pellet(state, {mima.left_26c5a, mima.top_26c62},
			mima.pair_angle, enemy_bullet_group::spread_2_horizontally_symmetric, 90);
		mima.pair_angle = static_cast<std::uint8_t>(mima.pair_angle - 3U);
		return result;
	}
	if(boss.phase_frame <= 226) {
		if((boss.phase_frame % 8) != 1) return MimaUpdateResult::boss;
		if(!mima.all_patterns) return add_pellet(state,
			{mima.left_26c5a, mima.top_26c62}, 0x40U,
			enemy_bullet_group::spread_3_narrow, 90);
		auto result = add_pellet(state, {mima.left_26c5a, mima.top_26c62},
			static_cast<std::uint8_t>((randring_next8(state.random) & 0x3FU) + 0x20U),
			enemy_bullet_group::one, 80);
		if(result != MimaUpdateResult::boss) return result;
		return add_pellet(state, {mima.left_26c5a, mima.top_26c62},
			static_cast<std::uint8_t>((randring_next8(state.random) & 0x3FU) + 0x20U),
			enemy_bullet_group::one, 80);
	}
	boss.phase_frame = 0; boss.patnum = 128;
	return MimaUpdateResult::boss;
}

void defeat_item(StageState &state, MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	auto &boss = state.boss;
	if(boss.phase_frame < 10) return;
	if(boss.phase_frame == 10) {
		callbacks.sound_effect(9U); boss.patnum = 131; return;
	}
	if(boss.phase_frame < 40) return;
	if(boss.phase_frame == 40) {
		callbacks.sound_effect(10U); boss.patnum = 134;
		if(mima.phase == 6) { items_add(state.items, state.score, mima.left_26c5a,
			mima.top_26c62, static_cast<std::int16_t>(ItemType::bomb)); return; }
	} else if(boss.phase_frame == 60) {
		callbacks.sound_effect(10U);
	} else if(boss.phase_frame == 80) {
		callbacks.sound_effect(10U); boss.patnum = 128;
		if(mima.phase == 2) { items_add(state.items, state.score, mima.left_26c5a,
			mima.top_26c62, static_cast<std::int16_t>(ItemType::one_up)); return; }
	} else return;
	items_add(state.items, state.score, mima.left_26c5a, mima.top_26c62,
		static_cast<std::int16_t>(ItemType::big_power));
}

[[nodiscard]] MimaUpdateResult bouncing_stars(StageState &state,
	MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	auto &boss = state.boss;
	auto &back = boss_back(state);
	if(boss.phase_frame < 10) return MimaUpdateResult::boss;
	if(boss.phase_frame < 70) {
		boss.patnum = 128;
		back.x = word_add(back.x, word_add(back.x, 64) < state.frame.player.current.x ? 2 : -2);
		return MimaUpdateResult::boss;
	}
	if(boss.phase_frame == 70) { callbacks.sound_effect(9U); boss.patnum = 131; return MimaUpdateResult::boss; }
	if(boss.phase_frame < 100) return MimaUpdateResult::boss;
	if(boss.phase_frame == 100) {
		callbacks.sound_effect(10U); boss.patnum = 134;
		const auto aimed = pc98::masterlib::iatan2_source(
			word_subtract(state.frame.player.current.y, mima.top_26c62),
			word_subtract(state.frame.player.current.x, mima.left_26c5a));
		if(!aimed) return MimaUpdateResult::source_iatan2_fault;
		mima.star_angle = *aimed;
		mima.star_direction = (state.frame.player.current.x <= 192) ? -1 : 1;
		state.bullet_special.turns_or_drift_frames = mima.all_patterns ? 1 : 0;
		return MimaUpdateResult::boss;
	}
	if(boss.phase_frame <= 300) {
		if((boss.phase_frame % 8) == 0) {
			callbacks.sound_effect(10U);
			const auto fire = [&](std::uint8_t delta) noexcept {
				return add_ball(state, {mima.left_26c5a, mima.top_26c62},
					static_cast<std::uint8_t>(mima.star_angle + delta),
					enemy_bullet_special_motion::bounce_top_bottom,
					bullet_patnum_star, 80);
			};
			auto result = fire(state.rank != 0 ? 0x0AU : 0x0FU);
			if(result != MimaUpdateResult::boss) return result;
			result = fire(state.rank != 0 ? 0xF6U : 0xF1U);
			if(result != MimaUpdateResult::boss) return result;
			if(state.rank != 0) {
				result = fire(0x1EU); if(result != MimaUpdateResult::boss) return result;
				result = fire(0xE2U); if(result != MimaUpdateResult::boss) return result;
			}
		}
		if(boss.phase_frame < 150 || (boss.phase_frame & 1) == 0) return MimaUpdateResult::boss;
		mima.star_angle = static_cast<std::uint8_t>(mima.star_angle +
			static_cast<std::uint8_t>((boss.phase_frame < 210) ? mima.star_direction : -mima.star_direction));
		return MimaUpdateResult::boss;
	}
	boss.phase_frame = 0; boss.patnum = 128;
	return MimaUpdateResult::boss;
}

[[nodiscard]] MimaUpdateResult pellet_fan(StageState &state,
	MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	auto &boss = state.boss;
	auto &back = boss_back(state);
	const auto fire = [&]() noexcept -> MimaUpdateResult {
		auto result = add_pellet(state, {mima.left_26c5a, mima.top_26c62},
			static_cast<std::uint8_t>(mima.fan_angle + 8U),
			enemy_bullet_group::spread_3_narrow, 80);
		if(result != MimaUpdateResult::boss) return result;
		result = add_pellet(state, {mima.left_26c5a, mima.top_26c62},
			static_cast<std::uint8_t>(0x78U - mima.fan_angle),
			enemy_bullet_group::spread_3_narrow, 80);
		mima.fan_angle = static_cast<std::uint8_t>(mima.fan_angle + 10U);
		return result;
	};
	if(boss.phase_frame < 30) return MimaUpdateResult::boss;
	if(boss.phase_frame < 70) {
		boss.patnum = 128;
		back.x = word_add(back.x, word_add(back.x, 64) < state.frame.player.current.x ? 2 : -2);
		return MimaUpdateResult::boss;
	}
	if(boss.phase_frame == 70) { callbacks.sound_effect(9U); boss.patnum = 131; return MimaUpdateResult::boss; }
	if(boss.phase_frame < 120) return MimaUpdateResult::boss;
	if(boss.phase_frame == 120) { callbacks.sound_effect(3U); boss.patnum = 134; mima.fan_angle = 4U; return fire(); }
	if(boss.phase_frame <= 160) {
		if((boss.phase_frame % 10) != 0) return MimaUpdateResult::boss;
		callbacks.sound_effect(3U); return fire();
	}
	boss.phase_frame = 0; boss.patnum = 128;
	return MimaUpdateResult::boss;
}

[[nodiscard]] MimaUpdateResult expanding_orbs(StageState &state,
	MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	auto &boss = state.boss;
	const auto page = state.frame.page_back;
	const auto &back = boss_back(state);
	if(boss.phase_frame < 40) return MimaUpdateResult::boss;
	if(boss.phase_frame == 40) {
		callbacks.sound_effect(9U); boss.patnum = 131;
		mima.orb_center_x = word_add(back.x, 80); mima.orb_center_y = word_add(back.y, 80);
		mima.orb_radius = 4; mima.orb_angle = randring_next8(state.random);
	} else if(boss.phase_frame < 80) return MimaUpdateResult::boss;
	else if(boss.phase_frame == 80) {
		callbacks.sound_effect(10U); boss.patnum = 134;
		for(std::size_t orb = 0U; orb < mima_orb_count; ++orb) {
			const auto offset = polar(static_cast<std::uint8_t>(mima.orb_angle +
				static_cast<std::uint8_t>(orb << 6U)), mima.orb_radius);
			const auto point = ScreenPoint{word_add(arithmetic_shift_right(mima.orb_center_x, 4U), offset.x),
				word_add(arithmetic_shift_right(mima.orb_center_y, 4U), offset.y)};
			mima.orb_on_page[0U][orb] = point; mima.orb_on_page[1U][orb] = point;
			mima.orb_flag[orb] = 1;
		}
	} else {
		mima.orb_radius = word_add(mima.orb_radius, 2);
		for(std::size_t orb = 0U; orb < mima_orb_count; ++orb) {
			const auto offset = polar(static_cast<std::uint8_t>(mima.orb_angle +
				static_cast<std::uint8_t>(orb << 6U)), mima.orb_radius);
			auto &point = mima.orb_on_page[page][orb];
			point = {word_add(mima.orb_center_x, offset.x), word_add(mima.orb_center_y, offset.y)};
			if((word_subtract(180, static_cast<std::int16_t>(state.rank << 4)) < mima.orb_radius) &&
				((static_cast<std::uint16_t>(boss.phase_frame) & 3U) == 0U)) {
				callbacks.sound_effect(3U);
				if((point.x > 0) && (point.x < playfield_right) && (point.y > 0) &&
					(point.y < screen_bottom)) {
					const auto angle = static_cast<std::uint8_t>((orb << 6U) + mima.orb_angle +
						(mima.all_patterns ? 0x60U : 0x40U));
					const auto result = add_pellet(state, point, angle,
						enemy_bullet_group::one, 128);
					if(result != MimaUpdateResult::boss) return result;
				}
			}
		}
		if(mima.orb_radius > 460) {
			for(std::size_t orb = 0U; orb < mima_orb_count; ++orb) mima.orb_flag[orb] = 0;
			boss.phase_frame = 0; boss.patnum = 128;
		}
	}
	mima.orb_angle = static_cast<std::uint8_t>(mima.orb_angle - 3U);
	return MimaUpdateResult::boss;
}

[[nodiscard]] MimaUpdateResult phase9_spiral(StageState &state,
	MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	const auto frame = state.boss.phase_frame;
	if(frame < 50) return MimaUpdateResult::boss;
	if(frame == 50) { mima.spiral_angle = 0U; return MimaUpdateResult::boss; }
	if(frame < 150) {
		if((frame & 1) != 0) {
			callbacks.sound_effect(3U);
			const auto result = add_ball(state, {mima.x_26c5c, mima.y_26c64},
				mima.spiral_angle, enemy_bullet_group::spread_2_horizontally_symmetric,
				bullet_patnum_ball, 50);
			mima.spiral_angle = static_cast<std::uint8_t>(mima.spiral_angle + state.boss.rank_param[4U]);
			return result;
		}
		return MimaUpdateResult::boss;
	}
	state.boss.phase_frame = 0;
	return MimaUpdateResult::boss;
}

[[nodiscard]] MimaUpdateResult phase9_charge(StageState &state,
	MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	auto &boss = state.boss;
	if(boss.phase_frame < 50) return MimaUpdateResult::boss;
	if(boss.phase_frame == 50) {
		const auto aimed = pc98::masterlib::iatan2_source(
			word_subtract(word_add(state.frame.player.current.y, 12), mima.y_26c64),
			word_subtract(word_add(state.frame.player.current.x, 12), mima.x_26c5c));
		if(!aimed) return MimaUpdateResult::source_iatan2_fault;
		mima.aim_angle_unused = *aimed; mima.charge_ring_radius = 30U;
		callbacks.sound_effect(9U); return MimaUpdateResult::boss;
	}
	if(boss.phase_frame <= 80) {
		if((boss.phase_frame & 1) == 0) return MimaUpdateResult::boss;
		callbacks.grcg_set_color(0U);
		for(const auto scale : {1, 2, 4}) callbacks.grcg_circle({mima.x_26c5c, mima.y_26c64},
			static_cast<std::int16_t>(mima.charge_ring_radius * scale));
		mima.charge_ring_radius = static_cast<std::uint8_t>(mima.charge_ring_radius - 2U);
		callbacks.grcg_set_color(13U);
		for(const auto scale : {1, 2, 4}) callbacks.grcg_circle({mima.x_26c5c, mima.y_26c64},
			static_cast<std::int16_t>(mima.charge_ring_radius * scale));
		callbacks.grcg_off();
		return MimaUpdateResult::boss;
	}
	if(boss.phase_frame < 200) {
		if((boss.phase_frame & 7) == 0) {
			auto result = add_ball(state, {mima.x_26c5c, mima.y_26c64}, 2U,
				enemy_bullet_group::random_angle_and_speed, bullet_patnum_ball, 60);
			if(result != MimaUpdateResult::boss) return result;
			result = add_pellet(state, {mima.x_26c5c, mima.y_26c64}, 2U,
				enemy_bullet_group::random_angle_and_speed, 60);
			if(result != MimaUpdateResult::boss) return result;
		}
		if((state.rank > 0) && ((boss.phase_frame & 7) == 4)) {
			auto result = add_ball(state, {mima.x_26c5c, mima.y_26c64}, 2U,
				enemy_bullet_group::random_angle_and_speed, bullet_patnum_ball, 60);
			if(result != MimaUpdateResult::boss) return result;
			result = add_pellet(state, {mima.x_26c5c, mima.y_26c64}, 2U,
				enemy_bullet_group::random_angle_and_speed, 60);
			if(result != MimaUpdateResult::boss) return result;
		}
		if((boss.phase_frame & 7) == 0) callbacks.sound_effect(3U);
		return MimaUpdateResult::boss;
	}
	boss.phase_frame = 0;
	return MimaUpdateResult::boss;
}

[[nodiscard]] MimaUpdateResult phase9_spread(StageState &state) noexcept
{
	const auto frame = state.boss.phase_frame;
	if(frame < 50) return MimaUpdateResult::boss;
	if(frame < 190) {
		if((frame & 31) == 0) return add_ball(state, {state.mima.x_26c5c, state.mima.y_26c64},
			0U, enemy_bullet_group::spread_5_medium_aimed, bullet_patnum_ball, 90);
		if((frame & 31) == 16) return add_ball(state, {state.mima.x_26c5c, state.mima.y_26c64},
			0U, enemy_bullet_group::spread_4_medium_aimed, bullet_patnum_ball, 90);
		return MimaUpdateResult::boss;
	}
	state.boss.phase_frame = 0;
	return MimaUpdateResult::boss;
}

void advance_pattern(StageState &state) noexcept
{
	auto &mima = state.mima;
	auto &boss = state.boss;
	if(boss.phase_frame == 0) {
		if((boss.damage >= mima.phase_damage_max) ||
			(mima.patterns_this_phase > mima.patterns_max)) {
			mima.phase = word_add(mima.phase, 1);
			mima.damage_multiplier = 0;
			mima.patterns_this_phase = 0;
		} else {
			mima.patterns_this_phase = word_add(mima.patterns_this_phase, 1);
			if(mima.patterns_this_phase >= mima.patterns_until_vulnerable) {
				mima.damage_multiplier = 1;
			}
			mima.pattern = word_add(mima.pattern, 1);
			if((!mima.all_patterns && (mima.pattern >= mima.pattern_count)) ||
				(mima.all_patterns && (mima.pattern > 7))) mima.pattern = 0;
			auto &back = boss_back(state);
			if(back.y < 48) mima.velocity_y = 1;
			else if(back.y > 64) mima.velocity_y = -1;
			else mima.velocity_y = static_cast<std::int16_t>(1 -
				static_cast<std::int16_t>(randring_next8(state.random) % 3U));
		}
	}
	if(boss.phase_frame < 10) boss_back(state).y = word_add(boss_back(state).y, mima.velocity_y);
}

[[nodiscard]] MimaUpdateResult phase_update(StageState &state,
	MimaCallbacks &callbacks) noexcept
{
	auto &mima = state.mima;
	auto &boss = state.boss;
	const auto charge_palette = [&](std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
		callbacks.palette_set(0U, {r, g, b}); callbacks.palette_show();
	};
	if(mima.phase == 0) {
		mima.bg_ring_radius = static_cast<std::uint8_t>(boss.phase_frame / 2);
		mima.bg_circle_radius = static_cast<std::uint8_t>(boss.phase_frame / 3);
		const auto value = static_cast<std::uint8_t>(boss.phase_frame >> 1);
		charge_palette(value, 0U, value);
		if(boss.phase_frame > 100) {
			mima.bg_circle_radius_base = mima.bg_circle_radius; mima.bg_circle_pulse_frame = 0;
			mima.phase = 1; boss.phase_frame = 0;
			mima.pattern = static_cast<std::int16_t>(randring_next8(state.random) % 3U);
			mima.patterns_this_phase = 0; mima.phase_damage_max = 600;
			mima.patterns_until_vulnerable = 2; mima.patterns_max = 10;
			mima.pattern_count = 3; boss.damage = 0;
		}
		return MimaUpdateResult::boss;
	}
	if(mima.phase == 1) {
		MimaUpdateResult result = MimaUpdateResult::boss;
		if(mima.pattern == 0) result = horizontal_sweep(state);
		else if(mima.pattern == 1) result = ray_column(state, callbacks);
		else if(mima.pattern == 2) result = flying_orbs(state, false, callbacks);
		if(result != MimaUpdateResult::boss) return result;
		advance_pattern(state); return MimaUpdateResult::boss;
	}
	if(mima.phase == 2 || mima.phase == 4 || mima.phase == 6) {
		const auto offset = mima.phase == 2 ? 50 : (mima.phase == 4 ? 75 : 100);
		const auto circle = mima.phase == 2 ? 30 : (mima.phase == 4 ? 50 : 70);
		mima.bg_ring_radius = static_cast<std::uint8_t>(boss.phase_frame / 4 + offset);
		mima.bg_circle_radius = static_cast<std::uint8_t>(boss.phase_frame / 5 + circle);
		defeat_item(state, callbacks);
		const auto half = static_cast<std::uint8_t>(boss.phase_frame >> 1);
		if(mima.phase == 2) charge_palette(static_cast<std::uint8_t>(51U - half), half, 50U);
		else if(mima.phase == 4) charge_palette(half, static_cast<std::uint8_t>(51U - half),
			static_cast<std::uint8_t>(51U - half));
		else charge_palette(static_cast<std::uint8_t>(51U - half), 0U, 0U);
		if(boss.phase_frame > 100) {
			mima.bg_circle_radius_base = mima.bg_circle_radius;
			mima.phase = word_add(mima.phase, 1); boss.phase_frame = 0;
			mima.pattern = (mima.phase == 7) ? static_cast<std::int16_t>(randring_next8(state.random) & 7U) :
				static_cast<std::int16_t>(randring_next8(state.random) % 3U);
			mima.patterns_this_phase = 0;
			if(mima.phase == 3) { mima.phase_damage_max = 700; mima.patterns_until_vulnerable = 2; mima.patterns_max = 12; mima.pattern_count = 3; }
			else if(mima.phase == 5) { mima.phase_damage_max = 800; mima.patterns_until_vulnerable = 2; mima.patterns_max = 12; mima.pattern_count = 3; }
			else { mima.phase_damage_max = 1500; mima.patterns_until_vulnerable = 3; mima.patterns_max = 200; mima.pattern_count = 9; mima.all_patterns = true; mima.bg_ring_col_tail = 2U; }
			boss.damage = 0;
		}
		return MimaUpdateResult::boss;
	}
	MimaUpdateResult result = MimaUpdateResult::boss;
	const bool entered_phase9 = mima.phase == 9;
	if(mima.phase == 3) {
		if(mima.pattern == 0) result = pellet_stream(state, callbacks);
		else if(mima.pattern == 1) result = pellet_pairs(state, callbacks);
		else if(mima.pattern == 2) result = flying_orbs(state, true, callbacks);
	} else if(mima.phase == 5) {
		if(mima.pattern == 0) result = bouncing_stars(state, callbacks);
		else if(mima.pattern == 1) result = pellet_fan(state, callbacks);
		else if(mima.pattern == 2) result = expanding_orbs(state, callbacks);
	} else if(mima.phase == 7) {
		switch(mima.pattern) {
		case 0: result = bouncing_stars(state, callbacks); break;
		case 1: result = flying_orbs(state, false, callbacks); break;
		case 2: result = pellet_stream(state, callbacks); break;
		case 3: result = ray_column(state, callbacks); break;
		case 4: result = pellet_pairs(state, callbacks); break;
		case 5: result = flying_orbs(state, true, callbacks); break;
		case 6: result = horizontal_sweep(state); break;
		case 7: result = pellet_fan(state, callbacks); break;
		case 8: result = expanding_orbs(state, callbacks); break;
		default: break;
		}
	} else if(mima.phase == 9) {
		if(mima.pattern == 0) result = phase9_spiral(state, callbacks);
		else if(mima.pattern == 1) result = phase9_charge(state, callbacks);
		else if(mima.pattern == 2) result = phase9_spread(state);
	}
	if(result != MimaUpdateResult::boss) return result;
	if((mima.phase == 3) || (mima.phase == 5) || (mima.phase == 7) || entered_phase9) {
		advance_pattern(state);
		if(entered_phase9) {
			if(boss.phase_frame == 1) boss_back(state).y = 64;
			if(boss.phase_frame < 30) boss_back(state).x = word_add(boss_back(state).x,
				mima.x_26c5c < state.frame.player.current.x ? 1 : -1);
		}
	}
	return MimaUpdateResult::boss;
}

} // namespace

void mima_combat_reset(StageState &state) noexcept
{
	shots_free_all(state.player_shots);
	state.boss.damage = 0;
	state.boss.phase = 0U;
	state.boss.phase_frame = 0;
	state.mima.hit_flash = false;
	state.mima.phase = 0;
	state.mima.damage_multiplier = 0;
	state.mima.patterns_this_phase = 0;
	state.tile_mode = 2U;
	state.mima.bg_ring_radius = 0U;
	state.mima.bg_circle_radius = 0U;
	state.mima.bg_ring_col_head = 13U;
	state.mima.bg_circle_col = 3U;
	state.mima.bg_ring_col_tail = 12U;
	state.mima.pattern = 0;
	state.mima.orb_flag.fill(0);
	state.mima.all_patterns = false;
	state.mima.form1_defeat_pending = false;
	state.mima.final_end_pending = false;
	set_rank_parameters(state.boss, state.rank);
}

MimaUpdateResult mima_bg_render(StageState &state, MimaCallbacks &callbacks) noexcept
{
	if(!pages_valid(state)) return MimaUpdateResult::invalid_page;
	auto &mima = state.mima;
	for(std::size_t orb = 0U; orb < mima_orb_slot_count; ++orb) {
		const auto point = mima.orb_on_page[state.frame.page_back][orb];
		if((mima.orb_flag[orb] != 0) && (point.y > -mima_orb_reach) &&
			(point.y < word_subtract(playfield_bottom, 2))) {
			callbacks.invalidate_scenery({point, 32, 32});
			if(mima.orb_flag[orb] == 2) mima.orb_flag[orb] = 0;
		}
	}
	state.boss.topleft_on_page[state.frame.page_back] =
		state.boss.topleft_on_page[state.frame.page_front];
	callbacks.egc_off();
	callbacks.grcg_set_color(0U);
	callbacks.clear_boss_playfield();
	const auto angle_step = state.reduce_effects ? std::uint8_t{4U} : std::uint8_t{1U};
	ring_pass(mima, callbacks, mima.bg_ring_col_tail,
		static_cast<std::uint8_t>(mima.bg_ring_phase - 16U), angle_step, false, 128);
	ring_pass(mima, callbacks, mima.bg_ring_col_tail,
		static_cast<std::uint8_t>(mima.bg_ring_phase - 8U), angle_step, false, 128);
	ring_pass(mima, callbacks, mima.bg_ring_col_head, mima.bg_ring_phase,
		angle_step, false, 128);
	ring_pass(mima, callbacks, mima.bg_ring_col_tail,
		static_cast<std::uint8_t>(mima.bg_ring_phase - 16U), angle_step, true, 256);
	ring_pass(mima, callbacks, mima.bg_ring_col_tail,
		static_cast<std::uint8_t>(mima.bg_ring_phase - 8U), angle_step, false, 256);
	ring_pass(mima, callbacks, mima.bg_ring_col_head, mima.bg_ring_phase,
		angle_step, false, 256);
	mima.bg_ring_phase = static_cast<std::uint8_t>(mima.bg_ring_phase + 1U);
	callbacks.grcg_off();
	return MimaUpdateResult::boss;
}

MimaUpdateResult mima_update(StageState &state, MimaCallbacks &callbacks) noexcept
{
	if(state.mima.form1_defeat_pending) return MimaUpdateResult::form1_defeat_pending;
	if(state.mima.final_end_pending) return MimaUpdateResult::final_end_pending;
	state.boss.phase_frame = word_add(state.boss.phase_frame, 1);
	if(!pages_valid(state)) return MimaUpdateResult::invalid_page;
	auto &mima = state.mima;
	const auto &back = boss_back(state);
	state.boss.target = {word_add(back.x, 72), word_add(back.y, 56)};
	mima.left_26c56 = word_add(back.x, 32);
	mima.muzzle_left = word_add(back.x, 40);
	mima.left_26c5a = word_add(back.x, 64);
	mima.x_26c5c = word_add(back.x, 64);
	mima.top_26c5e = word_add(back.y, 96);
	mima.muzzle_top = word_add(back.y, 16);
	mima.top_26c62 = word_add(back.y, 114);
	mima.y_26c64 = word_add(back.y, 44);
	if((state.stage_frame & 1U) == 0U) {
		if(!state.reduce_effects) {
			mima.ring_radius = static_cast<std::uint8_t>(mima.ring_radius + 8U);
			callbacks.grcg_set_color(3U);
			for(std::uint8_t radius = mima.ring_radius, i = 0U; i < 2U; ++i,
				radius = static_cast<std::uint8_t>(radius + 128U)) {
				callbacks.dot_square_ring(mima_ring_center, radius, 16U);
			}
			callbacks.grcg_off();
		}
		if((static_cast<std::uint16_t>(mima.phase) & 1U) != 0U) {
			mima.bg_circle_pulse_frame = static_cast<std::uint8_t>(mima.bg_circle_pulse_frame + 1U);
			if(mima.bg_circle_pulse_frame >= 64U) mima.bg_circle_pulse_frame = 0U;
			mima.bg_circle_radius = static_cast<std::uint8_t>(mima.bg_circle_radius_base -
				mima.bg_circle_pulse_frame / 4U);
		}
	}
	const auto phase = phase_update(state, callbacks);
	if(phase != MimaUpdateResult::boss) return phase;
	const auto hit = hit_test(state, callbacks);
	if(hit != MimaUpdateResult::boss) return hit;
	render_mima(state, callbacks);
	const auto orbs = render_orbs(state, callbacks);
	if(orbs != MimaUpdateResult::boss) return orbs;
	if(mima.phase == 8) {
		mima.form1_defeat_pending = true;
		return MimaUpdateResult::form1_defeat_pending;
	}
	if(mima.phase == 10) {
		mima.final_end_pending = true;
		return MimaUpdateResult::final_end_pending;
	}
	return MimaUpdateResult::boss;
}

void mima_after_form1_defeat(StageState &state, MimaCallbacks &) noexcept
{
	if(!state.mima.form1_defeat_pending) return;
	auto &mima = state.mima;
	mima.phase = word_add(mima.phase, 1);
	state.boss.patnum = 128;
	mima.patterns_this_phase = 0;
	state.boss.phase_frame = 0;
	mima.pattern = static_cast<std::int16_t>(randring_next8(state.random) % 3U);
	mima.patterns_this_phase = 0;
	state.boss.damage = 0;
	mima.phase_damage_max = 1100;
	mima.patterns_until_vulnerable = 2;
	mima.patterns_max = 200;
	mima.pattern_count = 3;
	mima.bg_circle_col = 1U;
	mima.bg_ring_col_tail = 2U;
	mima.bg_ring_radius = 200U;
	mima.bg_circle_radius = 150U;
	mima.bg_circle_radius_base = mima.bg_circle_radius;
	mima.form1_defeat_pending = false;
}

} // namespace th02::core
