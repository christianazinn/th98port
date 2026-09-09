#include "th02/core/item.hpp"
#include "th02/core/player.hpp"
#include "th02/core/score.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>

namespace {

using namespace th02::core;

enum class EventKind : std::uint8_t {
	hud_power,
	hud_bombs,
	hud_lives,
	sound,
	pointnum,
	item,
	pointnums_update,
};

struct Event {
	EventKind kind{};
	std::int32_t first{};
	std::int32_t second{};
	std::int32_t third{};
	bool operator==(const Event &) const = default;
};

class Recorder final : public ItemLifecyclePresentationCallbacks {
public:
	std::array<Event, 64U> events{};
	std::size_t count{};

	void hud_power(std::uint8_t power, std::uint8_t shot_level) noexcept override
	{
		push({EventKind::hud_power, power, shot_level, 0});
	}

	void hud_bombs(std::int8_t bombs) noexcept override
	{
		push({EventKind::hud_bombs, bombs, 0, 0});
	}

	void hud_lives(std::int8_t lives) noexcept override
	{
		push({EventKind::hud_lives, lives, 0, 0});
	}

	void sound_effect(std::uint8_t sound_id) noexcept override
	{
		push({EventKind::sound, sound_id, 0, 0});
	}

	void pointnum_add(std::int16_t left, std::int16_t top,
		std::uint16_t points
	) noexcept override
	{
		push({EventKind::pointnum, left, top, points});
	}

	void render_item(ItemSprite sprite) noexcept override
	{
		push({EventKind::item, sprite.left, sprite.top, sprite.patnum});
	}

	void pointnums_update_and_render() noexcept override
	{
		push({EventKind::pointnums_update, 0, 0, 0});
	}

private:
	void push(Event event) noexcept
	{
		events[count++] = event;
	}
};

struct Fixture {
	ItemPoolState items{};
	ItemLifecycleState lifecycle{};
	PlayerMissState player_power{};
	PlayerShotStreamState stream{};
	ScoreState score{};
	PlayerFrameState frame{};
	PlayerHitState hit{};
	Recorder presentation{};
};

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] Fixture fixture() noexcept
{
	Fixture result{};
	result.frame.page_front = 0U;
	result.frame.page_back = 1U;
	result.frame.player.current = {200, 100};
	result.frame.player.on_page[0] = {190, 90};
	result.frame.player.on_page[1] = {200, 100};
	result.player_power.power = 79U;
	result.score.lives = 4;
	result.score.bombs = 4;
	return result;
}

void add_item(Fixture &state, std::size_t slot, ItemType type,
	std::int16_t left, std::int16_t top, std::int16_t velocity_y = 0
) noexcept
{
	auto &item = state.items.items[slot];
	item.flag = item_alive;
	item.type = type;
	item.position[0] = {static_cast<std::int16_t>(left - 50),
		to_subpixels(static_cast<std::int16_t>(top - 50))};
	item.position[1] = {left, to_subpixels(top)};
	item.velocity_y = velocity_y;
	item.velocity_x_during_bounce = 0;
	item.age = 0;
}

[[nodiscard]] ItemLifecycleResult step(
	Fixture &state, std::int8_t rank = 1
) noexcept
{
	return items_lifecycle_step(state.items, state.lifecycle, state.player_power,
		state.stream, state.score, state.frame, state.hit, rank,
		state.presentation);
}

[[nodiscard]] bool event_at(const Recorder &recorder, std::size_t index,
	Event expected
) noexcept
{
	return (index < recorder.count) && (recorder.events[index] == expected);
}

