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

void bombs_render_phase_update(
	DeterministicState &state,
	GameplayObserver *observer
) noexcept
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
				if((frame % 8U) == 0U) {
					observe_common_effect(
						state, observer, CommonEffectKind::expanding_circle,
						144 << 4, 184 << 4, player_id
					);
				}
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
					const auto random_x = randring_next16(state);
					const auto random_y = randring_next16(state);
					if(observer != nullptr) {
						observe_common_effect(
							state, observer, CommonEffectKind::expanding_circle,
							static_cast<std::int16_t>(random_x % (288U << 4U)),
							static_cast<std::int16_t>(random_y % (368U << 4U)),
							player_id
						);
					}
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
				if((frame % 4U) == 0U) {
					observe_common_effect(
						state, observer, CommonEffectKind::expanding_circle,
						144 << 4, 184 << 4, player_id
					);
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
					const auto random_x = randring_next16(state);
					if(observer != nullptr) {
						auto center_x = static_cast<std::int16_t>(
							random_x & 1023U
						);
						do {
							observe_common_effect(
								state, observer,
								CommonEffectKind::expanding_circle,
								center_x, 368 << 4, player_id
							);
							center_x = exact::wrap_add(
								center_x, std::int16_t{96 << 4}
							);
						} while(center_x <= (288 << 4));
					}
				}
			} else if(frame >= 128U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == character_chiyuri) {
			const auto frame = state.bomb_frame[player_id];
			if(frame < 64U) {
				if((frame % 8U) == 0U) {
					observe_common_effect(
						state, observer, CommonEffectKind::contracting_circle,
						144 << 4, 184 << 4, player_id
					);
				}
			} else if(frame < 144U) {
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
				if((frame % 16U) == 0U) {
					if(observer == nullptr) {
						for(std::size_t effect = 0U; effect < 4U; ++effect) {
							(void)randring_next16(state);
						}
						continue;
					}
					const auto radius = static_cast<std::int16_t>(
						(frame - 64U) * (2U << 4U)
					);
					observe_common_square(
						state, observer,
						exact::wrap_subtract(std::int16_t{144 << 4}, radius),
						184 << 4, player_id
					);
					observe_common_square(
						state, observer,
						exact::wrap_add(std::int16_t{144 << 4}, radius),
						184 << 4, player_id
					);
					observe_common_square(
						state, observer, 144 << 4,
						exact::wrap_subtract(std::int16_t{184 << 4}, radius),
						player_id
					);
					observe_common_square(
						state, observer, 144 << 4,
						exact::wrap_add(std::int16_t{184 << 4}, radius),
						player_id
					);
				}
			} else if(frame >= 144U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == character_yumemi) {
			const auto frame = state.bomb_frame[player_id];
			if(frame > 64U && frame < 144U) {
				if((frame & 7U) == 0U) {
					observe_common_square(
						state, observer, 144 << 4, 184 << 4, player_id
					);
				}
				state.playfield_fg_shift_x[player_id] =
					((frame & 3U) < 2U) ? 4 : -4;
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
			if(observer != nullptr) {
				observer->bomb_common_effect_step(BombCommonEffectStep{
					.kind = (character == character_kana)
						? BombCommonEffectKind::kana
						: BombCommonEffectKind::kotohime,
					.player = static_cast<PlayerId>(player_id),
					.bomb_frame = frame,
					.fg_shift_x_at_effects =
						state.playfield_fg_shift_x[player_id],
				});
			}
		}
	}
}
