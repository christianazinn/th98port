// Included by frame.cpp inside its private implementation namespace.

void chargeshot_update_reimu(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	for(std::size_t group_id = 0U; group_id < 12U; ++group_id) {
		auto &group = state.reimu_charge[player_id][group_id];
		if(group.state == 0U) {
			continue;
		}
		state.players[player_id].gauge_charged = 0U;
		for(std::size_t point = group.x.size() - 1U; point > 0U; --point) {
			group.x[point] = group.x[point - 1U];
			group.y[point] = group.y[point - 1U];
		}
		const auto velocity = vector2(group.angle, group.speed);
		group.x[0] = exact::wrap_add(group.x[0], velocity.x);
		group.y[0] = exact::wrap_add(group.y[0], velocity.y);
		if(group.state == 1U) {
			if(group.speed > 0U) {
				group.speed = static_cast<std::uint8_t>(group.speed - 4U);
			} else if(group.frame >= 0x40U) {
				group.state = 2U;
				group.angle = static_cast<std::uint8_t>(
					(group_id < 2U) ? 0xB0U : 0xD0U
				);
				group.speed = 0xE0U;
			}
		}
		group.frame++;
		if(playfield_clip(group.x.back(), group.y.back(), -(24 << 4))) {
			group.state = 0U;
		}
	}
}

void chargeshot_update_mima(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	if(state.mima_charge_active[player_id] == 0U) {
		return;
	}
	auto &frames = state.mima_charge_frames[player_id];
	frames++;
	auto &nodes = state.mima_charge[player_id];
	for(std::size_t node_id = 0U; node_id < nodes.size(); ++node_id) {
		auto &node = nodes[node_id];
		if(node.active == 0U) {
			continue;
		}
		node.x = exact::wrap_add(node.x, node.velocity_x);
		node.y = exact::wrap_add(node.y, node.velocity_y);
		if(playfield_clip(node.x, node.y, -(16 << 4))) {
			node.active = 0U;
			// The last node clears the global flag even if earlier nodes remain.
			if(node_id == (nodes.size() - 1U)) {
				state.mima_charge_active[player_id] = 0U;
			}
		}
	}
	if(frames < 0x3CU && (frames % 4U) == 0U) {
		auto &node = nodes[frames / 4U];
		node.active = 1U;
		node.x = state.players[player_id].center_x.raw();
		node.y = state.players[player_id].center_y.raw();
	}
	state.players[player_id].gauge_charged = 0U;
}

void chargeshot_update_marisa(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.marisa_charge[player_id];
	if(shot.active == 0U) {
		return;
	}
	auto &player = state.players[player_id];
	player.gauge_charged = 0U;
	shot.age++;
	for(std::size_t node = shot.x.size() - 1U; node > 0U; --node) {
		shot.x[node] = shot.x[node - 1U];
		shot.y[node] = exact::wrap_add(
			shot.y[node - 1U], std::int16_t{0x18}
		);
	}
	if(shot.age < 0x18U) {
		shot.x[0] = player.center_x.raw();
		shot.y[0] = exact::wrap_subtract(
			player.center_y.raw(), std::int16_t{0x100}
		);
		return;
	}
	if(shot.age > 0x24U) {
		player.shot_active = 1U;
		shot.active = 0U;
	}
}

void chargeshot_update_kana(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto charge_state = state.kana_charge_state[player_id];
	if(charge_state == 0U) {
		return;
	}
	state.players[player_id].gauge_charged = 0U;
	for(auto &group : state.kana_charge[player_id]) {
		const auto velocity = vector2(group.angle, group.speed);
		for(std::size_t point = group.x.size() - 1U; point > 0U; --point) {
			group.x[point] = group.x[point - 1U];
			group.y[point] = group.y[point - 1U];
		}
		group.x[0] = exact::wrap_add(group.x[0], velocity.x);
		group.y[0] = exact::wrap_add(group.y[0], velocity.y);
		if(charge_state == 1U) {
			group.speed--;
		}
	}
	if(charge_state == 1U) {
		state.kana_charge[player_id][3].angle = static_cast<std::uint8_t>(
			state.kana_charge[player_id][3].angle - 2U
		);
		state.kana_charge[player_id][0].angle = static_cast<std::uint8_t>(
			state.kana_charge[player_id][0].angle + 2U
		);
		if(state.kana_charge_frames[player_id] > 0x28U) {
			state.kana_charge_state[player_id] = 2U;
			for(auto &group : state.kana_charge[player_id]) {
				group.speed = 0x80U;
			}
		}
	}
	if(state.kana_charge[player_id][0].y.back() <= -0x100) {
		state.kana_charge_state[player_id] = 0U;
	}
	state.kana_charge_frames[player_id]++;
}

