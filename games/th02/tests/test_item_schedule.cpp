#include "th02/core/item.hpp"
#include "th02/core/random.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

enum class EventKind : std::uint8_t {
	pointnums_init,
	item_invalidate,
	pointnums_invalidate,
};

struct Event {
	EventKind kind{};
	ItemInvalidation invalidation{};
	bool operator==(const Event &) const = default;
};

class Recorder final : public ItemScheduleCallbacks {
public:
	const ItemPoolState *items{};
	const ItemLifecycleState *lifecycle{};
	std::size_t expected_back_page{1U};
	std::array<std::size_t, 3U> expected_slots{};
	std::array<ItemPosition, 3U> expected_back{};
	std::array<std::uint8_t, 3U> expected_flags{};
	std::array<Event, item_count + 2U> events{};
	std::size_t count{};
	bool init_prefix_seen{true};
	bool invalidation_prefix_seen{true};

	void pointnums_init_for_rank_and_reset() noexcept override
	{
		bool all_free = true;
		for(const auto &item : items->items) {
			all_free = all_free && (item.flag == item_free);
		}
		init_prefix_seen = init_prefix_seen && all_free &&
			(lifecycle->point_items_collected == 0) &&
			(items->semirandom_ring_p == 5U) && items->miss_add_gameover;
		push({EventKind::pointnums_init, {}});
	}

	void invalidate_item(ItemInvalidation invalidation) noexcept override
	{
		if(count >= expected_slots.size()) {
			invalidation_prefix_seen = false;
		} else {
			const auto index = count;
			const auto &item = items->items[expected_slots[index]];
			invalidation_prefix_seen = invalidation_prefix_seen &&
				(item.flag == expected_flags[index]) &&
				(item.position[expected_back_page] == expected_back[index]);
		}
		push({EventKind::item_invalidate, invalidation});
	}

	void pointnums_invalidate() noexcept override
	{
		push({EventKind::pointnums_invalidate, {}});
	}

private:
	void push(Event event) noexcept
	{
		events[count++] = event;
	}
};

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] ItemPoolState patterned_pool() noexcept
{
	ItemPoolState state{};
	state.bigpower_override = 0x1234U;
	state.miss_add_gameover = true;
	state.semirandom_ring_p = 0xAAU;
	state.semirandom_cycle = 0xBBU;
	for(std::size_t i = 0U; i < state.items.size(); ++i) {
		auto &item = state.items[i];
		item.flag = static_cast<std::uint8_t>(i + 1U);
		item.type = static_cast<ItemType>(static_cast<std::uint8_t>(i + 20U));
		item.position[0] = {
			static_cast<std::int16_t>(100 + static_cast<int>(i)),
			static_cast<std::int16_t>(200 + static_cast<int>(i)),
		};
		item.position[1] = {
			static_cast<std::int16_t>(300 + static_cast<int>(i)),
			static_cast<std::int16_t>(400 + static_cast<int>(i)),
		};
		item.velocity_y = static_cast<std::int16_t>(500 + static_cast<int>(i));
		item.velocity_x_during_bounce = static_cast<std::int16_t>(
			600 + static_cast<int>(i)
		);
		item.age = static_cast<std::int16_t>(700 + static_cast<int>(i));
	}
	return state;
}

[[nodiscard]] ItemLifecycleState patterned_lifecycle() noexcept
{
	return {
		0x1234,
		0x12345678,
		0x12U,
		0x34U,
		0x4567,
		0x5678,
	};
}

[[nodiscard]] RandomState patterned_random() noexcept
{
	RandomState state{};
	state.seed = 0xCAFEBABEU;
	for(std::size_t i = 0U; i < state.ring.size(); ++i) {
		state.ring[i] = static_cast<std::uint8_t>(i);
	}
	return state;
}

[[nodiscard]] bool init_only_writes_its_source_fields_and_callback_prefix() noexcept
{
	auto items = patterned_pool();
	const auto items_before = items;
	auto lifecycle = patterned_lifecycle();
	const auto lifecycle_before = lifecycle;
	auto random = patterned_random();
	random.cursor = 0xFEU;
	random.ring[0xFEU] = 0xFDU;
	const auto random_before = random;
	Recorder callbacks{};
	callbacks.items = &items;
	callbacks.lifecycle = &lifecycle;

	items_init_and_reset(items, lifecycle, random, callbacks);

	auto expected_items = items_before;
	expected_items.semirandom_ring_p = 5U;
	expected_items.miss_add_gameover = false;
	for(auto &item : expected_items.items) item.flag = item_free;
	auto expected_lifecycle = lifecycle_before;
	expected_lifecycle.point_items_collected = 0;
	auto expected_random = random_before;
	expected_random.cursor = 0xFFU;
	return callbacks.init_prefix_seen && (callbacks.count == 1U) &&
		(callbacks.events[0U] == Event{EventKind::pointnums_init, {}}) &&
		(items == expected_items) && (lifecycle == expected_lifecycle) &&
		(random == expected_random);
}

