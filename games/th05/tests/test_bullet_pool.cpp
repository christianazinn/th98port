#include "th05/core/bullet_pool.hpp"
#include "th05/core/shot_collision.hpp"

#include <bit>
#include <cstdint>
#include <iostream>

namespace {
using namespace th05::core;
using Point = pc98::th0405::SubpixelPoint;

bool test_layout_and_speed_tune()
{
	static_assert(sizeof(BulletTemplate) == 14);
	static_assert(sizeof(Bullet) == 32);
	static_assert(sizeof(BulletPool) == bullet_count * sizeof(Bullet));
	for(std::uint16_t speed = 0; speed <= 255U; ++speed) {
		for(std::uint16_t playperf = 0; playperf <= 255U; ++playperf) {
			// Independent scalar model: (speed / 2) * (playperf + 32) / 32,
			// with the source's byte wrap before the inclusive clamp.
			const auto scaled =
				(static_cast<std::uint32_t>(speed / 2U) *
				 static_cast<std::uint32_t>(playperf + 32U)) / 32U;
			auto expected = static_cast<std::uint8_t>(scaled % 256U);
			if(expected > 128U) expected = 128U;
			if(expected < 8U) expected = 8U;
			if(bullet_speed_tune_for_playperf(
				static_cast<std::uint8_t>(speed),
				static_cast<std::uint8_t>(playperf)
			) != expected) return false;
		}
	}
	return true;
}

BulletTemplate alice_template()
{
	return {
		static_cast<std::uint8_t>(0x02U | bullet_no_decelerate),
		0,
		{1000, 1200},
		0,
		0xFF,
		0xAA,
		0xBB,
		0xCC,
		0xDD,
		64,
		24,
	};
}

BulletClipState neutral_clip()
{
	return {0, 16, {5000, 5000}, false, false, false};
}

bool test_reverse_single_allocation_and_residue()
{
	BulletPool pool{};
	pool.pellets[179].flag = bullet_alive;
	auto &slot = pool.pellets[178];
	slot.flag = bullet_free;
	slot.age = 0xE1;
	slot.pos.prev = {77, 88};
	slot.pos.velocity = {91, 92};
	slot.unused = 0xA1;
	slot.special_motion = 0xA2;
	slot.origin = {93, 94};
	slot.distance = 0x1234;
	const auto tmpl = alice_template();
	auto clip = neutral_clip();
	const auto result = bullets_add_regular_single(
		bullet_pool_view(pool), tmpl, clip
	);
	const auto expected_velocity = pc98::masterlib::vector2(64, 18);
	return result == BulletAddResult{
		BulletAddStatus::allocated, BulletPoolKind::pellets, 178
	} &&
		tmpl == alice_template() &&
		slot.flag == bullet_alive && slot.age == 0 &&
		slot.spawn_flag == bullet_spawn_cloud_forwards &&
		slot.pos.cur == tmpl.origin && slot.pos.prev == Point{77, 88} &&
		slot.pos.velocity == Point{expected_velocity.x, expected_velocity.y} &&
		slot.from_group == 0 && slot.move_flag == bullet_move_regular &&
		slot.u1.value == bullet_decelerate_frames &&
		slot.u2.value == 54 &&
		slot.patnum == 0 && slot.angle == 64 &&
		slot.speed_final == 18 && slot.speed_cur == 18 &&
		slot.unused == 0xA1 && slot.special_motion == 0xA2 &&
		slot.origin == Point{93, 94} && slot.distance == 0x1234;
}

bool test_clip_player_hit_and_temporary_tune()
{
	BulletPool pool{};
	const auto tmpl = BulletTemplate{
		0, 0, {128, 128}, 0, 0, 0, 0, 0, 0, 0, 24
	};
	auto clip = BulletClipState{
		17, 32, {128, 128}, false, false, false
	};
	const auto result = bullets_add_regular_single(
		bullet_pool_view(pool), tmpl, clip
	);
	// speed 24 at playperf 32 tunes to 24, and player overlap does not clip.
	return result.status == BulletAddStatus::allocated &&
		result.index == 179 && clip.player_is_hit &&
		tmpl.speed == 24 && pool.pellets[179].speed_cur == 24;
}

bool test_strict_playfield_and_killbox_edges()
{
	const Point strict_outside[] = {
		{-128, 0}, {6272, 0}, {0, -128}, {0, 6016}
	};
	const Point strict_inside[] = {
		{-127, 0}, {6271, 0}, {0, -127}, {0, 6015}
	};
	for(const auto origin : strict_outside) {
		BulletPool pool{};
		auto clip = neutral_clip();
		auto tmpl = alice_template();
		tmpl.origin = origin;
		if(bullets_add_regular_single(
			bullet_pool_view(pool), tmpl, clip
		).status != BulletAddStatus::clipped) return false;
	}
	for(const auto origin : strict_inside) {
		BulletPool pool{};
		auto clip = neutral_clip();
		auto tmpl = alice_template();
		tmpl.origin = origin;
		if(bullets_add_regular_single(
			bullet_pool_view(pool), tmpl, clip
		).status != BulletAddStatus::allocated) return false;
	}
	for(const bool y_axis : {false, true}) {
		for(const auto delta : {-65, -64, 64, 65}) {
			BulletPool pool{};
			auto clip = BulletClipState{
				17, 16, {1000, 1000}, false, true, false
			};
			auto tmpl = alice_template();
			tmpl.origin = y_axis ?
				Point{1000, static_cast<std::int16_t>(1000 + delta)} :
				Point{static_cast<std::int16_t>(1000 + delta), 1000};
			const auto result = bullets_add_regular_single(
				bullet_pool_view(pool), tmpl, clip
			);
			const auto expected_hit = delta >= -64 && delta <= 64;
			if(result.status != BulletAddStatus::allocated ||
				clip.player_is_hit != expected_hit) return false;
		}
	}
	return true;
}

bool test_speed_and_clear_time_flag_edges()
{
	struct Case {
		std::uint8_t speed;
		std::uint8_t clear_time;
		std::uint8_t spawn_type;
		std::uint8_t expected_move;
	};
	const Case cases[] = {
		{63, 0, 0, bullet_move_decelerate},
		{64, 0, 0, bullet_move_regular},
		{63, 17, 0, bullet_move_decelerate},
		{64, 17, 0, bullet_move_decelerate},
		{64, 17, bullet_no_decelerate, bullet_move_regular},
	};
	for(const auto test : cases) {
		BulletPool pool{};
		auto clip = BulletClipState{
			test.clear_time, 16, {5000, 5000}, false, true, false
		};
		auto tmpl = alice_template();
		tmpl.spawn_type = test.spawn_type;
		tmpl.speed = test.speed;
		const auto result = bullets_add_regular_single(
			bullet_pool_view(pool), tmpl, clip
		);
		if(result.status != BulletAddStatus::allocated ||
			pool.pellets[179].move_flag != test.expected_move) return false;
	}
	return true;
}

bool test_clear_zap_and_strict_playfield_clip()
{
	BulletPool pool{};
	const auto outside = BulletTemplate{
		0, 0, {-128, 0}, 0, 0, 0, 0, 0, 0, 0, 24
	};
	auto clip = neutral_clip();
	const auto left_edge = bullets_add_regular_single(
		bullet_pool_view(pool), outside, clip
	);
	if(left_edge.status != BulletAddStatus::clipped ||
		pool.pellets[179].flag != bullet_free) return false;
	const auto inside = BulletTemplate{
		0, 0, {-127, 0}, 0, 0, 0, 0, 0, 0, 0, 24
	};
	const auto inside_result = bullets_add_regular_single(
		bullet_pool_view(pool), inside, clip
	);
	if(inside_result.status != BulletAddStatus::allocated) return false;
	BulletClipState clear_clip{16, 16, {0, 0}, false, false, false};
	const auto clear_result = bullets_add_regular_single(
		bullet_pool_view(pool), inside, clear_clip
	);
	if(clear_result.status != BulletAddStatus::clipped ||
		clear_clip.player_is_hit) return false;
	BulletClipState zapped{16, 16, {0, 0}, false, false, true};
	const auto zapped_result = bullets_add_regular_single(
		bullet_pool_view(pool), outside, zapped
	);
	return zapped_result.status == BulletAddStatus::zapped &&
		pool.pellets[179].flag == bullet_alive;
}

bool test_full_pool_is_valid_no_slot_with_clip_side_effect()
{
	BulletPool pool{};
	for(auto &bullet : pool.pellets) bullet.flag = bullet_alive;
	const auto tmpl = BulletTemplate{
		static_cast<std::uint8_t>(0x02U | bullet_no_decelerate),
		0, {128, 128}, 0, 0, 0, 0, 0, 0, 0, 24
	};
	auto clip = BulletClipState{17, 16, {128, 128}, false, false, false};
	const auto result = bullets_add_regular_single(
		bullet_pool_view(pool), tmpl, clip
	);
	return result == BulletAddResult{
		BulletAddStatus::no_slot, BulletPoolKind::pellets, 0
	} && clip.player_is_hit;
}

bool test_first_slot_selection()
{
	BulletPool pool{};
	for(std::size_t index = 1; index < pool.pellets.size(); ++index) {
		pool.pellets[index].flag = bullet_alive;
	}
	auto clip = neutral_clip();
	const auto result = bullets_add_regular_single(
		bullet_pool_view(pool), alice_template(), clip
	);
	return result == BulletAddResult{
		BulletAddStatus::allocated, BulletPoolKind::pellets, 0
	} && pool.pellets[0].flag == bullet_alive;
}

bool test_explicit_unsupported_and_invalid_host()
{
	BulletPool pool{};
	auto clip = neutral_clip();
	auto group_template = alice_template();
	group_template.group = 1;
	if(bullets_add_regular_single(
		bullet_pool_view(pool), group_template, clip
	).status != BulletAddStatus::unsupported_group) return false;
	auto pattern_template = alice_template();
	pattern_template.patnum = 1;
	if(bullets_add_regular_single(
		bullet_pool_view(pool), pattern_template, clip
	).status != BulletAddStatus::unsupported_pattern) return false;
	auto gather_template = alice_template();
	gather_template.spawn_type = 1;
	if(bullets_add_regular_single(
		bullet_pool_view(pool), gather_template, clip
	).status != BulletAddStatus::unsupported_spawn_type) return false;
	clip.bullet_zap = true;
	if(bullets_add_regular_single(
		bullet_pool_view(pool), group_template, clip
	).status != BulletAddStatus::zapped) return false;
	const BulletPoolView malformed{
		std::span<Bullet>{pool.pellets.data(), pool.pellets.size() - 1U},
		std::span<Bullet>{pool.bullets16.data(), pool.bullets16.size()}
	};
	const auto before = pool.pellets[179];
	const auto invalid = bullets_add_regular_single(
		malformed, alice_template(), clip
	);
	return invalid.status == BulletAddStatus::invalid_host &&
		pool.pellets[179] == before;
}

} // namespace

int main()
{
	const struct {
		const char *name;
		bool (*run)();
	} tests[] = {
		{"layout and speed tune", test_layout_and_speed_tune},
		{"reverse single allocation and residue", test_reverse_single_allocation_and_residue},
		{"clip player hit and temporary tune", test_clip_player_hit_and_temporary_tune},
		{"strict playfield and killbox edges", test_strict_playfield_and_killbox_edges},
		{"speed and clear-time flag edges", test_speed_and_clear_time_flag_edges},
		{"clear zap and strict playfield clip", test_clear_zap_and_strict_playfield_clip},
		{"full pool valid no-slot", test_full_pool_is_valid_no_slot_with_clip_side_effect},
		{"first slot selection", test_first_slot_selection},
		{"explicit unsupported and invalid host", test_explicit_unsupported_and_invalid_host},
	};
	for(const auto &test : tests) {
		if(!test.run()) {
			std::cerr << "FAIL " << test.name << '\n';
			return 1;
		}
	}
	std::cout << "PASS " << (sizeof(tests) / sizeof(tests[0])) << " bullet pool tests\n";
	return 0;
}
