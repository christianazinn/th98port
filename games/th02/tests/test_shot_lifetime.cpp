#include "th02/core/shot.hpp"

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>

namespace {
using namespace th02::core;

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] PlayerShotPoolState patterned_pool() noexcept
{
	PlayerShotPoolState state{};
	for(std::size_t i = 0U; i < state.shots.size(); ++i) {
		const auto index = static_cast<int>(i);
		state.shots[i] = {
			static_cast<std::uint8_t>(i + 3U),
			static_cast<std::uint8_t>(i + 9U),
			{{
				{static_cast<std::int16_t>(100 + index),
					static_cast<std::int16_t>(200 + index)},
				{static_cast<std::int16_t>(300 + index),
					static_cast<std::int16_t>(400 + index)},
			}},
			{static_cast<std::int16_t>(500 + index),
				static_cast<std::int16_t>(600 + index)},
			static_cast<std::uint8_t>(0x20U + i), (i & 1U) != 0U,
		};
	}
	return state;
}

[[nodiscard]] PlayerShotLifetimeState patterned_lifetime() noexcept
{
	PlayerShotLifetimeState state{};
	for(std::size_t i = 0U; i < state.animation_frames.size(); ++i) {
		state.animation_frames[i] = static_cast<std::uint8_t>(0xE0U + i);
	}
	state.option_shots_alive = 3U;
	state.option_decay_interval = 4;
	return state;
}

void configure_shot(PlayerShot &shot, std::uint8_t flag, std::uint8_t decay,
	bool from_option, std::uint8_t patnum, ShotSubpixelPoint front,
	ShotSubpixelPoint back, ShotSubpixelPoint velocity) noexcept
{
	shot.flag = flag;
	shot.decay_cel = decay;
	shot.from_option = from_option;
	shot.patnum = patnum;
	shot.pos_on_page[0] = front;
	shot.pos_on_page[1] = back;
	shot.velocity = velocity;
}

