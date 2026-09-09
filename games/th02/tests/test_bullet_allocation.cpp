#include "th02/core/bullet.hpp"
#include "th02/core/random.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

using namespace th02::core;

constexpr std::int16_t spawn_left = 200;
constexpr std::int16_t spawn_top = 100;
constexpr std::uint8_t input_angle = 0x40U;
constexpr std::int16_t input_speed = 64;

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] EnemyBulletPoolState patterned_pool() noexcept
{
	EnemyBulletPoolState state{};
	state.rank_base_speed = 0;
	state.stack = 0U;
	state.easy_slow_skip_cycle = 0;
	for(std::size_t i = 0U; i < state.bullets.size(); ++i) {
		auto &bullet = state.bullets[i];
		bullet.flag = enemy_bullet_free;
		bullet.size_type = static_cast<std::int8_t>(20 + static_cast<int>(i));
		bullet.screen_topleft_on_page[0] = {
			static_cast<std::int16_t>(100 + static_cast<int>(i)),
			static_cast<std::int16_t>(200 + static_cast<int>(i)),
		};
		bullet.screen_topleft_on_page[1] = {
			static_cast<std::int16_t>(300 + static_cast<int>(i)),
			static_cast<std::int16_t>(400 + static_cast<int>(i)),
		};
		bullet.velocity = {
			static_cast<std::int16_t>(500 + static_cast<int>(i)),
			static_cast<std::int16_t>(600 + static_cast<int>(i)),
		};
		bullet.patnum = static_cast<std::uint8_t>(i + 7U);
		bullet.group_or_special_motion = static_cast<std::uint8_t>(i + 8U);
		bullet.angle = static_cast<std::uint8_t>(i + 9U);
		bullet.speed = static_cast<std::uint8_t>(i + 10U);
		bullet.u1 = static_cast<std::uint8_t>(i + 11U);
		bullet.padding = static_cast<std::int8_t>(i + 12U);
	}
	return state;
}

[[nodiscard]] RandomState patterned_random() noexcept
{
	RandomState random{};
	random.seed = 0xCAFEBABEU;
	for(std::size_t i = 0U; i < random.ring.size(); ++i) {
		random.ring[i] = static_cast<std::uint8_t>(i);
	}
	return random;
}

[[nodiscard]] PlayerFrameState player_frame() noexcept
{
	PlayerFrameState frame{};
	frame.page_front = 0U;
	frame.page_back = 1U;
	frame.player.current = {160, 220};
	return frame;
}

[[nodiscard]] bool bullet_is(const EnemyBullet &bullet, std::size_t back_page,
	std::uint8_t expected_angle, std::int16_t expected_speed,
	ShotSubpixelPoint expected_position,
	std::uint8_t stored_angle = input_angle) noexcept
{
	const auto velocity = pc98::masterlib::vector2(expected_angle, expected_speed);
	return bullet.flag == enemy_bullet_alive &&
		bullet.size_type == enemy_bullet_pellet &&
		bullet.screen_topleft_on_page[back_page] == expected_position &&
		bullet.velocity == ShotSubpixelPoint{velocity.x, velocity.y} &&
		bullet.group_or_special_motion == 0U && bullet.angle == stored_angle &&
		bullet.u1 == 0U;
}

[[nodiscard]] bool test_back_page_record_write_and_residue() noexcept
{
	auto state = patterned_pool();
	auto expected = state;
	auto random = patterned_random();
	const auto random_before = random;
	const auto result = bullets_add_pellet(state, random, player_frame(), 0, 1,
		false, {spawn_left, spawn_top}, input_angle, enemy_bullet_group::one,
		input_speed);

	auto &bullet = expected.bullets[0U];
	bullet.screen_topleft_on_page[1] = {to_subpixels(spawn_left), to_subpixels(spawn_top)};
	bullet.group_or_special_motion = 0U;
	bullet.flag = enemy_bullet_alive;
	bullet.size_type = enemy_bullet_pellet;
	bullet.u1 = 0U;
	bullet.angle = input_angle;
	const auto velocity = pc98::masterlib::vector2(input_angle, input_speed);
	bullet.velocity = {velocity.x, velocity.y};
	return result == EnemyBulletAddResult::completed && state == expected &&
		random == random_before;
}

struct GroupCase {
	std::uint8_t group{};
	bool aimed{};
	std::size_t count{};
	std::array<std::int16_t, 32U> offsets{};
};

