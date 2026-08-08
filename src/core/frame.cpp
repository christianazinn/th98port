#include "game_internal.hpp"
#include "trig.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace th03::detail {
namespace {

constexpr std::uint16_t input_up = 0x0001U;
constexpr std::uint16_t input_down = 0x0002U;
constexpr std::uint16_t input_left = 0x0004U;
constexpr std::uint16_t input_right = 0x0008U;
constexpr std::uint16_t input_bomb = 0x0010U;
constexpr std::uint16_t input_shot = 0x0020U;
constexpr std::uint16_t input_up_left = 0x0100U;
constexpr std::uint16_t input_down_left = 0x0200U;
constexpr std::uint16_t input_up_right = 0x0400U;
constexpr std::uint16_t input_down_right = 0x0800U;
constexpr std::uint16_t input_movement =
	input_up | input_down | input_left | input_right |
	input_up_left | input_down_left | input_up_right | input_down_right;
constexpr std::int16_t player_x_min = 8 << 4;
constexpr std::int16_t player_x_max = (288 - 8) << 4;
constexpr std::int16_t player_y_min = 24 << 4;
constexpr std::int16_t player_y_max = (368 - 16) << 4;
constexpr std::uint16_t gauge_64 = 64U << 4U;
constexpr std::uint16_t gauge_128 = 128U << 4U;
constexpr std::uint16_t gauge_192 = 192U << 4U;
constexpr std::uint16_t gauge_max = 256U << 4U;
constexpr std::uint8_t position_clip_x = 0x01U;
constexpr std::uint8_t position_clip_bottom = 0x02U;
constexpr std::uint8_t position_do_not_mirror_x = 0x80U;
constexpr std::uint8_t op_stop = 0x00U;
constexpr std::uint8_t op_move_linear = 0x01U;
constexpr std::uint8_t op_move_circular = 0x02U;
constexpr std::uint8_t op_wait = 0x03U;
constexpr std::uint8_t op_move_sine_x = 0x04U;
constexpr std::uint8_t op_move_sine_y = 0x05U;
constexpr std::uint8_t op_move = 0x06U;
constexpr std::uint8_t op_move_with_speed = 0x07U;
constexpr std::uint8_t op_move_linear_stop_at_player_y = 0x08U;
constexpr std::uint8_t op_move_linear_stop_at_player_x = 0x09U;
constexpr std::uint8_t op_spawn = 0x10U;
constexpr std::uint8_t op_move_circular_plus = 0x0AU;
constexpr std::uint8_t op_clip_x = 0x82U;
constexpr std::uint8_t op_clip_bottom = 0x83U;
constexpr std::int16_t shot_velocity_y = -(12 << 4);
constexpr std::int16_t shotpair_width = 32 << 4;
constexpr std::int16_t shotpair_height = 16 << 4;
constexpr std::int16_t shotpair_half_width = shotpair_width / 2;
constexpr std::int16_t shotpair_half_height = shotpair_height / 2;
constexpr std::array<std::array<std::uint16_t, 9>, 6> cpu_collision_routes{{
	{{0x0400U, 0x0008U, 0x0800U, 0x0001U, 0x0008U, 0x0008U, 0x0100U, 0x0400U, 0x0400U}},
	{{0x0004U, 0x0004U, 0x0100U, 0x0002U, 0x0002U, 0x0002U, 0x0100U, 0x0001U, 0x0400U}},
	{{0x0004U, 0x0004U, 0x0100U, 0x0004U, 0x0004U, 0x0100U, 0x0100U, 0x0004U, 0x0100U}},
	{{0x0200U, 0x0200U, 0x0008U, 0x0002U, 0x0800U, 0x0800U, 0x0004U, 0x0008U, 0x0008U}},
	{{0x0200U, 0x0200U, 0x0800U, 0x0001U, 0x0200U, 0x0001U, 0x0001U, 0x0004U, 0x0008U}},
	{{0x0004U, 0x0200U, 0x0002U, 0x0200U, 0x0100U, 0x0400U, 0x0100U, 0x0004U, 0x0001U}},
}};
constexpr std::array<std::array<std::uint16_t, 9>, 3> cpu_retry_routes{{
	{{0x0008U, 0x0800U, 0x0002U, 0x0000U, 0x0400U, 0x0200U, 0x0100U, 0x0004U, 0x0001U}},
	{{0x0004U, 0x0008U, 0x0002U, 0x0200U, 0x0800U, 0x0000U, 0x0100U, 0x0400U, 0x0001U}},
	{{0x0004U, 0x0200U, 0x0002U, 0x0000U, 0x0100U, 0x0800U, 0x0400U, 0x0008U, 0x0001U}},
}};

enum class MoveResult {
	invalid,
	valid,
	no_input,
};

struct Velocity {
	std::int16_t x = 0;
	std::int16_t y = 0;
};

void score_add(
	std::array<std::uint8_t, score_digits> &score,
	std::uint16_t delta
) noexcept
{
	std::uint16_t carry = delta;
	for(auto &digit : score) {
		const auto sum = static_cast<std::uint16_t>(digit + (carry % 10U));
		digit = static_cast<std::uint8_t>(sum % 10U);
		carry = static_cast<std::uint16_t>((carry / 10U) + (sum / 10U));
	}
}

std::uint16_t combo_add(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t hits,
	std::uint16_t bonus
) noexcept
{
	auto &combo = state.combos[player_id];
	if(hits < 2U) {
		return combo.bonus_total;
	}
	auto total = static_cast<std::uint32_t>(combo.bonus_total) + bonus;
	combo.bonus_total = static_cast<std::uint16_t>(
		std::min<std::uint32_t>(total, 0xFFFFU)
	);
	auto &player = state.players[player_id];
	player.combo_hits_max = std::max(player.combo_hits_max, hits);
	player.combo_bonus_max = std::max(player.combo_bonus_max, combo.bonus_total);
	const auto display_hits = std::min<std::uint8_t>(hits, 99U);
	if(
		display_hits > combo.hits_highest || combo.time < 32U
	) {
		combo.hits_highest = display_hits;
		if(combo.bonus_total != 0xFFFFU) {
			combo.time = 80U;
		}
	}
	return combo.bonus_total;
}

void combos_update(DeterministicState &state) noexcept
{
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		auto &combo = state.combos[player_id];
		if(combo.time == 0U) {
			continue;
		}
		combo.time--;
		if(combo.time == 0U) {
			combo.hits_highest = 0U;
			score_add(state.score[player_id], combo.bonus_total);
			combo.bonus_total = 0U;
		}
	}
}

