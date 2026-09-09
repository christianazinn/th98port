#include "th02/core/sigma.hpp"

#include "th02/core/bullet.hpp"
#include "th02/core/random.hpp"
#include "th02/core/score.hpp"
#include "th02/core/shot.hpp"
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
constexpr std::int16_t player_left_start = 192;
constexpr std::int16_t player_width = 32;
constexpr std::int16_t sigma_width = 128;
constexpr std::int16_t sigma_height = 64;
constexpr std::int16_t sigma_center_offset = 60;
constexpr std::int16_t sigma_patnum_rest = 128;
constexpr std::int16_t sigma_patnum_hit = 130;
constexpr std::int16_t sigma_patnum_defeat = 134;

[[nodiscard]] std::int16_t word_add(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs)));
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
		static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(rhs)));
	return std::bit_cast<std::int32_t>(bits);
}

[[nodiscard]] bool pages_valid(const StageState &state) noexcept
{
	return (state.frame.page_back < state.boss.topleft_on_page.size()) &&
		(state.frame.page_front < state.boss.topleft_on_page.size());
}

[[nodiscard]] ScreenPoint &boss_back(StageState &state) noexcept
{
	return state.boss.topleft_on_page[state.frame.page_back];
}

[[nodiscard]] const ScreenPoint &player_front(const StageState &state) noexcept
{
	return state.frame.player.on_page[state.frame.page_front];
}

[[nodiscard]] std::int16_t player_center_x(const StageState &state) noexcept
{
	return word_add(state.frame.player.current.x, player_width / 2);
}

[[nodiscard]] std::int16_t player_sigma_y(const StageState &state) noexcept
{
	return word_add(state.frame.player.current.y, player_width / 2);
}

[[nodiscard]] SigmaUpdateResult map_bullet(EnemyBulletAddResult result) noexcept
{
	switch(result) {
	case EnemyBulletAddResult::completed:
		return SigmaUpdateResult::boss;
	case EnemyBulletAddResult::invalid_page:
		return SigmaUpdateResult::bullet_invalid_page;
	case EnemyBulletAddResult::source_iatan2_fault:
		return SigmaUpdateResult::bullet_source_iatan2_fault;
	}
	return SigmaUpdateResult::bullet_source_iatan2_fault;
}

[[nodiscard]] SigmaUpdateResult add_pellet(StageState &state,
	ScreenPoint origin, std::uint8_t angle, std::uint8_t group,
	std::int16_t speed) noexcept
{
	return map_bullet(bullets_add_pellet(state.bullets, state.random,
		state.frame, state.player_miss.playperf, state.rank, state.bomb.bombing,
		origin, angle, group, speed));
}

[[nodiscard]] SigmaUpdateResult add_ball(StageState &state,
	ScreenPoint origin, std::uint8_t angle, std::uint8_t group,
	std::int16_t patnum, std::int16_t speed) noexcept
{
	return map_bullet(bullets_add_16x16(state.bullets, state.random,
		state.frame, state.player_miss.playperf, state.rank, state.bomb.bombing,
		origin, angle, group, patnum, speed));
}

[[nodiscard]] ScreenPoint polar(std::uint8_t angle, std::int16_t length) noexcept
{
	const auto vector = pc98::masterlib::vector2(angle, length);
	return {vector.x, vector.y};
}

[[nodiscard]] std::optional<ScreenPoint> vector_between(
	ScreenPoint from, ScreenPoint to, std::int16_t length
) noexcept
{
	const auto angle = pc98::masterlib::iatan2_source(
		word_subtract(to.y, from.y), word_subtract(to.x, from.x));
	if(!angle) return std::nullopt;
	return polar(*angle, length);
}

void sigma_put(const StageState &state, SigmaCallbacks &callbacks) noexcept
{
	callbacks.render_sigma({state.sigma.topleft, state.boss.patnum});
	callbacks.render_sigma({{word_add(state.sigma.topleft.x, sigma_width / 2),
		state.sigma.topleft.y}, word_add(state.boss.patnum, 1)});
}

[[nodiscard]] bool sigma_blasts_add(StageState &state, SigmaCallbacks &callbacks,
	std::int16_t x, std::int16_t y, std::int16_t radius_max) noexcept
{
	if((x > 0) && (x < 444) && (y > 0) && (y < screen_bottom)) {
		for(auto &blast : state.sigma.blasts) {
			if(blast.phase != static_cast<std::uint8_t>(SigmaBlastPhase::free)) continue;
			blast.phase = static_cast<std::uint8_t>(SigmaBlastPhase::telegraph);
			blast.phase_frame = 0U;
			blast.center = {x, y};
			blast.radius_max = radius_max;
			blast.patnum = 0x1AU;
			blast.radius = 64U;
			callbacks.sound_effect(9U);
			return false;
		}
	}
	return true;
}

void sigma_blasts_activate(SigmaState &sigma) noexcept
{
	for(auto &blast : sigma.blasts) {
		if(blast.phase == static_cast<std::uint8_t>(SigmaBlastPhase::activating)) {
			blast.phase = static_cast<std::uint8_t>(SigmaBlastPhase::expanding);
		}
	}
}