[[nodiscard]] bool test_update_motion_bounds_and_decay() noexcept
{
	auto pool = patterned_pool();
	auto lifetime = patterned_lifetime();
	const auto frames_before = lifetime.animation_frames;

	configure_shot(pool.shots[0], player_shot_alive, 0U, false, 0x32U,
		{to_subpixels(1), to_subpixels(2)}, {to_subpixels(100), to_subpixels(100)},
		{16, -16});
	configure_shot(pool.shots[1], player_shot_alive, 1U, false, 0x31U,
		{to_subpixels(3), to_subpixels(4)}, {to_subpixels(100), to_subpixels(100)},
		{});
	configure_shot(pool.shots[2], player_shot_alive, 6U, false, 0x7CU,
		{to_subpixels(5), to_subpixels(6)}, {to_subpixels(100), to_subpixels(100)},
		{});
	configure_shot(pool.shots[3], player_shot_alive, 0U, false, 0x31U,
		{to_subpixels(7), to_subpixels(8)}, {to_subpixels(16), to_subpixels(100)},
		{});
	configure_shot(pool.shots[4], player_shot_alive, 0U, false, 0x31U,
		{to_subpixels(9), to_subpixels(10)}, {to_subpixels(100), -1}, {});
	configure_shot(pool.shots[5], player_shot_alive, 1U, true, 0x42U,
		{to_subpixels(11), to_subpixels(12)}, {to_subpixels(100), to_subpixels(100)},
		{3, -16});
	configure_shot(pool.shots[6], player_shot_alive, 1U, true, 0x3BU,
		{to_subpixels(13), to_subpixels(14)}, {to_subpixels(100), to_subpixels(100)},
		{0, -16});
	configure_shot(pool.shots[7], player_shot_alive, 2U, true, 0x42U,
		{to_subpixels(15), to_subpixels(16)}, {to_subpixels(100), to_subpixels(100)},
		{0, -17});
	configure_shot(pool.shots[8], player_shot_alive, 4U, true, 0x42U,
		{to_subpixels(17), to_subpixels(18)}, {to_subpixels(100), to_subpixels(100)},
		{0, -17});
	configure_shot(pool.shots[9], player_shot_alive, 0U, false, 0x31U,
		{to_subpixels(19), to_subpixels(20)}, {to_subpixels(418), to_subpixels(100)},
		{});
	configure_shot(pool.shots[10], player_shot_alive, 0U, false, 0x31U,
		{to_subpixels(21), to_subpixels(22)}, {to_subpixels(417), to_subpixels(100)},
		{});
	configure_shot(pool.shots[11], player_shot_alive, 0U, false, 0x31U,
		{to_subpixels(23), to_subpixels(24)}, {to_subpixels(100), to_subpixels(384)},
		{});
	configure_shot(pool.shots[12], player_shot_alive, 0U, false, 0x31U,
		{to_subpixels(25), to_subpixels(26)}, {to_subpixels(100), to_subpixels(385)},
		{});
	configure_shot(pool.shots[13], player_shot_alive, 0U, false, 0x31U,
		{to_subpixels(27), to_subpixels(28)}, {to_subpixels(100), to_subpixels(8)},
		{});
	configure_shot(pool.shots[14], player_shot_alive, 0U, false, 0x31U,
		{to_subpixels(29), to_subpixels(30)}, {to_subpixels(100), to_subpixels(7)},
		{});
	configure_shot(pool.shots[15], player_shot_alive, 0U, false, 0x31U,
		{to_subpixels(31), to_subpixels(32)}, {to_subpixels(100), to_subpixels(100)},
		{});
	lifetime.animation_frames[15] = 255U;
	configure_shot(pool.shots[16], player_shot_alive, 255U, true, 0x42U,
		{to_subpixels(33), to_subpixels(34)}, {to_subpixels(100), to_subpixels(100)},
		{0, -17});
	configure_shot(pool.shots[37], player_shot_alive, 0U, false, 0x31U,
		{to_subpixels(35), to_subpixels(36)}, {to_subpixels(100), to_subpixels(100)},
		{});
	pool.shots[17].flag = player_shot_remove;
	const auto before = pool;

	if(shots_update_lifetime(pool, lifetime, 1U, 4U) !=
		PlayerShotLifetimeUpdateResult::completed) {
		return false;
	}
	if(pool.shots[0].pos_on_page[1] !=
		ShotSubpixelPoint{to_subpixels(101), to_subpixels(99)} ||
		pool.shots[0].flag != player_shot_alive ||
		lifetime.animation_frames[0] != 0xE1U) {
		return false;
	}
	if(pool.shots[1].decay_cel != 2U ||
		lifetime.animation_frames[1] != 0xE2U ||
		pool.shots[2].flag != player_shot_remove ||
		lifetime.animation_frames[2] != 0xE3U) {
		return false;
	}
	// Bounds happen before regular animation. The negative fractional Y uses
	// source SAR4, so -1 becomes -1 and is outside the top edge.
	if(pool.shots[3].flag != player_shot_remove ||
		lifetime.animation_frames[3] != frames_before[3] ||
		pool.shots[4].flag != player_shot_remove ||
		lifetime.animation_frames[4] != frames_before[4]) {
		return false;
	}
	if(pool.shots[5].pos_on_page[1] !=
		ShotSubpixelPoint{static_cast<std::int16_t>(to_subpixels(100) + 3),
			to_subpixels(99)} ||
		pool.shots[5].velocity.y != -20 ||
		pool.shots[6].velocity.y != -16 || pool.shots[7].decay_cel != 3U ||
		pool.shots[7].velocity.y != -17 ||
		pool.shots[8].flag != player_shot_remove ||
		pool.shots[8].velocity.y != -17) {
		return false;
	}
	if(pool.shots[9].flag != player_shot_remove ||
		pool.shots[10].flag != player_shot_alive ||
		pool.shots[11].flag != player_shot_alive ||
		pool.shots[12].flag != player_shot_remove ||
		pool.shots[13].flag != player_shot_alive ||
		pool.shots[14].flag != player_shot_remove ||
		lifetime.animation_frames[10] != static_cast<std::uint8_t>(frames_before[10] + 1U) ||
		lifetime.animation_frames[11] != static_cast<std::uint8_t>(frames_before[11] + 1U) ||
		lifetime.animation_frames[13] != static_cast<std::uint8_t>(frames_before[13] + 1U) ||
		lifetime.animation_frames[15] != 0U || pool.shots[16].decay_cel != 0U ||
		pool.shots[16].flag != player_shot_alive ||
		lifetime.animation_frames[37] != static_cast<std::uint8_t>(frames_before[37] + 1U)) {
		return false;
	}
	for(const std::size_t i : {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
		9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U, 37U}) {
		if(pool.shots[i].pos_on_page[0] != before.shots[i].pos_on_page[0]) {
			return false;
		}
	}
	return pool.shots[17] == before.shots[17] &&
		lifetime.animation_frames[5] == frames_before[5] &&
		lifetime.animation_frames[8] == frames_before[8];
}

