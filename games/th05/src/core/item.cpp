#include "th05/core/state.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/random.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace th05::core {
namespace {

constexpr std::int16_t item_half_width = 128;
constexpr std::int16_t item_half_height = 128;
constexpr std::int16_t item_pull_speed = 160;
constexpr std::int16_t playfield_width = 384;
constexpr std::int16_t playfield_height = 368;
constexpr std::int16_t power_max = 128;
constexpr std::int16_t power_overflow_max = 42;
constexpr std::uint8_t dream_max = 128;

constexpr std::array<std::uint16_t, power_overflow_max + 1> power_overflow_bonus{
	1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 30, 40, 50, 60, 70, 80,
	90, 100, 150, 200, 250, 300, 350, 400, 450, 500, 550, 600, 650,
	700, 750, 800, 850, 900, 950, 1000, 1050, 1100, 1200, 1250, 1280,
};

// th05/main/item/enemy_drops[data].asm.
constexpr std::array<std::uint8_t, 64> enemy_drops{
	0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1,
	0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1,
	0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 3,
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

[[nodiscard]] constexpr std::int16_t item_patnum(std::uint8_t type) noexcept
{
	return static_cast<std::int16_t>(44U + type);
}

void item_left_playfield(State &state, Presentation &presentation,
	const Item &item) noexcept
{
	switch(item.type) {
	case static_cast<std::uint8_t>(ItemType::point):
		if((state.items.dream > 1U) && (state.items.dream < dream_max)) {
			--state.items.dream;
		}
		break;

	case static_cast<std::uint8_t>(ItemType::bomb):
		playperf_lower(state, 2);
		break;

	case static_cast<std::uint8_t>(ItemType::one_up):
		playperf_lower(state, 4);
		break;
	}
	presentation.hud(HudField::dream);
}

void point_items_extend_update(State &state, Presentation &presentation) noexcept
{
	const auto threshold = static_cast<std::uint16_t>(
		(static_cast<unsigned>(state.items.extends_gained) * 100U) + 100U
	);
	if(threshold > state.items.extend_point_items_collected) return;
	playperf_raise(state, 4);
	++state.items.extends_gained;
	if(state.resources.rem_lives >= 99U) return;
	++state.resources.rem_lives;
	if(state.resources.bullet_clear_time < 20U) {
		state.resources.bullet_clear_time = 20;
	}
	presentation.hud(HudField::lives);
	presentation.popup(1);
	presentation.sound(7);
}

[[nodiscard]] bool item_collected(State &state, Presentation &presentation,
	const Item &item) noexcept
{
	if(item.type > static_cast<std::uint8_t>(ItemType::full_power)) return false;

	std::uint16_t score{};
	bool yellow{};
	switch(item.type) {
	case static_cast<std::uint8_t>(ItemType::power):
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
				if(!state.items.items_pull_to_player && state.items.dream < dream_max) {
					++state.items.dream;
				}
			}
			score = power_overflow_bonus[static_cast<std::uint16_t>(state.power_overflow)];
			presentation.hud(HudField::dream);
		}
		break;

	case static_cast<std::uint8_t>(ItemType::point): {
		std::int16_t height_above_max{};
		if(state.items.dream < dream_max) {
			height_above_max = word_sub(word_sub(item.pos.cur.y,
				static_cast<std::int16_t>(state.items.dream * 24U)), 896);
			score = 5120;
		} else {
			height_above_max = 0;
			score = state.items.item_point_score_at_full_dream;
		}
		if(height_above_max <= 0) {
			++state.items.item_playperf_raise;
			if(!state.items.items_pull_to_player) {
				const auto dream_delta = static_cast<std::int16_t>(6 - (item.pos.cur.y / 1024));
				state.items.dream = static_cast<std::uint8_t>(
					state.items.dream + dream_delta
				);
				if(state.items.dream > dream_max) state.items.dream = dream_max;
				presentation.hud(HudField::dream);
			}
			++state.items.total_max_valued_point_items_collected;
			yellow = true;
		} else {
			score = std::bit_cast<std::uint16_t>(word_sub(2800,
				static_cast<std::int16_t>(height_above_max / 2)));
		}
		++state.items.item_playperf_raise;
		++state.items.extend_point_items_collected;
		++state.items.total_point_items_collected;
		if(state.items.stage_point_items_collected < 999U) {
			++state.items.stage_point_items_collected;
		}
		point_items_extend_update(state, presentation);
		presentation.hud(HudField::point_items);
		break;
	}

	case static_cast<std::uint8_t>(ItemType::dream):
		if(state.items.dream >= dream_max) {
			score = 12800;
			yellow = true;
		} else {
			score = 1;
			state.items.dream = dream_max;
		}
		presentation.hud(HudField::dream);
		state.items.item_playperf_raise = static_cast<std::uint8_t>(
			state.items.item_playperf_raise + 2U
		);
		if(state.items.items_pull_to_player) {
			state.items.item_playperf_raise = static_cast<std::uint8_t>(
				state.items.item_playperf_raise + 2U
			);
		}
		break;

	case static_cast<std::uint8_t>(ItemType::big_power):
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
			// The source reads beyond this table at indices 43..47, but its
			// following clamp always replaces the observable payout with 2560.
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

	case static_cast<std::uint8_t>(ItemType::bomb):
		++state.bombs;
		score = 100;
		presentation.hud(HudField::bombs);
		break;

	case static_cast<std::uint8_t>(ItemType::one_up):
		playperf_raise(state, 3);
		++state.resources.rem_lives;
		presentation.hud(HudField::lives);
		presentation.sound(7);
		presentation.popup(1);
		score = 100;
		break;

	case static_cast<std::uint8_t>(ItemType::full_power):
		if(state.resources.bullet_clear_time < 20U) {
			state.resources.bullet_clear_time = 20;
		}
		presentation.popup(3);
		state.power = static_cast<std::uint8_t>(power_max);
		player_shot_level_update(state, presentation);
		score = 100;
		break;
	}

