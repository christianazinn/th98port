// Included by ../frame.cpp inside its private implementation namespace.

MoveResult movement_velocity(
	const PlayerState &player,
	std::uint16_t input,
	Velocity &velocity
) noexcept
{
	auto set = [&](std::int8_t x, std::int8_t y) {
		const bool diagonal = x != 0 && y != 0;
		auto speed_x = diagonal
			? player.speed_diagonal_x
			: player.speed_aligned_x;
		auto speed_y = diagonal
			? player.speed_diagonal_y
			: player.speed_aligned_y;
		const auto character =
			(static_cast<int>(player.paletted_character) - 1) / 2;
		if(player.hyper_active != 0U && player.hyper_effect_active_this_frame) {
			if(
				character == 0 || character == 1 || character == 4 ||
				character == 5 || character == 7 || character == 8
			) {
				const auto boost = static_cast<std::uint8_t>(diagonal ? 24U : 32U);
				speed_x = static_cast<std::uint8_t>(speed_x + boost);
				speed_y = static_cast<std::uint8_t>(speed_y + boost);
			} else if(character == 2) {
				const auto penalty = static_cast<std::uint8_t>(diagonal ? 12U : 16U);
				speed_x = static_cast<std::uint8_t>(speed_x - penalty);
				speed_y = static_cast<std::uint8_t>(speed_y - penalty);
			} else if(character == 6) {
				const auto penalty = static_cast<std::uint8_t>(diagonal ? 8U : 16U);
				speed_x = static_cast<std::uint8_t>(speed_x - penalty);
				speed_y = static_cast<std::uint8_t>(speed_y - penalty);
			}
		}
		velocity.x = static_cast<std::int16_t>(static_cast<int>(speed_x) * x);
		velocity.y = static_cast<std::int16_t>(static_cast<int>(speed_y) * y);
	};

	switch(input) {
	case input_left: set(-1, 0); break;
	case input_down | input_left:
	case input_down_left: set(-1, 1); break;
	case input_down: set(0, 1); break;
	case input_down | input_right:
	case input_down_right: set(1, 1); break;
	case input_right: set(1, 0); break;
	case input_up | input_right:
	case input_up_right: set(1, -1); break;
	case input_up: set(0, -1); break;
	case input_up | input_left:
	case input_up_left: set(-1, -1); break;
	case 0U: return MoveResult::no_input;
	default: return MoveResult::invalid;
	}
	return MoveResult::valid;
}

void apply_velocity(PlayerState &player, const Velocity &velocity) noexcept
{
	const auto x = exact::wrap_add(player.center_x.raw(), velocity.x);
	const auto y = exact::wrap_add(player.center_y.raw(), velocity.y);
	player.center_x = exact::Subpixel::from_raw(
		std::clamp(x, player_x_min, player_x_max)
	);
	player.center_y = exact::Subpixel::from_raw(
		std::clamp(y, player_y_min, player_y_max)
	);
}

void move_human(PlayerState &player, std::uint16_t live_input) noexcept
{
	auto movement = static_cast<std::uint16_t>(live_input & input_movement);
	bool use_previous = true;
	Velocity velocity{};
	for(;;) {
		const auto result = movement_velocity(player, movement, velocity);
		if(result == MoveResult::invalid && use_previous) {
			if(player.human_movement_last != movement) {
				movement = static_cast<std::uint16_t>(
					movement & static_cast<std::uint16_t>(~player.human_movement_last)
				);
				use_previous = false;
				continue;
			}
			return;
		}
		if(result == MoveResult::valid) {
			apply_velocity(player, velocity);
		}
		if(use_previous) {
			player.human_movement_last = movement;
		}
		return;
	}
}

void add_reimu_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	auto angle = std::uint8_t{0x90U};
	for(std::size_t group_id = 0U; group_id < 4U; ++group_id) {
		auto &group = state.reimu_charge[player_id][group_id];
		group.state = 1U;
		group.frame = 0U;
		group.x.fill(center_x);
		group.y.fill(center_y);
		group.angle = angle;
		group.speed = 0x60U;
		angle = static_cast<std::uint8_t>(angle + 0x20U);
	}
	state.reimu_chargeshot_primary = 1U;
}