[[nodiscard]] bool test_option_period_width_and_fault_prefix() noexcept
{
	auto pool = PlayerShotPoolState{};
	auto lifetime = PlayerShotLifetimeState{};
	lifetime.option_decay_interval = -1;
	configure_shot(pool.shots[0], player_shot_alive, 2U, true, 0x42U,
		{}, {to_subpixels(100), to_subpixels(100)}, {0, -17});
	if(shots_update_lifetime(pool, lifetime, 1U, 2U) !=
		PlayerShotLifetimeUpdateResult::completed ||
		pool.shots[0].decay_cel != 2U || pool.shots[0].velocity.y != -17) {
		return false;
	}
	if(shots_update_lifetime(pool, lifetime, 1U, 0xFFFFFFFFU) !=
		PlayerShotLifetimeUpdateResult::completed ||
		pool.shots[0].decay_cel != 3U || pool.shots[0].velocity.y != -9) {
		return false;
	}

	pool = {};
	lifetime = {};
	lifetime.option_decay_interval = 0;
	configure_shot(pool.shots[0], player_shot_alive, 2U, true, 0x42U,
		{}, {to_subpixels(16), to_subpixels(100)}, {});
	if(shots_update_lifetime(pool, lifetime, 1U, 0U) !=
		PlayerShotLifetimeUpdateResult::completed ||
		pool.shots[0].flag != player_shot_remove) {
		return false;
	}

	pool = {};
	lifetime = {};
	lifetime.option_decay_interval = 0;
	configure_shot(pool.shots[0], player_shot_alive, 0U, false, 0x31U,
		{}, {to_subpixels(100), to_subpixels(100)}, {16, 0});
	configure_shot(pool.shots[1], player_shot_alive, 2U, true, 0x42U,
		{}, {to_subpixels(100), to_subpixels(100)}, {0, -17});
	configure_shot(pool.shots[2], player_shot_alive, 0U, false, 0x31U,
		{}, {to_subpixels(100), to_subpixels(100)}, {16, 0});
	const auto before = pool;
	if(shots_update_lifetime(pool, lifetime, 1U, 1U) !=
		PlayerShotLifetimeUpdateResult::source_divide_fault ||
		pool.shots[0].pos_on_page[1].x != to_subpixels(101) ||
		lifetime.animation_frames[0] != 1U ||
		pool.shots[1].pos_on_page[1].y !=
			static_cast<std::int16_t>(to_subpixels(100) - 17) ||
		pool.shots[1].decay_cel != 2U || pool.shots[1].velocity.y != -17 ||
		pool.shots[2] != before.shots[2]) {
		return false;
	}
	const auto fault_prefix = pool;
	const auto fault_lifetime = lifetime;
	return shots_update_lifetime(pool, lifetime, 2U, 1U) ==
		PlayerShotLifetimeUpdateResult::invalid_page && pool == fault_prefix &&
		lifetime == fault_lifetime;
}

class RecordingInvalidation final : public PlayerShotInvalidationCallbacks {
public:
	explicit RecordingInvalidation(const PlayerShotPoolState &pool_arg) noexcept
		: pool(&pool_arg)
	{
	}

	const PlayerShotPoolState *pool{};
	std::array<PlayerShotInvalidation, 8U> invalidations{};
	std::size_t count{};
	bool callback_precedes_state_change{true};

	void invalidate_shot(PlayerShotInvalidation invalidation) noexcept override
	{
		if(count < invalidations.size()) invalidations[count] = invalidation;
		if(pool != nullptr) {
			if(count == 0U) {
				callback_precedes_state_change &= pool->shots[0].flag == player_shot_alive &&
					pool->shots[0].pos_on_page[1] ==
						ShotSubpixelPoint{to_subpixels(100), to_subpixels(110)};
			} else if(count == 1U) {
				callback_precedes_state_change &= pool->shots[1].flag == player_shot_remove &&
					pool->shots[1].pos_on_page[1] ==
						ShotSubpixelPoint{to_subpixels(200), to_subpixels(210)};
			}
		}
		++count;
	}
};