void sigma_blasts_update_and_render(StageState &state,
	SigmaCallbacks &callbacks) noexcept
{
	auto &sigma = state.sigma;
	for(std::size_t i = 0U; i < sigma.blasts.size(); ++i) {
		auto &blast = sigma.blasts[i];
		if(blast.phase == static_cast<std::uint8_t>(SigmaBlastPhase::free)) continue;
		blast.phase_frame = static_cast<std::uint8_t>(blast.phase_frame + 1U);
		if(blast.phase == static_cast<std::uint8_t>(SigmaBlastPhase::telegraph)) {
			if((blast.phase_frame & sigma.cel_interval_mask) == 0U) {
				blast.patnum = static_cast<std::uint8_t>(blast.patnum + 1U);
				if(blast.patnum >= 0x22U) {
					blast.phase = static_cast<std::uint8_t>(SigmaBlastPhase::activating);
					callbacks.sound_effect(3U);
				}
				blast.phase_frame = 0U;
				blast.radius = static_cast<std::uint8_t>(
					static_cast<std::int16_t>(blast.radius) - 8);
				callbacks.grcg_set_color(15U);
				callbacks.grcg_circle(blast.center, blast.radius);
				callbacks.grcg_off();
			}
			if(blast.phase == static_cast<std::uint8_t>(SigmaBlastPhase::telegraph)) {
				callbacks.render_sigma_blast({
					{word_subtract(blast.center.x, 16), word_subtract(blast.center.y, 16)},
					blast.patnum});
			}
		} else if(blast.phase == static_cast<std::uint8_t>(SigmaBlastPhase::expanding)) {
			if(state.frame.page_back == (i & 1U)) {
				callbacks.grcg_set_color(15U);
				callbacks.grcg_circle_fill(blast.center, blast.radius);
				callbacks.grcg_off();
				if(static_cast<std::int16_t>(blast.radius) < blast.radius_max) {
					blast.radius = static_cast<std::uint8_t>(blast.radius + 16U);
				}
			}
			if(blast.phase_frame > 32U) {
				blast.phase = static_cast<std::uint8_t>(SigmaBlastPhase::collapsing);
				blast.phase_frame = 0U;
				blast.radius_max = blast.radius;
				blast.radius = 8U;
			}
			const auto &player = player_front(state);
			const auto margin = static_cast<std::int16_t>(sigma.blast_hitbox_margin);
			if((word_add(word_subtract(blast.center.x,
				static_cast<std::int16_t>(blast.radius)), margin) < player.x) &&
				(word_subtract(word_subtract(word_add(blast.center.x,
					static_cast<std::int16_t>(blast.radius)), margin), 32) > player.x) &&
				(word_add(word_subtract(blast.center.y,
					static_cast<std::int16_t>(blast.radius)), margin) < player.y) &&
				(word_subtract(word_subtract(word_add(blast.center.y,
					static_cast<std::int16_t>(blast.radius)), margin), 32) > player.y)) {
				state.player_hit.raw_hit = 1U;
			}
		} else if(blast.phase == static_cast<std::uint8_t>(SigmaBlastPhase::collapsing)) {
			if(state.frame.page_back == (i & 1U)) {
				callbacks.grcg_set_color(15U);
				callbacks.grcg_circle_fill(blast.center, blast.radius_max);
				callbacks.grcg_off();
				callbacks.grcg_set_color(0U);
				callbacks.grcg_circle_fill(blast.center, blast.radius);
				callbacks.grcg_off();
				if(static_cast<std::int16_t>(blast.radius) >= blast.radius_max) {
					blast.phase = static_cast<std::uint8_t>(SigmaBlastPhase::free);
				} else {
					blast.radius = static_cast<std::uint8_t>(blast.radius + 16U);
				}
			}
		}
	}
}

[[nodiscard]] SigmaUpdateResult sigma_hittest_and_put(StageState &state,
	SigmaCallbacks &callbacks) noexcept
{
	const auto patnum_prev = state.boss.patnum;
	if(state.boss.phase == 0U) {
		PlayerShotSparkAllocatorCallbacks sparks(state.sparks, state.frame.page_back);
		const auto damage = shots_hittest(state.player_shots, state.score,
			state.frame.page_back, state.player_stream.shot_level, state.stage_frame,
			state.bomb.bombing, state.bomb_damage_frame_mask,
			{{word_add(boss_back(state).x, 36), word_add(boss_back(state).y, 32)}, 56, 32},
			sparks);
		if(!damage) return SigmaUpdateResult::invalid_shot_hittest;
		if(!sparks.all_allocations_succeeded()) {
			return SigmaUpdateResult::shot_spark_allocation_refused;
		}
		if(*damage != 0) {
			state.boss.patnum = sigma_patnum_hit;
			state.boss.damage = word_add(state.boss.damage, *damage);
		}
		const auto &player = player_front(state);
		if((player.x > word_add(state.sigma.topleft.x, 8)) &&
			(player.x < word_add(state.sigma.topleft.x, 88)) &&
			(player.y > state.sigma.topleft.y) &&
			(player.y < word_add(state.sigma.topleft.y, sigma_height))) {
			state.player_hit.raw_hit = 1U;
		}
	}
	sigma_put(state, callbacks);
	state.boss.patnum = patnum_prev;
	return SigmaUpdateResult::boss;
}

