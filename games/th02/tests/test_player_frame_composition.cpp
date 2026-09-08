#include "th02/core/player.hpp"
#include "th02/core/random.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

using namespace th02::core;

enum class InvalidationKind { shot, player, option };

struct InvalidationTrace final : PlayerFrameInvalidationCallbacks {
	std::array<InvalidationKind, 8> kinds{};
	std::array<PlayerFrameInvalidation, 8> rectangles{};
	std::size_t count{};

	void record(InvalidationKind kind, PlayerFrameInvalidation rectangle) noexcept
	{
		kinds[count] = kind;
		rectangles[count] = rectangle;
		++count;
	}

	void invalidate_shot(PlayerShotInvalidation rectangle) noexcept override
	{
		record(InvalidationKind::shot, PlayerFrameInvalidation{
			{rectangle.topleft.x, rectangle.topleft.y}, rectangle.width,
			rectangle.height,
		});
	}

	void invalidate_player(PlayerFrameInvalidation rectangle) noexcept override
	{
		record(InvalidationKind::player, rectangle);
	}

	void invalidate_option(PlayerFrameInvalidation rectangle) noexcept override
	{
		record(InvalidationKind::option, rectangle);
	}
};

struct Effects final : PlayerShotStreamEffects {
	std::size_t sound_count{};
	std::size_t bomb_count{};

	void shot_sound(const PlayerShotStreamState &) noexcept override
	{
		++sound_count;
	}

	void bomb_request(const PlayerShotStreamState &) noexcept override
	{
		++bomb_count;
	}
};

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(pixels) << 4U));
}

[[nodiscard]] PlayerFrameState frame_with_pages() noexcept
{
	PlayerFrameState result{};
	result.player.on_page = {{{100, 200}, {300, 400}}};
	result.player.current = {-10, -20};
	result.option_left_on_page = {{{80, 216}, {280, 416}}};
	result.page_front = 0U;
	result.page_back = 1U;
	result.pose = PlayerPose::right;
	return result;
}

[[nodiscard]] PlayerShotVolleyContext context_for_a() noexcept
{
	PlayerShotVolleyContext result{};
	result.type = PlayerShotType::a;
	result.positions = {{-1, -2}, {-3, -4}};
	result.back_page = 0U;
	return result;
}

[[nodiscard]] bool test_invalidation_order_and_player_only_carry() noexcept
{
	auto frame = frame_with_pages();
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	shots.shots[0].flag = player_shot_alive;
	shots.shots[0].pos_on_page = {{{160, 320}, {480, 640}}};
	shots.shots[1].flag = player_shot_remove;
	shots.shots[1].pos_on_page = {{{800, 960}, {1120, 1280}}};
	InvalidationTrace trace{};

	if(player_frame_invalidate(frame, shots, lifetime, false, trace) !=
		PlayerFrameInvalidationResult::completed) {
		return false;
	}
	return trace.count == 5U &&
		trace.kinds[0] == InvalidationKind::shot &&
		trace.kinds[1] == InvalidationKind::shot &&
		trace.kinds[2] == InvalidationKind::player &&
		trace.kinds[3] == InvalidationKind::option &&
		trace.kinds[4] == InvalidationKind::option &&
		trace.rectangles[2] == PlayerFrameInvalidation{{300, 400}, 32, 48} &&
		trace.rectangles[3] == PlayerFrameInvalidation{{280, 416}, 16, 16} &&
		trace.rectangles[4] == PlayerFrameInvalidation{{328, 416}, 16, 16} &&
		frame.player.on_page[0] == ScreenPoint{100, 200} &&
		frame.player.on_page[1] == ScreenPoint{100, 200} &&
		frame.player.current == ScreenPoint{-10, -20} &&
		frame.option_left_on_page[0] == ScreenPoint{80, 216} &&
		frame.option_left_on_page[1] == ScreenPoint{280, 416} &&
		shots.shots[0].pos_on_page[1] == shots.shots[0].pos_on_page[0] &&
		shots.shots[1].flag == player_shot_free;
}

[[nodiscard]] bool test_invalidation_pages_are_explicit() noexcept
{
	auto frame = frame_with_pages();
	frame.page_front = 1U;
	frame.page_back = 1U;
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	InvalidationTrace trace{};
	if(player_frame_invalidate(frame, shots, lifetime, true, trace) !=
		PlayerFrameInvalidationResult::completed || trace.count != 3U ||
		frame.player.on_page[1] != ScreenPoint{300, 400}) {
		return false;
	}

	frame.page_back = 2U;
	const auto frame_before = frame;
	const auto shots_before = shots;
	const auto lifetime_before = lifetime;
	trace = {};
	return player_frame_invalidate(frame, shots, lifetime, false, trace) ==
		PlayerFrameInvalidationResult::invalid_page && frame == frame_before &&
		shots == shots_before && lifetime == lifetime_before && trace.count == 0U;
}

