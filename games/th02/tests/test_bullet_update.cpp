#include "th02/core/bullet.hpp"
#include "th02/core/bullet_lifecycle.hpp"
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

constexpr std::size_t back_page = 1U;

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] PlayerFrameState player_frame() noexcept
{
	PlayerFrameState frame{};
	frame.page_front = 0U;
	frame.page_back = back_page;
	frame.player.current = {160, 120};
	return frame;
}

void make_alive(EnemyBullet &bullet, std::int8_t size, ScreenPoint topleft,
	ShotSubpixelPoint velocity = {}
) noexcept
{
	bullet.flag = enemy_bullet_alive;
	bullet.size_type = size;
	bullet.screen_topleft_on_page[back_page] = {
		to_subpixels(topleft.x), to_subpixels(topleft.y),
	};
	bullet.velocity = velocity;
}

class PresentationRecorder final : public EnemyBulletPresentationCallbacks {
public:
	std::array<EnemyBulletPresentationEvent, 32U> events{};
	std::size_t count{};

	void present(EnemyBulletPresentationEvent event) noexcept override
	{
		if(count < events.size()) {
			events[count] = event;
		}
		++count;
	}
};

class InvalidationRecorder final : public EnemyBulletInvalidationCallbacks {
public:
	std::size_t count{};

	void invalidate_bullet(EnemyBulletInvalidation) noexcept override
	{
		++count;
	}
};

[[nodiscard]] bool test_move_render_order_and_scroll_word_wrap() noexcept
{
	EnemyBulletPoolState state{};
	make_alive(state.bullets[0U], enemy_bullet_pellet, {100, 80}, {16, 0});
	make_alive(state.bullets[1U], enemy_bullet_16x16, {200, 100});
	state.bullets[1U].patnum = 7U;
	state.bullets[1U].group_or_special_motion = 0U;
	make_alive(state.bullets[2U], enemy_bullet_pellet, {110, 120});
	PlayerHitState hit{};
	PresentationRecorder events{};
	if(bullets_update_and_render(state, player_frame(), hit, {}, 0U, 350,
		events) != EnemyBulletUpdateResult::completed || hit.raw_hit != 0U ||
		events.count != 7U) return false;
	const std::array<EnemyBulletPresentationEvent, 7U> expected{{
		{EnemyBulletPresentationKind::set_pellet_color_white, {}, 0U},
		{EnemyBulletPresentationKind::render_pellet, {101, 30}, 0U},
		{EnemyBulletPresentationKind::grcg_off, {}, 0U},
		{EnemyBulletPresentationKind::render_bullet_16x16, {200, 50}, 7U},
		{EnemyBulletPresentationKind::set_pellet_color_white, {}, 0U},
		{EnemyBulletPresentationKind::render_pellet, {110, 70}, 0U},
		{EnemyBulletPresentationKind::grcg_off, {}, 0U},
	}};
	for(std::size_t i = 0U; i < expected.size(); ++i) {
		if(events.events[i] != expected[i]) return false;
	}
	return state.bullets[0U].screen_topleft_on_page[back_page] ==
		ShotSubpixelPoint{to_subpixels(101), to_subpixels(80)};
}

[[nodiscard]] bool test_clip_and_size_specific_raw_hit_windows() noexcept
{
	EnemyBulletPoolState state{};
	make_alive(state.bullets[0U], enemy_bullet_pellet, {20, 100});
	make_alive(state.bullets[1U], enemy_bullet_pellet, {167, 132});
	make_alive(state.bullets[2U], enemy_bullet_16x16, {157, 124});
	state.bullets[2U].group_or_special_motion = 0U;
	PlayerHitState hit{0U, 99U, true, false};
	PresentationRecorder events{};
	if(bullets_update_and_render(state, player_frame(), hit, {}, 0U, 0,
		events) != EnemyBulletUpdateResult::completed || hit.raw_hit != 1U ||
		state.bullets[0U].flag != enemy_bullet_remove ||
		state.bullets[1U].flag != enemy_bullet_remove ||
		state.bullets[2U].flag != enemy_bullet_remove || events.count != 1U ||
		events.events[0U] != EnemyBulletPresentationEvent{
			EnemyBulletPresentationKind::grcg_off, {}, 0U}) return false;

	state = {};
	make_alive(state.bullets[0U], enemy_bullet_pellet, {166, 132});
	make_alive(state.bullets[1U], enemy_bullet_16x16, {156, 124});
	state.bullets[1U].group_or_special_motion = 0U;
	hit = {};
	events = {};
	return bullets_update_and_render(state, player_frame(), hit, {}, 0U, 0,
		events) == EnemyBulletUpdateResult::completed && hit.raw_hit == 0U &&
		state.bullets[0U].flag == enemy_bullet_alive &&
		state.bullets[1U].flag == enemy_bullet_alive && events.count == 5U;
}

