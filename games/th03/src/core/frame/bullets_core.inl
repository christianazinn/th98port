// Included by frame.cpp inside its private implementation namespace.

void score_add(
	std::array<std::uint8_t, score_digits> &score,
	std::uint16_t delta
) noexcept
{
	std::uint16_t carry = delta;
	for(std::size_t index = 0U; index < (score_digits - 1U); ++index) {
		auto &digit = score[index];
		const auto sum = static_cast<std::uint16_t>(digit + (carry % 10U));
		digit = static_cast<std::uint8_t>(sum % 10U);
		carry = static_cast<std::uint16_t>((carry / 10U) + (sum / 10U));
	}
	// score_add.asm intentionally skips AAA for the most-significant digit.
	// Preserve its raw byte addition, including values above 9 and byte wrap.
	score.back() = exact::wrap_add(
		score.back(), static_cast<std::uint8_t>(carry)
	);
}

void gauge_avail_add(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t charge
) noexcept
{
	auto &player = state.players[player_id];
	auto &gauge = player.gauge_avail;
	if(player.hyper_active != 0U || gauge >= gauge_max) {
		return;
	}
	gauge = std::min<std::uint16_t>(
		gauge_max, static_cast<std::uint16_t>(gauge + charge)
	);
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
	// Anniversary caps only the active display field. Replay Patch 0.2.10
	// removes this cap, but both branches refresh time below first.
	const auto display_hits = state.three_digit_combos ?
		hits : std::min<std::uint8_t>(hits, 99U);
	if(hits > 99U && combo.bonus_total != 0xFFFFU) {
		combo.time = 80U;
	}
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

void fire_point_based_boss_attack_or_panic(
	DeterministicState &state,
	std::uint16_t points,
	std::uint8_t player_id,
	std::uint8_t chain_slot
) noexcept
{
	if(points < state.combo_points_for_boss_attack) {
		state.boss_panic_fired_in_combo[player_id] = 0U;
		return;
	}
	if(state.warning_flag[player_id] != WarningFlag::none) {
		return;
	}
	if(state.gba_boss_launched_by != static_cast<std::int8_t>(player_id)) {
		if(state.gba_flag_active[player_id] == GaugeBossAttackFlag::boss) {
			return;
		}
		state.gba_flag_next[player_id] = GaugeBossAttackFlag::boss;
		state.warning_flag[player_id] = WarningFlag::portrait;
		state.chains.pellet_and_fireball_value[player_id][chain_slot] = 0U;
		state.chains.charge_fireball[player_id][chain_slot] = 0U;
		state.chains.charge_exatt[player_id][chain_slot] = 0U;
		state.combo_points_for_boss_attack = static_cast<std::uint16_t>(
			state.combo_points_for_boss_attack + 5120U
		);
		if(state.gba_boss_level < 16U) {
			state.gba_boss_level++;
		}
		if(state.gba_boss_launched_by < 0) {
			state.players[player_id].boss_attacks_fired++;
		} else {
			state.players[player_id].boss_attacks_reversed++;
		}
		return;
	}
	if(
		points < 30000U ||
		state.boss_panic_fired_in_combo[player_id] != 0U ||
		state.gba_flag_active[player_id] != GaugeBossAttackFlag::none
	) {
		return;
	}
	state.gba_flag_next[player_id] = GaugeBossAttackFlag::gauge_bullet_init;
	state.warning_flag[player_id] = WarningFlag::portrait;
	state.boss_panic_fired_in_combo[player_id] = 1U;
	state.gba_gauge_level[player_id] = std::min<std::uint8_t>(
		16U, static_cast<std::uint8_t>(state.gba_gauge_level[player_id] + 3U)
	);
	state.players[player_id].boss_panics_fired++;
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

std::uint8_t bullet_speed_adjusted(
	const DeterministicState &state,
	std::uint8_t speed
) noexcept
{
	const auto adjusted = static_cast<std::uint8_t>(
		speed + state.bullet_base_speed
	);
	if(adjusted > (8U << 4U)) {
		return 8U << 4U;
	}
	if(adjusted < (1U << 4U)) {
		return 1U << 4U;
	}
	return adjusted;
}

BulletState *bullet_free_slot(DeterministicState &state) noexcept
{
	const auto slot = std::find_if(
		state.bullets.begin(), state.bullets.end(),
		[](const BulletState &bullet) { return bullet.flag == BulletFlag::free; }
	);
	return (slot == state.bullets.end()) ? nullptr : &*slot;
}

bool bullet_center_is_spawnable(
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	return (
		center_y > bullet_center_min && center_y < bullet_center_y_max &&
		center_x > bullet_center_min && center_x < bullet_center_x_max
	);
}

void bullets_add_group(
	DeterministicState &state,
	BulletFlag type,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id,
	std::uint8_t speed,
	std::uint8_t angle,
	std::uint8_t group,
	std::uint8_t count = 0U,
	std::uint8_t accel_type = 0U,
	bool has_trail = false
) noexcept
{
	state.bullet_template_angle = angle;
	if(!bullet_center_is_spawnable(center_x, center_y)) {
		return;
	}
	const auto adjusted_speed = bullet_speed_adjusted(state, speed);
	for(std::uint16_t group_index = 0U;; ++group_index) {
		auto *bullet = bullet_free_slot(state);
		if(bullet == nullptr) {
			return;
		}

		std::int16_t angle_offset = 0;
		std::int16_t actual_speed = adjusted_speed;
		bool aimed = false;
		bool done = false;
		switch(group) {
		case bullet_group_one:
			done = true;
			break;

		case bullet_group_one_aimed:
			aimed = true;
			done = true;
			break;

		case bullet_group_two_spread_narrow:
		case bullet_group_two_spread_medium:
		case bullet_group_two_spread_wide:
		case bullet_group_two_spread_narrow_aimed:
		case bullet_group_two_spread_medium_aimed:
		case bullet_group_two_spread_wide_aimed: {
			aimed = (group >= bullet_group_two_spread_narrow_aimed);
			const auto variant = aimed
				? static_cast<std::int16_t>(
					group - bullet_group_two_spread_narrow_aimed
				)
				: static_cast<std::int16_t>(
					group - bullet_group_two_spread_narrow
				);
			const auto extra = static_cast<std::int16_t>(variant * 3);
			if(group_index == 0U) {
				angle_offset = static_cast<std::int16_t>(3 + extra);
			} else {
				angle_offset = static_cast<std::int16_t>(-3 - extra);
				done = true;
			}
			break;
		}

		case bullet_group_two_spread_horizontally_symmetric:
			if(group_index == 0U) {
				angle_offset = 0;
			} else {
				angle_offset = static_cast<std::int16_t>(
					0x80 - (static_cast<std::int16_t>(angle) * 2)
				);
				done = true;
			}
			break;

		case bullet_group_three_spread_narrow:
		case bullet_group_three_spread_medium:
		case bullet_group_three_spread_wide:
		case bullet_group_three_spread_narrow_aimed:
		case bullet_group_three_spread_medium_aimed:
		case bullet_group_three_spread_wide_aimed: {
			aimed = (group >= bullet_group_three_spread_narrow_aimed);
			const auto variant = aimed
				? static_cast<std::int16_t>(
					group - bullet_group_three_spread_narrow_aimed
				)
				: static_cast<std::int16_t>(
					group - bullet_group_three_spread_narrow
				);
			const auto extra = static_cast<std::int16_t>(variant * 8);
			if(group_index == 0U) {
				angle_offset = 0;
			} else if(group_index == 1U) {
				angle_offset = static_cast<std::int16_t>(8 + extra);
			} else {
				angle_offset = static_cast<std::int16_t>(-8 - extra);
				done = true;
			}
			break;
		}

		case bullet_group_four_spread_narrow:
		case bullet_group_four_spread_medium:
		case bullet_group_four_spread_wide:
		case bullet_group_four_spread_narrow_aimed:
		case bullet_group_four_spread_medium_aimed:
		case bullet_group_four_spread_wide_aimed: {
			aimed = (group >= bullet_group_four_spread_narrow_aimed);
			const auto variant = aimed
				? static_cast<std::int16_t>(
					group - bullet_group_four_spread_narrow_aimed
				)
				: static_cast<std::int16_t>(
					group - bullet_group_four_spread_narrow
				);
			const auto extra = static_cast<std::int16_t>(variant * 6);
			switch(group_index) {
			case 0U:
				angle_offset = static_cast<std::int16_t>(3 + extra);
				break;
			case 1U:
				angle_offset = static_cast<std::int16_t>(-3 - extra);
				break;
			case 2U:
				angle_offset = static_cast<std::int16_t>(9 + (extra * 3));
				break;
			default:
				angle_offset = static_cast<std::int16_t>(-9 - (extra * 3));
				done = true;
				break;
			}
			break;
		}

		case bullet_group_five_spread_narrow:
		case bullet_group_five_spread_medium:
		case bullet_group_five_spread_wide:
		case bullet_group_five_spread_narrow_aimed:
		case bullet_group_five_spread_medium_aimed:
		case bullet_group_five_spread_wide_aimed: {
			aimed = (group >= bullet_group_five_spread_narrow_aimed);
			const auto variant = aimed
				? static_cast<std::int16_t>(
					group - bullet_group_five_spread_narrow_aimed
				)
				: static_cast<std::int16_t>(
					group - bullet_group_five_spread_narrow
				);
			const auto extra = static_cast<std::int16_t>(variant * 6);
			switch(group_index) {
			case 0U: angle_offset = 0; break;
			case 1U:
				angle_offset = static_cast<std::int16_t>(6 + extra);
				break;
			case 2U:
				angle_offset = static_cast<std::int16_t>(-6 - extra);
				break;
			case 3U:
				angle_offset = static_cast<std::int16_t>(12 + (extra * 2));
				break;
			default:
				angle_offset = static_cast<std::int16_t>(-12 - (extra * 2));
				done = true;
				break;
			}
			break;
		}

		case bullet_group_one_random_angle:
			angle_offset = static_cast<std::uint8_t>(randring_next16(state));
			done = true;
			break;

		case bullet_group_random_angle:
			angle_offset = static_cast<std::uint8_t>(randring_next16(state));
			done = (group_index >= count);
			break;

		case bullet_group_random_angle_and_speed:
			angle_offset = static_cast<std::uint8_t>(randring_next16(state));
			actual_speed = static_cast<std::int16_t>(
				actual_speed + static_cast<std::int16_t>(
					randring_next16(state) & 0x1FU
				)
			);
			done = (group_index >= count);
			break;

		case bullet_group_random_constrained_aimed:
			aimed = true;
			angle_offset = static_cast<std::int16_t>(
				(randring_next16(state) & 0x1FU) - 0x10
			);
			done = (group_index >= count);
			break;

		case bullet_group_four_ring:
			angle_offset = static_cast<std::int16_t>(group_index << 6U);
			done = (group_index >= 3U);
			break;

		case bullet_group_two_ring:
			angle_offset = static_cast<std::int16_t>(group_index << 7U);
			done = (group_index >= 1U);
			break;

		case bullet_group_eight_ring:
			angle_offset = static_cast<std::int16_t>(group_index << 5U);
			done = (group_index >= 7U);
			break;

		case bullet_group_sixteen_ring:
			angle_offset = static_cast<std::int16_t>(group_index << 4U);
			done = (group_index >= 15U);
			break;

		case bullet_group_thirty_two_ring:
			angle_offset = static_cast<std::int16_t>(group_index << 3U);
			done = (group_index >= 31U);
			break;

		case bullet_group_ring:
			assert(count != 0U);
			angle_offset = static_cast<std::int16_t>(
				exact::wrap_multiply(
					static_cast<std::int16_t>(group_index),
					static_cast<std::int16_t>(0x100)
				) / static_cast<std::int16_t>(count)
			);
			done = (group_index >= static_cast<std::uint16_t>(count - 1U));
			break;

		case bullet_group_two_ring_aimed:
			aimed = true;
			angle_offset = static_cast<std::int16_t>(group_index << 7U);
			done = (group_index >= 1U);
			break;

		case bullet_group_four_ring_aimed:
			aimed = true;
			angle_offset = static_cast<std::int16_t>(group_index << 6U);
			done = (group_index >= 3U);
			break;

		case bullet_group_eight_ring_aimed:
			aimed = true;
			angle_offset = static_cast<std::int16_t>(group_index << 5U);
			done = (group_index >= 7U);
			break;

		case bullet_group_sixteen_ring_aimed:
			aimed = true;
			angle_offset = static_cast<std::int16_t>(group_index << 4U);
			done = (group_index >= 15U);
			break;

		case bullet_group_thirty_two_ring_aimed:
			aimed = true;
			angle_offset = static_cast<std::int16_t>(group_index << 3U);
			done = (group_index >= 31U);
			break;

		case bullet_group_ring_aimed:
			assert(count != 0U);
			aimed = true;
			angle_offset = static_cast<std::int16_t>(
				exact::wrap_multiply(
					static_cast<std::int16_t>(group_index),
					static_cast<std::int16_t>(0x100)
				) / static_cast<std::int16_t>(count)
			);
			done = (group_index >= static_cast<std::uint16_t>(count - 1U));
			break;

		default:
			aimed = true;
			break;
		}

		bullet->flag = type;
		bullet->age = 0U;
		bullet->center_x = center_x;
		bullet->center_y = center_y;
		bullet->player_id = player_id;
		bullet->group_next = group;
		bullet->speed_next = adjusted_speed;
		bullet->accel_type = accel_type;
		bullet->is_collidable = true;
		bullet->has_trail = has_trail;
		if(has_trail) {
			bullet->trail_index = state.bullet_trail_cursor;
			auto &trail = state.bullet_trails[state.bullet_trail_cursor];
			trail.center_x.fill(center_x);
			trail.center_y.fill(center_y);
			state.bullet_trail_cursor++;
			if(state.bullet_trail_cursor >= state.bullet_trails.size()) {
				state.bullet_trail_cursor = 0U;
			}
		}
		bullet->angle_next = static_cast<std::uint8_t>(angle_offset + angle);
		const auto velocity = aimed
			? vector2_between_plus(
				center_x,
				center_y,
				state.players[player_id].center_x.raw(),
				state.players[player_id].center_y.raw(),
				bullet->angle_next,
				actual_speed
			)
			: vector2(
				bullet->angle_next,
				actual_speed
			);
		bullet->velocity_x = velocity.x;
		bullet->velocity_y = velocity.y;
		if(done) {
			return;
		}
	}
}

bool bullet_trail_update_and_clip(
	std::array<std::int16_t, 6> &coordinates,
	std::int16_t previous,
	std::int16_t minimum,
	std::int16_t maximum
) noexcept
{
	coordinates[5] = coordinates[4];
	if(coordinates[5] <= minimum || coordinates[5] >= maximum) {
		return false;
	}
	coordinates[4] = coordinates[3];
	coordinates[3] = coordinates[2];
	coordinates[2] = coordinates[1];
	coordinates[1] = coordinates[0];
	coordinates[0] = previous;
	return true;
}