[[nodiscard]] bool all_item_types_preserve_ordered_state_and_effects() noexcept
{
	auto state = fixture();
	for(std::size_t i = 0U; i < 5U; ++i) {
		add_item(state, i, static_cast<ItemType>(i), 200, 100);
	}

	if(step(state) != ItemLifecycleResult::completed ||
		state.lifecycle.frame_unused != 1 || state.player_power.power != 80U ||
		state.player_power.power_overflow != 5 || state.stream.shot_level != 9U ||
		state.score.lives != 5 || state.score.bombs != 5 ||
		state.score.score_delta != 3106 ||
		state.lifecycle.point_items_collected != 1 ||
		state.lifecycle.collect_skill != 5U || state.lifecycle.item_skill != 0) {
		return false;
	}
	for(std::size_t i = 0U; i < 5U; ++i) {
		if(state.items.items[i].flag != item_remove ||
			state.items.items[i].age != 1 || state.items.items[i].velocity_y != 1 ||
			state.items.items[i].position[0] != ItemPosition{
				150, 800
			}) {
			return false;
		}
	}

	constexpr std::array<Event, 15U> expected{{
		{EventKind::hud_power, 80, 9, 0},
		{EventKind::pointnum, 188, 100, 1},
		{EventKind::sound, 13, 0, 0},
		{EventKind::pointnum, 188, 100, 2100},
		{EventKind::sound, 13, 0, 0},
		{EventKind::pointnum, 188, 100, 1000},
		{EventKind::hud_bombs, 5, 0, 0},
		{EventKind::sound, 13, 0, 0},
		{EventKind::pointnum, 188, 100, 5},
		{EventKind::hud_power, 80, 9, 0},
		{EventKind::sound, 13, 0, 0},
		{EventKind::hud_lives, 5, 0, 0},
		{EventKind::sound, 8, 0, 0},
		{EventKind::sound, 13, 0, 0},
		{EventKind::pointnums_update, 0, 0, 0},
	}};
	if(state.presentation.count != expected.size()) return false;
	for(std::size_t i = 0U; i < expected.size(); ++i) {
		if(!event_at(state.presentation, i, expected[i])) return false;
	}
	return true;
}

[[nodiscard]] bool inclusive_edges_and_raw_hit_gate() noexcept
{
	auto edges = fixture();
	add_item(edges, 0U, ItemType::power, 192, 100);
	add_item(edges, 1U, ItemType::point, 224, 124);
	if(step(edges) != ItemLifecycleResult::completed ||
		edges.items.items[0].flag != item_remove ||
		edges.items.items[1].flag != item_remove ||
		edges.player_power.power != 80U ||
		edges.lifecycle.point_items_collected != 1 ||
		edges.score.score_delta != 1 + (2800 - (124 * 7))) {
		return false;
	}

	auto hit = fixture();
	hit.hit.raw_hit = 0xFFU;
	add_item(hit, 0U, ItemType::power, 200, 100);
	if(step(hit) != ItemLifecycleResult::completed ||
		hit.items.items[0].flag != item_alive || hit.player_power.power != 79U ||
		hit.items.items[0].age != 1 || hit.items.items[0].velocity_y != 1 ||
		hit.presentation.count != 2U ||
		!event_at(hit.presentation, 0U, {EventKind::item, 200, 100, 68}) ||
		!event_at(hit.presentation, 1U, {EventKind::pointnums_update, 0, 0, 0})) {
		return false;
	}
	return true;
}

[[nodiscard]] bool movement_removal_and_wrap_boundaries() noexcept
{
	auto state = fixture();
	state.lifecycle.drop_cycle = 0xFFU;
	state.lifecycle.item_skill = 7;
	add_item(state, 0U, ItemType::power, 300, -1);
	add_item(state, 1U, ItemType::point, 300, 383, 16);
	add_item(state, 2U, ItemType::bomb, 16, 200);
	add_item(state, 3U, ItemType::big_power, 416, 200);
	if(step(state) != ItemLifecycleResult::completed ||
		state.items.items[0].flag != item_alive ||
		state.items.items[0].position[1].screen_top != 0 ||
		state.items.items[0].age != 1 || state.items.items[0].velocity_y != 1 ||
		state.items.items[1].flag != item_remove ||
		state.items.items[1].age != 0 || state.items.items[1].velocity_y != 16 ||
		state.items.items[2].flag != item_remove || state.items.items[2].age != 0 ||
		state.items.items[3].flag != item_remove || state.items.items[3].age != 0 ||
		state.lifecycle.drop_cycle != 0U || state.lifecycle.item_skill != 6 ||
		state.presentation.count != 2U ||
		!event_at(state.presentation, 0U, {EventKind::item, 300, 0, 68}) ||
		!event_at(state.presentation, 1U, {EventKind::pointnums_update, 0, 0, 0})) {
		return false;
	}
	return true;
}

[[nodiscard]] bool rank_scaling_and_dword_wrap_are_source_sized() noexcept
{
	struct RankExpectation {
		std::int8_t rank{};
		std::int32_t expected{};
	};
	for(const auto expectation : std::array<RankExpectation, 6U>{{
		{static_cast<std::int8_t>(-1), 1}, {0, 1}, {1, 1},
		{2, 2}, {3, 4}, {4, 8},
	}}) {
		auto state = fixture();
		add_item(state, 0U, ItemType::power, 200, 100);
		if(step(state, expectation.rank) != ItemLifecycleResult::completed ||
			state.score.score_delta != expectation.expected) {
			return false;
		}
	}

	auto wrap = fixture();
	wrap.score.score_delta = std::numeric_limits<std::int32_t>::max();
	wrap.frame.player.current.y = 64;
	add_item(wrap, 0U, ItemType::point, 200, 64);
	if(step(wrap) != ItemLifecycleResult::completed ||
		wrap.score.score_delta != (std::numeric_limits<std::int32_t>::min() + 5119)) {
		return false;
	}

	auto unsupported = fixture();
	unsupported.score.score_delta = 44;
	add_item(unsupported, 0U, ItemType::power, 200, 100);
	if(step(unsupported, 5) != ItemLifecycleResult::unsupported_rank ||
		unsupported.items.items[0].flag != item_remove ||
		unsupported.score.score_delta != 44 || unsupported.presentation.count != 3U ||
		!event_at(unsupported.presentation, 2U, {EventKind::sound, 13, 0, 0})) {
		return false;
	}
	return true;
}

