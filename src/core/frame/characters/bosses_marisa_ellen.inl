// Included by frame.cpp inside its private implementation namespace.

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

void boss_update_marisa(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
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
	const auto damage = boss_hittest_damage(
		state, target_player, 32 << 4, observer
	);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void boss_pattern_ellen_spreads(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &boss = state.boss;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	if(
		boss.pattern_frame == 0x10U || boss.pattern_frame == 0x14U ||
		boss.pattern_frame == 0x18U
	) {
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			boss.center_x, boss.center_y, target_player
		);
	}
	if(boss.pattern_frame == 0x18U) {
		state.ellen_boss_spread_angle = 0x40U;
		return;
	}
	if(boss.pattern_frame < 0x1CU) {
		return;
	}
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
	std::uint8_t player_id,
	GameplayObserver *observer
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
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id)
		);
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

void boss_update_ellen(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
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
		boss_pattern_ellen_spreads(state, player_id, observer);
		break;
	case 6U: case 7U: case 8U: case 9U: case 10U:
		boss_pattern_ellen_orbits(state, player_id, observer);
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
	const auto damage = boss_hittest_damage(
		state, target_player, 24 << 4, observer
	);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}
