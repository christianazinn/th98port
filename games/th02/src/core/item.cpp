#include "th02/core/item.hpp"

#include "th02/core/player.hpp"
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
constexpr std::int16_t item_width = 16;
constexpr std::int16_t item_height = 16;
constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_top = 16;
constexpr std::int16_t playfield_right = 416;
constexpr std::int16_t playfield_bottom = 384;
constexpr std::int16_t player_width = 32;
constexpr std::int16_t player_height = 48;
constexpr std::uint8_t power_max = 80U;
constexpr std::int8_t bombs_max = 5;
constexpr std::int8_t rank_normal = 1;
constexpr std::int8_t rank_hard = 2;
constexpr std::int8_t rank_extra = 4;
constexpr std::int16_t power_overflow_max = 42;

constexpr std::array<std::int16_t, 43U> power_overflow_bonus{
	1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 30, 40, 50, 60, 70, 80,
	90, 100, 150, 200, 250, 300, 350, 400, 450, 500, 550, 600, 650,
	700, 750, 800, 850, 900, 950, 1000, 1050, 1100, 1200, 1250, 1280,
};

constexpr std::array<std::uint8_t, 5U> item_patnums{
	68U, 69U, 70U, 71U, 87U,
};

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

[[nodiscard]] std::int16_t add_word(
	std::int16_t value, std::int16_t delta
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(value) + delta
	));
}

[[nodiscard]] std::int32_t add_dword(
	std::int32_t value, std::int32_t delta
) noexcept
{
	return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(
		static_cast<std::int64_t>(value) + delta
	));
}

[[nodiscard]] std::uint8_t add_byte(std::uint8_t value, std::uint8_t delta)
	noexcept
{
	return static_cast<std::uint8_t>(value + delta);
}

[[nodiscard]] std::int16_t subpixels_to_pixels_floor(
	std::int16_t subpixels
) noexcept
{
	const auto widened = static_cast<std::int32_t>(subpixels);
	return static_cast<std::int16_t>((widened >= 0) ?
		(widened / 16) : -(((-widened) + 15) / 16));
}

[[nodiscard]] bool power_overflow_bonus_at(std::int16_t index,
	std::int16_t &bonus) noexcept
{
	if((index < 0) || (index > power_overflow_max)) return false;
	bonus = power_overflow_bonus[static_cast<std::size_t>(index)];
	return true;
}

[[nodiscard]] bool item_patnum(ItemType type, std::uint8_t &patnum) noexcept
{
	const auto raw_type = static_cast<std::uint8_t>(type);
	if(raw_type >= item_patnums.size()) return false;
	patnum = item_patnums[raw_type];
	return true;
}

[[nodiscard]] bool player_shot_level_update(
	PlayerMissState &player_power, PlayerShotStreamState &stream,
	ItemLifecyclePresentationCallbacks &presentation
) noexcept
{
	if(!hud_shot_level_from_power(player_power.power, stream.shot_level)) {
		return false;
	}
	presentation.hud_power(player_power.power, stream.shot_level);
	return true;
}

enum class ItemCollectionResult : std::uint8_t {
	not_collected,
	collected,
	unsupported_shot_level,
	unsupported_power_overflow_index,
};

