// Included by ../frame.cpp inside its private implementation namespace.

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
	// Anniversary caps only the active display field. Replay's three-digit
	// extension removes this cap, but both branches refresh time below first.
	const auto display_hits = std::min<std::uint8_t>(hits, 99U);
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

void bullet_add_kana_extra_attack(
	DeterministicState &state,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id
) noexcept
{
	// exatt_update_kana() writes 0x40 to the shared source template before
	// attempting the allocation. Preserve the write even if the arena is full
	// or the center is clipped; a later callback can inherit it.
	state.bullet_template_angle = 0x40U;
	if(!bullet_center_is_spawnable(center_x, center_y)) {
		return;
	}
	auto *bullet = bullet_free_slot(state);
	if(bullet == nullptr) {
		return;
	}
	bullet->flag = BulletFlag::bullet16;
	bullet->age = 0U;
	bullet->center_x = center_x;
	bullet->center_y = center_y;
	bullet->player_id = player_id;
	bullet->group_next = bullet_group_one;
	bullet->angle_next = 0x40U;
	bullet->speed_next = bullet_speed_adjusted(state, 1U << 4U);
	bullet->accel_type = 0x80U;
	bullet->is_collidable = true;
	bullet->has_trail = false;
	const auto velocity = vector2(
		bullet->angle_next, static_cast<std::int16_t>(bullet->speed_next)
	);
	bullet->velocity_x = velocity.x;
	bullet->velocity_y = velocity.y;
}

std::int16_t playfield_x_to_screen_subpixel(
	const DeterministicState &state,
	std::int16_t x,
	std::uint8_t player_id
) noexcept
{
	const auto pixel = exact::arithmetic_shift_right(x, 4U);
	return static_cast<std::int16_t>(
		(pixel + 16) + (static_cast<int>(player_id) * 320) +
		state.playfield_fg_shift_x[player_id]
	);
}

std::int16_t screen_x_to_playfield_subpixel(
	std::int16_t x,
	std::uint8_t player_id
) noexcept
{
	const auto pixel = static_cast<std::int16_t>(
		x - (static_cast<int>(player_id) * 320) - 16
	);
	return exact::wrap_shift_left(pixel, 4U);
}

enum class FireballVariant : std::uint8_t {
	blue = 0,
	red = 1,
};

bool fireball_add(
	DeterministicState &state,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id,
	FireballVariant variant
) noexcept
{
	const auto slot = std::find_if(
		state.fireballs.begin(), state.fireballs.end(),
		[](const FireballState &fireball) {
			return fireball.flag == FireballFlag::free;
		}
	);
	if(slot == state.fireballs.end()) {
		return false;
	}
	slot->flag = FireballFlag::transfer;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->player_id = player_id;
	const auto fall_speed = static_cast<std::uint16_t>(
		20U + (state.round_speed / 2U) + (randring_next16(state) & 31U)
	);
	slot->fall_velocity_y = static_cast<std::uint8_t>(
		std::min<std::uint16_t>(fall_speed, 8U << 4U)
	);
	const auto variant_value = static_cast<std::uint8_t>(variant);
	slot->hp = static_cast<std::uint8_t>(variant_value + 2U);
	slot->variant_as_explosion_target = static_cast<std::uint8_t>(
		2U + variant_value
	);
	slot->size_pixels = 48U;
	slot->generation = (variant == FireballVariant::blue)
		? 0U
		: static_cast<std::uint8_t>(state.fireball_generation_previous + 1U);

	const auto origin_screen = playfield_x_to_screen_subpixel(
		state, center_x, player_id
	);
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	std::int16_t target_x = 0;
	if((state.round_or_result_frame % 512U) < 256U) {
		target_x = static_cast<std::int16_t>(
			randring_next16(state) % static_cast<std::uint16_t>(288U << 4U)
		);
	} else {
		target_x = exact::wrap_add(
			state.players[target_player].center_x.raw(),
			static_cast<std::int16_t>(randring_next16(state) & 511U)
		);
	}
	slot->target_x_for_target_player = target_x;
	const auto target_screen = playfield_x_to_screen_subpixel(
		state, target_x, target_player
	);
	const auto travel_speed = static_cast<std::int16_t>(
		70U + (state.round_speed / 3U)
	);
	const auto velocity = vector2_between_plus(
		exact::wrap_shift_left(origin_screen, 4U),
		center_y,
		exact::wrap_shift_left(target_screen, 4U),
		0,
		0U,
		travel_speed
	);
	slot->velocity_x = velocity.x;
	slot->velocity_y = velocity.y;
	slot->target_x_for_origin_player = screen_x_to_playfield_subpixel(
		target_screen, player_id
	);
	return true;
}