	state.score.score_delta = static_cast<std::uint32_t>(
		state.score.score_delta + static_cast<std::uint32_t>(score)
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
	state.items.enemy_drop_ring_p = static_cast<std::uint8_t>(
		pc98::irand(state.random.seed) & 0x0FU
	);
	state.items.items_pull_to_player = false;
	state.items.items_init_unused = 0;
}

void items_add(State &state, Presentation &presentation,
	pc98::th0405::SubpixelPoint position, ItemType type
) noexcept
{
	if(state.items.items_pull_to_player &&
		((static_cast<std::uint8_t>(type) < static_cast<std::uint8_t>(ItemType::big_power)) ||
			type == ItemType::enemy_drop_next)) {
		type = ItemType::power;
	}
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
		item.patnum = item_patnum(item.type);
		presentation.item_splash(position);
		item.pulled_to_player = 0;
		++state.items.items_spawned;
		break;
	}
}

bool items_miss_add(State &state, Presentation &) noexcept
{
	const auto bigpower_draw = randring_next16_mod(state.random, 5);
	if(!bigpower_draw) return false;

	std::uint16_t unused_draw{};
	for(std::size_t attempts = 0; attempts < state.random.ring.size(); ++attempts) {
		const auto draw = randring_next16_mod(state.random, 5);
		if(!draw) return false;
		unused_draw = *draw;
		if(unused_draw != *bigpower_draw) break;
		if((attempts + 1U) == state.random.ring.size()) return false;
	}

	std::size_t field = 2;
	if(state.player_pos.cur.x < 2048) {
		field = 0;
	} else if(state.player_pos.cur.x <= 4096) {
		field = 1;
	}

	std::size_t item_index{};
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
			const auto draw = randring_next16_and(state.random, 1U);
			if(!draw) return false;
			type = static_cast<ItemType>(*draw);
		} else {
			type = ItemType::big_power;
		}
		if(state.resources.rem_lives == 1U) type = ItemType::full_power;
		item.type = static_cast<std::uint8_t>(type);
		item.patnum = item_patnum(item.type);
		++item_index;
		++state.items.items_spawned;
		if(item_index >= 5U) break;
	}
	return true;
}

bool items_update(State &state, Presentation &presentation) noexcept
{
	for(auto &item : state.items.items) {
		if(item.flag == item_free) continue;
		if(item.flag == item_remove) {
			item.flag = item_free;
			continue;
		}

		if(state.items.items_pull_to_player) {
			state.pointnum_times_2 = true;
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
			item_left_playfield(state, presentation, item);
			continue;
		}
		if(position.y < -item_half_height) item.pos.cur.y = -item_half_height;
		if(item.pos.velocity.y >= 0) item.pos.velocity.x = 0;

		if(state.miss_time == 0U &&
			word_sub_unsigned(word_add(state.player_pos.cur.x, 384), position.x) <= 768U &&
			word_sub_unsigned(word_add(state.player_pos.cur.y, 384), position.y) <= 608U) {
			if(!item_collected(state, presentation, item)) return false;
			presentation.sound(11);
			item.flag = item_remove;
			continue;
		}
		item.pos.velocity.y = word_add(item.pos.velocity.y, 1);
	}
	state.pointnum_times_2 = false;
	return true;
}

} // namespace th05::core
