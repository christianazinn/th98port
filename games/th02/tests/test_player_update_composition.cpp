#include "th02/core/player.hpp"
#include "th02/core/shot.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

constexpr std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return static_cast<std::int16_t>(pixels << 4);
}

void configure_alive_regular(PlayerShotPoolState &shots) noexcept
{
	auto &shot = shots.shots[0];
	shot.flag = player_shot_alive;
	shot.decay_cel = 0U;
	shot.from_option = false;
	shot.patnum = 0x31U;
	shot.pos_on_page[0] = {to_subpixels(20), to_subpixels(30)};
	shot.pos_on_page[1] = {to_subpixels(100), to_subpixels(200)};
	shot.velocity = {16, -16};
}

void configure_decaying_option(PlayerShotPoolState &shots) noexcept
{
	auto &shot = shots.shots[0];
	shot.flag = player_shot_alive;
	shot.decay_cel = 2U;
	shot.from_option = true;
	shot.patnum = 0x42U;
	shot.pos_on_page[1] = {to_subpixels(100), to_subpixels(200)};
	shot.velocity = {0, -17};
}

class RecordingMiss final : public PlayerUpdateCallbacks {
public:
	explicit RecordingMiss(const PlayerShotPoolState &shots_arg) noexcept
		: shots(&shots_arg)
	{
	}

	const PlayerShotPoolState *shots{};
	std::size_t calls{};
	bool sees_shot_lifetime_prefix{};
	PlayerHitState replacement{};
	PlayerMissResult result{PlayerMissResult::completed};

	PlayerMissResult player_miss_update(PlayerHitState &state) noexcept override
	{
		++calls;
		sees_shot_lifetime_prefix = shots != nullptr &&
			shots->shots[0].pos_on_page[1] ==
				ShotSubpixelPoint{to_subpixels(101), to_subpixels(199)};
		state = replacement;
		return result;
	}
};

[[nodiscard]] bool test_no_hit_updates_shots_without_miss() noexcept
{
	PlayerHitState player{};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	configure_alive_regular(shots);
	bool quit = true;
	RecordingMiss callbacks{shots};
	callbacks.replacement.raw_hit = 0xFFU;

	return player_update_state_step(
		player, quit, shots, lifetime, 1U, 0U, callbacks
	) == PlayerUpdateResult::completed && callbacks.calls == 0U && quit &&
		shots.shots[0].pos_on_page[1] ==
			ShotSubpixelPoint{to_subpixels(101), to_subpixels(199)} &&
		lifetime.animation_frames[0] == 1U;
}

[[nodiscard]] bool test_callback_clear_of_gameover_does_not_quit() noexcept
{
	PlayerHitState player{0xFFU, 0U, false, false};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	configure_alive_regular(shots);
	bool quit = false;
	RecordingMiss callbacks{shots};
	callbacks.replacement = {0U, 4U, false, true};

	return player_update_state_step(
		player, quit, shots, lifetime, 1U, 0U, callbacks
	) == PlayerUpdateResult::miss_handled && callbacks.calls == 1U &&
		callbacks.sees_shot_lifetime_prefix && !quit &&
		player == callbacks.replacement;
}

[[nodiscard]] bool test_miss_return_survives_callback_clear() noexcept
{
	PlayerHitState player{1U, 0U, false, false};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	configure_alive_regular(shots);
	bool quit = false;
	RecordingMiss callbacks{shots};
	callbacks.replacement = {0U, 7U, false, true};

	return player_update_state_step(
		player, quit, shots, lifetime, 1U, 0U, callbacks
	) == PlayerUpdateResult::miss_handled && callbacks.calls == 1U &&
		callbacks.sees_shot_lifetime_prefix && !quit &&
		player == callbacks.replacement;
}