void chargeshot_update_rikako(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto charge_state = state.rikako_charge_state[player_id];
	if(charge_state == 0U) {
		return;
	}
	state.players[player_id].gauge_charged = 0U;
	const auto frame = state.rikako_charge_frames[player_id];
	auto radius = state.rikako_charge_radius[player_id];
	if(charge_state == 1U) {
		state.rikako_charge_origin_y[player_id] = exact::wrap_subtract(
			state.rikako_charge_origin_y[player_id], std::int16_t{0x20}
		);
	} else {
		state.rikako_charge_origin_x[player_id] =
			state.players[player_id].center_x.raw();
		state.rikako_charge_origin_y[player_id] =
			state.players[player_id].center_y.raw();
	}

	for(std::size_t node_id = 0U; node_id < 4U; ++node_id) {
		auto &node = state.rikako_charge[player_id][node_id];
		const auto offset = vector2(
			node.angle, static_cast<std::int16_t>(radius)
		);
		node.x = exact::wrap_add(
			state.rikako_charge_origin_x[player_id], offset.x
		);
		node.y = exact::wrap_add(
			state.rikako_charge_origin_y[player_id], offset.y
		);
		const auto angle_delta = static_cast<std::uint8_t>(
			(node_id & 1U) != 0U ? 8U : 0xF8U
		);
		node.angle = static_cast<std::uint8_t>(node.angle + angle_delta);
	}
	if(frame < 0x20U) {
		radius = static_cast<std::uint16_t>(radius + 0x20U);
	} else if(charge_state == 1U && frame > 0x80U) {
		radius = static_cast<std::uint16_t>(radius + 0x60U);
		if(frame > 144U) {
			state.rikako_charge_state[player_id] = 0U;
		}
	} else if(charge_state != 1U) {
		state.rikako_charge_frames[player_id] = 0x20U;
	}
	state.rikako_charge_radius[player_id] = radius;
	state.rikako_charge_frames[player_id]++;
}

void chargeshot_update_chiyuri(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &nodes = state.chiyuri_charge[player_id];
	if(nodes.back().state == 0U) {
		return;
	}
	state.players[player_id].gauge_charged = 0U;
	for(std::size_t reverse_index = 0U; reverse_index < nodes.size(); ++reverse_index) {
		auto &node = nodes[nodes.size() - 1U - reverse_index];
		if(node.state == 0U) {
			return;
		}
		if(node.state == 1U) {
			node.y = exact::wrap_subtract(node.y, std::int16_t{14 << 4});
			if(node.y < -0x300) {
				if(reverse_index == 0U) {
					state.players[player_id].shot_active = 1U;
				}
				node.state = 0U;
				continue;
			}
		} else {
			node.state--;
			if(node.state == 1U) {
				node.x = state.players[player_id].center_x.raw();
				node.y = state.players[player_id].center_y.raw();
				observe_main_feedback(
					observer,
					MainFeedbackCueSite::chiyuri_chargeshot_activated,
					player_id
				);
			}
		}
		node.frame++;
	}
}

