// Included by ../frame.cpp inside its private implementation namespace.

void fireballs_update(DeterministicState &state) noexcept
{
	constexpr std::int16_t fireball_x_min = -(16 << 4);
	constexpr std::int16_t fireball_x_max = (288 + 16) << 4;
	constexpr std::int16_t fireball_y_max = (368 + 16) << 4;
	for(auto &fireball : state.fireballs) {
		if(fireball.flag == FireballFlag::free) {
			continue;
		}
		fireball.frame++;
		if(
			static_cast<std::uint8_t>(fireball.flag) >=
			static_cast<std::uint8_t>(FireballFlag::explosion_ignoring_enemies)
		) {
			if(fireball.frame == 14U) {
				fireball.flag = FireballFlag::explosion_hitting_enemies;
			} else if(fireball.frame == 32U) {
				fireball.flag = FireballFlag::explosion_ignoring_enemies;
			} else if(fireball.frame >= 36U) {
				fireball.flag = FireballFlag::free;
			}
			continue;
		}

		if(fireball.flag == FireballFlag::fall) {
			if(state.bomb_flag[fireball.player_id] != BombFlag::inactive) {
				fireball.flag = FireballFlag::free;
				continue;
			}
			const auto next_x = exact::wrap_add(
				fireball.center_x, fireball.velocity_x
			);
			if(next_x <= fireball_x_min || next_x >= fireball_x_max) {
				fireball.flag = FireballFlag::free;
				continue;
			}
			fireball.center_x = next_x;
			const auto next_y = exact::wrap_add(
				fireball.center_y, fireball.velocity_y
			);
			if(next_y >= fireball_y_max) {
				fireball.flag = FireballFlag::free;
				continue;
			}
			fireball.center_y = next_y;
			collmap_set_fireball(state, fireball);
			continue;
		}

		const auto next_x = exact::wrap_add(
			fireball.center_x, fireball.velocity_x
		);
		const auto reached = (fireball.player_id == 0U)
			? (next_x >= fireball.target_x_for_origin_player)
			: (next_x <= fireball.target_x_for_origin_player);
		if(reached) {
			fireball.center_x = fireball.target_x_for_target_player;
			fireball.center_y = 0;
			fireball.velocity_x = 0;
			fireball.velocity_y = fireball.fall_velocity_y;
			fireball.flag = FireballFlag::fall;
			fireball.player_id ^= 1U;
			continue;
		}
		fireball.center_x = next_x;
		fireball.center_y = exact::wrap_add(
			fireball.center_y, fireball.velocity_y
		);
	}
}