void extra_attack_transfer_init(
	DeterministicState &state,
	ExtraAttackState &slot,
	std::int16_t center_x,
	std::int16_t center_y,
	std::int16_t target_x,
	std::int16_t target_y,
	std::uint8_t player_id,
	std::int16_t velocity_base
) noexcept
{
	slot = ExtraAttackState{};
	slot.state = 2U;
	slot.center_x = center_x;
	slot.center_y = center_y;
	slot.player_id = player_id;
	slot.target_x_for_target_player = target_x;
	const auto origin_screen = playfield_x_to_screen_subpixel(
		state, center_x, player_id
	);
	const auto target_screen = playfield_x_to_screen_subpixel(
		state, target_x, static_cast<std::uint8_t>(1U - player_id)
	);
	const auto velocity = vector2_between_plus(
		exact::wrap_shift_left(origin_screen, 4U),
		center_y,
		exact::wrap_shift_left(target_screen, 4U),
		target_y,
		0U,
		static_cast<std::int16_t>(
			velocity_base + static_cast<std::int16_t>(state.round_speed / 4U)
		)
	);
	slot.velocity_x = velocity.x;
	slot.velocity_y = velocity.y;
	slot.target_x_for_origin_player = screen_x_to_playfield_subpixel(
		target_screen, player_id
	);
}

