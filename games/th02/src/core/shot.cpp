#include "th02/core/shot.hpp"

#include "th02/core/random.hpp"
#include "th02/core/score.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"

#include <array>
#include <bit>

namespace th02::core {
namespace {

constexpr unsigned subpixel_bits = 4U;
constexpr std::int16_t shot_spawn_top_offset = 32;
constexpr std::uint8_t shot_level_max = 9;
constexpr std::size_t shot_hittest_count = 35U;
constexpr std::int16_t shot_hittest_margin = 8;
constexpr std::int16_t shot_decay_large_offset = 8;
constexpr std::uint8_t shot_patnum_hit_animated = 0x32U;
constexpr std::uint8_t shot_patnum_hit_large = 0x78U;
constexpr std::uint8_t shot_patnum_large = 0x7CU;
constexpr std::uint8_t shot_decay_cels = 5U;
constexpr std::uint8_t shot_option_decay_cels = 4U;
constexpr std::uint8_t shot_option_flying_cel = 1U;
constexpr std::uint8_t shot_option_unaccelerated_patnum = 0x3BU;
constexpr std::int16_t shot_option_acceleration = 4;
constexpr std::int16_t shot_remove_left = static_cast<std::int16_t>(
	16 << subpixel_bits
);
constexpr std::int16_t shot_remove_right = static_cast<std::int16_t>(
	418 << subpixel_bits
);
constexpr std::int16_t shot_remove_top = 8;
constexpr std::int16_t shot_remove_bottom = 384;
constexpr std::int16_t shot_width = 16;
constexpr std::int16_t shot_height = 16;
constexpr std::int16_t shot_decay_large_width = 32;
constexpr std::int16_t shot_decay_large_height = 32;
constexpr std::array<std::int16_t, shot_level_max + 1U> shot_hit_damage{
	5, 5, 4, 4, 3, 3, 3, 2, 2, 1,
};
constexpr std::array<std::int16_t, shot_level_max + 1U> shot_option_hit_damage{
	2, 2, 2, 2, 2, 1, 1, 1, 1, 1,
};
constexpr std::array<std::int16_t, shot_level_max + 1U> shot_hit_damage_large{
	0, 0, 8, 8, 7, 7, 6, 5, 4, 3,
};

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

[[nodiscard]] std::int16_t arithmetic_shift_right(
	std::int16_t value, unsigned int count
) noexcept
{
	const auto divisor = static_cast<std::int32_t>(1) << count;
	if(value >= 0) return static_cast<std::int16_t>(value / divisor);
	return static_cast<std::int16_t>(
		-(((-static_cast<std::int32_t>(value)) + divisor - 1) / divisor)
	);
}

[[nodiscard]] std::int16_t to_pixels(std::int16_t subpixels) noexcept
{
	return arithmetic_shift_right(subpixels, subpixel_bits);
}

void score_delta_add_low_word(ScoreState &score, std::int16_t damage) noexcept
{
	const auto existing = std::bit_cast<std::uint32_t>(score.score_delta);
	const auto low = static_cast<std::uint16_t>(existing);
	const auto added = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(low) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(damage))
	);
	const auto updated = (existing & 0xFFFF0000U) | added;
	score.score_delta = std::bit_cast<std::int32_t>(updated);
}

[[nodiscard]] bool slot_is_valid(const PlayerShotPoolState &state,
	std::size_t back_page) noexcept
{
	return (state.current_slot >= 0) &&
		(static_cast<std::size_t>(state.current_slot) < state.shots.size()) &&
		(back_page < state.shots.front().pos_on_page.size());
}

