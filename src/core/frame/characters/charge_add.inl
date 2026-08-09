// Included by frame.cpp inside its private implementation namespace.

void add_reimu_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	auto angle = std::uint8_t{0x90U};
	for(std::size_t group_id = 0U; group_id < 4U; ++group_id) {
		auto &group = state.reimu_charge[player_id][group_id];
		group.state = 1U;
		group.frame = 0U;
		group.x.fill(center_x);
		group.y.fill(center_y);
		group.angle = angle;
		group.speed = 0x60U;
		angle = static_cast<std::uint8_t>(angle + 0x20U);
	}
	state.reimu_chargeshot_primary = 1U;
}

void add_reimu_hyper_amulet(
	DeterministicState &state,
	std::uint8_t player_id,
	std::int16_t center_x,
	std::int16_t center_y
) noexcept
{
	for(auto &group : state.reimu_charge[player_id]) {
		if(group.state != 0U) {
			continue;
		}
		group.state = 2U;
		group.frame = 0U;
		group.x.fill(center_x);
		group.y.fill(center_y);
		group.angle = 0xC0U;
		group.speed = 0xC0U;
		break;
	}
	state.reimu_chargeshot_primary = 0U;
}

void add_mima_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	constexpr std::array<std::uint8_t, 6> angles{
		192U, 182U, 202U, 192U, 176U, 208U,
	};
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	state.mima_charge_active[player_id] = 1U;
	state.mima_charge_frames[player_id] = 0U;
	auto &nodes = state.mima_charge[player_id];
	for(std::size_t node_id = 0U; node_id < nodes.size(); ++node_id) {
		auto &node = nodes[node_id];
		node.active = (node_id == 0U) ? 1U : 0U;
		node.visual_frame = 0U;
		node.x = center_x;
		node.y = center_y;
		const auto velocity = vector2(angles[node_id % angles.size()], 176);
		node.velocity_x = velocity.x;
		node.velocity_y = velocity.y;
	}
}

void marisa_charge_initialize(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.marisa_charge[player_id];
	shot.active = 1U;
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = exact::wrap_subtract(
		state.players[player_id].center_y.raw(), std::int16_t{0x100}
	);
	shot.x.fill(center_x);
	shot.y.fill(center_y);
}

void add_marisa_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	marisa_charge_initialize(state, player_id);
	state.marisa_charge[player_id].age = 0U;
	state.players[player_id].shot_active = 0U;
}

void activate_marisa_hyper(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.marisa_charge[player_id];
	if(shot.active == 0U) {
		marisa_charge_initialize(state, player_id);
	}
	shot.age = 0x10U;
	state.players[player_id].shot_active = 0U;
}

void add_ellen_hyper_node(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	for(auto &node : state.ellen_charge[player_id]) {
		if(node.state != 0U) {
			continue;
		}
		node.state = 1U;
		node.phase = 0U;
		node.x = state.players[player_id].center_x.raw();
		node.y = state.players[player_id].center_y.raw();
		node.angle = static_cast<std::uint8_t>(randring_next16(state));
		node.speed = 0x50U;
		return;
	}
}

void add_kana_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	state.kana_charge_state[player_id] = 1U;
	state.kana_charge_frames[player_id] = 0U;
	constexpr std::array<std::uint8_t, 4> angles{0x90U, 0xB8U, 0xC8U, 0xF0U};
	for(std::size_t group_id = 0U; group_id < 4U; ++group_id) {
		auto &group = state.kana_charge[player_id][group_id];
		group.x.fill(center_x);
		group.y.fill(center_y);
		group.angle = angles[group_id];
		group.speed = 0x30U;
	}
}

void add_rikako_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	state.rikako_charge_state[player_id] = 1U;
	state.rikako_charge_frames[player_id] = 0U;
	state.rikako_charge_radius[player_id] = 0x80U;
	auto angle = std::uint8_t{0x20U};
	for(auto &node : state.rikako_charge[player_id]) {
		node.x = center_x;
		node.y = center_y;
		node.angle = angle;
		angle = static_cast<std::uint8_t>(angle + 0x40U);
	}
	state.rikako_charge_origin_x[player_id] = center_x;
	state.rikako_charge_origin_y[player_id] = center_y;
}

void add_chiyuri_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &nodes = state.chiyuri_charge[player_id];
	nodes[0].frame = 0U;
	nodes[0].x = state.players[player_id].center_x.raw();
	nodes[0].y = state.players[player_id].center_y.raw();
	state.players[player_id].shot_active = 0U;
	for(std::size_t node_id = 0U; node_id < nodes.size(); ++node_id) {
		nodes[node_id].state = static_cast<std::uint8_t>((node_id * 4U) + 2U);
	}
}

void add_ellen_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto center_x = state.players[player_id].center_x.raw();
	const auto center_y = state.players[player_id].center_y.raw();
	auto &nodes = state.ellen_charge[player_id];
	for(std::size_t node_id = 0U; node_id < nodes.size(); ++node_id) {
		auto &node = nodes[node_id];
		node.state = static_cast<std::uint8_t>(node_id * 2U);
		node.phase = 0U;
		node.x = center_x;
		node.y = center_y;
		node.angle = static_cast<std::uint8_t>(randring_next16(state));
		node.speed = 0x50U;
	}
}

void add_kotohime_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.kotohime_charge[player_id];
	shot.active = 1U;
	shot.x = state.players[player_id].center_x.raw();
	shot.y = state.players[player_id].center_y.raw();
	shot.velocity_y = -0x10;
}

void add_yumemi_chargeshot(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	auto &shot = state.yumemi_charge[player_id];
	shot.state = 1U;
	shot.frame = 0U;
	shot.x = state.players[player_id].center_x.raw();
	shot.y = state.players[player_id].center_y.raw();
}
