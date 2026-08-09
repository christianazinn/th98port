// Included by frame.cpp inside its private implementation namespace.

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

std::uint16_t cpu_target_movement(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto &player = state.players[player_id];
	const auto target = cpu_target_point(state, player_id);
	std::uint8_t target_flags = static_cast<std::uint8_t>(
		exact::wrap_add(target.x, static_cast<std::int16_t>(16 << 4)) <
		player.center_x.raw()
	);
	target_flags = static_cast<std::uint8_t>(target_flags | (
		static_cast<std::uint8_t>(
			exact::wrap_subtract(
				target.x, static_cast<std::int16_t>(16 << 4)
			) > player.center_x.raw()
		) << 1U
	));
	target_flags = static_cast<std::uint8_t>(target_flags | (
		static_cast<std::uint8_t>(
			exact::wrap_add(target.y, static_cast<std::int16_t>(80 << 4)) <
			player.center_y.raw()
		) << 2U
	));
	target_flags = static_cast<std::uint8_t>(target_flags | (
		static_cast<std::uint8_t>(player.center_y.raw() < target.y) << 3U
	));
	switch(target_flags) {
	case 1U: return input_left;
	case 2U: return input_right;
	case 4U: return input_up;
	case 8U: return input_down;
	case 5U: return input_up | input_left;
	case 9U: return input_down | input_left;
	case 6U: return input_up | input_right;
	case 10U: return input_down | input_right;
	default: return 0U;
	}
}

bool cpu_safe_move(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &player = state.players[player_id];
	std::int16_t collision_x = 0;
	std::int16_t collision_y = 0;
	bool broad_hit = false;
	if((state.round_or_result_frame & 3U) == 0U) {
		for(std::uint8_t size = 32U; size <= 80U; size += 16U) {
			const auto hit = player_hittest(
				state, player_id, size, collision_x, collision_y
			);
			if(hit) {
				broad_hit = true;
				break;
			}
		}
	}
	auto input = broad_hit
		? cpu_initial_avoidance_input(player, collision_x, collision_y)
		: cpu_target_movement(state, player_id);
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
		const auto route_hit = player_hittest(
			state, player_id, hitbox_size, collision_x, collision_y
		);
		if(!route_hit) {
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
		if(player.gauge_charged <= gauge_64) {
			player_bomb(state, player_id);
		} else {
			player.is_hit = true;
		}
		return broad_hit;
	}
}

void player_update_cpu(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &player = state.players[player_id];
	auto live_input = std::uint16_t{0};
	if(!player_update_begin(state, player_id)) {
		return;
	}
	if(player.move_lock_time != 0U) {
		player.move_lock_time--;
		player_update_finish(state, player_id, live_input);
		return;
	}

	player.cpu_frame++;
	if(player.cpu_frame <= player.cpu_safety_frames) {
		// A broad collision makes sub_C568() skip target selection. The shared
		// target scratch therefore retains the value written by an earlier
		// callback in this frame.
		(void)cpu_safe_move(state, player_id);
	} else {
		const auto movement = cpu_target_movement(state, player_id);
		(void)(randring_next16(state) % 3U);
		Velocity velocity{};
		if(movement_velocity(player, movement, velocity) == MoveResult::valid) {
			apply_velocity(player, velocity);
		}
		std::int16_t collision_x = 0;
		std::int16_t collision_y = 0;
		const auto small_hit = player_hittest(
			state, player_id, 4U, collision_x, collision_y
		);
		if(small_hit) {
			if(player.gauge_charged > (80U << 4U)) {
				player.is_hit = true;
			} else if((randring_next16(state) & 3U) == 0U) {
				player_bomb(state, player_id);
			} else {
				player.is_hit = true;
			}
		}
	}

	std::uint16_t cadence = 10U;
	if(state.cpu_target_kind_scratch == 0) {
		cadence = 6U;
	} else if(state.cpu_target_kind_scratch == -1) {
		cadence = 32U;
	} else if(state.cpu_target_kind_scratch == -2) {
		cadence = 7U;
	}
	if((state.round_or_result_frame % cadence) == 0U) {
		state.autofire_shot_gate[player_id] = 4U;
		live_input = input_shot;
	}
	state.cpu_target_kind_scratch = 0;
	if(player.is_hit) {
		player.is_hit = false;
		state.autofire_shot_gate[player_id] = 4U;
		live_input = 0U;
	} else if(!(
		player.bombs == 0U &&
		static_cast<std::int8_t>(player.halfhearts) <= 3
	)) {
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
	}

	player_update_finish(state, player_id, live_input);
}
