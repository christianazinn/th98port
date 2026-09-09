#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th04::core {

struct State;
struct Presentation;

inline constexpr std::size_t item_count = 32;
inline constexpr std::uint8_t item_free = 0;
inline constexpr std::uint8_t item_alive = 1;
inline constexpr std::uint8_t item_remove = 2;

enum class ItemType : std::int8_t {
	enemy_drop_next = -1,
	power = 0,
	point = 1,
	dream = 2,
	big_power = 3,
	bomb = 4,
	one_up = 5,
	full_power = 6,
};

// th04/main/item/items[data].asm. Values are the source's score values
// divided by 10, as held in its 16-bit unsigned-int table.
inline constexpr std::array<std::uint16_t, 8> dream_score_per_items{
	0, 100, 200, 400, 600, 800, 1000, 1280,
};

// th04/main/item/items[bss].asm item_t. Keep the raw flag, type, and bool16
// storage separate from the portable enum/bool forms used by the API.
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
	std::uint8_t dream_items_collected{};
	std::uint16_t dream_score{};
	std::uint8_t stage_point_items_collected{};
	std::uint16_t items_spawned{}, items_collected{};
	std::uint16_t total_point_items_collected{};
	std::uint16_t total_max_valued_point_items_collected{};
	bool items_pull_to_player{};
	bool operator==(const ItemState &) const = default;
};

// th04/main/item/init.cpp. It intentionally does not clear item slots or the
// collection counters; stage setup owns those source stores.
void items_init(State &state, Presentation &presentation) noexcept;

// th04/main/item/add.cpp. A full pool and every odd enemy-drop-ring request
// are source-successful no-ops after their reached prefix writes.
void items_add(State &state, Presentation &presentation,
	pc98::th0405::SubpixelPoint position, ItemType type) noexcept;

// th04/main/item/miss_add.cpp. False reports an unsupported random state or
// a source-equivalent nonterminating duplicate-ring draw; earlier RNG writes
// are preserved.
[[nodiscard]] bool items_miss_add(State &state, Presentation &presentation) noexcept;

// th04/main/item/update.cpp, including collection and left-playfield rank
// penalties. False reports a source iatan2 division fault during a pull, or
// an invalid raw item/dream-table state; source writes before that point are
// retained.
[[nodiscard]] bool items_update(State &state, Presentation &presentation) noexcept;

} // namespace th04::core
