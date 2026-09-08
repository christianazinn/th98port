#include "th02/core/player.hpp"

#include "th02/core/item.hpp"
#include "th02/core/random.hpp"
#include "th02/core/score.hpp"
#include "th02/core/shot.hpp"
#include "th02/core/spark.hpp"

#include <array>
#include <bit>

namespace th02::core {
namespace {
std::int16_t add_word(std::int16_t value, std::int16_t delta) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(value) + delta));
}

constexpr std::int16_t player_width = 32;
constexpr std::int16_t player_height = 48;
constexpr std::int16_t option_width = 16;
constexpr std::int16_t option_height = 16;
constexpr std::int16_t option_to_option_distance = 48;
constexpr std::int16_t player_left_to_option_left = 16;
constexpr std::int16_t player_to_option_top = 16;

void negate_hits_if_invincible(PlayerHitState &state) noexcept
{
	if(state.invincible_via_bomb ||
		((state.invincibility_time != 0) && !state.miss_active)) {
		state.raw_hit = 0;
	}
}

std::int8_t add_byte(std::int8_t value, int delta) noexcept
{
	return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
		static_cast<int>(value) + delta));
}

constexpr std::uint16_t input_down = 0x0002;
constexpr std::uint16_t input_shot = 0x0010;
constexpr std::uint16_t input_bomb = 0x0020;
constexpr std::uint16_t input_movement_aligned = 0x000F;
constexpr std::uint16_t input_down_left = 0x4000;
constexpr std::uint16_t input_down_right = 0x8000;
constexpr std::uint16_t input_movement_diagonal = 0xF000;
constexpr std::uint8_t shot_stream_inactive = 0xFF;
constexpr std::uint8_t shot_level_max = 9;
constexpr std::uint8_t shot_cooldown = 8;
constexpr std::uint8_t shot_patnum_a_unpowered = 0x34;
constexpr std::uint8_t shot_patnum_b_unpowered = 0x3F;
constexpr std::uint8_t player_hit_gameover = 0xFFU;
constexpr std::uint8_t miss_invincibility_frames = 220U;
constexpr std::int16_t playperf_min = -6;
constexpr std::int16_t player_left_start = 208;
constexpr std::int16_t player_top_start = 366;
constexpr std::int16_t player_miss_patnum = 4;
constexpr std::int16_t player_still_patnum = 0;
constexpr std::int16_t player_center_x = player_width / 2;
constexpr std::int16_t player_center_y = player_height / 2;
constexpr std::int16_t miss_spark_speed = 128;

constexpr std::array<std::uint8_t, shot_level_max + 1U> power_reset_for{
	1U, 1U, 4U, 8U, 16U, 24U, 32U, 40U, 52U, 64U,
};
constexpr std::array<std::uint8_t, 21U> power_to_shot_level{
	0U, 1U, 2U, 2U, 3U, 3U, 4U, 4U, 5U, 5U, 6U,
	6U, 6U, 7U, 7U, 7U, 8U, 8U, 8U, 8U, 9U,
};

[[nodiscard]] bool page_is_valid(const PlayerFrameState &frame) noexcept
{
	return frame.page_back < frame.player.on_page.size();
}

[[nodiscard]] std::int8_t add_signed_byte(std::int8_t value, int delta) noexcept
{
	return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
		static_cast<int>(value) + delta));
}

void shot_patnums_set(PlayerShotStreamState &state) noexcept
{
	if(state.shot_level == shot_level_max) {
		state.shot_patnum = state.shot_patnum_powered;
		state.shot_option_patnum = state.shot_option_patnum_powered;
	} else {
		state.shot_patnum = shot_patnum_a_unpowered;
		state.shot_option_patnum = shot_patnum_b_unpowered;
	}
}
}

bool player_move(PlayerPositionState &state, std::size_t back_page,
	std::int16_t delta_x, std::int16_t delta_y) noexcept
{
	if(back_page >= state.on_page.size()) return false;
	auto &page = state.on_page[back_page];
	state.current.y = add_word(page.y, delta_y);
	if(state.current.y < 16) {
		state.current.y = 16;
	} else if(state.current.y > 344) {
		state.current.y = 344;
	}
	page.y = state.current.y;

	state.current.x = add_word(page.x, delta_x);
	if(state.current.x >= 390) {
		state.current.x = 390;
	} else if(state.current.x <= 28) {
		state.current.x = 28;
	}
	page.x = state.current.x;
	return true;
}

