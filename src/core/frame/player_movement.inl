// Included by frame.cpp inside its private implementation namespace.

MoveResult movement_velocity(
	const PlayerState &player,
	std::uint16_t input,
	Velocity &velocity
) noexcept
{
	auto set = [&](std::int8_t x, std::int8_t y) {
		const bool diagonal = x != 0 && y != 0;
		auto speed_x = diagonal
			? player.speed_diagonal_x
			: player.speed_aligned_x;
		auto speed_y = diagonal
			? player.speed_diagonal_y
			: player.speed_aligned_y;
		const auto character = character_index_from_paletted(
			player.paletted_character
		);
		if(player.hyper_active != 0U && player.hyper_effect_active_this_frame) {
			if(
				character == character_reimu ||
				character == character_mima ||
				character == character_kotohime ||
				character == character_kana ||
				character == character_chiyuri ||
				character == character_yumemi
			) {
				const auto boost = static_cast<std::uint8_t>(diagonal ? 24U : 32U);
				speed_x = static_cast<std::uint8_t>(speed_x + boost);
				speed_y = static_cast<std::uint8_t>(speed_y + boost);
			} else if(character == character_marisa) {
				const auto penalty = static_cast<std::uint8_t>(diagonal ? 12U : 16U);
				speed_x = static_cast<std::uint8_t>(speed_x - penalty);
				speed_y = static_cast<std::uint8_t>(speed_y - penalty);
			} else if(character == character_rikako) {
				const auto penalty = static_cast<std::uint8_t>(diagonal ? 8U : 16U);
				speed_x = static_cast<std::uint8_t>(speed_x - penalty);
				speed_y = static_cast<std::uint8_t>(speed_y - penalty);
			}
		}
		velocity.x = static_cast<std::int16_t>(static_cast<int>(speed_x) * x);
		velocity.y = static_cast<std::int16_t>(static_cast<int>(speed_y) * y);
	};

	switch(input) {
	case input_left: set(-1, 0); break;
	case input_down | input_left:
	case input_down_left: set(-1, 1); break;
	case input_down: set(0, 1); break;
	case input_down | input_right:
	case input_down_right: set(1, 1); break;
	case input_right: set(1, 0); break;
	case input_up | input_right:
	case input_up_right: set(1, -1); break;
	case input_up: set(0, -1); break;
	case input_up | input_left:
	case input_up_left: set(-1, -1); break;
	case 0U: return MoveResult::no_input;
	default: return MoveResult::invalid;
	}
	return MoveResult::valid;
}

void apply_velocity(PlayerState &player, const Velocity &velocity) noexcept
{
	const auto x = exact::wrap_add(player.center_x.raw(), velocity.x);
	const auto y = exact::wrap_add(player.center_y.raw(), velocity.y);
	player.center_x = exact::Subpixel::from_raw(
		std::clamp(x, player_x_min, player_x_max)
	);
	player.center_y = exact::Subpixel::from_raw(
		std::clamp(y, player_y_min, player_y_max)
	);
}

void move_human(PlayerState &player, std::uint16_t live_input) noexcept
{
	auto movement = static_cast<std::uint16_t>(live_input & input_movement);
	bool use_previous = true;
	Velocity velocity{};
	for(;;) {
		const auto result = movement_velocity(player, movement, velocity);
		if(result == MoveResult::invalid && use_previous) {
			if(player.human_movement_last != movement) {
				movement = static_cast<std::uint16_t>(
					movement & static_cast<std::uint16_t>(~player.human_movement_last)
				);
				use_previous = false;
				continue;
			}
			return;
		}
		if(result == MoveResult::valid) {
			apply_velocity(player, velocity);
		}
		if(use_previous) {
			player.human_movement_last = movement;
		}
		return;
	}
}
