#include "th04/core/shot_collision.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <utility>

namespace {
using namespace th04::core;

bool test_cache_and_damage()
{
	ShotState state{};
	state.shots_alive_count = 3;
	state.shots_alive[0] = {{0, 0}, 2};
	state.shots_alive[1] = {{0, 0}, 1};
	state.shots_alive[2] = {{0, 0}, 0};
	for(std::size_t i = 0; i < 3; ++i) {
		state.shots[i] = {shot_alive, 77, {{static_cast<std::int16_t>(i + 100), 200},
			{333, 444}, {-13, 17}}, 28, 10, -42};
	}
	ShotCollisionState c{};
	c.score_delta = 0xFFFFFFFFU;
	const auto cache = state.shots_alive;
	const auto result = shots_hittest(state, c);
	if(!result || result->damage != 18 || result->spark_count != 2 || c.score_delta != 17 ||
		c.spark_parity != 3 || state.shots_alive != cache || state.shots_alive_count != 3 ||
		result->sparks[0] != pc98::th0405::SubpixelPoint{102, 200} ||
		result->sparks[1] != pc98::th0405::SubpixelPoint{100, 200}) return false;
	for(const auto &shot : std::span(state.shots).first(3)) {
		if(shot.flag != 2 || shot.pos.velocity != pc98::th0405::SubpixelPoint{-2, 2} ||
			shot.pos.prev != pc98::th0405::SubpixelPoint{333, 444} ||
			shot.age != 77 || shot.patnum_base != 40 || shot.damage != 10 || shot.angle != -42) return false;
	}
	// Existing hit flags are not skipped; velocity decays again, cache unchanged.
	const auto second = shots_hittest(state, c);
	return second && second->damage == 18 && second->spark_count == 1 &&
		state.shots[0].pos.velocity == pc98::th0405::SubpixelPoint{0, 0} && c.spark_parity == 6;
}

bool test_all_damage_and_parity()
{
	for(int damage = -128; damage <= 127; ++damage) {
		for(unsigned parity = 0; parity < 256; ++parity) {
			ShotState state{};
			state.shots_alive_count = 68;
			for(std::size_t i = 0; i < 68; ++i) {
				state.shots[i].damage = static_cast<std::int8_t>(damage);
				state.shots_alive[i].shot_index = static_cast<std::uint16_t>(i);
			}
			ShotCollisionState c{};
			c.spark_parity = static_cast<std::uint8_t>(parity);
			unsigned expected = 0;
			for(unsigned hit = 1; hit <= 68; ++hit) expected += static_cast<unsigned>(damage < 0 ? damage + 256 : damage) / hit;
			const auto result = shots_hittest(state, c);
			if(!result || result->damage != expected || result->spark_count != 34 ||
				c.score_delta != expected || c.spark_parity != (parity + 68) % 256) return false;
		}
	}
	return true;
}

bool test_wrapped_geometry()
{
	for(bool y_axis : {false, true}) {
	for(int center : {-32768, -50, 0, 32767}) {
		for(int radius : {-32768, -1, 0, 128, 32767}) {
			for(int coordinate = -32768; coordinate < 32768; ++coordinate) {
				ShotState state{};
				state.shots_alive_count = 1;
				state.shots[0].damage = 9;
				ShotCollisionState c{};
				if(y_axis) {
					state.shots_alive[0].pos.y = static_cast<std::int16_t>(coordinate);
					c.center.y = static_cast<std::int16_t>(center);
					c.radius.y = static_cast<std::int16_t>(radius);
				} else {
					state.shots_alive[0].pos.x = static_cast<std::int16_t>(coordinate);
					c.center.x = static_cast<std::int16_t>(center);
					c.radius.x = static_cast<std::int16_t>(radius);
				}
				const int left = (center - radius + 65536) % 65536;
				const int delta = (coordinate - left + 131072) % 65536;
				const bool hit = delta <= (radius * 2 + 65536) % 65536;
				const auto result = shots_hittest(state, c);
				if(!result || result->damage != (hit ? 9 : 0) || state.shots[0].flag != (hit ? 2 : 0)) return false;
			}
		}
	}
	}
	return true;
}

bool test_bomb_and_laser_order()
{
	for(bool bomb : {false, true}) for(bool boss : {false, true}) {
		for(unsigned mod4 = 0; mod4 < 4; ++mod4) for(unsigned mod2 = 0; mod2 < 2; ++mod2) {
			for(unsigned time : {0U, 1U, 31U, 32U, 33U, 65535U}) {
				ShotState state{};
				state.shots_alive_count = 1;
				state.shots[0].damage = 7;
				state.shot_laser_time = static_cast<std::uint16_t>(time);
				ShotCollisionState c{{0, 1}, {384, 1}, bomb, boss,
					static_cast<std::uint8_t>(mod2), static_cast<std::uint8_t>(mod4), 1, 0};
				unsigned expected = 7;
				if(bomb) {
					if(mod4 == 0) expected += 5;
					if(boss) expected /= 4;
				}
				const bool lasers = mod2 != 0 && time > 32;
				if(lasers) expected += 6;
				const auto result = shots_hittest(state, c);
				if(!result || result->damage != expected || c.score_delta != expected ||
					c.spark_parity != (lasers ? 4 : 2) || result->spark_count != (lasers ? 1 : 0)) return false;
				if(lasers && result->sparks[0] != pc98::th0405::SubpixelPoint{384, 1}) return false;
			}
		}
	}
	// Negative top is compared unsigned, not geometrically clamped to zero.
	ShotState state{};
	state.shot_laser_time = 33;
	ShotCollisionState c{{0, 0}, {384, 1}, false, false, 1, 0, 3, 0};
	const auto result = shots_hittest(state, c);
	if(!result || result->damage != 0 || c.spark_parity != 3) return false;
	// A wrapped negative laser Y can satisfy the original unsigned comparison.
	state.shot_laser_bottomcenter.cur.y = -1;
	const auto wrapped = shots_hittest(state, c);
	if(!wrapped || wrapped->damage != 6 || wrapped->spark_count != 1 ||
		wrapped->sparks[0] != pc98::th0405::SubpixelPoint{-384, 0}) return false;
	state.shot_laser_bottomcenter.cur = {32760, 0};
	c = {{-32392, 0}, {}, false, false, 1, 0, 3, 0};
	const auto wrap_x = shots_hittest(state, c);
	return wrap_x && wrap_x->damage == 3 && wrap_x->spark_count == 1 &&
		wrap_x->sparks[0] == pc98::th0405::SubpixelPoint{-32392, 0};
}

bool test_refusal_and_duplicate_pointer()
{
	ShotState state{};
	ShotCollisionState c{};
	state.shots_alive_count = 69;
	auto before = state;
	auto c_before = c;
	if(shots_hittest(state, c) || state != before || c != c_before) return false;
	state.shots_alive_count = 2;
	state.shots[0].damage = 12;
	state.shots[0].pos.velocity = {-216, 216};
	state.shots_alive[1].shot_index = 68;
	before = state;
	if(shots_hittest(state, c) || state != before || c != c_before) return false;
	// An invalid pointer outside the hitbox is never dereferenced by the source.
	state.shots_alive[1].pos.x = 1;
	const auto one = shots_hittest(state, c);
	if(!one || one->damage != 12) return false;
	state.shots_alive[1] = {{0, 0}, 0};
	const auto repeated = shots_hittest(state, c);
	return repeated && repeated->damage == 18 &&
		state.shots[0].pos.velocity == pc98::th0405::SubpixelPoint{-1, 1};
}

bool test_spark_pool_consumer()
{
	using namespace pc98::th0405;
	ShotState state{};
	state.shots_alive_count = 5;
	for(std::size_t i = 0; i < 5; ++i) {
		state.shots_alive[i].shot_index = static_cast<std::uint16_t>(i);
		state.shots[i].damage = 12;
		state.shots[i].pos.cur = {100, 200};
	}
	// First request clips without advancing either ring. Second hits an
	// occupied slot; only the third allocates and consumes one random word.
	state.shots[0].pos.cur.x = -1;
	std::array<Spark, SPARK_COUNT_TH04> sparks{};
	sparks[95].flag = SPARK_ALIVE;
	sparks[0].angle = 64;
	sparks[0].center.prev = {7, 8};
	std::uint16_t ring = 95 * 16;
	RandomState random{};
	random.ring[0] = 7;
	ShotCollisionState c{};
	const auto result = shots_hittest(state, c, {sparks, ring}, random);
	if(!result || result->spark_count != 3 || result->damage != 27 ||
		c.score_delta != 27 || ring != 16 || random.cursor != 1 ||
		sparks[0].flag != SPARK_ALIVE || sparks[0].center.cur != SubpixelPoint{100, 200} ||
		sparks[0].center.velocity != SubpixelPoint{0, 135} ||
		sparks[0].center.prev != SubpixelPoint{7, 8} || sparks[0].angle != 64) return false;
	const auto before = state;
	const auto c_before = c;
	const auto sparks_before = sparks;
	for(unsigned invalid : {0U, 1U, 2U, 3U}) {
		ring = invalid == 0 ? 1 : 16;
		random.cursor = invalid == 1 ? 256 : 1;
		const auto random_before = random;
		const auto ring_before = ring;
		const auto size = invalid == 2 ? 64U : (invalid == 3 ? 95U : 96U);
		if(shots_hittest(state, c, {std::span(sparks).first(size), ring}, random) ||
			state != before || c != c_before || sparks != sparks_before ||
			random != random_before || ring != ring_before) return false;
	}
	return true;
}

bool test_synchronous_spark_order()
{
	ShotState state{};
	state.shots_alive_count = 3;
	for(std::size_t i = 0; i < 3; ++i) {
		state.shots_alive[i] = {{0, 0}, static_cast<std::uint16_t>(i)};
		state.shots[i].flag = shot_alive;
		state.shots[i].damage = 12;
		state.shots[i].pos.cur = {static_cast<std::int16_t>(100 + i), 200};
	}
	ShotCollisionState c{};
	c.score_delta = 90;
	unsigned calls = 0;
	bool ordered = true;
	const auto result = shots_hittest(state, c,
		[&](pc98::th0405::SubpixelPoint center) noexcept {
			const auto index = calls * 2;
			ordered = ordered && c.score_delta == 90 &&
				c.spark_parity == index + 1 && state.shots[index].flag == shot_hit &&
				center == state.shots[index].pos.cur;
			if(calls == 0) ordered = ordered && state.shots[1].flag == shot_alive;
			++calls;
		});
	if(!result || !ordered || calls != 2 || result->damage != 22 || c.score_delta != 112) return false;
	state.shots_alive_count = 69;
	const auto refused = shots_hittest(state, c,
		[&](pc98::th0405::SubpixelPoint) noexcept { ++calls; });
	return !refused && calls == 2 && c.score_delta == 112;
}
} // namespace

int main()
{
	for(const auto &[name, test] : std::array{
		std::pair{"cache/damage", test_cache_and_damage},
		std::pair{"damage/parity", test_all_damage_and_parity},
		std::pair{"geometry", test_wrapped_geometry},
		std::pair{"bomb/laser", test_bomb_and_laser_order},
		std::pair{"refusal/duplicates", test_refusal_and_duplicate_pointer},
		std::pair{"spark pool consumer", test_spark_pool_consumer},
		std::pair{"synchronous sparks", test_synchronous_spark_order}}) {
		if(!test()) {
			std::cerr << "FAIL " << name << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
