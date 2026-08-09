// Included by ../frame.cpp inside its private implementation namespace.

void boss_move(BossState &boss) noexcept
{
	boss.center_x = exact::wrap_add(boss.center_x, boss.velocity_x);
	boss.center_y = exact::wrap_add(boss.center_y, boss.velocity_y);
	boss.motion_angle++;
	boss.velocity_y = vector2(boss.motion_angle, 16).y;
	if(boss.center_x <= (48 << 4)) {
		boss.velocity_x = 32;
	} else if(boss.center_x >= (240 << 4)) {
		boss.velocity_x = -32;
	}
}

void boss_exit_move(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &boss = state.boss;
	boss.center_x = exact::wrap_add(boss.center_x, boss.velocity_x);
	boss.center_y = exact::wrap_add(boss.center_y, static_cast<std::int16_t>(32));
	if(boss.center_x <= (48 << 4)) {
		boss.velocity_x = 32;
	} else if(boss.center_x >= (240 << 4)) {
		boss.velocity_x = -32;
	}
	state.boss_target_active[1U - player_id] = 0U;
	if(boss.center_y >= (416 << 4)) {
		boss.state = 0U;
		state.gba_boss_launched_by = -1;
		state.combo_points_for_boss_attack = 5120U;
	}
}

bool boss_launch_or_reverse(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	if(state.gba_flag_active[player_id] != GaugeBossAttackFlag::boss) {
		return false;
	}
	if(state.gba_boss_launched_by < 0) {
		const auto initial = state.boss_initial[player_id];
		// ZUN keeps this byte outside the 32-byte boss record copied at launch,
		// so a new boss attack inherits the previous attack's last pattern.
		const auto pattern_previous = state.boss.pattern_previous;
		state.boss = BossState{};
		static_cast<BossInitialState &>(state.boss) = initial;
		state.boss.pattern_previous = pattern_previous;
		state.boss.pattern_count = static_cast<std::uint8_t>(
			(randring_next16(state) & 7U) + 1U
		);
		state.boss.pattern_frame = 0U;
		state.gba_boss_launched_by = static_cast<std::int8_t>(player_id);
		state.gba_flag_active[player_id] = GaugeBossAttackFlag::none;
		state.boss_target_active[1U - player_id] = 1U;
		return true;
	}
	if(state.boss.state != 0xFFU) {
		state.boss.state = 0xFFU;
		state.boss.pattern_frame = 0U;
		state.boss_target_active[player_id] = 0U;
	}
	return false;
}

void boss_target_update(DeterministicState &state) noexcept
{
	if(state.boss.state != 0x80U) {
		state.boss_target_x = state.boss.center_x;
		state.boss_target_y = exact::wrap_add(
			state.boss.center_y, static_cast<std::int16_t>(128 << 4)
		);
	}
}

void boss_choose_pattern(DeterministicState &state) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if(boss.pattern_frame < 0x50U) {
		return;
	}
	std::uint8_t next = 0U;
	for(;;) {
		next = static_cast<std::uint8_t>(randring_next16(state) & 0x0FU);
		if((static_cast<int>(boss.pattern_previous) + 2) < next) {
			break;
		}
		if((static_cast<int>(boss.pattern_previous) - 2) <= next) {
			continue;
		}
		break;
	}
	boss.pattern_previous = next;
	boss.pattern_frame = 0U;
	boss.state = static_cast<std::uint8_t>(next + 2U);
	boss.pattern_index++;
	if(boss.pattern_index > boss.pattern_count) {
		boss.state = 0x80U;
	}
}

std::uint8_t boss_hittest_damage(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t radius
) noexcept
{
	if(
		state.boss.center_y < 0 ||
		state.defeat_flag == DefeatFlag::banner
	) {
		return 0U;
	}
	const auto left = exact::wrap_subtract(state.boss.center_x, radius);
	const auto right = exact::wrap_add(state.boss.center_x, radius);
	const auto top = exact::wrap_subtract(state.boss.center_y, radius);
	const auto bottom = exact::wrap_add(state.boss.center_y, radius);
	std::uint8_t damage = 0U;
	for(auto &shot : state.shotpairs) {
		if(!shot.alive || shot.player_id != player_id) {
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
		damage = static_cast<std::uint8_t>(damage + 2U);
	}
	damage = static_cast<std::uint8_t>(damage + chargeshot_hittest(
		state, player_id, left, right, top, bottom
	));
	if(
		state.bomb_flag[player_id] != BombFlag::inactive &&
		(state.round_or_result_frame & 1U) == 0U
	) {
		damage++;
	}
	return damage;
}

void boss_defeat_if_needed(
	DeterministicState &state,
	std::uint8_t target_player
) noexcept
{
	if(
		static_cast<std::int16_t>(state.boss.hp) > 0 ||
		state.boss.state == 0xFFU
	) {
		return;
	}
	state.boss.state = 0xFFU;
	state.boss.pattern_frame = 0U;
	if(state.gba_flag_active[target_player] != GaugeBossAttackFlag::boss) {
		state.combo_points_for_boss_attack = 5120U;
	}
	state.boss_target_active[target_player] = 0U;
	if(state.gba_boss_level < 16U) {
		state.gba_boss_level++;
	}
}

void boss_pattern_reimu_orbit_pellets(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 0U) {
		boss.anchor_x = boss.center_x;
		boss.anchor_y = exact::wrap_add(
			boss.center_y, static_cast<std::int16_t>(48 << 4)
		);
		boss.direction = static_cast<std::uint8_t>(randring_next16(state) & 1U);
		boss.pattern_angle = 0x10U;
	}
	auto angle = static_cast<std::uint8_t>(boss.pattern_frame * 2U);
	if(boss.direction != 0U) {
		angle = static_cast<std::uint8_t>(0U - angle);
	}
	if((boss.pattern_frame % boss.params[0]) == 0U) {
		const auto offset = vector2(angle, 32 << 4);
		bullets_add_group(
			state,
			BulletFlag::pellet_cloud,
			exact::wrap_add(boss.center_x, offset.x),
			exact::wrap_add(boss.center_y, offset.y),
			static_cast<std::uint8_t>(1U - player_id),
			boss.pattern_angle,
			static_cast<std::uint8_t>(angle * 2U),
			bullet_group_one
		);
		angle = static_cast<std::uint8_t>(angle + 0x80U);
		const auto opposite = vector2(angle, 32 << 4);
		bullets_add_group(
			state,
			BulletFlag::pellet_cloud,
			exact::wrap_add(boss.center_x, opposite.x),
			exact::wrap_add(boss.center_y, opposite.y),
			static_cast<std::uint8_t>(1U - player_id),
			boss.pattern_angle,
			static_cast<std::uint8_t>((angle - 0x80U) * 2U + 0x80U),
			bullet_group_one
		);
	}
	if((boss.pattern_frame & 1U) == 0U) {
		boss.pattern_angle++;
	}
	const auto y_angle = static_cast<std::uint8_t>(
		static_cast<std::uint8_t>(boss.pattern_frame * 2U) - 0x40U
	);
	boss.center_y = exact::wrap_add(
		boss.anchor_y, vector2(y_angle, 48 << 4).y
	);
	if(boss.pattern_frame >= 0x80U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_reimu_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if(boss.pattern_frame < 0x40U) {
		if((state.round_frame & 0x0FU) != 0U) {
			return;
		}
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[2], 0x20U,
			bullet_group_three_spread_narrow
		);
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[2], 0x60U,
			bullet_group_three_spread_narrow
		);
		return;
	}
	if(boss.pattern_frame < 0x64U) {
		if((state.round_frame & 0x0FU) != 0U) {
			return;
		}
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[2], 0x40U,
			bullet_group_five_spread_medium
		);
		return;
	}
	boss.state = 1U;
	boss.pattern_frame = 0U;
}