void extra_attack_add(
	DeterministicState &state,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id
) noexcept
{
	const auto character =
		(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
	if(character == 0) {
		const auto end = state.extra_attacks[player_id].begin() +
			extra_attack_regular_count;
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(),
			end,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == end) {
			return;
		}
		const auto target_x = static_cast<std::int16_t>(
			randring_next16(state) % (288U << 4U)
		);
		const auto target_y = static_cast<std::int16_t>(
			randring_next16(state) & 2047U
		);
		extra_attack_transfer_init(
			state,
			*slot,
			center_x,
			center_y,
			target_x,
			target_y,
			player_id,
			0x5A
		);
		return;
	}
	if(character == 1) {
		const auto end = state.extra_attacks[player_id].begin() +
			extra_attack_mima_count;
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(), end,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == end) {
			return;
		}
		// ZUN uses the playfield width for both coordinates here, allowing
		// Mima's transfer target to extend well below the visible playfield.
		const auto target_x = static_cast<std::int16_t>(
			randring_next16(state) % (288U << 4U)
		);
		const auto target_y = static_cast<std::int16_t>(
			randring_next16(state) % (288U << 4U)
		);
		extra_attack_transfer_init(
			state, *slot, center_x, center_y, target_x, target_y, player_id, 0x5A
		);
		slot->mima_timer = 0U;
		slot->frame = 3U;
		return;
	}
	if(character == 2) {
		const auto end = state.extra_attacks[player_id].begin() +
			extra_attack_marisa_count;
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(), end,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == end) {
			return;
		}
		extra_attack_transfer_init(
			state,
			*slot,
			center_x,
			center_y,
			static_cast<std::int16_t>(
				randring_next16(state) % (288U << 4U)
			),
			368 << 4,
			player_id,
			0x6E
		);
		return;
	}
	if(character == 3) {
		const auto target_x = static_cast<std::int16_t>(
			(randring_next16(state) % (200U << 4U)) + (44U << 4U)
		);
		const auto target_y = static_cast<std::int16_t>(
			(randring_next16(state) % (200U << 4U)) + (80U << 4U)
		);
		const auto angle_delta = ((randring_next16(state) & 1U) == 0U)
			? std::int8_t{-1}
			: std::int8_t{1};
		const auto angle = static_cast<std::uint8_t>(randring_next16(state));
		bool spawned = false;
		for(std::size_t slot_id = 0U;
			slot_id < extra_attack_ellen_count; ++slot_id) {
			auto &slot = state.extra_attacks[player_id][slot_id];
			if(slot.state != 0U) {
				continue;
			}
			const auto trail_x = slot.ellen_trail_x;
			const auto trail_y = slot.ellen_trail_y;
			extra_attack_transfer_init(
				state,
				slot,
				center_x,
				center_y,
				target_x,
				target_y,
				player_id,
				0x46
			);
			slot.ellen_trail_x = trail_x;
			slot.ellen_trail_y = trail_y;
			slot.ellen_radius = 0U;
			slot.ellen_angle_delta = angle_delta;
			slot.angle = spawned
				? static_cast<std::uint8_t>(angle + 0x80U)
				: angle;
			if(spawned) {
				return;
			}
			spawned = true;
		}
		return;
	}
	if(character == 4) {
		const auto end = state.extra_attacks[player_id].begin() +
			extra_attack_regular_count;
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(), end,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == end) {
			return;
		}
		const auto target_x = static_cast<std::int16_t>(
			randring_next16(state) % (288U << 4U)
		);
		const auto target_y = static_cast<std::int16_t>(
			randring_next16(state) % (64U << 4U)
		);
		extra_attack_transfer_init(
			state,
			*slot,
			center_x,
			center_y,
			target_x,
			target_y,
			player_id,
			0x5A
		);
		slot->kotohime_target_y = static_cast<std::int16_t>(
			(randring_next16(state) & 4095U) + (96U << 4U)
		);
		slot->kotohime_velocity_y = static_cast<std::uint16_t>(
			(randring_next16(state) & 0x1FU) + 0x10U
		);
		slot->kotohime_milestone = 0U;
		slot->behavior_variant = 0U;
		return;
	}
	if(character == 5) {
		// Kana chooses this target even if all eight slots are occupied.
		const auto target_x = static_cast<std::int16_t>(
			randring_next16(state) % (288U << 4U)
		);
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(),
			state.extra_attacks[player_id].begin() + extra_attack_regular_count,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == state.extra_attacks[player_id].begin() + extra_attack_regular_count) {
			return;
		}
		extra_attack_transfer_init(
			state,
			*slot,
			center_x,
			center_y,
			target_x,
			static_cast<std::int16_t>(randring_next16(state) % (128U << 4U)),
			player_id,
			0x5A
		);
		slot->angle = static_cast<std::uint8_t>(
			(randring_next16(state) & 0x1FU) - 0x0FU
		);
		if(target_x >= (144 << 4)) {
			slot->angle = static_cast<std::uint8_t>(slot->angle + 0x80U);
		}
		return;
	}
	if(character == 6) {
		const auto end = state.extra_attacks[player_id].begin() +
			extra_attack_regular_count;
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(), end,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == end) {
			return;
		}
		extra_attack_transfer_init(
			state,
			*slot,
			center_x,
			center_y,
			static_cast<std::int16_t>(
				randring_next16(state) % (288U << 4U)
			),
			16 << 4,
			player_id,
			0x64
		);
		slot->behavior_variant = static_cast<std::uint8_t>(
			randring_next16(state) & 1U
		);
		slot->angle = 0x40U;
		slot->speed = static_cast<std::uint8_t>(
			(randring_next16(state) & 0x0FU) + 0x20U
		);
		return;
	}
	if(character == 7) {
		for(std::size_t slot_id = 0U; slot_id < extra_attack_count; slot_id += 2U) {
			auto &top = state.extra_attacks[player_id][slot_id];
			if(top.state != 0U) {
				continue;
			}
			auto &bottom = state.extra_attacks[player_id][slot_id + 1U];
			extra_attack_transfer_init(
				state,
				top,
				center_x,
				center_y,
				static_cast<std::int16_t>(
					(randring_next16(state) % (276U << 4U)) + 0x60U
				),
				0,
				player_id,
				0x50
			);
			extra_attack_transfer_init(
				state,
				bottom,
				center_x,
				center_y,
				static_cast<std::int16_t>(
					(randring_next16(state) % (276U << 4U)) + 0x60U
				),
				376 << 4,
				player_id,
				0x50
			);
			return;
		}
		return;
	}
	if(character == 8) {
		const auto end = state.extra_attacks[player_id].begin() +
			extra_attack_regular_count;
		const auto slot = std::find_if(
			state.extra_attacks[player_id].begin(), end,
			[](const ExtraAttackState &attack) { return attack.state == 0U; }
		);
		if(slot == end) {
			return;
		}
		// Turbo C++ evaluates these arguments from left to right. Keep the two
		// ring reads explicitly sequenced so host compiler argument evaluation
		// order cannot swap Yumemi's destination coordinates.
		const auto target_x = static_cast<std::int16_t>(
			randring_next16(state) % (288U << 4U)
		);
		const auto target_y = static_cast<std::int16_t>(
			(randring_next16(state) % (256U << 4U)) + (96U << 4U)
		);
		extra_attack_transfer_init(
			state,
			*slot,
			center_x,
			center_y,
			target_x,
			target_y,
			player_id,
			0x46
		);
		slot->yumemi_extent = 0U;
		slot->yumemi_duration = 0x60U;
	}
}

