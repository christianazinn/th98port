// Included by frame.cpp inside its private implementation namespace.

void extra_attacks_update(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	const auto character = character_index_from_paletted(
		state.players[player_id].paletted_character
	);
	if(character == character_reimu) {
		extra_attacks_update_reimu(state, player_id, observer);
	} else if(character == character_mima) {
		extra_attacks_update_mima(state, player_id, observer);
	} else if(character == character_marisa) {
		extra_attacks_update_marisa(state, player_id, observer);
	} else if(character == character_ellen) {
		extra_attacks_update_ellen(state, player_id, observer);
	} else if(character == character_kotohime) {
		extra_attacks_update_kotohime(state, player_id, observer);
	} else if(character == character_kana) {
		extra_attacks_update_kana(state, player_id);
	} else if(character == character_rikako) {
		extra_attacks_update_rikako(state, player_id, observer);
	} else if(character == character_chiyuri) {
		extra_attacks_update_chiyuri(state, player_id, observer);
	} else if(character == character_yumemi) {
		extra_attacks_update_yumemi(state, player_id, observer);
	}
}

void extra_attacks_render_phase_update(DeterministicState &state) noexcept
{
	// Reimu advances this byte in her renderer; Kana already advances it in
	// the update callback.
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		const auto character = character_index_from_paletted(
			state.players[player_id].paletted_character
		);
		if(character != character_reimu) {
			continue;
		}
		for(std::size_t slot = 0U; slot < extra_attack_regular_count; ++slot) {
			auto &attack = state.extra_attacks[player_id][slot];
			if(attack.state != 0U) {
				attack.frame++;
			}
		}
	}
}

void extra_attack_add_direct(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t angle,
	std::uint8_t speed
) noexcept
{
	const auto slot = std::find_if(
		state.extra_attacks[player_id].begin(),
		state.extra_attacks[player_id].begin() + extra_attack_regular_count,
		[](const ExtraAttackState &attack) { return attack.state == 0U; }
	);
	if(slot == state.extra_attacks[player_id].begin() + extra_attack_regular_count) {
		return;
	}
	*slot = ExtraAttackState{};
	slot->state = 1U;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->player_id = static_cast<std::uint8_t>(1U - player_id);
	slot->angle = angle;
	slot->speed = speed;
	const auto velocity = vector2(angle, speed);
	slot->velocity_x = velocity.x;
	slot->velocity_y = velocity.y;
}

void extra_attack_add_direct_marisa(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	const auto end = state.extra_attacks[player_id].begin() +
		extra_attack_marisa_count;
	const auto slot = std::find_if(
		state.extra_attacks[player_id].begin(), end,
		[](const ExtraAttackState &attack) { return attack.state == 0U; }
	);
	if(slot == end) {
		return;
	}
	// marisa_19B06() receives the target PID and selects the opposite buffer.
	// The Boss-side API receives the owner instead, but preserves the source's
	// partial overwrite and therefore all stale, unmentioned slot fields.
	slot->state = 3U;
	slot->frame = 0U;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->player_id = static_cast<std::uint8_t>(1U - player_id);
}

void extra_attack_add_direct_yumemi(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	const auto begin = state.extra_attacks[player_id].begin() +
		extra_attack_regular_count;
	const auto end = state.extra_attacks[player_id].end();
	const auto slot = std::find_if(
		begin, end,
		[](const ExtraAttackState &attack) { return attack.state == 0U; }
	);
	if(slot == end) {
		return;
	}
	// yumemi_1A95F() overwrites only the fields below, preserving the source's
	// stale contents in every other member of the reused slot.
	slot->state = 3U;
	slot->frame = 0U;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->player_id = static_cast<std::uint8_t>(1U - player_id);
	slot->yumemi_extent = 0U;
	slot->yumemi_duration = 0x20U;
}

void extra_attack_add_direct_rikako(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t angle
) noexcept
{
	const auto end = state.extra_attacks[player_id].begin() +
		extra_attack_rikako_count;
	const auto slot = std::find_if(
		state.extra_attacks[player_id].begin(), end,
		[](const ExtraAttackState &attack) { return attack.state == 0U; }
	);
	if(slot == end) {
		return;
	}
	*slot = ExtraAttackState{};
	slot->state = 1U;
	slot->frame = 0x40U;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->player_id = static_cast<std::uint8_t>(1U - player_id);
	slot->angle = angle;
	slot->speed = 0x20U;
	slot->behavior_variant = 1U;
}

void extra_attack_add_direct_ellen(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t angle,
	std::int8_t angle_delta
) noexcept
{
	const auto end = state.extra_attacks[player_id].begin() +
		extra_attack_ellen_count;
	const auto slot = std::find_if(
		state.extra_attacks[player_id].begin(), end,
		[](const ExtraAttackState &attack) { return attack.state == 0U; }
	);
	if(slot == end) {
		return;
	}
	// Ellen's direct allocator overwrites only these fields. In particular,
	// the trail is filled naturally during the initial expanding orbit.
	slot->state = 1U;
	slot->frame = 0U;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->ellen_radius = 0;
	slot->ellen_angle_delta = angle_delta;
	slot->angle = angle;
	slot->player_id = static_cast<std::uint8_t>(1U - player_id);
}