void boss_pattern_reimu_orbit_exatt(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if(boss.pattern_frame == 0U) {
		boss.render_state = 1U;
		boss.pattern_angle = static_cast<std::uint8_t>(randring_next16(state));
	}
	const auto count = boss.params[3];
	if(boss.pattern_frame < 0x18U) {
		const auto radius = static_cast<std::int16_t>(boss.pattern_frame << 5U);
		for(std::size_t i = 0U; i < count; ++i) {
			const auto angle = static_cast<std::uint8_t>(
				((i << 8U) / count) + boss.pattern_angle
			);
			const auto offset = vector2(angle, radius);
			boss.orbit_x[i] = exact::wrap_add(boss.center_x, offset.x);
			boss.orbit_y[i] = exact::wrap_add(boss.center_y, offset.y);
		}
		boss.pattern_angle = static_cast<std::uint8_t>(boss.pattern_angle + 8U);
		return;
	}
	if(boss.pattern_frame < 0x50U) {
		for(std::size_t i = 0U; i < count; ++i) {
			const auto angle = static_cast<std::uint8_t>(
				((i << 8U) / count) + boss.pattern_angle
			);
			const auto offset = vector2(angle, 48 << 4);
			boss.orbit_x[i] = exact::wrap_add(boss.center_x, offset.x);
			boss.orbit_y[i] = exact::wrap_add(boss.center_y, offset.y);
		}
		boss.pattern_angle = static_cast<std::uint8_t>(boss.pattern_angle + 8U);
		if((state.round_frame & 0x0FU) == 0U) {
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id), boss.params[4],
				static_cast<std::uint8_t>(randring_next16(state)),
				bullet_group_ring, boss.params[5]
			);
		}
		return;
	}
	if(boss.pattern_frame == 0x50U) {
		for(std::size_t i = 0U; i < count; ++i) {
			const auto angle = static_cast<std::uint8_t>(
				((i << 8U) / count) + boss.pattern_angle
			);
			extra_attack_add_direct(
				state, player_id, boss.orbit_x[i], boss.orbit_y[i], angle, 0x50U
			);
		}
		boss.state = 1U;
		boss.pattern_frame = 0U;
		boss.render_state = 0U;
	}
}

void boss_pattern_reimu_four_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if(boss.pattern_frame < 0x80U) {
		if((boss.pattern_frame & 0x0FU) != 0U) {
			return;
		}
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[2],
			static_cast<std::uint8_t>(boss.pattern_frame),
			bullet_group_four_spread_narrow
		);
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[2],
			static_cast<std::uint8_t>(0x78U - boss.pattern_frame),
			bullet_group_four_spread_narrow
		);
		return;
	}
	boss.state = 1U;
	boss.pattern_frame = 0U;
}

void boss_update_reimu(DeterministicState &state, std::uint8_t player_id) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			5U - (state.gba_boss_level / 5U)
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x18U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 2U) + 0x28U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 8U) + 4U
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x10U
		);
		state.boss.params[5] = state.boss.params[4];
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	if(state.boss.state == 0U) {
		if(state.boss.pattern_frame != 0x64U) {
			return;
		}
		state.boss.pattern_frame = 0U;
		state.boss.state = 1U;
	}
	if(state.boss.state == 1U) {
		boss_choose_pattern(state);
	}
	switch(state.boss.state) {
	case 2U: case 3U: case 4U: case 5U: case 6U:
		boss_pattern_reimu_orbit_pellets(state, player_id);
		break;
	case 8U: case 9U: case 10U: case 13U:
		boss_pattern_reimu_spreads(state, player_id);
		break;
	case 15U: case 16U: case 17U:
		boss_pattern_reimu_orbit_exatt(state, player_id);
		break;
	case 7U: case 11U: case 12U: case 14U:
		boss_pattern_reimu_four_spreads(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 28, 28
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_pattern_mima_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame < 2U) {
		boss.render_state = 2U;
		state.mima_boss_spread_angle = 0U;
		state.mima_boss_pellet_angle = 192U;
		return;
	}
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	if((boss.pattern_frame & 1U) == 0U) {
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			boss.center_x,
			boss.center_y,
			target_player,
			boss.params[0],
			state.mima_boss_spread_angle,
			bullet_group_two_spread_horizontally_symmetric
		);
		state.mima_boss_spread_angle = static_cast<std::uint8_t>(
			state.mima_boss_spread_angle + 9U
		);
		if(
			static_cast<std::uint16_t>(boss.params[5]) > boss.pattern_frame &&
			(boss.pattern_frame % 6U) == 0U
		) {
			bullets_add_group(
				state,
				BulletFlag::pellet,
				boss.center_x,
				boss.center_y,
				target_player,
				boss.params[4],
				state.mima_boss_pellet_angle,
				bullet_group_four_spread_medium
			);
			bullets_add_group(
				state,
				BulletFlag::pellet,
				boss.center_x,
				boss.center_y,
				target_player,
				boss.params[4],
				static_cast<std::uint8_t>(
					0x80U - state.mima_boss_pellet_angle
				),
				bullet_group_four_spread_medium
			);
		}
		state.mima_boss_pellet_angle = static_cast<std::uint8_t>(
			state.mima_boss_pellet_angle + 3U
		);
	}
	if(boss.pattern_frame > 0x82U) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_mima_random_bursts(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 0x10U) {
		boss.render_state = 5U;
	} else if(boss.pattern_frame < 0x20U) {
		return;
	}
	const auto center_x = exact::wrap_add(
		boss.center_x, static_cast<std::int16_t>(24 << 4)
	);
	const auto center_y = exact::wrap_add(
		boss.center_y, static_cast<std::int16_t>(-(32 << 4))
	);
	if(
		boss.pattern_frame == 0x20U || boss.pattern_frame == 0x24U ||
		boss.pattern_frame == 0x28U
	) {
		return;
	}
	if(boss.pattern_frame <= 0x30U) {
		return;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	if((boss.pattern_frame & 3U) == 0U) {
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			center_x,
			center_y,
			target_player,
			boss.params[2],
			0U,
			bullet_group_random_angle_and_speed,
			boss.params[1]
		);
		bullets_add_group(
			state,
			BulletFlag::pellet,
			center_x,
			center_y,
			target_player,
			boss.params[2],
			0U,
			bullet_group_random_angle_and_speed,
			boss.params[1]
		);
	}
	if(boss.pattern_frame == 0x40U) {
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			center_x,
			center_y,
			target_player,
			2U << 4U,
			static_cast<std::uint8_t>(randring_next16(state)),
			bullet_group_thirty_two_ring
		);
	}
	if(boss.pattern_frame >= 0x80U) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_mima_aimed_trails(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	const auto phase_mod = static_cast<std::uint8_t>(
		boss.pattern_frame % boss.params[3]
	);
	boss_move(boss);
	boss.render_state = 5U;
	const auto half_phase = static_cast<std::uint8_t>(boss.params[3] / 2U);
	if(phase_mod == 1U || phase_mod == half_phase) {
		const auto group = (phase_mod == half_phase)
			? bullet_group_four_spread_medium_aimed
			: bullet_group_five_spread_medium_aimed;
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			exact::wrap_add(
				boss.center_x, static_cast<std::int16_t>(24 << 4)
			),
			exact::wrap_add(
				boss.center_y, static_cast<std::int16_t>(-(32 << 4))
			),
			static_cast<std::uint8_t>(1U - player_id),
			(5U << 4U) + 8U,
			0U,
			group,
			0U,
			0U,
			true
		);
	}
	if(boss.pattern_frame >= 150U) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_mima_orbit_clouds(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if(boss.pattern_frame == 1U) {
		state.mima_boss_orbit_angle_delta =
			((randring_next16(state) & 1U) != 0U)
				? std::int8_t{7}
				: std::int8_t{-7};
		state.mima_boss_orbit_angle = static_cast<std::uint8_t>(
			randring_next16(state)
		);
	}
	if((boss.pattern_frame & 1U) == 0U) {
		const auto offset = vector2(state.mima_boss_orbit_angle, 48 << 4);
		bullets_add_group(
			state,
			BulletFlag::pellet_cloud,
			exact::wrap_add(boss.center_x, offset.x),
			exact::wrap_add(boss.center_y, offset.y),
			static_cast<std::uint8_t>(1U - player_id),
			boss.params[6],
			static_cast<std::uint8_t>(
				state.mima_boss_orbit_angle + 0x40U
			),
			bullet_group_two_ring
		);
		state.mima_boss_orbit_angle = static_cast<std::uint8_t>(
			state.mima_boss_orbit_angle + state.mima_boss_orbit_angle_delta
		);
	}
	if(boss.pattern_frame >= 0x48U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_update_mima(DeterministicState &state, std::uint8_t player_id) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x20U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 4U) + 2U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x20U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			0x40U - (state.gba_boss_level * 2U)
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x28U
		);
		state.boss.params[5] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 4U) + 0x40U
		);
		state.boss.params[6] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x36U
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	const auto dispatch_state = state.boss.state;
	switch(dispatch_state) {
	case 0U:
		if(state.boss.pattern_frame != 0x64U) {
			return;
		}
		state.boss.pattern_frame = 0U;
		state.boss.state = 1U;
		break;
	case 1U:
		boss_choose_pattern(state);
		break;
	case 2U: case 3U: case 4U: case 5U:
		boss_pattern_mima_spreads(state, player_id);
		break;
	case 6U: case 7U: case 8U: case 9U: case 10U:
		boss_pattern_mima_random_bursts(state, player_id);
		break;
	case 11U: case 12U: case 13U:
		boss_pattern_mima_aimed_trails(state, player_id);
		break;
	case 14U: case 15U: case 16U: case 17U:
		boss_pattern_mima_orbit_clouds(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 28, 28
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_pattern_marisa_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss.render_state = 1U;
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	if(boss.pattern_frame == 0x20U) {
		state.marisa_boss_spread_angle = 0U;
	}
	if(boss.pattern_frame < 0x50U) {
		if((boss.pattern_frame & 3U) != 0U) {
			return;
		}
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			boss.center_x,
			boss.center_y,
			target_player,
			boss.params[0],
			state.marisa_boss_spread_angle,
			bullet_group_two_spread_horizontally_symmetric
		);
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			boss.center_x,
			boss.center_y,
			target_player,
			static_cast<std::uint8_t>(boss.params[0] / 2U),
			state.marisa_boss_spread_angle,
			bullet_group_two_spread_horizontally_symmetric
		);
		state.marisa_boss_spread_angle = static_cast<std::uint8_t>(
			state.marisa_boss_spread_angle + 7U
		);
		return;
	}
	boss.render_state = 0U;
	boss.state = 1U;
	boss.pattern_frame = 0U;
}

