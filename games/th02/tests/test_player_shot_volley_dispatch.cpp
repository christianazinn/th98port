#include "th02/core/player.hpp"
#include "th02/core/random.hpp"
#include "th02/core/shot.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

[[nodiscard]] RandomState patterned_random() noexcept
{
	RandomState result{};
	for(std::size_t i = 0U; i < result.ring.size(); ++i) {
		result.ring[i] = static_cast<std::uint8_t>(i);
	}
	result.seed = 0x12345678U;
	result.cursor = 20U;
	return result;
}

class Effects final : public PlayerShotStreamEffects {
public:
	explicit Effects(const PlayerShotPoolState &pool_arg) noexcept : pool(&pool_arg) {}

	const PlayerShotPoolState *pool{};
	std::size_t sound_count{};
	std::size_t bomb_count{};
	bool sound_sees_volley{};

	void shot_sound(const PlayerShotStreamState &) noexcept override
	{
		++sound_count;
		sound_sees_volley = pool != nullptr &&
			pool->shots[0].flag == player_shot_alive;
	}

	void bomb_request(const PlayerShotStreamState &) noexcept override
	{
		++bomb_count;
	}
};

[[nodiscard]] PlayerShotVolleyContext context_for(PlayerShotType type) noexcept
{
	PlayerShotVolleyContext context{};
	context.type = type;
	context.positions = {{100, 200}, {300, 400}};
	context.back_page = 1U;
	context.boss_position = {180, 100};
	context.boss_pos_x_unused = 1234;
	return context;
}