void shots_add(DeterministicState &state, std::uint8_t player_id) noexcept
{
	const auto &player = state.players[player_id];
	if(player.shot_mode == 0U) {
		return;
	}
	// TH3R03 begins with SM_1_PAIR. Preserve the original slot-ordering and
	// coordinates; the remaining modes are added with the systems that enable
	// them.
	if(player.shot_mode != 1U) {
		return;
	}
	const auto slot = std::find_if(
		state.shotpairs.begin(), state.shotpairs.end(),
		[](const ShotPairState &shot) { return !shot.alive; }
	);
	if(slot == state.shotpairs.end()) {
		return;
	}
	slot->alive = true;
	slot->topleft_x = exact::wrap_subtract(
		player.center_x.raw(), static_cast<std::int16_t>(16 << 4)
	);
	slot->topleft_y = player.center_y.raw();
	slot->velocity_y = shot_velocity_y;
	slot->player_id = player_id;
}

void shots_update(DeterministicState &state) noexcept
{
	for(auto &shot : state.shotpairs) {
		if(!shot.alive) {
			continue;
		}
		shot.topleft_y = exact::wrap_add(shot.topleft_y, shot.velocity_y);
		if(shot.topleft_y <= -16) {
			shot.alive = false;
		}
	}
}

bool playfield_clip(
	std::int16_t x,
	std::int16_t y,
	std::int16_t negative_radius
) noexcept
{
	return (
		x <= negative_radius ||
		x >= exact::wrap_subtract(
			static_cast<std::int16_t>(288 << 4), negative_radius
		) ||
		y <= negative_radius ||
		y >= exact::wrap_subtract(
			static_cast<std::int16_t>(368 << 4), negative_radius
		)
	);
}

void chargeshot_update_reimu(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	for(std::size_t group_id = 0U; group_id < 12U; ++group_id) {
		auto &group = state.reimu_charge[player_id][group_id];
		if(group.state == 0U) {
			continue;
		}
		state.players[player_id].gauge_charged = 0U;
		for(std::size_t point = group.x.size() - 1U; point > 0U; --point) {
			group.x[point] = group.x[point - 1U];
			group.y[point] = group.y[point - 1U];
		}
		const auto velocity = vector2(group.angle, group.speed);
		group.x[0] = exact::wrap_add(group.x[0], velocity.x);
		group.y[0] = exact::wrap_add(group.y[0], velocity.y);
		if(group.state == 1U) {
			if(group.speed > 0U) {
				group.speed = static_cast<std::uint8_t>(group.speed - 4U);
			} else if(group.frame >= 0x40U) {
				group.state = 2U;
				group.angle = static_cast<std::uint8_t>(
					(group_id < 2U) ? 0xB0U : 0xD0U
				);
				group.speed = 0xE0U;
			}
		}
		group.frame++;
		if(playfield_clip(group.x.back(), group.y.back(), -(24 << 4))) {
			group.state = 0U;
		}
	}
	bool any_primary = false;
	for(std::size_t group_id = 0U; group_id < 4U; ++group_id) {
		any_primary = any_primary ||
			(state.reimu_charge[player_id][group_id].state != 0U);
	}
	state.reimu_charge_primary[player_id] = static_cast<std::uint8_t>(any_primary);
}

void chargeshot_update_kana(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto charge_state = state.kana_charge_state[player_id];
	if(charge_state == 0U) {
		return;
	}
	state.players[player_id].gauge_charged = 0U;
	for(auto &group : state.kana_charge[player_id]) {
		const auto velocity = vector2(group.angle, group.speed);
		for(std::size_t point = group.x.size() - 1U; point > 0U; --point) {
			group.x[point] = group.x[point - 1U];
			group.y[point] = group.y[point - 1U];
		}
		group.x[0] = exact::wrap_add(group.x[0], velocity.x);
		group.y[0] = exact::wrap_add(group.y[0], velocity.y);
		if(charge_state == 1U) {
			group.speed--;
		}
	}
	if(charge_state == 1U) {
		state.kana_charge[player_id][3].angle = static_cast<std::uint8_t>(
			state.kana_charge[player_id][3].angle - 2U
		);
		state.kana_charge[player_id][0].angle = static_cast<std::uint8_t>(
			state.kana_charge[player_id][0].angle + 2U
		);
		if(state.kana_charge_frames[player_id] > 0x28U) {
			state.kana_charge_state[player_id] = 2U;
			for(auto &group : state.kana_charge[player_id]) {
				group.speed = 0x80U;
			}
		}
	}
	if(state.kana_charge[player_id][0].y.back() <= -0x100) {
		state.kana_charge_state[player_id] = 0U;
	}
	state.kana_charge_frames[player_id]++;
}

void chargeshots_update(DeterministicState &state) noexcept
{
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		const auto character =
			(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
		if(character == 0) {
			chargeshot_update_reimu(state, player_id);
		} else if(character == 5) {
			chargeshot_update_kana(state, player_id);
		}
	}
}