[[nodiscard]] bool page_is_valid(std::size_t page) noexcept
{
	return page < PlayerShot{}.pos_on_page.size();
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

std::optional<std::int16_t> shots_hittest(PlayerShotPoolState &state,
	ScoreState &score, std::size_t back_page, std::uint8_t shot_level,
	std::uint32_t stage_frame, bool bombing, std::uint8_t bomb_damage_frame_mask,
	PlayerShotHitbox hitbox, PlayerShotHitCallbacks &callbacks) noexcept
{
	if(back_page >= state.shots.front().pos_on_page.size() ||
		shot_level > shot_level_max) {
		return std::nullopt;
	}

	const auto damage_hit = shot_hit_damage[shot_level];
	const auto damage_option = shot_option_hit_damage[shot_level];
	const auto damage_large = shot_hit_damage_large[shot_level];
	// The source derives this before moving the shared left edge. The regular
	// branch reads it; the option branch re-spells left + width afterwards.
	const auto right = add_word(hitbox.topleft.x, hitbox.width);
	const auto top_edge = subtract_word(hitbox.topleft.y, shot_hittest_margin);
	const auto left = subtract_word(hitbox.topleft.x, shot_hittest_margin);
	const auto bottom_edge = add_word(top_edge, hitbox.height);
	std::int16_t damage = 0;

	for(std::size_t i = 0; i < shot_hittest_count; ++i) {
		auto &shot = state.shots[i];
		if(shot.flag != player_shot_alive) continue;

		auto &position = shot.pos_on_page[back_page];
		const auto shot_left = to_pixels(position.x);
		if(!shot.from_option) {
			if(shot.decay_cel != 0U || shot_left <= left || shot_left >= right) {
				continue;
			}
			const auto shot_top = to_pixels(position.y);
			if(shot_top <= top_edge || shot_top >= bottom_edge) continue;

			damage = add_word(damage, 1);
			if(shot.patnum >= shot_patnum_hit_animated) {
				shot.velocity.x = arithmetic_shift_right(shot.velocity.x, 3U);
				shot.velocity.y = arithmetic_shift_right(shot.velocity.y, 3U);
				if(shot.patnum >= shot_patnum_hit_large) {
					shot.decay_cel = 1U;
					position.x = subtract_word(position.x,
						to_subpixels(shot_decay_large_offset));
					position.y = subtract_word(position.y,
						to_subpixels(shot_decay_large_offset));
					damage = add_word(damage, damage_large);
				} else {
					shot.decay_cel = 2U;
					damage = add_word(damage, damage_hit);
				}
			}
			// shot_left is intentionally the pre-offset X. The Y value is re-read
			// after a large shot's centered-decay position mutation.
			callbacks.shot_hit_spark({
				{shot_left, to_pixels(position.y)}, 30, 1, false,
			});
		} else {
			const auto option_right = add_word(left, hitbox.width);
			if(shot_left <= left || shot_left >= option_right) continue;
			const auto shot_top = to_pixels(position.y);
			if(shot_top <= top_edge || shot_top >= bottom_edge) continue;

			if(shot.decay_cel == 1U) {
				callbacks.shot_hit_spark({
					{shot_left, to_pixels(position.y)}, 40, 1, false,
				});
				shot.decay_cel = 2U;
			}
			damage = add_word(damage, damage_option);
		}
	}
	if(bombing && (static_cast<std::uint8_t>(stage_frame) &
		bomb_damage_frame_mask) == 0U) {
		damage = add_word(damage, 1);
	}
	score_delta_add_low_word(score, damage);
	return damage;
}

PlayerShotLifetimeUpdateResult shots_update_lifetime(
	PlayerShotPoolState &state, PlayerShotLifetimeState &lifetime,
	std::size_t back_page, std::uint32_t stage_frame
) noexcept
{
	if(!page_is_valid(back_page)) {
		return PlayerShotLifetimeUpdateResult::invalid_page;
	}

	for(std::size_t i = 0U; i < state.shots.size(); ++i) {
		auto &shot = state.shots[i];
		if(shot.flag != player_shot_alive) continue;

		auto &position = shot.pos_on_page[back_page];
		position.x = add_word(position.x, shot.velocity.x);
		position.y = add_word(position.y, shot.velocity.y);
		const auto top = to_pixels(position.y);
		if(position.x <= shot_remove_left || position.x >= shot_remove_right ||
			top > shot_remove_bottom || top < shot_remove_top) {
			shot.flag = player_shot_remove;
			continue;
		}

		if(!shot.from_option) {
			lifetime.animation_frames[i] = static_cast<std::uint8_t>(
				lifetime.animation_frames[i] + 1U
			);
			if(shot.decay_cel == 0U) continue;
			if(shot.decay_cel > shot_decay_cels) {
				shot.flag = player_shot_remove;
				continue;
			}
			if((stage_frame & 3U) == 0U) {
				shot.decay_cel = static_cast<std::uint8_t>(shot.decay_cel + 1U);
			}
			continue;
		}

		if(shot.decay_cel > shot_option_flying_cel) {
			// The signed byte promotes before C++ converts it to the uint32_t
			// type of stage_frame. Thus a negative source period becomes a
			// large uint32_t divisor, not an unsigned byte or signed modulo.
			const auto period = static_cast<std::uint32_t>(
				static_cast<std::int32_t>(lifetime.option_decay_interval)
			);
			if(period == 0U) {
				return PlayerShotLifetimeUpdateResult::source_divide_fault;
			}
			if((stage_frame % period) == 0U) {
				shot.decay_cel = static_cast<std::uint8_t>(shot.decay_cel + 1U);
				if(shot.decay_cel > shot_option_decay_cels) {
					shot.flag = player_shot_remove;
					continue;
				}
			}
			if((stage_frame & 1U) != 0U) {
				shot.velocity.y = arithmetic_shift_right(shot.velocity.y, 1U);
			}
		} else if(shot.patnum != shot_option_unaccelerated_patnum) {
			shot.velocity.y = subtract_word(
				shot.velocity.y, shot_option_acceleration
			);
		}
	}
	return PlayerShotLifetimeUpdateResult::completed;
}

bool shots_invalidate_lifetime(PlayerShotPoolState &state,
	PlayerShotLifetimeState &lifetime, std::size_t back_page,
	std::size_t front_page, bool reduce_effects,
	PlayerShotInvalidationCallbacks &callbacks
) noexcept
{
	if(!page_is_valid(back_page) || !page_is_valid(front_page)) return false;

	std::uint8_t options_alive = 0U;
	for(std::size_t i = 0U; i < state.shots.size(); ++i) {
		auto &shot = state.shots[i];
		if(shot.flag == player_shot_free) continue;

		if(!reduce_effects || back_page == (i & 1U)) {
			const auto &position = shot.pos_on_page[back_page];
			const auto regular_size = shot.patnum < shot_patnum_large ||
				shot.decay_cel == 0U;
			callbacks.invalidate_shot({
				{to_pixels(position.x), to_pixels(position.y)},
				regular_size ? shot_width : shot_decay_large_width,
				regular_size ? shot_height : shot_decay_large_height,
			});
		}
		if(shot.flag == player_shot_remove) {
			shot.flag = player_shot_free;
			continue;
		}
		shot.pos_on_page[back_page] = shot.pos_on_page[front_page];
		if(shot.from_option) {
			options_alive = static_cast<std::uint8_t>(options_alive + 1U);
		}
	}
	if(options_alive == 0U) lifetime.option_shots_alive = 0U;
	return true;
}

} // namespace th02::core
