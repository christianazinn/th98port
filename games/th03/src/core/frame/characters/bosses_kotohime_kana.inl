// Included by frame.cpp inside its private implementation namespace.

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
	std::uint8_t player_id,
	GameplayObserver *observer
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
			observe_main_feedback(
				observer, MainFeedbackCueSite::low_burst, player_id
			);
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
	std::uint8_t player_id,
	GameplayObserver *observer
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
			observe_main_feedback(
				observer, MainFeedbackCueSite::low_burst, player_id
			);
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
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame % boss.params[5]) == 1U) {
		observe_main_feedback(
			observer, MainFeedbackCueSite::low_burst, player_id
		);
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
	std::uint8_t player_id,
	GameplayObserver *observer
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
			observe_main_feedback(
				observer, MainFeedbackCueSite::low_burst, player_id
			);
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
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	if(boss_launch_or_reverse(state, player_id, observer)) {
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
		boss_pattern_kotohime_random_rings(state, player_id, observer);
		break;
	case 7U: case 8U: case 9U:
		boss_pattern_kotohime_aimed_spreads(state, player_id, observer);
		break;
	case 12U: case 13U: case 14U:
		boss_pattern_kotohime_pellet_rings(state, player_id, observer);
		break;
	case 10U: case 11U: case 15U: case 16U: case 17U:
		boss_pattern_kotohime_bursts(state, player_id, observer);
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
	const auto damage = boss_hittest_damage(
		state, target_player, 32 << 4, observer
	);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player, observer);
}

void boss_pattern_kana_accel_ring(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		return;
	}
	if(boss.pattern_frame == 0x10U) {
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id)
		);
		boss.render_frame = 0U;
		boss.render_state = 2U;
		return;
	}
	if(boss.pattern_frame == 0x28U) {
		observe_main_feedback(
			observer, MainFeedbackCueSite::rapid_burst, player_id
		);
		observe_common_effect(
			state, observer, CommonEffectKind::expanding_circle,
			boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id)
		);
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
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		return;
	}
	if(boss.pattern_frame == 0x10U) {
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id)
		);
		boss.render_frame = 0U;
		boss.render_state = 2U;
		return;
	}
	if(boss.pattern_frame >= 0x28U && boss.pattern_frame < 0x50U) {
		if(boss.pattern_frame == 0x28U) {
			observe_main_feedback(
				observer, MainFeedbackCueSite::rapid_burst, player_id
			);
			observe_common_effect(
				state, observer, CommonEffectKind::expanding_circle,
				boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id)
			);
			bullets_add_group(
				state, BulletFlag::bullet16, boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id), 1U << 4U,
				state.bullet_template_angle,
				bullet_group_ring, boss.params[1]
			);
			return;
		}
		if(boss.pattern_frame == 0x40U) {
			observe_main_feedback(
				observer, MainFeedbackCueSite::rapid_burst, player_id
			);
			observe_common_effect(
				state, observer, CommonEffectKind::expanding_circle,
				boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id)
			);
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
			observe_main_feedback(
				observer, MainFeedbackCueSite::rapid_burst, player_id
			);
			observe_common_effect(
				state, observer, CommonEffectKind::expanding_circle,
				boss.center_x, boss.center_y,
				static_cast<std::uint8_t>(1U - player_id)
			);
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
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if((boss.pattern_frame % boss.params[4]) == 0U) {
		observe_main_feedback(
			observer, MainFeedbackCueSite::rapid_burst, player_id
		);
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
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &boss = state.boss;
	if(boss.pattern_frame == 1U) {
		boss.render_state = 1U;
		return;
	}
	if(boss.pattern_frame == 0x10U) {
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			boss.center_x, boss.center_y,
			static_cast<std::uint8_t>(1U - player_id)
		);
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
			observe_main_feedback(
				observer, MainFeedbackCueSite::rapid_burst, player_id
			);
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

void boss_update_kana(
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
		boss_pattern_kana_accel_ring(state, player_id, observer);
		break;
	case 7U: case 8U: case 9U: case 10U: case 11U: case 12U:
		boss_pattern_kana_rings(state, player_id, observer);
		break;
	case 13U: case 14U: case 15U:
		boss_pattern_kana_clouds(state, player_id, observer);
		break;
	case 5U: case 6U: case 16U: case 17U:
		boss_pattern_kana_spiral(state, player_id, observer);
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
	const auto damage = boss_hittest_damage(
		state, target_player, 32 << 4, observer
	);
	state.boss.hit = damage;
	state.boss.hp = exact::wrap_subtract(
		state.boss.hp, static_cast<std::uint16_t>(damage)
	);
	boss_defeat_if_needed(state, target_player, observer);
}
