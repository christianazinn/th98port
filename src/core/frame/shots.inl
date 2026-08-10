// Included by frame.cpp inside its private implementation namespace.

void add_reimu_hyper_amulet(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept;

void shots_add(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	const auto &player = state.players[player_id];
	if(player.shot_mode == shot_mode_none) {
		return;
	}
	std::int16_t left = player.center_x.raw();
	std::int16_t top = player.center_y.raw();
	std::uint8_t pairs_fired = 0U;
	if(player.shot_mode == shot_mode_three_pairs) {
		left = exact::wrap_subtract(left, static_cast<std::int16_t>(64 << 4));
	} else if(player.shot_mode == shot_mode_two_pairs) {
		left = exact::wrap_subtract(left, static_cast<std::int16_t>(32 << 4));
		pairs_fired = 2U;
	} else if(player.shot_mode == shot_mode_reimu_hyper) {
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
	for(std::size_t slot_id = 0U; slot_id < state.shotpairs.size(); ++slot_id) {
		auto &slot = state.shotpairs[slot_id];
		if(slot.alive) {
			continue;
		}
		slot.alive = true;
		slot.topleft_x = left;
		slot.topleft_y = top;
		slot.velocity_y = shot_velocity_y;
		slot.player_id = player_id;
		if(observer != nullptr) {
			observer->shot_pair_spawned(ShotPairSpawned{
				.slot = static_cast<ShotPairSlot>(slot_id),
				.player_id = static_cast<PlayerId>(player_id),
			});
		}
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
