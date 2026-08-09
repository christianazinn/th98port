// Included by frame.cpp inside its private implementation namespace.

void extra_attacks_update_reimu(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot = 0U; slot < extra_attack_regular_count; ++slot) {
		auto &attack = state.extra_attacks[player_id][slot];
		if(attack.state == 0U) {
			continue;
		}
		if(attack.state == 1U) {
			attack.center_x = exact::wrap_add(
				attack.center_x, attack.velocity_x
			);
			if(attack.center_x <= 0 || attack.center_x >= (288 << 4)) {
				attack.velocity_x = exact::wrap_negate(attack.velocity_x);
				attack.center_x = exact::wrap_add(
					attack.center_x, attack.velocity_x
				);
			}
			attack.center_y = exact::wrap_add(
				attack.center_y, attack.velocity_y
			);
			if(attack.center_y < -(24 << 4)) {
				attack.velocity_y = exact::wrap_negate(attack.velocity_y);
				attack.center_y = -(24 << 4);
			}
			if(attack.center_y >= (368 << 4)) {
				attack.state = 0U;
				continue;
			}
			attack.velocity_y = exact::wrap_add(
				attack.velocity_y, static_cast<std::int16_t>(1)
			);
			extra_attack_shot_hittest(
				state, target_player, attack.center_x, attack.center_y
			);
			collmap_set_rect_striped(
				state, target_player, attack.center_x, attack.center_y, 8, 16
			);
			collmap_set_rect_striped(
				state,
				target_player,
				exact::wrap_subtract(
					attack.center_x, static_cast<std::int16_t>(12 << 4)
				),
				attack.center_y,
				4,
				8
			);
			collmap_set_rect_striped(
				state,
				target_player,
				exact::wrap_add(
					attack.center_x, static_cast<std::int16_t>(12 << 4)
				),
				attack.center_y,
				4,
				8
			);
		} else if(attack.state == 2U) {
			if(extra_attack_transfer_update(attack)) {
				auto angle = static_cast<std::uint8_t>(
					randring_next16(state) & 0x1FU
				);
				angle = static_cast<std::uint8_t>(
					angle + ((randring_next16(state) & 1U) * 0x60U)
				);
				attack.angle = static_cast<std::uint8_t>(angle + 0x80U);
				const auto velocity = vector2(
					attack.angle,
					static_cast<std::int16_t>((state.round_speed / 8U) + 0x32U)
				);
				attack.velocity_x = velocity.x;
				attack.velocity_y = velocity.y;
			}
		} else if(attack.state <= 0x14U) {
			attack.state++;
		} else {
			attack.state = 1U;
		}
	}
}

void extra_attacks_update_mima(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot_id = 0U; slot_id < extra_attack_mima_count; ++slot_id) {
		auto &attack = state.extra_attacks[player_id][slot_id];
		if(attack.state == 0U) {
			continue;
		}

		if(attack.state == 1U) {
			const auto center_x = exact::wrap_add(
				attack.center_x, attack.velocity_x
			);
			const auto center_y = exact::wrap_add(
				attack.center_y, attack.velocity_y
			);
			if(center_x <= 0 || center_x >= (288 << 4)) {
				if(attack.frame == 0U) {
					attack.state = 0U;
					continue;
				}
				attack.frame--;
				attack.velocity_x = exact::wrap_negate(attack.velocity_x);
				attack.angle = iatan2(attack.velocity_y, attack.velocity_x);
			} else if(center_y <= 0 || center_y >= (368 << 4)) {
				if(attack.frame == 0U) {
					attack.state = 0U;
					continue;
				}
				attack.frame--;
				attack.velocity_y = exact::wrap_negate(attack.velocity_y);
				attack.angle = iatan2(attack.velocity_y, attack.velocity_x);
			} else {
				attack.center_x = center_x;
				attack.center_y = center_y;
			}

			const auto timer = attack.mima_timer;
			if(timer >= 0x10U && timer < 0x20U && (timer & 1U) != 0U) {
				attack.speed = static_cast<std::uint8_t>(attack.speed + 1U);
			} else if(timer >= 0x20U && timer < 0x50U) {
				attack.speed = static_cast<std::uint8_t>(attack.speed + 1U);
			}
			if(timer < 0x50U) {
				const auto velocity = vector2(
					attack.angle, static_cast<std::int16_t>(attack.speed)
				);
				attack.velocity_x = velocity.x;
				attack.velocity_y = velocity.y;
			}
			if(
				state.defeat_flag != DefeatFlag::banner &&
				extra_attack_shot_hittest(
					state, target_player, center_x, center_y
				)
			) {
				attack.velocity_y = exact::wrap_subtract(
					attack.velocity_y, std::int16_t{8}
				);
			}
			collmap_set_rect_striped(
				state, target_player, center_x, center_y, 8, 8
			);
		} else if(attack.state == 2U) {
			if(extra_attack_transfer_update(attack)) {
				attack.angle = static_cast<std::uint8_t>(randring_next16(state));
				attack.speed = 0U;
				attack.velocity_x = 0;
				attack.velocity_y = 0;
				continue;
			}
		} else if(attack.state <= 0x1CU) {
			attack.state++;
		} else {
			attack.mima_timer = 0U;
			attack.state = 1U;
		}
		attack.mima_timer = exact::wrap_add(
			attack.mima_timer, std::uint16_t{1}
		);
	}
}