PlayerFrameInvalidationResult player_frame_invalidate(
	PlayerFrameState &frame, PlayerShotPoolState &shots,
	PlayerShotLifetimeState &lifetime, bool reduce_effects,
	PlayerFrameInvalidationCallbacks &callbacks
) noexcept
{
	if(frame.page_front >= frame.player.on_page.size() ||
		frame.page_back >= frame.player.on_page.size()) {
		return PlayerFrameInvalidationResult::invalid_page;
	}
	if(!shots_invalidate_lifetime(shots, lifetime, frame.page_back,
		frame.page_front, reduce_effects, callbacks)) {
		return PlayerFrameInvalidationResult::invalid_page;
	}

	const auto player_old = frame.player.on_page[frame.page_back];
	const auto option_old = frame.option_left_on_page[frame.page_back];
	callbacks.invalidate_player({player_old, player_width, player_height});
	callbacks.invalidate_option({option_old, option_width, option_height});
	callbacks.invalidate_option({
		{add_word(option_old.x, option_to_option_distance), option_old.y},
		option_width, option_height,
	});

	frame.player.on_page[frame.page_back] = frame.player.on_page[frame.page_front];
	return PlayerFrameInvalidationResult::completed;
}

std::optional<MovementIntent> movement_intent(
	std::uint16_t input, MovementSpeeds speeds, std::size_t back_page) noexcept
{
	if(back_page > 1) return std::nullopt;
	MovementIntent result{};
	// Signed byte speeds promote to signed words before negation in the DOS
	// source. The wider intermediates retain -(-128) == 128.
	std::int16_t dx = 0, dy = 0;
	auto &pose = result.pose;
	switch(input & 0xF000U) {
	case 0x1000: dy = -speeds.diagonal_y; dx = -speeds.diagonal_x; pose = PlayerPose::left; break;
	case 0x2000: dy = -speeds.diagonal_y; dx = speeds.diagonal_x; pose = PlayerPose::right; break;
	case 0x4000: dy = speeds.diagonal_y; dx = -speeds.diagonal_x; pose = PlayerPose::left; break;
	case 0x8000: dy = speeds.diagonal_y; dx = speeds.diagonal_x; pose = PlayerPose::right; break;
	default:
		switch(input & 0x000FU) {
		case 1: dy = -speeds.aligned_y; dx = 0; pose = PlayerPose::still; break;
		case 2: dy = speeds.aligned_y; dx = 0; pose = PlayerPose::still; break;
		case 4: dx = -speeds.aligned_x; dy = 0; pose = PlayerPose::left; break;
		case 8: dx = speeds.aligned_x; dy = 0; pose = PlayerPose::right; break;
		case 5: dy = -speeds.diagonal_y; dx = -speeds.diagonal_x; pose = PlayerPose::left; break;
		case 9: dy = -speeds.diagonal_y; dx = speeds.diagonal_x; pose = PlayerPose::right; break;
		case 6: dy = speeds.diagonal_y; dx = -speeds.diagonal_x; pose = PlayerPose::left; break;
		case 10: dy = speeds.diagonal_y; dx = speeds.diagonal_x; pose = PlayerPose::right; break;
		default: dx = 0; dy = 0; pose = PlayerPose::still; break;
		}
	}
	if(speeds.aligned_x == 5) {
		const auto page = static_cast<std::int16_t>(back_page);
		if(dx < 0) dx += page;
		else if(dx > 0) dx -= page;
		if(dy < 0) dy += page;
		else if(dy > 0) dy -= page;
	}
	result.delta = {dx, dy};
	return result;
}

bool player_move_and_shoot_allowed(PlayerHitState &state) noexcept
{
	negate_hits_if_invincible(state);
	return state.raw_hit == 0;
}

void prepare_player_callbacks(PlayerHitState &state) noexcept
{
	negate_hits_if_invincible(state);
	if(state.invincibility_time > 0) {
		--state.invincibility_time;
	}
}

bool hud_shot_level_from_power(std::uint8_t power,
	std::uint8_t &shot_level
) noexcept
{
	const auto table_index = static_cast<std::size_t>(power >> 2U);
	if(table_index >= power_to_shot_level.size()) return false;
	shot_level = power_to_shot_level[table_index];
	return true;
}

