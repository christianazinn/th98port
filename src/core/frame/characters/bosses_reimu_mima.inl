// Included by frame.cpp inside its private implementation namespace.

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

void boss_update_reimu(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	if(boss_launch_or_reverse(state, player_id, observer)) {
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
	const auto damage = boss_hittest_damage(
		state, target_player, 32 << 4, observer
	);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player, observer);
}

void boss_pattern_mima_spreads(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
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
		observe_main_feedback(
			observer, MainFeedbackCueSite::low_burst, player_id
		);
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
	std::uint8_t player_id,
	GameplayObserver *observer
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
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			center_x, center_y, static_cast<std::uint8_t>(1U - player_id)
		);
		return;
	}
	if(boss.pattern_frame <= 0x30U) {
		return;
	}
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	if((boss.pattern_frame & 1U) == 0U) {
		observe_main_feedback(
			observer, MainFeedbackCueSite::low_burst, player_id
		);
	}
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
	std::uint8_t player_id,
	GameplayObserver *observer
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
		observe_main_feedback(
			observer, MainFeedbackCueSite::rapid_burst, player_id
		);
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
	std::uint8_t player_id,
	GameplayObserver *observer
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
		observe_main_feedback(
			observer, MainFeedbackCueSite::rapid_burst, player_id
		);
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

void boss_update_mima(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	if(boss_launch_or_reverse(state, player_id, observer)) {
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
		boss_pattern_mima_spreads(state, player_id, observer);
		break;
	case 6U: case 7U: case 8U: case 9U: case 10U:
		boss_pattern_mima_random_bursts(state, player_id, observer);
		break;
	case 11U: case 12U: case 13U:
		boss_pattern_mima_aimed_trails(state, player_id, observer);
		break;
	case 14U: case 15U: case 16U: case 17U:
		boss_pattern_mima_orbit_clouds(state, player_id, observer);
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
	const auto damage = boss_hittest_damage(
		state, target_player, 32 << 4, observer
	);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player, observer);
}
