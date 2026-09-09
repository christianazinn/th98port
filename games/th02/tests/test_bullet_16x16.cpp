#include "th02/core/bullet.hpp"
#include "th02/core/random.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

using namespace th02::core;

constexpr ScreenPoint spawn{200, 100};
constexpr std::uint8_t input_angle = 0x40U;
constexpr std::int16_t input_speed = 64;
constexpr std::int16_t source_patnum = 0x1234;

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] EnemyBulletPoolState patterned_pool() noexcept
{
	EnemyBulletPoolState state{};
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

[[nodiscard]] bool bullet_has_prefix(const EnemyBullet &bullet,
	std::size_t page_back, std::uint8_t group_or_special_motion,
	std::uint8_t angle, std::uint8_t speed) noexcept
{
	return bullet.flag == enemy_bullet_alive &&
		bullet.size_type == enemy_bullet_16x16 &&
		bullet.group_or_special_motion == group_or_special_motion &&
		bullet.patnum == 0x34U && bullet.u1 == 0U &&
		bullet.angle == angle && bullet.speed == speed &&
		bullet.screen_topleft_on_page[page_back] == ShotSubpixelPoint{
			to_subpixels(spawn.x), to_subpixels(spawn.y),
		};
}

[[nodiscard]] bool test_ordinary_groups_share_pellet_expansion_and_cache() noexcept
{
	using namespace enemy_bullet_group;
	auto state = patterned_pool();
	auto random = patterned_random();
	const auto random_before = random;
	if(bullets_add_16x16(state, random, player_frame(), 0, 1, false, spawn,
		input_angle, spread_2_narrow, source_patnum, input_speed) !=
		EnemyBulletAddResult::completed) return false;
	for(std::size_t i = 0U; i < 2U; ++i) {
		const auto expected_angle = static_cast<std::uint8_t>(
			static_cast<std::int16_t>(input_angle) + ((i == 0U) ? 3 : -3)
		);
		const auto velocity = pc98::masterlib::vector2(expected_angle, input_speed);
		if(!bullet_has_prefix(state.bullets[i], 1U, spread_2_narrow,
			input_angle, static_cast<std::uint8_t>(input_speed)) ||
			state.bullets[i].velocity != ShotSubpixelPoint{velocity.x, velocity.y}) {
			return false;
		}
	}
	if(state.bullets[2U].flag != enemy_bullet_free || random != random_before) {
		return false;
	}

	state = patterned_pool();
	random = patterned_random();
	random.ring[0U] = 0x12U;
	random.ring[1U] = 0x1DU;
	if(bullets_add_16x16(state, random, player_frame(), 0, 1, false, spawn,
		0U, random_angle_and_speed, source_patnum, input_speed) !=
		EnemyBulletAddResult::completed || random.cursor != 2U) return false;
	const auto velocity = pc98::masterlib::vector2(0x12U, 93);
	return bullet_has_prefix(state.bullets[0U], 1U, random_angle_and_speed, 0U,
		static_cast<std::uint8_t>(input_speed)) &&
		state.bullets[0U].velocity == ShotSubpixelPoint{velocity.x, velocity.y} &&
		state.bullets[1U].flag == enemy_bullet_free;
}

[[nodiscard]] bool test_special_threshold_direct_motion_and_gravity_prefix() noexcept
{
	auto state = patterned_pool();
	auto random = patterned_random();
	const auto random_before = random;
	constexpr std::uint8_t unrecognized_special = 0xFEU;
	if(bullets_add_16x16(state, random, player_frame(), 0, 1, false, spawn,
		input_angle, unrecognized_special, source_patnum, input_speed) !=
		EnemyBulletAddResult::completed) return false;
	const auto velocity = pc98::masterlib::vector2(input_angle, input_speed);
	if(!bullet_has_prefix(state.bullets[0U], 1U, unrecognized_special,
		input_angle, static_cast<std::uint8_t>(input_speed)) ||
		state.bullets[0U].velocity != ShotSubpixelPoint{velocity.x, velocity.y} ||
		state.bullets[1U].flag != enemy_bullet_free || random != random_before) {
		return false;
	}

	state = patterned_pool();
	random = patterned_random();
	if(bullets_add_16x16(state, random, player_frame(), 100, 0, false, spawn,
		input_angle, enemy_bullet_special_motion::gravity, source_patnum, 0) !=
		EnemyBulletAddResult::completed || state.easy_slow_skip_cycle != 0 ||
		!bullet_has_prefix(state.bullets[0U], 1U,
			enemy_bullet_special_motion::gravity, input_angle, 0U) ||
		state.bullets[0U].velocity != ShotSubpixelPoint{}) return false;

	state = patterned_pool();
	random = patterned_random();
	if(bullets_add_16x16(state, random, player_frame(), 100, 0, false, spawn,
		input_angle, unrecognized_special, source_patnum, 0) !=
		EnemyBulletAddResult::completed || state.easy_slow_skip_cycle != 1 ||
		state.bullets[0U].flag != enemy_bullet_free) return false;

	state = patterned_pool();
	random = patterned_random();
	return bullets_add_16x16(state, random, player_frame(), 100, 0, false, spawn,
		input_angle, enemy_bullet_group::one, source_patnum, 0) ==
		EnemyBulletAddResult::completed && state.easy_slow_skip_cycle == 1 &&
		state.bullets[0U].flag == enemy_bullet_free;
}

[[nodiscard]] bool test_gravity_clip_bomb_prefix_and_tuned_cache() noexcept
{
	auto state = patterned_pool();
	auto random = patterned_random();
	const auto state_before = state;
	const auto random_before = random;
	if(bullets_add_16x16(state, random, player_frame(), 100, 0, false,
		{23, spawn.y}, input_angle, enemy_bullet_special_motion::gravity,
		source_patnum, 0) != EnemyBulletAddResult::completed ||
		state != state_before || random != random_before) return false;
	if(bullets_add_16x16(state, random, player_frame(), 100, 0, true, spawn,
		input_angle, enemy_bullet_special_motion::gravity, source_patnum, 0) !=
		EnemyBulletAddResult::completed || state != state_before ||
		random != random_before) return false;

	state = patterned_pool();
	state.rank_base_speed = 8;
	random = patterned_random();
	if(bullets_add_16x16(state, random, player_frame(), 24, 1, false, spawn,
		input_angle, enemy_bullet_special_motion::first, source_patnum,
		input_speed) != EnemyBulletAddResult::completed) return false;
	const auto velocity = pc98::masterlib::vector2(input_angle, 104);
	return bullet_has_prefix(state.bullets[0U], 1U,
		enemy_bullet_special_motion::first, input_angle, 104U) &&
		state.bullets[0U].velocity == ShotSubpixelPoint{velocity.x, velocity.y} &&
		state.easy_slow_skip_cycle == 0;
}

[[nodiscard]] bool test_stack_and_source_pool_boundaries() noexcept
{
	auto state = patterned_pool();
	state.stack = 1U;
	auto random = patterned_random();
	if(bullets_add_16x16(state, random, player_frame(), 0, 1, false, spawn,
		input_angle, enemy_bullet_special_motion::first, source_patnum,
		input_speed) != EnemyBulletAddResult::completed ||
		!bullet_has_prefix(state.bullets[0U], 1U,
			enemy_bullet_special_motion::first, input_angle,
			static_cast<std::uint8_t>(input_speed)) ||
		!bullet_has_prefix(state.bullets[1U], 1U,
			enemy_bullet_special_motion::first, input_angle, 32U) ||
		state.bullets[2U].flag != enemy_bullet_free) return false;

	state = patterned_pool();
	for(auto &bullet : state.bullets) bullet.flag = enemy_bullet_alive;
	random = patterned_random();
	const auto state_before = state;
	const auto random_before = random;
	if(bullets_add_16x16(state, random, player_frame(), 0, 1, false, spawn,
		input_angle, enemy_bullet_special_motion::first, source_patnum,
		input_speed) != EnemyBulletAddResult::completed || state != state_before ||
		random != random_before) return false;

	state.bullets[149U].flag = enemy_bullet_free;
	if(bullets_add_16x16(state, random, player_frame(), 0, 1, false, spawn,
		input_angle, enemy_bullet_special_motion::first, source_patnum,
		input_speed) != EnemyBulletAddResult::completed ||
		!bullet_has_prefix(state.bullets[149U], 1U,
			enemy_bullet_special_motion::first, input_angle,
			static_cast<std::uint8_t>(input_speed))) return false;

	state = patterned_pool();
	random = patterned_random();
	const auto frame = player_frame();
	const auto aimed = pc98::masterlib::iatan2_source(
		static_cast<std::int16_t>((frame.player.current.y + 20) - spawn.y),
		static_cast<std::int16_t>((frame.player.current.x + 12) - spawn.x)
	);
	if(!aimed || bullets_add_16x16(state, random, frame, 0, 1, false, spawn,
		input_angle, 0x7FU, source_patnum, input_speed) !=
		EnemyBulletAddResult::completed) return false;
	const auto expected_angle = static_cast<std::uint8_t>(*aimed + input_angle);
	const auto expected_velocity = pc98::masterlib::vector2(expected_angle, input_speed);
	for(const auto &bullet : state.bullets) {
		if(!bullet_has_prefix(bullet, 1U, 0x7FU, input_angle,
			static_cast<std::uint8_t>(input_speed)) ||
			bullet.velocity != ShotSubpixelPoint{expected_velocity.x, expected_velocity.y}) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool test_ordinary_fault_prefix_and_host_page_refusal() noexcept
{
	auto state = patterned_pool();
	auto random = patterned_random();
	auto frame = player_frame();
	frame.player.current = {187, std::numeric_limits<std::int16_t>::min()};
	const auto before = state.bullets[0U];
	if(bullets_add_16x16(state, random, frame, 0, 1, false, {200, 20},
		input_angle, enemy_bullet_group::one_aimed, source_patnum, input_speed) !=
		EnemyBulletAddResult::source_iatan2_fault) return false;
	const auto &faulted = state.bullets[0U];
	if(faulted.flag != enemy_bullet_alive ||
		faulted.size_type != enemy_bullet_16x16 ||
		faulted.group_or_special_motion != enemy_bullet_group::one_aimed ||
		faulted.patnum != 0x34U || faulted.u1 != 0U ||
		faulted.speed != static_cast<std::uint8_t>(input_speed) ||
		faulted.angle != input_angle ||
		faulted.screen_topleft_on_page[1U] != ShotSubpixelPoint{200, 20} ||
		faulted.velocity != before.velocity ||
		faulted.screen_topleft_on_page[0U] != before.screen_topleft_on_page[0U] ||
		faulted.padding != before.padding) return false;

	state = patterned_pool();
	random = patterned_random();
	frame = player_frame();
	frame.page_back = 2U;
	const auto refused_state = state;
	const auto refused_random = random;
	return bullets_add_16x16(state, random, frame, 0, 1, false, spawn,
		input_angle, enemy_bullet_group::one, source_patnum, input_speed) ==
		EnemyBulletAddResult::invalid_page && state == refused_state &&
		random == refused_random;
}

} // namespace

int main()
{
	const struct {
		const char *name;
		bool (*run)();
	} tests[] = {
		{"ordinary groups and cached speed", test_ordinary_groups_share_pellet_expansion_and_cache},
		{"special threshold and gravity", test_special_threshold_direct_motion_and_gravity_prefix},
		{"gravity prefixes and tuned cache", test_gravity_clip_bomb_prefix_and_tuned_cache},
		{"stack and pool boundaries", test_stack_and_source_pool_boundaries},
		{"ordinary fault and host refusal", test_ordinary_fault_prefix_and_host_page_refusal},
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