PlayerMissResult player_miss_state_step(
	PlayerMissState &miss, PlayerFrameState &frame, PlayerHitState &hit,
	PlayerShotStreamState &stream, ScoreState &score, ItemPoolState &items,
	SparkPoolState &sparks, RandomState &random,
	PlayerMissPresentationCallbacks &callbacks
) noexcept
{
	auto add_sparks = [&](std::int16_t count) noexcept {
		const auto &back = frame.player.on_page[frame.page_back];
		return sparks_add(sparks, frame.page_back,
			add_word(back.x, player_center_x), std::bit_cast<std::uint16_t>(
				add_word(back.y, player_center_y)
			), miss_spark_speed, count, true);
	};

	if(miss.miss_frame == 0U) {
		callbacks.sound_effect(2U);
		hit.miss_active = true;
		++miss.stage_miss_count;
		score_delta_commit(score);
		++miss.total_miss_count;
		if(!page_is_valid(frame)) return PlayerMissResult::invalid_page;
		if(!add_sparks(16)) return PlayerMissResult::spark_allocation_failed;
	}

	if(!page_is_valid(frame)) return PlayerMissResult::invalid_page;
	auto &back = frame.player.on_page[frame.page_back];
	frame.player.current = back;
	++miss.miss_frame;
	if(miss.miss_frame < 24U) {
		callbacks.render_player({
			back,
			static_cast<std::int16_t>(player_miss_patnum +
				(static_cast<std::int16_t>(miss.miss_frame) >> 2)),
		});
	} else if(miss.miss_frame == 24U) {
		hit.invincibility_time = miss_invincibility_frames;
		if(miss.playperf > 2) {
			miss.playperf = 0;
		} else {
			miss.playperf = add_word(miss.playperf, -2);
			if(miss.playperf < playperf_min) {
				miss.playperf = playperf_min;
			}
		}
		if(!add_sparks(32)) return PlayerMissResult::spark_allocation_failed;
		if(score.lives == 0) {
			miss.miss_frame = 0U;
			items.miss_add_gameover = true;
			items_miss_add(items, random, back.x, back.y, frame.player.current.x);
			items.miss_add_gameover = false;
			hit.raw_hit = player_hit_gameover;
		} else {
			if(stream.shot_level >= power_reset_for.size()) {
				return PlayerMissResult::unsupported_shot_level;
			}
			miss.power = power_reset_for[stream.shot_level];
			if(!hud_shot_level_from_power(miss.power, stream.shot_level)) {
				return PlayerMissResult::unsupported_shot_level;
			}
			callbacks.hud_power(miss.power, stream.shot_level);
			items_miss_add(items, random, back.x, back.y, frame.player.current.x);
		}
	} else if(miss.miss_frame < 43U) {
		back.y = add_word(back.y, 4);
	} else if(miss.miss_frame == 43U) {
		score.lives = add_signed_byte(score.lives, -1);
		callbacks.hud_lives(score.lives);
		score.bombs = std::bit_cast<std::int8_t>(miss.start_bombs);
		if(score.lives == 0) {
			score.bombs = add_signed_byte(score.bombs, 2);
		}
		callbacks.hud_bombs(score.bombs);
		if(!add_sparks(16)) return PlayerMissResult::spark_allocation_failed;
		frame.player.on_page[0] = {player_left_start, player_top_start};
		frame.player.on_page[1] = {player_left_start, player_top_start};
	} else if(miss.miss_frame < 68U) {
		back.y = add_word(back.y, -2);
		callbacks.render_player({back, player_still_patnum});
	} else {
		miss.miss_frame = 0U;
		hit.miss_active = false;
		hit.raw_hit = 0U;
	}
	return PlayerMissResult::completed;
}

PlayerUpdateResult player_update_state_step(
	PlayerHitState &player, bool &quit, PlayerShotPoolState &shots,
	PlayerShotLifetimeState &lifetime, std::size_t back_page,
	std::uint32_t stage_frame, PlayerUpdateCallbacks &callbacks
) noexcept
{
	prepare_player_callbacks(player);
	const auto shot_result = shots_update_lifetime(
		shots, lifetime, back_page, stage_frame
	);
	if(shot_result == PlayerShotLifetimeUpdateResult::invalid_page) {
		return PlayerUpdateResult::shot_lifetime_invalid_page;
	}
	if(shot_result == PlayerShotLifetimeUpdateResult::source_divide_fault) {
		return PlayerUpdateResult::shot_lifetime_source_divide_fault;
	}

	if(player.raw_hit != 0U) {
		const auto miss_result = callbacks.player_miss_update(player);
		switch(miss_result) {
		case PlayerMissResult::completed:
			break;
		case PlayerMissResult::invalid_page:
			return PlayerUpdateResult::miss_invalid_page;
		case PlayerMissResult::spark_allocation_failed:
			return PlayerUpdateResult::miss_spark_allocation_failed;
		case PlayerMissResult::unsupported_shot_level:
			return PlayerUpdateResult::miss_unsupported_shot_level;
		}
		if(player.raw_hit == player_hit_gameover) {
			quit = true;
			player.raw_hit = 0U;
		}
		return PlayerUpdateResult::miss_handled;
	}
	return PlayerUpdateResult::completed;
}

