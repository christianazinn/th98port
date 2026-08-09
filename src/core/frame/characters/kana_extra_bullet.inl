// Included by frame.cpp inside its private implementation namespace.

void bullet_add_kana_extra_attack(
	DeterministicState &state,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id
) noexcept
{
	// exatt_update_kana() writes 0x40 to the shared source template before
	// attempting the allocation. Preserve the write even if the arena is full
	// or the center is clipped; a later callback can inherit it.
	state.bullet_template_angle = 0x40U;
	if(!bullet_center_is_spawnable(center_x, center_y)) {
		return;
	}
	auto *bullet = bullet_free_slot(state);
	if(bullet == nullptr) {
		return;
	}
	bullet->flag = BulletFlag::bullet16;
	bullet->age = 0U;
	bullet->center_x = center_x;
	bullet->center_y = center_y;
	bullet->player_id = player_id;
	bullet->group_next = bullet_group_one;
	bullet->angle_next = 0x40U;
	bullet->speed_next = bullet_speed_adjusted(state, 1U << 4U);
	bullet->accel_type = 0x80U;
	bullet->is_collidable = true;
	bullet->has_trail = false;
	const auto velocity = vector2(
		bullet->angle_next, static_cast<std::int16_t>(bullet->speed_next)
	);
	bullet->velocity_x = velocity.x;
	bullet->velocity_y = velocity.y;
}