[[nodiscard]] bool test_all_three_selected_bodies() noexcept
{
	{
		PlayerShotPoolState pool{};
		PlayerShotLifetimeState lifetime{};
		lifetime.option_shots_alive = 7U;
		auto random = patterned_random();
		auto context = context_for(PlayerShotType::a);
		Effects effects{pool};
		PlayerShotVolleyDispatcher dispatch{
			pool, lifetime, random, context, effects
		};
		PlayerShotStreamState stream{0xFFU, 0xFFU, 0U, 0U, 0U, 0U, 0U,
			0x30U, 0x37U, 0};
		if(player_move_and_shoot_shot_stream_step(
			stream, 0x0010U, dispatch
		) != PlayerShotStreamResult::completed ||
			pool.shots[0].flag != player_shot_alive ||
			pool.shots[0].from_option || pool.shots[0].patnum != 0x34U ||
			lifetime.option_shots_alive != 0U || effects.sound_count != 1U ||
			!effects.sound_sees_volley || effects.bomb_count != 0U ||
			stream.stream_a_phase != 1U || stream.stream_a_cooldown != 8U) {
			return false;
		}
	}

	{
		PlayerShotPoolState pool{};
		PlayerShotLifetimeState lifetime{};
		auto random = patterned_random();
		auto context = context_for(PlayerShotType::b);
		Effects effects{pool};
		PlayerShotVolleyDispatcher dispatch{
			pool, lifetime, random, context, effects
		};
		PlayerShotStreamState stream{0xFFU, 0xFFU, 0U, 0U, 3U, 0U, 0U,
			0x30U, 0x37U, 0};
		if(player_move_and_shoot_shot_stream_step(
			stream, 0x0010U, dispatch
		) != PlayerShotStreamResult::completed ||
			pool.shots[0].flag != player_shot_alive ||
			pool.shots[2].flag != player_shot_alive ||
			!pool.shots[2].from_option || lifetime.option_shots_alive != 1U ||
			context.boss_pos_x_unused != -1 || effects.sound_count != 1U ||
			!effects.sound_sees_volley || stream.stream_a_phase != 1U ||
			stream.stream_a_cooldown != 8U) {
			return false;
		}
	}

	{
		PlayerShotPoolState pool{};
		PlayerShotLifetimeState lifetime{};
		auto random = patterned_random();
		auto context = context_for(PlayerShotType::c);
		Effects effects{pool};
		PlayerShotVolleyDispatcher dispatch{
			pool, lifetime, random, context, effects
		};
		PlayerShotStreamState stream{0xFFU, 0xFFU, 0U, 0U, 2U, 0U, 0U,
			0x30U, 0x37U, 0};
		if(player_move_and_shoot_shot_stream_step(
			stream, 0x0010U, dispatch
		) != PlayerShotStreamResult::completed || context.shot_c_cycle != 1U ||
			pool.shots[0].flag != player_shot_alive || pool.shots[0].patnum != 0x7CU ||
			pool.shots[1].flag != player_shot_alive || !pool.shots[1].from_option ||
			lifetime.option_shots_alive != 1U || effects.sound_count != 1U ||
			!effects.sound_sees_volley || stream.stream_a_phase != 1U ||
			stream.stream_a_cooldown != 8U) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool test_host_refusals_stop_at_selected_volley() noexcept
{
	PlayerShotPoolState pool{};
	PlayerShotLifetimeState lifetime{};
	auto random = patterned_random();
	auto context = context_for(PlayerShotType::invalid);
	const auto pool_before = pool;
	const auto lifetime_before = lifetime;
	const auto random_before = random;
	Effects effects{pool};
	PlayerShotVolleyDispatcher dispatch{pool, lifetime, random, context, effects};
	PlayerShotStreamState stream{0xFFU, 0xFFU, 0U, 0U, 0U, 0xA1U, 0xB2U,
		0x30U, 0x37U, 0};
	if(player_move_and_shoot_shot_stream_step(
		stream, 0x0010U | 0x0020U, dispatch
	) != PlayerShotStreamResult::invalid_shot_selection || pool != pool_before ||
		lifetime != lifetime_before || random != random_before ||
		effects.sound_count != 0U || effects.bomb_count != 0U ||
		stream.stream_a_phase != 0U || stream.stream_a_cooldown != 0U ||
		stream.shot_patnum != 0x34U || stream.shot_option_patnum != 0x3FU) {
		return false;
	}

	pool = {};
	lifetime = {};
	random = patterned_random();
	context = context_for(PlayerShotType::a);
	context.back_page = 2U;
	const auto invalid_pool_before = pool;
	const auto invalid_lifetime_before = lifetime;
	const auto invalid_random_before = random;
	Effects invalid_effects{pool};
	PlayerShotVolleyDispatcher invalid_dispatch{
		pool, lifetime, random, context, invalid_effects
	};
	stream = {0xFFU, 0xFFU, 0U, 0U, 0U, 0xA1U, 0xB2U, 0x30U, 0x37U, 0};
	return player_move_and_shoot_shot_stream_step(
		stream, 0x0010U | 0x0020U, invalid_dispatch
	) == PlayerShotStreamResult::volley_invalid_page && pool == invalid_pool_before &&
		lifetime == invalid_lifetime_before && random == invalid_random_before &&
		invalid_effects.sound_count == 0U && invalid_effects.bomb_count == 0U &&
		stream.stream_a_phase == 0U && stream.stream_a_cooldown == 0U &&
		stream.shot_patnum == 0x34U && stream.shot_option_patnum == 0x3FU;
}

[[nodiscard]] bool test_b_homing_fault_stops_after_its_prefix() noexcept
{
	PlayerShotPoolState pool{};
	PlayerShotLifetimeState lifetime{};
	auto random = patterned_random();
	auto context = context_for(PlayerShotType::b);
	context.positions.player_topleft = {0, 32737};
	context.boss_position = {0, 1};
	Effects effects{pool};
	PlayerShotVolleyDispatcher dispatch{pool, lifetime, random, context, effects};
	PlayerShotStreamState stream{0xFFU, 0xFFU, 0U, 0U, 9U, 0U, 0U,
		0x30U, 0x37U, 0};

	return player_move_and_shoot_shot_stream_step(
		stream, 0x0010U | 0x0020U, dispatch
	) == PlayerShotStreamResult::volley_source_iatan2_fault &&
		pool.shots[0].flag == player_shot_alive &&
		pool.shots[1].flag == player_shot_alive &&
		pool.shots[2].flag == player_shot_free && pool.current_slot == 2 &&
		stream.shot_patnum == 0x32U && stream.stream_a_phase == 0U &&
		stream.stream_a_cooldown == 0U && lifetime.option_shots_alive == 0U &&
		effects.sound_count == 0U && effects.bomb_count == 0U;
}

} // namespace

int main()
{
	if(!test_all_three_selected_bodies()) {
		std::fputs("FAIL selected volley bodies\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_host_refusals_stop_at_selected_volley()) {
		std::fputs("FAIL selected volley host refusals\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_b_homing_fault_stops_after_its_prefix()) {
		std::fputs("FAIL selected volley B fault prefix\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
