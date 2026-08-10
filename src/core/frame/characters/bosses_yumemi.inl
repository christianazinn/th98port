// Included by frame.cpp inside its private implementation namespace.

void boss_pattern_yumemi_accel_rings(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
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
		boss_yumemi_random_effect(
			state, observer,
			state.yumemi_boss_muzzle_x, state.yumemi_boss_muzzle_y,
			static_cast<std::uint8_t>(1U - player_id)
		);
		return;
	}
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	if(boss.pattern_frame == 0x20U) {
		observe_common_effect(
			state, observer, CommonEffectKind::expanding_circle,
			state.yumemi_boss_muzzle_x, state.yumemi_boss_muzzle_y,
			static_cast<std::uint8_t>(1U - player_id)
		);
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
	std::uint8_t player_id,
	GameplayObserver *observer
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
			boss_yumemi_random_effect(
				state, observer,
				state.yumemi_boss_target_x, state.yumemi_boss_target_y,
				static_cast<std::uint8_t>(1U - player_id)
			);
		}
		return;
	}
	if(boss.pattern_frame < 0x20U) {
		return;
	}
	if(boss.pattern_frame == 0x20U) {
		observe_common_effect(
			state, observer, CommonEffectKind::expanding_circle,
			state.yumemi_boss_muzzle_x, state.yumemi_boss_muzzle_y,
			static_cast<std::uint8_t>(1U - player_id)
		);
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
	std::uint8_t player_id,
	GameplayObserver *observer
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
		boss_yumemi_random_effect(
			state, observer,
			state.yumemi_boss_target_x, state.yumemi_boss_target_y,
			target_player
		);
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
	std::uint8_t player_id,
	GameplayObserver *observer
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
		boss_pattern_yumemi_accel_rings(state, player_id, observer);
		break;
	case 7U: case 8U: case 9U:
		boss_pattern_yumemi_random_groups(state, player_id, observer);
		break;
	case 11U: case 12U: case 13U: case 14U:
		boss_pattern_yumemi_four_rings(state, player_id);
		break;
	case 15U: case 16U: case 17U:
		boss_pattern_yumemi_crosses(state, player_id, observer);
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
	const auto damage = boss_hittest_damage(
		state, target_player, 32 << 4, observer
	);
	boss.hit = damage;
	boss.hp = exact::wrap_subtract(
		boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player);
}

void bosses_update(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