void add_reimu_hyper_amulet(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	for(auto &group : state.reimu_charge[player_id]) {
		if(group.state != 0U) {
			continue;
		}
		group.state = 2U;
		group.frame = 0U;
		group.x.fill(center_x);
		group.y.fill(center_y);
		group.angle = 0xC0U;
		group.speed = 0xC0U;
		break;
	}
	state.reimu_chargeshot_primary = 0U;
}

void add_mima_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	constexpr std::array<std::uint8_t, 6> angles{
		192U, 182U, 202U, 192U, 176U, 208U,
	};
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	state.mima_charge_active[player_id] = 1U;
	state.mima_charge_frames[player_id] = 0U;
	auto &nodes = state.mima_charge[player_id];
	for(std::size_t node_id = 0U; node_id < nodes.size(); ++node_id) {
		auto &node = nodes[node_id];
		node.active = (node_id == 0U) ? 1U : 0U;
		node.visual_frame = 0U;
		node.x = center_x;
		node.y = center_y;
		const auto velocity = vector2(angles[node_id % angles.size()], 176);
		node.velocity_x = velocity.x;
		node.velocity_y = velocity.y;
	}
}

void marisa_charge_initialize(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.marisa_charge[player_id];
	shot.active = 1U;
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = exact::wrap_subtract(
		state.players[player_id].center_y.raw(), std::int16_t{0x100}
	);
	shot.x.fill(center_x);
	shot.y.fill(center_y);
}

void add_marisa_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	marisa_charge_initialize(state, player_id);
	state.marisa_charge[player_id].age = 0U;
	state.players[player_id].shot_active = 0U;
}

void activate_marisa_hyper(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.marisa_charge[player_id];
	if(shot.active == 0U) {
		marisa_charge_initialize(state, player_id);
	}
	shot.age = 0x10U;
	state.players[player_id].shot_active = 0U;
}

void add_ellen_hyper_node(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	for(auto &node : state.ellen_charge[player_id]) {
		if(node.state != 0U) {
			continue;
		}
		node.state = 1U;
		node.phase = 0U;
		node.x = state.players[player_id].center_x.raw();
		node.y = state.players[player_id].center_y.raw();
		node.angle = static_cast<std::uint8_t>(randring_next16(state));
		node.speed = 0x50U;
		return;
	}
}

void add_kana_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	state.kana_charge_state[player_id] = 1U;
	state.kana_charge_frames[player_id] = 0U;
	constexpr std::array<std::uint8_t, 4> angles{0x90U, 0xB8U, 0xC8U, 0xF0U};
	for(std::size_t group_id = 0U; group_id < 4U; ++group_id) {
		auto &group = state.kana_charge[player_id][group_id];
		group.x.fill(center_x);
		group.y.fill(center_y);
		group.angle = angles[group_id];
		group.speed = 0x30U;
	}
}

void add_rikako_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	state.rikako_charge_state[player_id] = 1U;
	state.rikako_charge_frames[player_id] = 0U;
	state.rikako_charge_radius[player_id] = 0x80U;
	auto angle = std::uint8_t{0x20U};
	for(auto &node : state.rikako_charge[player_id]) {
		node.x = center_x;
		node.y = center_y;
		node.angle = angle;
		angle = static_cast<std::uint8_t>(angle + 0x40U);
	}
	state.rikako_charge_origin_x[player_id] = center_x;
	state.rikako_charge_origin_y[player_id] = center_y;
}

void add_chiyuri_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &nodes = state.chiyuri_charge[player_id];
	nodes[0].frame = 0U;
	nodes[0].x = state.players[player_id].center_x.raw();
	nodes[0].y = state.players[player_id].center_y.raw();
	state.players[player_id].shot_active = 0U;
	for(std::size_t node_id = 0U; node_id < nodes.size(); ++node_id) {
		nodes[node_id].state = static_cast<std::uint8_t>((node_id * 4U) + 2U);
	}
}

