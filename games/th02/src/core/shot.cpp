#include "th02/core/shot.hpp"

#include "th02/core/random.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"

#include <bit>

namespace th02::core {
namespace {

constexpr unsigned subpixel_bits = 4U;
constexpr std::int16_t shot_spawn_top_offset = 32;
constexpr std::uint8_t shot_level_max = 9;

[[nodiscard]] std::int16_t add_word(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) <<
		subpixel_bits
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t subtract_word(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) -
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] bool slot_is_valid(const PlayerShotPoolState &state,
	std::size_t back_page) noexcept
{
	return (state.current_slot >= 0) &&
		(static_cast<std::size_t>(state.current_slot) < state.shots.size()) &&
		(back_page < state.shots.front().pos_on_page.size());
}

[[nodiscard]] PlayerShot &current_shot(PlayerShotPoolState &state) noexcept
{
	return state.shots[static_cast<std::size_t>(state.current_slot)];
}

[[nodiscard]] std::uint8_t randring_angle(RandomState &random,
	std::uint8_t mask, std::uint8_t base) noexcept
{
	return static_cast<std::uint8_t>((randring_next8(random) & mask) + base);
}

[[nodiscard]] std::uint8_t homing_angle(RandomState &random,
	std::int16_t y_delta, std::int16_t x_delta,
	ShotBVolleyResult &result) noexcept
{
	const auto aimed = pc98::masterlib::iatan2_source(y_delta, x_delta);
	if(!aimed) {
		result = ShotBVolleyResult::source_iatan2_fault;
		return 0U;
	}
	const auto random_delta = static_cast<int>(randring_next8(random) & 7U) - 3;
	return static_cast<std::uint8_t>(
		static_cast<int>(*aimed) + random_delta
	);
}

} // namespace

bool shot_add(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	std::int16_t left_offset, std::uint8_t angle) noexcept
{
	if(!slot_is_valid(state, back_page)) return false;

	auto &shot = current_shot(state);
	shot.flag = player_shot_alive;
	shot.decay_cel = 0;
	shot.pos_on_page[back_page].x = to_subpixels(
		add_word(positions.player_topleft.x, left_offset)
	);
	shot.pos_on_page[back_page].y = state.spawn_top;
	// (Cos/Sin * 12) SAR 4 equals the exact shared Q8 vector at Q12.4
	// length 192. Calling it directly avoids a lossy intermediate vector.
	const auto velocity = pc98::masterlib::vector2(angle, 192);
	shot.velocity = {velocity.x, velocity.y};
	shot.patnum = state.patnum;
	shot.from_option = false;
	return true;
}

bool shot_option_add(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	std::int16_t left_offset, ShotSubpixelPoint velocity) noexcept
{
	if(!slot_is_valid(state, back_page)) return false;

	auto &shot = current_shot(state);
	shot.flag = player_shot_alive;
	shot.decay_cel = 1;
	shot.pos_on_page[back_page].x = to_subpixels(
		add_word(positions.option_left_topleft_on_back_page.x, left_offset)
	);
	shot.pos_on_page[back_page].y = to_subpixels(
		positions.option_left_topleft_on_back_page.y
	);
	shot.velocity = velocity;
	shot.patnum = state.option_patnum;
	shot.from_option = true;
	return true;
}

void shots_free_all(PlayerShotPoolState &state) noexcept
{
	for(auto &shot : state.shots) {
		shot.flag = player_shot_free;
	}
}

