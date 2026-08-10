// Included by frame.cpp inside its private implementation namespace.

bool player_hittest(
	const DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t hitbox_size,
	std::int16_t &collision_x,
	std::int16_t &collision_y
) noexcept;

void player_defeat_update(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &player = state.players[player_id];
	if(observer != nullptr) {
		observer->player_defeat_step(PlayerDefeatStep{
			.player = static_cast<PlayerId>(player_id),
			.lose_anim_time_before = player.lose_anim_time,
			.center_x = player.center_x.raw(),
			.center_y = player.center_y.raw(),
			.fg_shift_x_before_defeat_shake =
				state.playfield_fg_shift_x[player_id],
			.round_or_result_frame_before = state.round_or_result_frame,
		});
	}
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
	if(observer != nullptr) {
		const auto winner_id = static_cast<std::uint8_t>(1U - player_id);
		observer->main_hud_player_value_changed(MainHudPlayerValueChanged{
			.player = static_cast<PlayerId>(winner_id),
			.kind = MainHudPlayerValueKind::rounds_won,
			.value_after = state.players[winner_id].rounds_won,
		});
	}
}

bool player_update_begin(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
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
			player_defeat_update(state, player_id, observer);
		}
		return false;
	}

	player.hyper_effect_active_this_frame = false;
	if(!player.hyper_character_callback) {
		player.shot_mode = shot_mode_one_pair;
		if(player.hyper_active != 0U) {
			player.hyper_character_callback = true;
		}
	} else if(player.hyper_active == 0U) {
		const auto character = character_index_from_paletted(
			player.paletted_character
		);
		if(character == character_rikako) {
			state.rikako_charge_state[player_id] = 0U;
		}
		player.hyper_character_callback = false;
	} else {
		player.hyper_effect_active_this_frame = true;
		const auto character = character_index_from_paletted(
			player.paletted_character
		);
		if(character == character_reimu) {
			player.shot_mode = shot_mode_reimu_hyper;
		} else if(character == character_mima) {
			player.shot_mode = shot_mode_two_pairs;
			player.shot_active = 2U;
		} else if(character == character_marisa) {
			player.shot_mode = shot_mode_none;
			activate_marisa_hyper(state, player_id);
		} else if(character == character_ellen) {
			player.shot_mode = shot_mode_one_pair;
			if((state.round_frame & 3U) == 0U) {
				add_ellen_hyper_node(state, player_id);
			}
		} else if(
			character == character_kotohime ||
			character == character_kana ||
			character == character_chiyuri ||
			character == character_yumemi
		) {
			player.shot_mode = shot_mode_three_pairs;
		} else if(
			character == character_rikako &&
			player.gauge_avail > (250U << 4U)
		) {
			add_rikako_chargeshot(state, player_id);
			state.rikako_charge_state[player_id] = 2U;
		}
	}
	return true;
}

std::optional<std::int8_t> player_knockback_update(
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
		player.knockback_time--;
		return source_velocity_x(velocity);
	} else if(player.knockback_time == 0U) {
		player.knockback_active = false;
	}
	player.knockback_time--;
	return std::nullopt;
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
	std::uint16_t live_input,
	std::optional<std::int8_t> &final_velocity_x_write,
	GameplayObserver *observer
) noexcept
{
	auto &player = state.players[player_id];
	bool acted = false;
	if(player.knockback_active) {
		const auto knockback_write = player_knockback_update(state, player_id);
		if(knockback_write) {
			final_velocity_x_write = knockback_write;
		}
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
				shots_add(state, player_id, observer);
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
			player_bomb(state, player_id, observer);
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
		observe_hit_circle_exact(
			state, observer, HitCircleKind::player,
			player.center_x.raw(), player.center_y.raw(), player_id
		);
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
		if(observer != nullptr) {
			observer->main_hud_player_value_changed(
				MainHudPlayerValueChanged{
					.player = static_cast<PlayerId>(player_id),
					.kind = MainHudPlayerValueKind::halfhearts,
					.value_after = player.halfhearts,
				}
			);
		}
	}
	player.is_hit = false;
}

void player_update_human(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint16_t live_input,
	GameplayObserver *observer
) noexcept
{
	auto &player = state.players[player_id];
	std::optional<std::int8_t> final_velocity_x_write;
	if(!player_update_begin(state, player_id, observer)) {
		if(observer != nullptr) {
			observer->player_pose_committed(PlayerPoseCommitted{
				.player = static_cast<PlayerId>(player_id),
				.final_velocity_x_write_q4_4 = std::nullopt,
			});
		}
		return;
	}
	if(player.move_lock_time != 0U) {
		player.move_lock_time--;
	} else {
		final_velocity_x_write = move_human(player, live_input);
	}
	player_update_finish(
		state, player_id, live_input, final_velocity_x_write, observer
	);
	if(observer != nullptr) {
		observer->player_pose_committed(PlayerPoseCommitted{
			.player = static_cast<PlayerId>(player_id),
			.final_velocity_x_write_q4_4 = final_velocity_x_write,
		});
	}
}
