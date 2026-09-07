// Included by frame.cpp inside its private implementation namespace.

std::int16_t playfield_x_to_screen_subpixel(
	const DeterministicState &state,
	std::int16_t x,
	std::uint8_t player_id
) noexcept
{
	const auto pixel = exact::arithmetic_shift_right(x, 4U);
	return static_cast<std::int16_t>(
		(pixel + 16) + (static_cast<int>(player_id) * 320) +
		state.playfield_fg_shift_x[player_id]
	);
}

std::int16_t screen_x_to_playfield_subpixel(
	std::int16_t x,
	std::uint8_t player_id
) noexcept
{
	const auto pixel = static_cast<std::int16_t>(
		x - (static_cast<int>(player_id) * 320) - 16
	);
	return exact::wrap_shift_left(pixel, 4U);
}

enum class FireballVariant : std::uint8_t {
	blue = 0,
	red = 1,
};

bool fireball_add(
	DeterministicState &state,
	std::int16_t center_x,
	std::int16_t center_y,
	std::uint8_t player_id,
	FireballVariant variant
) noexcept
{
	const auto slot = std::find_if(
		state.fireballs.begin(), state.fireballs.end(),
		[](const FireballState &fireball) {
			return fireball.flag == FireballFlag::free;
		}
	);
	if(slot == state.fireballs.end()) {
		return false;
	}
	slot->flag = FireballFlag::transfer;
	slot->center_x = center_x;
	slot->center_y = center_y;
	slot->player_id = player_id;
	const auto fall_speed = static_cast<std::uint16_t>(
		20U + (state.round_speed / 2U) + (randring_next16(state) & 31U)
	);
	slot->fall_velocity_y = static_cast<std::uint8_t>(
		std::min<std::uint16_t>(fall_speed, 8U << 4U)
	);
	const auto variant_value = static_cast<std::uint8_t>(variant);
	slot->hp = static_cast<std::uint8_t>(variant_value + 2U);
	slot->variant_as_explosion_target = static_cast<std::uint8_t>(
		2U + variant_value
	);
	slot->size_pixels = 48U;
	slot->generation = (variant == FireballVariant::blue)
		? 0U
		: static_cast<std::uint8_t>(state.fireball_generation_previous + 1U);

	const auto origin_screen = playfield_x_to_screen_subpixel(
		state, center_x, player_id
	);
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	std::int16_t target_x = 0;
	if((state.round_or_result_frame % 512U) < 256U) {
		target_x = static_cast<std::int16_t>(
			randring_next16(state) % static_cast<std::uint16_t>(288U << 4U)
		);
	} else {
		target_x = exact::wrap_add(
			state.players[target_player].center_x.raw(),
			static_cast<std::int16_t>(randring_next16(state) & 511U)
		);
	}
	slot->target_x_for_target_player = target_x;
	const auto target_screen = playfield_x_to_screen_subpixel(
		state, target_x, target_player
	);
	const auto travel_speed = static_cast<std::int16_t>(
		70U + (state.round_speed / 3U)
	);
	const auto velocity = vector2_between_plus(
		exact::wrap_shift_left(origin_screen, 4U),
		center_y,
		exact::wrap_shift_left(target_screen, 4U),
		0,
		0U,
		travel_speed
	);
	slot->velocity_x = velocity.x;
	slot->velocity_y = velocity.y;
	slot->target_x_for_origin_player = screen_x_to_playfield_subpixel(
		target_screen, player_id
	);
	return true;
}

void extra_attack_transfer_init(
	DeterministicState &state,
	ExtraAttackState &slot,
	std::int16_t center_x,
	std::int16_t center_y,
	std::int16_t target_x,
	std::int16_t target_y,
	std::uint8_t player_id,
	std::int16_t velocity_base
) noexcept
{
	slot = ExtraAttackState{};
	slot.state = 2U;
	slot.center_x = center_x;
	slot.center_y = center_y;
	slot.player_id = player_id;
	slot.target_x_for_target_player = target_x;
	const auto origin_screen = playfield_x_to_screen_subpixel(
		state, center_x, player_id
	);
	const auto target_screen = playfield_x_to_screen_subpixel(
		state, target_x, static_cast<std::uint8_t>(1U - player_id)
	);
	const auto velocity = vector2_between_plus(
		exact::wrap_shift_left(origin_screen, 4U),
		center_y,
		exact::wrap_shift_left(target_screen, 4U),
		target_y,
		0U,
		static_cast<std::int16_t>(
			velocity_base + static_cast<std::int16_t>(state.round_speed / 4U)
		)
	);
	slot.velocity_x = velocity.x;
	slot.velocity_y = velocity.y;
	slot.target_x_for_origin_player = screen_x_to_playfield_subpixel(
		target_screen, player_id
	);
}