void chain_fire_charged_exatt(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t chain_slot,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	auto &charge = state.chains.charge_exatt[player_id][chain_slot];
	const auto threshold = static_cast<std::uint8_t>(
		6U - (state.round_speed / 32U)
	);
	if(charge < threshold) {
		return;
	}
	charge = 0U;
	extra_attack_add(state, center_x, center_y, player_id);
}

void fireball_add_if_charged(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t chain_slot,
	std::int16_t origin_x,
	std::int16_t origin_y,
	FireballVariant variant = FireballVariant::blue
) noexcept
{
	auto &charge = state.chains.charge_fireball[player_id][chain_slot];
	const auto threshold = static_cast<std::uint8_t>(
		12U - (state.round_speed / 16U)
	);
	if(charge < threshold) {
		return;
	}
	charge = 0U;
	(void)fireball_add(
		state, origin_x, origin_y, player_id, variant
	);
}

bool bullet_add_transfer_pellet(
	DeterministicState &state,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id,
	std::uint8_t template_speed
) noexcept
{
	// The collision caller initializes the source template angle before the
	// type-only bullets_add_transfer_pellet() wrapper.
	state.bullet_template_angle = 0U;
	if(!bullet_center_is_spawnable(center_x, center_y)) {
		return false;
	}
	auto *bullet = bullet_free_slot(state);
	if(bullet == nullptr) {
		return false;
	}
	bullet->flag = BulletFlag::transfer_pellet;
	bullet->age = 0U;
	bullet->center_x = center_x;
	bullet->center_y = center_y;
	bullet->player_id = player_id;
	bullet->group_next = bullet_group_random_constrained_aimed;
	bullet->speed_next = bullet_speed_adjusted(state, template_speed);
	bullet->angle_next = 0U;
	bullet->accel_type = 0U;
	bullet->is_collidable = true;
	bullet->has_trail = false;

	const auto origin_screen = playfield_x_to_screen_subpixel(
		state, center_x, player_id
	);
	const auto target_x = static_cast<std::int16_t>(
		randring_next16(state) % static_cast<std::uint16_t>(288U << 4U)
	);
	bullet->target_x_for_target_player = target_x;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto target_screen = playfield_x_to_screen_subpixel(
		state, target_x, target_player
	);
	const auto target_y = static_cast<std::int16_t>(
		randring_next16(state) & 0x00FFU
	);
	const auto travel_speed = static_cast<std::int16_t>(
		70U + (state.round_speed / 3U)
	);
	const auto velocity = vector2_between_plus(
		exact::wrap_shift_left(origin_screen, 4U),
		center_y,
		exact::wrap_shift_left(target_screen, 4U),
		target_y,
		0U,
		travel_speed
	);
	bullet->velocity_x = velocity.x;
	bullet->velocity_y = velocity.y;
	bullet->target_x_for_origin_player = screen_x_to_playfield_subpixel(
		target_screen, player_id
	);
	return true;
}

