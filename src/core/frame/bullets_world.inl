// Included by frame.cpp inside its private implementation namespace.

void chain_fire_charged_exatt(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t chain_slot,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	auto &charge = state.chains.charge_exatt[player_id][chain_slot];
	const auto threshold = static_cast<std::uint8_t>(
		6U - (state.round_speed / 32U)
	);
	if(charge < threshold) {
		return;
	}
	charge = 0U;
	extra_attack_add(state, center_x, center_y, player_id);
}

void fireball_add_if_charged(
	DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t chain_slot,
	std::int16_t origin_x,
	std::int16_t origin_y,
	FireballVariant variant = FireballVariant::blue
) noexcept
{
	auto &charge = state.chains.charge_fireball[player_id][chain_slot];
	const auto threshold = static_cast<std::uint8_t>(
		12U - (state.round_speed / 16U)
	);
	if(charge < threshold) {
		return;
	}
	charge = 0U;
	(void)fireball_add(
		state, origin_x, origin_y, player_id, variant
	);
}

bool bullet_add_transfer_pellet(
	DeterministicState &state,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id,
	std::uint8_t template_speed
) noexcept
{
	// The collision caller initializes the source template angle before the
	// type-only bullets_add_transfer_pellet() wrapper.
	state.bullet_template_angle = 0U;
	if(!bullet_center_is_spawnable(center_x, center_y)) {
		return false;
	}
	auto *bullet = bullet_free_slot(state);
	if(bullet == nullptr) {
		return false;
	}
	bullet->flag = BulletFlag::transfer_pellet;
	bullet->age = 0U;
	bullet->center_x = center_x;
	bullet->center_y = center_y;
	bullet->player_id = player_id;
	bullet->group_next = bullet_group_random_constrained_aimed;
	bullet->speed_next = bullet_speed_adjusted(state, template_speed);
	bullet->angle_next = 0U;
	bullet->accel_type = 0U;
	bullet->is_collidable = true;
	bullet->has_trail = false;

	const auto origin_screen = playfield_x_to_screen_subpixel(
		state, center_x, player_id
	);
	const auto target_x = static_cast<std::int16_t>(
		randring_next16(state) % static_cast<std::uint16_t>(288U << 4U)
	);
	bullet->target_x_for_target_player = target_x;
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	const auto target_screen = playfield_x_to_screen_subpixel(
		state, target_x, target_player
	);
	const auto target_y = static_cast<std::int16_t>(
		randring_next16(state) & 0x00FFU
	);
	const auto travel_speed = static_cast<std::int16_t>(
		70U + (state.round_speed / 3U)
	);
	const auto velocity = vector2_between_plus(
		exact::wrap_shift_left(origin_screen, 4U),
		center_y,
		exact::wrap_shift_left(target_screen, 4U),
		target_y,
		0U,
		travel_speed
	);
	bullet->velocity_x = velocity.x;
	bullet->velocity_y = velocity.y;
	bullet->target_x_for_origin_player = screen_x_to_playfield_subpixel(
		target_screen, player_id
	);
	return true;
}

bool bullet_add_cloud_from_transfer(
	DeterministicState &state,
	const BulletState &source
) noexcept
{
	// bullets_add_next_from_p() copies this field before attempting the add.
	state.bullet_template_angle = source.angle_next;
	if(!bullet_center_is_spawnable(source.center_x, source.center_y)) {
		return false;
	}
	auto *bullet = bullet_free_slot(state);
	if(bullet == nullptr) {
		return false;
	}
	bullet->flag = BulletFlag::pellet_cloud;
	bullet->age = 0U;
	bullet->center_x = source.center_x;
	bullet->center_y = source.center_y;
	bullet->player_id = source.player_id;
	bullet->group_next = source.group_next;
	bullet->speed_next = bullet_speed_adjusted(state, source.speed_next);
	bullet->accel_type = 0U;
	bullet->is_collidable = true;
	bullet->has_trail = false;
	const auto angle_offset = static_cast<std::uint8_t>(
		(randring_next16(state) & 0x001FU) - 0x10U
	);
	bullet->angle_next = angle_offset;
	const auto &target = state.players[bullet->player_id];
	const auto velocity = vector2_between_plus(
		bullet->center_x,
		bullet->center_y,
		target.center_x.raw(),
		target.center_y.raw(),
		angle_offset,
		static_cast<std::int16_t>(bullet->speed_next)
	);
	bullet->velocity_x = velocity.x;
	bullet->velocity_y = velocity.y;
	return true;
}

