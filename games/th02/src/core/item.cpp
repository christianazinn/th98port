#include "th02/core/item.hpp"

#include "th02/core/random.hpp"
#include "th02/core/score.hpp"

#include <array>
#include <bit>

namespace th02::core {
namespace {

constexpr std::int16_t miss_field_left_max = 128;
constexpr std::int16_t miss_field_center_max = 256;
constexpr std::int8_t lives_max = 5;
constexpr std::int16_t item_bounce_velocity_y = -48;
constexpr std::uint16_t semirandom_bomb_value = 18U;
constexpr std::uint16_t semirandom_bomb_modulus = 512U;

constexpr std::array<std::int8_t, item_miss_count> velocity_y_sides{
	-80, -68, -56, -44, -32,
};
constexpr std::array<std::int8_t, item_miss_count> velocity_x_center{
	-2, -1, 0, 1, 2,
};
constexpr std::array<std::int8_t, item_miss_count> velocity_y_center{
	-56, -68, -80, -68, -56,
};
constexpr std::array<ItemType, 10U> semirandom_ring{
	ItemType::power,
	ItemType::point,
	ItemType::power,
	ItemType::point,
	ItemType::point,
	ItemType::power,
	ItemType::power,
	ItemType::point,
	ItemType::point,
	ItemType::power,
};

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int8_t field_distance_from_center(
	std::int16_t source_player_x
) noexcept
{
	if(source_player_x <= miss_field_left_max) return -1;
	if(source_player_x <= miss_field_center_max) return 0;
	return 1;
}

[[nodiscard]] ItemType random_power_or_point(RandomState &random) noexcept
{
	return static_cast<ItemType>(
		randring_next8(random) % static_cast<std::uint8_t>(ItemType::bomb)
	);
}

[[nodiscard]] ItemType narrow_item_type(std::int16_t type) noexcept
{
	return static_cast<ItemType>(static_cast<std::uint8_t>(type));
}

} // namespace

void items_miss_add(ItemPoolState &state, RandomState &random,
	std::int16_t left, std::int16_t top, std::int16_t source_player_x
) noexcept
{
	int items_added = 0;
	bool bigpower_not_spawned_yet = true;
	const auto field_distance = field_distance_from_center(source_player_x);
	const auto spawn = ItemPosition{left, to_subpixels(top)};

	for(auto &item : state.items) {
		if(item.flag != item_free) continue;

		item.flag = item_alive;
		item.position[0] = spawn;
		item.position[1] = spawn;
		item.age = 0;

		if(!state.miss_add_gameover) {
			if(bigpower_not_spawned_yet &&
				((randring_next8(random) % (item_miss_count - items_added)) == 0U)) {
				item.type = ItemType::big_power;
				bigpower_not_spawned_yet = false;
			} else {
				item.type = random_power_or_point(random);
			}
		} else {
			item.type = ItemType::big_power;
		}

		if(field_distance != 0) {
			item.velocity_y = velocity_y_sides[static_cast<std::size_t>(items_added)];
			item.velocity_x_during_bounce = static_cast<std::int16_t>(
				static_cast<int>(field_distance) * -2
			);
		} else {
			item.velocity_y = velocity_y_center[static_cast<std::size_t>(items_added)];
			item.velocity_x_during_bounce = velocity_x_center[
				static_cast<std::size_t>(items_added)
			];
		}

		++items_added;
		if(items_added >= item_miss_count) break;
	}
}

void items_add(ItemPoolState &state, const ScoreState &score,
	std::int16_t left, std::int16_t top, std::int16_t type
) noexcept
{
	if((state.bigpower_override != 0U) &&
		(type < static_cast<std::int16_t>(ItemType::bomb))) {
		--state.bigpower_override;
		type = static_cast<std::int16_t>(ItemType::big_power);
	}

	if((score.lives == lives_max) &&
		(type == static_cast<std::int16_t>(ItemType::one_up))) {
		type = static_cast<std::int16_t>(ItemType::bomb);
	}

	const auto spawn = ItemPosition{left, to_subpixels(top)};
	for(auto &item : state.items) {
		if(item.flag != item_free) continue;
		item.flag = item_alive;
		item.type = narrow_item_type(type);
		item.position[0] = spawn;
		item.position[1] = spawn;
		item.velocity_y = item_bounce_velocity_y;
		item.velocity_x_during_bounce = 0;
		item.age = 0;
		break;
	}
}

bool items_add_semirandom(ItemPoolState &state, RandomState &random,
	const ScoreState &score, std::int16_t left, std::int16_t top
) noexcept
{
	if(state.bigpower_override != 0U) {
		--state.bigpower_override;
		items_add(state, score, left, top,
			static_cast<std::int16_t>(ItemType::big_power));
		return true;
	}

	const auto cycle = state.semirandom_cycle;
	state.semirandom_cycle = static_cast<std::uint8_t>(cycle + 1U);
	if((cycle % 3U) != 0U) return true;

	const auto random_value = randring_next16(random);
	if((random_value % semirandom_bomb_modulus) == semirandom_bomb_value) {
		items_add(state, score, left, top,
			static_cast<std::int16_t>(ItemType::bomb));
	} else {
		if(state.semirandom_ring_p >= semirandom_ring.size()) return false;
		items_add(state, score, left, top, static_cast<std::int16_t>(
			semirandom_ring[state.semirandom_ring_p]
		));
	}

	state.semirandom_ring_p = static_cast<std::uint8_t>(
		state.semirandom_ring_p + 1U
	);
	if(state.semirandom_ring_p >= semirandom_ring.size()) {
		state.semirandom_ring_p = 0U;
	}
	return true;
}

} // namespace th02::core