void extra_attacks_update_marisa(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot_id = 0U; slot_id < extra_attack_marisa_count; ++slot_id) {
		auto &attack = state.extra_attacks[player_id][slot_id];
		if(attack.state == 0U) {
			continue;
		}

		if(attack.state == 1U) {
			if(attack.frame < 0x30U) {
				attack.center_y = exact::wrap_subtract(
					attack.center_y, std::int16_t{16 << 4}
				);
				if(attack.center_y < -(16 << 4)) {
					attack.frame = 0x30U;
					attack.center_y = -(16 << 4);
				}
			} else if(attack.frame >= 0x78U) {
				attack.state = 0U;
				continue;
			}

			auto top = attack.center_y;
			if(top < 0) {
				top = 0;
			}
			const auto collmap_h = static_cast<std::int16_t>((368 << 4) - top);
			if(attack.frame < 0x64U) {
				collmap_set_rect_striped(
					state,
					target_player,
					attack.center_x,
					static_cast<std::int16_t>(top + (collmap_h / 2)),
					2,
					static_cast<int>(collmap_h / 32)
				);
			}
		} else if(attack.state == 2U) {
			if(extra_attack_transfer_update(attack)) {
				attack.center_y = 368 << 4;
				continue;
			}
		} else if(attack.state <= 0x1CU) {
			attack.state++;
			// State 0x0C only spawns a SUB_CE0C visual, which has no RNG or
			// gameplay-owned state in the headless core.
		} else {
			attack.frame = 0U;
			attack.state = 1U;
		}
		attack.frame++;
	}
}

void extra_attacks_update_kana(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot = 0U; slot < extra_attack_regular_count; ++slot) {
		auto &attack = state.extra_attacks[player_id][slot];
		if(attack.state == 0U) {
			continue;
		}
		if(attack.state == 1U) {
			const auto velocity = vector2(
				attack.angle, static_cast<std::int16_t>(attack.speed)
			);
			attack.speed++;
			attack.center_x = exact::wrap_add(attack.center_x, velocity.x);
			attack.center_y = exact::wrap_add(attack.center_y, velocity.y);
			if((attack.frame & 0x0FU) == 0U) {
				bullet_add_kana_extra_attack(
					state, attack.center_x, attack.center_y, target_player
				);
			}
			if(playfield_clip(
				attack.center_x, attack.center_y, -(24 << 4)
			)) {
				attack.state = 0U;
				continue;
			}
		} else if(attack.state == 2U) {
			(void)extra_attack_transfer_update(attack);
		} else if(attack.state <= 0x1CU) {
			attack.state++;
		} else {
			attack.frame = 0U;
			attack.state = 1U;
			attack.speed = 8U;
		}
		attack.frame++;
	}
}