void collmap_set_bullet(
	DeterministicState &state,
	const BulletState &bullet
) noexcept
{
	if(
		bullet.center_x < 0 || bullet.center_x >= (288 << 4) ||
		bullet.center_y < 0 || bullet.center_y >= (368 << 4)
	) {
		return;
	}
	const auto x = static_cast<std::size_t>(bullet.center_x >> 5U);
	const auto y = static_cast<std::size_t>(bullet.center_y >> 5U);
	state.collision_map[bullet.player_id][x][y] = true;
}

template <typename Callback>
void for_each_explosion(
	DeterministicState &state,
	Callback &&callback
) noexcept
{
	// Enemy and fireball objects share one 64-slot EFE arena in the original.
	// Preserve that physical order even though the native state stores the two
	// subclasses separately.
	for(auto &explosion : state.enemies) {
		callback(explosion);
	}
	for(auto &explosion : state.fireballs) {
		callback(explosion);
	}
}

template <typename Explosion>
bool explosion_overlaps(
	const Explosion &explosion,
	std::uint8_t player_id,
	std::int16_t left,
	std::int16_t right,
	std::int16_t top,
	std::int16_t bottom
) noexcept
{
	const auto flag = static_cast<std::uint8_t>(explosion.flag);
	if(
		flag < static_cast<std::uint8_t>(EnemyFlag::explosion_ignoring_enemies) ||
		explosion.player_id != player_id || (explosion.frame & 3U) != 0U
	) {
		return false;
	}
	const auto radius = static_cast<std::int16_t>(explosion.size_pixels * 6U);
	return !(
		right < exact::wrap_subtract(explosion.center_x, radius) ||
		bottom < exact::wrap_subtract(explosion.center_y, radius) ||
		left > exact::wrap_add(explosion.center_x, radius) ||
		top > exact::wrap_add(explosion.center_y, radius)
	);
}

template <typename Explosion>
void explosion_enemy_hit_increment(Explosion &explosion) noexcept
{
	using Flag = decltype(explosion.flag);
	explosion.flag = static_cast<Flag>(
		static_cast<std::uint8_t>(explosion.flag) + 1U
	);
}

std::uint8_t explosion_max_enemy_hits_half(
	const EnemyState &explosion
) noexcept
{
	return explosion.explosion_max_enemy_hits_half;
}

std::uint8_t explosion_max_enemy_hits_half(
	const FireballState &explosion
) noexcept
{
	// This byte is deliberately overlaid with the fireball variant in the
	// original EFE subclass.
	return explosion.variant_as_explosion_target;
}

bool pellet_explosions_hittest(
	DeterministicState &state,
	const BulletState &pellet
) noexcept
{
	constexpr std::int16_t pellet_radius = 6 << 4;
	const auto left = exact::wrap_subtract(pellet.center_x, pellet_radius);
	const auto right = exact::wrap_add(pellet.center_x, pellet_radius);
	const auto top = exact::wrap_subtract(pellet.center_y, pellet_radius);
	const auto bottom = exact::wrap_add(pellet.center_y, pellet_radius);
	bool collided = false;
	for_each_explosion(state, [&](auto &explosion) {
		if(!explosion_overlaps(
			explosion, pellet.player_id, left, right, top, bottom
		)) {
			return;
		}
		collided = true;
		const auto slot = explosion.chain_slot;
		auto &hits = state.chains.hits[pellet.player_id][slot];
		if(hits != 0xFFU) {
			hits++;
		}
		score_add(state.score[pellet.player_id], 1U);
		auto bonus = static_cast<std::uint16_t>(hits * 2U);
		bonus = static_cast<std::uint16_t>(bonus + state.round_speed);
		if(state.round_speed <= (3U << 4U)) {
			bonus >>= 1U;
		} else if(state.round_speed >= (6U << 4U)) {
			bonus = static_cast<std::uint16_t>(bonus + bonus);
		}
		auto &value = state.chains.pellet_and_fireball_value
			[pellet.player_id][slot];
		value++;
		gauge_avail_add(state, pellet.player_id, 4U);
		if(value < 4U) {
			state.chains.charge_fireball[pellet.player_id][slot] =
				static_cast<std::uint8_t>(
					state.chains.charge_fireball[pellet.player_id][slot] + 2U
				);
		} else if(value < 10U) {
			state.chains.charge_fireball[pellet.player_id][slot] =
				static_cast<std::uint8_t>(
					state.chains.charge_fireball[pellet.player_id][slot] + 5U
				);
		} else if(value < 20U) {
			state.chains.charge_fireball[pellet.player_id][slot] =
				static_cast<std::uint8_t>(
					state.chains.charge_fireball[pellet.player_id][slot] + 2U
				);
			state.chains.charge_exatt[pellet.player_id][slot]++;
		} else {
			state.chains.charge_exatt[pellet.player_id][slot] =
				static_cast<std::uint8_t>(
					state.chains.charge_exatt[pellet.player_id][slot] + 2U
				);
		}
		bonus = combo_add(state, pellet.player_id, hits, bonus);
		fire_point_based_boss_attack_or_panic(
			state, bonus, pellet.player_id, slot
		);
		fireball_add_if_charged(
			state,
			pellet.player_id,
			slot,
			explosion.center_x,
			explosion.center_y
		);
	});
	return collided;
}