[[nodiscard]] bool test_all_clip_and_hit_boundaries() noexcept
{
	const std::array<ScreenPoint, 4U> clip_edges{{
		{20, 100}, {416, 100}, {100, 384}, {100, 0},
	}};
	for(const auto edge : clip_edges) {
		EnemyBulletPoolState state{};
		make_alive(state.bullets[0U], enemy_bullet_pellet, edge);
		PlayerHitState hit{};
		PresentationRecorder events{};
		if(bullets_update_and_render(state, player_frame(), hit, {}, 0U, 0,
			events) != EnemyBulletUpdateResult::completed || hit.raw_hit != 0U ||
			state.bullets[0U].flag != enemy_bullet_remove || events.count != 1U ||
			events.events[0U].kind != EnemyBulletPresentationKind::grcg_off) {
			return false;
		}
	}

	const std::array<ScreenPoint, 4U> pellet_hit_corners{{
		{167, 132}, {176, 132}, {167, 142}, {176, 142},
	}};
	const std::array<ScreenPoint, 4U> pellet_miss_edges{{
		{166, 132}, {177, 132}, {167, 131}, {167, 143},
	}};
	const std::array<ScreenPoint, 4U> bullet_hit_corners{{
		{157, 124}, {178, 124}, {157, 144}, {178, 144},
	}};
	const std::array<ScreenPoint, 4U> bullet_miss_edges{{
		{156, 124}, {179, 124}, {157, 123}, {157, 145},
	}};
	auto check_hit_bounds = [&](std::int8_t size,
		const std::array<ScreenPoint, 4U> &hit_corners,
		const std::array<ScreenPoint, 4U> &miss_edges
	) noexcept {
		for(const auto point : hit_corners) {
			EnemyBulletPoolState state{};
			make_alive(state.bullets[0U], size, point);
			state.bullets[0U].group_or_special_motion = 0U;
			PlayerHitState hit{0U, 99U, true, false};
			PresentationRecorder events{};
			if(bullets_update_and_render(state, player_frame(), hit, {}, 0U, 0,
				events) != EnemyBulletUpdateResult::completed || hit.raw_hit != 1U ||
				state.bullets[0U].flag != enemy_bullet_remove || events.count != 1U) {
				return false;
			}
		}
		for(const auto point : miss_edges) {
			EnemyBulletPoolState state{};
			make_alive(state.bullets[0U], size, point);
			state.bullets[0U].group_or_special_motion = 0U;
			PlayerHitState hit{};
			PresentationRecorder events{};
			if(bullets_update_and_render(state, player_frame(), hit, {}, 0U, 0,
				events) != EnemyBulletUpdateResult::completed || hit.raw_hit != 0U ||
				state.bullets[0U].flag != enemy_bullet_alive || events.count != 3U) {
				return false;
			}
		}
		return true;
	};
	return check_hit_bounds(enemy_bullet_pellet, pellet_hit_corners,
		pellet_miss_edges) && check_hit_bounds(enemy_bullet_16x16,
		bullet_hit_corners, bullet_miss_edges);
}