[[nodiscard]] bool test_invalidation_order_sizes_pages_and_latch() noexcept
{
	auto pool = PlayerShotPoolState{};
	auto lifetime = PlayerShotLifetimeState{};
	lifetime.option_shots_alive = 1U;
	configure_shot(pool.shots[0], player_shot_alive, 0U, false, 0x31U,
		{to_subpixels(10), to_subpixels(20)}, {to_subpixels(100), to_subpixels(110)},
		{});
	configure_shot(pool.shots[1], player_shot_remove, 2U, false, 0x7CU,
		{to_subpixels(30), to_subpixels(40)}, {to_subpixels(200), to_subpixels(210)},
		{});
	configure_shot(pool.shots[2], 0x7FU, 0U, true, 0x7CU,
		{to_subpixels(50), to_subpixels(60)}, {to_subpixels(300), to_subpixels(310)},
		{});
	configure_shot(pool.shots[3], player_shot_alive, 2U, true, 0x7CU,
		{to_subpixels(70), to_subpixels(80)}, {-1, -17}, {});
	const auto before = pool;
	RecordingInvalidation callbacks{pool};
	if(!shots_invalidate_lifetime(pool, lifetime, 1U, 0U, false, callbacks) ||
		callbacks.count != 4U || !callbacks.callback_precedes_state_change ||
		callbacks.invalidations[0] != PlayerShotInvalidation{{100, 110}, 16, 16} ||
		callbacks.invalidations[1] != PlayerShotInvalidation{{200, 210}, 32, 32} ||
		callbacks.invalidations[2] != PlayerShotInvalidation{{300, 310}, 16, 16} ||
		callbacks.invalidations[3] != PlayerShotInvalidation{{-1, -2}, 32, 32}) {
		return false;
	}
	if(pool.shots[0].pos_on_page[1] != before.shots[0].pos_on_page[0] ||
		pool.shots[1].flag != player_shot_free ||
		pool.shots[1].pos_on_page != before.shots[1].pos_on_page ||
		pool.shots[2].pos_on_page[1] != before.shots[2].pos_on_page[0] ||
		pool.shots[3].pos_on_page[1] != before.shots[3].pos_on_page[0] ||
		lifetime.option_shots_alive != 1U) {
		return false;
	}

	const auto after = pool;
	const auto after_lifetime = lifetime;
	RecordingInvalidation rejected{pool};
	if(shots_invalidate_lifetime(pool, lifetime, 2U, 0U, false, rejected) ||
		rejected.count != 0U || pool != after || lifetime != after_lifetime) {
		return false;
	}

	pool = {};
	lifetime = {};
	configure_shot(pool.shots[0], player_shot_alive, 0U, false, 0x31U,
		{to_subpixels(7), to_subpixels(8)}, {to_subpixels(70), to_subpixels(80)},
		{});
	RecordingInvalidation same_page{pool};
	return shots_invalidate_lifetime(pool, lifetime, 0U, 0U, false, same_page) &&
		same_page.count == 1U && pool.shots[0].pos_on_page[0] ==
			ShotSubpixelPoint{to_subpixels(7), to_subpixels(8)} &&
		pool.shots[0].pos_on_page[1] == ShotSubpixelPoint{
			to_subpixels(70), to_subpixels(80),
		};
}

[[nodiscard]] bool test_reduced_invalidation_and_option_clear() noexcept
{
	auto pool = PlayerShotPoolState{};
	auto lifetime = PlayerShotLifetimeState{};
	lifetime.option_shots_alive = 9U;
	for(std::size_t i = 0U; i < 4U; ++i) {
		configure_shot(pool.shots[i], player_shot_alive, 0U, i == 0U, 0x31U,
			{to_subpixels(static_cast<std::int16_t>(10 + static_cast<int>(i))),
				to_subpixels(20)},
			{to_subpixels(static_cast<std::int16_t>(100 + static_cast<int>(i))),
				to_subpixels(200)}, {});
	}
	pool.shots[1].flag = player_shot_remove;
	pool.shots[1].from_option = true;
	RecordingInvalidation reduced{pool};
	if(!shots_invalidate_lifetime(pool, lifetime, 1U, 0U, true, reduced) ||
		reduced.count != 2U ||
		reduced.invalidations[0].topleft != ShotPixelPoint{101, 200} ||
		reduced.invalidations[1].topleft != ShotPixelPoint{103, 200} ||
		pool.shots[1].flag != player_shot_free || lifetime.option_shots_alive != 9U) {
		return false;
	}

	pool = {};
	lifetime.option_shots_alive = 5U;
	configure_shot(pool.shots[0], player_shot_remove, 2U, true, 0x7CU,
		{to_subpixels(10), to_subpixels(20)}, {to_subpixels(30), to_subpixels(40)},
		{});
	RecordingInvalidation clear_latch{pool};
	return shots_invalidate_lifetime(pool, lifetime, 1U, 0U, false, clear_latch) &&
		clear_latch.count == 1U && pool.shots[0].flag == player_shot_free &&
		lifetime.option_shots_alive == 0U;
}

} // namespace

int main()
{
	if(!test_update_motion_bounds_and_decay()) {
		std::fputs("FAIL update motion/decay\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_option_period_width_and_fault_prefix()) {
		std::fputs("FAIL update period/fault\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_invalidation_order_sizes_pages_and_latch()) {
		std::fputs("FAIL invalidate order/pages\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_reduced_invalidation_and_option_clear()) {
		std::fputs("FAIL invalidate reduced/latch\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