[[nodiscard]] bool test_hit_guard_and_invalid_page_prefixes() noexcept
{
	auto frame = frame_with_pages();
	PlayerHitState hit{1U, 0U, false, false};
	const auto frame_before = frame;
	const auto hit_before = hit;
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	RandomState random{};
	auto context = context_for_a();
	const auto context_before = context;
	Effects effects{};
	PlayerShotVolleyDispatcher dispatcher{shots, lifetime, random, context, effects};
	PlayerShotStreamState stream{};
	const auto stream_before = stream;
	if(player_frame_move_and_shoot(frame, hit, {4, 4, 3, 3}, 0x0010U,
		stream, dispatcher) != PlayerFrameResult::hit_blocked ||
		frame != frame_before || hit != hit_before || context != context_before ||
		stream != stream_before || effects.sound_count != 0U ||
		effects.bomb_count != 0U) {
		return false;
	}

	frame = frame_with_pages();
	frame.page_back = 2U;
	hit = {};
	const auto invalid_frame_before = frame;
	const auto invalid_context_before = context;
	return player_frame_move_and_shoot(frame, hit, {4, 4, 3, 3}, 0x0010U,
		stream, dispatcher) == PlayerFrameResult::invalid_page &&
		frame == invalid_frame_before && context == invalid_context_before &&
		effects.sound_count == 0U && effects.bomb_count == 0U;
}

[[nodiscard]] bool test_invincibility_guard_does_not_decrement() noexcept
{
	auto frame = frame_with_pages();
	PlayerHitState hit{1U, 5U, false, false};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	RandomState random{};
	auto context = context_for_a();
	Effects effects{};
	PlayerShotVolleyDispatcher dispatcher{shots, lifetime, random, context, effects};
	PlayerShotStreamState stream{};
	InvalidationTrace trace{};
	return player_frame_invalidate(frame, shots, lifetime, false, trace) ==
		PlayerFrameInvalidationResult::completed &&
		player_frame_move_and_shoot(frame, hit, {4, 4, 3, 3}, 0U, stream,
		dispatcher) == PlayerFrameResult::completed && hit.raw_hit == 0U &&
		hit.invincibility_time == 5U && frame.player.current == ScreenPoint{100, 200};
}

struct Direction {
	std::uint16_t input{};
	ScreenPoint delta{};
	PlayerPose pose{PlayerPose::still};
};