void extra_attacks_update_kotohime(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot = 0U; slot < 10U; ++slot) {
		auto &attack = state.extra_attacks[player_id][slot];
		if(attack.state == 0U) {
			continue;
		}
		if(attack.state == 1U) {
			if(attack.center_y >= attack.kotohime_target_y) {
				if(attack.kotohime_milestone == 4U) {
					if(attack.behavior_variant != 0U) {
						auto speed = static_cast<std::uint8_t>(1U << 4U);
						auto angle = static_cast<std::uint8_t>(randring_next16(state));
						const auto angle_delta =
							(state.gauge_pattern_param_b[player_id] != 0U)
							? std::int8_t{-12}
							: std::int8_t{12};
						for(std::size_t firing = 0U; firing < 4U; ++firing) {
							bullets_add_group(
								state,
								state.kotohime_gauge_pattern_type[player_id],
								attack.center_x,
								attack.center_y,
								target_player,
								speed,
								angle,
								bullet_group_ring,
								state.gauge_pattern_param_a[player_id]
							);
							speed = static_cast<std::uint8_t>(speed + 2U);
							angle = static_cast<std::uint8_t>(angle + angle_delta);
						}
						state.bullet_template_angle = angle;
					}
				} else if(attack.kotohime_milestone == 0x10U) {
					auto angle = static_cast<std::uint8_t>(randring_next16(state));
					if(attack.behavior_variant == 0U) {
						bullets_add_group(
							state,
							BulletFlag::bullet16,
							attack.center_x,
							attack.center_y,
							target_player,
							(1U << 4U) + 8U,
							angle,
							bullet_group_ring,
							10U
						);
					} else {
						auto speed = static_cast<std::uint8_t>(1U << 4U);
						const auto angle_delta =
							(state.gauge_pattern_param_b[player_id] != 0U)
							? std::int8_t{7}
							: std::int8_t{-7};
						for(std::size_t firing = 0U; firing < 10U; ++firing) {
							bullets_add_group(
								state,
								state.kotohime_gauge_pattern_type[player_id],
								attack.center_x,
								attack.center_y,
								target_player,
								speed,
								angle,
								bullet_group_ring,
								state.gauge_pattern_param_a[player_id]
							);
							speed = static_cast<std::uint8_t>(speed + 2U);
							angle = static_cast<std::uint8_t>(angle + angle_delta);
						}
						state.bullet_template_angle = angle;
					}
					attack.state = 0U;
				}
				attack.kotohime_milestone++;
			} else {
				attack.center_y = exact::wrap_add(
					attack.center_y,
					static_cast<std::int16_t>(attack.kotohime_velocity_y)
				);
				extra_attack_shot_hittest(
					state, target_player, attack.center_x, attack.center_y
				);
				collmap_set_rect_striped(
					state, target_player, attack.center_x, attack.center_y, 6, 6
				);
			}
		} else if(attack.state == 2U) {
			(void)extra_attack_transfer_update(attack);
		} else if(attack.state <= 0x1CU) {
			attack.state++;
		} else {
			attack.frame = 0U;
			attack.state = 1U;
		}
		attack.frame++;
	}
}