bool chargeshot_hittest(
	const DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t left,
	std::int16_t right,
	std::int16_t top,
	std::int16_t bottom
) noexcept
{
	const auto character =
		(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
	if(character == 0) {
		if(
			state.reimu_charge_primary[player_id] != 0U &&
			(state.round_or_result_frame & 1U) != 0U
		) {
			return false;
		}
		for(std::size_t group_id = 0U; group_id < 6U; ++group_id) {
			const auto &group = state.reimu_charge[player_id][group_id];
			if(
				group.state == 0U || group.x[0] <= -(16 << 4) ||
				group.x[0] >= (288 << 4) || group.y[0] <= -(16 << 4)
			) {
				continue;
			}
			if(
				exact::wrap_subtract(group.x[0], right) <= (20 << 4) &&
				exact::wrap_subtract(left, group.x[0]) <= (20 << 4) &&
				exact::wrap_subtract(group.y[0], bottom) <= (20 << 4) &&
				exact::wrap_subtract(top, group.y[0]) <= (20 << 4)
			) {
				return true;
			}
		}
		return false;
	}
	if(character == 5 && state.kana_charge_state[player_id] != 0U) {
		for(const auto &group : state.kana_charge[player_id]) {
			for(std::size_t point = 0U; point <= 12U; point += 4U) {
				if(
					exact::wrap_subtract(group.x[point], right) <= (12 << 4) &&
					exact::wrap_subtract(left, group.x[point]) <= (12 << 4) &&
					exact::wrap_subtract(group.y[point], bottom) <= (12 << 4) &&
					exact::wrap_subtract(top, group.y[point]) <= (12 << 4)
				) {
					return true;
				}
			}
		}
	}
	return false;
}

MoveResult movement_velocity(
	const PlayerState &player,
	std::uint16_t input,
	Velocity &velocity
) noexcept
{
	auto set = [&](std::int8_t x, std::int8_t y) {
		const bool diagonal = x != 0 && y != 0;
		const auto speed_x = diagonal
			? player.speed_diagonal_x
			: player.speed_aligned_x;
		const auto speed_y = diagonal
			? player.speed_diagonal_y
			: player.speed_aligned_y;
		velocity.x = static_cast<std::int16_t>(static_cast<int>(speed_x) * x);
		velocity.y = static_cast<std::int16_t>(static_cast<int>(speed_y) * y);
	};

	switch(input) {
	case input_left: set(-1, 0); break;
	case input_down | input_left:
	case input_down_left: set(-1, 1); break;
	case input_down: set(0, 1); break;
	case input_down | input_right:
	case input_down_right: set(1, 1); break;
	case input_right: set(1, 0); break;
	case input_up | input_right:
	case input_up_right: set(1, -1); break;
	case input_up: set(0, -1); break;
	case input_up | input_left:
	case input_up_left: set(-1, -1); break;
	case 0U: return MoveResult::no_input;
	default: return MoveResult::invalid;
	}
	return MoveResult::valid;
}

void apply_velocity(PlayerState &player, const Velocity &velocity) noexcept
{
	const auto x = exact::wrap_add(player.center_x.raw(), velocity.x);
	const auto y = exact::wrap_add(player.center_y.raw(), velocity.y);
	player.center_x = exact::Subpixel::from_raw(
		std::clamp(x, player_x_min, player_x_max)
	);
	player.center_y = exact::Subpixel::from_raw(
		std::clamp(y, player_y_min, player_y_max)
	);
}

void move_human(PlayerState &player, std::uint16_t live_input) noexcept
{
	auto movement = static_cast<std::uint16_t>(live_input & input_movement);
	bool use_previous = true;
	Velocity velocity{};
	for(;;) {
		const auto result = movement_velocity(player, movement, velocity);
		if(result == MoveResult::invalid && use_previous) {
			if(player.human_movement_last != movement) {
				movement = static_cast<std::uint16_t>(
					movement & static_cast<std::uint16_t>(~player.human_movement_last)
				);
				use_previous = false;
				continue;
			}
			return;
		}
		if(result == MoveResult::valid) {
			apply_velocity(player, velocity);
		}
		if(use_previous) {
			player.human_movement_last = movement;
		}
		return;
	}
}

void add_reimu_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	auto angle = std::uint8_t{0x90U};
	for(std::size_t group_id = 0U; group_id < 4U; ++group_id) {
		auto &group = state.reimu_charge[player_id][group_id];
		group.state = 1U;
		group.frame = 0U;
		group.x.fill(center_x);
		group.y.fill(center_y);
		group.angle = angle;
		group.speed = 0x60U;
		angle = static_cast<std::uint8_t>(angle + 0x20U);
	}
	state.reimu_charge_primary[player_id] = 1U;
}

void add_kana_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	state.kana_charge_state[player_id] = 1U;
	state.kana_charge_frames[player_id] = 0U;
	constexpr std::array<std::uint8_t, 4> angles{0x90U, 0xB8U, 0xC8U, 0xF0U};
	for(std::size_t group_id = 0U; group_id < 4U; ++group_id) {
		auto &group = state.kana_charge[player_id][group_id];
		group.x.fill(center_x);
		group.y.fill(center_y);
		group.angle = angles[group_id];
		group.speed = 0x30U;
	}
}

void release_gauge(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &player = state.players[player_id];
	const auto charged = player.gauge_charged;
	if(charged >= gauge_64) {
		const auto character =
			(static_cast<int>(player.paletted_character) - 1) / 2;
		if(character == 0) {
			add_reimu_chargeshot(state, player_id);
		} else if(character == 5) {
			add_kana_chargeshot(state, player_id);
		}
		if(charged >= gauge_128) {
			if(charged >= gauge_max) {
				player.gauge_avail = gauge_64;
				player.boss_attacks_fired++;
			} else if(charged >= gauge_192) {
				player.gauge_attacks_fired++;
				player.gauge_avail = static_cast<std::uint16_t>(
					player.gauge_avail - gauge_128
				);
			} else {
				player.gauge_attacks_fired++;
				player.gauge_avail = static_cast<std::uint16_t>(
					player.gauge_avail - gauge_64
				);
			}
		}
		player.invincibility_time = static_cast<std::uint8_t>(
			player.invincibility_time + 6U
		);
	}
	player.gauge_charged = 0U;
}

void player_update_human(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint16_t live_input
) noexcept
{
	auto &player = state.players[player_id];
	if(player.invincibility_time != 0U) {
		player.invincibility_time--;
	}
	move_human(player, live_input);

	if(state.autofire_shot_gate[player_id] <= 2U) {
		if(player.gauge_charged < player.gauge_avail) {
			const auto charged = static_cast<std::uint16_t>(
				player.gauge_charged + player.gauge_charge_speed
			);
			player.gauge_charged = std::min(charged, player.gauge_avail);
		} else {
			player.spell_ready_frames++;
			if(player.spell_ready_frames >= 128U) {
				release_gauge(state, player_id);
			}
		}
	} else if(state.autofire_shot_gate[player_id] <= 4U) {
		release_gauge(state, player_id);
	}

	if((live_input & input_shot) != 0U) {
		if(state.autofire_shot_gate[player_id] > 2U) {
			if(player.move_lock_time == 0U && player.shot_active != 0U) {
				shots_add(state, player_id);
			}
			state.autofire_shot_gate[player_id] = 0U;
		}
	} else if(state.autofire_shot_gate[player_id] < 8U) {
		state.autofire_shot_gate[player_id]++;
	}

	if((live_input & input_bomb) != 0U && player.bombs != 0U) {
		player.bombs--;
	}
}

bool player_hittest(
	const DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t hitbox_size,
	std::int16_t &collision_x,
	std::int16_t &collision_y
) noexcept;

std::uint16_t cpu_initial_avoidance_input(
	const PlayerState &player,
	std::int16_t collision_x,
	std::int16_t collision_y
) noexcept
{
	const auto delta_x = static_cast<std::int16_t>(
		collision_x - player.center_x.raw()
	);
	const auto delta_y = static_cast<std::int16_t>(
		collision_y - player.center_y.raw()
	);
	std::size_t row = 0U;
	if(delta_x <= -(16 << 4)) {
		row = 0U;
	} else if(delta_x <= (16 << 4)) {
		row = 1U;
	} else {
		row = 2U;
	}
	row += static_cast<std::size_t>(player.cpu_dodge_strategy) * 3U;
	row = std::min<std::size_t>(row, cpu_collision_routes.size() - 1U);

	std::size_t direction = 0U;
	if(delta_y < -(16 << 4)) {
		if(player.center_x.raw() >= (240 << 4)) {
			if(player.center_y.raw() < (128 << 4)) {
				return input_left;
			}
			direction = 0U;
		} else if(player.center_x.raw() >= (16 << 4)) {
			direction = 1U;
		} else {
			if(player.center_y.raw() < (128 << 4)) {
				return input_right;
			}
			direction = 2U;
		}
	} else if(delta_y < (32 << 4)) {
		direction = (player.center_x.raw() >= (240 << 4))
			? 3U : ((player.center_x.raw() >= (16 << 4)) ? 4U : 5U);
	} else {
		direction = (player.center_x.raw() >= (240 << 4))
			? 6U : ((player.center_x.raw() >= (16 << 4)) ? 7U : 8U);
	}
	auto input = cpu_collision_routes[row][direction];
	if(
		player.center_y.raw() >= (304 << 4) && input == input_down
	) {
		input = (player.center_x.raw() >= (128 << 4)) ? input_left : input_right;
	}
	return input;
}