bool bullet_add_cloud_from_transfer(
	DeterministicState &state,
	const BulletState &source
) noexcept
{
	// bullets_add_next_from_p() copies this field before attempting the add.
	state.bullet_template_angle = source.angle_next;
	if(!bullet_center_is_spawnable(source.center_x, source.center_y)) {
		return false;
	}
	auto *bullet = bullet_free_slot(state);
	if(bullet == nullptr) {
		return false;
	}
	bullet->flag = BulletFlag::pellet_cloud;
	bullet->age = 0U;
	bullet->center_x = source.center_x;
	bullet->center_y = source.center_y;
	bullet->player_id = source.player_id;
	bullet->group_next = source.group_next;
	bullet->speed_next = bullet_speed_adjusted(state, source.speed_next);
	bullet->accel_type = 0U;
	bullet->is_collidable = true;
	bullet->has_trail = false;
	const auto angle_offset = static_cast<std::uint8_t>(
		(randring_next16(state) & 0x001FU) - 0x10U
	);
	bullet->angle_next = angle_offset;
	const auto &target = state.players[bullet->player_id];
	const auto velocity = vector2_between_plus(
		bullet->center_x,
		bullet->center_y,
		target.center_x.raw(),
		target.center_y.raw(),
		angle_offset,
		static_cast<std::int16_t>(bullet->speed_next)
	);
	bullet->velocity_x = velocity.x;
	bullet->velocity_y = velocity.y;
	return true;
}

void collmap_set_bullet(
	DeterministicState &state,
	const BulletState &bullet
) noexcept
{
	if(
		bullet.center_x < 0 || bullet.center_x >= (288 << 4) ||
		bullet.center_y < 0 || bullet.center_y >= (368 << 4)
	) {
		return;
	}
	const auto x = static_cast<std::size_t>(bullet.center_x >> 5U);
	const auto y = static_cast<std::size_t>(bullet.center_y >> 5U);
	state.collision_map[bullet.player_id][x][y] = true;
}

template <typename Callback>
void for_each_explosion(
	DeterministicState &state,
	Callback &&callback
) noexcept
{
	// Enemy and fireball objects share one 64-slot EFE arena in the original.
	// Preserve that physical order even though the native state stores the two
	// subclasses separately.
	for(auto &explosion : state.enemies) {
		callback(explosion);
	}
	for(auto &explosion : state.fireballs) {
		callback(explosion);
	}
}

