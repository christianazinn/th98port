#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th05::core {

struct State;
struct Presentation;

inline constexpr std::size_t item_count = 40;
inline constexpr std::uint8_t item_free = 0;
inline constexpr std::uint8_t item_alive = 1;
inline constexpr std::uint8_t item_remove = 2;

enum class ItemType : std::int8_t {
	none = -2,
	enemy_drop_next = -1,
	power = 0,
	point = 1,
	dream = 2,
	big_power = 3,
	bomb = 4,
	one_up = 5,
	full_power = 6,
};

// th04/main/item/items[bss].asm item_t. The raw flag, type, and bool16
// storage remain distinct from the portable API types.
struct Item {
	std::uint8_t flag{}, unused{};
	pc98::th0405::Motion pos{};
	std::uint8_t type{}, unknown{};
	std::int16_t patnum{};
	std::uint16_t pulled_to_player{};
	bool operator==(const Item &) const = default;
};
static_assert(sizeof(Item) == 20U);

struct ItemState {
	std::array<Item, item_count> items{};
	std::uint8_t enemy_drop_ring_p{};
	std::uint8_t item_playperf_raise{}, item_playperf_lower{};
	std::uint16_t items_init_unused{};
	std::uint16_t item_point_score_at_full_dream{};
	std::uint8_t dream{};
	std::uint16_t stage_point_items_collected{};
	std::uint16_t extend_point_items_collected{};
	std::uint8_t extends_gained{};
	std::uint16_t items_spawned{}, items_collected{};
	std::uint16_t total_point_items_collected{};
	std::uint16_t total_max_valued_point_items_collected{};
	bool items_pull_to_player{};
	bool operator==(const ItemState &) const = default;
};

// th04/main/item/init.cpp. It does not clear item slots, dream, or counters.
void items_init(State &state, Presentation &presentation) noexcept;

// th04/main/item/add.cpp. A full pool and every odd enemy-drop-ring request
// are source-successful no-ops after their reached writes.
void items_add(State &state, Presentation &presentation,
	pc98::th0405::SubpixelPoint position, ItemType type) noexcept;

// th04/main/item/miss_add.cpp. False reports an unsupported random state or
// a source-equivalent nonterminating duplicate-ring draw; prior draws remain.
[[nodiscard]] bool items_miss_add(State &state, Presentation &presentation) noexcept;

// th04/main/item/update.cpp, including TH05 collection and off-playfield
// behavior. False reports a source iatan2 fault or unsupported raw item type.
[[nodiscard]] bool items_update(State &state, Presentation &presentation) noexcept;

} // namespace th05::core