bool cpu_safe_move(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint16_t target_input
) noexcept
{
	auto &player = state.players[player_id];
	std::int16_t collision_x = 0;
	std::int16_t collision_y = 0;
	bool broad_hit = false;
	if((state.round_or_result_frame & 3U) == 0U) {
		for(std::uint8_t size = 32U; size <= 80U; size += 16U) {
			if(player_hittest(
				state, player_id, size, collision_x, collision_y
			)) {
				broad_hit = true;
				break;
			}
		}
	}
	auto input = broad_hit
		? cpu_initial_avoidance_input(player, collision_x, collision_y)
		: target_input;
	const auto center_x = player.center_x;
	const auto center_y = player.center_y;
	std::uint8_t route_attempt = 0U;
	std::uint8_t fallback_attempt = 0U;
	std::size_t route_group = 0U;
	for(;;) {
		Velocity velocity{};
		if(movement_velocity(player, input, velocity) == MoveResult::valid) {
			apply_velocity(player, velocity);
		}
		const auto hitbox_size = static_cast<std::uint8_t>(
			(fallback_attempt == 0U) ? 20U : 4U
		);
		if(!player_hittest(
			state, player_id, hitbox_size, collision_x, collision_y
		)) {
			return broad_hit;
		}
		player.center_x = center_x;
		player.center_y = center_y;
		route_attempt++;
		if(route_attempt < 9U) {
			route_group = (collision_x < (80 << 4))
				? 0U : ((collision_x < (176 << 4)) ? 1U : 2U);
			input = cpu_retry_routes[route_group][route_attempt];
			continue;
		}
		if(fallback_attempt < 9U) {
			input = cpu_retry_routes[route_group][fallback_attempt];
			fallback_attempt++;
			continue;
		}
		return true;
	}
}

void player_update_cpu(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &player = state.players[player_id];
	if(player.invincibility_time != 0U) {
		player.invincibility_time--;
	}
	player.cpu_frame++;

	std::int16_t target_x = 144 << 4;
	std::int16_t target_y = 300 << 4;
	std::uint8_t target_id = 0xFFU;
	for(const auto &enemy : state.enemies) {
		if(
			enemy.flag != EnemyFlag::running_spawned ||
			enemy.player_id != player_id ||
			enemy.center_x < 0 || enemy.center_x > (288 << 4) ||
			enemy.center_y < 0 || enemy.center_y > (384 << 4) ||
			enemy.formation_index >= target_id
		) {
			continue;
		}
		target_id = enemy.formation_index;
		target_x = static_cast<std::int16_t>(enemy.center_x - (16 << 4));
		target_y = static_cast<std::int16_t>(enemy.center_y + (110 << 4));
		if(target_id == 0U) {
			break;
		}
	}
	if(
		target_x <= 0 || target_x >= (288 << 4) ||
		target_y <= 0 || target_y >= (384 << 4)
	) {
		target_x = 144 << 4;
		target_y = 300 << 4;
	}

	std::uint8_t target_flags = static_cast<std::uint8_t>(
		(target_x + (16 << 4)) < player.center_x.raw()
	);
	target_flags = static_cast<std::uint8_t>(target_flags | (
		static_cast<std::uint8_t>((target_x - (16 << 4)) > player.center_x.raw()) << 1U
	));
	target_flags = static_cast<std::uint8_t>(target_flags | (
		static_cast<std::uint8_t>((target_y + (80 << 4)) < player.center_y.raw()) << 2U
	));
	target_flags = static_cast<std::uint8_t>(target_flags | (
		static_cast<std::uint8_t>(player.center_y.raw() < target_y) << 3U
	));
	std::uint16_t movement = 0U;
	switch(target_flags) {
	case 1U: movement = input_left; break;
	case 2U: movement = input_right; break;
	case 4U: movement = input_up; break;
	case 8U: movement = input_down; break;
	case 5U: movement = input_up | input_left; break;
	case 9U: movement = input_down | input_left; break;
	case 6U: movement = input_up | input_right; break;
	case 10U: movement = input_down | input_right; break;
	default: break;
	}
	const auto broad_collision = cpu_safe_move(state, player_id, movement);

	const auto cadence = broad_collision
		? 6U
		: ((target_id == 0xFFU)
		? 32U
		: ((target_id == 0U) ? 6U : 10U));
	auto live_input = std::uint16_t{0};
	if((state.round_or_result_frame % cadence) == 0U) {
		state.autofire_shot_gate[player_id] = 4U;
		live_input = input_shot;
	}
	if(
		player.cpu_charge_ring[player.cpu_charge_cursor] <
			(player.gauge_avail >> 4U) ||
		player.gauge_avail == gauge_max
	) {
		state.autofire_shot_gate[player_id] = 0U;
		live_input = input_shot;
		if(player.gauge_charged >= player.gauge_avail) {
			state.autofire_shot_gate[player_id] = 4U;
			live_input = 0U;
			player.cpu_charge_cursor = static_cast<std::uint8_t>(
				(player.cpu_charge_cursor + 1U) % 64U
			);
		}
	}

	if(state.autofire_shot_gate[player_id] <= 2U) {
		if(player.gauge_charged < player.gauge_avail) {
			const auto charged = static_cast<std::uint16_t>(
				player.gauge_charged + player.gauge_charge_speed
			);
			player.gauge_charged = std::min(charged, player.gauge_avail);
		} else {
			player.spell_ready_frames++;
			if(player.spell_ready_frames >= 128U) {
				release_gauge(state, player_id);
			}
		}
	} else if(state.autofire_shot_gate[player_id] <= 4U) {
		release_gauge(state, player_id);
	}

	if((live_input & input_shot) != 0U) {
		if(state.autofire_shot_gate[player_id] > 2U) {
			if(player.move_lock_time == 0U && player.shot_active != 0U) {
				shots_add(state, player_id);
			}
			state.autofire_shot_gate[player_id] = 0U;
		}
	} else if(state.autofire_shot_gate[player_id] < 8U) {
		state.autofire_shot_gate[player_id]++;
	}
}