void boss_marisa_add_direct_pair(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t first_x,
	std::int16_t second_x
) noexcept
{
	constexpr auto center_y = static_cast<std::int16_t>(368 << 4);
	extra_attack_add_direct_marisa(state, player_id, first_x, center_y);
	extra_attack_add_direct_marisa(state, player_id, second_x, center_y);
}

void boss_pattern_marisa_four_direct_pairs(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss.render_state = 1U;
	if(boss.pattern_frame < 0x38U) {
		return;
	}
	switch(boss.pattern_frame) {
	case 0x40U:
		boss_marisa_add_direct_pair(state, player_id, 8 << 4, 280 << 4);
		return;
	case 0x50U:
		boss_marisa_add_direct_pair(state, player_id, 56 << 4, 232 << 4);
		return;
	case 0x60U:
		boss_marisa_add_direct_pair(state, player_id, 104 << 4, 184 << 4);
		return;
	case 0x70U:
		boss_marisa_add_direct_pair(state, player_id, 152 << 4, 136 << 4);
		return;
	default:
		break;
	}
	if(boss.pattern_frame == 0x84U) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_marisa_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame & 0x1FU) == 0U) {
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		auto angle = static_cast<std::uint8_t>(randring_next16(state));
		std::uint8_t cloud_count = boss.params[1];
		if(state.gba_boss_level >= 8U) {
			bullets_add_group(
				state,
				BulletFlag::bullet16,
				boss.center_x,
				boss.center_y,
				target_player,
				boss.params[3],
				angle,
				bullet_group_ring,
				boss.params[4]
			);
			angle = static_cast<std::uint8_t>(
				angle + (256U / static_cast<unsigned>(boss.params[4]))
			);
			cloud_count = boss.params[4];
		}
		bullets_add_group(
			state,
			BulletFlag::pellet_cloud,
			boss.center_x,
			boss.center_y,
			target_player,
			boss.params[2],
			angle,
			bullet_group_ring,
			cloud_count
		);
	}
	if(boss.pattern_frame >= 0x82U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_marisa_three_direct_pairs(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss.render_state = 1U;
	if(boss.pattern_frame < 0x38U) {
		return;
	}
	switch(boss.pattern_frame) {
	case 0x40U:
		boss_marisa_add_direct_pair(state, player_id, 32 << 4, 256 << 4);
		return;
	case 0x50U:
		boss_marisa_add_direct_pair(state, player_id, 80 << 4, 208 << 4);
		return;
	case 0x60U:
		boss_marisa_add_direct_pair(state, player_id, 128 << 4, 160 << 4);
		return;
	default:
		break;
	}
	if(boss.pattern_frame == 0x70U) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_update_marisa(DeterministicState &state, std::uint8_t player_id) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 2U) + 0x32U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 2U) + 0x18U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x20U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x0AU
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x16U
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	const auto dispatch_state = state.boss.state;
	switch(dispatch_state) {
	case 0U:
		if(state.boss.pattern_frame != 0x64U) {
			return;
		}
		state.boss.pattern_frame = 0U;
		state.boss.state = 1U;
		break;
	case 1U:
		boss_choose_pattern(state);
		break;
	case 2U: case 3U: case 4U: case 5U:
		boss_pattern_marisa_spreads(state, player_id);
		break;
	case 6U: case 7U: case 17U:
		boss_pattern_marisa_three_direct_pairs(state, player_id);
		break;
	case 8U: case 9U: case 10U:
		boss_pattern_marisa_four_direct_pairs(state, player_id);
		break;
	case 11U: case 12U: case 13U: case 14U: case 15U: case 16U:
		boss_pattern_marisa_rings(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 28, 28
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_pattern_ellen_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 0x18U) {
		state.ellen_boss_spread_angle = 0x40U;
		return;
	}
	if(boss.pattern_frame < 0x1CU) {
		return;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	if(boss.pattern_frame < 0x24U) {
		if((boss.pattern_frame & 1U) != 0U) {
			return;
		}
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			target_player, boss.params[0], state.ellen_boss_spread_angle,
			bullet_group_two_spread_horizontally_symmetric
		);
		state.ellen_boss_spread_angle = static_cast<std::uint8_t>(
			state.ellen_boss_spread_angle + 8U
		);
	} else if(boss.pattern_frame == 0x24U) {
		for(std::size_t i = 0U; i < 8U; ++i) {
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				target_player, boss.params[0], state.ellen_boss_spread_angle,
				bullet_group_two_spread_horizontally_symmetric
			);
			state.ellen_boss_spread_angle = static_cast<std::uint8_t>(
				state.ellen_boss_spread_angle + 8U
			);
		}
		state.bullet_template_angle = state.ellen_boss_spread_angle;
	} else if(boss.pattern_frame <= 0x2CU) {
		if((boss.pattern_frame & 1U) != 0U) {
			return;
		}
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			target_player, boss.params[0], state.ellen_boss_spread_angle,
			bullet_group_two_spread_horizontally_symmetric
		);
		state.ellen_boss_spread_angle = static_cast<std::uint8_t>(
			state.ellen_boss_spread_angle + 8U
		);
	}
	if(boss.pattern_frame >= 0x2CU) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_ellen_orbits(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	if(
		boss.pattern_frame == 0x20U || boss.pattern_frame == 0x24U ||
		boss.pattern_frame == 0x28U
	) {
		return;
	}
	if(boss.pattern_frame == 0x30U || boss.pattern_frame == 0x60U) {
		auto angle = static_cast<std::uint8_t>(randring_next16(state));
		const auto count = boss.params[1];
		const auto delta = (boss.pattern_frame == 0x30U)
			? std::int8_t{2} : std::int8_t{-2};
		for(std::uint8_t i = 0U; i < count; ++i) {
			extra_attack_add_direct_ellen(
				state, player_id, boss.center_x, boss.center_y, angle, delta
			);
			angle = static_cast<std::uint8_t>(
				angle + (256U / static_cast<unsigned>(count))
			);
		}
		if(boss.pattern_frame == 0x60U) {
			boss.state = 1U;
			boss.pattern_frame = 0U;
		}
	}
}