PlayerMissUpdateCallbacks::PlayerMissUpdateCallbacks(
	PlayerMissState &miss_arg, PlayerFrameState &frame_arg,
	PlayerShotStreamState &stream_arg, ScoreState &score_arg,
	ItemPoolState &items_arg, SparkPoolState &sparks_arg,
	RandomState &random_arg, PlayerMissPresentationCallbacks &presentation_arg
) noexcept
	: miss(miss_arg), frame(frame_arg), stream(stream_arg), score(score_arg),
	  items(items_arg), sparks(sparks_arg), random(random_arg),
	  presentation(presentation_arg)
{
}

PlayerMissResult PlayerMissUpdateCallbacks::player_miss_update(
	PlayerHitState &state
) noexcept
{
	return player_miss_state_step(miss, frame, state, stream, score, items,
		sparks, random, presentation);
}

PlayerShotVolleyDispatcher::PlayerShotVolleyDispatcher(PlayerShotPoolState &pool_arg,
	PlayerShotLifetimeState &lifetime_arg, RandomState &random_arg,
	PlayerShotVolleyContext &context_arg, PlayerShotStreamEffects &effects_arg
) noexcept
	: pool(pool_arg), lifetime(lifetime_arg), random(random_arg),
	  context(context_arg), effects(effects_arg)
{
}

PlayerShotVolleyContext &PlayerShotVolleyDispatcher::volley_context() noexcept
{
	return context;
}

PlayerShotStreamResult PlayerShotVolleyDispatcher::shot_volley(
	const PlayerShotStreamState &state, std::uint8_t &shot_patnum,
	std::uint8_t &shot_option_patnum
) noexcept
{
	switch(context.type) {
	case PlayerShotType::a:
		return shot_a_volley(pool, context.positions, context.back_page, random,
			state.shot_level, state.spread_angle_delta, shot_patnum,
			lifetime.option_shots_alive) ? PlayerShotStreamResult::completed :
				PlayerShotStreamResult::volley_invalid_page;
	case PlayerShotType::b: {
		const auto result = shot_b_volley(pool, context.positions,
			context.back_page, random, state.shot_level, context.boss_position,
			context.boss_pos_x_unused, shot_patnum, shot_option_patnum,
			lifetime.option_shots_alive);
		if(result == ShotBVolleyResult::invalid_page) {
			return PlayerShotStreamResult::volley_invalid_page;
		}
		if(result == ShotBVolleyResult::source_iatan2_fault) {
			return PlayerShotStreamResult::volley_source_iatan2_fault;
		}
		return PlayerShotStreamResult::completed;
	}
	case PlayerShotType::c:
		return shot_c_volley(pool, context.positions, context.back_page, random,
			state.shot_level, context.shot_c_cycle, shot_patnum,
			shot_option_patnum, lifetime.option_shots_alive) ?
				PlayerShotStreamResult::completed :
				PlayerShotStreamResult::volley_invalid_page;
	case PlayerShotType::invalid:
		return PlayerShotStreamResult::invalid_shot_selection;
	}
	return PlayerShotStreamResult::invalid_shot_selection;
}

void PlayerShotVolleyDispatcher::shot_sound(
	const PlayerShotStreamState &state
) noexcept
{
	effects.shot_sound(state);
}

void PlayerShotVolleyDispatcher::bomb_request(
	const PlayerShotStreamState &state
) noexcept
{
	effects.bomb_request(state);
}

