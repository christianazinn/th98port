// Included by frame.cpp inside its private implementation namespace.

void gauge_pattern_reimu(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.reimu_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x20U
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			0x20U - state.gba_gauge_level[player_id]
		);
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto frame = state.reimu_gauge_pattern_frame[player_id];
	if((frame % state.gauge_pattern_param_b[player_id]) == 0U) {
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			32 << 4, 8 << 4, target_player
		);
		bullets_add_group(
			state,
			type,
			32 << 4,
			8 << 4,
			target_player,
			2 << 4,
			0U,
			bullet_group_five_spread_medium_aimed
		);
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			256 << 4, 8 << 4, target_player
		);
		bullets_add_group(
			state,
			type,
			256 << 4,
			8 << 4,
			target_player,
			2 << 4,
			0U,
			bullet_group_five_spread_medium_aimed
		);
	}
	if((frame % 0x20U) == 0x10U) {
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			64 << 4, 8 << 4, target_player
		);
		bullets_add_group(
			state,
			type,
			64 << 4,
			8 << 4,
			target_player,
			(3 << 4) + 2,
			0U,
			bullet_group_ring,
			state.gauge_pattern_param_a[player_id]
		);
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			(288 - 64) << 4, 8 << 4, target_player
		);
		bullets_add_group(
			state,
			type,
			(288 - 64) << 4,
			8 << 4,
			target_player,
			(3 << 4) + 2,
			0U,
			bullet_group_ring,
			state.gauge_pattern_param_a[player_id]
		);
	}
	if(frame > 0x40U) {
		gauge_attack_finish(state, player_id, observer);
	}
	state.reimu_gauge_pattern_frame[player_id]++;
}

void gauge_pattern_marisa(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.marisa_gauge_pattern_y[player_id] = static_cast<std::int16_t>(
			- static_cast<std::int16_t>((randring_next16(state) & 0x1FU) << 4U)
		);
		state.marisa_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			0x30U - state.gba_gauge_level[player_id]
		);
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet
		: BulletFlag::bullet16;
	auto &frame = state.marisa_gauge_pattern_frame[player_id];
	auto &center_y = state.marisa_gauge_pattern_y[player_id];
	const auto phase = static_cast<std::uint8_t>(frame % 0x18U);
	if(phase == 8U || phase == 0x14U) {
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			(phase == 8U) ? 0 : (288 << 4), center_y,
			static_cast<std::uint8_t>(1U - player_id)
		);
	}
	if(phase == 0U || phase == 0x0CU) {
		auto speed = std::uint8_t{(3U << 4U) + 8U};
		const auto center_x = static_cast<std::int16_t>(
			(phase == 0U) ? 0 : (288 << 4)
		);
		const auto angle = static_cast<std::uint8_t>(
			(phase == 0U) ? 0x20U : 0x60U
		);
		for(std::size_t shot = 0U; shot < 5U; ++shot) {
			bullets_add_group(
				state,
				type,
				center_x,
				center_y,
				static_cast<std::uint8_t>(1U - player_id),
				speed,
				angle,
				bullet_group_one
			);
			speed = static_cast<std::uint8_t>(speed - 6U);
		}
		center_y = exact::wrap_add(
			center_y,
			static_cast<std::int16_t>(
				static_cast<std::uint16_t>(
					state.gauge_pattern_param_a[player_id]
				) << 4U
			)
		);
	}
	if(center_y >= (368 << 4)) {
		gauge_attack_finish(state, player_id, observer);
	}
	frame++;
}

void gauge_pattern_kotohime(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? BulletFlag::pellet : BulletFlag::bullet16;
		state.kotohime_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			(state.gba_gauge_level[player_id] / 2U) + 4U
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			randring_next16(state) & 1U
		);
		state.kotohime_gauge_pattern_type[player_id] = type;
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	auto &frame = state.kotohime_gauge_pattern_frame[player_id];
	if(frame == 0U) {
		auto *slot = &state.extra_attacks[player_id][extra_attack_regular_count];
		if(slot->state != 0U) {
			slot = &state.extra_attacks[player_id][extra_attack_regular_count + 1U];
		}
		const auto target_x = static_cast<std::int16_t>(
			(randring_next16(state) % (160U << 4U)) + (64U << 4U)
		);
		extra_attack_transfer_init(
			state,
			*slot,
			state.players[player_id].center_x.raw(),
			state.players[player_id].center_y.raw(),
			target_x,
			0,
			player_id,
			0x78
		);
		slot->kotohime_target_y = 0x0C00;
		slot->kotohime_velocity_y = 0x20U;
		slot->kotohime_milestone = 0U;
		slot->behavior_variant = 1U;
	} else if(frame >= 0x80U) {
		gauge_attack_finish(state, player_id, observer);
	}
	frame++;
}