void boss_pattern_ellen_pellet_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame % boss.params[3]) == 1U) {
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		bullets_add_group(
			state, BulletFlag::pellet, boss.center_x, boss.center_y,
			target_player, boss.params[2], state.ellen_boss_pellet_angle,
			bullet_group_four_spread_narrow
		);
		bullets_add_group(
			state, BulletFlag::pellet, boss.center_x, boss.center_y,
			target_player, boss.params[2],
			static_cast<std::uint8_t>(state.ellen_boss_pellet_angle + 0x80U),
			bullet_group_four_spread_narrow
		);
		const auto speed_half = static_cast<std::uint8_t>(boss.params[2] / 2U);
		bullets_add_group(
			state, BulletFlag::pellet, boss.center_x, boss.center_y,
			target_player, speed_half,
			static_cast<std::uint8_t>(state.ellen_boss_pellet_angle + 0x80U),
			bullet_group_four_spread_narrow
		);
		bullets_add_group(
			state, BulletFlag::pellet, boss.center_x, boss.center_y,
			target_player, speed_half, state.ellen_boss_pellet_angle,
			bullet_group_four_spread_narrow
		);
		state.ellen_boss_pellet_angle = static_cast<std::uint8_t>(
			state.ellen_boss_pellet_angle + 0x10U
		);
	}
	if(boss.pattern_frame >= 0x60U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_update_ellen(DeterministicState &state, std::uint8_t player_id) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 2U) + 0x32U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 10U) + 4U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x30U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			10U - (state.gba_boss_level / 3U)
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	const auto dispatch_state = state.boss.state;
	if(dispatch_state == 0U) {
		if(state.boss.pattern_frame != 0x64U) {
			return;
		}
		state.boss.pattern_frame = 0U;
		state.boss.state = 1U;
	} else if(dispatch_state == 1U) {
		boss_choose_pattern(state);
	}
	switch(dispatch_state) {
	case 2U: case 3U: case 4U: case 5U:
		boss_pattern_ellen_spreads(state, player_id);
		break;
	case 6U: case 7U: case 8U: case 9U: case 10U:
		boss_pattern_ellen_orbits(state, player_id);
		break;
	case 11U: case 12U: case 13U: case 14U:
	case 15U: case 16U: case 17U:
		boss_pattern_ellen_pellet_spreads(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 20, 24
	);
	const auto damage = boss_hittest_damage(state, target_player, 24 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_kotohime_add_from_orbit(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t group,
	std::uint8_t speed,
	bool randomize_angle
) noexcept
{
	auto &boss = state.boss;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto center_count = boss.params[1];
	for(std::uint8_t i = 0U; i < center_count; ++i) {
		const auto center_angle = static_cast<std::uint8_t>(
			((static_cast<unsigned>(i) << 8U) / center_count) +
			boss.pattern_angle
		);
		const auto offset = vector2(center_angle, boss.anchor_x);
		const auto angle = randomize_angle
			? static_cast<std::uint8_t>(randring_next16(state))
			: std::uint8_t{0U};
		bullets_add_group(
			state,
			BulletFlag::bullet16,
			exact::wrap_add(boss.center_x, offset.x),
			exact::wrap_add(boss.center_y, offset.y),
			target_player,
			speed,
			angle,
			group
		);
	}
}

void boss_pattern_kotohime_random_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		boss.anchor_x = 0;
		boss.render_frame = 0U;
	}
	boss.pattern_angle = static_cast<std::uint8_t>(boss.pattern_angle + 4U);
	if(boss.anchor_x > (64 << 4)) {
		boss.render_state = 2U;
		boss.pattern_angle = static_cast<std::uint8_t>(
			boss.pattern_angle + 2U
		);
		boss.render_frame++;
		if(boss.render_frame > 0x40U) {
			boss_kotohime_add_from_orbit(
				state,
				player_id,
				bullet_group_sixteen_ring,
				boss.params[0],
				true
			);
			boss.render_state = 0U;
			boss.state = 1U;
			boss.pattern_frame = 0U;
		}
		return;
	}
	boss_move(boss);
	boss.anchor_x = exact::wrap_add(
		boss.anchor_x, static_cast<std::int16_t>(0x20)
	);
}

void boss_pattern_kotohime_aimed_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		boss.anchor_x = 0;
		boss.render_frame = 0U;
		boss.pattern_angle = 0U;
	}
	boss.pattern_angle = static_cast<std::uint8_t>(boss.pattern_angle - 2U);
	if(boss.anchor_x > (64 << 4)) {
		boss.render_state = 2U;
		boss.anchor_x = exact::wrap_add(
			boss.anchor_x, static_cast<std::int16_t>(8)
		);
		boss.pattern_angle = static_cast<std::uint8_t>(
			boss.pattern_angle + 2U
		);
		boss.render_frame++;
		if(boss.render_frame > 0x40U) {
			boss_kotohime_add_from_orbit(
				state,
				player_id,
				bullet_group_two_spread_narrow_aimed,
				boss.params[3],
				false
			);
			boss_kotohime_add_from_orbit(
				state,
				player_id,
				bullet_group_five_spread_medium_aimed,
				boss.params[2],
				false
			);
			boss.render_state = 0U;
			boss.state = 1U;
			boss.pattern_frame = 0U;
		}
		return;
	}
	boss_move(boss);
	boss.anchor_x = exact::wrap_add(
		boss.anchor_x, static_cast<std::int16_t>(0x20)
	);
}

void boss_pattern_kotohime_pellet_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame % boss.params[5]) == 1U) {
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		const auto angle = static_cast<std::uint8_t>(boss.pattern_frame);
		constexpr std::array<std::int16_t, 5> offsets_x{
			-(48 << 4), -(24 << 4), 0, 24 << 4, 48 << 4,
		};
		constexpr std::array<std::int16_t, 5> offsets_y{
			0, 12 << 4, 24 << 4, 12 << 4, 0,
		};
		for(std::size_t i = 0U; i < offsets_x.size(); ++i) {
			bullets_add_group(
				state,
				BulletFlag::pellet,
				exact::wrap_add(boss.center_x, offsets_x[i]),
				exact::wrap_add(boss.center_y, offsets_y[i]),
				target_player,
				boss.params[4],
				angle,
				bullet_group_ring,
				4U
			);
		}
	}
	if(boss.pattern_frame >= 0x60U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_kotohime_bursts(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		boss.anchor_x = 0;
		boss.render_frame = 0U;
	}
	boss.pattern_angle = static_cast<std::uint8_t>(boss.pattern_angle - 4U);
	if(boss.anchor_x > (64 << 4)) {
		boss.render_state = 2U;
		boss.anchor_x = exact::wrap_add(
			boss.anchor_x, static_cast<std::int16_t>(8)
		);
		boss.pattern_angle = static_cast<std::uint8_t>(
			boss.pattern_angle - 2U
		);
		boss.render_frame++;
		if(boss.render_frame > 0x40U) {
			const auto target_player = static_cast<std::uint8_t>(1U - player_id);
			const auto center_count = boss.params[1];
			const auto burst_bullet_count = boss.params[6];
			for(std::uint8_t outer = 0U; outer < center_count; ++outer) {
				const auto center_angle = static_cast<std::uint8_t>(
					((static_cast<unsigned>(outer) << 8U) / center_count) +
					boss.pattern_angle
				);
				const auto offset = vector2(center_angle, boss.anchor_x);
				const auto center_x = exact::wrap_add(boss.center_x, offset.x);
				const auto center_y = exact::wrap_add(boss.center_y, offset.y);
				for(std::uint8_t inner = 0U; inner < burst_bullet_count; ++inner) {
					const auto angle = static_cast<std::uint8_t>(
						((static_cast<unsigned>(inner) << 7U) / burst_bullet_count) +
						center_angle + 0x40U
					);
					const auto speed = static_cast<std::uint8_t>(
						((static_cast<unsigned>(inner) * 0x30U) / burst_bullet_count) +
						0x10U
					);
					bullets_add_group(
						state,
						BulletFlag::bullet16,
						center_x,
						center_y,
						target_player,
						speed,
						angle,
						bullet_group_one
					);
				}
			}
			boss.render_state = 0U;
			boss.state = 1U;
			boss.pattern_frame = 0U;
		}
		return;
	}
	boss_move(boss);
	boss.anchor_x = exact::wrap_add(
		boss.anchor_x, static_cast<std::int16_t>(0x20)
	);
}

