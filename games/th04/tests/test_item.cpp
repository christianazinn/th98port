#include "th04/core/state.hpp"

#include "pc98/random.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
using namespace th04::core;

struct Pointnum {
	Point point{};
	std::uint16_t score{};
	bool yellow{};
	bool operator==(const Pointnum &) const = default;
};

struct Trace final : Presentation {
	std::vector<std::uint8_t> events{};
	std::vector<Pointnum> pointnums{};
	std::vector<Point> splashes{};

	void sound(std::uint8_t id) noexcept override
	{
		events.push_back(static_cast<std::uint8_t>(0x40U + id));
	}
	void hud(HudField field) noexcept override
	{
		events.push_back(static_cast<std::uint8_t>(0x10U + static_cast<unsigned>(field)));
	}
	void popup(std::uint8_t id) noexcept override
	{
		events.push_back(static_cast<std::uint8_t>(0x20U + id));
	}
	void pointnum(Point point, std::uint16_t score, bool yellow) noexcept override
	{
		events.push_back(0x30U);
		pointnums.push_back({point, score, yellow});
	}
	void item_splash(Point point) noexcept override { splashes.push_back(point); }
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.player_pos.cur = {3072, 1600};
	state.resources = {8, 32, 2, 0};
	state.power = 64;
	state.random.seed = 0x11223344U;
	return state;
}

void set_live_item(State &state, ItemType type, Point point, Point velocity = {}) noexcept
{
	auto &item = state.items.items[0];
	item.flag = item_alive;
	item.pos.cur = point;
	item.pos.velocity = velocity;
	item.type = static_cast<std::uint8_t>(type);
	item.patnum = static_cast<std::int16_t>(44 + static_cast<std::int8_t>(type));
	item.pulled_to_player = 0;
}

[[nodiscard]] bool init_and_enemy_drops()
{
	auto state = source_state();
	Trace presentation{};
	state.items.items[0].flag = item_alive;
	state.items.items_spawned = 13;
	state.items.items_pull_to_player = true;
	state.items.dream_score = 800;
	const auto seed_before = state.random.seed;
	const auto expected_ring = static_cast<std::uint8_t>(pc98::irand(state.random.seed) & 15U);
	state.random.seed = seed_before;
	items_init(state, presentation);
	if(state.items.enemy_drop_ring_p != expected_ring || state.random.seed == seed_before ||
		state.items.items[0].flag != item_alive || state.items.items_spawned != 13 ||
		state.items.items_pull_to_player || state.items.dream_score != 0 || !presentation.events.empty()) {
		return false;
	}

	state.items = {};
	items_add(state, presentation, {100, 200}, ItemType::enemy_drop_next);
	if(state.items.enemy_drop_ring_p != 1 || state.items.items_spawned != 0 ||
		!presentation.splashes.empty()) return false;
	items_add(state, presentation, {100, 200}, ItemType::enemy_drop_next);
	const auto &item = state.items.items[0];
	return state.items.enemy_drop_ring_p == 2 && state.items.items_spawned == 1 &&
		item.flag == item_alive && item.pos.cur == Point{100, 200} &&
		item.pos.velocity == Point{0, -48} && item.type == static_cast<std::uint8_t>(ItemType::point) &&
		item.patnum == 45 && item.pulled_to_player == 0 &&
		presentation.splashes == std::vector<Point>{{100, 200}};
}

[[nodiscard]] bool miss_set_and_rng()
{
	auto state = source_state();
	Trace presentation{};
	state.player_pos.cur = {1000, 3000};
	state.random.ring[0] = 2;
	state.random.ring[1] = 0;
	state.random.ring[2] = 1;
	state.random.ring[3] = 0;
	state.random.ring[4] = 0;
	state.random.ring[5] = 0;
	state.random.ring[6] = 0;
	if(!items_miss_add(state, presentation) || state.random.cursor != 6 ||
		state.items.items_spawned != 5 || !presentation.events.empty()) return false;
	constexpr std::uint8_t types[]{1, 0, 3, 0, 0};
	constexpr std::int16_t ys[]{-48, -56, -64, -72, -80};
	constexpr std::int16_t xs[]{0, 12, 24, 36, 48};
	for(std::size_t i = 0; i < 5; ++i) {
		const auto &item = state.items.items[i];
		if(item.flag != item_alive || item.pos.cur != Point{1000, 3000} ||
			item.pos.velocity != Point{xs[i], ys[i]} || item.type != types[i] ||
			item.patnum != static_cast<std::int16_t>(44 + types[i]) ||
			item.pulled_to_player != 0) return false;
	}
	return true;
}