[[nodiscard]] ItemCollectionResult item_hittest_y(
	Item &item, ItemLifecycleState &lifecycle, PlayerMissState &player_power,
	PlayerShotStreamState &stream, ScoreState &score, std::int16_t left,
	std::int16_t top, std::int16_t player_top,
	ItemLifecyclePresentationCallbacks &presentation
) noexcept
{
	if((top < player_top) || (top > add_word(player_top, player_height / 2))) {
		return ItemCollectionResult::not_collected;
	}

	const auto pointnum_left = add_word(left, -12);
	std::int16_t bonus{};
	switch(item.type) {
	case ItemType::power:
		if(player_power.power < power_max) {
			player_power.power = add_byte(player_power.power, 1U);
			if(!player_shot_level_update(player_power, stream, presentation)) {
				return ItemCollectionResult::unsupported_shot_level;
			}
			player_power.power_overflow = 0;
			lifecycle.item_score_this_frame = add_dword(
				lifecycle.item_score_this_frame, 1
			);
		} else {
			if(player_power.power_overflow < power_overflow_max) {
				player_power.power_overflow = add_word(
					player_power.power_overflow, 1
				);
			}
			if(!power_overflow_bonus_at(player_power.power_overflow, bonus)) {
				return ItemCollectionResult::unsupported_power_overflow_index;
			}
			lifecycle.item_score_this_frame = add_dword(
				lifecycle.item_score_this_frame, bonus
			);
			lifecycle.collect_skill = add_byte(lifecycle.collect_skill, 1U);
		}
		lifecycle.collect_skill = add_byte(lifecycle.collect_skill, 1U);
		if(!power_overflow_bonus_at(player_power.power_overflow, bonus)) {
			return ItemCollectionResult::unsupported_power_overflow_index;
		}
		presentation.pointnum_add(pointnum_left, top,
			static_cast<std::uint16_t>(bonus));
		break;

	case ItemType::point: {
		lifecycle.point_items_collected = add_word(
			lifecycle.point_items_collected, 1
		);
		std::uint16_t value{};
		if(top <= (playfield_top + 48)) {
			lifecycle.collect_skill = add_byte(lifecycle.collect_skill, 8U);
			value = 5120U;
		} else {
			value = static_cast<std::uint16_t>(
				2800 - (static_cast<std::int32_t>(top) * 7)
			);
			if(value >= 2000U) {
				lifecycle.collect_skill = add_byte(lifecycle.collect_skill, 4U);
			} else if(value >= 1000U) {
				lifecycle.collect_skill = add_byte(lifecycle.collect_skill, 2U);
			} else {
				lifecycle.collect_skill = add_byte(lifecycle.collect_skill, 1U);
			}
		}
		presentation.pointnum_add(pointnum_left, top, value);
		lifecycle.item_score_this_frame = add_dword(
			lifecycle.item_score_this_frame, static_cast<std::int32_t>(value)
		);
		break;
	}

	case ItemType::bomb:
		if(score.bombs < bombs_max) {
			++score.bombs;
			lifecycle.item_score_this_frame = add_dword(
				lifecycle.item_score_this_frame, 1000
			);
			presentation.pointnum_add(pointnum_left, top, 1000U);
			presentation.hud_bombs(score.bombs);
		} else {
			lifecycle.item_score_this_frame = add_dword(
				lifecycle.item_score_this_frame, 6553
			);
			presentation.pointnum_add(pointnum_left, top, 6553U);
			lifecycle.collect_skill = add_byte(lifecycle.collect_skill, 16U);
		}
		break;

	case ItemType::big_power:
		if(player_power.power < (power_max - 10U)) {
			player_power.power = add_byte(player_power.power, 10U);
			lifecycle.item_score_this_frame = add_dword(
				lifecycle.item_score_this_frame, 1
			);
			player_power.power_overflow = 0;
		} else {
			if(player_power.power_overflow < (power_overflow_max - 4)) {
				player_power.power_overflow = add_word(
					player_power.power_overflow, 5
				);
			} else {
				player_power.power_overflow = power_overflow_max;
			}
			player_power.power = power_max;
			if(!power_overflow_bonus_at(player_power.power_overflow, bonus)) {
				return ItemCollectionResult::unsupported_power_overflow_index;
			}
			lifecycle.item_score_this_frame = add_dword(
				lifecycle.item_score_this_frame, bonus
			);
		}
		if(!power_overflow_bonus_at(player_power.power_overflow, bonus)) {
			return ItemCollectionResult::unsupported_power_overflow_index;
		}
		presentation.pointnum_add(pointnum_left, top,
			static_cast<std::uint16_t>(bonus));
		if(!player_shot_level_update(player_power, stream, presentation)) {
			return ItemCollectionResult::unsupported_shot_level;
		}
		break;

	case ItemType::one_up:
		if(score.lives < lives_max) {
			++score.lives;
			presentation.hud_lives(score.lives);
			presentation.sound_effect(8U);
		}
		break;
	}

	item.flag = item_remove;
	presentation.sound_effect(13U);
	if(lifecycle.collect_skill >= 32U) {
		lifecycle.item_skill = add_word(lifecycle.item_skill, 1);
		lifecycle.collect_skill = static_cast<std::uint8_t>(
			lifecycle.collect_skill - 32U
		);
	}
	return ItemCollectionResult::collected;
}

} // namespace

void items_init_and_reset(ItemPoolState &state, ItemLifecycleState &lifecycle,
	RandomState &random, ItemScheduleCallbacks &callbacks
) noexcept
{
	// randring1_next8_ge_lt(0, (SEMIRANDOM_RING_SIZE - 2)) takes the
	// power-of-two mask branch for the source's ten-entry ring.
	state.semirandom_ring_p = static_cast<std::uint8_t>(
		randring_next8(random) & 7U
	);
	for(auto &item : state.items) {
		item.flag = item_free;
	}
	lifecycle.point_items_collected = 0;
	callbacks.pointnums_init_for_rank_and_reset();
	state.miss_add_gameover = false;
}

