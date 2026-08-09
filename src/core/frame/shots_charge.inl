// Included by ../frame.cpp inside its private implementation namespace.

void add_reimu_hyper_amulet(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept;

void shots_add(DeterministicState &state, std::uint8_t player_id) noexcept
{
	const auto &player = state.players[player_id];
	if(player.shot_mode == 0U) {
		return;
	}
	std::int16_t left = player.center_x.raw();
	std::int16_t top = player.center_y.raw();
	std::uint8_t pairs_fired = 0U;
	if(player.shot_mode == 3U) {
		left = exact::wrap_subtract(left, static_cast<std::int16_t>(64 << 4));
	} else if(player.shot_mode == 2U) {
		left = exact::wrap_subtract(left, static_cast<std::int16_t>(32 << 4));
		pairs_fired = 2U;
	} else if(player.shot_mode == 4U) {
		left = exact::wrap_subtract(left, static_cast<std::int16_t>(24 << 4));
		top = exact::wrap_subtract(top, static_cast<std::int16_t>(1 << 4));
		add_reimu_hyper_amulet(state, player_id, left, top);
		left = exact::wrap_add(left, static_cast<std::int16_t>(48 << 4));
		add_reimu_hyper_amulet(state, player_id, left, top);
		left = exact::wrap_subtract(
			player.center_x.raw(), static_cast<std::int16_t>(16 << 4)
		);
		pairs_fired = 3U;
	} else {
		left = exact::wrap_subtract(left, static_cast<std::int16_t>(16 << 4));
		pairs_fired = 3U;
	}
	while(left <= -(32 << 4)) {
		left = exact::wrap_add(left, static_cast<std::int16_t>(32 << 4));
		pairs_fired++;
	}
	for(auto &slot : state.shotpairs) {
		if(slot.alive) {
			continue;
		}
		slot.alive = true;
		slot.topleft_x = left;
		slot.topleft_y = top;
		slot.velocity_y = shot_velocity_y;
		slot.player_id = player_id;
		pairs_fired++;
		if(pairs_fired >= 4U) {
			return;
		}
		left = exact::wrap_add(left, static_cast<std::int16_t>(32 << 4));
		if(left >= (288 << 4)) {
			return;
		}
	}
}

void shots_update(DeterministicState &state) noexcept
{
	for(auto &shot : state.shotpairs) {
		if(!shot.alive) {
			continue;
		}
		shot.topleft_y = exact::wrap_add(shot.topleft_y, shot.velocity_y);
		if(shot.topleft_y <= -16) {
			shot.alive = false;
		}
	}
}

bool playfield_clip(
	std::int16_t x,
	std::int16_t y,
	std::int16_t negative_radius
) noexcept
{
	return (
		x <= negative_radius ||
		x >= exact::wrap_subtract(
			static_cast<std::int16_t>(288 << 4), negative_radius
		) ||
		y <= negative_radius ||
		y >= exact::wrap_subtract(
			static_cast<std::int16_t>(368 << 4), negative_radius
		)
	);
}

struct CpuTargetPoint {
	std::int16_t x = 144 << 4;
	std::int16_t y = 300 << 4;
};

CpuTargetPoint cpu_target_point(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	CpuTargetPoint target{};
	std::uint8_t target_id = 0xFFU;
	if(state.boss_target_active[player_id] != 0U) {
		target_id = 0xFEU;
		target.x = state.boss_target_x;
		target.y = state.boss_target_y;
	} else {
		for(const auto &enemy : state.enemies) {
			if(
				enemy.flag != EnemyFlag::running_spawned ||
				enemy.player_id != player_id ||
				enemy.center_x < 0 || enemy.center_x > (288 << 4) ||
				enemy.center_y < 0 || enemy.center_y > (384 << 4) ||
				enemy.formation_index >= target_id
			) {
				continue;
			}
			target_id = enemy.formation_index;
			target.x = static_cast<std::int16_t>(enemy.center_x - (16 << 4));
			target.y = static_cast<std::int16_t>(enemy.center_y + (110 << 4));
			if(target_id == 0U) {
				break;
			}
		}
	}
	state.cpu_target_kind_scratch = (target_id == 0xFFU)
		? static_cast<std::int8_t>(-1)
		: ((target_id == 0xFEU)
			? static_cast<std::int8_t>(-2)
			: static_cast<std::int8_t>(target_id));
	if(
		target.x <= 0 || target.x >= (288 << 4) ||
		target.y <= 0 || target.y >= (384 << 4)
	) {
		target.x = 144 << 4;
		target.y = 300 << 4;
	}
	return target;
}

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
	std::uint8_t player_id
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
			}
		}
		node.frame++;
	}
}

