// Included by frame.cpp inside its private implementation namespace.

	const auto character = character_index_from_paletted(
		state.players[player_id].paletted_character
	);
	if(character == character_reimu) {
		boss_update_reimu(state, player_id, observer);
	} else if(character == character_mima) {
		boss_update_mima(state, player_id, observer);
	} else if(character == character_marisa) {
		boss_update_marisa(state, player_id, observer);
	} else if(character == character_ellen) {
		boss_update_ellen(state, player_id, observer);
	} else if(character == character_kotohime) {
		boss_update_kotohime(state, player_id, observer);
	} else if(character == character_kana) {
		boss_update_kana(state, player_id, observer);
	} else if(character == character_rikako) {
		boss_update_rikako(state, player_id, observer);
	} else if(character == character_chiyuri) {
		boss_update_chiyuri(state, player_id, observer);
	} else if(character == character_yumemi) {
		boss_update_yumemi(state, player_id, observer);
	}
}

void boss_render_phase_update(
	DeterministicState &state,
	GameplayObserver *observer
) noexcept
{
	if(state.gba_boss_launched_by < 0) {
		return;
	}
	const auto owner = static_cast<std::uint8_t>(state.gba_boss_launched_by);
	const auto character = character_index_from_paletted(
		state.players[owner].paletted_character
	);
	if(state.boss.state == 0U) {
		// kotohime_12103() advances shared Boss state from the otherwise
		// presentational materialization renderer. Later attack patterns use
		// both values as their orbit angle and radius.
		if(
			character == character_kotohime &&
			(
				(state.round_frame & 1U) == 0U ||
				state.boss.pattern_frame >= 0x40U
			)
		) {
			state.boss.pattern_angle = static_cast<std::uint8_t>(
				state.boss.pattern_angle + 2U
			);
			state.boss.anchor_x = static_cast<std::int16_t>(
				(200U - (state.boss.pattern_frame * 2U)) << 4U
			);
		}
		return;
	}
	if(state.boss.state != 0xFFU) {
		// mima_10053() advances this animation byte from the active Boss
		// Attack renderer. A hit returns before this block, and state 5
		// deliberately resets the frame without changing the state byte.
		if(character == character_mima && state.boss.hit == 0U) {
			if(state.boss.render_state == 5U) {
				state.boss.render_frame = 0U;
			}
			if(
				state.boss.render_state == 2U ||
				state.boss.render_state == 5U
			) {
				if(state.boss.render_frame >= 0x20U) {
					state.boss.render_state = 3U;
				}
				state.boss.render_frame++;
			}
		}
		return;
	}
	if(static_cast<std::int16_t>(state.boss.pattern_frame * 8U) >= 200) {
		const auto winner = static_cast<std::uint8_t>(
			1 - state.gba_boss_launched_by
		);
		score_add(state.score[winner], 2560U);
		state.boss.state = 0U;
		state.gba_boss_launched_by = -1;
		if(character == character_chiyuri) {
			observe_boss_palette_baseline_restored(observer, winner);
		}
	}
}