[[nodiscard]] bool sigma_defeat(StageState &state, SigmaCallbacks &callbacks) noexcept
{
	const auto frame = state.boss.phase_frame;
	auto &sigma = state.sigma;
	if(frame < 10) {
		sigma_put(state, callbacks);
		return false;
	}
	if(frame == 10) {
		sigma.beam_half_w = 8U;
		sigma_put(state, callbacks);
		callbacks.sound_effect(9U);
	} else if(frame < 90) {
		sigma_put(state, callbacks);
		if(state.frame.page_back == 0U) {
			callbacks.grcg_set_color(15U);
			callbacks.grcg_circle_fill({word_add(sigma.topleft.x, sigma_width / 2),
				word_add(sigma.topleft.y, sigma_height)}, 24);
			callbacks.grcg_off();
		}
		if((frame >= 50) && (state.frame.page_back != 0U)) {
			callbacks.grcg_set_color(15U);
			callbacks.grcg_vline(word_add(sigma.topleft.x, sigma_width / 2),
				playfield_top, word_subtract(playfield_bottom, 1));
			callbacks.grcg_off();
		}
		if(frame == 89) callbacks.sound_effect(3U);
	} else if(frame < 100) {
		callbacks.grcg_set_color(15U);
		callbacks.grcg_bytebox_fill(static_cast<std::uint8_t>(
			word_subtract(word_add(sigma.topleft.x, sigma_width / 2),
				static_cast<std::int16_t>(sigma.beam_half_w)) / 8), playfield_top,
			static_cast<std::uint8_t>(word_add(word_add(sigma.topleft.x, sigma_width / 2),
				static_cast<std::int16_t>(sigma.beam_half_w)) / 8),
			word_subtract(playfield_bottom, 1));
		callbacks.grcg_off();
		const auto &player = player_front(state);
		if((word_subtract(word_add(sigma.topleft.x, 56),
			static_cast<std::int16_t>(sigma.beam_half_w)) < player.x) &&
			(player.x < word_add(word_add(sigma.topleft.x, 40),
				static_cast<std::int16_t>(sigma.beam_half_w)))) {
			state.player_hit.raw_hit = 1U;
		}
		sigma.beam_half_w = static_cast<std::uint8_t>(sigma.beam_half_w + 8U);
		sigma_put(state, callbacks);
	} else if(frame == 100) {
		sigma.beam_hitbox_left = word_subtract(word_add(sigma.topleft.x, 56),
			static_cast<std::int16_t>(sigma.beam_half_w));
		sigma.beam_hitbox_right = word_add(word_add(sigma.topleft.x, 40),
			static_cast<std::int16_t>(sigma.beam_half_w));
		sigma.beam_vram_left = static_cast<std::uint8_t>(word_subtract(
			word_add(sigma.topleft.x, sigma_width / 2),
			static_cast<std::int16_t>(sigma.beam_half_w)) / 8);
		sigma.beam_vram_right = static_cast<std::uint8_t>(word_subtract(
			static_cast<std::int16_t>(word_add(
				word_add(sigma.topleft.x, sigma_width / 2),
				static_cast<std::int16_t>(sigma.beam_half_w)) / 8), 1));
		state.boss.patnum = sigma_patnum_defeat;
	} else if(frame <= 140) {
		callbacks.grcg_set_color(15U);
		callbacks.grcg_bytebox_fill(sigma.beam_vram_left, playfield_top,
			sigma.beam_vram_right, word_subtract(playfield_bottom, 1));
		callbacks.grcg_off();
		sigma_put(state, callbacks);
		const auto &player = player_front(state);
		if((player.x > sigma.beam_hitbox_left) && (player.x < sigma.beam_hitbox_right)) {
			state.player_hit.raw_hit = 1U;
		}
	} else if(frame < 180) {
		callbacks.grcg_set_color(15U);
		callbacks.grcg_bytebox_fill(sigma.beam_vram_left, playfield_top,
			sigma.beam_vram_right, word_subtract(playfield_bottom, 1));
		callbacks.grcg_off();
		if((frame % 10) == 0) state.boss.patnum = word_add(state.boss.patnum, 2);
		sigma_put(state, callbacks);
		const auto &player = player_front(state);
		if((player.x > sigma.beam_hitbox_left) && (player.x < sigma.beam_hitbox_right)) {
			state.player_hit.raw_hit = 1U;
		}
		const auto shake = static_cast<std::uint16_t>(frame & 1);
		callbacks.graph_scroll(static_cast<std::uint16_t>(screen_bottom - state.scroll.line),
			static_cast<std::uint16_t>(state.scroll.line * 40 + shake), shake);
		state.bomb_animation.slowdown_factor = 2U;
	} else if(frame <= 220) {
		state.bomb_animation.slowdown_factor = 1U;
		callbacks.grcg_set_color(15U);
		callbacks.grcg_bytebox_fill(sigma.beam_vram_left, playfield_top,
			sigma.beam_vram_right, word_subtract(playfield_bottom, 1));
		callbacks.grcg_off();
		const auto &player = player_front(state);
		if((player.x > sigma.beam_hitbox_left) && (player.x < sigma.beam_hitbox_right)) {
			state.player_hit.raw_hit = 1U;
		}
	} else if(frame < 224) {
		callbacks.grcg_set_color(0U);
		callbacks.grcg_bytebox_fill(sigma.beam_vram_left, playfield_top,
			sigma.beam_vram_right, word_subtract(playfield_bottom, 1));
		callbacks.grcg_off();
	} else {
		return true;
	}
	return false;
}

