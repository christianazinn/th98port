#include "th05/core/bomb.hpp"
#include "th05/core/state.hpp"

#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_sparks.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using namespace th05::core;

enum class EventKind : std::uint8_t {
	sound,
	hud,
	bomb_cel,
	bomb_background,
	graph_scroll,
	palette_show,
	geometry,
};

struct Event {
	EventKind kind{};
	std::int32_t value{};
	bool operator==(const Event &) const = default;
};

struct Trace final : Presentation {
	std::vector<Event> events{};
	std::vector<BombGeometry> geometry{};

	void sound(std::uint8_t id) noexcept override
	{
		events.push_back({EventKind::sound, id});
	}

	void hud(HudField field) noexcept override
	{
		events.push_back({EventKind::hud, static_cast<std::int32_t>(field)});
	}

	void bomb_cel(std::uint8_t cel) noexcept override
	{
		events.push_back({EventKind::bomb_cel, cel});
	}

	void bomb_background_put(std::uint8_t playchar) noexcept override
	{
		events.push_back({EventKind::bomb_background, playchar});
	}

	void graph_scroll(std::int16_t line) noexcept override
	{
		events.push_back({EventKind::graph_scroll, line});
	}

	void palette_show(std::uint16_t tone) noexcept override
	{
		events.push_back({EventKind::palette_show, tone});
	}

	void bomb_geometry(const BombGeometry &entry) noexcept override
	{
		geometry.push_back(entry);
		events.push_back({
			EventKind::geometry, static_cast<std::int32_t>(entry.kind)
		});
	}
};

[[nodiscard]] State source_state(std::uint8_t playchar) noexcept
{
	State state{};
	state.playchar = playchar;
	state.bombs = 2U;
	state.resources.playperf = 20U;
	state.playperf_min = 16U;
	state.resources.playperf_max = 32U;
	state.player_pos.cur = {3000, 5000};
	state.player_pos.prev = {2990, 4990};
	state.items.dream = 16U;
	return state;
}

[[nodiscard]] std::size_t geometry_count(
	const Trace &trace, BombGeometryKind kind
) noexcept
{
	std::size_t count{};
	for(const auto &entry : trace.geometry) {
		if(entry.kind == kind) ++count;
	}
	return count;
}

[[nodiscard]] bool bomb_entry_and_deathbomb()
{
	constexpr std::array invincibility{192U, 160U, 144U, 224U};
	constexpr std::array clear_time{192U, 128U, 96U, 192U};
	for(std::uint8_t playchar = 0U; playchar < 4U; ++playchar) {
		auto state = source_state(playchar);
		Trace trace{};
		player_bomb(state, trace);
		if(
			state.bombs != 1U || !state.bombing || state.bomb_frame != 0U ||
			state.player_invincibility_time != invincibility[playchar] ||
			state.resources.bullet_clear_time != clear_time[playchar] ||
			!state.bomb_background || !state.items.items_pull_to_player ||
			state.bombs_used != 1U || state.resources.playperf != 19U ||
			trace.events != std::vector<Event>{
				{EventKind::hud, static_cast<std::int32_t>(HudField::bombs)},
				{EventKind::sound, 13},
			}
		) return false;
	}

	{
		auto state = source_state(0U);
		state.bombs = 0U;
		Trace trace{};
		player_bomb(state, trace);
		if(state.bombing || !trace.events.empty()) return false;
	}
	{
		auto state = source_state(0U);
		state.bombing_disabled = true;
		Trace trace{};
		player_bomb(state, trace);
		if(state.bombing || state.bombs != 2U || !trace.events.empty()) return false;
	}
	{
		auto state = source_state(0U);
		state.miss_time = 32U;
		Trace trace{};
		player_bomb(state, trace);
		if(state.bombing || state.bombs != 2U || state.miss_time != 32U ||
			!trace.events.empty()) return false;
	}
	{
		auto state = source_state(2U);
		state.bombs = 1U;
		state.miss_time = 40U;
		state.player_is_hit = true;
		state.miss_move_lock_time = 72U;
		Trace trace{};
		player_bomb(state, trace);
		if(
			!state.bombing || state.bombs != 0U || state.miss_time != 0U ||
			state.player_is_hit || state.miss_move_lock_time != 0U ||
			state.player_invincibility_time != 144U ||
			state.resources.bullet_clear_time != 96U
		) return false;
	}
	{
		auto state = source_state(4U);
		Trace trace{};
		player_bomb(state, trace);
		if(state.bombing || state.bombs != 2U || !trace.events.empty()) return false;
	}
	return true;
}

[[nodiscard]] bool four_bomb_lifetimes()
{
	constexpr std::array update_counts{161U, 97U, 81U, 193U};
	for(std::uint8_t playchar = 0U; playchar < 4U; ++playchar) {
		auto state = source_state(playchar);
		state.bombs = 1U;
		state.items.dream = 128U;
		Trace trace{};
		player_bomb(state, trace);
		std::uint16_t updates{};
		while(state.bombing) {
			if(!bomb_update(state, trace)) return false;
			++updates;
			if(updates > 200U) return false;
		}
		if(
			updates != update_counts[playchar] ||
			state.bomb_frame != static_cast<std::uint8_t>(updates) ||
			state.items.items_pull_to_player || !state.bomb_background ||
			!state.scroll_active || state.palette_tone != 100U
		) return false;
	}
	return true;
}