template <typename Explosion>
bool explosion_overlaps(
	const Explosion &explosion,
	std::uint8_t player_id,
	std::int16_t left,
	std::int16_t right,
	std::int16_t top,
	std::int16_t bottom
) noexcept
{
	const auto flag = static_cast<std::uint8_t>(explosion.flag);
	if(
		flag < static_cast<std::uint8_t>(EnemyFlag::explosion_ignoring_enemies) ||
		explosion.player_id != player_id || (explosion.frame & 3U) != 0U
	) {
		return false;
	}
	const auto radius = static_cast<std::int16_t>(explosion.size_pixels * 6U);
	return !(
		right < exact::wrap_subtract(explosion.center_x, radius) ||
		bottom < exact::wrap_subtract(explosion.center_y, radius) ||
		left > exact::wrap_add(explosion.center_x, radius) ||
		top > exact::wrap_add(explosion.center_y, radius)
	);
}

template <typename Explosion>
void explosion_enemy_hit_increment(Explosion &explosion) noexcept
{
	using Flag = decltype(explosion.flag);
	explosion.flag = static_cast<Flag>(
		static_cast<std::uint8_t>(explosion.flag) + 1U
	);
}

std::uint8_t explosion_max_enemy_hits_half(
	const EnemyState &explosion
) noexcept
{
	return explosion.explosion_max_enemy_hits_half;
}

std::uint8_t explosion_max_enemy_hits_half(
	const FireballState &explosion
) noexcept
{
	// This byte is deliberately overlaid with the fireball variant in the
	// original EFE subclass.
	return explosion.variant_as_explosion_target;
}

bool pellet_explosions_hittest(
	DeterministicState &state,
	const BulletState &pellet
) noexcept
{
	constexpr std::int16_t pellet_radius = 6 << 4;
	const auto left = exact::wrap_subtract(pellet.center_x, pellet_radius);
	const auto right = exact::wrap_add(pellet.center_x, pellet_radius);
	const auto top = exact::wrap_subtract(pellet.center_y, pellet_radius);
	const auto bottom = exact::wrap_add(pellet.center_y, pellet_radius);
	bool collided = false;
	for_each_explosion(state, [&](auto &explosion) {
		if(!explosion_overlaps(
			explosion, pellet.player_id, left, right, top, bottom
		)) {
			return;
		}
		collided = true;
		const auto slot = explosion.chain_slot;
		auto &hits = state.chains.hits[pellet.player_id][slot];
		if(hits != 0xFFU) {
			hits++;
		}
		score_add(state.score[pellet.player_id], 1U);
		auto bonus = static_cast<std::uint16_t>(hits * 2U);
		bonus = static_cast<std::uint16_t>(bonus + state.round_speed);
		if(state.round_speed <= (3U << 4U)) {
			bonus >>= 1U;
		} else if(state.round_speed >= (6U << 4U)) {
			bonus = static_cast<std::uint16_t>(bonus + bonus);
		}
		auto &value = state.chains.pellet_and_fireball_value
			[pellet.player_id][slot];
		value++;
		gauge_avail_add(state, pellet.player_id, 4U);
		if(value < 4U) {
			state.chains.charge_fireball[pellet.player_id][slot] =
				static_cast<std::uint8_t>(
					state.chains.charge_fireball[pellet.player_id][slot] + 2U
				);
		} else if(value < 10U) {
			state.chains.charge_fireball[pellet.player_id][slot] =
				static_cast<std::uint8_t>(
					state.chains.charge_fireball[pellet.player_id][slot] + 5U
				);
		} else if(value < 20U) {
			state.chains.charge_fireball[pellet.player_id][slot] =
				static_cast<std::uint8_t>(
					state.chains.charge_fireball[pellet.player_id][slot] + 2U
				);
			state.chains.charge_exatt[pellet.player_id][slot]++;
		} else {
			state.chains.charge_exatt[pellet.player_id][slot] =
				static_cast<std::uint8_t>(
					state.chains.charge_exatt[pellet.player_id][slot] + 2U
				);
		}
		bonus = combo_add(state, pellet.player_id, hits, bonus);
		fire_point_based_boss_attack_or_panic(
			state, bonus, pellet.player_id, slot
		);
		fireball_add_if_charged(
			state,
			pellet.player_id,
			slot,
			explosion.center_x,
			explosion.center_y
		);
	});
	return collided;
}

