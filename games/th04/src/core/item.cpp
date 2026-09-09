#include "th04/core/state.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/random.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace th04::core {
namespace {

constexpr std::int16_t item_half_width = 128;
constexpr std::int16_t item_half_height = 128;
constexpr std::int16_t item_pull_speed = 160;
constexpr std::int16_t playfield_width = 384;
constexpr std::int16_t playfield_height = 368;
constexpr std::int16_t power_max = 128;
constexpr std::int16_t power_overflow_max = 42;

constexpr std::array<std::uint16_t, power_overflow_max + 1> power_overflow_bonus{
	1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 30, 40, 50, 60, 70, 80,
	90, 100, 150, 200, 250, 300, 350, 400, 450, 500, 550, 600, 650,
	700, 750, 800, 850, 900, 950, 1000, 1050, 1100, 1200, 1250, 1280,
};

constexpr std::array<std::uint8_t, 64> enemy_drops{
	0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 2,
	1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 2,
	0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 2,
	1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 3,
};

constexpr std::array<std::array<std::array<std::int16_t, 5>, 2>, 3>
	item_miss_velocities{{
		{{{-48, -56, -64, -72, -80}, {0, 12, 24, 36, 48}}},
		{{{-48, -56, -64, -56, -48}, {-24, -12, 0, 12, 24}}},
		{{{-48, -56, -64, -72, -80}, {0, -12, -24, -36, -48}}},
	}};

[[nodiscard]] constexpr std::int16_t word_add(
	std::int16_t value, std::int16_t delta
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(value) + delta
	));
}

[[nodiscard]] constexpr std::int16_t word_sub(
	std::int16_t value, std::int16_t delta
) noexcept
{
	return word_add(value, std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		0U - std::bit_cast<std::uint16_t>(delta)
	)));
}

[[nodiscard]] constexpr std::uint16_t word_sub_unsigned(
	std::int16_t value, std::int16_t delta
) noexcept
{
	return static_cast<std::uint16_t>(
		std::bit_cast<std::uint16_t>(value) - std::bit_cast<std::uint16_t>(delta)
	);
}

[[nodiscard]] constexpr std::uint16_t item_patnum(std::uint8_t type) noexcept
{
	return static_cast<std::uint16_t>(44U + type);
}

void item_left_playfield(State &state, const Item &item) noexcept
{
	switch(static_cast<ItemType>(static_cast<std::int8_t>(item.type))) {
	case ItemType::power:
	case ItemType::big_power:
		++state.items.item_playperf_lower;
		break;
	case ItemType::point:
		state.items.item_playperf_lower = static_cast<std::uint8_t>(
			state.items.item_playperf_lower + 2U
		);
		break;
	case ItemType::dream:
		state.items.item_playperf_lower = static_cast<std::uint8_t>(
			state.items.item_playperf_lower + 4U
		);
		break;
	case ItemType::bomb:
		playperf_lower(state, 2);
		break;
	case ItemType::one_up:
		playperf_lower(state, 4);
		break;
	case ItemType::enemy_drop_next:
	case ItemType::full_power:
		break;
	}
	if(state.items.item_playperf_lower >= 64U) {
		state.items.item_playperf_lower = static_cast<std::uint8_t>(
			state.items.item_playperf_lower - 48U
		);
		playperf_lower(state, 1);
	}
}