void chargeshot_update_ellen(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &nodes = state.ellen_charge[player_id];
	if(std::none_of(nodes.begin(), nodes.end(), [](const auto &node) {
		return node.state != 0U;
	})) {
		return;
	}
	const auto target = cpu_target_point(state, player_id);
	const auto target_y = exact::wrap_subtract(
		target.y, static_cast<std::int16_t>(110 << 4)
	);
	auto &player = state.players[player_id];
	player.gauge_charged = 0U;
	const auto player_x = player.center_x.raw();
	const auto player_y = player.center_y.raw();
	for(auto &node : nodes) {
		if(node.state == 0U) {
			continue;
		}
		if(node.state == 1U) {
			node.x = exact::wrap_add(node.x, node.velocity_x);
			node.y = exact::wrap_add(node.y, node.velocity_y);
			if(node.phase != 0U) {
				if(playfield_clip(node.x, node.y, -(8 << 4))) {
					node.state = 0U;
				}
				continue;
			}
			node.speed = static_cast<std::uint8_t>(node.speed - 2U);
			const auto velocity = vector2(
				node.angle, static_cast<std::int16_t>(node.speed)
			);
			node.velocity_x = velocity.x;
			node.velocity_y = velocity.y;
			if(node.speed != 0U) {
				continue;
			}
			node.phase = 1U;
			node.angle = iatan2(
				exact::wrap_subtract(target_y, node.y),
				exact::wrap_subtract(target.x, node.x)
			);
			const auto homing_velocity = vector2(node.angle, 160);
			node.velocity_x = homing_velocity.x;
			node.velocity_y = homing_velocity.y;
			continue;
		}
		node.state--;
		if(node.state == 1U) {
			observe_main_feedback(
				observer, MainFeedbackCueSite::shot_fired, player_id
			);
		}
		node.x = player_x;
		node.y = player_y;
	}
}

void chargeshot_update_kotohime(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.kotohime_charge[player_id];
	if(shot.active == 0U) {
		return;
	}
	state.players[player_id].gauge_charged = 0U;
	shot.y = exact::wrap_add(shot.y, shot.velocity_y);
	if(shot.y <= -0x100) {
		shot.active = 0U;
		return;
	}
	shot.velocity_y = exact::wrap_subtract(
		shot.velocity_y, std::int16_t{2}
	);
}

void chargeshot_update_yumemi(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	auto &shot = state.yumemi_charge[player_id];
	if(shot.state == 0U) {
		return;
	}
	shot.frame = static_cast<std::uint8_t>(shot.frame + 1U);
	const auto frame = shot.frame;
	if(shot.state == 1U) {
		shot.y = exact::wrap_subtract(shot.y, std::int16_t{0x40});
		if(frame >= 0x40U || shot.y <= 0) {
			observe_main_feedback(
				observer, MainFeedbackCueSite::yumemi_charge_detonated,
				player_id
			);
			shot.state = 2U;
			shot.frame = 0U;
		}
	} else {
		if(frame >= 0x18U && (frame & 1U) == 0U) {
			observe_main_feedback(
				observer,
				MainFeedbackCueSite::yumemi_charge_active_pulse,
				player_id
			);
		}
		if(frame >= 0x40U) {
			shot.state = 0U;
		}
	}
	state.players[player_id].gauge_charged = 0U;
}

void chargeshots_update(
	DeterministicState &state,
	GameplayObserver *observer
) noexcept
{
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		const auto character = character_index_from_paletted(
			state.players[player_id].paletted_character
		);
		if(character == character_reimu) {
			chargeshot_update_reimu(state, player_id);
		} else if(character == character_mima) {
			chargeshot_update_mima(state, player_id);
		} else if(character == character_marisa) {
			chargeshot_update_marisa(state, player_id);
		} else if(character == character_ellen) {
			chargeshot_update_ellen(state, player_id, observer);
		} else if(character == character_kotohime) {
			chargeshot_update_kotohime(state, player_id);
		} else if(character == character_kana) {
			chargeshot_update_kana(state, player_id);
		} else if(character == character_rikako) {
			chargeshot_update_rikako(state, player_id);
		} else if(character == character_chiyuri) {
			chargeshot_update_chiyuri(state, player_id, observer);
		} else if(character == character_yumemi) {
			chargeshot_update_yumemi(state, player_id, observer);
		}
	}
}
