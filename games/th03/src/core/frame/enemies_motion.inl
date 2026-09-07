// Included by ../frame.cpp inside its private implementation namespace.

std::int16_t enemy_speedtune(std::int16_t value, std::uint8_t speed) noexcept
{
	const auto tuned_one = static_cast<std::int16_t>(
		exact::wrap_multiply(
			static_cast<std::int16_t>(speed), value
		) / static_cast<std::int16_t>(9)
	);
	const auto tuned_two = static_cast<std::int16_t>(
		exact::wrap_multiply(static_cast<std::int16_t>(2), value) /
		static_cast<std::int16_t>(3)
	);
	return exact::wrap_add(tuned_one, tuned_two);
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

void set_linear_velocity(EnemyState &enemy, std::uint8_t angle) noexcept
{
	const auto velocity = vector2(angle, static_cast<std::int16_t>(enemy.speed));
	enemy.velocity_x = velocity.x;
	enemy.velocity_y = velocity.y;
}

void set_linear_angle_and_velocity(
	EnemyState &enemy,
	std::uint8_t angle
) noexcept
{
	set_linear_velocity(enemy, angle);
	enemy.angle = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(angle) << 8U
	);
}

void enemy_angle_update(EnemyState &enemy, std::uint8_t speed) noexcept
{
	const auto initial = static_cast<std::int16_t>(
		exact::wrap_multiply(
			static_cast<std::int16_t>(enemy.angle_speed),
			static_cast<std::int16_t>(0x100 * 2)
		) / static_cast<std::int16_t>(3)
	);
	const auto tuned = exact::wrap_add(
		initial,
		static_cast<std::int16_t>(
			exact::wrap_multiply(
				static_cast<std::int16_t>(speed), initial
			) / static_cast<std::int16_t>(9)
		)
	);
	enemy.angle = exact::wrap_add(
		enemy.angle, static_cast<std::uint16_t>(tuned)
	);
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
			enemy.explosion_max_enemy_hits_half = size_words;
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
				set_linear_angle_and_velocity(
					enemy, script[enemy.script_ip + 1U]
				);
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
				set_linear_velocity(
					enemy, static_cast<std::uint8_t>(enemy.angle >> 8U)
				);
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
	state.enemy_speed = static_cast<std::uint8_t>(state.round_speed / 32U);
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
		slot->flag = EnemyFlag::running_unspawned;
		slot->script_ip = 0U;
		slot->script_op_frame = 0U;
		slot->loop_index = 0U;
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

void collmap_set_fireball(
	DeterministicState &state,
	const FireballState &fireball
) noexcept
{
	constexpr int tile_w = 16 / 2;
	constexpr int tile_h = 16 / 2;
	const auto center_y = exact::wrap_add(
		fireball.center_y, static_cast<std::int16_t>(8 << 4)
	);
	if(center_y < 0 || center_y >= (368 << 4)) {
		return;
	}
	int top = static_cast<int>(center_y) >> 5;
	top -= tile_h / 2;
	int bottom = top + tile_h;
	top = std::max(top, 0);
	bottom = std::min(bottom, static_cast<int>(collmap_height) - 1);
	const auto h = bottom - top;
	if(h <= 0) {
		return;
	}
	int left = static_cast<int>(fireball.center_x) >> 5;
	left -= tile_w / 2;
	const int right = left + tile_w;
	left = std::max(left, 0);
	for(int x = left; x < right && x < static_cast<int>(collmap_width); ++x) {
		for(int row = 0; row < h; row += 4) {
			state.collision_map[fireball.player_id][static_cast<std::size_t>(x)]
				[static_cast<std::size_t>(top + row)] = true;
		}
	}
}

void collmap_set_rect_striped(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y,
	int tile_w,
	int tile_h
) noexcept
{
	if(center_y < 0 || center_y >= (368 << 4)) {
		return;
	}
	int top = static_cast<int>(center_y) >> 5;
	top -= tile_h / 2;
	int bottom = top + tile_h;
	top = std::max(top, 0);
	bottom = std::min(bottom, static_cast<int>(collmap_height) - 1);
	const auto height = bottom - top;
	if(height <= 0) {
		return;
	}
	int left = static_cast<int>(center_x) >> 5;
	left -= tile_w / 2;
	int width = tile_w;
	if(left < 0) {
		width += left;
		left = 0;
	}
	if(width <= 0) {
		return;
	}
	const auto right = std::min(
		left + width, static_cast<int>(collmap_width)
	);
	for(int x = left; x < right; ++x) {
		for(int row = 0; row < height; row += 4) {
			state.collision_map[player_id][static_cast<std::size_t>(x)]
				[static_cast<std::size_t>(top + row)] = true;
		}
	}
}

void collmap_set_slope_striped(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t top_x,
	std::int16_t bottom_x
) noexcept
{
	constexpr std::int16_t stripe_count =
		static_cast<std::int16_t>(collmap_height / 2U);
	const auto slope_width = exact::wrap_subtract(bottom_x, top_x);
	auto left = bottom_x;
	auto y_double = static_cast<std::int16_t>(stripe_count - 1);
	int row = static_cast<int>(collmap_height) - 1;
	for(;;) {
		const auto tile_x = static_cast<std::uint16_t>(left) >> 5U;
		for(std::uint16_t column = 0U; column < 2U; ++column) {
			const auto x = static_cast<std::uint16_t>(tile_x + column);
			if(x < collmap_width) {
				state.collision_map[player_id][x][static_cast<std::size_t>(row)] =
					true;
			}
		}
		y_double--;
		if(y_double == 0) {
			return;
		}
		const auto interpolated = static_cast<std::int16_t>(
			(static_cast<std::int32_t>(slope_width) * y_double) / stripe_count
		);
		left = exact::wrap_add(top_x, interpolated);
		row -= 2;
	}
}