[[nodiscard]] bool test_all_fixed_group_families_and_counts() noexcept
{
	using namespace enemy_bullet_group;
	const std::array<GroupCase, 29U> cases{{
		{spread_2_narrow, false, 2U, {3, -3}},
		{spread_2_medium, false, 2U, {6, -6}},
		{spread_2_wide, false, 2U, {9, -9}},
		{spread_2_narrow_aimed, true, 2U, {3, -3}},
		{spread_2_medium_aimed, true, 2U, {6, -6}},
		{spread_2_wide_aimed, true, 2U, {9, -9}},
		{spread_2_ultrawide_aimed, true, 2U, {14, -14}},
		{spread_3_narrow, false, 3U, {0, 8, -8}},
		{spread_3_medium, false, 3U, {0, 16, -16}},
		{spread_3_wide, false, 3U, {0, 24, -24}},
		{spread_3_narrow_aimed, true, 3U, {0, 8, -8}},
		{spread_3_medium_aimed, true, 3U, {0, 16, -16}},
		{spread_3_wide_aimed, true, 3U, {0, 24, -24}},
		{spread_4_narrow, false, 4U, {3, -3, 9, -9}},
		{spread_4_medium, false, 4U, {9, -9, 27, -27}},
		{spread_4_wide, false, 4U, {15, -15, 45, -45}},
		{spread_4_narrow_aimed, true, 4U, {3, -3, 9, -9}},
		{spread_4_medium_aimed, true, 4U, {9, -9, 27, -27}},
		{spread_4_wide_aimed, true, 4U, {15, -15, 45, -45}},
		{spread_5_narrow, false, 5U, {0, 6, -6, 12, -12}},
		{spread_5_medium, false, 5U, {0, 12, -12, 24, -24}},
		{spread_5_wide, false, 5U, {0, 18, -18, 36, -36}},
		{spread_5_narrow_aimed, true, 5U, {0, 6, -6, 12, -12}},
		{spread_5_medium_aimed, true, 5U, {0, 12, -12, 24, -24}},
		{spread_5_wide_aimed, true, 5U, {0, 18, -18, 36, -36}},
		{ring_2, false, 2U, {0, 128}},
		{ring_4, false, 4U, {0, 64, 128, 192}},
		{ring_8, false, 8U, {0, 32, 64, 96, 128, 160, 192, 224}},
		{ring_16, false, 16U, {0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240}},
	}};
	const auto frame = player_frame();
	const auto aimed = pc98::masterlib::iatan2_source(
		static_cast<std::int16_t>((frame.player.current.y + 20) - spawn_top),
		static_cast<std::int16_t>((frame.player.current.x + 12) - spawn_left)
	);
	if(!aimed) return false;

	for(const auto &test : cases) {
		auto state = patterned_pool();
		auto random = patterned_random();
		const auto random_before = random;
		if(bullets_add_pellet(state, random, frame, 0, 1, false,
			{spawn_left, spawn_top}, input_angle, test.group, input_speed) !=
			EnemyBulletAddResult::completed) return false;
		for(std::size_t i = 0U; i < test.count; ++i) {
			const auto base = test.aimed ? static_cast<std::int16_t>(*aimed) : 0;
			const auto expected_angle = static_cast<std::uint8_t>(
				base + static_cast<std::int16_t>(input_angle) + test.offsets[i]
			);
			if(!bullet_is(state.bullets[i], frame.page_back, expected_angle,
				input_speed, {to_subpixels(spawn_left), to_subpixels(spawn_top)})) {
				return false;
			}
		}
		if(state.bullets[test.count].flag != enemy_bullet_free || random != random_before) {
			return false;
		}
	}

	for(const auto &ring : std::array<GroupCase, 1U>{{
		{ring_32, false, 32U, {0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120,
			128, 136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248}},
	}}) {
		auto state = patterned_pool();
		auto random = patterned_random();
		const auto random_before = random;
		if(bullets_add_pellet(state, random, frame, 0, 1, false,
			{spawn_left, spawn_top}, input_angle, ring.group, input_speed) !=
			EnemyBulletAddResult::completed) return false;
		for(std::size_t i = 0U; i < ring.count; ++i) {
			if(!bullet_is(state.bullets[i], frame.page_back,
				static_cast<std::uint8_t>(input_angle + ring.offsets[i]), input_speed,
				{to_subpixels(spawn_left), to_subpixels(spawn_top)})) return false;
		}
		if(random != random_before) return false;
	}
	return true;
}