void bullets_update(DeterministicState &state) noexcept
{
	for(auto &bullet : state.bullets) {
		if(bullet.flag == BulletFlag::free) {
			continue;
		}
		bullet.age++;
		if(bullet.flag == BulletFlag::pellet_cloud) {
			if((bullet.age & 3U) == 0U && bullet.age >= 12U) {
				bullet.flag = BulletFlag::pellet;
			}
			continue;
		}
		if(
			bullet.flag != BulletFlag::transfer_pellet &&
			(state.bomb_flag[bullet.player_id] != BombFlag::inactive ||
			 state.damage_all_on[bullet.player_id] != 0U)
		) {
			bullet.flag = BulletFlag::free;
			continue;
		}

		auto next_x = exact::wrap_add(
			bullet.center_x, bullet.velocity_x
		);
		if(bullet.flag == BulletFlag::transfer_pellet) {
			const auto reached = (bullet.player_id == 0U)
				? (next_x >= bullet.target_x_for_origin_player)
				: (next_x <= bullet.target_x_for_origin_player);
			if(reached) {
				bullet.flag = BulletFlag::pellet_cloud;
				bullet.player_id ^= 1U;
				bullet.center_x = bullet.target_x_for_target_player;
				bullet.center_y = 2 << 4;
				bullet.speed_next = static_cast<std::uint8_t>(
					bullet.speed_next + 8U
				);
				(void)bullet_add_cloud_from_transfer(state, bullet);
				bullet.flag = BulletFlag::free;
				continue;
			}
		} else if(bullet.has_trail) {
			if(next_x <= bullet_center_min) {
				next_x = bullet_center_min;
			} else if(next_x >= bullet_center_x_max) {
				next_x = bullet_center_x_max;
			}
			auto &trail = state.bullet_trails[bullet.trail_index];
			if(!bullet_trail_update_and_clip(
				trail.center_x,
				bullet.center_x,
				bullet_center_min,
				bullet_center_x_max
			)) {
				bullet.flag = BulletFlag::free;
				continue;
			}
		} else if(
			next_x <= bullet_center_min || next_x >= bullet_center_x_max
		) {
			bullet.flag = BulletFlag::free;
			continue;
		}
		bullet.center_x = next_x;

		const auto next_y = exact::wrap_add(
			bullet.center_y, bullet.velocity_y
		);
		if(bullet.has_trail) {
			auto &trail = state.bullet_trails[bullet.trail_index];
			if(!bullet_trail_update_and_clip(
				trail.center_y,
				bullet.center_y,
				bullet_center_min,
				bullet_center_y_max
			)) {
				bullet.flag = BulletFlag::free;
				continue;
			}
		} else if(next_y <= bullet_center_min || next_y >= bullet_center_y_max) {
			bullet.flag = BulletFlag::free;
			continue;
		}
		bullet.center_y = next_y;
		if(bullet.accel_type == 0x80U && bullet.velocity_y < (5 << 4)) {
			bullet.velocity_y = exact::wrap_add(
				bullet.velocity_y, static_cast<std::int16_t>(1)
			);
		}
		if(
			bullet.flag == BulletFlag::pellet &&
			pellet_explosions_hittest(state, bullet)
		) {
			if(
				state.gba_flag_active[bullet.player_id ^ 1U] ==
				GaugeBossAttackFlag::none
			) {
				(void)bullet_add_transfer_pellet(
					state,
					bullet.center_x,
					bullet.center_y,
					bullet.player_id,
					bullet.speed_next
				);
			}
			bullet.flag = BulletFlag::free;
			continue;
		}
		if(
			bullet.flag != BulletFlag::transfer_pellet &&
			bullet.is_collidable
		) {
			collmap_set_bullet(state, bullet);
		}
	}
}
