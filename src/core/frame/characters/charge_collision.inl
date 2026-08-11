// Included by frame.cpp inside its private implementation namespace.

std::uint8_t chargeshot_hittest(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t left,
	std::int16_t right,
	std::int16_t top,
	std::int16_t bottom,
	GameplayObserver *observer,
	bool *one_hit = nullptr
) noexcept
{
	if(one_hit != nullptr) {
		*one_hit = false;
	}
	const auto character = character_index_from_paletted(
		state.players[player_id].paletted_character
	);
	if(character == character_reimu) {
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
				observe_hit_circle_random(
					state, observer, left, right, top, bottom, player_id
				);
				return 1U;
			}
		}
		return 0U;
	}
	if(character == character_mima) {
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
			observe_hit_circle_random(
				state, observer, left, right, top, bottom, player_id
			);
			hits++;
		}
		return hits;
	}
	if(character == character_marisa) {
		const auto &shot = state.marisa_charge[player_id];
		if(shot.active == 0U) {
			return 0U;
		}
		for(std::size_t node = 0U; node < shot.x.size(); node += 4U) {
			if(
				shot.x[node] >= left && shot.x[node] <= right &&
				shot.y[node] >= top
			) {
				observe_hit_circle_random(
					state, observer, left, right, top, bottom, player_id
				);
				return 1U;
			}
		}
		return 0U;
	}
	if(character == character_ellen) {
		std::uint8_t hits = 0U;
		for(auto &node : state.ellen_charge[player_id]) {
			if(
				node.state != 1U || node.x < left || node.x > right ||
				node.y < top || node.y > bottom
			) {
				continue;
			}
			observe_hit_circle_random(
				state, observer, left, right, top, bottom, player_id
			);
			node.state = 0U;
			hits = static_cast<std::uint8_t>(hits + 2U);
		}
		return hits;
	}
	if(character == character_kotohime) {
		const auto &shot = state.kotohime_charge[player_id];
		if(
			shot.active != 0U &&
			exact::wrap_subtract(shot.x, right) <= (40 << 4) &&
			exact::wrap_subtract(left, shot.x) <= (40 << 4) &&
			shot.y >= top && shot.y <= bottom
		) {
			observe_hit_circle_random(
				state, observer, left, right, top, bottom, player_id
			);
			if(one_hit != nullptr) {
				*one_hit = true;
			}
			return 3U;
		}
		return 0U;
	}
	if(character == character_kana && state.kana_charge_state[player_id] != 0U) {
		for(const auto &group : state.kana_charge[player_id]) {
			for(std::size_t point = 0U; point <= 12U; point += 4U) {
				if(
					exact::wrap_subtract(group.x[point], right) <= (12 << 4) &&
					exact::wrap_subtract(left, group.x[point]) <= (12 << 4) &&
					exact::wrap_subtract(group.y[point], bottom) <= (12 << 4) &&
					exact::wrap_subtract(top, group.y[point]) <= (12 << 4)
				) {
					observe_hit_circle_random(
						state, observer, left, right, top, bottom, player_id
					);
					return 1U;
				}
			}
		}
	}
	if(character == character_rikako && state.rikako_charge_state[player_id] != 0U) {
		std::uint8_t hits = 0U;
		for(const auto &node : state.rikako_charge[player_id]) {
			if(
				exact::wrap_subtract(node.x, right) <= (12 << 4) &&
				exact::wrap_subtract(left, node.x) <= (12 << 4) &&
				exact::wrap_subtract(node.y, bottom) <= (12 << 4) &&
				exact::wrap_subtract(top, node.y) <= (12 << 4)
			) {
				observe_hit_circle_random(
					state, observer, left, right, top, bottom, player_id
				);
				hits++;
			}
		}
		return hits;
	}
	if(character == character_chiyuri) {
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
				observe_hit_circle_random(
					state, observer, left, right, top, bottom, player_id
				);
				hits++;
			}
		}
		return hits;
	}
	if(character == character_yumemi) {
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
				observe_main_feedback(
					observer, MainFeedbackCueSite::yumemi_charge_detonated,
					player_id
				);
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
	observe_hit_circle_random(
		state, observer, left, right, top, bottom, player_id
	);
		return 1U;
	}
	return 0U;
}