[[nodiscard]] bool refusals_preserve_the_source_prefix() noexcept
{
	auto invalid_page = fixture();
	invalid_page.lifecycle.frame_unused = 7;
	invalid_page.lifecycle.item_score_this_frame = 99;
	invalid_page.frame.page_back = 2U;
	if(step(invalid_page) != ItemLifecycleResult::invalid_page ||
		invalid_page.lifecycle.frame_unused != 8 ||
		invalid_page.lifecycle.item_score_this_frame != 0 ||
		invalid_page.presentation.count != 0U) {
		return false;
	}

	auto overflow = fixture();
	overflow.player_power.power = 80U;
	overflow.player_power.power_overflow = -2;
	add_item(overflow, 0U, ItemType::power, 200, 100);
	if(step(overflow) != ItemLifecycleResult::unsupported_power_overflow_index ||
		overflow.player_power.power_overflow != -1 ||
		overflow.items.items[0].flag != item_alive ||
		overflow.items.items[0].age != 1 || overflow.items.items[0].velocity_y != 1 ||
		overflow.lifecycle.item_score_this_frame != 0 || overflow.presentation.count != 0U) {
		return false;
	}

	auto max_bonus = fixture();
	max_bonus.player_power.power = 80U;
	max_bonus.player_power.power_overflow = 42;
	max_bonus.lifecycle.collect_skill = 31U;
	add_item(max_bonus, 0U, ItemType::power, 200, 100);
	if(step(max_bonus) != ItemLifecycleResult::completed ||
		max_bonus.player_power.power_overflow != 42 ||
		max_bonus.lifecycle.item_score_this_frame != 1280 ||
		max_bonus.lifecycle.collect_skill != 1U || max_bonus.lifecycle.item_skill != 1 ||
		max_bonus.score.score_delta != 1280 ||
		!event_at(max_bonus.presentation, 0U, {EventKind::pointnum, 188, 100, 1280})) {
		return false;
	}

	auto patnum = fixture();
	add_item(patnum, 0U, static_cast<ItemType>(5U), 300, 200);
	if(step(patnum) != ItemLifecycleResult::unsupported_item_patnum ||
		patnum.items.items[0].flag != item_alive || patnum.items.items[0].age != 1 ||
		patnum.items.items[0].velocity_y != 1 || patnum.presentation.count != 0U) {
		return false;
	}

	// item_hittest_y() has no default case. An unknown byte collected inside
	// its geometry therefore takes the common remove/sound/tail path; only an
	// uncollected unknown value reaches the later ITEM_PATNUM table read above.
	auto collected_unknown = fixture();
	add_item(collected_unknown, 0U, static_cast<ItemType>(5U), 200, 100);
	if(step(collected_unknown) != ItemLifecycleResult::completed ||
		collected_unknown.items.items[0].flag != item_remove ||
		collected_unknown.presentation.count != 2U ||
		!event_at(collected_unknown.presentation, 0U, {EventKind::sound, 13, 0, 0}) ||
		!event_at(collected_unknown.presentation, 1U,
			{EventKind::pointnums_update, 0, 0, 0})) {
		return false;
	}

	auto empty = fixture();
	empty.lifecycle.frame_unused = std::numeric_limits<std::int16_t>::max();
	empty.items.items[0].flag = 0x7FU;
	empty.items.items[0].age = 123;
	empty.items.items[0].velocity_y = -17;
	const auto raw_nonalive = empty.items.items[0];
	if(step(empty) != ItemLifecycleResult::completed ||
		empty.lifecycle.frame_unused != std::numeric_limits<std::int16_t>::min() ||
		empty.items.items[0] != raw_nonalive ||
		empty.presentation.count != 1U ||
		!event_at(empty.presentation, 0U, {EventKind::pointnums_update, 0, 0, 0})) {
		return false;
	}
	return true;
}