[[nodiscard]] bool move_weave(StageState &state) noexcept
{
	auto &frame = state.boss.phase_frame;
	if(frame < 50) return true;
	if(frame == 50) {
		state.sigma.weave_velocity_x = state.frame.player.current.x < player_left_start ? 1 : -1;
	}
	auto &back = boss_back(state);
	if(frame < 114) {
		back.x = word_add(back.x, state.sigma.weave_velocity_x);
		back.y = word_add(back.y, 1);
	} else if(frame < 242) {
		back.x = word_subtract(back.x, state.sigma.weave_velocity_x);
	} else if(frame < 306) {
		back.x = word_add(back.x, state.sigma.weave_velocity_x);
		back.y = word_subtract(back.y, 1);
	} else {
		frame = 0;
	}
	return false;
}

[[nodiscard]] bool move_sweep(StageState &state) noexcept
{
	auto &frame = state.boss.phase_frame;
	if(frame < 50) return true;
	if(frame == 50) {
		state.sigma.sweep_velocity_x = state.frame.player.current.x < player_left_start ? 2 : -2;
	}
	auto &back = boss_back(state);
	if(frame < 130) back.x = word_add(back.x, state.sigma.sweep_velocity_x);
	else if(frame < 290) back.x = word_subtract(back.x, state.sigma.sweep_velocity_x);
	else if(frame < 370) back.x = word_add(back.x, state.sigma.sweep_velocity_x);
	else frame = 0;
	return false;
}

[[nodiscard]] SigmaUpdateResult pattern_dash_spray(StageState &state,
	SigmaCallbacks &callbacks) noexcept
{
	auto &sigma = state.sigma;
	auto &frame = state.boss.phase_frame;
	if(frame < 20) return SigmaUpdateResult::boss;
	if(frame == 20) {
		sigma.spray_half_angle = 0U;
		sigma.dash_velocity_x = state.frame.player.current.x < player_left_start ? 1 : -1;
	}
	if((frame % 4) == 0) callbacks.sound_effect(3U);
	const auto add_drop = [&]() noexcept {
		return add_pellet(state, {word_add(sigma.topleft.x,
			static_cast<std::int16_t>(randring_next8(state.random) & 0x7FU)),
			word_add(sigma.topleft.y, sigma_height)}, 0x40U,
			enemy_bullet_group::one,
			static_cast<std::int16_t>((randring_next8(state.random) & 0x1FU) + 30U));
	};
	if(frame < 148) {
		boss_back(state).x = word_add(boss_back(state).x, sigma.dash_velocity_x);
		if((frame & 1) != 0) return add_drop();
	} else if(frame < 212) {
		boss_back(state).x = word_subtract(boss_back(state).x,
			static_cast<std::int16_t>(sigma.dash_velocity_x * 4));
		if((frame & 1) != 0) return add_drop();
	} else if(frame < 340) {
		boss_back(state).x = word_add(boss_back(state).x, sigma.dash_velocity_x);
		if((frame & 1) != 0) return add_drop();
	} else if(frame < 440) {
		sigma.spray_half_angle = static_cast<std::uint8_t>(sigma.spray_half_angle + 1U);
		const auto span = static_cast<std::uint16_t>(sigma.spray_half_angle) * 2U;
		const auto angle = static_cast<std::uint8_t>((randring_next8(state.random) % span) +
			static_cast<std::uint16_t>(0x40U - sigma.spray_half_angle));
		if((frame & 3) != 0) return SigmaUpdateResult::boss;
		for(std::size_t i = 0U; i < 3U; ++i) {
			const auto result = add_pellet(state, {word_add(sigma.topleft.x,
				static_cast<std::int16_t>(randring_next8(state.random) & 0x7FU)),
				word_add(sigma.topleft.y, sigma_height)}, angle,
				enemy_bullet_group::one,
				static_cast<std::int16_t>((randring_next8(state.random) & 0x1FU) + 30U));
			if(result != SigmaUpdateResult::boss) return result;
		}
	} else {
		frame = 0;
	}
	return SigmaUpdateResult::boss;
}

[[nodiscard]] SigmaUpdateResult pattern_traps(StageState &state,
	SigmaCallbacks &callbacks) noexcept
{
	auto &sigma = state.sigma;
	const auto frame = state.boss.phase_frame;
	if(frame < 100) return SigmaUpdateResult::boss;
	if(frame == 100) {
		sigma.cel_interval_mask = 7U;
		sigma.blast_hitbox_margin = 4;
		(void)sigma_blasts_add(state, callbacks, 128, 320, 64);
	} else if(frame == 120) {
		(void)sigma_blasts_add(state, callbacks, 320, 320, 64);
	} else if((frame > 130) && (frame < 150) && ((frame % 4) == 0)) {
		const auto result = add_pellet(state, {sigma.center_x, sigma.center_y}, 0U,
			enemy_bullet_group::spread_5_medium_aimed, 120);
		if(result != SigmaUpdateResult::boss) return result;
	}
	if(frame > 200) state.boss.phase_frame = 0;
	return SigmaUpdateResult::boss;
}

[[nodiscard]] SigmaUpdateResult pattern_weave_ring32(StageState &state) noexcept
{
	if(move_weave(state)) return SigmaUpdateResult::boss;
	if((state.boss.phase_frame & 0x1F) == 0) {
		return add_pellet(state, {state.sigma.center_x, state.sigma.center_y},
			randring_next8(state.random), enemy_bullet_group::ring_32, 85);
	}
	return SigmaUpdateResult::boss;
}