[[nodiscard]] bool item_collected(State &state, Presentation &presentation,
	const Item &item, bool pointnum_times_2) noexcept
{
	if(item.type > static_cast<std::uint8_t>(ItemType::full_power)) return false;
	std::uint16_t score{};
	bool yellow = false;
	switch(static_cast<ItemType>(static_cast<std::int8_t>(item.type))) {
	case ItemType::power:
		if(state.power < power_max) {
			if(state.power == (power_max - 1)) {
				presentation.popup(3);
				if(state.resources.bullet_clear_time < 20U) {
					state.resources.bullet_clear_time = 20;
				}
			}
			++state.power;
			player_shot_level_update(state, presentation);
			score = 1;
		} else {
			state.power_overflow = word_add(state.power_overflow, 1);
			if(static_cast<std::uint16_t>(state.power_overflow) >= power_overflow_max) {
				state.power_overflow = power_overflow_max;
				yellow = true;
			}
			score = power_overflow_bonus[static_cast<std::uint16_t>(state.power_overflow)];
			if(pointnum_times_2) ++state.items.item_playperf_raise;
		}
		break;

	case ItemType::point:
		if(item.pos.cur.y <= 832) {
			score = 5120;
			state.items.item_playperf_raise = static_cast<std::uint8_t>(
				state.items.item_playperf_raise + 4U
			);
			++state.items.total_max_valued_point_items_collected;
			yellow = true;
			if(pointnum_times_2) {
				state.items.item_playperf_raise = static_cast<std::uint8_t>(
					state.items.item_playperf_raise + 4U
				);
			}
		} else {
			score = static_cast<std::uint16_t>(3300 - (item.pos.cur.y / 2));
			state.items.item_playperf_raise = static_cast<std::uint8_t>(
				state.items.item_playperf_raise + 2U
			);
			if(pointnum_times_2) {
				state.items.item_playperf_raise = static_cast<std::uint8_t>(
					state.items.item_playperf_raise + 2U
				);
			}
		}
		++state.items.total_point_items_collected;
		score = static_cast<std::uint16_t>(score + state.items.dream_score);
		++state.items.stage_point_items_collected;
		presentation.hud(HudField::point_items);
		break;

	case ItemType::dream:
		if(state.items.dream_items_collected <= 6U) {
			++state.items.dream_items_collected;
		}
		if(state.items.dream_items_collected >= dream_score_per_items.size()) return false;
		state.items.dream_score = dream_score_per_items[state.items.dream_items_collected];
		score = state.items.dream_score;
		presentation.hud(HudField::dream);
		state.items.item_playperf_raise = static_cast<std::uint8_t>(
			state.items.item_playperf_raise + 2U
		);
		if(pointnum_times_2) {
			state.items.item_playperf_raise = static_cast<std::uint8_t>(
				state.items.item_playperf_raise + 2U
			);
		}
		break;

	case ItemType::big_power:
		if(state.power < power_max) {
			state.power = static_cast<std::uint8_t>(state.power + 10U);
			if(state.power >= power_max) {
			state.power = static_cast<std::uint8_t>(power_max);
				presentation.popup(3);
				if(state.resources.bullet_clear_time < 20U) {
					state.resources.bullet_clear_time = 20;
				}
			}
			player_shot_level_update(state, presentation);
			score = 1;
		} else {
			state.power_overflow = word_add(state.power_overflow, 5);
			// A source index above 42 reads adjacent data, but the following
			// source clamp always replaces that score with 2560. Do not invent a
			// host representation of that unobservable out-of-table read.
			if(static_cast<std::uint16_t>(state.power_overflow) <= power_overflow_max) {
				score = power_overflow_bonus[static_cast<std::uint16_t>(
					state.power_overflow
				)];
			}
			if(static_cast<std::uint16_t>(state.power_overflow) > power_overflow_max) {
				state.power_overflow = power_overflow_max;
			}
			if(state.power_overflow == power_overflow_max) {
				score = 2560;
				yellow = true;
			}
		}
		break;

	case ItemType::bomb:
		++state.bombs;
		score = 100;
		presentation.hud(HudField::bombs);
		break;

	case ItemType::one_up:
		playperf_raise(state, 3);
		++state.resources.rem_lives;
		presentation.hud(HudField::lives);
		presentation.sound(7);
		presentation.popup(1);
		score = 100;
		break;

	case ItemType::full_power:
		if(state.resources.bullet_clear_time < 20U) {
			state.resources.bullet_clear_time = 20;
		}
		presentation.popup(3);
		state.power = static_cast<std::uint8_t>(power_max);
		player_shot_level_update(state, presentation);
		score = 100;
		break;

	case ItemType::enemy_drop_next:
		return false;
	}

	const auto score_with_pull = pointnum_times_2
		? static_cast<std::uint16_t>(score * 2U) : score;
	state.score.score_delta = static_cast<std::uint32_t>(
		state.score.score_delta + static_cast<std::uint32_t>(score_with_pull)
	);
	presentation.pointnum(item.pos.cur, score, yellow);
	if(state.items.item_playperf_raise >= 32U) {
		state.items.item_playperf_raise = static_cast<std::uint8_t>(
			state.items.item_playperf_raise - 32U
		);
		playperf_raise(state, 1);
	}
	++state.items.items_collected;
	return true;
}

} // namespace

void items_init(State &state, Presentation &) noexcept
{
	state.items.enemy_drop_ring_p = static_cast<std::uint8_t>(pc98::irand(state.random.seed) & 0x0FU);
	state.items.items_pull_to_player = false;
	state.items.dream_score = 0;
}

