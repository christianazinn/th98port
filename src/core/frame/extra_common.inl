// Included by frame.cpp inside its private implementation namespace.

bool extra_attack_transfer_update(ExtraAttackState &attack) noexcept
{
	const auto next_x = exact::wrap_add(attack.center_x, attack.velocity_x);
	const auto reached = (attack.player_id == 0U)
		? (next_x >= attack.target_x_for_origin_player)
		: (next_x <= attack.target_x_for_origin_player);
	if(reached) {
		attack.center_x = attack.target_x_for_target_player;
		attack.state = 3U;
		attack.player_id ^= 1U;
		return true;
	}
	attack.center_x = next_x;
	attack.center_y = exact::wrap_add(attack.center_y, attack.velocity_y);
	return false;
}

bool extra_attack_bounds_hittest(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t left,
	std::int16_t right,
	std::int16_t top,
	std::int16_t bottom,
	GameplayObserver *observer
) noexcept
{
	// hitbox_hittest() rejects every target once the result banner is active.
	// Extra Attacks that use explicit bounds still go through that same source
	// helper and must not consume Charge-hit RNG during the result sequence.
	if(state.defeat_flag == DefeatFlag::banner) {
		return false;
	}
	bool hit = false;
	for(auto &shot : state.shotpairs) {
		if(!shot.alive || shot.player_id != player_id) {
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
			shot_center_x, shot_center_y, player_id
		);
		hit = true;
	}
	const auto charge_hit = (
		chargeshot_hittest(
			state, player_id, left, right, top, bottom, observer
		) != 0U
	);
	const auto bomb_hit = (
		state.bomb_flag[player_id] != BombFlag::inactive &&
		(state.round_or_result_frame & 1U) == 0U
	);
	return (hit || charge_hit || bomb_hit);
}

bool extra_attack_shot_hittest(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y,
	GameplayObserver *observer
) noexcept
{
	if(center_y < 0) {
		return false;
	}
	constexpr std::int16_t radius = 16 << 4;
	return extra_attack_bounds_hittest(
		state,
		player_id,
		exact::wrap_subtract(center_x, radius),
		exact::wrap_add(center_x, radius),
		exact::wrap_subtract(center_y, radius),
		exact::wrap_add(center_y, radius),
		observer
	);
}