[[nodiscard]] SigmaUpdateResult pattern_orbit_blasts(StageState &state,
	SigmaCallbacks &callbacks) noexcept
{
	auto &sigma = state.sigma;
	const auto frame = state.boss.phase_frame;
	if(frame < 50) return SigmaUpdateResult::boss;
	if(frame == 50) callbacks.sound_effect(9U);
	if(frame < 100) {
		state.boss.patnum = static_cast<std::int16_t>(state.frame.page_back * 4U + 128U);
		return SigmaUpdateResult::boss;
	}
	if(frame == 100) {
		sigma.orbit_center_x = player_center_x(state);
		sigma.orbit_center_y = player_sigma_y(state);
		sigma.cel_interval_mask = 3U;
		sigma.blast_hitbox_margin = -4;
		sigma.orbit_angle = 0U;
		state.boss.patnum = sigma_patnum_rest;
	}
	if(frame < 130) {
		if((frame % 8) == 0) {
			callbacks.grcg_set_color(15U);
			callbacks.grcg_circle({sigma.orbit_center_x, sigma.orbit_center_y}, 112);
		} else if((frame % 8) == 2) {
			callbacks.grcg_set_color(0U);
			callbacks.grcg_circle({sigma.orbit_center_x, sigma.orbit_center_y}, 112);
		}
		callbacks.grcg_off();
		return SigmaUpdateResult::boss;
	}
	if(frame < 450) {
		if(state.frame.page_back == 0U) {
			callbacks.grcg_set_color(15U);
			callbacks.grcg_circle({sigma.orbit_center_x, sigma.orbit_center_y}, 112);
			callbacks.grcg_off();
		}
		if(frame >= 150 && (frame % 12) == 0) {
			const auto first = polar(sigma.orbit_angle, 112);
			(void)sigma_blasts_add(state, callbacks,
				word_add(sigma.orbit_center_x, first.x), word_add(sigma.orbit_center_y, first.y), 24);
			const auto antipodal = static_cast<std::uint8_t>(sigma.orbit_angle + 0x80U);
			const auto second = polar(antipodal, 112);
			(void)sigma_blasts_add(state, callbacks,
				word_add(sigma.orbit_center_x, second.x), word_add(sigma.orbit_center_y, second.y), 24);
			sigma.orbit_angle = static_cast<std::uint8_t>(sigma.orbit_angle - 0x10U);
		}
		if(frame < 200) return SigmaUpdateResult::boss;
		if((frame & 0x0F) == 0) {
			return add_pellet(state, {sigma.center_x, sigma.center_y},
				randring_next8(state.random), enemy_bullet_group::ring_32, 80);
		}
	} else if(state.frame.page_back == 0U) {
		callbacks.grcg_set_color(0U);
		callbacks.grcg_circle({sigma.orbit_center_x, sigma.orbit_center_y}, 112);
		callbacks.grcg_off();
		state.boss.phase_frame = 0;
	}
	return SigmaUpdateResult::boss;
}

[[nodiscard]] SigmaUpdateResult pattern_weave_ring16(StageState &state) noexcept
{
	if(move_weave(state)) return SigmaUpdateResult::boss;
	if((state.boss.phase_frame & 7) == 0) {
		return add_pellet(state, {state.sigma.center_x, state.sigma.center_y},
			randring_next8(state.random), enemy_bullet_group::ring_16, 50);
	}
	return SigmaUpdateResult::boss;
}

[[nodiscard]] SigmaUpdateResult pattern_billiards(StageState &state,
	SigmaCallbacks &callbacks) noexcept
{
	auto &sigma = state.sigma;
	const auto frame = state.boss.phase_frame;
	if(frame < 50) return SigmaUpdateResult::boss;
	if(frame == 50) callbacks.sound_effect(9U);
	if(frame < 100) {
		state.boss.patnum = static_cast<std::int16_t>(state.frame.page_back * 4U + 128U);
		return SigmaUpdateResult::boss;
	}
	if(frame == 100) {
		state.boss.patnum = sigma_patnum_rest;
		sigma.billiard_left = playfield_left;
		sigma.billiard_volley = 0U;
		state.bullet_special.turns_or_drift_frames = 1;
	}
	if((frame & 0x0F) != 0) return SigmaUpdateResult::boss;
	auto result = add_ball(state, {sigma.billiard_left, word_add(playfield_top, 8)},
		0x40U, enemy_bullet_special_motion::bounce_left_right_top_bottom,
		static_cast<std::int16_t>(122 + sigma.billiard_volley), 64);
	if(result != SigmaUpdateResult::boss) return result;
	result = add_ball(state, {word_subtract(word_add(playfield_right, 16), sigma.billiard_left),
		word_add(playfield_top, 8)}, 0x40U,
		enemy_bullet_special_motion::bounce_left_right_top_bottom,
		static_cast<std::int16_t>(122 + sigma.billiard_volley), 64);
	if(result != SigmaUpdateResult::boss) return result;
	sigma.billiard_left = word_add(sigma.billiard_left, 16);
	if(sigma.billiard_left == word_add(playfield_left, 192)) {
		state.lasers.wait_frames = 0x20U;
		(void)lasers_add(state.lasers, {word_add(sigma.topleft.x, 60), sigma.center_y}, 1, 0x6F, callbacks);
		state.lasers.wait_frames = 0x30U;
		(void)lasers_add(state.lasers, {word_add(sigma.topleft.x, 44), sigma.center_y}, 1, 0x6F, callbacks);
		(void)lasers_add(state.lasers, {word_add(sigma.topleft.x, 76), sigma.center_y}, 1, 0x6F, callbacks);
		state.lasers.wait_frames = 0x64U;
		(void)lasers_add(state.lasers, {word_add(sigma.topleft.x, 28), sigma.center_y}, 1, 0x6F, callbacks);
		(void)lasers_add(state.lasers, {word_add(sigma.topleft.x, 92), sigma.center_y}, 1, 0x6F, callbacks);
		state.lasers.wait_frames = 0x10U;
	}
	if(sigma.billiard_left >= word_add(playfield_right, 16)) {
		sigma.billiard_left = playfield_left;
		sigma.billiard_volley = static_cast<std::uint8_t>(sigma.billiard_volley + 1U);
		if(sigma.billiard_volley >= 2U) state.boss.phase_frame = 0;
	}
	return SigmaUpdateResult::boss;
}