std::int16_t enemy_speedtune(std::int16_t value, std::uint8_t speed) noexcept
{
	const auto wide = static_cast<int>(value);
	return static_cast<std::int16_t>(
		((static_cast<int>(speed) * wide) / 9) + ((2 * wide) / 3)
	);
}

bool enemy_move_and_clip(EnemyState &enemy, std::uint8_t speed) noexcept
{
	const auto velocity_x = enemy_speedtune(enemy.velocity_x, speed);
	if((enemy.position_type & position_do_not_mirror_x) != 0U) {
		enemy.center_x = exact::wrap_add(enemy.center_x, velocity_x);
	} else {
		enemy.center_x = exact::wrap_subtract(enemy.center_x, velocity_x);
	}
	if(
		(enemy.position_type & position_clip_x) != 0U &&
		(enemy.center_x <= (-32 << 4) || enemy.center_x >= (320 << 4))
	) {
		enemy.flag = EnemyFlag::free;
		return true;
	}

	const auto velocity_y = enemy_speedtune(enemy.velocity_y, speed);
	enemy.center_y = exact::wrap_add(enemy.center_y, velocity_y);
	if(
		(enemy.position_type & position_clip_bottom) != 0U &&
		enemy.center_y >= (400 << 4)
	) {
		enemy.flag = EnemyFlag::free;
		return true;
	}
	return false;
}

bool set_linear_velocity(EnemyState &enemy, std::uint8_t angle) noexcept
{
	const auto velocity = vector2(angle, static_cast<std::int16_t>(enemy.speed));
	enemy.velocity_x = velocity.x;
	enemy.velocity_y = velocity.y;
	enemy.angle = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(angle) << 8U
	);
	return true;
}

void enemy_angle_update(EnemyState &enemy, std::uint8_t speed) noexcept
{
	const auto initial = static_cast<std::int16_t>(
		(static_cast<int>(enemy.angle_speed) * (0x100 * 2)) / 3
	);
	const auto tuned = static_cast<std::int16_t>(
		initial + ((static_cast<int>(speed) * static_cast<int>(initial)) / 9)
	);
	enemy.angle = static_cast<std::uint16_t>(enemy.angle +
		static_cast<std::uint16_t>(tuned));
}

bool wait_and_advance(
	EnemyState &enemy,
	std::uint8_t duration,
	std::uint8_t op_size,
	std::uint8_t speed
) noexcept
{
	const auto tuned =
		((static_cast<int>(duration) / 2) * 3) -
		((static_cast<int>(speed) * static_cast<int>(duration)) / 6);
	if(tuned <= static_cast<int>(enemy.script_op_frame)) {
		enemy.script_op_frame = 0U;
		enemy.script_ip = static_cast<std::uint16_t>(enemy.script_ip + op_size);
	} else {
		enemy.script_op_frame++;
	}
	return true;
}