void extra_attacks_update_ellen(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot_id = 0U;
		slot_id < extra_attack_ellen_count; ++slot_id) {
		auto &attack = state.extra_attacks[player_id][slot_id];
		if(attack.state == 0U) {
			continue;
		}
		if(attack.state == 1U) {
			for(std::size_t trail = attack.ellen_trail_x.size() - 1U;
				trail > 0U; --trail) {
				attack.ellen_trail_x[trail] = attack.ellen_trail_x[trail - 1U];
				attack.ellen_trail_y[trail] = attack.ellen_trail_y[trail - 1U];
			}
			if(attack.ellen_radius <= 0x0D00) {
				const auto offset = vector2(attack.angle, attack.ellen_radius);
				attack.ellen_trail_x[0] = exact::wrap_add(
					attack.center_x, offset.x
				);
				attack.ellen_trail_y[0] = exact::wrap_add(
					attack.center_y, offset.y
				);
				attack.velocity_x = exact::wrap_subtract(
					attack.ellen_trail_x[0], attack.ellen_trail_x[1]
				);
				attack.velocity_y = exact::wrap_subtract(
					attack.ellen_trail_y[0], attack.ellen_trail_y[1]
				);
				attack.ellen_radius = exact::wrap_add(
					attack.ellen_radius, static_cast<std::int16_t>(0x18)
				);
				attack.angle = static_cast<std::uint8_t>(
					attack.angle + attack.ellen_angle_delta
				);
			} else {
				if(
					playfield_clip(
						attack.ellen_trail_x.front(),
						attack.ellen_trail_y.front(),
						-(24 << 4)
					) &&
					playfield_clip(
						attack.ellen_trail_x.back(),
						attack.ellen_trail_y.back(),
						-(24 << 4)
					)
				) {
					attack.state = 0U;
					continue;
				}
				attack.ellen_trail_x[0] = exact::wrap_add(
					attack.ellen_trail_x[0], attack.velocity_x
				);
				attack.ellen_trail_y[0] = exact::wrap_add(
					attack.ellen_trail_y[0], attack.velocity_y
				);
			}
			extra_attack_shot_hittest(
				state,
				target_player,
				attack.ellen_trail_x[0],
				attack.ellen_trail_y[0]
			);
			collmap_set_rect_striped(
				state,
				target_player,
				attack.ellen_trail_x[0],
				attack.ellen_trail_y[0],
				6,
				6
			);
		} else if(attack.state == 2U) {
			if(extra_attack_transfer_update(attack)) {
				attack.ellen_trail_x.fill(attack.center_x);
				attack.ellen_trail_y.fill(attack.center_y);
			}
		} else if(attack.state <= 0x1CU) {
			attack.state++;
		} else {
			attack.frame = 0U;
			attack.state = 1U;
		}
		attack.frame++;
	}
}

void extra_attacks_update_rikako(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot = 0U; slot < extra_attack_rikako_count; ++slot) {
		auto &attack = state.extra_attacks[player_id][slot];
		if(attack.state == 0U) {
			continue;
		}
		if(attack.state == 1U) {
			const auto velocity = vector2(
				attack.angle, static_cast<std::int16_t>(attack.speed)
			);
			attack.center_x = exact::wrap_add(attack.center_x, velocity.x);
			attack.center_y = exact::wrap_add(attack.center_y, velocity.y);
			if(
				attack.behavior_variant == 0U && attack.frame >= 0x40U &&
				attack.frame <= 0x50U
			) {
				const auto target_angle = iatan2(
					exact::wrap_subtract(
						state.players[target_player].center_y.raw(), attack.center_y
					),
					exact::wrap_subtract(
						state.players[target_player].center_x.raw(), attack.center_x
					)
				);
				attack.speed++;
				const auto angle_delta = std::bit_cast<std::int8_t>(
					static_cast<std::uint8_t>(target_angle - attack.angle)
				);
				attack.angle = static_cast<std::uint8_t>(
					attack.angle + static_cast<std::int8_t>(angle_delta / 8)
				);
			}
			if(playfield_clip(
				attack.center_x, attack.center_y, -(32 << 4)
			)) {
				attack.state = 0U;
				continue;
			}
			extra_attack_shot_hittest(
				state, target_player, attack.center_x, attack.center_y
			);
			collmap_set_rect_striped(
				state, target_player, attack.center_x, attack.center_y, 8, 16
			);
			collmap_set_rect_striped(
				state,
				target_player,
				exact::wrap_subtract(
					attack.center_x, static_cast<std::int16_t>(12 << 4)
				),
				attack.center_y,
				4,
				8
			);
			collmap_set_rect_striped(
				state,
				target_player,
				exact::wrap_add(
					attack.center_x, static_cast<std::int16_t>(12 << 4)
				),
				attack.center_y,
				4,
				8
			);
		} else if(attack.state == 2U) {
			(void)extra_attack_transfer_update(attack);
		} else if(attack.state <= 0x1CU) {
			attack.state++;
		} else {
			attack.frame = 0U;
			attack.state = 1U;
		}
		attack.frame++;
	}
}