void add_ellen_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	auto &nodes = state.ellen_charge[player_id];
	for(std::size_t node_id = 0U; node_id < nodes.size(); ++node_id) {
		auto &node = nodes[node_id];
		node.state = static_cast<std::uint8_t>(node_id * 2U);
		node.phase = 0U;
		node.x = center_x;
		node.y = center_y;
		node.angle = static_cast<std::uint8_t>(randring_next16(state));
		node.speed = 0x50U;
	}
}

void add_kotohime_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.kotohime_charge[player_id];
	shot.active = 1U;
	shot.x = state.players[player_id].center_x.raw();
	shot.y = state.players[player_id].center_y.raw();
	shot.velocity_y = -0x10;
}

void add_yumemi_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.yumemi_charge[player_id];
	shot.state = 1U;
	shot.frame = 0U;
	shot.x = state.players[player_id].center_x.raw();
	shot.y = state.players[player_id].center_y.raw();
}

void release_gauge(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &player = state.players[player_id];
	const auto charged = player.gauge_charged;
	if(charged >= gauge_64) {
		const auto character =
			(static_cast<int>(player.paletted_character) - 1) / 2;
		if(character == 0) {
			add_reimu_chargeshot(state, player_id);
		} else if(character == 1) {
			add_mima_chargeshot(state, player_id);
		} else if(character == 2) {
			add_marisa_chargeshot(state, player_id);
		} else if(character == 3) {
			add_ellen_chargeshot(state, player_id);
		} else if(character == 4) {
			add_kotohime_chargeshot(state, player_id);
		} else if(character == 5) {
			add_kana_chargeshot(state, player_id);
		} else if(character == 6) {
			add_rikako_chargeshot(state, player_id);
		} else if(character == 7) {
			add_chiyuri_chargeshot(state, player_id);
		} else if(character == 8) {
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

void bomb_character_update(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &flag = state.bomb_flag[player_id];
	if(flag == BombFlag::inactive) {
		return;
	}
	const auto character =
		(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
	if(flag == BombFlag::preparing) {
		flag = BombFlag::active;
		state.bomb_frame[player_id] = 0U;
		if(character == 3) {
			for(auto &vector : state.ellen_bomb_vectors[player_id]) {
				vector.x = ellen_bomb_vector_inactive;
			}
		}
	}
	state.bomb_frame[player_id]++;
	if(
		character == 1 && state.bomb_frame[player_id] <= 64U &&
		(state.bomb_frame[player_id] % 8U) == 0U
	) {
		// mima_17043() chooses each visual column from the shared random ring.
		(void)randring_next16(state);
	}
	if(character == 3) {
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
		const auto character =
			(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
		if(character == 0 && state.bomb_frame[player_id] < 32U) {
			// reimu_bomb() creates one visual record with two shared-ring draws.
			(void)randring_next16(state);
			(void)randring_next16(state);
		}
		if(character == 0) {
			const auto frame = state.bomb_frame[player_id];
			if(frame >= 64U && frame < 112U) {
				state.playfield_fg_shift_x[player_id] =
					(((frame % 8U) & 3U) < 2U) ? 4 : -4;
			} else if(frame >= 112U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == 1) {
			const auto frame = state.bomb_frame[player_id];
			if(frame > 80U && frame < 144U) {
				state.playfield_fg_shift_x[player_id] =
					(((frame % 8U) & 3U) < 2U) ? 4 : -4;
			} else if(frame >= 144U) {
				state.playfield_fg_shift_x[player_id] = 0;
			}
		}
		if(character == 2) {
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
		if(character == 3) {
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
		if(character == 6) {
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
		if(character == 7) {
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
		if(character == 8) {
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
		if(character == 4 || character == 5) {
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

void gauge_pattern_reimu(
	DeterministicState &state,
	std::uint8_t player_id
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
		flag = GaugeBossAttackFlag::none;
	}
	state.reimu_gauge_pattern_frame[player_id]++;
}

void gauge_pattern_marisa(
	DeterministicState &state,
	std::uint8_t player_id
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
		flag = GaugeBossAttackFlag::none;
	}
	frame++;
}

void gauge_pattern_kotohime(
	DeterministicState &state,
	std::uint8_t player_id
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
		flag = GaugeBossAttackFlag::none;
	}
	frame++;
}

void gauge_pattern_kana(
	DeterministicState &state,
	std::uint8_t player_id
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
	}
	if((frame % 0x40U) < 0x20U) {
		center_x = exact::wrap_add(center_x, static_cast<std::int16_t>(0x80));
	} else {
		center_x = exact::wrap_subtract(
			center_x, static_cast<std::int16_t>(0x80)
		);
	}
	if(frame >= 0x80U) {
		flag = GaugeBossAttackFlag::none;
	}
	state.kana_gauge_pattern_frame[player_id]++;
}

void gauge_pattern_mima(
	DeterministicState &state,
	std::uint8_t player_id
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
		bullets_add_group(
			state, type, center_x, 16 << 4, target_player,
			state.gauge_pattern_param_b[player_id], angle,
			bullet_group_five_spread_narrow
		);
		if((frame % 0x10U) != 0U) {
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
		flag = GaugeBossAttackFlag::none;
	}
	frame++;
}

void gauge_pattern_ellen(
	DeterministicState &state,
	std::uint8_t player_id
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
		bullets_add_group(
			state, type, center_x, 0, target_player, (3 << 4) + 8, 0U,
			bullet_group_ring_aimed, state.gauge_pattern_param_a[player_id]
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
		bullets_add_group(
			state, type, 0, center_y, target_player,
			state.gauge_pattern_param_b[player_id], angle,
			bullet_group_sixteen_ring
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

	flag = GaugeBossAttackFlag::none;
	frame++;
}

void gauge_pattern_rikako(
	DeterministicState &state,
	std::uint8_t player_id
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
	}
	if(frame >= 0x80U) {
		flag = GaugeBossAttackFlag::none;
	}
	state.rikako_gauge_pattern_frame[player_id]++;
}

void gauge_pattern_chiyuri(
	DeterministicState &state,
	std::uint8_t player_id
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
		flag = GaugeBossAttackFlag::none;
	}
}

void gauge_pattern_yumemi(
	DeterministicState &state,
	std::uint8_t player_id
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
			flag = GaugeBossAttackFlag::none;
			break;
		default:
			break;
		}
		if(frame <= 0x28U) {
			// SUB_CE5B() is visual, but consumes the shared random ring.
			(void)randring_next16(state);
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
	std::uint8_t player_id
) noexcept
{
	const auto character =
		(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
	if(character == 0) {
		gauge_pattern_reimu(state, player_id);
	} else if(character == 1) {
		gauge_pattern_mima(state, player_id);
	} else if(character == 2) {
		gauge_pattern_marisa(state, player_id);
	} else if(character == 3) {
		gauge_pattern_ellen(state, player_id);
	} else if(character == 4) {
		gauge_pattern_kotohime(state, player_id);
	} else if(character == 5) {
		gauge_pattern_kana(state, player_id);
	} else if(character == 6) {
		gauge_pattern_rikako(state, player_id);
	} else if(character == 7) {
		gauge_pattern_chiyuri(state, player_id);
	} else if(character == 8) {
		gauge_pattern_yumemi(state, player_id);
	}
}

bool player_hittest(
	const DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t hitbox_size,
	std::int16_t &collision_x,
	std::int16_t &collision_y
) noexcept;

void player_defeat_update(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &player = state.players[player_id];
	if(player.lose_anim_time == 0x30U) {
		state.defeat_flag = DefeatFlag::explode;
		state.damage_all_on[1U - player_id] = 1U;
	}
	state.playfield_fg_shift_x[player_id] =
		((player.lose_anim_time & 3U) < 2U) ? 6 : -6;
	player.lose_anim_time--;
	if(player.lose_anim_time != 0U) {
		return;
	}
	player.lose_anim_time = 0xFFU;
	state.playfield_fg_shift_x[player_id] = 0;
	state.defeat_flag = DefeatFlag::banner;
	state.players[1U - player_id].rounds_won++;
	state.winner = static_cast<std::int8_t>(1U - player_id);
	state.round_or_result_frame = 0U;
}

bool player_update_begin(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &player = state.players[player_id];
	if(player.invincibility_time != 0U) {
		player.invincibility_time--;
	}
	if(state.damage_all_on[player_id] != 0U) {
		state.damage_all_on[player_id]--;
	}
	if(state.defeat_flag == DefeatFlag::banner) {
		return false;
	}
	if(player.lose_anim_time != 0U) {
		if(player.lose_anim_time != 0xFFU) {
			player_defeat_update(state, player_id);
		}
		return false;
	}

	player.hyper_effect_active_this_frame = false;
	if(!player.hyper_character_callback) {
		player.shot_mode = 1U;
		if(player.hyper_active != 0U) {
			player.hyper_character_callback = true;
		}
	} else if(player.hyper_active == 0U) {
		const auto character =
			(static_cast<int>(player.paletted_character) - 1) / 2;
		if(character == 6) {
			state.rikako_charge_state[player_id] = 0U;
		}
		player.hyper_character_callback = false;
	} else {
		player.hyper_effect_active_this_frame = true;
		const auto character =
			(static_cast<int>(player.paletted_character) - 1) / 2;
		if(character == 0) {
			player.shot_mode = 4U;
		} else if(character == 1) {
			player.shot_mode = 2U;
			player.shot_active = 2U;
		} else if(character == 2) {
			player.shot_mode = 0U;
			activate_marisa_hyper(state, player_id);
		} else if(character == 3) {
			player.shot_mode = 1U;
			if((state.round_frame & 3U) == 0U) {
				add_ellen_hyper_node(state, player_id);
			}
		} else if(
			character == 4 || character == 5 ||
			character == 7 || character == 8
		) {
			player.shot_mode = 3U;
		} else if(character == 6 && player.gauge_avail > (250U << 4U)) {
			add_rikako_chargeshot(state, player_id);
			state.rikako_charge_state[player_id] = 2U;
		}
	}
	return true;
}

void player_knockback_update(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &player = state.players[player_id];
	if(player.knockback_time == knockback_frames) {
		player.knockback_angle = static_cast<std::uint8_t>(
			randring_next16(state) & 0x3FU
		);
		if(player.center_x.raw() < (144 << 4)) {
			if(player.center_y.raw() > (184 << 4)) {
				player.knockback_angle = static_cast<std::uint8_t>(
					player.knockback_angle + 192U
				);
			}
		} else if(player.center_y.raw() < (184 << 4)) {
			player.knockback_angle = static_cast<std::uint8_t>(
				player.knockback_angle + 0x40U
			);
		} else {
			player.knockback_angle = static_cast<std::uint8_t>(
				player.knockback_angle + 0x80U
			);
		}
		player.knockback_length = 0x40U;
	}
	if(player.knockback_time > 0x20U) {
		player.knockback_length = static_cast<std::uint8_t>(
			player.knockback_length - 2U
		);
		const auto knockback = vector2(
			player.knockback_angle,
			static_cast<std::int16_t>(player.knockback_length)
		);
		const Velocity velocity{knockback.x, knockback.y};
		apply_velocity(player, velocity);
	} else if(player.knockback_time == 0U) {
		player.knockback_active = false;
	}
	player.knockback_time--;
}

std::uint8_t players_hit_damage_update(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &player = state.players[player_id];
	auto damage = player.hit_damage_next;
	if(player.is_cpu != 0U) {
		damage = static_cast<std::uint8_t>(
			damage + state.cpu_hit_damage_additional
		);
	}
	player.hit_damage_next = 3U;
	auto &other = state.players[1U - player_id];
	if(other.hit_damage_next > 3U) {
		other.hit_damage_next--;
	}
	if(
		static_cast<int>(player.halfhearts) - static_cast<int>(damage) <= 0 &&
		player.halfhearts > 1U
	) {
		damage = static_cast<std::uint8_t>(player.halfhearts - 1U);
	}
	if(player_id == 0U && state.skill > 0U) {
		state.skill--;
	}
	return damage;
}

void player_update_finish(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint16_t live_input
) noexcept
{
	auto &player = state.players[player_id];
	bool acted = false;
	if(player.knockback_active) {
		player_knockback_update(state, player_id);
		player.gauge_charged = 0U;
		player.cpu_frame = 0U;
		acted = true;
	} else if(
		state.autofire_shot_gate[player_id] <= 2U &&
		player.hyper_active == 0U
	) {
		if(player.gauge_charged < player.gauge_avail) {
			const auto charged = static_cast<std::uint16_t>(
				player.gauge_charged + player.gauge_charge_speed
			);
			if(charged > player.gauge_avail) {
				player.gauge_charged = player.gauge_avail;
				player.spell_ready_frames = 0U;
			} else {
				player.gauge_charged = charged;
			}
		} else {
			player.spell_ready_frames++;
			if(player.spell_ready_frames >= 128U) {
				acted = (player.gauge_charged >= gauge_64);
				release_gauge(state, player_id);
			}
		}
	} else if(state.autofire_shot_gate[player_id] <= 4U) {
		acted = (player.gauge_charged >= gauge_64);
		release_gauge(state, player_id);
	}

	if(player.shot_active == 2U) {
		live_input = 0U;
	}
	if(player.move_lock_time == 0U && player.shot_active != 0U) {
		bool shot_fired = false;
		if(
			(live_input & input_shot) != 0U || player.shot_active == 2U
		) {
			if(state.autofire_shot_gate[player_id] > 2U) {
				shots_add(state, player_id);
				state.autofire_shot_gate[player_id] = 0U;
				shot_fired = true;
			}
		}
		if(
			!shot_fired && (live_input & input_shot) == 0U &&
			state.autofire_shot_gate[player_id] < 8U
		) {
			state.autofire_shot_gate[player_id]++;
		}
		if((live_input & input_bomb) != 0U) {
			player_bomb(state, player_id);
		}
		if(player.shot_active == 2U) {
			player.shot_active = 1U;
		}
	}

	if(player.hyper_active != 0U) {
		if(player.gauge_avail > gauge_64) {
			player.gauge_avail = static_cast<std::uint16_t>(
				player.gauge_avail - 6U
			);
		} else {
			player.hyper_active = 0U;
			player.gauge_avail = gauge_64;
		}
		acted = true;
	}
	if(state.defeat_flag != DefeatFlag::none || acted) {
		return;
	}
	std::int16_t collision_x = 0;
	std::int16_t collision_y = 0;
	player.is_hit = player_hittest(
		state, player_id, 4U, collision_x, collision_y
	);
	if(!player.is_hit) {
		return;
	}
	if(player.halfhearts != 0U) {
		const auto damage = players_hit_damage_update(state, player_id);
		player.halfhearts = (damage > player.halfhearts)
			? 0U
			: static_cast<std::uint8_t>(player.halfhearts - damage);
		if(player.halfhearts == 0U) {
			player.lose_anim_time = 0x30U;
			state.last_fatal_player = player_id;
		} else {
			player.knockback_active = true;
			player.knockback_time = knockback_frames;
			player.invincibility_time = miss_invincibility_frames;
			player.move_lock_time = knockback_frames;
		}
	}
	player.is_hit = false;
}

void player_update_human(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint16_t live_input
) noexcept
{
	auto &player = state.players[player_id];
	if(!player_update_begin(state, player_id)) {
		return;
	}
	if(player.move_lock_time != 0U) {
		player.move_lock_time--;
	} else {
		move_human(player, live_input);
	}
	player_update_finish(state, player_id, live_input);
}

std::uint16_t cpu_initial_avoidance_input(
	const PlayerState &player,
	std::int16_t collision_x,
	std::int16_t collision_y
) noexcept
{
	const auto delta_x = static_cast<std::int16_t>(
		collision_x - player.center_x.raw()
	);
	const auto delta_y = static_cast<std::int16_t>(
		collision_y - player.center_y.raw()
	);
	std::size_t row = 0U;
	if(delta_x <= -(16 << 4)) {
		row = 0U;
	} else if(delta_x <= (16 << 4)) {
		row = 1U;
	} else {
		row = 2U;
	}
	row += static_cast<std::size_t>(player.cpu_dodge_strategy) * 3U;
	row = std::min<std::size_t>(row, cpu_collision_routes.size() - 1U);

	std::size_t direction = 0U;
	if(delta_y < -(16 << 4)) {
		if(player.center_x.raw() >= (240 << 4)) {
			if(player.center_y.raw() < (128 << 4)) {
				return input_left;
			}
			direction = 0U;
		} else if(player.center_x.raw() >= (16 << 4)) {
			direction = 1U;
		} else {
			if(player.center_y.raw() < (128 << 4)) {
				return input_right;
			}
			direction = 2U;
		}
	} else if(delta_y < (32 << 4)) {
		direction = (player.center_x.raw() >= (240 << 4))
			? 3U : ((player.center_x.raw() >= (16 << 4)) ? 4U : 5U);
	} else {
		direction = (player.center_x.raw() >= (240 << 4))
			? 6U : ((player.center_x.raw() >= (16 << 4)) ? 7U : 8U);
	}
	auto input = cpu_collision_routes[row][direction];
	if(
		player.center_y.raw() >= (304 << 4) && input == input_down
	) {
		input = (player.center_x.raw() >= (128 << 4)) ? input_left : input_right;
	}
	return input;
}

std::uint16_t cpu_target_movement(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto &player = state.players[player_id];
	const auto target = cpu_target_point(state, player_id);
	std::uint8_t target_flags = static_cast<std::uint8_t>(
		exact::wrap_add(target.x, static_cast<std::int16_t>(16 << 4)) <
		player.center_x.raw()
	);
	target_flags = static_cast<std::uint8_t>(target_flags | (
		static_cast<std::uint8_t>(
			exact::wrap_subtract(
				target.x, static_cast<std::int16_t>(16 << 4)
			) > player.center_x.raw()
		) << 1U
	));
	target_flags = static_cast<std::uint8_t>(target_flags | (
		static_cast<std::uint8_t>(
			exact::wrap_add(target.y, static_cast<std::int16_t>(80 << 4)) <
			player.center_y.raw()
		) << 2U
	));
	target_flags = static_cast<std::uint8_t>(target_flags | (
		static_cast<std::uint8_t>(player.center_y.raw() < target.y) << 3U
	));
	switch(target_flags) {
	case 1U: return input_left;
	case 2U: return input_right;
	case 4U: return input_up;
	case 8U: return input_down;
	case 5U: return input_up | input_left;
	case 9U: return input_down | input_left;
	case 6U: return input_up | input_right;
	case 10U: return input_down | input_right;
	default: return 0U;
	}
}

bool cpu_safe_move(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &player = state.players[player_id];
	std::int16_t collision_x = 0;
	std::int16_t collision_y = 0;
	bool broad_hit = false;
	if((state.round_or_result_frame & 3U) == 0U) {
		for(std::uint8_t size = 32U; size <= 80U; size += 16U) {
			const auto hit = player_hittest(
				state, player_id, size, collision_x, collision_y
			);
			if(hit) {
				broad_hit = true;
				break;
			}
		}
	}
	auto input = broad_hit
		? cpu_initial_avoidance_input(player, collision_x, collision_y)
		: cpu_target_movement(state, player_id);
	const auto center_x = player.center_x;
	const auto center_y = player.center_y;
	std::uint8_t route_attempt = 0U;
	std::uint8_t fallback_attempt = 0U;
	std::size_t route_group = 0U;
	for(;;) {
		Velocity velocity{};
		if(movement_velocity(player, input, velocity) == MoveResult::valid) {
			apply_velocity(player, velocity);
		}
		const auto hitbox_size = static_cast<std::uint8_t>(
			(fallback_attempt == 0U) ? 20U : 4U
		);
		const auto route_hit = player_hittest(
			state, player_id, hitbox_size, collision_x, collision_y
		);
		if(!route_hit) {
			return broad_hit;
		}
		player.center_x = center_x;
		player.center_y = center_y;
		route_attempt++;
		if(route_attempt < 9U) {
			route_group = (collision_x < (80 << 4))
				? 0U : ((collision_x < (176 << 4)) ? 1U : 2U);
			input = cpu_retry_routes[route_group][route_attempt];
			continue;
		}
		if(fallback_attempt < 9U) {
			input = cpu_retry_routes[route_group][fallback_attempt];
			fallback_attempt++;
			continue;
		}
		if(player.gauge_charged <= gauge_64) {
			player_bomb(state, player_id);
		} else {
			player.is_hit = true;
		}
		return broad_hit;
	}
}

void player_update_cpu(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &player = state.players[player_id];
	auto live_input = std::uint16_t{0};
	if(!player_update_begin(state, player_id)) {
		return;
	}
	if(player.move_lock_time != 0U) {
		player.move_lock_time--;
		player_update_finish(state, player_id, live_input);
		return;
	}

	player.cpu_frame++;
	if(player.cpu_frame <= player.cpu_safety_frames) {
		// A broad collision makes sub_C568() skip target selection. The shared
		// target scratch therefore retains the value written by an earlier
		// callback in this frame.
		(void)cpu_safe_move(state, player_id);
	} else {
		const auto movement = cpu_target_movement(state, player_id);
		(void)(randring_next16(state) % 3U);
		Velocity velocity{};
		if(movement_velocity(player, movement, velocity) == MoveResult::valid) {
			apply_velocity(player, velocity);
		}
		std::int16_t collision_x = 0;
		std::int16_t collision_y = 0;
		const auto small_hit = player_hittest(
			state, player_id, 4U, collision_x, collision_y
		);
		if(small_hit) {
			if(player.gauge_charged > (80U << 4U)) {
				player.is_hit = true;
			} else if((randring_next16(state) & 3U) == 0U) {
				player_bomb(state, player_id);
			} else {
				player.is_hit = true;
			}
		}
	}

	std::uint16_t cadence = 10U;
	if(state.cpu_target_kind_scratch == 0) {
		cadence = 6U;
	} else if(state.cpu_target_kind_scratch == -1) {
		cadence = 32U;
	} else if(state.cpu_target_kind_scratch == -2) {
		cadence = 7U;
	}
	if((state.round_or_result_frame % cadence) == 0U) {
		state.autofire_shot_gate[player_id] = 4U;
		live_input = input_shot;
	}
	state.cpu_target_kind_scratch = 0;
	if(player.is_hit) {
		player.is_hit = false;
		state.autofire_shot_gate[player_id] = 4U;
		live_input = 0U;
	} else if(!(
		player.bombs == 0U &&
		static_cast<std::int8_t>(player.halfhearts) <= 3
	)) {
		if(
			player.cpu_charge_ring[player.cpu_charge_cursor] <
				(player.gauge_avail >> 4U) ||
			player.gauge_avail == gauge_max
		) {
			state.autofire_shot_gate[player_id] = 0U;
			live_input = input_shot;
			if(player.gauge_charged >= player.gauge_avail) {
				state.autofire_shot_gate[player_id] = 4U;
				live_input = 0U;
				player.cpu_charge_cursor = static_cast<std::uint8_t>(
					(player.cpu_charge_cursor + 1U) % 64U
				);
			}
		}
	}

	player_update_finish(state, player_id, live_input);
}