[[nodiscard]] SigmaUpdateResult pattern_laser_weave(StageState &state,
	SigmaCallbacks &callbacks) noexcept
{
	if(move_weave(state)) return SigmaUpdateResult::boss;
	auto &sigma = state.sigma;
	const auto frame = state.boss.phase_frame;
	if(frame == 100) {
		sigma.laser_i = 0;
		return SigmaUpdateResult::boss;
	}
	constexpr std::array<std::int16_t, 5U> offsets{60, 44, 76, 28, 92};
	const auto spawn = [&]() noexcept {
		const auto index = static_cast<std::size_t>(sigma.laser_i);
		(void)lasers_add(state.lasers, {word_add(sigma.topleft.x, offsets[index]),
			sigma.center_y}, 1, 0x6F, callbacks);
	};
	if((frame == 114) || (frame == 242)) {
		spawn();
		sigma.laser_i = static_cast<std::int8_t>(sigma.laser_i + 1);
		return SigmaUpdateResult::boss;
	}
	if((frame == 118) || (frame == 246)) {
		spawn();
		sigma.laser_i = static_cast<std::int8_t>(sigma.laser_i + 1);
		spawn();
		sigma.laser_i = static_cast<std::int8_t>(sigma.laser_i + 1);
		return SigmaUpdateResult::boss;
	}
	if((frame == 122) || (frame == 250)) {
		spawn();
		sigma.laser_i = static_cast<std::int8_t>(sigma.laser_i + 1);
		spawn();
		sigma.laser_i = 0;
		return SigmaUpdateResult::boss;
	}
	if((frame > 120) && (frame < 240) && ((frame & 0x0F) == 0)) {
		std::uint8_t angle = 0x90U;
		std::int16_t i = 0;
		while(angle < 0xF0U) {
			const auto result = add_ball(state, {sigma.center_x, sigma.center_y}, angle,
				enemy_bullet_special_motion::bounce_left_right_top_bottom,
				static_cast<std::int16_t>(122 + (i % 2)), 60);
			if(result != SigmaUpdateResult::boss) return result;
			angle = static_cast<std::uint8_t>(angle + 0x10U);
			i = word_add(i, 1);
		}
	}
	return SigmaUpdateResult::boss;
}

[[nodiscard]] SigmaUpdateResult pattern_stream_blasts(StageState &state,
	SigmaCallbacks &callbacks) noexcept
{
	auto &sigma = state.sigma;
	const auto frame = state.boss.phase_frame;
	if(frame < 50) return SigmaUpdateResult::boss;
	if(frame == 50) callbacks.sound_effect(9U);
	if(frame < 100) {
		state.boss.patnum = static_cast<std::int16_t>(state.frame.page_back * 4U + 128U);
		return SigmaUpdateResult::boss;
	}
	if(frame == 100) {
		const auto velocity = vector_between({sigma.center_x, sigma.center_y},
			{player_center_x(state), player_sigma_y(state)}, 48);
		if(!velocity) return SigmaUpdateResult::source_iatan2_fault;
		sigma.stream_velocity = *velocity;
		sigma.stream_mirror_velocity_x = word_subtract(0, sigma.stream_velocity.x);
		sigma.stream_x = sigma.center_x;
		sigma.stream_mirror_x = sigma.center_x;
		sigma.blast_hitbox_margin = -4;
		sigma.cel_interval_mask = 3U;
		sigma.stream_y = sigma.center_y;
		state.boss.patnum = sigma_patnum_rest;
	}
	if((frame & 7) == 0) {
		const auto first_rejected = sigma_blasts_add(state, callbacks,
			sigma.stream_x, sigma.stream_y, 48);
		if(sigma_blasts_add(state, callbacks, sigma.stream_mirror_x,
			sigma.stream_y, 48) && first_rejected) {
			state.boss.phase_frame = 0;
		}
		sigma.stream_x = word_add(sigma.stream_x, sigma.stream_velocity.x);
		sigma.stream_mirror_x = word_add(sigma.stream_mirror_x,
			sigma.stream_mirror_velocity_x);
		sigma.stream_y = word_add(sigma.stream_y, sigma.stream_velocity.y);
		if(player_center_x(state) > sigma.stream_x) {
			sigma.stream_x = word_add(sigma.stream_x, 16);
		} else if(player_center_x(state) < sigma.stream_x) {
			sigma.stream_x = word_subtract(sigma.stream_x, 16);
		}
		if(player_center_x(state) > sigma.stream_mirror_x) {
			sigma.stream_mirror_x = word_subtract(sigma.stream_mirror_x, 16);
		} else if(player_center_x(state) < sigma.stream_mirror_x) {
			sigma.stream_mirror_x = word_add(sigma.stream_mirror_x, 16);
		}
		return add_pellet(state, {sigma.center_x, sigma.center_y},
			randring_next8(state.random), enemy_bullet_group::ring_16, 60);
	}
	return SigmaUpdateResult::boss;
}