void gauge_pattern_kana(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.kana_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.kana_gauge_pattern_x[player_id] = 0;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			8U - (state.gba_gauge_level[player_id] / 4U)
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x30U
		);
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto frame = state.kana_gauge_pattern_frame[player_id];
	auto &center_x = state.kana_gauge_pattern_x[player_id];
	if((frame % state.gauge_pattern_param_a[player_id]) == 0U) {
		bullets_add_group(
			state,
			type,
			center_x,
			0,
			target_player,
			state.gauge_pattern_param_b[player_id],
			0x40U,
			bullet_group_one
		);
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			center_x, 0, target_player
		);
		bullets_add_group(
			state,
			type,
			static_cast<std::int16_t>((288 << 4) - center_x),
			0,
			target_player,
			state.gauge_pattern_param_b[player_id],
			0x40U,
			bullet_group_one
		);
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			static_cast<std::int16_t>((288 << 4) - center_x), 0,
			target_player
		);
	}
	if((frame % 0x40U) < 0x20U) {
		center_x = exact::wrap_add(center_x, static_cast<std::int16_t>(0x80));
	} else {
		center_x = exact::wrap_subtract(
			center_x, static_cast<std::int16_t>(0x80)
		);
	}
	if(frame >= 0x80U) {
		gauge_attack_finish(state, player_id, observer);
	}
	state.kana_gauge_pattern_frame[player_id]++;
}

void gauge_pattern_mima(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.mima_gauge_pattern_angle_delta[player_id] =
			((randring_next16(state) & 1U) == 0U) ? -7 : 7;
		state.mima_gauge_pattern_angle[player_id] = 0x40U;
		state.mima_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x26U
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			(state.gba_gauge_level[player_id] * 2U) + 0x18U
		);
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	auto &frame = state.mima_gauge_pattern_frame[player_id];
	if((frame % 8U) == 0U) {
		const auto center_x =
			(static_cast<std::uint8_t>(
				state.mima_gauge_pattern_angle_delta[player_id]
			) > 7U)
			? static_cast<std::int16_t>(32 << 4)
			: static_cast<std::int16_t>(256 << 4);
		const auto angle = state.mima_gauge_pattern_angle[player_id];
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			center_x, 16 << 4, target_player
		);
		bullets_add_group(
			state, type, center_x, 16 << 4, target_player,
			state.gauge_pattern_param_b[player_id], angle,
			bullet_group_five_spread_narrow
		);
		if((frame % 0x10U) != 0U) {
			observe_common_effect(
				state, observer, CommonEffectKind::contracting_circle,
				center_x, 16 << 4, target_player
			);
			bullets_add_group(
				state, type, center_x, 16 << 4, target_player,
				(3 << 4) + 6, angle, bullet_group_ring,
				state.gauge_pattern_param_a[player_id]
			);
		}
		state.mima_gauge_pattern_angle[player_id] =
			static_cast<std::uint8_t>(
				angle + state.mima_gauge_pattern_angle_delta[player_id]
			);
	}
	if(frame > 0x40U) {
		gauge_attack_finish(state, player_id, observer);
	}
	frame++;
}