bool enemy_run(
	EnemyState &enemy,
	const GameContent &content,
	const PlayerState &player,
	std::uint8_t speed
) noexcept
{
	if(enemy.formation_type >= content.enemy_formations.size()) {
		return false;
	}
	const auto &formation = content.enemy_formations[enemy.formation_type];
	if(enemy.formation_index >= formation.enemy_scripts.size()) {
		return false;
	}
	const auto &script = formation.enemy_scripts[enemy.formation_index];
	for(;;) {
		if(enemy.script_ip >= script.size()) {
			return false;
		}
		const auto code = script[enemy.script_ip];
		if(code == op_stop) {
			enemy.flag = EnemyFlag::free;
			return true;
		}
		if(code == op_wait) {
			if(enemy.script_ip + 2U > script.size()) {
				return false;
			}
			return wait_and_advance(
				enemy, script[enemy.script_ip + 1U], 2U, speed
			);
		}
		if(code == op_spawn) {
			constexpr std::uint8_t spawn_size = 14U;
			if(
				static_cast<std::size_t>(enemy.script_ip) + spawn_size > script.size() ||
				speed >= 4U
			) {
				return false;
			}
			enemy.flag = EnemyFlag::running_spawned;
			const auto spawn_x = static_cast<std::int16_t>(
				std::bit_cast<std::int8_t>(script[enemy.script_ip + 1U]) * 128
			);
			enemy.center_x = (
				(enemy.position_type & position_do_not_mirror_x) != 0U
			) ? spawn_x : static_cast<std::int16_t>((288 << 4) - spawn_x);
			enemy.center_y = static_cast<std::int16_t>(
				std::bit_cast<std::int8_t>(script[enemy.script_ip + 2U]) * 128
			);
			const auto size_words = script[enemy.script_ip + 3U + speed];
			enemy.size_pixels = static_cast<std::uint8_t>(size_words * 16U);
			enemy.hp = script[enemy.script_ip + 7U + speed];
			if(script[enemy.script_ip + 11U] != 0U) {
				enemy.position_type |= position_clip_x;
			}
			if(script[enemy.script_ip + 12U] != 0U) {
				enemy.position_type |= position_clip_bottom;
			}
			enemy.script_ip = static_cast<std::uint16_t>(
				enemy.script_ip + spawn_size
			);
			continue;
		}
		if(
			code == op_move_linear ||
			code == op_move_linear_stop_at_player_y ||
			code == op_move_linear_stop_at_player_x
		) {
			constexpr std::uint8_t linear_size = 4U;
			if(
				static_cast<std::size_t>(enemy.script_ip) + linear_size > script.size()
			) {
				return false;
			}
			if(enemy.script_op_frame == 0U) {
				enemy.speed = script[enemy.script_ip + 2U];
				if(!set_linear_velocity(enemy, script[enemy.script_ip + 1U])) {
					return false;
				}
			}
			if(enemy_move_and_clip(enemy, speed)) {
				return true;
			}
			if(
				(code == op_move_linear_stop_at_player_y &&
					enemy.center_y >= exact::wrap_subtract(
						player.center_y.raw(), static_cast<std::int16_t>(32 << 4)
					) &&
					enemy.center_y <= exact::wrap_add(
						player.center_y.raw(), static_cast<std::int16_t>(32 << 4)
					)) ||
				(code == op_move_linear_stop_at_player_x &&
					enemy.center_x >= exact::wrap_subtract(
						player.center_x.raw(), static_cast<std::int16_t>(16 << 4)
					) &&
					enemy.center_x <= exact::wrap_add(
						player.center_x.raw(), static_cast<std::int16_t>(16 << 4)
					))
			) {
				enemy.script_op_frame = 0U;
				enemy.script_ip = static_cast<std::uint16_t>(
					enemy.script_ip + linear_size
				);
				continue;
			}
			return wait_and_advance(
				enemy, script[enemy.script_ip + 3U], linear_size, speed
			);
		}
		if(code == op_move_circular || code == op_move_circular_plus) {
			const auto op_size = static_cast<std::uint8_t>(
				(code == op_move_circular) ? 5U : 7U
			);
			if(
				static_cast<std::size_t>(enemy.script_ip) + op_size > script.size()
			) {
				return false;
			}
			if(enemy.script_op_frame == 0U) {
				enemy.angle = static_cast<std::uint16_t>(
					static_cast<std::uint16_t>(script[enemy.script_ip + 1U]) << 8U
				);
				enemy.angle_speed = std::bit_cast<std::int8_t>(
					script[enemy.script_ip + 3U]
				);
			}
			enemy.speed = script[enemy.script_ip + 2U];
			const auto velocity = vector2(
				static_cast<std::uint8_t>(enemy.angle >> 8U), enemy.speed
			);
			enemy.velocity_x = velocity.x;
			enemy.velocity_y = velocity.y;
			std::uint8_t duration_offset = 4U;
			if(code == op_move_circular_plus) {
				enemy.velocity_x = exact::wrap_add(
					enemy.velocity_x,
					static_cast<std::int16_t>(std::bit_cast<std::int8_t>(
						script[enemy.script_ip + 4U]
					))
				);
				enemy.velocity_y = exact::wrap_add(
					enemy.velocity_y,
					static_cast<std::int16_t>(std::bit_cast<std::int8_t>(
						script[enemy.script_ip + 5U]
					))
				);
				duration_offset = 6U;
			}
			if(enemy_move_and_clip(enemy, speed)) {
				return true;
			}
			enemy_angle_update(enemy, speed);
			return wait_and_advance(
				enemy, script[enemy.script_ip + duration_offset], op_size, speed
			);
		}
		if(code == op_move) {
			constexpr std::uint8_t move_size = 2U;
			if(
				static_cast<std::size_t>(enemy.script_ip) + move_size > script.size()
			) {
				return false;
			}
			if(enemy_move_and_clip(enemy, speed)) {
				return true;
			}
			return wait_and_advance(
				enemy, script[enemy.script_ip + 1U], move_size, speed
			);
		}
		if(code == op_move_with_speed) {
			constexpr std::uint8_t speed_size = 3U;
			if(
				static_cast<std::size_t>(enemy.script_ip) + speed_size > script.size()
			) {
				return false;
			}
			if(enemy.script_op_frame == 0U) {
				enemy.speed = script[enemy.script_ip + 1U];
				if(!set_linear_velocity(
					enemy, static_cast<std::uint8_t>(enemy.angle >> 8U)
				)) {
					return false;
				}
			}
			if(enemy_move_and_clip(enemy, speed)) {
				return true;
			}
			return wait_and_advance(
				enemy, script[enemy.script_ip + 2U], speed_size, speed
			);
		}
		if(code == op_move_sine_x || code == op_move_sine_y) {
			constexpr std::uint8_t sine_size = 5U;
			if(
				static_cast<std::size_t>(enemy.script_ip) + sine_size > script.size()
			) {
				return false;
			}
			if(enemy.script_op_frame == 0U) {
				enemy.angle = 0U;
				enemy.angle_speed = std::bit_cast<std::int8_t>(
					script[enemy.script_ip + 2U]
				);
			}
			const auto oscillating = vector2(
				static_cast<std::uint8_t>(enemy.angle >> 8U),
				static_cast<std::int16_t>(script[enemy.script_ip + 1U])
			).x;
			const auto linear = static_cast<std::int16_t>(
				std::bit_cast<std::int8_t>(script[enemy.script_ip + 3U])
			);
			if(code == op_move_sine_x) {
				enemy.velocity_x = oscillating;
				enemy.velocity_y = linear;
			} else {
				enemy.velocity_x = linear;
				enemy.velocity_y = oscillating;
			}
			if(enemy_move_and_clip(enemy, speed)) {
				return true;
			}
			enemy_angle_update(enemy, speed);
			return wait_and_advance(
				enemy, script[enemy.script_ip + 4U], sine_size, speed
			);
		}
		if(code == op_clip_x || code == op_clip_bottom) {
			if(code == op_clip_x) {
				enemy.position_type |= position_clip_x;
			} else {
				enemy.position_type |= position_clip_bottom;
			}
			enemy.script_ip++;
			continue;
		}
		return false;
	}
}

bool add_formation(
	DeterministicState &state,
	const GameContent &content,
	std::uint8_t player_id
) noexcept
{
	if(state.enemies_alive[player_id] != 0U) {
		return true;
	}
	const auto cursor = state.formation_cursor[player_id];
	const auto formation_id = state.formation_type_ring[cursor];
	if(formation_id >= content.enemy_formations.size()) {
		return false;
	}
	const auto position_type = state.formation_position_ring[cursor];
	const auto &formation = content.enemy_formations[formation_id];
	for(std::size_t index = 0U; index < formation.enemy_scripts.size(); ++index) {
		auto slot = std::find_if(
			state.enemies.begin(), state.enemies.end(),
			[](const EnemyState &enemy) { return enemy.flag == EnemyFlag::free; }
		);
		if(slot == state.enemies.end()) {
			break;
		}
		*slot = EnemyState{};
		slot->flag = EnemyFlag::running_unspawned;
		slot->player_id = player_id;
		slot->formation_type = formation_id;
		slot->formation_index = static_cast<std::uint8_t>(index);
		slot->position_type = position_type;
	}
	state.formation_cursor[player_id] = static_cast<std::uint8_t>(cursor + 1U);
	return true;
}

void collmap_set_enemy(
	DeterministicState &state,
	const EnemyState &enemy
) noexcept
{
	if(enemy.center_y < 0 || enemy.center_y >= (368 << 4)) {
		return;
	}
	const auto tile_w = static_cast<int>((enemy.size_pixels * 3U) / 16U);
	const auto tile_h = tile_w;
	int top = static_cast<int>(enemy.center_y) >> 5;
	top -= tile_h / 2;
	int bottom = top + tile_h;
	top = std::max(top, 0);
	bottom = std::min(bottom, static_cast<int>(collmap_height) - 1);
	const auto h = bottom - top;
	if(h <= 0) {
		return;
	}
	int left = static_cast<int>(enemy.center_x) >> 5;
	left -= tile_w / 2;
	const int right = left + tile_w;
	left = std::max(left, 0);
	for(int x = left; x < right && x < static_cast<int>(collmap_width); ++x) {
		for(int row = 0; row < h; row += 4) {
			state.collision_map[enemy.player_id][static_cast<std::size_t>(x)]
				[static_cast<std::size_t>(top + row)] = true;
		}
	}
}

