// Included by frame.cpp inside its private implementation namespace.

void release_gauge(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &player = state.players[player_id];
	const auto charged = player.gauge_charged;
	if(charged >= gauge_64) {
		const auto character = character_index_from_paletted(
			player.paletted_character
		);
		if(character == character_reimu) {
			add_reimu_chargeshot(state, player_id);
		} else if(character == character_mima) {
			add_mima_chargeshot(state, player_id);
		} else if(character == character_marisa) {
			add_marisa_chargeshot(state, player_id);
		} else if(character == character_ellen) {
			add_ellen_chargeshot(state, player_id);
		} else if(character == character_kotohime) {
			add_kotohime_chargeshot(state, player_id);
		} else if(character == character_kana) {
			add_kana_chargeshot(state, player_id);
		} else if(character == character_rikako) {
			add_rikako_chargeshot(state, player_id);
		} else if(character == character_chiyuri) {
			add_chiyuri_chargeshot(state, player_id);
		} else if(character == character_yumemi) {
			add_yumemi_chargeshot(state, player_id);
		}
		if(charged >= gauge_128) {
			state.warning_flag[player_id] = WarningFlag::portrait;
			if(charged >= gauge_max) {
				if(state.gba_boss_launched_by == static_cast<std::int8_t>(player_id)) {
					player.gauge_attacks_fired++;
					state.gba_flag_next[player_id] =
						GaugeBossAttackFlag::gauge_bullet_init;
					player.gauge_avail = static_cast<std::uint16_t>(
						player.gauge_avail - gauge_128
					);
					if(state.gba_gauge_level[player_id] < 16U) {
						state.gba_gauge_level[player_id]++;
					}
				} else {
					state.gba_flag_next[player_id] = GaugeBossAttackFlag::boss;
					player.gauge_avail = gauge_64;
					if(state.gba_boss_level < 16U) {
						state.gba_boss_level++;
					}
					state.combo_points_for_boss_attack =
						static_cast<std::uint16_t>(
							state.combo_points_for_boss_attack + 10240U
						);
					if(state.gba_boss_launched_by < 0) {
						player.boss_attacks_fired++;
					} else {
						player.boss_attacks_reversed++;
					}
				}
			} else if(charged >= gauge_192) {
				player.gauge_attacks_fired++;
				state.gba_flag_next[player_id] =
					GaugeBossAttackFlag::gauge_bullet_init;
				player.gauge_avail = static_cast<std::uint16_t>(
					player.gauge_avail - gauge_128
				);
				if(state.gba_gauge_level[player_id] < 16U) {
					state.gba_gauge_level[player_id]++;
				}
			} else {
				player.gauge_attacks_fired++;
				state.gba_flag_next[player_id] =
					GaugeBossAttackFlag::gauge_pellet_init;
				player.gauge_avail = static_cast<std::uint16_t>(
					player.gauge_avail - gauge_64
				);
				if(state.gba_gauge_level[player_id] < 16U) {
					state.gba_gauge_level[player_id]++;
				}
			}
		}
		player.invincibility_time = static_cast<std::uint8_t>(
			player.invincibility_time + 6U
		);
	}
	player.gauge_charged = 0U;
}

void player_bomb(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &player = state.players[player_id];
	if(
		state.bomb_flag[player_id] != BombFlag::inactive ||
		player.hyper_active != 0U
	) {
		return;
	}
	if(player.bombs != 0U) {
		state.bomb_flag[player_id] = BombFlag::preparing;
		player.bombs--;
		player.invincibility_time = bomb_frames;
		if(player_id == 0U && state.skill > 0U) {
			state.skill--;
		}
		return;
	}
	if(player.gauge_avail < gauge_max) {
		return;
	}
	state.damage_all_on[player_id] = 1U;
	player.hyper_active = static_cast<std::uint8_t>(player.paletted_character);
	if(player_id == 0U && state.skill > 0U) {
		state.skill--;
	}
}