[[nodiscard]] bool collection_thresholds_and_one_pixel_misses() noexcept
{
	for(const auto [power, expected_power, expected_overflow, expected_score] :
		std::array<std::array<int, 4U>, 2U>{{
			{{69, 79, 0, 1}}, {{70, 80, 5, 5}},
		}}) {
		auto state = fixture();
		state.player_power.power = static_cast<std::uint8_t>(power);
		add_item(state, 0U, ItemType::big_power, 200, 100);
		if(step(state) != ItemLifecycleResult::completed ||
			state.player_power.power != expected_power ||
			state.player_power.power_overflow != expected_overflow ||
			state.score.score_delta != expected_score) {
			return false;
		}
	}

	for(const auto overflow : {37, 38}) {
		auto state = fixture();
		state.player_power.power = 80U;
		state.player_power.power_overflow = static_cast<std::int16_t>(overflow);
		add_item(state, 0U, ItemType::big_power, 200, 100);
		if(step(state) != ItemLifecycleResult::completed ||
			state.player_power.power_overflow != 42 ||
			state.score.score_delta != 1280 ||
			!event_at(state.presentation, 0U,
				{EventKind::pointnum, 188, 100, 1280})) {
			return false;
		}
	}

	auto bomb_max = fixture();
	bomb_max.score.bombs = 5;
	add_item(bomb_max, 0U, ItemType::bomb, 200, 100);
	if(step(bomb_max) != ItemLifecycleResult::completed ||
		bomb_max.score.bombs != 5 || bomb_max.score.score_delta != 6553 ||
		bomb_max.lifecycle.collect_skill != 16U || bomb_max.presentation.count != 3U ||
		!event_at(bomb_max.presentation, 0U, {EventKind::pointnum, 188, 100, 6553}) ||
		!event_at(bomb_max.presentation, 1U, {EventKind::sound, 13, 0, 0})) {
		return false;
	}

	auto lives_max = fixture();
	lives_max.score.lives = 5;
	add_item(lives_max, 0U, ItemType::one_up, 200, 100);
	if(step(lives_max) != ItemLifecycleResult::completed || lives_max.score.lives != 5 ||
		lives_max.presentation.count != 2U ||
		!event_at(lives_max.presentation, 0U, {EventKind::sound, 13, 0, 0}) ||
		!event_at(lives_max.presentation, 1U,
			{EventKind::pointnums_update, 0, 0, 0})) {
		return false;
	}

	auto outside = fixture();
	add_item(outside, 0U, ItemType::power, 191, 100);
	add_item(outside, 1U, ItemType::power, 225, 100);
	add_item(outside, 2U, ItemType::power, 200, 99);
	add_item(outside, 3U, ItemType::power, 200, 125);
	if(step(outside) != ItemLifecycleResult::completed || outside.player_power.power != 79U ||
		outside.presentation.count != 5U ||
		!event_at(outside.presentation, 0U, {EventKind::item, 191, 100, 68}) ||
		!event_at(outside.presentation, 1U, {EventKind::item, 225, 100, 68}) ||
		!event_at(outside.presentation, 2U, {EventKind::item, 200, 99, 68}) ||
		!event_at(outside.presentation, 3U, {EventKind::item, 200, 125, 68}) ||
		!event_at(outside.presentation, 4U,
			{EventKind::pointnums_update, 0, 0, 0})) {
		return false;
	}
	for(std::size_t i = 0U; i < 4U; ++i) {
		if(outside.items.items[i].flag != item_alive) return false;
	}
	return true;
}

} // namespace

int main()
{
	if(!all_item_types_preserve_ordered_state_and_effects()) {
		std::fputs("FAIL all item types/effect order\n", stderr);
		return EXIT_FAILURE;
	}
	if(!inclusive_edges_and_raw_hit_gate()) {
		std::fputs("FAIL inclusive edges/raw-hit gate\n", stderr);
		return EXIT_FAILURE;
	}
	if(!movement_removal_and_wrap_boundaries()) {
		std::fputs("FAIL movement/removal/wrap boundaries\n", stderr);
		return EXIT_FAILURE;
	}
	if(!rank_scaling_and_dword_wrap_are_source_sized()) {
		std::fputs("FAIL rank scaling/dword wrap\n", stderr);
		return EXIT_FAILURE;
	}
	if(!refusals_preserve_the_source_prefix()) {
		std::fputs("FAIL lifecycle refusal prefix\n", stderr);
		return EXIT_FAILURE;
	}
	if(!collection_thresholds_and_one_pixel_misses()) {
		std::fputs("FAIL lifecycle collection thresholds/borders\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