[[nodiscard]] bool reimu_rng_and_driver_order()
{
	auto state = source_state(0U);
	state.bombing = true;
	state.bomb_frame = 32U;
	state.stage_frame = 0U;
	state.items.dream = 4U;
	state.random.ring.fill(0U);
	// The source helper takes the 16-bit ring word modulo 48, before the
	// result narrows into the vector angle.
	state.random.ring[3] = 0x10U;
	state.random.ring[4] = 0x01U;
	BombReimuStar untouched{};
	untouched.topleft = {3011, 3022};
	bomb_reimu_star_store(state.bomb.animation, 28U, untouched);
	Trace trace{};
	if(!bomb_update(state, trace)) return false;

	const auto velocity = pc98::masterlib::vector2(72U, 160);
	const auto head = bomb_reimu_star_load(state.bomb.animation, 0U);
	const auto overlap = bomb_reimu_star_load(state.bomb.animation, 7U);
	const auto retained = bomb_reimu_star_load(state.bomb.animation, 29U);
	if(
		state.random.cursor != 24U || state.bomb_frame != 33U ||
		state.items.dream != 3U || state.palette_tone != 196U ||
		head.topleft != Point{
			static_cast<std::int16_t>(512 + velocity.x), velocity.y
		} ||
		head.velocity != Point{
			static_cast<std::int16_t>(-velocity.x), velocity.y
		} ||
		overlap.topleft != Point{513, 0} ||
		retained.topleft != Point{3011, 3022} ||
		geometry_count(trace, BombGeometryKind::reimu_star) != 24U ||
		geometry_count(trace, BombGeometryKind::growing_circle) != 1U ||
		trace.events.size() < 3U ||
		trace.events[0] != Event{EventKind::graph_scroll, 0} ||
		trace.events[1] != Event{EventKind::bomb_background, 0} ||
		trace.events.back() != Event{
			EventKind::hud, static_cast<std::int32_t>(HudField::dream)
		}
	) return false;

	state = source_state(0U);
	state.bombing = true;
	state.bomb_frame = 32U;
	state.random.cursor = 256U;
	trace = {};
	if(bomb_update(state, trace) || state.bomb_frame != 32U ||
		state.random.cursor != 256U || trace.events.size() != 2U ||
		trace.events[0] != Event{EventKind::graph_scroll, 0} ||
		trace.events[1] != Event{EventKind::bomb_background, 0}) return false;
	return true;
}

[[nodiscard]] bool marisa_sparks_and_shot_blocking()
{
	auto state = source_state(1U);
	state.bombing = true;
	state.bomb_frame = 15U;
	state.items.dream = 5U;
	Trace trace{};
	if(!bomb_update(state, trace) || state.bomb_frame != 16U ||
		state.shot_spawn.shot_time != 0U || state.palette_tone != 106U ||
		state.items.dream != 5U) return false;

	trace = {};
	if(!bomb_update(state, trace) || state.bomb_frame != 17U ||
		state.shot_spawn.shot_time != 0xFFU || state.items.dream != 4U ||
		state.spark_ring_offset != 384U || trace.events != std::vector<Event>{
			{EventKind::bomb_background, 1},
			{EventKind::sound, 15},
			{EventKind::palette_show, 170},
			{EventKind::geometry, static_cast<std::int32_t>(BombGeometryKind::marisa_laser)},
			{EventKind::hud, static_cast<std::int32_t>(HudField::dream)},
		}) return false;
	for(std::size_t index = 0; index < 24U; ++index) {
		if(state.sparks[index].flag != pc98::th0405::SPARK_ALIVE ||
			state.sparks[index].center.cur != state.player_pos.cur) return false;
	}
	const auto laser = bomb_animation_record_load<BombMarisaLaser>(state.bomb.animation, 0U);
	if(laser.center != state.player_pos.cur || laser.radius != 23) return false;

	state = source_state(1U);
	state.bombing = true;
	state.bomb_frame = 16U;
	state.spark_ring_offset = 1U;
	trace = {};
	if(bomb_update(state, trace) || state.bomb_frame != 16U ||
		state.shot_spawn.shot_time != 0U || trace.events != std::vector<Event>{
			{EventKind::bomb_background, 1},
		}) return false;
	const auto reached = bomb_animation_record_load<BombMarisaLaser>(state.bomb.animation, 0U);
	return reached.center == state.player_pos.cur && reached.radius == 0;
}