void gauge_pattern_ellen(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.ellen_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.ellen_gauge_pattern_x[player_id] = 144 << 4;
		state.ellen_gauge_pattern_y[player_id] = 0;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x0EU
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x1CU
		);
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	auto &frame = state.ellen_gauge_pattern_frame[player_id];
	if((frame % 8U) != 0U) {
		frame++;
		return;
	}
	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	auto &center_x = state.ellen_gauge_pattern_x[player_id];
	if(center_x > 0) {
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			center_x, 0, target_player
		);
		bullets_add_group(
			state, type, center_x, 0, target_player, (3 << 4) + 8, 0U,
			bullet_group_ring_aimed, state.gauge_pattern_param_a[player_id]
		);
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			static_cast<std::int16_t>((288 << 4) - center_x), 0,
			target_player
		);
		bullets_add_group(
			state, type, static_cast<std::int16_t>((288 << 4) - center_x),
			0, target_player, (3 << 4) + 8, 0U,
			bullet_group_ring_aimed, state.gauge_pattern_param_a[player_id]
		);
		center_x = exact::wrap_subtract(
			center_x, static_cast<std::int16_t>(24 << 4)
		);
		frame++;
		return;
	}

	auto &center_y = state.ellen_gauge_pattern_y[player_id];
	if(center_y < (144 << 4)) {
		const auto angle = static_cast<std::uint8_t>(randring_next16(state));
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			0, center_y, target_player
		);
		bullets_add_group(
			state, type, 0, center_y, target_player,
			state.gauge_pattern_param_b[player_id], angle,
			bullet_group_sixteen_ring
		);
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			288 << 4, center_y, target_player
		);
		bullets_add_group(
			state, type, 288 << 4, center_y, target_player,
			state.gauge_pattern_param_b[player_id], angle,
			bullet_group_sixteen_ring
		);
		// ZUN overwrites the template angle before the next firing, but the
		// shared-ring read and write remain part of the deterministic state.
		state.bullet_template_angle = static_cast<std::uint8_t>(
			randring_next16(state)
		);
		center_y = exact::wrap_add(
			center_y, static_cast<std::int16_t>(24 << 4)
		);
		frame++;
		return;
	}

	gauge_attack_finish(state, player_id, observer);
	frame++;
}

void gauge_pattern_rikako(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.rikako_gauge_pattern_frame[player_id] = 0U;
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			0x10U - (state.gba_gauge_level[player_id] / 2U)
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x30U
		);
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto frame = state.rikako_gauge_pattern_frame[player_id];
	if((frame % state.gauge_pattern_param_a[player_id]) == 0U) {
		const auto angle = static_cast<std::uint8_t>(0x20U - (frame / 4U));
		const auto speed = state.gauge_pattern_param_b[player_id];
		bullets_add_group(
			state, type, 0, 0, target_player, speed, angle,
			bullet_group_one_aimed
		);
		bullets_add_group(
			state, type, 288 << 4, 0, target_player, speed, angle,
			bullet_group_one_aimed
		);
		const auto mirrored_angle = static_cast<std::uint8_t>(-angle);
		bullets_add_group(
			state, type, 288 << 4, 0, target_player, speed,
			mirrored_angle, bullet_group_one_aimed
		);
		bullets_add_group(
			state, type, 0, 0, target_player, speed, mirrored_angle,
			bullet_group_one_aimed
		);
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			0, 0, target_player
		);
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			288 << 4, 0, target_player
		);
	}
	if(frame >= 0x80U) {
		gauge_attack_finish(state, player_id, observer);
	}
	state.rikako_gauge_pattern_frame[player_id]++;
}

void gauge_pattern_chiyuri(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.chiyuri_gauge_pattern_frame[player_id] = 0U;
		state.chiyuri_gauge_pattern_x[player_id] = -(24 << 4);
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			((state.gba_gauge_level[player_id] / 2U) << 4U) + 0x20U
		);
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	auto &frame = state.chiyuri_gauge_pattern_frame[player_id];
	auto &center_x = state.chiyuri_gauge_pattern_x[player_id];
	if((frame % 0x10U) == 0U) {
		center_x = exact::wrap_add(
			center_x, static_cast<std::int16_t>(24 << 4)
		);
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			center_x, 0, target_player
		);
		observe_common_effect(
			state, observer, CommonEffectKind::contracting_circle,
			static_cast<std::int16_t>((288 << 4) - center_x), 0,
			target_player
		);
	}
	if((frame & 1U) != 0U) {
		const auto speed = static_cast<std::uint8_t>(
			((frame % 0x10U) << 2U) + ((1U << 4U) + 8U)
		);
		bullets_add_group(
			state, type, center_x, 0, target_player, speed, 0U,
			bullet_group_one_aimed
		);
		bullets_add_group(
			state, type,
			static_cast<std::int16_t>((288 << 4) - center_x),
			0, target_player, speed, 0U, bullet_group_one_aimed
		);
	}
	frame++;
	if(frame >= state.gauge_pattern_param_a[player_id]) {
		gauge_attack_finish(state, player_id, observer);
	}
}