[[nodiscard]] bool test_every_direction_reaches_a_real_volley() noexcept
{
	constexpr std::array<Direction, 13> directions{{
		{0U, {0, 0}, PlayerPose::still},
		{0x0001U, {0, -4}, PlayerPose::still},
		{0x0002U, {0, 4}, PlayerPose::still},
		{0x0004U, {-4, 0}, PlayerPose::left},
		{0x0008U, {4, 0}, PlayerPose::right},
		{0x1000U, {-3, -3}, PlayerPose::left},
		{0x2000U, {3, -3}, PlayerPose::right},
		{0x4000U, {-3, 3}, PlayerPose::left},
		{0x8000U, {3, 3}, PlayerPose::right},
		{0x0005U, {-3, -3}, PlayerPose::left},
		{0x0009U, {3, -3}, PlayerPose::right},
		{0x0006U, {-3, 3}, PlayerPose::left},
		{0x000AU, {3, 3}, PlayerPose::right},
	}};

	for(const auto direction : directions) {
		auto frame = frame_with_pages();
		PlayerShotPoolState shots{};
		PlayerShotLifetimeState lifetime{};
		RandomState random{};
		auto context = context_for_a();
		Effects effects{};
		PlayerShotVolleyDispatcher dispatcher{shots, lifetime, random, context, effects};
		PlayerShotStreamState stream{0xFFU, 0xFFU, 0U, 0U, 0U, 0U, 0U,
			0x30U, 0x37U, 0};
		PlayerHitState hit{};

		InvalidationTrace trace{};
		if(player_frame_invalidate(frame, shots, lifetime, false, trace) !=
			PlayerFrameInvalidationResult::completed ||
			player_frame_move_and_shoot(frame, hit, {4, 4, 3, 3},
				direction.input | 0x0010U, stream, dispatcher) !=
				PlayerFrameResult::completed) {
			return false;
		}

		const ScreenPoint moved{
			static_cast<std::int16_t>(100 + direction.delta.x),
			static_cast<std::int16_t>(200 + direction.delta.y),
		};
		const ScreenPoint option{
			static_cast<std::int16_t>(moved.x - 16 - direction.delta.x),
			static_cast<std::int16_t>(moved.y + 16 - direction.delta.y),
		};
		if(frame.player.on_page[0] != ScreenPoint{100, 200} ||
			frame.player.on_page[1] != moved || frame.player.current != moved ||
			frame.pose != direction.pose || frame.option_left_on_page[0] !=
				ScreenPoint{80, 216} || frame.option_left_on_page[1] != option ||
			context.positions.player_topleft != ShotPixelPoint{moved.x, moved.y} ||
			context.positions.option_left_topleft_on_back_page !=
				ShotPixelPoint{option.x, option.y} ||
			context.back_page != 1U || shots.shots[0].flag != player_shot_alive ||
			shots.shots[0].pos_on_page[1] != ShotSubpixelPoint{
				to_subpixels(static_cast<std::int16_t>(moved.x + 8)),
				to_subpixels(static_cast<std::int16_t>(moved.y + 32)),
			} || effects.sound_count != 1U || effects.bomb_count != 0U) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool test_boundary_option_uses_requested_delta() noexcept
{
	auto frame = frame_with_pages();
	frame.player.on_page[0] = {28, 16};
	PlayerHitState hit{};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	RandomState random{};
	auto context = context_for_a();
	Effects effects{};
	PlayerShotVolleyDispatcher dispatcher{shots, lifetime, random, context, effects};
	PlayerShotStreamState stream{};
	InvalidationTrace trace{};
	if(player_frame_invalidate(frame, shots, lifetime, false, trace) !=
		PlayerFrameInvalidationResult::completed ||
		player_frame_move_and_shoot(frame, hit, {4, 4, 3, 3}, 0x1010U, stream,
			dispatcher) != PlayerFrameResult::completed) {
		return false;
	}
	return frame.player.current == ScreenPoint{28, 16} &&
		frame.option_left_on_page[1] == ScreenPoint{15, 35} &&
		context.positions.option_left_topleft_on_back_page == ShotPixelPoint{15, 35};
}

[[nodiscard]] bool test_two_explicit_pages_carry_player_and_shot_state() noexcept
{
	auto frame = frame_with_pages();
	PlayerHitState hit{};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	RandomState random{};
	auto context = context_for_a();
	Effects effects{};
	PlayerShotVolleyDispatcher dispatcher{shots, lifetime, random, context, effects};
	PlayerShotStreamState stream{0xFFU, 0xFFU, 0U, 0U, 0U, 0U, 0U,
		0x30U, 0x37U, 0};
	InvalidationTrace trace{};
	if(player_frame_invalidate(frame, shots, lifetime, false, trace) !=
		PlayerFrameInvalidationResult::completed ||
		player_frame_move_and_shoot(frame, hit, {4, 4, 3, 3}, 0x0018U, stream,
			dispatcher) != PlayerFrameResult::completed ||
		frame.player.on_page[1] != ScreenPoint{104, 200} ||
		shots.shots[0].flag != player_shot_alive) {
		return false;
	}
	const auto first_spawn = shots.shots[0].pos_on_page[1];
	if(shots_update_lifetime(shots, lifetime, 1U, 1U) !=
		PlayerShotLifetimeUpdateResult::completed ||
		shots.shots[0].pos_on_page[1] == first_spawn) {
		return false;
	}
	const auto first_updated = shots.shots[0].pos_on_page[1];

	frame.page_front = 1U;
	frame.page_back = 0U;
	trace = {};
	if(player_frame_invalidate(frame, shots, lifetime, false, trace) !=
		PlayerFrameInvalidationResult::completed ||
		shots.shots[0].pos_on_page[0] != first_updated ||
		player_frame_move_and_shoot(frame, hit, {4, 4, 3, 3}, 0x0011U, stream,
			dispatcher) != PlayerFrameResult::completed) {
		return false;
	}
	return frame.player.on_page[1] == ScreenPoint{104, 200} &&
		frame.player.on_page[0] == ScreenPoint{104, 196} &&
		frame.player.current == ScreenPoint{104, 196} &&
		frame.option_left_on_page[0] == ScreenPoint{88, 216} &&
		context.positions.player_topleft == ShotPixelPoint{104, 196} &&
		context.positions.option_left_topleft_on_back_page == ShotPixelPoint{88, 216} &&
		context.back_page == 0U && shots.shots[0].flag == player_shot_alive &&
		stream.stream_a_cooldown == 7U && effects.sound_count == 1U;
}

} // namespace

int main()
{
	if(!test_invalidation_order_and_player_only_carry()) return EXIT_FAILURE;
	if(!test_invalidation_pages_are_explicit()) return EXIT_FAILURE;
	if(!test_hit_guard_and_invalid_page_prefixes()) return EXIT_FAILURE;
	if(!test_invincibility_guard_does_not_decrement()) return EXIT_FAILURE;
	if(!test_every_direction_reaches_a_real_volley()) return EXIT_FAILURE;
	if(!test_boundary_option_uses_requested_delta()) return EXIT_FAILURE;
	if(!test_two_explicit_pages_carry_player_and_shot_state()) return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
