// Included by frame.cpp inside its private implementation namespace.

void boss_pattern_rikako_spreads(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &boss = state.boss;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	if(boss.pattern_frame == 0x10U) {
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			boss.center_x, boss.center_y, target_player
		);
	}
	if(boss.pattern_frame == 0x28U) {
		observe_common_effect(
			state, observer, CommonEffectKind::expanding_circle,
			boss.center_x, boss.center_y, target_player
		);
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
				target_player,
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
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &boss = state.boss;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	if(boss.pattern_frame == 0x10U) {
		observe_common_square(
			state, observer, boss.center_x, boss.center_y, target_player
		);
	}
	if(boss.pattern_frame == 0x28U) {
		observe_common_effect(
			state, observer, CommonEffectKind::expanding_circle,
			boss.center_x, boss.center_y, target_player
		);
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
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	if(boss_launch_or_reverse(state, player_id, observer)) {
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
	const auto pattern_frame_at_dispatch = state.boss.pattern_frame;
	const auto render_state_at_dispatch = state.boss.render_state;
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
		boss_pattern_rikako_spreads(state, player_id, observer);
		break;
	case 7U: case 8U: case 9U: case 10U: case 11U: case 12U:
		boss_pattern_rikako_exatt_and_rings(state, player_id, observer);
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
	observe_boss_animation_step(
		observer,
		player_id,
		dispatch_state,
		pattern_frame_at_dispatch,
		render_state_at_dispatch
	);
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	collmap_set_rect_striped(
		state, target_player, state.boss.center_x, state.boss.center_y, 32, 24
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

void boss_chiyuri_random_effect(
	DeterministicState &state,
	GameplayObserver *observer,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id
) noexcept
{
	observe_common_square(
		state, observer, center_x, center_y, player_id
	);
}

void boss_chiyuri_finish_pattern(BossState &boss) noexcept
{
	boss.state = 1U;
	boss.pattern_frame = 0U;
	boss.render_state = 0x20U;
}

void boss_pattern_chiyuri_cardinals(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &boss = state.boss;
	if((boss.pattern_frame % boss.params[1]) != 1U) {
		return;
	}
	if(boss.pattern_frame == 1U) {
		state.chiyuri_boss_pattern_angle = 224U;
		state.chiyuri_boss_pattern_direction = 0U;
		const auto target_player = static_cast<std::uint8_t>(1U - player_id);
		boss_chiyuri_random_effect(
			state, observer,
			exact::wrap_subtract(boss.center_x, std::int16_t{56 << 4}),
			boss.center_y, target_player
		);
		boss_chiyuri_random_effect(
			state, observer,
			exact::wrap_add(boss.center_x, std::int16_t{56 << 4}),
			boss.center_y, target_player
		);
		boss_chiyuri_random_effect(
			state, observer, boss.center_x,
			exact::wrap_subtract(boss.center_y, std::int16_t{56 << 4}),
			target_player
		);
		boss_chiyuri_random_effect(
			state, observer, boss.center_x,
			exact::wrap_add(boss.center_y, std::int16_t{56 << 4}),
			target_player
		);
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
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &boss = state.boss;
	if((boss.pattern_frame % boss.params[3]) != 1U) {
		return;
	}
	boss_chiyuri_random_effect(
		state, observer, boss.center_x, boss.center_y,
		static_cast<std::uint8_t>(1U - player_id)
	);
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
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &boss = state.boss;
	if((boss.pattern_frame & 7U) == 0U) {
		boss_chiyuri_random_effect(
			state, observer, boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id)
		);
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
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &boss = state.boss;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto count = boss.params[6];
	if(boss.pattern_frame == 0x20U) {
		boss_chiyuri_random_effect(
			state, observer,
			exact::wrap_subtract(boss.center_x, std::int16_t{56 << 4}),
			boss.center_y, target_player
		);
		boss_chiyuri_random_effect(
			state, observer,
			exact::wrap_add(boss.center_x, std::int16_t{56 << 4}),
			boss.center_y, target_player
		);
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
	boss_chiyuri_random_effect(
		state, observer, boss.center_x,
		exact::wrap_subtract(boss.center_y, std::int16_t{56 << 4}),
		target_player
	);
	boss_chiyuri_random_effect(
		state, observer, boss.center_x,
		exact::wrap_add(boss.center_y, std::int16_t{56 << 4}),
		target_player
	);
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
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	if(boss_launch_or_reverse(state, player_id, observer)) {
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
	const auto pattern_frame_at_dispatch = boss.pattern_frame;
	const auto render_state_at_dispatch = boss.render_state;
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
		if(boss.pattern_frame == 0x50U) {
			const auto target_player = static_cast<std::uint8_t>(1U - player_id);
			observe_common_effect(
				state, observer, CommonEffectKind::contracting_circle,
				exact::wrap_subtract(boss.center_x, std::int16_t{56 << 4}),
				boss.center_y, target_player
			);
			observe_common_effect(
				state, observer, CommonEffectKind::contracting_circle,
				exact::wrap_add(boss.center_x, std::int16_t{56 << 4}),
				boss.center_y, target_player
			);
			observe_common_effect(
				state, observer, CommonEffectKind::contracting_circle,
				boss.center_x,
				exact::wrap_subtract(boss.center_y, std::int16_t{56 << 4}),
				target_player
			);
			observe_common_effect(
				state, observer, CommonEffectKind::contracting_circle,
				boss.center_x,
				exact::wrap_add(boss.center_y, std::int16_t{56 << 4}),
				target_player
			);
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
		boss_pattern_chiyuri_cardinals(state, player_id, observer);
		break;
	case 6U: case 7U: case 8U: case 9U:
		boss_pattern_chiyuri_rings(state, player_id, observer);
		break;
	case 10U: case 11U: case 12U: case 13U:
		boss_pattern_chiyuri_trail_ring(state, player_id, observer);
		break;
	case 14U: case 15U: case 16U: case 17U:
		boss_pattern_chiyuri_lattices(state, player_id, observer);
		break;
	case 0x80U:
		boss_exit_move(state, player_id);
		observe_boss_palette_baseline_restored(observer, 1U - player_id);
		break;
	case 0xFFU:
		return;
	default:
		break;
	}
	observe_boss_animation_step(
		observer,
		player_id,
		dispatch_state,
		pattern_frame_at_dispatch,
		render_state_at_dispatch
	);

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
	const auto damage = boss_hittest_damage(
		state, target_player, 32 << 4, observer
	);
	boss.hit = damage;
	boss.hp = exact::wrap_subtract(
		boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player, observer);
}

void boss_yumemi_random_effect(
	DeterministicState &state,
	GameplayObserver *observer,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id
) noexcept
{
	observe_common_square(
		state, observer, center_x, center_y, player_id
	);
}

BulletFlag boss_yumemi_random_bullet_type(DeterministicState &state) noexcept
{
	return ((randring_next16(state) & 1U) == 0U)
		? BulletFlag::pellet
		: BulletFlag::bullet16;
}