bool shot_a_volley(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	RandomState &random, std::uint8_t shot_level,
	std::int8_t spread_angle_delta, std::uint8_t &shot_patnum,
	std::uint8_t &option_shots_alive) noexcept
{
	if(back_page >= state.shots.front().pos_on_page.size()) return false;

	std::int16_t volley_i = 0;
	std::int16_t volley_last = 0;
	std::int16_t tmp = 0;
	state.spawn_top = to_subpixels(add_word(
		positions.player_topleft.y, shot_spawn_top_offset
	));
	state.current_slot = 0;
	// The pool writer cache and this argument represent one source global.
	// Synchronize at the selected-shot callback boundary, including a partial
	// full-power volley that rewrites the patnum before the next callback.
	state.patnum = shot_patnum;
	for(; static_cast<std::size_t>(state.current_slot) < state.shots.size();
		++state.current_slot) {
		if(current_shot(state).flag != player_shot_free) {
			continue;
		}
		switch(shot_level) {
		case 0:
			(void)shot_add(state, positions, back_page, 8, 192U);
			break;

		case 1:
			(void)shot_add(state, positions, back_page, 8,
				randring_angle(random, 3U, 190U));
			break;

		case 2:
			if(volley_i == 1) {
				tmp = 16;
			}
			(void)shot_add(state, positions, back_page, tmp,
				randring_angle(random, 7U, 188U));
			volley_last = 1;
			break;

		case 3:
			if(volley_i == 1) {
				tmp = 16;
			}
			(void)shot_add(state, positions, back_page, tmp, 192U);
			volley_last = 1;
			break;

		case 5:
			tmp = 2;
			[[fallthrough]];
		case 4:
			if(volley_i == 0) {
				(void)shot_add(state, positions, back_page, 8, 192U);
			} else if(volley_i == 1) {
				(void)shot_add(state, positions, back_page, 8,
					static_cast<std::uint8_t>(187 - tmp));
			} else {
				(void)shot_add(state, positions, back_page, 8,
					static_cast<std::uint8_t>(tmp + 197));
			}
			volley_last = 2;
			break;

		case 6:
			if(volley_i == 0) {
				(void)shot_add(state, positions, back_page, 0, 192U);
			} else if(volley_i == 1) {
				(void)shot_add(state, positions, back_page, 16, 192U);
			} else {
				tmp = static_cast<std::int16_t>(
					randring_angle(random, 31U, 177U)
				);
				(void)shot_add(state, positions, back_page, 8,
					static_cast<std::uint8_t>(tmp));
			}
			volley_last = 3;
			break;

		case 7:
			if(volley_i == 0) {
				(void)shot_add(state, positions, back_page, 0, 192U);
			} else if(volley_i == 1) {
				(void)shot_add(state, positions, back_page, 16, 192U);
			} else if(volley_i == 2) {
				(void)shot_add(state, positions, back_page, 8, 184U);
			} else {
				(void)shot_add(state, positions, back_page, 8, 200U);
			}
			volley_last = 3;
			break;

		case 8:
			if(volley_i == 0) {
				tmp = 192;
			} else if(volley_i == 1) {
				tmp = 184;
			} else if(volley_i == 2) {
				tmp = 200;
			} else if(volley_i == 3) {
				tmp = 176;
			} else {
				tmp = 208;
			}
			(void)shot_add(state, positions, back_page, 8,
				static_cast<std::uint8_t>(tmp));
			volley_last = 4;
			break;

		case shot_level_max:
			if(volley_i == 0) {
				(void)shot_add(state, positions, back_page, 0, 192U);
			} else if(volley_i == 1) {
				(void)shot_add(state, positions, back_page, 16, 192U);
			} else if(volley_i == 2) {
				state.patnum = 0x34U;
				tmp = 179;
				(void)shot_add(state, positions, back_page, 8,
					static_cast<std::uint8_t>(tmp));
			} else if(volley_i == 3) {
				tmp = 205;
				(void)shot_add(state, positions, back_page, 8,
					static_cast<std::uint8_t>(tmp));
			} else if(volley_i == 4) {
				state.patnum = 0x36U;
				tmp = static_cast<std::int16_t>(
					192 - static_cast<int>(spread_angle_delta)
				);
				(void)shot_add(state, positions, back_page, -24,
					static_cast<std::uint8_t>(tmp));
			} else {
				tmp = static_cast<std::int16_t>(
					static_cast<int>(spread_angle_delta) + 192
				);
				(void)shot_add(state, positions, back_page, 40,
					static_cast<std::uint8_t>(tmp));
			}
			volley_last = 5;
			break;
		}
		if(volley_i == volley_last) {
			break;
		}
		++volley_i;
	}
	shot_patnum = state.patnum;
	option_shots_alive = 0;
	return true;
}