[[nodiscard]] bool test_single_symmetric_and_random_groups() noexcept
{
	using namespace enemy_bullet_group;
	auto frame = player_frame();
	auto state = patterned_pool();
	auto random = patterned_random();
	if(bullets_add_pellet(state, random, frame, 0, 1, false,
		{spawn_left, spawn_top}, input_angle, one, input_speed) !=
		EnemyBulletAddResult::completed ||
		!bullet_is(state.bullets[0], 1U, input_angle, input_speed,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)})) return false;

	state = patterned_pool();
	random = patterned_random();
	const auto aim = pc98::masterlib::iatan2_source(
		static_cast<std::int16_t>((frame.player.current.y + 20) - spawn_top),
		static_cast<std::int16_t>((frame.player.current.x + 12) - spawn_left)
	);
	if(!aim || bullets_add_pellet(state, random, frame, 0, 1, false,
		{spawn_left, spawn_top}, input_angle, one_aimed, input_speed) !=
		EnemyBulletAddResult::completed ||
		!bullet_is(state.bullets[0], 1U,
			static_cast<std::uint8_t>(*aim + input_angle), input_speed,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)})) return false;

	state = patterned_pool();
	random = patterned_random();
	constexpr std::uint8_t symmetric_angle = 0x20U;
	if(bullets_add_pellet(state, random, frame, 0, 1, false,
		{spawn_left, spawn_top}, symmetric_angle,
		spread_2_horizontally_symmetric, input_speed) !=
		EnemyBulletAddResult::completed ||
		!bullet_is(state.bullets[0], 1U, symmetric_angle, input_speed,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)}, symmetric_angle) ||
		!bullet_is(state.bullets[1], 1U,
			static_cast<std::uint8_t>(0x80U - symmetric_angle), input_speed,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)}, symmetric_angle)) return false;

	state = patterned_pool();
	random = patterned_random();
	random.ring[0] = 3U;
	if(bullets_add_pellet(state, random, frame, 0, 1, false,
		{spawn_left, spawn_top}, input_angle, one_random_angle, input_speed) !=
		EnemyBulletAddResult::completed || random.cursor != 1U ||
		!bullet_is(state.bullets[0], 1U,
			static_cast<std::uint8_t>(input_angle + 3U), input_speed,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)})) return false;

	state = patterned_pool();
	random = patterned_random();
	random.ring[0] = 10U;
	random.ring[1] = 20U;
	random.ring[2] = 30U;
	if(bullets_add_pellet(state, random, frame, 0, 1, false,
		{spawn_left, spawn_top}, 2U, random_angle, input_speed) !=
		EnemyBulletAddResult::completed || random.cursor != 3U) return false;
	for(std::size_t i = 0U; i < 3U; ++i) {
		if(!bullet_is(state.bullets[i], 1U,
			static_cast<std::uint8_t>(2U + static_cast<std::uint8_t>((i + 1U) * 10U)),
			input_speed, {to_subpixels(spawn_left), to_subpixels(spawn_top)}, 2U)) return false;
	}

	state = patterned_pool();
	random = patterned_random();
	const std::array<std::uint8_t, 6U> ring{1U, 17U, 2U, 18U, 3U, 19U};
	for(std::size_t i = 0U; i < ring.size(); ++i) random.ring[i] = ring[i];
	if(bullets_add_pellet(state, random, frame, 0, 1, false,
		{spawn_left, spawn_top}, 2U, random_angle_and_speed, input_speed) !=
		EnemyBulletAddResult::completed || random.cursor != 6U) return false;
	for(std::size_t i = 0U; i < 3U; ++i) {
		if(!bullet_is(state.bullets[i], 1U,
			static_cast<std::uint8_t>(2U + ring[i * 2U]),
			static_cast<std::int16_t>(input_speed + (ring[(i * 2U) + 1U] & 0x1FU)),
			{to_subpixels(spawn_left), to_subpixels(spawn_top)}, 2U)) return false;
	}
	return true;
}