[[nodiscard]] bool collection_effects()
{
	auto state = source_state();
	Trace presentation{};
	state.items.items_pull_to_player = true;
	state.player_pos.cur = {3072, 800};
	set_live_item(state, ItemType::point, {2912, 800});
	if(!items_update(state, presentation) || state.items.items[0].flag != item_remove ||
		state.score.score_delta != 10240 || state.items.items_collected != 1 ||
		state.items.total_point_items_collected != 1 ||
		state.items.total_max_valued_point_items_collected != 1 ||
		state.items.item_playperf_raise != 8 || state.resources.playperf != 8 ||
		presentation.pointnums != std::vector<Pointnum>{{{3072, 800}, 5120, true}} ||
		presentation.events != std::vector<std::uint8_t>{
			static_cast<std::uint8_t>(0x10U + static_cast<unsigned>(HudField::point_items)),
			0x30U, 0x4BU,
		}) return false;
	if(!items_update(state, presentation) || state.items.items[0].flag != item_free) return false;

	state = source_state();
	presentation = {};
	state.power = 127;
	set_live_item(state, ItemType::power, state.player_pos.cur);
	if(!items_update(state, presentation) || state.power != 128 || state.shot_level != 9 ||
		state.resources.bullet_clear_time != 20 || state.score.score_delta != 1 ||
		presentation.pointnums != std::vector<Pointnum>{{{3072, 1600}, 1, false}} ||
		presentation.events != std::vector<std::uint8_t>{0x23U,
			static_cast<std::uint8_t>(0x10U + static_cast<unsigned>(HudField::power)), 0x30U, 0x4BU}) {
		return false;
	}

	state = source_state();
	presentation = {};
	state.power = 128;
	state.power_overflow = 40;
	set_live_item(state, ItemType::big_power, state.player_pos.cur);
	if(!items_update(state, presentation) || state.power_overflow != 42 ||
		state.score.score_delta != 2560 || presentation.pointnums !=
			std::vector<Pointnum>{{{3072, 1600}, 2560, true}}) return false;

	state = source_state();
	presentation = {};
	set_live_item(state, ItemType::one_up, state.player_pos.cur);
	if(!items_update(state, presentation) || state.resources.playperf != 11 ||
		state.resources.rem_lives != 3 || state.score.score_delta != 100 ||
		presentation.events != std::vector<std::uint8_t>{
			static_cast<std::uint8_t>(0x10U + static_cast<unsigned>(HudField::lives)),
			0x47U, 0x21U, 0x30U, 0x4BU,
		}) return false;
	return true;
}

[[nodiscard]] bool removal_and_miss_gate()
{
	auto state = source_state();
	Trace presentation{};
	state.resources.playperf = 20;
	state.items.item_playperf_lower = 63;
	set_live_item(state, ItemType::point, {2000, 6015}, {0, 1});
	if(!items_update(state, presentation) || state.items.items[0].flag != item_remove ||
		state.items.item_playperf_lower != 17 || state.resources.playperf != 19) return false;
	if(!items_update(state, presentation) || state.items.items[0].flag != item_free) return false;

	state = source_state();
	presentation = {};
	state.miss_time = 1;
	set_live_item(state, ItemType::dream, state.player_pos.cur);
	if(!items_update(state, presentation) || state.items.items[0].flag != item_alive ||
		state.items.items_collected != 0 || state.items.items[0].pos.velocity.y != 1 ||
		!presentation.events.empty()) return false;
	return true;
}

} // namespace

int main()
{
	const bool checks[]{init_and_enemy_drops(), miss_set_and_rng(), collection_effects(),
		removal_and_miss_gate()};
	for(std::size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); ++i) {
		if(!checks[i]) {
			std::cerr << "TH04 item check failed: " << i << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