[[nodiscard]] bool test_special_chase_homing_and_decelerate() noexcept
{
	const auto frame = player_frame();
	EnemyBulletSpecialState special{3, 2, 1};
	EnemyBulletPoolState state{};
	make_alive(state.bullets[0U], enemy_bullet_16x16, {100, 100});
	state.bullets[0U].group_or_special_motion =
		enemy_bullet_special_motion::chase;
	make_alive(state.bullets[1U], enemy_bullet_16x16, {100, 100});
	state.bullets[1U].group_or_special_motion =
		enemy_bullet_special_motion::homing;
	state.bullets[1U].speed = 64U;
	make_alive(state.bullets[2U], enemy_bullet_16x16, {100, 100}, {-1, 1});
	state.bullets[2U].group_or_special_motion =
		enemy_bullet_special_motion::decelerate_then_turn_aimed;
	state.bullets[2U].speed = 64U;
	PresentationRecorder events{};
	PlayerHitState hit{};
	if(bullets_update_and_render(state, frame, hit, special, 8U, 0, events) !=
		EnemyBulletUpdateResult::completed) return false;
	if(state.bullets[0U].velocity != ShotSubpixelPoint{3, 3}) return false;
	const auto homing_angle = pc98::masterlib::iatan2_source(28, 68);
	if(!homing_angle || state.bullets[1U].velocity !=
		ShotSubpixelPoint{pc98::masterlib::vector2(*homing_angle, 64).x,
			pc98::masterlib::vector2(*homing_angle, 64).y} ||
		state.bullets[1U].u1 != 1U) return false;
	const auto decelerate_angle = pc98::masterlib::iatan2_source(28, 69);
	if(!decelerate_angle || state.bullets[2U].velocity !=
		ShotSubpixelPoint{pc98::masterlib::vector2(*decelerate_angle, 64).x,
			pc98::masterlib::vector2(*decelerate_angle, 64).y} ||
		state.bullets[2U].u1 != 1U ||
		state.bullets[2U].group_or_special_motion != 0U) return false;

	state = {};
	make_alive(state.bullets[0U], enemy_bullet_16x16, {100, 100});
	state.bullets[0U].group_or_special_motion =
		enemy_bullet_special_motion::homing;
	state.bullets[0U].u1 = 1U;
	state.bullets[0U].velocity = {7, -9};
	events = {};
	if(bullets_update_and_render(state, frame, hit, special, 9U, 0,
		events) != EnemyBulletUpdateResult::completed ||
		state.bullets[0U].u1 != 2U ||
		state.bullets[0U].group_or_special_motion != 0U ||
		state.bullets[0U].velocity != ShotSubpixelPoint{7, -9}) {
		return false;
	}

	state = {};
	make_alive(state.bullets[0U], enemy_bullet_16x16, {100, 100});
	state.bullets[0U].group_or_special_motion =
		enemy_bullet_special_motion::homing;
	state.bullets[0U].speed = 64U;
	state.bullets[0U].u1 = 255U;
	events = {};
	if(bullets_update_and_render(state, frame, hit, {0, 256, 0}, 0U, 0,
		events) != EnemyBulletUpdateResult::completed ||
		state.bullets[0U].u1 != 0U ||
		state.bullets[0U].group_or_special_motion !=
			enemy_bullet_special_motion::homing) return false;

	state = {};
	make_alive(state.bullets[0U], enemy_bullet_16x16, {168, 128});
	state.bullets[0U].group_or_special_motion =
		enemy_bullet_special_motion::chase;
	events = {};
	return bullets_update_and_render(state, frame, hit, {5, 0, 0}, 0U, 0,
		events) == EnemyBulletUpdateResult::completed &&
		state.bullets[0U].velocity == ShotSubpixelPoint{};
}

[[nodiscard]] bool test_special_drift_gravity_bounce_and_unknown() noexcept
{
	const auto frame = player_frame();
	EnemyBulletSpecialState special{5, 7, 1};
	EnemyBulletPoolState state{};
	make_alive(state.bullets[0U], enemy_bullet_16x16, {100, 100});
	state.bullets[0U].group_or_special_motion =
		enemy_bullet_special_motion::drift_angle_chase;
	state.bullets[0U].angle = 0U;
	state.bullets[0U].speed = 64U;
	make_alive(state.bullets[1U], enemy_bullet_16x16, {100, 100});
	state.bullets[1U].group_or_special_motion =
		enemy_bullet_special_motion::drift_angle_and_speed;
	state.bullets[1U].angle = 0xFEU;
	state.bullets[1U].speed = 0xFFU;
	state.bullets[1U].u1 = 1U;
	make_alive(state.bullets[2U], enemy_bullet_16x16, {100, 100}, {0, 32767});
	state.bullets[2U].group_or_special_motion =
		enemy_bullet_special_motion::gravity;
	make_alive(state.bullets[3U], enemy_bullet_16x16, {39, 23}, {10, -20});
	state.bullets[3U].group_or_special_motion =
		enemy_bullet_special_motion::bounce_left_right_top_bottom;
	make_alive(state.bullets[4U], enemy_bullet_16x16, {100, 100}, {4, 5});
	state.bullets[4U].group_or_special_motion = 0x86U;
	PresentationRecorder events{};
	PlayerHitState hit{};
	if(bullets_update_and_render(state, frame, hit, special, 0U, 0, events) !=
		EnemyBulletUpdateResult::completed) return false;
	const auto drift_chase = pc98::masterlib::vector2(5U, 64);
	if(state.bullets[0U].velocity != ShotSubpixelPoint{
		static_cast<std::int16_t>(drift_chase.x + 7),
		static_cast<std::int16_t>(drift_chase.y + 7)} ||
		state.bullets[0U].u1 != 1U ||
		state.bullets[0U].group_or_special_motion !=
			enemy_bullet_special_motion::drift_angle_chase) return false;
	if(state.bullets[1U].angle != 3U || state.bullets[1U].speed != 6U ||
		state.bullets[1U].velocity != ShotSubpixelPoint{
			pc98::masterlib::vector2(3U, 6).x,
			pc98::masterlib::vector2(3U, 6).y} ||
		state.bullets[1U].group_or_special_motion != 0U) return false;
	if(state.bullets[2U].velocity.y != -32764) {
		return false;
	}
	if(state.bullets[3U].velocity != ShotSubpixelPoint{-10, 20} ||
		state.bullets[3U].u1 != 2U ||
		state.bullets[3U].group_or_special_motion != 0U) return false;
	return state.bullets[4U].velocity == ShotSubpixelPoint{4, 5} &&
		state.bullets[4U].group_or_special_motion == 0x86U;
}

