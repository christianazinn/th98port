#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

struct RandomState;
struct ScoreState;

inline constexpr std::size_t item_count = 20U;
inline constexpr int item_miss_count = 5;
inline constexpr std::uint8_t item_free = 0U;
inline constexpr std::uint8_t item_alive = 1U;
inline constexpr std::uint8_t item_remove = 2U;

// TH02 stores the source item_type_t enum in one byte.
enum class ItemType : std::uint8_t {
	power = 0U,
	point = 1U,
	bomb = 2U,
	big_power = 3U,
	one_up = 4U,
};

// screen_left is pixel space; screen_top is signed Q12.4 screen space.
struct ItemPosition {
	std::int16_t screen_left{};
	std::int16_t screen_top{};
	bool operator==(const ItemPosition &) const = default;
};

// Packed source item_t record. Keep the entity flag raw: F_REMOVE and any
// other nonzero source byte must remain distinguishable from F_ALIVE.
struct Item {
	std::uint8_t flag{};
	ItemType type{};
	std::array<ItemPosition, 2U> position{};
	std::int16_t velocity_y{};
	std::int16_t velocity_x_during_bounce{};
	std::int16_t age{};
	bool operator==(const Item &) const = default;
};

static_assert(sizeof(ItemPosition) == 4U);
static_assert(sizeof(Item) == 16U);
static_assert(offsetof(Item, flag) == 0U);
static_assert(offsetof(Item, type) == 1U);
static_assert(offsetof(Item, position) == 2U);
static_assert(offsetof(Item, velocity_y) == 10U);
static_assert(offsetof(Item, velocity_x_during_bounce) == 12U);
static_assert(offsetof(Item, age) == 14U);

// The pool and source globals consumed by the three completed allocation
// bodies. Item update, collection, invalidation, rendering, and the caller's
// temporary writes to miss_add_gameover remain separate source contracts.
struct ItemPoolState {
	std::array<Item, item_count> items{};
	std::uint16_t bigpower_override{};
	bool miss_add_gameover{};
	std::uint8_t semirandom_ring_p{};
	std::uint8_t semirandom_cycle{};
	bool operator==(const ItemPoolState &) const = default;
};

// th02/main/item/item.cpp::items_miss_add. It scans source slots from 0 to 19
// and initializes at most five F_FREE entries on both pages. source_player_x
// models global player_topleft.x, deliberately separate from the left spawn
// parameter; it selects the trajectory table but is not written into a slot.
void items_miss_add(ItemPoolState &state, RandomState &random,
	std::int16_t left, std::int16_t top,
	std::int16_t source_player_x) noexcept;

// th02/main/item/item.cpp::items_add. The parameter order follows the source
// definition, not its stale header declaration. It consumes a pending
// big-power override before the free-slot scan, converts a one-up only when
// lives equals the source maximum, and narrows the source int type to the
// record's raw byte. A full pool therefore still observes the prefix writes.
void items_add(ItemPoolState &state, const ScoreState &score,
	std::int16_t left, std::int16_t top, std::int16_t type) noexcept;

// th02/main/item/item.cpp::items_add_semirandom. A true result includes the
// source's cycle skip and a full-pool direct-add attempt. False is the bounded
// native refusal at an actually reached non-rare semirandom-ring read whose
// byte index is outside the ten-entry source table; its prior cycle/RNG writes
// remain observable. No item update or collection behavior is implied.
[[nodiscard]] bool items_add_semirandom(ItemPoolState &state,
	RandomState &random, const ScoreState &score, std::int16_t left,
	std::int16_t top) noexcept;

} // namespace th02::core