void fireballs_hittest(
	DeterministicState &state,
	GameplayObserver *observer
) noexcept
{
	if(state.defeat_flag == DefeatFlag::banner) {
		return;
	}
	// Anniversary initializes this once before the reverse fireball scan. A
	// later non-explosion collision therefore reuses the most recent explosion
	// chain slot, rather than resetting to the fallback for each fireball.
	std::uint8_t explosion_chain_slot = chain_ring_size - 1U;
	for(auto target_it = state.fireballs.rbegin();
		target_it != state.fireballs.rend(); ++target_it) {
		auto &target = *target_it;
		if(target.flag != FireballFlag::fall) {
			continue;
		}
		constexpr std::int16_t radius_x = 12 << 4;
		constexpr std::int16_t radius_y = 10 << 4;
		const auto center_y = exact::wrap_add(
			target.center_y, static_cast<std::int16_t>(3 << 4)
		);
		const auto left = exact::wrap_subtract(target.center_x, radius_x);
		const auto right = exact::wrap_add(target.center_x, radius_x);
		const auto top = exact::wrap_subtract(center_y, radius_y);
		const auto bottom = exact::wrap_add(center_y, radius_y);
		bool hit = false;
		bool explosion_collision = false;

		for(auto &shot : state.shotpairs) {
			if(!shot.alive || shot.player_id != target.player_id) {
				continue;
			}
			const auto shot_center_x = exact::wrap_add(
				shot.topleft_x, shotpair_half_width
			);
			const auto shot_center_y = exact::wrap_add(
				shot.topleft_y, shotpair_half_height
			);
			if(
				exact::wrap_subtract(shot_center_x, shotpair_half_width) > right ||
				exact::wrap_add(shot_center_x, shotpair_half_width) < left ||
				shot_center_y < top || shot_center_y > bottom
			) {
				continue;
			}
			shot.alive = false;
			observe_hit_circle_exact(
				state, observer, HitCircleKind::enemy,
				shot_center_x, shot_center_y, target.player_id
			);
			hit = true;
		}

		const auto against = target.variant_as_explosion_target;
		const auto against_value = static_cast<std::uint8_t>(against * 3U);
		for_each_explosion(state, [&](auto &explosion) {
			if(!explosion_overlaps(
				explosion, target.player_id, left, right, top, bottom
			)) {
				return;
			}
			explosion_collision = true;
			explosion_chain_slot = explosion.chain_slot;
			hit = true;
			auto &hits = state.chains.hits[target.player_id]
				[explosion_chain_slot];
			if(hits != 0xFFU) {
				hits++;
			}
			std::uint16_t bonus = 0U;
			if(against == 2U) {
				bonus = static_cast<std::uint16_t>(100U + (hits * 16U));
				score_add(state.score[target.player_id], 40U);
			} else {
				bonus = static_cast<std::uint16_t>(444U + (hits * 16U));
				score_add(state.score[target.player_id], 80U);
			}
			bonus = static_cast<std::uint16_t>(bonus + state.round_speed);
			if(state.round_speed <= (3U << 4U)) {
				bonus >>= 1U;
			} else if(state.round_speed >= (6U << 4U)) {
				bonus = static_cast<std::uint16_t>(bonus + bonus);
			}
			auto &value = state.chains.pellet_and_fireball_value
				[target.player_id][explosion_chain_slot];
			value = static_cast<std::uint8_t>(value + against_value);
			gauge_avail_add(
				state,
				target.player_id,
				static_cast<std::uint8_t>(against_value * 3U)
			);
			auto &fireball_charge = state.chains.charge_fireball
				[target.player_id][explosion_chain_slot];
			if(value < 4U) {
				fireball_charge = static_cast<std::uint8_t>(fireball_charge + 2U);
			} else if(value < 10U) {
				fireball_charge = static_cast<std::uint8_t>(fireball_charge + 5U);
			} else if(value < 20U) {
				fireball_charge = static_cast<std::uint8_t>(fireball_charge + 2U);
				state.chains.charge_exatt[target.player_id]
					[explosion_chain_slot]++;
			} else {
				state.chains.charge_exatt[target.player_id]
					[explosion_chain_slot] = static_cast<std::uint8_t>(
						state.chains.charge_exatt[target.player_id]
							[explosion_chain_slot] + 2U
					);
			}
			bonus = combo_add(state, target.player_id, hits, bonus);
			fire_point_based_boss_attack_or_panic(
				state,
				bonus,
				target.player_id,
				explosion_chain_slot
			);
			fireball_add_if_charged(
				state,
				target.player_id,
				explosion_chain_slot,
				explosion.center_x,
				explosion.center_y,
				FireballVariant::red
			);
		});

		bool charge_one_hit = false;
		const auto charge_damage = chargeshot_hittest(
			state, target.player_id, left, right, top, bottom, observer,
			&charge_one_hit
		);
		if(charge_damage != 0U) {
			hit = true;
		}
		if(
			state.bomb_flag[target.player_id] != BombFlag::inactive &&
			(state.round_or_result_frame & 1U) == 0U
		) {
			hit = true;
		}
		if(!hit) {
			continue;
		}

		if(target.hp == 0U || explosion_collision || charge_one_hit) {
			state.fireball_generation_previous = target.generation;
			if(explosion_collision) {
				target.flag = FireballFlag::explosion_ignoring_enemies;
				target.chain_slot = explosion_chain_slot;
				auto &extra_charge = state.chains.charge_exatt
					[target.player_id][explosion_chain_slot];
				if(against == 3U) {
					extra_charge++;
				}
				const auto threshold = static_cast<std::uint8_t>(
					7U - (state.round_speed / 32U)
				);
				if(extra_charge >= threshold) {
					extra_charge = 0U;
					extra_attack_add(
						state,
						target.center_x,
						target.center_y,
						target.player_id
					);
				}
				if(state.fireball_generation_previous < 4U) {
					(void)fireball_add(
						state,
						target.center_x,
						target.center_y,
						target.player_id,
						FireballVariant::red
					);
				}
			} else {
				const auto cursor = state.chains.cursor[target.player_id];
				state.chains.charge_exatt[target.player_id][cursor] =
					static_cast<std::uint8_t>(
						state.chains.charge_exatt[target.player_id][cursor] +
						(against - 1U)
					);
				target.flag = FireballFlag::free;
				chain_fire_charged_exatt(
					state,
					target.player_id,
					cursor,
					target.center_x,
					target.center_y
				);
				gauge_avail_add(state, target.player_id, 16U);
				score_add(state.score[target.player_id], 50U);
				(void)fireball_add(
					state,
					target.center_x,
					target.center_y,
					target.player_id,
					FireballVariant::red
				);
			}
			chain_fire_charged_exatt(
				state,
				target.player_id,
				explosion_chain_slot,
				target.center_x,
				target.center_y
			);
		} else {
			target.hp--;
		}
		target.frame = 0U;
	}
}