[[maybe_unused]] bool player_hittest(
	const DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t hitbox_size,
	std::int16_t &collision_x,
	std::int16_t &collision_y
) noexcept
{
	const auto &player = state.players[player_id];
	if(player.invincibility_time != 0U) {
		return false;
	}
	const auto radius = static_cast<int>(hitbox_size >> 1U);
	const auto center_x = static_cast<int>(player.center_x.raw()) >> 5;
	const auto center_y = static_cast<int>(player.center_y.raw()) >> 5;
	const auto left = center_x - radius;
	const auto top = std::max(center_y - radius, 0);
	const auto bottom = std::min(
		center_y - radius + static_cast<int>(hitbox_size),
		static_cast<int>(collmap_height) - 1
	);
	const auto right = left + static_cast<int>(hitbox_size);
	const auto first_byte = left >> 3;
	const auto last_byte = (right - 1) >> 3;
	for(int byte_x = first_byte; byte_x <= last_byte; ++byte_x) {
		if(byte_x < 0 || byte_x >= static_cast<int>(collmap_width / 8U)) {
			continue;
		}
		const auto tile_begin = std::max(left, byte_x * 8);
		const auto tile_end = std::min(right, (byte_x + 1) * 8);
		for(int y = top; y < bottom; ++y) {
			for(int x = tile_begin; x < tile_end; ++x) {
				if(state.collision_map[player_id][static_cast<std::size_t>(x)]
					[static_cast<std::size_t>(y)]) {
					collision_x = static_cast<std::int16_t>(byte_x << 8);
					collision_y = static_cast<std::int16_t>(top << 5);
					return true;
				}
			}
		}
	}
	return false;
}

bool enemies_update(
	DeterministicState &state,
	const GameContent &content
) noexcept
{
	state.enemies_alive = {};
	for(auto &enemy : state.enemies) {
		if(enemy.flag == EnemyFlag::free) {
			continue;
		}
		enemy.frame++;
		if(
			static_cast<std::uint8_t>(enemy.flag) >=
			static_cast<std::uint8_t>(EnemyFlag::explosion_ignoring_enemies)
		) {
			if(enemy.frame == 10U) {
				enemy.flag = EnemyFlag::explosion_hitting_enemies;
			} else if(enemy.frame == 32U) {
				enemy.flag = EnemyFlag::explosion_ignoring_enemies;
			} else if(enemy.frame >= 36U) {
				enemy.flag = EnemyFlag::free;
			}
			continue;
		}
		if(!enemy_run(
			enemy, content, state.players[enemy.player_id], state.enemy_speed
		)) {
			return false;
		}
		if(enemy.flag != EnemyFlag::free) {
			state.enemies_alive[enemy.player_id]++;
			if(enemy.flag == EnemyFlag::running_spawned) {
				collmap_set_enemy(state, enemy);
			}
		}
	}

	for(auto target_it = state.enemies.rbegin(); target_it != state.enemies.rend(); ++target_it) {
		auto &target = *target_it;
		if(target.flag != EnemyFlag::running_spawned || target.center_y <= 0) {
			continue;
		}
		const auto radius = static_cast<std::int16_t>(
			(target.size_pixels == 16U) ? (10 << 4) : (target.size_pixels * 7)
		);
		const auto left = exact::wrap_subtract(target.center_x, radius);
		const auto right = exact::wrap_add(target.center_x, radius);
		const auto top = exact::wrap_subtract(target.center_y, radius);
		const auto bottom = exact::wrap_add(target.center_y, radius);
		bool hit = false;
		bool explosion_collision = false;
		std::uint8_t explosion_chain_slot = 0U;

		for(auto &shot : state.shotpairs) {
			if(!shot.alive || shot.player_id != target.player_id) {
				continue;
			}
			const auto shot_center_x = exact::wrap_add(
				shot.topleft_x, shotpair_half_width
			);
			const auto shot_center_y = exact::wrap_add(
				shot.topleft_y, shotpair_half_height
			);
			if(
				exact::wrap_subtract(shot_center_x, shotpair_half_width) > right ||
				exact::wrap_add(shot_center_x, shotpair_half_width) < left ||
				shot_center_y < top || shot_center_y > bottom
			) {
				continue;
			}
			shot.alive = false;
			hit = true;
		}

		for(auto &explosion : state.enemies) {
			const auto flag = static_cast<std::uint8_t>(explosion.flag);
			if(
				flag < static_cast<std::uint8_t>(EnemyFlag::explosion_ignoring_enemies) ||
				explosion.player_id != target.player_id ||
				explosion.flag == EnemyFlag::explosion_ignoring_enemies ||
				flag >= static_cast<std::uint8_t>(EnemyFlag::explosion_hitting_enemies) +
					static_cast<std::uint8_t>(explosion.explosion_max_enemy_hits_half * 2U) ||
				(explosion.frame & 3U) != 0U
			) {
				continue;
			}
			const auto explosion_radius = static_cast<std::int16_t>(
				explosion.size_pixels * 6
			);
			if(
				right < exact::wrap_subtract(explosion.center_x, explosion_radius) ||
				bottom < exact::wrap_subtract(explosion.center_y, explosion_radius) ||
				left > exact::wrap_add(explosion.center_x, explosion_radius) ||
				top > exact::wrap_add(explosion.center_y, explosion_radius)
			) {
				continue;
			}
			explosion.flag = static_cast<EnemyFlag>(flag + 1U);
			explosion_collision = true;
			explosion_chain_slot = explosion.chain_slot;
			hit = true;
		}
		if(chargeshot_hittest(
			state, target.player_id, left, right, top, bottom
		)) {
			hit = true;
		}

		if(!hit) {
			continue;
		}
		if(target.hp != 0U) {
			target.hp--;
			target.frame = 0U;
			continue;
		}

		target.flag = EnemyFlag::explosion_ignoring_enemies;
		target.frame = 0U;
		if(!explosion_collision) {
			const auto slot = state.chains.cursor[target.player_id];
			target.chain_slot = slot;
			state.chains.hits[target.player_id][slot] = 1U;
			state.chains.pellet_and_fireball_value[target.player_id][slot] = 0U;
			state.chains.cursor[target.player_id] = static_cast<std::uint8_t>(
				(slot + 1U) % chain_ring_size
			);
		} else {
			target.chain_slot = explosion_chain_slot;
			auto &hits = state.chains.hits[target.player_id][explosion_chain_slot];
			if(hits != 0xFFU) {
				hits++;
			}
			if(state.players[target.player_id].gauge_avail < gauge_max) {
				state.players[target.player_id].gauge_avail = std::min<std::uint16_t>(
					gauge_max,
					static_cast<std::uint16_t>(
						state.players[target.player_id].gauge_avail + 32U
					)
				);
			}
			(void)combo_add(
				state, target.player_id, hits,
				static_cast<std::uint16_t>(hits * 16U)
			);
			const auto charge_slot = static_cast<std::uint8_t>(
				state.round_speed / 32U
			);
			if(hits <= 2U) {
				state.chains.charge_fireball[target.player_id][charge_slot]++;
			} else if(hits <= static_cast<std::uint8_t>(9U - charge_slot)) {
				// A chained kill in this range launches one transfer pellet. Its
				// constructor draws an overlapping 16-bit ring value for the target
				// X coordinate and another for the target Y coordinate.
				(void)randring_next16(state);
				(void)randring_next16(state);
				state.chains.charge_fireball[target.player_id][charge_slot] =
					static_cast<std::uint8_t>(
						state.chains.charge_fireball[target.player_id][charge_slot] + 2U
					);
			} else if(hits <= static_cast<std::uint8_t>(14U - (charge_slot * 2U))) {
				state.chains.charge_fireball[target.player_id][charge_slot] =
					static_cast<std::uint8_t>(
						state.chains.charge_fireball[target.player_id][charge_slot] + 3U
					);
				if((hits & 1U) == 0U) {
					state.chains.charge_exatt[target.player_id][charge_slot]++;
				}
			} else {
				state.chains.charge_fireball[target.player_id][charge_slot] =
					static_cast<std::uint8_t>(
						state.chains.charge_fireball[target.player_id][charge_slot] + 4U
					);
				state.chains.charge_exatt[target.player_id][charge_slot]++;
			}
		}
		target.explosion_max_enemy_hits_half = static_cast<std::uint8_t>(
			target.size_pixels / 16U
		);
		score_add(
			state.score[target.player_id],
			static_cast<std::uint16_t>((target.size_pixels / 16U) * 10U)
		);
	}
	return true;
}