void bullets_update(DeterministicState &state) noexcept
{
	for(auto &bullet : state.bullets) {
		if(bullet.flag == BulletFlag::free) {
			continue;
		}
		bullet.age++;
		if(bullet.flag == BulletFlag::pellet_cloud) {
			if((bullet.age & 3U) == 0U && bullet.age >= 12U) {
				bullet.flag = BulletFlag::pellet;
			}
			continue;
		}
		if(
			bullet.flag != BulletFlag::transfer_pellet &&
			(state.bomb_flag[bullet.player_id] != BombFlag::inactive ||
			 state.damage_all_on[bullet.player_id] != 0U)
		) {
			bullet.flag = BulletFlag::free;
			continue;
		}

		auto next_x = exact::wrap_add(
			bullet.center_x, bullet.velocity_x
		);
		if(bullet.flag == BulletFlag::transfer_pellet) {
			const auto reached = (bullet.player_id == 0U)
				? (next_x >= bullet.target_x_for_origin_player)
				: (next_x <= bullet.target_x_for_origin_player);
			if(reached) {
				bullet.flag = BulletFlag::pellet_cloud;
				bullet.player_id ^= 1U;
				bullet.center_x = bullet.target_x_for_target_player;
				bullet.center_y = 2 << 4;
				bullet.speed_next = static_cast<std::uint8_t>(
					bullet.speed_next + 8U
				);
				(void)bullet_add_cloud_from_transfer(state, bullet);
				bullet.flag = BulletFlag::free;
				continue;
			}
		} else if(bullet.has_trail) {
			if(next_x <= bullet_center_min) {
				next_x = bullet_center_min;
			} else if(next_x >= bullet_center_x_max) {
				next_x = bullet_center_x_max;
			}
			auto &trail = state.bullet_trails[bullet.trail_index];
			if(!bullet_trail_update_and_clip(
				trail.center_x,
				bullet.center_x,
				bullet_center_min,
				bullet_center_x_max
			)) {
				bullet.flag = BulletFlag::free;
				continue;
			}
		} else if(
			next_x <= bullet_center_min || next_x >= bullet_center_x_max
		) {
			bullet.flag = BulletFlag::free;
			continue;
		}
		bullet.center_x = next_x;

		const auto next_y = exact::wrap_add(
			bullet.center_y, bullet.velocity_y
		);
		if(bullet.has_trail) {
			auto &trail = state.bullet_trails[bullet.trail_index];
			if(!bullet_trail_update_and_clip(
				trail.center_y,
				bullet.center_y,
				bullet_center_min,
				bullet_center_y_max
			)) {
				bullet.flag = BulletFlag::free;
				continue;
			}
		} else if(next_y <= bullet_center_min || next_y >= bullet_center_y_max) {
			bullet.flag = BulletFlag::free;
			continue;
		}
		bullet.center_y = next_y;
		if(bullet.accel_type == 0x80U && bullet.velocity_y < (5 << 4)) {
			bullet.velocity_y = exact::wrap_add(
				bullet.velocity_y, static_cast<std::int16_t>(1)
			);
		}
		if(
			bullet.flag == BulletFlag::pellet &&
			pellet_explosions_hittest(state, bullet)
		) {
			if(
				state.gba_flag_active[bullet.player_id ^ 1U] ==
				GaugeBossAttackFlag::none
			) {
				(void)bullet_add_transfer_pellet(
					state,
					bullet.center_x,
					bullet.center_y,
					bullet.player_id,
					bullet.speed_next
				);
			}
			bullet.flag = BulletFlag::free;
			continue;
		}
		if(
			bullet.flag != BulletFlag::transfer_pellet &&
			bullet.is_collidable
		) {
			collmap_set_bullet(state, bullet);
		}
	}
}