void items_add(State &state, Presentation &presentation,
	pc98::th0405::SubpixelPoint position, ItemType type
) noexcept
{
	if(type == ItemType::enemy_drop_next) {
		++state.items.enemy_drop_ring_p;
		if((state.items.enemy_drop_ring_p & 1U) != 0U) return;
		type = static_cast<ItemType>(enemy_drops[
			(static_cast<std::size_t>(state.items.enemy_drop_ring_p) / 2U) % enemy_drops.size()
		]);
	}
	for(auto &item : state.items.items) {
		if(item.flag != item_free) continue;
		item.flag = item_alive;
		item.unknown = 0;
		item.pos.cur = position;
		item.pos.velocity = {0, -48};
		item.type = static_cast<std::uint8_t>(type);
		item.patnum = static_cast<std::int16_t>(item_patnum(item.type));
		presentation.item_splash(position);
		item.pulled_to_player = 0;
		++state.items.items_spawned;
		break;
	}
}

bool items_miss_add(State &state, Presentation &) noexcept
{
	const auto bigpower_draw = randring_next16_mod(state.random2, 5);
	if(!bigpower_draw) return false;
	std::uint16_t unused_draw{};
	for(std::size_t attempts = 0; attempts < state.random2.ring.size(); ++attempts) {
		const auto draw = randring_next16_mod(state.random2, 5);
		if(!draw) return false;
		unused_draw = *draw;
		if(unused_draw != *bigpower_draw) break;
		if((attempts + 1U) == state.random2.ring.size()) return false;
	}

	std::size_t field = 2;
	if(state.player_pos.cur.x < 2048) {
		field = 0;
	} else if(state.player_pos.cur.x <= 4096) {
		field = 1;
	}

	std::size_t item_index = 0;
	for(auto &item : state.items.items) {
		if(item.flag != item_free) continue;
		item.flag = item_alive;
		item.unknown = 0;
		item.pos.cur = state.player_pos.cur;
		item.pos.velocity.y = item_miss_velocities[field][0][item_index];
		item.pos.velocity.x = item_miss_velocities[field][1][item_index];
		item.pulled_to_player = 0;
		ItemType type{};
		if(static_cast<std::size_t>(*bigpower_draw) != item_index) {
			const auto draw = randring_next16_and(state.random2, 1U);
			if(!draw) return false;
			type = static_cast<ItemType>(*draw);
		} else {
			type = ItemType::big_power;
		}
		if(state.resources.rem_lives == 1U) type = ItemType::full_power;
		item.type = static_cast<std::uint8_t>(type);
		item.patnum = static_cast<std::int16_t>(item_patnum(item.type));
		++item_index;
		++state.items.items_spawned;
		if(item_index >= 5U) break;
	}
	return true;
}

bool items_update(State &state, Presentation &presentation) noexcept
{
	const bool pointnum_times_2 = state.items.items_pull_to_player;
	for(auto &item : state.items.items) {
		if(item.flag == item_free) continue;
		if(item.flag == item_remove) {
			item.flag = item_free;
			continue;
		}

		if(state.items.items_pull_to_player) {
			item.pulled_to_player = 1;
			const auto angle = pc98::masterlib::iatan2_source(
				word_sub(state.player_pos.cur.y, item.pos.cur.y),
				word_sub(state.player_pos.cur.x, item.pos.cur.x)
			);
			if(!angle) return false;
			const auto velocity = pc98::masterlib::vector2(*angle, item_pull_speed);
			item.pos.velocity = {velocity.x, velocity.y};
		} else if(item.pulled_to_player != 0U) {
			item.pos.velocity = {};
			item.pulled_to_player = 0;
		}

		const auto position = pc98::th0405::update_motion(item.pos);
		if(position.x <= -item_half_width ||
			position.x >= static_cast<std::int16_t>(playfield_width * 16 + item_half_width) ||
			position.y >= static_cast<std::int16_t>(playfield_height * 16 + item_half_height)) {
			item.flag = item_remove;
			item_left_playfield(state, item);
			continue;
		}
		if(position.y < -item_half_height) item.pos.cur.y = -item_half_height;
		if(item.pos.velocity.y >= 0) item.pos.velocity.x = 0;

		if(state.miss_time == 0U &&
			word_sub_unsigned(word_add(state.player_pos.cur.x, 384), position.x) <= 768U &&
			word_sub_unsigned(word_add(state.player_pos.cur.y, 384), position.y) <= 608U) {
			if(!item_collected(state, presentation, item, pointnum_times_2)) return false;
			presentation.sound(11);
			item.flag = item_remove;
			continue;
		}
		item.pos.velocity.y = word_add(item.pos.velocity.y, 1);
	}
	return true;
}

} // namespace th04::core
