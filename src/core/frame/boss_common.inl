// Included by frame.cpp inside its private implementation namespace.

void observe_boss_palette_baseline_restored(
	GameplayObserver *observer,
	std::uint8_t player_id
) noexcept
{
	if(observer != nullptr) {
		observer->boss_palette_baseline_restored(
			BossPaletteBaselineRestored{
				.player = static_cast<PlayerId>(player_id),
			}
		);
	}
}

void observe_boss_animation_step(
	GameplayObserver *observer,
	std::uint8_t owner,
	std::uint8_t state_at_dispatch,
	std::uint16_t pattern_frame_at_dispatch,
	std::uint8_t render_state_at_dispatch
) noexcept
{
	if(observer != nullptr) {
		observer->boss_animation_step(BossAnimationStep{
			.owner = static_cast<PlayerId>(owner),
			.state_at_dispatch = state_at_dispatch,
			.pattern_frame_at_dispatch = pattern_frame_at_dispatch,
			.render_state_at_dispatch = render_state_at_dispatch,
		});
	}
}

void boss_move(BossState &boss) noexcept
{
	boss.center_x = exact::wrap_add(boss.center_x, boss.velocity_x);
	boss.center_y = exact::wrap_add(boss.center_y, boss.velocity_y);
	boss.motion_angle++;
	boss.velocity_y = vector2(boss.motion_angle, 16).y;
	if(boss.center_x <= (48 << 4)) {
		boss.velocity_x = 32;
	} else if(boss.center_x >= (240 << 4)) {
		boss.velocity_x = -32;
	}
}

void boss_exit_move(DeterministicState &state, std::uint8_t player_id) noexcept
{
	auto &boss = state.boss;
	boss.center_x = exact::wrap_add(boss.center_x, boss.velocity_x);
	boss.center_y = exact::wrap_add(boss.center_y, static_cast<std::int16_t>(32));
	if(boss.center_x <= (48 << 4)) {
		boss.velocity_x = 32;
	} else if(boss.center_x >= (240 << 4)) {
		boss.velocity_x = -32;
	}
	state.boss_target_active[1U - player_id] = 0U;
	if(boss.center_y >= (416 << 4)) {
		boss.state = 0U;
		state.gba_boss_launched_by = -1;
		state.combo_points_for_boss_attack = 5120U;
	}
}

bool boss_launch_or_reverse(
	DeterministicState &state,
	std::uint8_t player_id,
	GameplayObserver *observer
) noexcept
{
	if(state.gba_flag_active[player_id] != GaugeBossAttackFlag::boss) {
		return false;
	}
	if(state.gba_boss_launched_by < 0) {
		const auto initial = state.boss_initial[player_id];
		// ZUN keeps this byte outside the 32-byte boss record copied at launch,
		// so a new boss attack inherits the previous attack's last pattern.
		const auto pattern_previous = state.boss.pattern_previous;
		state.boss = BossState{};
		static_cast<BossInitialState &>(state.boss) = initial;
		state.boss.pattern_previous = pattern_previous;
		state.boss.pattern_count = static_cast<std::uint8_t>(
			(randring_next16(state) & 7U) + 1U
		);
		state.boss.pattern_frame = 0U;
		state.gba_boss_launched_by = static_cast<std::int8_t>(player_id);
		state.gba_flag_active[player_id] = GaugeBossAttackFlag::none;
		observe_main_feedback(
			observer, MainFeedbackCueSite::boss_launched, player_id
		);
		observe_boss_palette_baseline_restored(observer, 1U - player_id);
		if(observer != nullptr) {
			observer->boss_activated(BossActivated{
				.owner = static_cast<PlayerId>(player_id),
				.paletted_character =
					state.players[player_id].paletted_character,
			});
		}
		state.boss_target_active[1U - player_id] = 1U;
		return true;
	}
	if(state.boss.state != 0xFFU) {
		state.boss.state = 0xFFU;
		state.boss.pattern_frame = 0U;
		observe_boss_palette_baseline_restored(observer, 1U - player_id);
		state.boss_target_active[player_id] = 0U;
	}
	return false;
}

void boss_target_update(DeterministicState &state) noexcept
{
	if(state.boss.state != 0x80U) {
		state.boss_target_x = state.boss.center_x;
		state.boss_target_y = exact::wrap_add(
			state.boss.center_y, static_cast<std::int16_t>(128 << 4)
		);
	}
}

void boss_choose_pattern(DeterministicState &state) noexcept
{
	auto &boss = state.boss;
	boss_move(boss);
	if(boss.pattern_frame < 0x50U) {
		return;
	}
	std::uint8_t next = 0U;
	for(;;) {
		next = static_cast<std::uint8_t>(randring_next16(state) & 0x0FU);
		if((static_cast<int>(boss.pattern_previous) + 2) < next) {
			break;
		}
		if((static_cast<int>(boss.pattern_previous) - 2) <= next) {
			continue;
		}
		break;
	}
	boss.pattern_previous = next;
	boss.pattern_frame = 0U;
	boss.state = static_cast<std::uint8_t>(next + 2U);
	boss.pattern_index++;
	if(boss.pattern_index > boss.pattern_count) {
		boss.state = 0x80U;
	}
}

std::uint8_t boss_hittest_damage(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t radius,
	GameplayObserver *observer
) noexcept
{
	if(
		state.boss.center_y < 0 ||
		state.defeat_flag == DefeatFlag::banner
	) {
		return 0U;
	}
	const auto left = exact::wrap_subtract(state.boss.center_x, radius);
	const auto right = exact::wrap_add(state.boss.center_x, radius);
	const auto top = exact::wrap_subtract(state.boss.center_y, radius);
	const auto bottom = exact::wrap_add(state.boss.center_y, radius);
	std::uint8_t damage = 0U;
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
		damage = static_cast<std::uint8_t>(damage + 2U);
	}
	damage = static_cast<std::uint8_t>(damage + chargeshot_hittest(
		state, player_id, left, right, top, bottom, observer
	));
	if(
		state.bomb_flag[player_id] != BombFlag::inactive &&
		(state.round_or_result_frame & 1U) == 0U
	) {
		damage++;
	}
	return damage;
}

void boss_defeat_if_needed(
	DeterministicState &state,
	std::uint8_t target_player,
	GameplayObserver *observer
) noexcept
{
	if(
		static_cast<std::int16_t>(state.boss.hp) > 0 ||
		state.boss.state == 0xFFU
	) {
		return;
	}
	state.boss.state = 0xFFU;
	state.boss.pattern_frame = 0U;
	observe_boss_palette_baseline_restored(observer, target_player);
	if(state.gba_flag_active[target_player] != GaugeBossAttackFlag::boss) {
		state.combo_points_for_boss_attack = 5120U;
	}
	state.boss_target_active[target_player] = 0U;
	if(state.gba_boss_level < 16U) {
		state.gba_boss_level++;
	}
}