void boss_update_kotohime(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 3U) + 6U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x20U
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x18U
		);
		state.boss.params[5] = static_cast<std::uint8_t>(
			0x20U - (state.gba_boss_level / 2U)
		);
		state.boss.params[6] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 8U
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	const auto dispatch_state = state.boss.state;
	switch(dispatch_state) {
	case 0U:
		if(state.boss.pattern_frame != 0x64U) {
			return;
		}
		state.boss.pattern_frame = 0U;
		state.boss.state = 1U;
		break;
	case 1U:
		boss_choose_pattern(state);
		break;
	case 2U: case 3U: case 4U: case 5U: case 6U:
		boss_pattern_kotohime_random_rings(state, player_id);
		break;
	case 7U: case 8U: case 9U:
		boss_pattern_kotohime_aimed_spreads(state, player_id);
		break;
	case 12U: case 13U: case 14U:
		boss_pattern_kotohime_pellet_rings(state, player_id);
		break;
	case 10U: case 11U: case 15U: case 16U: case 17U:
		boss_pattern_kotohime_bursts(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 32, 24
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_pattern_kana_accel_ring(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		return;
	}
	if(boss.pattern_frame == 0x10U) {
		boss.render_frame = 0U;
		boss.render_state = 2U;
		return;
	}
	if(boss.pattern_frame == 0x28U) {
		extra_attack_add_direct(state, player_id, boss.center_x, boss.center_y, 224U, 8U);
		extra_attack_add_direct(state, player_id, boss.center_x, boss.center_y, 8U, 8U);
		extra_attack_add_direct(state, player_id, boss.center_x, boss.center_y, 0x78U, 8U);
		extra_attack_add_direct(state, player_id, boss.center_x, boss.center_y, 160U, 8U);
		bullets_add_group(
			state, BulletFlag::bullet16, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), 3U << 4U,
			state.bullet_template_angle,
			bullet_group_ring, boss.params[0], 0x80U
		);
		return;
	}
	if(boss.pattern_frame > 0x50U) {
		boss.render_state = 0U;
		boss.pattern_frame = 0U;
		boss.state = 1U;
	}
}

void boss_pattern_kana_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		return;
	}
	if(boss.pattern_frame == 0x10U) {
		boss.render_frame = 0U;
		boss.render_state = 2U;
		return;
	}
	if(boss.pattern_frame >= 0x28U && boss.pattern_frame < 0x50U) {
		if(boss.pattern_frame == 0x28U) {
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id), 1U << 4U,
				state.bullet_template_angle,
				bullet_group_ring, boss.params[1]
			);
			return;
		}
		if(boss.pattern_frame == 0x40U) {
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id), (2U << 4U) + 8U,
				state.bullet_template_angle,
				bullet_group_ring, boss.params[2]
			);
			auto angle = std::uint8_t{0x20U};
			auto speed = std::uint8_t{1U << 4U};
			for(std::size_t i = 0U; i < 0x10U; ++i) {
				bullets_add_group(
					state, BulletFlag::pellet, boss.center_x, boss.center_y,
					static_cast<std::uint8_t>(1U - player_id), speed, angle,
					bullet_group_two_spread_horizontally_symmetric
				);
				angle = static_cast<std::uint8_t>(angle + 6U);
				speed = static_cast<std::uint8_t>(speed + 3U);
			}
			// The source mutates the shared template after the final add too.
			state.bullet_template_angle = angle;
			return;
		}
		if(boss.pattern_frame == 0x48U) {
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id), (3U << 4U) + 8U,
				state.bullet_template_angle,
				bullet_group_ring, boss.params[3]
			);
		}
		return;
	}
	if(boss.pattern_frame > 0x60U) {
		boss.render_state = 0U;
		boss.pattern_frame = 0U;
		boss.state = 1U;
	}
}

void boss_pattern_kana_clouds(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame % boss.params[4]) == 0U) {
		auto angle = static_cast<std::uint8_t>(
			(randring_next16(state) & 0x1FU) - 0x10U
		);
		if((randring_next16(state) & 1U) != 0U) {
			angle = static_cast<std::uint8_t>(0x80U - angle);
		}
		extra_attack_add_direct(
			state, player_id, boss.center_x, boss.center_y, angle, 8U
		);
		bullets_add_group(
			state, BulletFlag::pellet_cloud, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), 3U << 4U,
			state.bullet_template_angle,
			bullet_group_ring, boss.params[5]
		);
	}
	if(boss.pattern_frame > 0x64U) {
		boss.render_state = 0U;
		boss.pattern_frame = 0U;
		boss.state = 1U;
	}
}

void boss_pattern_kana_spiral(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		return;
	}
	if(boss.pattern_frame == 0x10U) {
		boss.render_frame = 0U;
		boss.render_state = 2U;
		boss.pattern_angle = static_cast<std::uint8_t>(-0x40);
		switch(randring_next16(state) & 3U) {
		case 0U: boss.angle_delta = -3; break;
		case 1U: boss.angle_delta = -1; break;
		case 2U: boss.angle_delta = 1; break;
		default: boss.angle_delta = 3; break;
		}
		return;
	}
	if(boss.pattern_frame >= 0x20U && boss.pattern_frame < 0x6CU) {
		if((boss.pattern_frame & 3U) == 0U) {
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id), boss.params[6],
				boss.pattern_angle, bullet_group_ring, 5U
			);
		}
		if(boss.pattern_frame > 0x40U) {
			boss.pattern_angle = static_cast<std::uint8_t>(
				boss.pattern_angle + boss.angle_delta
			);
		}
		return;
	}
	if(boss.pattern_frame > 0x70U) {
		boss.render_state = 0U;
		boss.pattern_frame = 0U;
		boss.state = 1U;
	}
}

void boss_update_kana(DeterministicState &state, std::uint8_t player_id) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x18U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x14U
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			0x20U - state.gba_boss_level
		);
		state.boss.params[5] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x18U
		);
		state.boss.params[6] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x40U
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	const auto dispatch_state = state.boss.state;
	switch(dispatch_state) {
	case 0U:
		if(state.boss.pattern_frame != 0x64U) {
			return;
		}
		state.boss.pattern_frame = 0U;
		state.boss.state = 1U;
		break;
	case 1U:
		boss_choose_pattern(state);
		break;
	case 2U: case 3U: case 4U:
		boss_pattern_kana_accel_ring(state, player_id);
		break;
	case 7U: case 8U: case 9U: case 10U: case 11U: case 12U:
		boss_pattern_kana_rings(state, player_id);
		break;
	case 13U: case 14U: case 15U:
		boss_pattern_kana_clouds(state, player_id);
		break;
	case 5U: case 6U: case 16U: case 17U:
		boss_pattern_kana_spiral(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	state.boss.render_frame++;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 32, 24
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_pattern_rikako_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 0x28U) {
		const auto angle_delta = ((randring_next16(state) & 1U) != 0U)
			? static_cast<std::uint8_t>(0x30U)
			: static_cast<std::uint8_t>(-0x30);
		const auto count = boss.params[0];
		for(std::size_t i = 0U; i < count; ++i) {
			const auto angle = static_cast<std::uint8_t>(
				(i << 8U) / count
			);
			const auto offset = vector2(angle, 48 << 4);
			bullets_add_group(
				state,
				BulletFlag::bullet16,
				exact::wrap_add(boss.center_x, offset.x),
				exact::wrap_add(boss.center_y, offset.y),
				static_cast<std::uint8_t>(1U - player_id),
				(1U << 4U) + 12U,
				static_cast<std::uint8_t>(angle + angle_delta),
				bullet_group_five_spread_wide
			);
		}
	}
	if(boss.pattern_frame > 0x50U) {
		boss.pattern_frame = 0U;
		boss.state = 1U;
	}
}

void boss_pattern_rikako_exatt_and_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 0x10U) {
		// SUB_CE5B() is a presentation effect with one shared-ring draw.
		(void)randring_next16(state);
	}
	if(boss.pattern_frame == 0x28U) {
		// This draw is overwritten before first use in ZUN's loop.
		(void)randring_next16(state);
		const auto count = boss.params[1];
		for(std::size_t i = 0U; i < count; ++i) {
			const auto angle = static_cast<std::uint8_t>(
				(i << 8U) / count
			);
			extra_attack_add_direct_rikako(
				state, player_id, boss.center_x, boss.center_y, angle
			);
		}
	}
	if(boss.pattern_frame == 0x40U) {
		auto speed = std::uint8_t{2U << 4U};
		for(std::size_t i = 0U; i < 4U; ++i) {
			speed = static_cast<std::uint8_t>(speed + 8U);
			bullets_add_group(
				state,
				BulletFlag::pellet,
				boss.center_x,
				boss.center_y,
				static_cast<std::uint8_t>(1U - player_id),
				speed,
				static_cast<std::uint8_t>(randring_next16(state)),
				bullet_group_ring,
				boss.params[2]
			);
		}
	}
	if(boss.pattern_frame > 0x50U) {
		boss.pattern_frame = 0U;
		boss.state = 1U;
	}
}