ShotBVolleyResult shot_b_volley(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	RandomState &random, std::uint8_t shot_level, ShotPixelPoint boss_position,
	std::int16_t &boss_pos_x_unused, std::uint8_t &shot_patnum,
	std::uint8_t &shot_option_patnum, std::uint8_t &option_shots_alive) noexcept
{
	if(back_page >= state.shots.front().pos_on_page.size()) {
		return ShotBVolleyResult::invalid_page;
	}

	std::int16_t volley_i = 0;
	std::int16_t volley_last = 0;
	std::int16_t tmp = 0;
	bool options_phase = false;
	state.spawn_top = to_subpixels(add_word(
		positions.player_topleft.y, shot_spawn_top_offset
	));
	state.current_slot = 0;
	// These caches and the references are respectively one regular and one
	// option source global. Synchronize them at the selected-shot boundary.
	state.patnum = shot_patnum;
	state.option_patnum = shot_option_patnum;
	for(; static_cast<std::size_t>(state.current_slot) < state.shots.size();
		++state.current_slot) {
		if(current_shot(state).flag != player_shot_free) {
			continue;
		}
		if(!options_phase) {
			switch(shot_level) {
			case 0:
				(void)shot_add(state, positions, back_page, 8, 192U);
				break;

			case 1:
				(void)shot_add(state, positions, back_page, 8,
					randring_angle(random, 3U, 190U));
				break;

			case 2:
				(void)shot_add(state, positions, back_page, 8,
					randring_angle(random, 7U, 188U));
				break;

			case 3:
				if(volley_i == 0) {
					(void)shot_add(state, positions, back_page, 0,
						randring_angle(random, 7U, 188U));
				} else {
					(void)shot_add(state, positions, back_page, 16,
						randring_angle(random, 7U, 188U));
				}
				volley_last = 1;
				break;

			case 4:
				if(volley_i == 1) {
					tmp = 16;
				}
				(void)shot_add(state, positions, back_page, tmp, 192U);
				volley_last = 1;
				break;

			case 5:
				if(volley_i == 0) {
					(void)shot_add(state, positions, back_page, 0, 190U);
				} else {
					(void)shot_add(state, positions, back_page, 16, 194U);
				}
				volley_last = 1;
				break;

			case 7:
				tmp = 2;
				[[fallthrough]];
			case 6:
				if(volley_i == 0) {
					(void)shot_add(state, positions, back_page, 8, 192U);
				} else if(volley_i == 1) {
					(void)shot_add(state, positions, back_page, 8,
						static_cast<std::uint8_t>(187 - tmp));
				} else {
					(void)shot_add(state, positions, back_page, 8,
						static_cast<std::uint8_t>(tmp + 197));
				}
				volley_last = 2;
				break;

			case 8:
				if(volley_i == 0) {
					(void)shot_add(state, positions, back_page, 0, 192U);
				} else if(volley_i == 1) {
					(void)shot_add(state, positions, back_page, 16, 192U);
				} else {
					tmp = static_cast<std::int16_t>(
						randring_angle(random, 31U, 177U)
					);
					(void)shot_add(state, positions, back_page, 8,
						static_cast<std::uint8_t>(tmp));
				}
				volley_last = 3;
				break;

			case shot_level_max:
				if(volley_i == 0) {
					(void)shot_add(state, positions, back_page, 0, 192U);
				} else if(volley_i == 1) {
					(void)shot_add(state, positions, back_page, 16, 192U);
				} else if(volley_i == 2) {
					state.patnum = 0x32U;
					shot_patnum = state.patnum;
					if(boss_position.y > 0) {
						ShotBVolleyResult result = ShotBVolleyResult::completed;
						tmp = static_cast<std::int16_t>(homing_angle(random,
							subtract_word(subtract_word(boss_position.y,
								positions.player_topleft.y), 32),
							subtract_word(boss_position.x,
								positions.player_topleft.x), result));
						if(result != ShotBVolleyResult::completed) return result;
					} else {
						tmp = 183;
					}
					(void)shot_add(state, positions, back_page, 0,
						static_cast<std::uint8_t>(tmp));
				} else if(volley_i == 3) {
					if(boss_position.y > 0) {
						ShotBVolleyResult result = ShotBVolleyResult::completed;
						tmp = static_cast<std::int16_t>(homing_angle(random,
							subtract_word(subtract_word(boss_position.y,
								positions.player_topleft.y), 32),
							subtract_word(subtract_word(boss_position.x,
								positions.player_topleft.x), 16), result));
						if(result != ShotBVolleyResult::completed) return result;
					} else {
						tmp = 201;
					}
					(void)shot_add(state, positions, back_page, 16,
						static_cast<std::uint8_t>(tmp));
				}
				volley_last = 3;
				break;
			}
			if(volley_i == volley_last) {
				options_phase = true;
				volley_i = 0;
				continue;
			}
		} else {
			if(shot_level < 2U) {
				break;
			}
			if(shot_level == shot_level_max) {
				boss_pos_x_unused = boss_position.x;
			} else {
				boss_pos_x_unused = -1;
			}
			if(option_shots_alive != 0U) {
				break;
			}
			switch(shot_level) {
			case 2:
			case 3:
			case 4:
				(void)shot_option_add(state, positions, back_page,
					static_cast<std::int16_t>(volley_i * 48), {0, -4});
				volley_last = 1;
				break;

			case 5:
			case 6:
			case 7:
				if(volley_i == 0) {
					(void)shot_option_add(state, positions, back_page, 0, {0, -16});
				} else if(volley_i == 1) {
					(void)shot_option_add(state, positions, back_page, 48, {0, -16});
				} else if(volley_i == 2) {
					(void)shot_option_add(state, positions, back_page, -8, {-3, -6});
				} else {
					(void)shot_option_add(state, positions, back_page, 56, {3, -6});
				}
				volley_last = 3;
				break;

			case 8:
				if(volley_i == 0) {
					(void)shot_option_add(state, positions, back_page, 0, {0, -34});
				} else if(volley_i == 1) {
					(void)shot_option_add(state, positions, back_page, 48, {0, -34});
				} else if(volley_i == 2) {
					(void)shot_option_add(state, positions, back_page, -8, {-5, -28});
				} else if(volley_i == 3) {
					(void)shot_option_add(state, positions, back_page, 56, {5, -28});
				} else if(volley_i == 4) {
					(void)shot_option_add(state, positions, back_page, -16, {-10, -24});
				} else {
					(void)shot_option_add(state, positions, back_page, 64, {10, -24});
				}
				volley_last = 5;
				[[fallthrough]];
			case shot_level_max:
				if(volley_i == 0) {
					(void)shot_option_add(state, positions, back_page, 0, {0, -54});
				} else if(volley_i == 1) {
					(void)shot_option_add(state, positions, back_page, 48, {0, -54});
				} else if(volley_i == 2) {
					(void)shot_option_add(state, positions, back_page, -8, {-16, -50});
				} else if(volley_i == 3) {
					(void)shot_option_add(state, positions, back_page, 56, {16, -50});
				} else if(volley_i == 4) {
					(void)shot_option_add(state, positions, back_page, -16, {-32, -40});
				} else {
					(void)shot_option_add(state, positions, back_page, 64, {32, -40});
				}
				volley_last = 5;
				break;
			}
			if(volley_i == volley_last) {
				option_shots_alive = 1U;
				break;
			}
		}
		++volley_i;
	}
	shot_patnum = state.patnum;
	return ShotBVolleyResult::completed;
}