[[nodiscard]] bool test_default_group_fills_the_available_pool() noexcept
{
	auto state = patterned_pool();
	auto random = patterned_random();
	const auto random_before = random;
	const auto frame = player_frame();
	const auto aimed = pc98::masterlib::iatan2_source(
		static_cast<std::int16_t>((frame.player.current.y + 20) - spawn_top),
		static_cast<std::int16_t>((frame.player.current.x + 12) - spawn_left)
	);
	if(!aimed || bullets_add_pellet(state, random, frame, 0, 1, false,
		{spawn_left, spawn_top}, input_angle, 0U, input_speed) !=
		EnemyBulletAddResult::completed) return false;
	const auto expected_angle = static_cast<std::uint8_t>(*aimed + input_angle);
	for(const auto &bullet : state.bullets) {
		if(!bullet_is(bullet, frame.page_back, expected_angle, input_speed,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)})) return false;
	}
	return random == random_before;
}

[[nodiscard]] bool test_clip_bomb_and_speed_prefixes() noexcept
{
	const auto frame = player_frame();
	for(const auto clip : std::array<ScreenPoint, 4U>{{
		{23, spawn_top}, {417, spawn_top}, {spawn_left, -1}, {spawn_left, 384},
	}}) {
		auto state = patterned_pool();
		auto random = patterned_random();
		const auto before = state;
		const auto random_before = random;
		if(bullets_add_pellet(state, random, frame, 0, 1, false, clip,
			input_angle, enemy_bullet_group::one, input_speed) !=
			EnemyBulletAddResult::completed || state != before || random != random_before) {
			return false;
		}
	}
	for(const auto inside : std::array<ScreenPoint, 4U>{{
		{24, spawn_top}, {416, spawn_top}, {spawn_left, 0}, {spawn_left, 383},
	}}) {
		auto state = patterned_pool();
		auto random = patterned_random();
		if(bullets_add_pellet(state, random, frame, 0, 1, false, inside,
			input_angle, enemy_bullet_group::one, input_speed) !=
			EnemyBulletAddResult::completed || state.bullets[0].flag != enemy_bullet_alive) {
			return false;
		}
	}
	auto state = patterned_pool();
	auto random = patterned_random();
	const auto before_bomb = state;
	if(bullets_add_pellet(state, random, frame, 0, 1, true,
		{spawn_left, spawn_top}, input_angle, enemy_bullet_group::one,
		input_speed) != EnemyBulletAddResult::completed || state != before_bomb) {
		return false;
	}

	state = patterned_pool();
	random = patterned_random();
	if(bullets_add_pellet(state, random, frame, 0, 0, false,
		{spawn_left, spawn_top}, input_angle, enemy_bullet_group::one, 0) !=
		EnemyBulletAddResult::completed || state.easy_slow_skip_cycle != 1 ||
		state.bullets[0].flag != enemy_bullet_free) return false;
	if(bullets_add_pellet(state, random, frame, 0, 0, false,
		{spawn_left, spawn_top}, input_angle, enemy_bullet_group::one, 0) !=
		EnemyBulletAddResult::completed || state.easy_slow_skip_cycle != 2 ||
		!bullet_is(state.bullets[0], 1U, input_angle, 16,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)})) return false;

	state = patterned_pool();
	state.rank_base_speed = -10;
	random = patterned_random();
	if(bullets_add_pellet(state, random, frame, 0, 1, false,
		{spawn_left, spawn_top}, input_angle, enemy_bullet_group::one, 20) !=
		EnemyBulletAddResult::completed || state.easy_slow_skip_cycle != 1 ||
		!bullet_is(state.bullets[0], 1U, input_angle, 16,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)})) return false;

	state = patterned_pool();
	random = patterned_random();
	if(bullets_add_pellet(state, random, frame, 2, 1, false,
		{spawn_left, spawn_top}, input_angle, enemy_bullet_group::one, 30000) !=
		EnemyBulletAddResult::completed) return false;
	// 30000 * 2 wraps to -5536 before the signed /48, producing 29885.
	return bullet_is(state.bullets[0], 1U, input_angle, 29885,
		{to_subpixels(spawn_left), to_subpixels(spawn_top)});
}