void boss_pattern_rikako_four_spreads(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	const auto phase_mod = static_cast<std::uint16_t>(
		boss.pattern_frame % boss.params[3]
	);
	if(phase_mod == 1U) {
		boss.pattern_angle = static_cast<std::uint8_t>(randring_next16(state));
		boss.direction = static_cast<std::uint8_t>(
			(randring_next16(state) & 1U) + 1U
		);
	}
	if(
		phase_mod == 1U || phase_mod == 5U ||
		phase_mod == 9U || phase_mod == 0x0DU
	) {
		const auto type = (boss.direction == 1U)
			? BulletFlag::pellet
			: BulletFlag::bullet16;
		bullets_add_group(
			state, type, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[4],
			boss.pattern_angle, bullet_group_four_spread_narrow
		);
		bullets_add_group(
			state, type, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id), boss.params[4],
			static_cast<std::uint8_t>(boss.pattern_angle + 0x80U),
			bullet_group_four_spread_narrow
		);
	}
	if(boss.pattern_frame >= 0x80U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_update_rikako(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 4U) + 8U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x1CU
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			0x20U - state.gba_boss_level
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x40U
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}
	boss_target_update(state);
	state.boss.pattern_frame++;
	const auto dispatch_state = state.boss.state;
	switch(dispatch_state) {
	case 0U:
		if(state.boss.pattern_frame == 0x64U) {
			state.boss.pattern_frame = 0U;
			state.boss.state = 1U;
		} else {
			// The original switch branch skips the common collision tail while
			// Rikako's Boss Attack is still materializing.
			return;
		}
		break;
	case 1U:
		boss_choose_pattern(state);
		break;
	case 2U: case 3U: case 4U: case 5U:
		boss_pattern_rikako_spreads(state, player_id);
		break;
	case 7U: case 8U: case 9U: case 10U: case 11U: case 12U:
		boss_pattern_rikako_exatt_and_rings(state, player_id);
		break;
	case 6U: case 13U: case 14U: case 15U: case 16U: case 17U:
		boss_pattern_rikako_four_spreads(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 32, 24
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_chiyuri_random_effect(DeterministicState &state) noexcept
{
	// SUB_CE5B() is a presentation effect with one shared-ring draw.
	(void)randring_next16(state);
}

void boss_chiyuri_finish_pattern(BossState &boss) noexcept
{
	boss.state = 1U;
	boss.pattern_frame = 0U;
	boss.render_state = 0x20U;
}

void boss_pattern_chiyuri_cardinals(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if((boss.pattern_frame % boss.params[1]) != 1U) {
		return;
	}
	if(boss.pattern_frame == 1U) {
		state.chiyuri_boss_pattern_angle = 224U;
		state.chiyuri_boss_pattern_direction = 0U;
		boss_chiyuri_random_effect(state);
		boss_chiyuri_random_effect(state);
		boss_chiyuri_random_effect(state);
		boss_chiyuri_random_effect(state);
	} else if(boss.pattern_frame >= 0x80U) {
		// ZUN resets the pattern before emitting this invocation's four pellets.
		boss_chiyuri_finish_pattern(boss);
	}

	if(state.chiyuri_boss_pattern_direction == 0U) {
		state.chiyuri_boss_pattern_angle = static_cast<std::uint8_t>(
			state.chiyuri_boss_pattern_angle + 8U
		);
		if(state.chiyuri_boss_pattern_angle == 0x20U) {
			state.chiyuri_boss_pattern_direction = 1U;
			state.chiyuri_boss_pattern_angle = 0x24U;
		}
	} else {
		state.chiyuri_boss_pattern_angle = static_cast<std::uint8_t>(
			state.chiyuri_boss_pattern_angle - 8U
		);
		if(state.chiyuri_boss_pattern_angle == 228U) {
			state.chiyuri_boss_pattern_direction = 0U;
			state.chiyuri_boss_pattern_angle = 224U;
		}
	}

	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	auto angle = state.chiyuri_boss_pattern_angle;
	bullets_add_group(
		state, BulletFlag::pellet,
		exact::wrap_add(boss.center_x, static_cast<std::int16_t>(56 << 4)),
		boss.center_y, target_player, boss.params[0], angle, bullet_group_one
	);
	angle = static_cast<std::uint8_t>(angle + 0x40U);
	bullets_add_group(
		state, BulletFlag::pellet, boss.center_x,
		exact::wrap_add(boss.center_y, static_cast<std::int16_t>(56 << 4)),
		target_player, boss.params[0], angle, bullet_group_one
	);
	angle = static_cast<std::uint8_t>(angle + 0x40U);
	bullets_add_group(
		state, BulletFlag::pellet,
		exact::wrap_add(boss.center_x, static_cast<std::int16_t>(-(56 << 4))),
		boss.center_y, target_player, boss.params[0], angle, bullet_group_one
	);
	angle = static_cast<std::uint8_t>(angle + 0x40U);
	bullets_add_group(
		state, BulletFlag::pellet, boss.center_x,
		exact::wrap_add(boss.center_y, static_cast<std::int16_t>(-(56 << 4))),
		target_player, boss.params[0], angle, bullet_group_one
	);
}

void boss_pattern_chiyuri_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if((boss.pattern_frame % boss.params[3]) != 1U) {
		return;
	}
	boss_chiyuri_random_effect(state);
	if(boss.pattern_frame >= 0x80U) {
		// As above, the final scheduled ring is emitted after the reset.
		boss_chiyuri_finish_pattern(boss);
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto angle = static_cast<std::uint8_t>(randring_next16(state));
	bullets_add_group(
		state, BulletFlag::bullet16, boss.center_x, boss.center_y,
		target_player, boss.params[2], angle, bullet_group_thirty_two_ring
	);
	if(state.gba_boss_level < 8U) {
		return;
	}
	const auto speed = static_cast<std::uint8_t>(boss.params[2] / 4U);
	bullets_add_group(
		state, BulletFlag::pellet,
		exact::wrap_add(boss.center_x, static_cast<std::int16_t>(56 << 4)),
		boss.center_y, target_player, speed, angle, bullet_group_eight_ring
	);
	bullets_add_group(
		state, BulletFlag::pellet,
		exact::wrap_add(boss.center_x, static_cast<std::int16_t>(-(56 << 4))),
		boss.center_y, target_player, speed, angle, bullet_group_eight_ring
	);
	bullets_add_group(
		state, BulletFlag::pellet, boss.center_x,
		exact::wrap_add(boss.center_y, static_cast<std::int16_t>(56 << 4)),
		target_player, speed, angle, bullet_group_eight_ring
	);
	bullets_add_group(
		state, BulletFlag::pellet, boss.center_x,
		exact::wrap_add(boss.center_y, static_cast<std::int16_t>(-(56 << 4))),
		target_player, speed, angle, bullet_group_eight_ring
	);
}

void boss_pattern_chiyuri_trail_ring(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if((boss.pattern_frame & 7U) == 0U) {
		boss_chiyuri_random_effect(state);
	}
	if(boss.pattern_frame != 0x30U) {
		return;
	}
	const auto angle = static_cast<std::uint8_t>(randring_next16(state));
	bullets_add_group(
		state, BulletFlag::bullet16, boss.center_x, boss.center_y,
		static_cast<std::uint8_t>(1U - player_id), boss.params[4], angle,
		bullet_group_ring, 48U, 0U, true
	);
	boss_chiyuri_finish_pattern(boss);
}

void boss_pattern_chiyuri_lattices(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto count = boss.params[6];
	if(boss.pattern_frame == 0x20U) {
		boss_chiyuri_random_effect(state);
		boss_chiyuri_random_effect(state);
		auto speed = boss.params[5];
		for(std::uint8_t pass = 0U; pass < 2U; ++pass) {
			for(std::uint8_t i = 0U; i < count; ++i) {
				const auto offset = static_cast<std::int16_t>(
					((static_cast<unsigned>(i) * 0x70U) / count) << 4U
				);
				const auto center_x = exact::wrap_add(
					exact::wrap_add(
						boss.center_x, static_cast<std::int16_t>(-(56 << 4))
					),
					offset
				);
				const auto angle = static_cast<std::uint8_t>(
					0x60U - ((static_cast<unsigned>(i) << 6U) / count)
				);
				bullets_add_group(
					state, BulletFlag::pellet, center_x, boss.center_y,
					target_player, speed, angle, bullet_group_one
				);
				bullets_add_group(
					state, BulletFlag::pellet, center_x, boss.center_y,
					target_player, speed,
					static_cast<std::uint8_t>(0U - angle), bullet_group_one
				);
				speed = (pass == 0U)
					? static_cast<std::uint8_t>(speed + 2U)
					: static_cast<std::uint8_t>(speed - 2U);
			}
		}
		return;
	}
	if(boss.pattern_frame != 0x40U) {
		return;
	}
	boss_chiyuri_random_effect(state);
	boss_chiyuri_random_effect(state);
	auto speed = boss.params[5];
	for(std::uint8_t pass = 0U; pass < 2U; ++pass) {
		for(std::uint8_t i = 0U; i < count; ++i) {
			const auto offset = static_cast<std::int16_t>(
				((static_cast<unsigned>(i) * 0x70U) / count) << 4U
			);
			const auto center_y = exact::wrap_add(
				exact::wrap_add(
					boss.center_y, static_cast<std::int16_t>(-(56 << 4))
				),
				offset
			);
			const auto angle = static_cast<std::uint8_t>(
				static_cast<int>((static_cast<unsigned>(i) << 6U) / count) -
				0x20
			);
			bullets_add_group(
				state, BulletFlag::pellet, boss.center_x, center_y,
				target_player, speed, angle,
				bullet_group_two_spread_horizontally_symmetric
			);
			speed = (pass == 0U)
				? static_cast<std::uint8_t>(speed + 2U)
				: static_cast<std::uint8_t>(speed - 2U);
		}
	}
	boss_chiyuri_finish_pattern(boss);
}

void boss_update_chiyuri(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x30U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			6U - (state.gba_boss_level / 4U)
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x34U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			0x20U - state.gba_boss_level
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 2U) + 0x38U
		);
		state.boss.params[5] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x10U
		);
		state.boss.params[6] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x0CU
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}

	boss_target_update(state);
	auto &boss = state.boss;
	boss.pattern_frame++;
	const auto dispatch_state = boss.state;
	switch(dispatch_state) {
	case 0U:
		if(boss.pattern_frame == 0x60U) {
			boss.pattern_frame = 0U;
			boss.state = 1U;
			boss.pattern_angle = 0x10U;
			boss.render_state = 0x10U;
		} else if(boss.pattern_frame != 0x50U) {
			// State 0 skips the common tail except for both effect frames.
			return;
		}
		break;

	case 1U:
		if(boss.render_state == 0x10U) {
			constexpr std::array<std::int16_t, 5> target_x{
				48 << 4, 96 << 4, 144 << 4, 192 << 4, 240 << 4,
			};
			constexpr std::array<std::int16_t, 5> target_y{
				112 << 4, 96 << 4, 80 << 4, 96 << 4, 112 << 4,
			};
			const auto point = static_cast<std::size_t>(
				randring_next16(state) % target_x.size()
			);
			boss.center_x = target_x[point];
			boss.center_y = target_y[point];
			boss.pattern_angle = 0x10U;
		}
		if(boss.pattern_frame >= 0x30U) {
			boss.pattern_frame = 0U;
			boss.state = static_cast<std::uint8_t>(
				(randring_next16(state) & 0x0FU) + 2U
			);
			boss.pattern_index++;
			if(boss.pattern_index > boss.pattern_count) {
				boss.state = 0x80U;
			}
		}
		break;

	case 2U: case 3U: case 4U: case 5U:
		boss_pattern_chiyuri_cardinals(state, player_id);
		break;
	case 6U: case 7U: case 8U: case 9U:
		boss_pattern_chiyuri_rings(state, player_id);
		break;
	case 10U: case 11U: case 12U: case 13U:
		boss_pattern_chiyuri_trail_ring(state, player_id);
		break;
	case 14U: case 15U: case 16U: case 17U:
		boss_pattern_chiyuri_lattices(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}

	boss.render_frame = static_cast<std::uint8_t>(
		boss.render_frame + ((state.round_or_result_frame & 3U) == 0U)
	);
	boss.render_frame &= 3U;
	if(boss.pattern_angle != 0U) {
		boss.pattern_angle--;
	}
	if(boss.render_state != 0U) {
		boss.render_state--;
	}

	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, boss.center_x, boss.center_y, 32, 24
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	boss.hit = damage;
	boss.hp = exact::wrap_subtract(
		boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_yumemi_random_effect(DeterministicState &state) noexcept
{
	// SUB_CE5B() is a presentation effect with one shared-ring draw.
	(void)randring_next16(state);
}

BulletFlag boss_yumemi_random_bullet_type(DeterministicState &state) noexcept
{
	return ((randring_next16(state) & 1U) == 0U)
		? BulletFlag::pellet
		: BulletFlag::bullet16;
}

void boss_pattern_yumemi_accel_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		state.yumemi_boss_angle = static_cast<std::uint8_t>(
			randring_next16(state)
		);
		state.yumemi_boss_speed_offset = 0U;
		state.yumemi_boss_radius = 256U;
		state.yumemi_boss_angle_delta =
			((randring_next16(state) & 1U) == 0U) ? 3 : -3;
	}
	if(boss.pattern_frame < 4U) {
		boss_yumemi_random_effect(state);
		return;
	}
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	if(boss.pattern_frame == 0x20U) {
		boss.render_state = 2U;
	}
	if((static_cast<std::uint8_t>(boss.pattern_frame) & 3U) == 0U) {
		bullets_add_group(
			state, BulletFlag::bullet16,
			state.yumemi_boss_muzzle_x, state.yumemi_boss_muzzle_y,
			static_cast<std::uint8_t>(1U - player_id),
			static_cast<std::uint8_t>(
				boss.params[0] + state.yumemi_boss_speed_offset
			),
			state.yumemi_boss_angle, bullet_group_ring, boss.params[1]
		);
		state.yumemi_boss_angle = static_cast<std::uint8_t>(
			state.yumemi_boss_angle + state.yumemi_boss_angle_delta
		);
		state.yumemi_boss_speed_offset = static_cast<std::uint8_t>(
			state.yumemi_boss_speed_offset + 8U
		);
	}
	if(boss.pattern_frame > 0x3CU) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_yumemi_random_groups(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		state.yumemi_boss_target_x =
			state.players[target_player].center_x.raw();
		state.yumemi_boss_target_y =
			state.players[target_player].center_y.raw();
		state.yumemi_boss_angle = iatan2(
			exact::wrap_subtract(
				state.yumemi_boss_target_y, state.yumemi_boss_muzzle_y
			),
			exact::wrap_subtract(
				state.yumemi_boss_target_x, state.yumemi_boss_muzzle_x
			)
		);
		state.yumemi_boss_radius = 256U;
		boss.render_frame = 0x20U;
	}
	if(boss.pattern_frame < 0x10U) {
		if((boss.pattern_frame & 3U) == 1U) {
			boss_yumemi_random_effect(state);
		}
		return;
	}
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	if(boss.pattern_frame == 0x20U) {
		boss.render_state = 2U;
	}
	if((static_cast<std::uint8_t>(boss.pattern_frame) & 1U) == 0U) {
		constexpr std::array<std::uint8_t, 5> groups{
			bullet_group_three_spread_narrow,
			bullet_group_four_spread_narrow,
			bullet_group_two_spread_narrow,
			bullet_group_five_spread_narrow,
			bullet_group_one,
		};
		const auto speed = static_cast<std::uint8_t>(
			(randring_next16(state) & 0x1FU) + boss.params[2]
		);
		const auto type = boss_yumemi_random_bullet_type(state);
		const auto group = groups[
			static_cast<std::size_t>(randring_next16(state) % groups.size())
		];
		bullets_add_group(
			state, type,
			state.yumemi_boss_muzzle_x, state.yumemi_boss_muzzle_y,
			static_cast<std::uint8_t>(1U - player_id), speed,
			state.yumemi_boss_angle, group
		);
	}
	if(boss.pattern_frame > 0x60U) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_yumemi_four_rings(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame % boss.params[3]) == 0U) {
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		const auto type = boss_yumemi_random_bullet_type(state);
		constexpr std::array<std::int16_t, 4> offset_x{
			0, 0, -(32 << 4), 32 << 4,
		};
		constexpr std::array<std::int16_t, 4> offset_y{
			-(32 << 4), 32 << 4, 0, 0,
		};
		for(std::size_t i = 0U; i < offset_x.size(); ++i) {
			bullets_add_group(
				state, type,
				exact::wrap_add(boss.center_x, offset_x[i]),
				exact::wrap_add(boss.center_y, offset_y[i]),
				target_player, 2U << 4U,
				static_cast<std::uint8_t>(randring_next16(state)),
				bullet_group_four_ring
			);
		}
	}
	if(boss.pattern_frame >= 0x60U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_pattern_yumemi_crosses(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_angle == 0U) {
		if(boss.center_x > (56 << 4) && boss.center_x < (232 << 4)) {
			boss_move(boss);
			return;
		}
		boss.pattern_angle = 1U;
		boss.pattern_frame = 0U;
		return;
	}

	boss.render_frame = 2U;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	state.yumemi_boss_target_x = state.players[target_player].center_x.raw();
	state.yumemi_boss_target_y = state.players[target_player].center_y.raw();
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	boss_move(boss);
	boss_move(boss);
	if((boss.pattern_frame & 0x0FU) == 0x0FU) {
		boss.render_state = 1U;
		boss_yumemi_random_effect(state);
		extra_attack_add_direct_yumemi(
			state, player_id,
			state.yumemi_boss_target_x, state.yumemi_boss_target_y
		);
		return;
	}
	if(static_cast<std::uint16_t>(boss.params[4]) < boss.pattern_frame) {
		boss.render_state = 0U;
		boss.state = 1U;
		boss.pattern_frame = 0U;
		boss.pattern_angle = 0U;
	}
}