[[maybe_unused]] bool player_hittest(
	const DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t hitbox_size,
	std::int16_t &collision_x,
	std::int16_t &collision_y
) noexcept
{
	const auto &player = state.players[player_id];
	if(player.invincibility_time != 0U || player.hyper_active != 0U) {
		return false;
	}

	auto memory_read = [&](std::uint16_t offset) {
		const auto collmap_offset = static_cast<std::uint16_t>(
			offset - anniversary_collmap_dgroup_offset
		);
		constexpr auto collmap_bytes = static_cast<std::uint16_t>(
			player_count * (collmap_width / 8U) * collmap_height
		);
		if(collmap_offset < collmap_bytes) {
			const auto playfield = static_cast<std::size_t>(
				collmap_offset / ((collmap_width / 8U) * collmap_height)
			);
			const auto within_playfield = static_cast<std::size_t>(
				collmap_offset % ((collmap_width / 8U) * collmap_height)
			);
			const auto byte_x = within_playfield / collmap_height;
			const auto y = within_playfield % collmap_height;
			std::uint8_t packed = 0U;
			for(std::size_t bit = 0U; bit < 8U; ++bit) {
				if(state.collision_map[playfield][(byte_x * 8U) + bit][y]) {
					packed |= static_cast<std::uint8_t>(0x80U >> bit);
				}
			}
			return packed;
		}
		// A negative first column wraps the packed-map pointer around DGROUP.
		// P1 wraps into the public-domain master.lib SinTable8/CosTable8 data;
		// reconstruct those bytes from the already-ported integer functions.
		// The little-endian byte value matters because the scan masks it before
		// deciding whether this source landmine reports a collision.
		auto table_byte = [&](std::uint16_t table_offset, auto value_at) {
			const auto byte_offset = static_cast<std::uint16_t>(
				offset - table_offset
			);
			const auto value = exact::bits(value_at(
				static_cast<std::uint8_t>(byte_offset >> 1U)
			));
			return static_cast<std::uint8_t>(
				(byte_offset & 1U) ? (value >> 8U) : value
			);
		};
		const auto sine_offset = static_cast<std::uint16_t>(
			offset - anniversary_sine_table_dgroup_offset
		);
		if(sine_offset < (64U * sizeof(std::int16_t))) {
			return table_byte(anniversary_sine_table_dgroup_offset, sine8);
		}
		const auto cosine_offset = static_cast<std::uint16_t>(
			offset - anniversary_cosine_table_dgroup_offset
		);
		if(cosine_offset < (256U * sizeof(std::int16_t))) {
			return table_byte(anniversary_cosine_table_dgroup_offset, cosine8);
		}
		const auto runtime_offset = static_cast<std::uint16_t>(
			offset - anniversary_runtime_profile_offset
		);
		if(
			runtime_offset <
			anniversary_borland_runtime_collision_alias_profile.size()
		) {
			return anniversary_borland_runtime_collision_alias_profile[
				runtime_offset
			];
		}
		return std::uint8_t{0U};
	};

	const auto radius = static_cast<std::int16_t>(hitbox_size >> 1U);
	const auto center_x = exact::arithmetic_shift_right(
		player.center_x.raw(), 5U
	);
	const auto center_y = exact::arithmetic_shift_right(
		player.center_y.raw(), 5U
	);
	const auto left = static_cast<std::int16_t>(center_x - radius);
	auto top = static_cast<std::int16_t>(center_y - radius);
	auto bottom = static_cast<std::int16_t>(top + hitbox_size);
	if(top < 0) {
		top = 0;
	} else if(bottom >= static_cast<std::int16_t>(collmap_height)) {
		bottom = static_cast<std::int16_t>(collmap_height - 1U);
	}
	const auto first_bit = static_cast<std::uint8_t>(left & 7);
	auto byte_x = static_cast<std::int8_t>(
		exact::arithmetic_shift_right(left, 3U)
	);
	auto width_remaining = static_cast<std::int16_t>(hitbox_size + first_bit);
	std::uint8_t mask = static_cast<std::uint8_t>(0xFFU >> first_bit);
	if(width_remaining <= 8) {
		mask ^= static_cast<std::uint8_t>(0xFFU >> width_remaining);
	}
	constexpr auto collmap_size = static_cast<std::uint16_t>(
		(collmap_width / 8U) * collmap_height
	);
	auto memory_offset = static_cast<std::uint16_t>(
		anniversary_collmap_dgroup_offset +
		(static_cast<std::uint8_t>(byte_x) * collmap_height) +
		(player_id * collmap_size) + static_cast<std::uint16_t>(top)
	);
	// CL still contains first_bit until the first in-range column initializes
	// the stride. This register alias is observable after a negative byte_x.
	auto column_stride = first_bit;
	const auto tile_bottom = static_cast<std::uint8_t>(bottom);
	do {
		if(byte_x >= static_cast<std::int8_t>(collmap_width / 8U)) {
			break;
		}
		if(byte_x >= 0) {
			auto tile_y = static_cast<std::uint8_t>(top);
			column_stride = static_cast<std::uint8_t>(collmap_height);
			do {
				if((memory_read(memory_offset) & mask) != 0U) {
					collision_x = static_cast<std::int16_t>(byte_x * 256);
					collision_y = static_cast<std::int16_t>(top * 32);
					return true;
				}
				memory_offset++;
				column_stride--;
				tile_y++;
			} while(tile_y < tile_bottom);
		}
		byte_x++;
		memory_offset = static_cast<std::uint16_t>(
			memory_offset + column_stride
		);
		width_remaining = static_cast<std::int16_t>(width_remaining - 8);
		mask = 0xFFU;
		if(static_cast<std::uint16_t>(width_remaining) < 8U) {
			mask = static_cast<std::uint8_t>(~(
				0xFFU >> static_cast<std::uint8_t>(width_remaining)
			));
		}
	} while(width_remaining > 0);
	return false;
}