PlayerShotStreamResult player_move_and_shoot_shot_stream_step(
	PlayerShotStreamState &state, std::uint16_t input,
	PlayerShotStreamCallbacks &callbacks
) noexcept
{
	if((input & input_shot) != 0) {
		if(state.stream_a_phase == shot_stream_inactive) {
			state.stream_a_phase = 0;
			state.stream_a_cooldown = 0;
		} else if(state.stream_b_phase == shot_stream_inactive) {
			state.stream_b_phase = 0;
			state.stream_b_cooldown = 0;
		}
	} else {
		if(state.stream_a_phase >= 3) {
			state.stream_a_phase = shot_stream_inactive;
			state.stream_b_phase = 4;
		} else if(state.stream_b_phase >= 2) {
			state.stream_b_phase = shot_stream_inactive;
		}
	}

	if(state.stream_a_phase != shot_stream_inactive) {
		if(state.stream_a_cooldown == 0) {
			shot_patnums_set(state);
			const auto volley_result = callbacks.shot_volley(
				state, state.shot_patnum, state.shot_option_patnum
			);
			if(volley_result != PlayerShotStreamResult::completed) {
				return volley_result;
			}
			callbacks.shot_sound(state);
			state.stream_a_cooldown = shot_cooldown;
			++state.stream_a_phase;
			if((input & input_bomb) != 0) {
				callbacks.bomb_request(state);
			}
			return PlayerShotStreamResult::completed;
		}
		--state.stream_a_cooldown;
	}
	if(state.stream_b_phase < 2) {
		if(state.stream_b_cooldown == 0) {
			shot_patnums_set(state);
			if(state.shot_level == shot_level_max) {
				if((input & (input_down | input_down_left | input_down_right)) != 0) {
					state.spread_angle_delta = add_byte(state.spread_angle_delta, 5);
					if(state.spread_angle_delta > 26) {
						state.spread_angle_delta = 26;
					}
				} else if((input & (input_movement_aligned | input_movement_diagonal)) == 0) {
					state.spread_angle_delta = add_byte(state.spread_angle_delta, -5);
					if(state.spread_angle_delta < 0) {
						state.spread_angle_delta = 0;
					}
				}
			}
			const auto volley_result = callbacks.shot_volley(
				state, state.shot_patnum, state.shot_option_patnum
			);
			if(volley_result != PlayerShotStreamResult::completed) {
				return volley_result;
			}
			state.stream_b_cooldown = shot_cooldown;
			++state.stream_b_phase;
		} else {
			callbacks.shot_sound(state);
			--state.stream_b_cooldown;
		}
	}
	if((input & input_bomb) != 0) {
		callbacks.bomb_request(state);
	}
	return PlayerShotStreamResult::completed;
}

PlayerFrameResult player_frame_move_and_shoot(
	PlayerFrameState &frame, PlayerHitState &hit, MovementSpeeds speeds,
	std::uint16_t input, PlayerShotStreamState &stream,
	PlayerShotVolleyDispatcher &dispatcher
) noexcept
{
	if(!player_move_and_shoot_allowed(hit)) {
		return PlayerFrameResult::hit_blocked;
	}
	const auto intent = movement_intent(input, speeds, frame.page_back);
	if(!intent.has_value()) {
		return PlayerFrameResult::invalid_page;
	}
	frame.pose = intent->pose;
	if(!player_move(frame.player, frame.page_back,
		intent->delta.x, intent->delta.y)) {
		return PlayerFrameResult::invalid_page;
	}
	frame.option_left_on_page[frame.page_back] = {
		add_word(frame.player.current.x,
			static_cast<std::int16_t>(-player_left_to_option_left - intent->delta.x)),
		add_word(frame.player.current.y,
			static_cast<std::int16_t>(player_to_option_top - intent->delta.y)),
	};

	auto &context = dispatcher.volley_context();
	context.positions = PlayerShotWriterPositions{
		{frame.player.current.x, frame.player.current.y},
		{frame.option_left_on_page[frame.page_back].x,
			frame.option_left_on_page[frame.page_back].y},
	};
	context.back_page = frame.page_back;
	switch(player_move_and_shoot_shot_stream_step(stream, input, dispatcher)) {
	case PlayerShotStreamResult::completed:
		return PlayerFrameResult::completed;
	case PlayerShotStreamResult::invalid_shot_selection:
		return PlayerFrameResult::invalid_shot_selection;
	case PlayerShotStreamResult::volley_invalid_page:
		return PlayerFrameResult::volley_invalid_page;
	case PlayerShotStreamResult::volley_source_iatan2_fault:
		return PlayerFrameResult::volley_source_iatan2_fault;
	}
	return PlayerFrameResult::invalid_shot_selection;
}

} // namespace th02::core