ItemInvalidationResult items_invalidate(ItemPoolState &state,
	std::size_t page_back, std::size_t page_front,
	ItemScheduleCallbacks &callbacks
) noexcept
{
	if((page_back >= state.items[0].position.size()) ||
		(page_front >= state.items[0].position.size())) {
		return ItemInvalidationResult::invalid_page;
	}

	for(auto &item : state.items) {
		if(item.flag == item_free) continue;

		auto &back = item.position[page_back];
		callbacks.invalidate_item({
			back.screen_left,
			subpixels_to_pixels_floor(back.screen_top),
			item_width,
			item_height,
		});
		back = item.position[page_front];
		if(item.flag == item_remove) {
			item.flag = item_free;
		}
	}
	callbacks.pointnums_invalidate();
	return ItemInvalidationResult::completed;
}

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

ItemLifecycleResult items_lifecycle_step(
	ItemPoolState &items, ItemLifecycleState &lifecycle,
	PlayerMissState &player_power, PlayerShotStreamState &stream,
	ScoreState &score, const PlayerFrameState &frame,
	const PlayerHitState &hit, std::int8_t rank,
	ItemLifecyclePresentationCallbacks &presentation
) noexcept
{
	lifecycle.frame_unused = add_word(lifecycle.frame_unused, 1);
	lifecycle.item_score_this_frame = 0;

	if(frame.page_back >= items.items[0].position.size()) {
		return ItemLifecycleResult::invalid_page;
	}

	for(auto &item : items.items) {
		if(item.flag != item_alive) continue;

		auto &position = item.position[frame.page_back];
		if(item.velocity_y <= 0) {
			position.screen_left = add_word(
				position.screen_left, item.velocity_x_during_bounce
			);
		}
		position.screen_top = add_word(position.screen_top, item.velocity_y);

		const auto left = position.screen_left;
		auto top = subpixels_to_pixels_floor(position.screen_top);
		if(top < (playfield_top - item_height)) {
			top = playfield_top - item_height;
			position.screen_top = to_subpixels(top);
		} else if(top >= playfield_bottom) {
			item.flag = item_remove;
			lifecycle.drop_cycle = add_byte(lifecycle.drop_cycle, 1U);
			if((lifecycle.drop_cycle & 0x0FU) == 0U) {
				lifecycle.item_skill = add_word(lifecycle.item_skill, -1);
			}
			continue;
		}

		if((left <= (playfield_left - item_width)) || (left >= playfield_right)) {
			item.flag = item_remove;
			continue;
		}

		item.age = add_word(item.age, 1);
		item.velocity_y = add_word(item.velocity_y, 1);

		if((hit.raw_hit == 0U) &&
			(left >= add_word(frame.player.current.x, -8)) &&
			(left <= add_word(frame.player.current.x, 24))) {
			switch(item_hittest_y(item, lifecycle, player_power, stream, score,
				left, top, frame.player.current.y, presentation)) {
			case ItemCollectionResult::not_collected:
				break;
			case ItemCollectionResult::collected:
				continue;
			case ItemCollectionResult::unsupported_shot_level:
				return ItemLifecycleResult::unsupported_shot_level;
			case ItemCollectionResult::unsupported_power_overflow_index:
				return ItemLifecycleResult::unsupported_power_overflow_index;
			}
		}

		std::uint8_t patnum{};
		if(!item_patnum(item.type, patnum)) {
			return ItemLifecycleResult::unsupported_item_patnum;
		}
		presentation.render_item({left, top, patnum});
	}

	if(lifecycle.item_score_this_frame != 0) {
		if(rank >= rank_hard) {
			if(rank > rank_extra) return ItemLifecycleResult::unsupported_rank;
			lifecycle.item_score_this_frame = std::bit_cast<std::int32_t>(
				static_cast<std::uint32_t>(lifecycle.item_score_this_frame) <<
				static_cast<unsigned>(rank - rank_normal)
			);
		}
		score.score_delta = add_dword(
			score.score_delta, lifecycle.item_score_this_frame
		);
	}
	presentation.pointnums_update_and_render();
	return ItemLifecycleResult::completed;
}

} // namespace th02::core