bool enemies_update(
	DeterministicState &state,
	const GameContent &content,
	GameplayObserver *observer
) noexcept
{
	state.enemies_alive = {};
	for(auto &enemy : state.enemies) {
		if(enemy.flag == EnemyFlag::free) {
			continue;
		}
		enemy.frame++;
		if(
			static_cast<std::uint8_t>(enemy.flag) >=
			static_cast<std::uint8_t>(EnemyFlag::explosion_ignoring_enemies)
		) {
			if(enemy.frame == 10U) {
				enemy.flag = EnemyFlag::explosion_hitting_enemies;
				observe_main_feedback(
					observer, MainFeedbackCueSite::enemy_explosion_hitting,
					enemy.player_id
				);
			} else if(enemy.frame == 32U) {
				enemy.flag = EnemyFlag::explosion_ignoring_enemies;
			} else if(enemy.frame >= 36U) {
				enemy.flag = EnemyFlag::free;
			}
			continue;
		}
		if(!enemy_run(
			enemy, content, state.players[enemy.player_id], state.enemy_speed
		)) {
			return false;
		}
		if(enemy.flag != EnemyFlag::free) {
			state.enemies_alive[enemy.player_id]++;
			if(enemy.flag == EnemyFlag::running_spawned) {
				collmap_set_enemy(state, enemy);
			}
		}
	}

	for(auto target_it = state.enemies.rbegin(); target_it != state.enemies.rend(); ++target_it) {
		auto &target = *target_it;
		if(target.flag != EnemyFlag::running_spawned || target.center_y <= 0) {
			continue;
		}
		if(state.damage_all_on[target.player_id] != 0U) {
			target.hp = 0U;
			target.flag = EnemyFlag::explosion_ignoring_enemies;
			target.frame = 0U;
			const auto slot = state.chains.cursor[target.player_id];
			target.chain_slot = slot;
			state.chains.hits[target.player_id][slot] = 1U;
			state.chains.pellet_and_fireball_value[target.player_id][slot] = 0U;
			state.chains.cursor[target.player_id] = static_cast<std::uint8_t>(
				(slot + 1U) % chain_ring_size
			);
			continue;
		}
		if(state.defeat_flag == DefeatFlag::banner) {
			continue;
		}
		const auto radius = static_cast<std::int16_t>(
			(target.size_pixels == 16U) ? (10 << 4) : (target.size_pixels * 7)
		);
		const auto left = exact::wrap_subtract(target.center_x, radius);
		const auto right = exact::wrap_add(target.center_x, radius);
		const auto top = exact::wrap_subtract(target.center_y, radius);
		const auto bottom = exact::wrap_add(target.center_y, radius);
		bool hit = false;
		bool explosion_collision = false;
		std::uint8_t explosion_chain_slot = 0U;

		for(auto &shot : state.shotpairs) {
			if(!shot.alive || shot.player_id != target.player_id) {
				continue;
			}
			const auto shot_center_x = exact::wrap_add(
				shot.topleft_x, shotpair_half_width
			);
			const auto shot_center_y = exact::wrap_add(
				shot.topleft_y, shotpair_half_height
			);
			if(
				exact::wrap_subtract(shot_center_x, shotpair_half_width) > right ||
				exact::wrap_add(shot_center_x, shotpair_half_width) < left ||
				shot_center_y < top || shot_center_y > bottom
			) {
				continue;
			}
			shot.alive = false;
			observe_hit_circle_exact(
				state, observer, HitCircleKind::enemy,
				shot_center_x, shot_center_y, target.player_id
			);
			hit = true;
		}

		for_each_explosion(state, [&](auto &explosion) {
			const auto flag = static_cast<std::uint8_t>(explosion.flag);
			if(
				flag < static_cast<std::uint8_t>(EnemyFlag::explosion_ignoring_enemies) ||
				explosion.player_id != target.player_id ||
				flag == static_cast<std::uint8_t>(
					EnemyFlag::explosion_ignoring_enemies
				) ||
				flag >= static_cast<std::uint8_t>(EnemyFlag::explosion_hitting_enemies) +
					static_cast<std::uint8_t>(
						explosion_max_enemy_hits_half(explosion) * 2U
					) ||
				(explosion.frame & 3U) != 0U
			) {
				return;
			}
			const auto explosion_radius = static_cast<std::int16_t>(
				explosion.size_pixels * 6
			);
			if(
				right < exact::wrap_subtract(explosion.center_x, explosion_radius) ||
				bottom < exact::wrap_subtract(explosion.center_y, explosion_radius) ||
				left > exact::wrap_add(explosion.center_x, explosion_radius) ||
				top > exact::wrap_add(explosion.center_y, explosion_radius)
			) {
				return;
			}
			explosion_enemy_hit_increment(explosion);
			explosion_collision = true;
			explosion_chain_slot = explosion.chain_slot;
			hit = true;
		});
		bool charge_one_hit = false;
		const auto charge_damage = chargeshot_hittest(
			state, target.player_id, left, right, top, bottom, observer,
			&charge_one_hit
		);
		if(charge_damage != 0U) {
			hit = true;
		}
		if(
			state.bomb_flag[target.player_id] != BombFlag::inactive &&
			(state.round_or_result_frame & 1U) == 0U
		) {
			hit = true;
		}

		if(!hit) {
			continue;
		}
		if(target.hp != 0U && !charge_one_hit) {
			target.hp--;
			target.frame = 0U;
			continue;
		}

		target.flag = EnemyFlag::explosion_ignoring_enemies;
		target.frame = 0U;
		if(!explosion_collision) {
			const auto slot = state.chains.cursor[target.player_id];
			target.chain_slot = slot;
			state.chains.hits[target.player_id][slot] = 1U;
			state.chains.pellet_and_fireball_value[target.player_id][slot] = 0U;
			state.chains.cursor[target.player_id] = static_cast<std::uint8_t>(
				(slot + 1U) % chain_ring_size
			);
		} else {
			target.chain_slot = explosion_chain_slot;
			auto &hits = state.chains.hits[target.player_id][explosion_chain_slot];
			if(hits != 0xFFU) {
				hits++;
			}
			gauge_avail_add(state, target.player_id, 32U);
			const auto bonus = combo_add(
				state, target.player_id, hits,
				static_cast<std::uint16_t>(hits * 16U)
			);
			fire_point_based_boss_attack_or_panic(
				state, bonus, target.player_id, explosion_chain_slot
			);
			const auto charge_slot = static_cast<std::uint8_t>(
				state.round_speed / 32U
			);
			if(hits <= 2U) {
				state.chains.charge_fireball[target.player_id][charge_slot]++;
			} else if(hits <= static_cast<std::uint8_t>(9U - charge_slot)) {
				std::uint16_t pellet_speed = 24U;
				pellet_speed = static_cast<std::uint16_t>(
					((pellet_speed * state.round_speed) >> 7U) + pellet_speed
				);
				pellet_speed = std::min<std::uint16_t>(pellet_speed, 0x7FU);
				(void)bullet_add_transfer_pellet(
					state,
					target.center_x,
					target.center_y,
					target.player_id,
					static_cast<std::uint8_t>(pellet_speed)
				);
				state.chains.charge_fireball[target.player_id][charge_slot] =
					static_cast<std::uint8_t>(
						state.chains.charge_fireball[target.player_id][charge_slot] + 2U
					);
			} else if(hits <= static_cast<std::uint8_t>(14U - (charge_slot * 2U))) {
				state.chains.charge_fireball[target.player_id][charge_slot] =
					static_cast<std::uint8_t>(
						state.chains.charge_fireball[target.player_id][charge_slot] + 3U
					);
				if((hits & 1U) == 0U) {
					std::uint16_t pellet_speed = 24U;
					pellet_speed = static_cast<std::uint16_t>(
						((pellet_speed * state.round_speed) >> 7U) + pellet_speed
					);
					pellet_speed = std::min<std::uint16_t>(pellet_speed, 0x7FU);
					(void)bullet_add_transfer_pellet(
						state,
						target.center_x,
						target.center_y,
						target.player_id,
						static_cast<std::uint8_t>(pellet_speed)
					);
					state.chains.charge_exatt[target.player_id][charge_slot]++;
				}
			} else {
				state.chains.charge_fireball[target.player_id][charge_slot] =
					static_cast<std::uint8_t>(
						state.chains.charge_fireball[target.player_id][charge_slot] + 4U
					);
				state.chains.charge_exatt[target.player_id][charge_slot]++;
			}
			chain_fire_charged_exatt(
				state,
				target.player_id,
				charge_slot,
				target.center_x,
				target.center_y
			);
			fireball_add_if_charged(
				state,
				target.player_id,
				explosion_chain_slot,
				target.center_x,
				target.center_y
			);
		}
		score_add(
			state.score[target.player_id],
			static_cast<std::uint16_t>(
				target.explosion_max_enemy_hits_half * 10U
			)
		);
	}
	return true;
}