void hud_start_update_prefix(DeterministicState &state) noexcept
{
	if(state.hud_start_flag == 1U) {
		state.hud_start_state = 0U;
		state.hud_start_frame = 0U;
		state.hud_start_flag = 2U;
	}
	if(state.hud_start_state == 0U) {
		// The first banner chunk is also the last to settle. Its velocity passes
		// through zero and becomes negative before it returns to x=64.
		state.hud_start_frame++;
		if(state.hud_start_frame >= 69U) {
			state.hud_start_state = 1U;
			state.hud_start_frame = 0U;
		}
	} else if(state.hud_start_state == 1U) {
		state.hud_start_frame++;
		if(state.hud_start_frame > 0x56U) {
			state.hud_start_state = 2U;
			state.hud_start_frame = 0U;
		}
	} else if(state.hud_start_state == 2U) {
		// The particle initialization is gameplay-relevant despite being purely
		// visual: each of 128 particles consumes two 16-bit ring values and two
		// master.lib IRAND values, in that order.
		for(std::size_t i = 0; i < 128U; ++i) {
			(void)(randring_next16(state) & 511U);
			(void)randring_next16(state);
			(void)(irand(state.random_seed) % 3U);
			(void)(irand(state.random_seed) % 3U);
		}
		state.hud_start_state = 3U;
		state.hud_start_frame = 0U;
	} else if(state.hud_start_state == 3U) {
		state.hud_start_frame++;
		if(state.hud_start_frame >= 0x40U) {
			state.hud_start_state = 4U;
		}
	} else {
		state.hud_start_flag = 0U;
	}
}

void background_update(DeterministicState &state) noexcept
{
	if(state.background_phase != BackgroundPhase::initial) {
		return;
	}

	if(state.background_initial_kind == BackgroundInitialKind::type_a) {
		// sub_D135(): 24 GRCG particles followed by 24 sprite particles.
		for(std::size_t i = 0; i < 48U; ++i) {
			(void)irand(state.random_seed);
		}
		if(state.round_frame > 128U) {
			state.background_phase = BackgroundPhase::type_a_steady;
		}
	} else {
		// sub_D340(): 24 GRCG particles, then only the first 16 of its
		// 26 sprite particles call irand(). These presentation calls share the
		// gameplay RNG and are therefore part of deterministic call order.
		for(std::size_t i = 0; i < 40U; ++i) {
			(void)irand(state.random_seed);
		}
		if(state.round_frame > 144U) {
			state.background_phase = BackgroundPhase::type_b_steady;
		}
	}
}

} // namespace

bool gameplay_frame_prefix(
	DeterministicState &state,
	const GameContent &content
) noexcept
{
	state.collision_map = {};
	shots_update(state);
	chargeshots_update(state);
	hud_start_update_prefix(state);
	if(state.hud_start_flag != 0U) {
		return true;
	}
	if(content.enemy_formations.empty()) {
		return false;
	}
	state.enemy_speed = static_cast<std::uint8_t>(state.round_speed / 32U);
	if(!add_formation(state, content, 0U) || !add_formation(state, content, 1U)) {
		return false;
	}
	return enemies_update(state, content);
}

void gameplay_input_latch(
	DeterministicState &state,
	CoreInput &input,
	std::uint8_t autofire_mask
) noexcept
{
	auto apply = [&](std::uint16_t value, std::uint8_t player_id) {
		if(state.players[player_id].is_cpu != 0U) {
			return value;
		}
		if((autofire_mask & (1U << player_id)) == 0U) {
			return value;
		}
		if((input.charge_mask & (1U << player_id)) != 0U) {
			return static_cast<std::uint16_t>(value | input_shot);
		}
		if(
			(value & input_shot) != 0U &&
			state.autofire_shot_gate[player_id] <= 3U
		) {
			return static_cast<std::uint16_t>(value & ~input_shot);
		}
		return value;
	};
	input.p1 = apply(input.p1, 0U);
	input.p2 = apply(input.p2, 1U);
	state.input_player = {input.p1, input.p2};
	state.input_system = input.system;
	state.global_frame++;
}

void gameplay_frame_suffix(DeterministicState &state) noexcept
{
	player_update_human(state, 0U, state.input_player[0]);
	if(state.players[1].is_cpu != 0U) {
		player_update_cpu(state, 1U);
	} else {
		player_update_human(state, 1U, state.input_player[1]);
	}
	background_update(state);
	combos_update(state);

	state.round_frame++;
	state.round_or_result_frame++;
	if((state.round_or_result_frame & 63U) == 0U) {
		if(state.round_speed < 0x7FU) {
			state.round_speed++;
		}
		if((state.round_or_result_frame & 1023U) == 0U) {
			randring_fill(state);
			for(auto &player : state.players) {
				if(player.hit_damage_next < 4U) {
					player.hit_damage_next++;
				}
			}
		}
		if(state.round_or_result_frame < 2000U) {
			state.players[0].cpu_frame = 0U;
			state.players[1].cpu_frame = 0U;
		}
	}
}

} // namespace th03::detail