[[nodiscard]] bool mima_cold_storage_and_random()
{
	auto state = source_state(2U);
	state.bombing = true;
	state.bomb_frame = 0U;
	state.items.dream = 5U;
	state.random.ring[0] = 0x34U;
	state.random.ring[1] = 0x12U;
	Trace trace{};
	if(!bomb_update(state, trace) || state.bomb_frame != 1U ||
		state.random.cursor != 1U || state.items.dream != 4U) return false;
	const auto spawned = bomb_animation_record_load<BombMimaCircle>(state.bomb.animation, 4U);
	const auto offset = pc98::masterlib::vector2(0x34U, 256);
	if(spawned.angle != 0x34U || spawned.distance != 240 ||
		spawned.center != BombPixelPoint{
			static_cast<std::int16_t>(224 + offset.x),
			static_cast<std::int16_t>(117 + offset.y),
		}) return false;
	if(!bomb_update(state, trace) || state.bomb_frame != 2U ||
		bomb_animation_record_load<BombMimaCircle>(state.bomb.animation, 4U).center.x != -999) return false;

	state = source_state(2U);
	state.bombing = true;
	state.bomb_frame = 0U;
	state.random.cursor = 256U;
	trace = {};
	if(bomb_update(state, trace) || state.bomb_frame != 0U ||
		state.random.cursor != 256U || state.items.dream != 16U) return false;
	const auto reached = bomb_animation_record_load<BombMimaCircle>(state.bomb.animation, 4U);
	return reached.center == BombPixelPoint{state.player_pos.cur.x, state.player_pos.cur.y} &&
		reached.distance == 256;
}

[[nodiscard]] bool dream_raw_overlay_and_stage_six()
{
	{
		auto state = source_state(3U);
		const BombMimaCircle cold{{77, 88}, 99, 123, 0x20U, 0};
		bomb_animation_record_store(state.bomb.animation, 0U, cold);
		state.bombs = 1U;
		Trace trace{};
		player_bomb(state, trace);
		const auto after_entry = bomb_animation_record_load<BombYuukaHeart>(state.bomb.animation, 0U);
		if(after_entry.topleft != BombPixelPoint{77, 88} || after_entry.unused != 99 ||
			after_entry.distance != 123 || after_entry.angle != 0x20U) return false;

		state.bomb_frame = 32U;
		trace = {};
		if(!bomb_update(state, trace) || state.bomb_frame != 33U ||
			bomb_animation_record_load<BombYuukaHeart>(state.bomb.animation, 0U).distance != 18 ||
			bomb_animation_record_load<BombYuukaHeart>(state.bomb.animation, 0U).angle != 0x22U ||
			geometry_count(trace, BombGeometryKind::yuuka_heart) != 16U) return false;
	}
	{
		auto state = source_state(0U);
		state.bombing = true;
		state.bomb_frame = 34U;
		state.boss_phase = 1U;
		state.items.dream = 4U;
		Trace trace{};
		if(!bomb_update(state, trace) || state.items.dream != 3U) return false;
		state.bomb_frame = 36U;
		trace = {};
		if(!bomb_update(state, trace) || state.items.dream != 3U ||
			geometry_count(trace, BombGeometryKind::reimu_star) != 24U) return false;
		state.boss_phase = 0U;
		state.bomb_frame = 38U;
		trace = {};
		if(!bomb_update(state, trace) || state.items.dream != 2U) return false;
	}
	{
		auto state = source_state(3U);
		state.bombing = true;
		state.bomb_frame = 160U;
		state.stage_id = 5U;
		state.items.items_pull_to_player = true;
		Trace trace{};
		if(!bomb_update(state, trace) || state.bomb_frame != 161U || state.scroll_active ||
			state.items.items_pull_to_player || !state.bomb_background ||
			trace.events != std::vector<Event>{{EventKind::sound, 15}}) return false;
		trace = {};
		if(!bomb_update(state, trace) || state.bomb_frame != 162U || state.scroll_active ||
			!trace.events.empty()) return false;
	}
	{
		auto state = source_state(4U);
		state.bombing = true;
		Trace trace{};
		if(bomb_update(state, trace) || state.bomb_frame != 0U || !trace.events.empty()) return false;
	}
	return true;
}

} // namespace

int main()
{
	if(!bomb_entry_and_deathbomb()) {
		std::cerr << "TH05 bomb entry/deathbomb test failure\n";
		return EXIT_FAILURE;
	}
	if(!four_bomb_lifetimes()) {
		std::cerr << "TH05 bomb lifetime test failure\n";
		return EXIT_FAILURE;
	}
	if(!reimu_rng_and_driver_order()) {
		std::cerr << "TH05 Reimu bomb test failure\n";
		return EXIT_FAILURE;
	}
	if(!marisa_sparks_and_shot_blocking()) {
		std::cerr << "TH05 Marisa bomb test failure\n";
		return EXIT_FAILURE;
	}
	if(!mima_cold_storage_and_random()) {
		std::cerr << "TH05 Mima bomb test failure\n";
		return EXIT_FAILURE;
	}
	if(!dream_raw_overlay_and_stage_six()) {
		std::cerr << "TH05 Yuuka/Dream bomb test failure\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
