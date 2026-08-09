// Included by ../frame.cpp inside its private implementation namespace.

void hud_start_update_prefix(DeterministicState &state) noexcept
{
	if(state.hud_start_flag == 1U) {
		state.hud_start_state = 0U;
		state.hud_start_frame = 0U;
		state.hud_start_flag = 2U;
	}
	if(state.hud_start_state == 0U) {
		// The first banner chunk is also the last to settle. Its velocity passes
		// through zero and becomes negative before it returns to x=64.
		state.hud_start_frame++;
		if(state.hud_start_frame >= 69U) {
			state.hud_start_state = 1U;
			state.hud_start_frame = 0U;
		}
	} else if(state.hud_start_state == 1U) {
		state.hud_start_frame++;
		if(state.hud_start_frame > 0x56U) {
			state.hud_start_state = 2U;
			state.hud_start_frame = 0U;
		}
	} else if(state.hud_start_state == 2U) {
		// The particle initialization is gameplay-relevant despite being purely
		// visual: each of 128 particles consumes two 16-bit ring values and two
		// master.lib IRAND values, in that order.
		for(std::size_t i = 0; i < 128U; ++i) {
			(void)(randring_next16(state) & 511U);
			(void)randring_next16(state);
			(void)(irand(state.random_seed) % 3U);
			(void)(irand(state.random_seed) % 3U);
		}
		state.hud_start_state = 3U;
		state.hud_start_frame = 0U;
	} else if(state.hud_start_state == 3U) {
		state.hud_start_frame++;
		if(state.hud_start_frame >= 0x40U) {
			state.hud_start_state = 4U;
		}
	} else {
		state.hud_start_flag = 0U;
	}
}

void background_update(DeterministicState &state) noexcept
{
	if(state.background_phase != BackgroundPhase::initial) {
		return;
	}

	if(state.background_initial_kind == BackgroundInitialKind::type_a) {
		// sub_D135(): 24 GRCG particles followed by 24 sprite particles.
		for(std::size_t i = 0; i < 48U; ++i) {
			(void)irand(state.random_seed);
		}
		if(state.round_frame > 128U) {
			state.background_phase = BackgroundPhase::type_a_steady;
		}
	} else {
		// sub_D340(): 24 GRCG particles, then only the first 16 of its
		// 26 sprite particles call irand(). These presentation calls share the
		// gameplay RNG and are therefore part of deterministic call order.
		for(std::size_t i = 0; i < 40U; ++i) {
			(void)irand(state.random_seed);
		}
		if(state.round_frame > 144U) {
			state.background_phase = BackgroundPhase::type_b_steady;
		}
	}
}

void result_bonus_update(DeterministicState &state) noexcept
{
	const auto winner = static_cast<std::uint8_t>(1U - state.last_fatal_player);
	if(winner >= player_count) {
		return;
	}
	if(state.round_or_result_frame == 1U) {
		const auto &player = state.players[winner];
		state.result_bonus_remaining =
			(static_cast<std::uint32_t>(player.combo_hits_max) * 1000U) +
			(static_cast<std::uint32_t>(player.gauge_attacks_fired) * 10000U) +
			(static_cast<std::uint32_t>(player.boss_attacks_fired) * 15000U) +
			(static_cast<std::uint32_t>(player.boss_attacks_reversed) * 20000U) +
			(static_cast<std::uint32_t>(player.boss_panics_fired) * 30000U);
		if(state.story_stage == story_stage_yumemi) {
			state.result_bonus_remaining +=
				static_cast<std::uint32_t>(state.story_lives) * 100000U;
			state.extends_gained = extends_disabled;
		}
		const auto per_frame = state.result_bonus_remaining / 192U;
		state.result_bonus_per_frame = static_cast<std::uint16_t>(
			std::min<std::uint32_t>(per_frame, 0xFFFFU)
		);
	}
	if(state.result_bonus_per_frame < state.result_bonus_remaining) {
		score_add(state.score[winner], state.result_bonus_per_frame);
		state.result_bonus_remaining -= state.result_bonus_per_frame;
	} else if(state.result_bonus_remaining != 0U) {
		score_add(
			state.score[winner],
			static_cast<std::uint16_t>(state.result_bonus_remaining)
		);
		state.result_bonus_remaining = 0U;
	}
}

bool result_control_update(DeterministicState &state) noexcept
{
	if(state.defeat_flag != DefeatFlag::banner) {
		return false;
	}
	const auto start_result_transition = [&state]() {
		state.result_phase = ResultPhase::opening;
	};
	const auto finish_route = [&state](std::uint8_t route) {
		if(!state.result_transition_complete) {
			return false;
		}
		state.route = route;
		state.round_complete = true;
		return true;
	};
	const auto result_gate = static_cast<std::uint16_t>(
		(state.story_stage < story_stage_yumemi) ? 320U : 460U
	);

	if(state.game_mode != game_mode_story && !state.practice_uses_stock) {
		const auto match_complete =
			state.players[0].rounds_won >= 2U ||
			state.players[1].rounds_won >= 2U;
		if(match_complete) {
			result_bonus_update(state);
			if(state.round_or_result_frame == result_gate) {
				start_result_transition();
			}
			return finish_route(2U);
		}
		if(state.round_or_result_frame == 160U) {
			start_result_transition();
		}
		return finish_route(1U);
	}

	if(state.winner == 0) {
		result_bonus_update(state);
		if(state.round_or_result_frame == result_gate) {
			start_result_transition();
		}
		return finish_route(2U);
	}
	if(state.round_or_result_frame == 160U) {
		start_result_transition();
	}
	if(!state.result_transition_complete) {
		return false;
	}
	if(state.story_lives != 0U) {
		state.story_lives--;
		state.route = 1U;
	} else {
		state.route = 2U;
	}
	state.round_complete = true;
	return true;
}

void result_transition_update(DeterministicState &state) noexcept
{
	if(state.result_phase == ResultPhase::idle) {
		return;
	}
	state.result_phase_frame++;
	if(state.result_phase_frame < 16U) {
		return;
	}
	state.result_phase_frame = 0U;
	if(state.result_phase == ResultPhase::opening) {
		state.result_transition_complete = true;
		state.result_phase = ResultPhase::closing;
	} else {
		state.result_phase = ResultPhase::idle;
	}
}

void story_extend_update(DeterministicState &state) noexcept
{
	if(state.extends_gained >= state.score[0][6]) {
		return;
	}
	state.story_lives = static_cast<std::uint8_t>(state.story_lives + 1U);
	state.extends_gained = static_cast<std::uint8_t>(
		state.extends_gained + 1U
	);
	if(state.extends_gained == extends_max) {
		state.extends_gained = extends_disabled;
	}
}

void round_mode_update(DeterministicState &state) noexcept
{
	if(state.demo_number == 0U || state.game_mode != game_mode_demo) {
		return;
	}
	if(state.demo_frame < 7000U) {
		state.demo_frame++;
	} else if(state.demo_frame == 7000U) {
		// The source reassigns the same opening callback every frame without
		// resetting that callback's independent phase byte.
		state.result_phase = ResultPhase::opening;
	}
	if(state.result_transition_complete || state.input_system != 0U) {
		state.session_exit_requested = true;
	}
}