[[nodiscard]] bool invalidation_order_raw_flags_and_page_copy() noexcept
{
	ItemPoolState items{};
	items.bigpower_override = 77U;
	items.miss_add_gameover = true;
	items.semirandom_ring_p = 6U;
	items.semirandom_cycle = 7U;
	items.items[0U] = {
		item_free, ItemType::power,
		{{{20, to_subpixels(30)}, {40, to_subpixels(50)}}}, -3, -4, -5,
	};
	items.items[1U] = {
		item_alive, ItemType::point,
		{{{100, to_subpixels(101)}, {80, -17}}}, -3, -4, -5,
	};
	items.items[2U] = {
		item_remove, ItemType::bomb,
		{{{220, to_subpixels(221)}, {200, to_subpixels(203)}}}, -6, -7, -8,
	};
	items.items[3U] = {
		0x7FU, ItemType::big_power,
		{{{320, to_subpixels(-10)}, {300, to_subpixels(-1)}}}, -9, -10, -11,
	};
	const auto before = items;
	Recorder callbacks{};
	callbacks.items = &items;
	callbacks.expected_slots = {1U, 2U, 3U};
	callbacks.expected_back = {
		before.items[1U].position[1U],
		before.items[2U].position[1U],
		before.items[3U].position[1U],
	};
	callbacks.expected_flags = {item_alive, item_remove, 0x7FU};

	if(items_invalidate(items, 1U, 0U, callbacks) !=
		ItemInvalidationResult::completed) {
		return false;
	}

	if(!callbacks.invalidation_prefix_seen || callbacks.count != 4U) return false;
	constexpr std::array<Event, 4U> expected_events{{
		{EventKind::item_invalidate, {80, -2, 16, 16}},
		{EventKind::item_invalidate, {200, 203, 16, 16}},
		{EventKind::item_invalidate, {300, -1, 16, 16}},
		{EventKind::pointnums_invalidate, {}},
	}};
	for(std::size_t i = 0U; i < expected_events.size(); ++i) {
		if(callbacks.events[i] != expected_events[i]) return false;
	}
	if(items.items[0U] != before.items[0U] ||
		items.items[1U].flag != item_alive ||
		items.items[1U].position[1U] != before.items[1U].position[0U] ||
		items.items[2U].flag != item_free ||
		items.items[2U].position[1U] != before.items[2U].position[0U] ||
		items.items[3U].flag != 0x7FU ||
		items.items[3U].position[1U] != before.items[3U].position[0U]) {
		return false;
	}
	return items.bigpower_override == before.bigpower_override &&
		items.miss_add_gameover == before.miss_add_gameover &&
		items.semirandom_ring_p == before.semirandom_ring_p &&
		items.semirandom_cycle == before.semirandom_cycle;
}

[[nodiscard]] bool same_page_is_explicit_and_invalid_pages_do_nothing() noexcept
{
	ItemPoolState items{};
	items.items[0U] = {
		item_remove, ItemType::power,
		{{{123, -17}, {456, to_subpixels(99)}}}, 7, 8, 9,
	};
	const auto same_page_before = items;
	Recorder same_page_callbacks{};
	same_page_callbacks.items = &items;
	same_page_callbacks.expected_back_page = 0U;
	same_page_callbacks.expected_slots = {0U, 0U, 0U};
	same_page_callbacks.expected_back = {
		same_page_before.items[0U].position[0U], {}, {}
	};
	same_page_callbacks.expected_flags = {item_remove, 0U, 0U};
	if(items_invalidate(items, 0U, 0U, same_page_callbacks) !=
		ItemInvalidationResult::completed ||
		!same_page_callbacks.invalidation_prefix_seen ||
		same_page_callbacks.count != 2U ||
		items.items[0U].flag != item_free ||
		items.items[0U].position != same_page_before.items[0U].position) {
		return false;
	}

	const auto before_invalid = items;
	Recorder invalid_callbacks{};
	invalid_callbacks.items = &items;
	if(items_invalidate(items, 2U, 0U, invalid_callbacks) !=
		ItemInvalidationResult::invalid_page || items != before_invalid ||
		invalid_callbacks.count != 0U) {
		return false;
	}
	return items_invalidate(items, 0U, 2U, invalid_callbacks) ==
		ItemInvalidationResult::invalid_page && (items == before_invalid) &&
		(invalid_callbacks.count == 0U);
}

} // namespace

int main()
{
	if(!init_only_writes_its_source_fields_and_callback_prefix()) {
		std::fputs("FAIL item init/reset source writes and callback prefix\n", stderr);
		return EXIT_FAILURE;
	}
	if(!invalidation_order_raw_flags_and_page_copy()) {
		std::fputs("FAIL item invalidation source order/pages/raw flags\n", stderr);
		return EXIT_FAILURE;
	}
	if(!same_page_is_explicit_and_invalid_pages_do_nothing()) {
		std::fputs("FAIL item invalidation same/invalid pages\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