void boss_pattern_yumemi_rain(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame % boss.params[5]) == 0U) {
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		if(boss.pattern_frame < 0x40U) {
			bullets_add_group(
				state, BulletFlag::bullet16,
				state.yumemi_boss_muzzle_x, state.yumemi_boss_muzzle_y,
				target_player, 6U << 4U, static_cast<std::uint8_t>(-0x40),
				bullet_group_five_spread_wide
			);
			bullets_add_group(
				state, BulletFlag::bullet16,
				state.yumemi_boss_muzzle_x, state.yumemi_boss_muzzle_y,
				target_player, 6U << 4U, static_cast<std::uint8_t>(-0x40),
				bullet_group_four_spread_wide
			);
		}
		if(boss.pattern_frame >= 0x20U) {
			for(std::size_t i = 0U; i < 12U; ++i) {
				const auto center_x = static_cast<std::int16_t>(
					randring_next16(state) % (288U << 4U)
				);
				const auto angle = static_cast<std::uint8_t>(
					(randring_next16(state) & 0x3FU) + 0x20U
				);
				bullets_add_group(
					state, BulletFlag::bullet16, center_x, 0,
					target_player, (2U << 4U) + 4U, angle,
					bullet_group_one
				);
			}
		}
	}
	if(boss.pattern_frame >= 0x60U) {
		boss.state = 1U;
		boss.pattern_frame = 0U;
	}
}