[[nodiscard]] bool test_fault_prefix_and_host_page_refusal() noexcept
{
	EnemyBulletPoolState state{};
	make_alive(state.bullets[0U], enemy_bullet_pellet, {100, 100});
	make_alive(state.bullets[1U], enemy_bullet_16x16, {200, 8});
	state.bullets[1U].group_or_special_motion =
		enemy_bullet_special_motion::homing;
	state.bullets[1U].speed = 64U;
	make_alive(state.bullets[2U], enemy_bullet_pellet, {110, 110});
	auto frame = player_frame();
	frame.player.current.y = std::numeric_limits<std::int16_t>::min();
	PresentationRecorder events{};
	PlayerHitState hit{};
	const auto third_before = state.bullets[2U];
	if(bullets_update_and_render(state, frame, hit, {0, 2, 0}, 0U, 0,
		events) != EnemyBulletUpdateResult::source_iatan2_fault ||
		state.bullets[1U].u1 != 1U || state.bullets[2U] != third_before ||
		events.count != 2U || events.events[0U].kind !=
			EnemyBulletPresentationKind::set_pellet_color_white ||
		events.events[1U].kind != EnemyBulletPresentationKind::render_pellet) {
		return false;
	}

	const auto before = state;
	frame.page_back = 2U;
	events = {};
	return bullets_update_and_render(state, frame, hit, {}, 0U, 0, events) ==
		EnemyBulletUpdateResult::invalid_page && state == before &&
		events.count == 0U;
}

[[nodiscard]] bool test_allocate_invalidate_then_update() noexcept
{
	EnemyBulletPoolState state{};
	RandomState random{};
	auto frame = player_frame();
	if(bullets_add_16x16(state, random, frame, 0, 0, false, {100, 100}, 0U,
		enemy_bullet_special_motion::gravity, 4, 64) !=
		EnemyBulletAddResult::completed) return false;
	const auto velocity_before = state.bullets[0U].velocity;
	InvalidationRecorder invalidation{};
	if(bullets_invalidate(state, 0U, back_page, invalidation) !=
		EnemyBulletLifecycleResult::completed || invalidation.count != 1U ||
		state.bullets[0U].screen_topleft_on_page[0U] !=
			state.bullets[0U].screen_topleft_on_page[back_page]) return false;
	frame.page_back = 0U;
	PresentationRecorder events{};
	PlayerHitState hit{};
	return bullets_update_and_render(state, frame, hit, {1, 0, 0}, 0U, 0,
		events) == EnemyBulletUpdateResult::completed &&
		state.bullets[0U].velocity.y ==
			static_cast<std::int16_t>(velocity_before.y + 1) && events.count == 3U &&
		events.events[0U].kind == EnemyBulletPresentationKind::grcg_off &&
		events.events[1U].kind == EnemyBulletPresentationKind::render_bullet_16x16 &&
		events.events[2U].kind == EnemyBulletPresentationKind::grcg_off;
}

} // namespace

int main()
{
	const struct {
		const char *name;
		bool (*run)();
	} tests[] = {
		{"move render order and scroll", test_move_render_order_and_scroll_word_wrap},
		{"clip and raw hit windows", test_clip_and_size_specific_raw_hit_windows},
		{"all clip and hit boundaries", test_all_clip_and_hit_boundaries},
		{"chase homing and decelerate", test_special_chase_homing_and_decelerate},
		{"drift gravity bounce and unknown", test_special_drift_gravity_bounce_and_unknown},
		{"fault prefix and host refusal", test_fault_prefix_and_host_page_refusal},
		{"allocate invalidate update", test_allocate_invalidate_then_update},
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