void chargeshot_update_ellen(
	DeterministicState &state,
	std::uint8_t player_id
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
	std::uint8_t player_id
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
			shot.state = 2U;
			shot.frame = 0U;
		}
	} else if(frame >= 0x40U) {
		shot.state = 0U;
	}
	state.players[player_id].gauge_charged = 0U;
}

void chargeshots_update(DeterministicState &state) noexcept
{
	for(std::uint8_t player_id = 0U; player_id < player_count; ++player_id) {
		const auto character =
			(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
		if(character == 0) {
			chargeshot_update_reimu(state, player_id);
		} else if(character == 1) {
			chargeshot_update_mima(state, player_id);
		} else if(character == 2) {
			chargeshot_update_marisa(state, player_id);
		} else if(character == 3) {
			chargeshot_update_ellen(state, player_id);
		} else if(character == 4) {
			chargeshot_update_kotohime(state, player_id);
		} else if(character == 5) {
			chargeshot_update_kana(state, player_id);
		} else if(character == 6) {
			chargeshot_update_rikako(state, player_id);
		} else if(character == 7) {
			chargeshot_update_chiyuri(state, player_id);
		} else if(character == 8) {
			chargeshot_update_yumemi(state, player_id);
		}
	}
}

std::uint8_t chargeshot_hittest(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t left,
	std::int16_t right,
	std::int16_t top,
	std::int16_t bottom,
	bool *one_hit = nullptr
) noexcept
{
	if(one_hit != nullptr) {
		*one_hit = false;
	}
	const auto character =
		(static_cast<int>(state.players[player_id].paletted_character) - 1) / 2;
	if(character == 0) {
		if(
			state.reimu_chargeshot_primary != 0U &&
			(state.round_or_result_frame & 1U) != 0U
		) {
			return 0U;
		}
		for(std::size_t group_id = 0U; group_id < 6U; ++group_id) {
			const auto &group = state.reimu_charge[player_id][group_id];
			if(
				group.state == 0U || group.x[0] <= -(16 << 4) ||
				group.x[0] >= (288 << 4) || group.y[0] <= -(16 << 4)
			) {
				continue;
			}
			if(
				exact::wrap_subtract(group.x[0], right) <= (20 << 4) &&
				exact::wrap_subtract(left, group.x[0]) <= (20 << 4) &&
				exact::wrap_subtract(group.y[0], bottom) <= (20 << 4) &&
				exact::wrap_subtract(top, group.y[0]) <= (20 << 4)
			) {
				// hitcircles_enemy_add() randomly centers the visual inside this
				// hitbox. Those two presentation-owned draws share the gameplay
				// ring and therefore remain part of deterministic call order.
				(void)randring_next16(state);
				(void)randring_next16(state);
				return 1U;
			}
		}
		return 0U;
	}
	if(character == 1) {
		if(state.mima_charge_active[player_id] == 0U) {
			return 0U;
		}
		std::uint8_t hits = 0U;
		for(const auto &node : state.mima_charge[player_id]) {
			if(
				node.active == 0U ||
				exact::wrap_add(node.x, std::int16_t{-(16 << 4)}) > right ||
				exact::wrap_add(node.x, std::int16_t{16 << 4}) < left ||
				node.y < top || node.y > bottom
			) {
				continue;
			}
			(void)randring_next16(state);
			(void)randring_next16(state);
			hits++;
		}
		return hits;
	}
	if(character == 2) {
		const auto &shot = state.marisa_charge[player_id];
		if(shot.active == 0U) {
			return 0U;
		}
		for(std::size_t node = 0U; node < shot.x.size(); node += 4U) {
			if(
				shot.x[node] >= left && shot.x[node] <= right &&
				shot.y[node] >= top
			) {
				(void)randring_next16(state);
				(void)randring_next16(state);
				return 1U;
			}
		}
		return 0U;
	}
	if(character == 3) {
		std::uint8_t hits = 0U;
		for(auto &node : state.ellen_charge[player_id]) {
			if(
				node.state != 1U || node.x < left || node.x > right ||
				node.y < top || node.y > bottom
			) {
				continue;
			}
			(void)randring_next16(state);
			(void)randring_next16(state);
			node.state = 0U;
			hits = static_cast<std::uint8_t>(hits + 2U);
		}
		return hits;
	}
	if(character == 4) {
		const auto &shot = state.kotohime_charge[player_id];
		if(
			shot.active != 0U &&
			exact::wrap_subtract(shot.x, right) <= (40 << 4) &&
			exact::wrap_subtract(left, shot.x) <= (40 << 4) &&
			shot.y >= top && shot.y <= bottom
		) {
			(void)randring_next16(state);
			(void)randring_next16(state);
			if(one_hit != nullptr) {
				*one_hit = true;
			}
			return 3U;
		}
		return 0U;
	}
	if(character == 5 && state.kana_charge_state[player_id] != 0U) {
		for(const auto &group : state.kana_charge[player_id]) {
			for(std::size_t point = 0U; point <= 12U; point += 4U) {
				if(
					exact::wrap_subtract(group.x[point], right) <= (12 << 4) &&
					exact::wrap_subtract(left, group.x[point]) <= (12 << 4) &&
					exact::wrap_subtract(group.y[point], bottom) <= (12 << 4) &&
					exact::wrap_subtract(top, group.y[point]) <= (12 << 4)
				) {
					(void)randring_next16(state);
					(void)randring_next16(state);
					return 1U;
				}
			}
		}
	}
	if(character == 6 && state.rikako_charge_state[player_id] != 0U) {
		std::uint8_t hits = 0U;
		for(const auto &node : state.rikako_charge[player_id]) {
			if(
				exact::wrap_subtract(node.x, right) <= (12 << 4) &&
				exact::wrap_subtract(left, node.x) <= (12 << 4) &&
				exact::wrap_subtract(node.y, bottom) <= (12 << 4) &&
				exact::wrap_subtract(top, node.y) <= (12 << 4)
			) {
				(void)randring_next16(state);
				(void)randring_next16(state);
				hits++;
			}
		}
		return hits;
	}
	if(character == 7) {
		std::uint8_t hits = 0U;
		const auto &nodes = state.chiyuri_charge[player_id];
		for(std::size_t reverse_index = 0U; reverse_index < nodes.size(); ++reverse_index) {
			const auto &node = nodes[nodes.size() - 1U - reverse_index];
			if(node.state == 0U) {
				break;
			}
			if(node.state != 1U) {
				continue;
			}
			if(
				exact::wrap_subtract(node.x, right) <= (14 << 4) &&
				exact::wrap_subtract(left, node.x) <= (14 << 4) &&
				exact::wrap_subtract(node.y, bottom) <= (32 << 4) &&
				exact::wrap_subtract(top, node.y) <= -(16 << 4)
			) {
				(void)randring_next16(state);
				(void)randring_next16(state);
				hits++;
			}
		}
		return hits;
	}
	if(character == 8) {
		auto &shot = state.yumemi_charge[player_id];
		if(shot.state == 0U) {
			return 0U;
		}
		const auto overlaps = [&](std::int16_t radius) {
			return !(
				exact::wrap_subtract(shot.x, radius) > right ||
				exact::wrap_add(shot.x, radius) < left ||
				exact::wrap_subtract(shot.y, radius) > bottom ||
				exact::wrap_add(shot.y, radius) < top
			);
		};
		if(shot.state == 1U) {
			if(overlaps(8 << 4)) {
				// ZUN turns the projectile into its large explosion but reports a
				// miss to this caller on the triggering collision frame.
				shot.state = 2U;
				shot.frame = 0U;
			}
			return 0U;
		}
		if(!overlaps(56 << 4)) {
			return 0U;
		}
		// hitcircles_enemy_add() consumes two shared-ring values to choose
		// the visual center. They remain replay-visible in the headless core.
		(void)randring_next16(state);
		(void)randring_next16(state);
		return 1U;
	}
	return 0U;
}