[[nodiscard]] SigmaUpdateResult pattern_weave_player_blast(StageState &state,
	SigmaCallbacks &callbacks) noexcept
{
	if(move_weave(state)) return SigmaUpdateResult::boss;
	if(state.boss.phase_frame == 50) state.sigma.cel_interval_mask = 7U;
	if((state.boss.phase_frame & 0x3F) == 32) {
		(void)sigma_blasts_add(state, callbacks, player_center_x(state),
			player_sigma_y(state), 48);
	}
	if((state.boss.phase_frame & 0x3F) == 0) {
		return add_pellet(state, {state.sigma.center_x, state.sigma.center_y},
			randring_next8(state.random), enemy_bullet_group::ring_32, 60);
	}
	return SigmaUpdateResult::boss;
}

[[nodiscard]] SigmaUpdateResult pattern_final_spread(StageState &state,
	SigmaCallbacks &callbacks) noexcept
{
	if(move_sweep(state)) return SigmaUpdateResult::boss;
	if(state.boss.phase_frame == 50) state.sigma.cel_interval_mask = 7U;
	if((state.boss.phase_frame & 0x3F) == 0) {
		(void)sigma_blasts_add(state, callbacks, player_center_x(state),
			player_sigma_y(state), 48);
	}
	if((state.boss.phase_frame & 7) == 0) {
		return add_pellet(state, {state.sigma.center_x, state.sigma.center_y}, 0U,
			enemy_bullet_group::spread_2_ultrawide_aimed, 70);
	}
	return SigmaUpdateResult::boss;
}

[[nodiscard]] SigmaUpdateResult pattern_final_ball_ring(StageState &state) noexcept
{
	if(move_sweep(state)) return SigmaUpdateResult::boss;
	if(state.boss.phase_frame == 50) state.sigma.ball_ring_angle = 0U;
	if((state.boss.phase_frame & 7) == 0) {
		const auto result = add_ball(state, {state.sigma.center_x, state.sigma.center_y},
			state.sigma.ball_ring_angle, enemy_bullet_group::ring_8, 86, 80);
		if(result != SigmaUpdateResult::boss) return result;
		state.sigma.ball_ring_angle = static_cast<std::uint8_t>(
			state.sigma.ball_ring_angle + 8U);
	}
	return SigmaUpdateResult::boss;
}

[[nodiscard]] SigmaUpdateResult pattern_final_blast_ring(StageState &state,
	SigmaCallbacks &callbacks) noexcept
{
	if(move_sweep(state)) return SigmaUpdateResult::boss;
	if(state.boss.phase_frame == 50) state.sigma.cel_interval_mask = 7U;
	if((state.boss.phase_frame & 0x1F) == 0) {
		(void)sigma_blasts_add(state, callbacks, player_center_x(state),
			player_sigma_y(state), 48);
	}
	if((state.boss.phase_frame & 0x1F) == 16) {
		return add_pellet(state, {state.sigma.center_x, state.sigma.center_y},
			randring_next8(state.random), enemy_bullet_group::ring_16, 60);
	}
	return SigmaUpdateResult::boss;
}

[[nodiscard]] SigmaUpdateResult run_pattern(StageState &state,
	SigmaCallbacks &callbacks, std::uint8_t count) noexcept
{
	SigmaUpdateResult result = SigmaUpdateResult::boss;
	if(state.sigma.pattern == 0U) {
		switch(state.sigma.phase) {
		case 1U: result = pattern_dash_spray(state, callbacks); break;
		case 3U: result = pattern_orbit_blasts(state, callbacks); break;
		case 5U: result = pattern_billiards(state, callbacks); break;
		case 7U: result = pattern_stream_blasts(state, callbacks); break;
		case 9U: result = pattern_final_spread(state, callbacks); break;
		default: break;
		}
	} else if(state.sigma.pattern == 1U) {
		switch(state.sigma.phase) {
		case 1U: result = pattern_traps(state, callbacks); break;
		case 3U: result = pattern_weave_ring16(state); break;
		case 5U: result = pattern_laser_weave(state, callbacks); break;
		case 7U: result = pattern_weave_player_blast(state, callbacks); break;
		case 9U: result = pattern_final_ball_ring(state); break;
		default: break;
		}
	} else if(state.sigma.pattern == 2U) {
		if(state.sigma.phase == 1U) result = pattern_weave_ring32(state);
		else if(state.sigma.phase == 9U) result = pattern_final_blast_ring(state, callbacks);
	}
	if(result != SigmaUpdateResult::boss) return result;
	if(state.boss.phase_frame == 0) {
		state.sigma.pattern = static_cast<std::uint8_t>(state.sigma.pattern + 1U);
		if(state.sigma.pattern >= count) {
			state.sigma.pattern = 0U;
			state.sigma.pattern_looped_unused = 1U;
		}
		if(state.boss.damage >= state.sigma.phase_damage_max) {
			state.boss.damage = 0;
			state.sigma.phase = static_cast<std::uint8_t>(state.sigma.phase + 1U);
			state.sigma.pattern = 0U;
			state.sigma.pattern_looped_unused = 0U;
		}
	}
	return SigmaUpdateResult::boss;
}

} // namespace