bool shot_c_volley(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	RandomState &random, std::uint8_t shot_level, std::uint8_t &shot_c_cycle,
	std::uint8_t &shot_patnum, std::uint8_t &shot_option_patnum,
	std::uint8_t &option_shots_alive) noexcept
{
	if(back_page >= state.shots.front().pos_on_page.size()) return false;

	std::int16_t volley_i = 0;
	std::int16_t volley_last = 0;
	std::int16_t tmp = 0;
	bool options_phase = false;
	++shot_c_cycle;
	state.spawn_top = to_subpixels(add_word(
		positions.player_topleft.y, shot_spawn_top_offset
	));
	state.patnum = shot_patnum;
	state.option_patnum = shot_option_patnum;
	if(shot_level >= 2U) {
		state.patnum = 0x7CU;
		shot_patnum = state.patnum;
	}
	state.current_slot = 0;
	for(; static_cast<std::size_t>(state.current_slot) < state.shots.size();
		++state.current_slot) {
		if(current_shot(state).flag != player_shot_free) {
			continue;
		}
		if(!options_phase) {
			switch(shot_level) {
			case 0:
			case 1:
			case 2:
				(void)shot_add(state, positions, back_page, 8, 192U);
				break;

			case 3:
			case 4:
			case 5:
				(void)shot_add(state, positions, back_page, 8,
					randring_angle(random, 3U, 190U));
				break;

			case 6:
			case 7:
			case 8:
				(void)shot_add(state, positions, back_page, 8,
					randring_angle(random, 7U, 188U));
				break;

			case shot_level_max:
				if(volley_i == 0) {
					(void)shot_add(state, positions, back_page, 8, 192U);
				} else {
					if(volley_i == 1) {
						tmp = -16;
						state.patnum = 0x31U;
						shot_patnum = state.patnum;
					} else if(volley_i == 2) {
						tmp = 32;
					} else if(volley_i == 3) {
						tmp = -16;
					} else if(volley_i == 4) {
						tmp = 32;
					}
					(void)shot_add(state, positions, back_page, tmp, 192U);
					if(volley_i >= 3) {
						current_shot(state).pos_on_page[back_page].y = subtract_word(
							current_shot(state).pos_on_page[back_page].y,
							to_subpixels(16)
						);
					}
				}
				volley_last = ((shot_c_cycle % 3U) == 0U) ? 4 : 0;
				break;
			}
			if(volley_i == volley_last) {
				options_phase = true;
				volley_i = 0;
				continue;
			}
		} else {
			if(shot_level < 2U) {
				break;
			}
			if(shot_level < 3U) {
				if(option_shots_alive != 0U) {
					break;
				}
			} else if(shot_level < 4U) {
				if((shot_c_cycle % 5U) != 0U) {
					break;
				}
			} else if(shot_level < 6U) {
				if((shot_c_cycle & 3U) != 0U) {
					break;
				}
			} else if(shot_level < shot_level_max) {
				if((shot_c_cycle % 3U) != 0U) {
					break;
				}
			} else if((shot_c_cycle & 1U) != 0U) {
				break;
			}
			switch(shot_level) {
			case 2:
			case 3:
				(void)shot_option_add(state, positions, back_page,
					static_cast<std::int16_t>(volley_i * 48), {0, -4});
				volley_last = 1;
				break;

			case 4:
				(void)shot_option_add(state, positions, back_page,
					static_cast<std::int16_t>(volley_i * 48), {0, -16});
				volley_last = 1;
				break;

			case 5:
				if(volley_i == 0) {
					(void)shot_option_add(state, positions, back_page, 0, {4, -16});
				} else if(volley_i == 1) {
					(void)shot_option_add(state, positions, back_page, 48, {-4, -16});
				} else if(volley_i == 2) {
					(void)shot_option_add(state, positions, back_page, -8, {0, -16});
				} else {
					(void)shot_option_add(state, positions, back_page, 56, {0, -16});
				}
				volley_last = 3;
				break;

			case 6:
				if(volley_i == 0) {
					(void)shot_option_add(state, positions, back_page, 0, {6, -24});
				} else if(volley_i == 1) {
					(void)shot_option_add(state, positions, back_page, 48, {-6, -24});
				} else if(volley_i == 2) {
					(void)shot_option_add(state, positions, back_page, -8, {0, -24});
				} else {
					(void)shot_option_add(state, positions, back_page, 56, {0, -24});
				}
				volley_last = 3;
				break;

			case 7:
				if(volley_i == 0) {
					(void)shot_option_add(state, positions, back_page, 0, {6, -26});
				} else if(volley_i == 1) {
					(void)shot_option_add(state, positions, back_page, 48, {-6, -26});
				} else if(volley_i == 2) {
					(void)shot_option_add(state, positions, back_page, -8, {0, -26});
				} else {
					(void)shot_option_add(state, positions, back_page, 56, {0, -26});
				}
				volley_last = 3;
				break;

			case 8:
				if(volley_i == 0) {
					(void)shot_option_add(state, positions, back_page, 0, {10, -48});
				} else if(volley_i == 1) {
					(void)shot_option_add(state, positions, back_page, 48, {-10, -48});
				} else if(volley_i == 2) {
					(void)shot_option_add(state, positions, back_page, -8, {-4, -48});
				} else {
					(void)shot_option_add(state, positions, back_page, 56, {4, -48});
				}
				volley_last = 3;
				break;

			case shot_level_max:
				if(volley_i == 0) {
					(void)shot_option_add(state, positions, back_page, 0, {-10, -160});
				} else if(volley_i == 1) {
					(void)shot_option_add(state, positions, back_page, 48, {10, -160});
				} else if(volley_i == 2) {
					(void)shot_option_add(state, positions, back_page, -8, {20, -140});
				} else if(volley_i == 3) {
					(void)shot_option_add(state, positions, back_page, 56, {-20, -140});
				} else if(volley_i == 4) {
					(void)shot_option_add(state, positions, back_page, -16, {-7, -120});
				} else {
					(void)shot_option_add(state, positions, back_page, 64, {7, -120});
				}
				volley_last = ((shot_c_cycle & 3U) == 0U) ? 5 : 3;
				break;
			}
			if(volley_i == volley_last) {
				option_shots_alive = 1U;
				break;
			}
		}
		++volley_i;
	}
	shot_patnum = state.patnum;
	return true;
}

} // namespace th02::core