void gauge_pattern_yumemi(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &flag = state.gba_flag_active[player_id];
	if(
		flag == GaugeBossAttackFlag::gauge_pellet_init ||
		flag == GaugeBossAttackFlag::gauge_bullet_init
	) {
		state.yumemi_gauge_pattern_frame[player_id] = 0U;
		state.gauge_pattern_param_a[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x18U
		);
		state.gauge_pattern_param_b[player_id] = static_cast<std::uint8_t>(
			state.gba_gauge_level[player_id] + 0x20U
		);
		flag = (flag == GaugeBossAttackFlag::gauge_pellet_init)
			? GaugeBossAttackFlag::gauge_pellet_active
			: GaugeBossAttackFlag::gauge_bullet_active;
		return;
	}
	if(
		flag != GaugeBossAttackFlag::gauge_pellet_active &&
		flag != GaugeBossAttackFlag::gauge_bullet_active
	) {
		return;
	}

	const auto type = (flag == GaugeBossAttackFlag::gauge_pellet_active)
		? BulletFlag::pellet : BulletFlag::bullet16;
	auto &frame = state.yumemi_gauge_pattern_frame[player_id];
	if((frame % 8U) == 0U) {
		std::int16_t fire_x = 0;
		std::int16_t fire_y = 0;
		bool fire = false;
		switch(frame) {
		case 0U:
		case 8U:
			break;
		case 0x10U:
			fire_x = 144 << 4;
			fire_y = 32 << 4;
			fire = true;
			break;
		case 0x18U:
			fire_x = 96 << 4;
			fire_y = 80 << 4;
			fire = true;
			break;
		case 0x20U:
			fire_x = 192 << 4;
			fire_y = 80 << 4;
			fire = true;
			break;
		case 0x28U:
			fire_x = 96 << 4;
			fire_y = 56 << 4;
			fire = true;
			break;
		case 0x30U:
			fire_x = 192 << 4;
			fire_y = 56 << 4;
			fire = true;
			break;
		case 0x38U:
			fire_x = 144 << 4;
			fire_y = 104 << 4;
			fire = true;
			gauge_attack_finish(state, player_id, observer);
			break;
		default:
			break;
		}
		if(frame <= 0x28U) {
			constexpr std::array<std::int16_t, 6> effect_x{
				144 << 4, 96 << 4, 192 << 4,
				96 << 4, 192 << 4, 144 << 4,
			};
			constexpr std::array<std::int16_t, 6> effect_y{
				32 << 4, 80 << 4, 80 << 4,
				56 << 4, 56 << 4, 104 << 4,
			};
			const auto effect = static_cast<std::size_t>(frame / 8U);
			observe_common_square(
				state, observer, effect_x[effect], effect_y[effect],
				static_cast<std::uint8_t>(1U - player_id)
			);
		}
		if(fire) {
			bullets_add_group(
				state, type, fire_x, fire_y,
				static_cast<std::uint8_t>(1U - player_id),
				state.gauge_pattern_param_b[player_id], 0U,
				bullet_group_ring_aimed,
				state.gauge_pattern_param_a[player_id]
			);
		}
	}
	frame++;
}

void gauge_patterns_update(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	const auto character = character_index_from_paletted(
		state.players[player_id].paletted_character
	);
	if(character == character_reimu) {
		gauge_pattern_reimu(state, player_id, observer);
	} else if(character == character_mima) {
		gauge_pattern_mima(state, player_id, observer);
	} else if(character == character_marisa) {
		gauge_pattern_marisa(state, player_id, observer);
	} else if(character == character_ellen) {
		gauge_pattern_ellen(state, player_id, observer);
	} else if(character == character_kotohime) {
		gauge_pattern_kotohime(state, player_id, observer);
	} else if(character == character_kana) {
		gauge_pattern_kana(state, player_id, observer);
	} else if(character == character_rikako) {
		gauge_pattern_rikako(state, player_id, observer);
	} else if(character == character_chiyuri) {
		gauge_pattern_chiyuri(state, player_id, observer);
	} else if(character == character_yumemi) {
		gauge_pattern_yumemi(state, player_id, observer);
	}
}
