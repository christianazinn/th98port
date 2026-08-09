// Included by frame.cpp inside its private implementation namespace.

void bomb_character_update(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.bomb_flag[player_id];
	if(flag == BombFlag::inactive) {
		return;
	}
	const auto character = character_index_from_paletted(
		state.players[player_id].paletted_character
	);
	if(flag == BombFlag::preparing) {
		flag = BombFlag::active;
		state.bomb_frame[player_id] = 0U;
		if(character == character_ellen) {
			for(auto &vector : state.ellen_bomb_vectors[player_id]) {
				vector.x = ellen_bomb_vector_inactive;
			}
		}
	}
	state.bomb_frame[player_id]++;
	if(
		character == character_mima && state.bomb_frame[player_id] <= 64U &&
		(state.bomb_frame[player_id] % 8U) == 0U
	) {
		// mima_17043() chooses each visual column from the shared random ring.
		(void)randring_next16(state);
	}
	if(character == character_ellen) {
		for(auto &vector : state.ellen_bomb_vectors[player_id]) {
			bool respawn = (vector.x == ellen_bomb_vector_free);
			if(vector.x == ellen_bomb_vector_inactive) {
				continue;
			}
			if(!respawn) {
				vector.x = exact::wrap_add(vector.x, vector.velocity_x);
				vector.y = exact::wrap_add(vector.y, vector.velocity_y);
				respawn = playfield_clip(
					vector.x, vector.y, static_cast<std::int16_t>(-(32 << 4))
				);
			}
			if(respawn) {
				vector.x = 144 << 4;
				vector.y = 184 << 4;
				const auto velocity = vector2(
					static_cast<std::uint8_t>(randring_next16(state)), 224
				);
				vector.velocity_x = velocity.x;
				vector.velocity_y = velocity.y;
			}
		}
	}
	if(state.bomb_frame[player_id] >= bomb_frames) {
		flag = BombFlag::inactive;
	}
}

void bombs_render_phase_update(DeterministicState &state) noexcept
{
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		if(state.bomb_flag[player_id] == BombFlag::inactive) {
			continue;
		}
		const auto character = character_index_from_paletted(
			state.players[player_id].paletted_character
		);
		if(character == character_reimu && state.bomb_frame[player_id] < 32U) {
			// reimu_bomb() creates one visual record with two shared-ring draws.
			(void)randring_next16(state);
			(void)randring_next16(state);
		}
		if(character == character_reimu) {
			const auto frame = state.bomb_frame[player_id];
			if(frame >= 64U && frame < 112U) {
				state.playfield_fg_shift_x[player_id] =
					(((frame % 8U) & 3U) < 2U) ? 4 : -4;
			} else if(frame >= 112U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == character_mima) {
			const auto frame = state.bomb_frame[player_id];
			if(frame > 80U && frame < 144U) {
				state.playfield_fg_shift_x[player_id] =
					(((frame % 8U) & 3U) < 2U) ? 4 : -4;
			} else if(frame >= 144U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == character_marisa) {
			const auto frame = state.bomb_frame[player_id];
			if(frame >= 64U && frame < 128U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
				if((frame % 8U) == 0U) {
					(void)randring_next16(state);
					(void)randring_next16(state);
				}
			} else if(frame >= 128U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == character_ellen) {
			const auto frame = state.bomb_frame[player_id];
			if(frame < 64U) {
				state.ellen_bomb_vector_cursor = 0U;
			} else if(frame < 128U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
				if(
					(frame % 4U) == 0U &&
					state.ellen_bomb_vector_cursor < 8U
				) {
					state.ellen_bomb_vectors[player_id]
						[state.ellen_bomb_vector_cursor].x =
							ellen_bomb_vector_free;
					state.ellen_bomb_vector_cursor++;
				}
			} else {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == character_rikako) {
			const auto frame = state.bomb_frame[player_id];
			if(frame >= 64U && frame < 128U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
				if((frame % 8U) == 0U) {
					(void)randring_next16(state);
				}
			} else if(frame >= 128U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == character_chiyuri) {
			const auto frame = state.bomb_frame[player_id];
			if(frame >= 64U && frame < 144U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
				if((frame % 16U) == 0U) {
					// Four SUB_CE5B() visuals share the gameplay random ring.
					for(std::size_t effect = 0U; effect < 4U; ++effect) {
						(void)randring_next16(state);
					}
				}
			} else if(frame >= 144U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == character_yumemi) {
			const auto frame = state.bomb_frame[player_id];
			if(frame > 64U && frame < 144U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
				if((frame & 7U) == 0U) {
					// Yumemi's periodic SUB_CE5B() visual consumes one ring value.
					(void)randring_next16(state);
				}
			} else if(frame >= 144U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == character_kotohime || character == character_kana) {
			const auto frame = state.bomb_frame[player_id];
			if(frame >= 64U && frame < 128U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
			} else if(frame >= 128U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
	}
}