[[nodiscard]] bool test_callback_gameover_sets_quit_after_callback() noexcept
{
	PlayerHitState player{1U, 0U, false, false};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	configure_alive_regular(shots);
	bool quit = false;
	RecordingMiss callbacks{shots};
	callbacks.replacement = {0xFFU, 6U, true, true};

	return player_update_state_step(
		player, quit, shots, lifetime, 1U, 0U, callbacks
	) == PlayerUpdateResult::miss_handled && callbacks.calls == 1U &&
		callbacks.sees_shot_lifetime_prefix && quit && player.raw_hit == 0U &&
		player.invincibility_time == 6U && player.invincible_via_bomb &&
		player.miss_active;
}

[[nodiscard]] bool test_guard_precedes_shots_and_miss() noexcept
{
	PlayerHitState player{1U, 1U, false, false};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	configure_alive_regular(shots);
	bool quit = false;
	RecordingMiss callbacks{shots};

	return player_update_state_step(
		player, quit, shots, lifetime, 1U, 0U, callbacks
	) == PlayerUpdateResult::completed && callbacks.calls == 0U && !quit &&
		player.raw_hit == 0U && player.invincibility_time == 0U &&
		shots.shots[0].pos_on_page[1] ==
			ShotSubpixelPoint{to_subpixels(101), to_subpixels(199)};
}

[[nodiscard]] bool test_lifetime_refusals_keep_prefix_and_skip_miss() noexcept
{
	PlayerHitState player{1U, 3U, false, true};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	configure_alive_regular(shots);
	const auto shots_before = shots;
	const auto lifetime_before = lifetime;
	bool quit = false;
	RecordingMiss callbacks{shots};
	if(player_update_state_step(
		player, quit, shots, lifetime, 2U, 0U, callbacks
	) != PlayerUpdateResult::shot_lifetime_invalid_page || callbacks.calls != 0U ||
		quit || player.raw_hit != 1U || player.invincibility_time != 2U ||
		shots != shots_before || lifetime != lifetime_before) {
		return false;
	}

	player = {1U, 3U, false, true};
	shots = {};
	lifetime = {};
	lifetime.option_decay_interval = 0;
	configure_decaying_option(shots);
	quit = false;
	RecordingMiss divide_callbacks{shots};
	return player_update_state_step(
		player, quit, shots, lifetime, 1U, 1U, divide_callbacks
	) == PlayerUpdateResult::shot_lifetime_source_divide_fault &&
		divide_callbacks.calls == 0U && !quit && player.raw_hit == 1U &&
		player.invincibility_time == 2U &&
		shots.shots[0].pos_on_page[1] ==
			ShotSubpixelPoint{to_subpixels(100), static_cast<std::int16_t>(to_subpixels(200) - 17)} &&
		shots.shots[0].decay_cel == 2U;
}

[[nodiscard]] bool test_failed_miss_skips_gameover_conversion() noexcept
{
	PlayerHitState player{1U, 0U, false, false};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	configure_alive_regular(shots);
	bool quit = false;
	RecordingMiss callbacks{shots};
	callbacks.replacement = {0xFFU, 0U, false, true};
	callbacks.result = PlayerMissResult::spark_allocation_failed;

	return player_update_state_step(
		player, quit, shots, lifetime, 1U, 0U, callbacks
	) == PlayerUpdateResult::miss_spark_allocation_failed &&
		callbacks.calls == 1U && callbacks.sees_shot_lifetime_prefix && !quit &&
		player == callbacks.replacement;
}

} // namespace

int main()
{
	if(!test_no_hit_updates_shots_without_miss()) {
		std::fputs("FAIL no-hit composition\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_miss_return_survives_callback_clear()) {
		std::fputs("FAIL miss return composition\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_callback_gameover_sets_quit_after_callback()) {
		std::fputs("FAIL callback gameover composition\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_callback_clear_of_gameover_does_not_quit()) {
		std::fputs("FAIL callback gameover clear composition\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_guard_precedes_shots_and_miss()) {
		std::fputs("FAIL guard ordering composition\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_lifetime_refusals_keep_prefix_and_skip_miss()) {
		std::fputs("FAIL lifetime refusal composition\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_failed_miss_skips_gameover_conversion()) {
		std::fputs("FAIL failed miss conversion ordering\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