void boss_update_yumemi(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	if(boss_launch_or_reverse(state, player_id)) {
		state.boss.params[0] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[1] = static_cast<std::uint8_t>(
			(state.gba_boss_level / 2U) + 0x10U
		);
		state.boss.params[2] = static_cast<std::uint8_t>(
			state.gba_boss_level + 0x40U
		);
		state.boss.params[3] = static_cast<std::uint8_t>(
			0x12U - state.gba_boss_level
		);
		state.boss.params[4] = static_cast<std::uint8_t>(
			(state.gba_boss_level * 4U) + 0x50U
		);
		state.boss.params[5] = static_cast<std::uint8_t>(
			0x10U - (state.gba_boss_level / 2U)
		);
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		return;
	}

	boss_target_update(state);
	auto &boss = state.boss;
	boss.pattern_frame++;
	state.yumemi_boss_muzzle_x = exact::wrap_add(
		boss.center_x, static_cast<std::int16_t>(9 << 4)
	);
	state.yumemi_boss_muzzle_y = exact::wrap_add(
		boss.center_y, static_cast<std::int16_t>(-(24 << 4))
	);
	const auto dispatch_state = boss.state;
	switch(dispatch_state) {
	case 0U:
		if(boss.pattern_frame != 0x60U) {
			return;
		}
		boss.pattern_frame = 0U;
		boss.state = 1U;
		break;
	case 1U:
		boss_choose_pattern(state);
		break;
	case 2U: case 3U: case 4U:
		boss_pattern_yumemi_accel_rings(state, player_id);
		break;
	case 7U: case 8U: case 9U:
		boss_pattern_yumemi_random_groups(state, player_id);
		break;
	case 11U: case 12U: case 13U: case 14U:
		boss_pattern_yumemi_four_rings(state, player_id);
		break;
	case 15U: case 16U: case 17U:
		boss_pattern_yumemi_crosses(state, player_id);
		break;
	case 5U: case 6U: case 10U:
		boss_pattern_yumemi_rain(state, player_id);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}

	if(state.yumemi_boss_radius != 0U) {
		state.yumemi_boss_radius = exact::wrap_subtract(
			state.yumemi_boss_radius, std::uint16_t{8U}
		);
	}
	if(boss.render_frame != 0U) {
		boss.render_frame--;
	}

	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, boss.center_x, boss.center_y, 28, 28
	);
	const auto damage = boss_hittest_damage(state, target_player, 32 << 4);
	boss.hit = damage;
	boss.hp = exact::wrap_subtract(
		boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void bosses_update(DeterministicState &state, std::uint8_t player_id) noexcept
{
	const auto character =
		(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
	if(character == 0) {
		boss_update_reimu(state, player_id);
	} else if(character == 1) {
		boss_update_mima(state, player_id);
	} else if(character == 2) {
		boss_update_marisa(state, player_id);
	} else if(character == 3) {
		boss_update_ellen(state, player_id);
	} else if(character == 4) {
		boss_update_kotohime(state, player_id);
	} else if(character == 5) {
		boss_update_kana(state, player_id);
	} else if(character == 6) {
		boss_update_rikako(state, player_id);
	} else if(character == 7) {
		boss_update_chiyuri(state, player_id);
	} else if(character == 8) {
		boss_update_yumemi(state, player_id);
	}
}

void boss_render_phase_update(DeterministicState &state) noexcept
{
	if(state.gba_boss_launched_by < 0) {
		return;
	}
	const auto owner = static_cast<std::uint8_t>(state.gba_boss_launched_by);
	const auto character =
		(static_cast<int>(state.players[owner].paletted_character) - 1) / 2;
	if(state.boss.state == 0U) {
		// kotohime_12103() advances shared Boss state from the otherwise
		// presentational materialization renderer. Later attack patterns use
		// both values as their orbit angle and radius.
		if(
			character == 4 &&
			(
				(state.round_frame & 1U) == 0U ||
				state.boss.pattern_frame >= 0x40U
			)
		) {
			state.boss.pattern_angle = static_cast<std::uint8_t>(
				state.boss.pattern_angle + 2U
			);
			state.boss.anchor_x = static_cast<std::int16_t>(
				(200U - (state.boss.pattern_frame * 2U)) << 4U
			);
		}
		return;
	}
	if(state.boss.state != 0xFFU) {
		// mima_10053() advances this animation byte from the active Boss
		// Attack renderer. A hit returns before this block, and state 5
		// deliberately resets the frame without changing the state byte.
		if(character == 1 && state.boss.hit == 0U) {
			if(state.boss.render_state == 5U) {
				state.boss.render_frame = 0U;
			}
			if(
				state.boss.render_state == 2U ||
				state.boss.render_state == 5U
			) {
				if(state.boss.render_frame >= 0x20U) {
					state.boss.render_state = 3U;
				}
				state.boss.render_frame++;
			}
		}
		return;
	}
	if(static_cast<std::int16_t>(state.boss.pattern_frame * 8U) >= 200) {
		const auto winner = static_cast<std::uint8_t>(
			1 - state.gba_boss_launched_by
		);
		score_add(state.score[winner], 2560U);
		state.boss.state = 0U;
		state.gba_boss_launched_by = -1;
	}
}