[[nodiscard]] bool test_stack_full_and_partial_pool_behavior() noexcept
{
	const auto frame = player_frame();
	auto state = patterned_pool();
	state.stack = 1U;
	auto random = patterned_random();
	if(bullets_add_pellet(state, random, frame, 0, 2, false,
		{spawn_left, spawn_top}, input_angle, enemy_bullet_group::one, 64) !=
		EnemyBulletAddResult::completed ||
		!bullet_is(state.bullets[0], 1U, input_angle, 64,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)}) ||
		!bullet_is(state.bullets[1], 1U, input_angle, 32,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)})) return false;

	state = patterned_pool();
	state.stack = 1U;
	random = patterned_random();
	if(bullets_add_pellet(state, random, frame, 0, 3, false,
		{spawn_left, spawn_top}, input_angle, enemy_bullet_group::one, 64) !=
		EnemyBulletAddResult::completed ||
		!bullet_is(state.bullets[1], 1U, input_angle, 40,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)})) return false;

	state = patterned_pool();
	for(auto &bullet : state.bullets) bullet.flag = enemy_bullet_alive;
	random = patterned_random();
	const auto full_before = state;
	const auto random_before = random;
	if(bullets_add_pellet(state, random, frame, 0, 1, false,
		{spawn_left, spawn_top}, 2U, enemy_bullet_group::random_angle_and_speed,
		input_speed) != EnemyBulletAddResult::completed || state != full_before ||
		random != random_before) return false;

	state = patterned_pool();
	for(auto &bullet : state.bullets) bullet.flag = enemy_bullet_alive;
	state.bullets[3].flag = enemy_bullet_free;
	state.bullets[149].flag = enemy_bullet_free;
	random = patterned_random();
	random.ring[0] = 7U;
	random.ring[1] = 8U;
	random.ring[2] = 9U;
	random.ring[3] = 10U;
	if(bullets_add_pellet(state, random, frame, 0, 1, false,
		{spawn_left, spawn_top}, 2U, enemy_bullet_group::random_angle_and_speed,
		input_speed) != EnemyBulletAddResult::completed || random.cursor != 4U ||
		!bullet_is(state.bullets[3], 1U, 9U, 72,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)}, 2U) ||
		!bullet_is(state.bullets[149], 1U, 11U, 74,
			{to_subpixels(spawn_left), to_subpixels(spawn_top)}, 2U)) return false;
	return true;
}

[[nodiscard]] bool test_source_fault_prefix_and_host_page_refusal() noexcept
{
	auto state = patterned_pool();
	auto random = patterned_random();
	auto frame = player_frame();
	frame.player.current = {187, std::numeric_limits<std::int16_t>::min()};
	const auto before = state.bullets[0];
	if(bullets_add_pellet(state, random, frame, 0, 1, false, {200, 20},
		input_angle, enemy_bullet_group::one_aimed, input_speed) !=
		EnemyBulletAddResult::source_iatan2_fault) return false;
	const auto &faulted = state.bullets[0];
	if(faulted.flag != enemy_bullet_alive || faulted.size_type != enemy_bullet_pellet ||
	faulted.screen_topleft_on_page[1] != ShotSubpixelPoint{200, 20} ||
		faulted.group_or_special_motion != 0U || faulted.angle != input_angle ||
		faulted.u1 != 0U || faulted.velocity != before.velocity ||
		faulted.screen_topleft_on_page[0] != before.screen_topleft_on_page[0] ||
		faulted.patnum != before.patnum || faulted.speed != before.speed ||
		faulted.padding != before.padding) return false;

	state = patterned_pool();
	random = patterned_random();
	frame = player_frame();
	frame.page_back = 2U;
	const auto refused_before = state;
	const auto random_before = random;
	return bullets_add_pellet(state, random, frame, 0, 1, false,
		{spawn_left, spawn_top}, input_angle, enemy_bullet_group::one,
		input_speed) == EnemyBulletAddResult::invalid_page &&
		state == refused_before && random == random_before;
}

} // namespace

int main()
{
	const struct {
		const char *name;
		bool (*run)();
	} tests[] = {
		{"back-page record residue", test_back_page_record_write_and_residue},
		{"fixed group families", test_all_fixed_group_families_and_counts},
		{"single, symmetric and random groups", test_single_symmetric_and_random_groups},
		{"default group fill", test_default_group_fills_the_available_pool},
		{"clip, bomb and speed prefixes", test_clip_bomb_and_speed_prefixes},
		{"stack, full and partial pools", test_stack_full_and_partial_pool_behavior},
		{"source fault and host refusal", test_source_fault_prefix_and_host_page_refusal},
	};
	for(const auto &test : tests) {
		if(!test.run()) {
			std::fputs("FAIL ", stderr);
			std::fputs(test.name, stderr);
			std::fputc('\n', stderr);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