void sigma_combat_reset(StageState &state) noexcept
{
	state.tile_mode = 2U;
	state.boss.topleft_on_page[0U] = {160, 48};
	state.boss.topleft_on_page[1U] = {160, 48};
	state.sigma.topleft = {160, 48};
	state.boss.damage = 0;
	state.boss.phase_frame = 0;
	state.boss.patnum = sigma_patnum_rest;
	state.sigma.phase = 0U;
	state.sigma.cel_interval_mask = 7U;
}

SigmaUpdateResult sigma_bg_render(StageState &state,
	SigmaCallbacks &callbacks) noexcept
{
	if(!pages_valid(state)) return SigmaUpdateResult::invalid_page;
	state.boss.topleft_on_page[state.frame.page_back] =
		state.boss.topleft_on_page[state.frame.page_front];
	callbacks.egc_off();
	callbacks.grcg_set_color(0U);
	callbacks.clear_boss_playfield();
	callbacks.grcg_off();
	sigma_blasts_activate(state.sigma);
	return SigmaUpdateResult::boss;
}

SigmaUpdateResult sigma_update(StageState &state, SigmaCallbacks &callbacks) noexcept
{
	state.boss.phase_frame = word_add(state.boss.phase_frame, 1);
	if(!pages_valid(state)) return SigmaUpdateResult::invalid_page;
	auto &sigma = state.sigma;
	sigma.topleft = boss_back(state);
	sigma.center_x = word_add(sigma.topleft.x, sigma_center_offset);
	sigma.center_y = word_add(sigma.topleft.y, sigma_center_offset);
	state.boss.target = {sigma.center_x, sigma.center_y};
	if((state.stage_frame & 1U) == 0U) {
		sigma.ring_radius = static_cast<std::uint8_t>(sigma.ring_radius + 8U);
		callbacks.grcg_set_color(4U);
		std::uint8_t radius = sigma.ring_radius;
		const auto angle_step = state.reduce_effects ? std::uint8_t{16U} : std::uint8_t{2U};
		for(std::size_t i = 0U; i < 2U; ++i) {
			callbacks.dot_square_ring({224, 200}, radius, angle_step);
			radius = static_cast<std::uint8_t>(radius + 128U);
		}
		callbacks.grcg_off();
	}
	if(state.boss.phase != 0U) {
		if(sigma_defeat(state, callbacks)) return SigmaUpdateResult::stage_clear;
	} else {
		SigmaUpdateResult result = SigmaUpdateResult::boss;
		if(sigma.phase == 0U) {
			if(state.boss.phase_frame > 50) {
				sigma.pattern = 0U;
				state.boss.phase_frame = 0;
				sigma.phase = 1U;
				sigma.pattern_looped_unused = 0U;
				sigma.phase_damage_max = 1800;
			}
		} else if(sigma.phase == 1U) {
			result = run_pattern(state, callbacks, 3U);
		} else if(sigma.phase == 2U) {
			sigma.pattern = 0U;
			state.boss.phase_frame = 0;
			sigma.phase = 3U;
			sigma.pattern_looped_unused = 0U;
			sigma.phase_damage_max = 1800;
		} else if(sigma.phase == 3U) {
			result = run_pattern(state, callbacks, 2U);
		} else if(sigma.phase == 4U) {
			sigma.pattern = 0U;
			state.boss.phase_frame = 0;
			sigma.phase = 5U;
			sigma.pattern_looped_unused = 0U;
			sigma.phase_damage_max = 1800;
		} else if(sigma.phase == 5U) {
			result = run_pattern(state, callbacks, 2U);
		} else if(sigma.phase == 6U) {
			sigma.pattern = 0U;
			state.boss.phase_frame = 0;
			sigma.phase = 7U;
			sigma.pattern_looped_unused = 0U;
			sigma.phase_damage_max = 1800;
		} else if(sigma.phase == 7U) {
			result = run_pattern(state, callbacks, 2U);
		} else if(sigma.phase == 8U) {
			sigma.pattern = 0U;
			state.boss.phase_frame = 0;
			sigma.phase = 9U;
			sigma.pattern_looped_unused = 0U;
			sigma.phase_damage_max = 5000;
		} else if(sigma.phase == 9U) {
			result = run_pattern(state, callbacks, 3U);
			if(result != SigmaUpdateResult::boss) return result;
			if(state.boss.damage >= 1300) {
				state.boss.phase = 1U;
				state.score.score_delta = dword_add(state.score.score_delta, 300000);
				state.boss.phase_frame = 0;
			}
		}
		if(result != SigmaUpdateResult::boss) return result;
		sigma.topleft = boss_back(state);
		result = sigma_hittest_and_put(state, callbacks);
		if(result != SigmaUpdateResult::boss) return result;
		sigma.frame = static_cast<std::uint32_t>(sigma.frame + 1U);
	}
	sigma_blasts_update_and_render(state, callbacks);
	return SigmaUpdateResult::boss;
}

} // namespace th02::core
