// Included by frame.cpp inside its private implementation namespace.

void extra_attack_add(
	DeterministicState &state,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id
) noexcept
{
	const auto character = character_index_from_paletted(
		state.players[player_id].paletted_character
	);
	if(character == character_reimu) {
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
	if(character == character_mima) {
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
	if(character == character_marisa) {
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
	if(character == character_ellen) {
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
	if(character == character_kotohime) {
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
	if(character == character_kana) {
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
	if(character == character_rikako) {
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
	if(character == character_chiyuri) {
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
	if(character == character_yumemi) {
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