void extra_attacks_update_chiyuri(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(std::size_t slot_id = 0U; slot_id < extra_attack_count; slot_id += 2U) {
		auto &top = state.extra_attacks[player_id][slot_id];
		if(top.state == 0U) {
			continue;
		}
		auto &bottom = state.extra_attacks[player_id][slot_id + 1U];
		if(top.state == 1U) {
			top.frame++;
			if(top.frame > 0x28U && top.frame < 0x70U) {
				collmap_set_slope_striped(
					state, target_player, top.center_x, bottom.center_x
				);
			} else if(top.frame > 0x88U) {
				top.state = 0U;
				bottom.state = 0U;
			}
			continue;
		}

		bool top_done = false;
		if(top.state == 2U) {
			if(extra_attack_transfer_update(top)) {
				top.center_y = 0;
			}
		} else if(top.state <= 0x1CU) {
			top.state++;
		} else {
			top.frame = 0U;
			top_done = true;
		}
		top.frame++;

		if(bottom.state == 2U) {
			if(extra_attack_transfer_update(bottom)) {
				bottom.center_y = 368 << 4;
			}
		} else if(bottom.state <= 0x1CU) {
			bottom.state++;
		} else {
			bottom.frame = 0U;
			if(top_done) {
				top.state = 1U;
				// ZUN decrements the raw slot pointer back to the top half here.
				// The shared slot[1]++ below therefore advances the top frame a
				// second time and leaves the bottom frame at zero.
				top.frame++;
				continue;
			}
		}
		bottom.frame++;
	}
}

void extra_attacks_update_yumemi(
	DeterministicState &state,
	std::uint8_t player_id
) noexcept
{
	const auto target_player = static_cast<std::uint8_t>(1U - player_id);
	for(auto &attack : state.extra_attacks[player_id]) {
		if(attack.state == 0U) {
			continue;
		}
		if(attack.state == 1U) {
			if(attack.frame <= 8U) {
				attack.yumemi_extent = exact::wrap_add(
					attack.yumemi_extent, std::uint16_t{6U}
				);
			} else if(
				attack.frame >= attack.yumemi_duration &&
				attack.frame < static_cast<std::uint16_t>(
					attack.yumemi_duration + 8U
				)
			) {
				attack.yumemi_extent = exact::wrap_subtract(
					attack.yumemi_extent, std::uint16_t{6U}
				);
			} else if(
				attack.frame >= static_cast<std::uint16_t>(
					attack.yumemi_duration + 8U
				)
			) {
				// The source still executes this frame's hit tests and map writes.
				attack.state = 0U;
			}

			const auto extent_radius = static_cast<std::int16_t>(
				attack.yumemi_extent << 3U
			);
			constexpr std::int16_t narrow_radius = 4 << 4;
			(void)extra_attack_bounds_hittest(
				state,
				target_player,
				exact::wrap_subtract(attack.center_x, extent_radius),
				exact::wrap_add(attack.center_x, extent_radius),
				exact::wrap_subtract(attack.center_y, narrow_radius),
				exact::wrap_add(attack.center_y, narrow_radius)
			);
			(void)extra_attack_bounds_hittest(
				state,
				target_player,
				exact::wrap_subtract(attack.center_x, narrow_radius),
				exact::wrap_add(attack.center_x, narrow_radius),
				exact::wrap_subtract(attack.center_y, extent_radius),
				exact::wrap_add(attack.center_y, extent_radius)
			);
			collmap_set_rect_striped(
				state,
				target_player,
				attack.center_x,
				attack.center_y,
				static_cast<int>(attack.yumemi_extent),
				8
			);
			const auto vertical_center_y = exact::wrap_add(
				attack.center_y,
				static_cast<std::int16_t>(attack.yumemi_extent << 2U)
			);
			const auto extent_signed = exact::from_bits<std::int16_t>(
				attack.yumemi_extent
			);
			const auto vertical_height = static_cast<int>(exact::wrap_add(
				extent_signed,
				static_cast<std::int16_t>(extent_signed / 4)
			));
			collmap_set_rect_striped(
				state,
				target_player,
				attack.center_x,
				vertical_center_y,
				8,
				vertical_height
			);
		} else if(attack.state == 2U) {
			if(extra_attack_transfer_update(attack)) {
				continue;
			}
		} else if(attack.state <= 0x28U) {
			attack.state++;
		} else {
			attack.yumemi_extent = 0x10U;
			attack.frame = 0U;
			attack.state = 1U;
		}
		attack.frame++;
	}
}
