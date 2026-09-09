#include "th02/core/player_bomb_update.hpp"

#include "th02/core/random.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

namespace {

using namespace th02::core;

enum class EventKind : std::uint8_t {
	shot_sound,
	hud_bombs,
	bomb_sound,
	bullet_invalidation,
	circle_color,
	circle_point,
	circle_off,
};

struct Event {
	EventKind kind{};
	std::int16_t value{};
	bool operator==(const Event &) const = default;
};

struct Recorder final : BombAnimationCallbacks, BombPresentationCallbacks {
	std::array<Event, 512U> events{};
	std::size_t event_count{};
	std::uint8_t tile_mode{7U};

	void push(EventKind kind, std::int16_t value = 0) noexcept
	{
		if(event_count < events.size()) events[event_count++] = {kind, value};
	}

	void grcg_set_color(std::uint8_t color) noexcept override
	{
		push(EventKind::circle_color, static_cast<std::int16_t>(color));
	}
	void circle_point(ScreenPoint) noexcept override
	{
		push(EventKind::circle_point);
	}
	void particle(ScreenPoint, std::int16_t) noexcept override {}
	void grcg_off() noexcept override { push(EventKind::circle_off); }

	void invalidate_bullet(EnemyBulletInvalidation) noexcept override
	{
		push(EventKind::bullet_invalidation);
	}
	BombRgb palette_get(std::uint8_t) noexcept override { return {}; }
	void palette_set(std::uint8_t, BombRgb) noexcept override {}
	void palette_show() noexcept override {}
	std::uint8_t tile_mode_get() noexcept override { return tile_mode; }
	void tile_mode_set(std::uint8_t mode) noexcept override { tile_mode = mode; }
	void boxfill(std::int16_t, std::int16_t, std::int16_t,
		std::int16_t) noexcept override {}
	void bomb_bft(std::int16_t) noexcept override {}
	void bomb_b_tram(std::uint8_t) noexcept override {}
	void bomb_smear(std::uint8_t, std::int16_t) noexcept override {}
	void pi_put(std::int16_t, std::int16_t, std::uint8_t) noexcept override {}
	void sound_effect(std::uint8_t sound_id) noexcept override
	{
		push(sound_id == 1U ? EventKind::shot_sound : EventKind::bomb_sound,
			static_cast<std::int16_t>(sound_id));
	}
	void overlay_wipe() noexcept override {}
	void hud_bombs(std::int8_t bombs) noexcept override
	{
		push(EventKind::hud_bombs, bombs);
	}
	void invalidate_bomb(BombInvalidation) noexcept override {}
	void render_all() noexcept override {}
};

struct Fixture {
	PlayerFrameState frame{};
	BossState boss{};
	PlayerHitState player{};
	MovementSpeeds speeds{4, 4, 3, 3};
	std::uint16_t input{};
	PlayerShotStreamState stream{};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState shot_lifetime{};
	PlayerShotVolleyContext volley{};
	RandomState random{};
	BombState bomb{};
	ScoreState score{};
	EnemyBulletPoolState bullets{};
	SparkPoolState sparks{};
	BombAnimationState animation{};
	BombEffectState effects{};
	Recorder callbacks{};

	Fixture() noexcept
	{
		frame.player.on_page = {{{100, 200}, {100, 200}}};
		frame.player.current = {100, 200};
		frame.option_left_on_page = {{{84, 216}, {84, 216}}};
		frame.page_front = 0U;
		frame.page_back = 1U;
		boss.target = {180, 100};
		volley.type = PlayerShotType::a;
		stream.stream_a_phase = 0xFFU;
		stream.stream_b_phase = 0xFFU;
	}
};

[[nodiscard]] PlayerBombUpdateOutcome run(Fixture &fixture) noexcept
{
	PlayerBombUpdateContext context{
		fixture.frame,
		fixture.boss,
		fixture.player,
		fixture.speeds,
		fixture.input,
		fixture.stream,
		fixture.shots,
		fixture.shot_lifetime,
		fixture.volley,
		fixture.random,
		fixture.bomb,
		fixture.score,
		fixture.bullets,
		fixture.sparks,
		fixture.animation,
		fixture.effects,
		BombSelection::reimu_a,
		0,
		false,
	};
	return player_move_and_bomb_update(context, fixture.callbacks, fixture.callbacks);
}

[[nodiscard]] bool test_real_shot_then_bomb_order() noexcept
{
	Fixture fixture;
	fixture.input = 0x0010U | 0x0020U;
	fixture.score.bombs = 1;
	const auto outcome = run(fixture);
	if(outcome.player_result != PlayerFrameResult::completed ||
		!outcome.bomb_request_result.has_value() ||
		*outcome.bomb_request_result != PlayerBombResult::started ||
		!outcome.bomb_update_called ||
		outcome.bomb_result != BombUpdateResult::running ||
		fixture.shots.shots[0].flag != player_shot_alive ||
		fixture.score.bombs != 0 || !fixture.bomb.bombing ||
		fixture.bomb.bomb_frame != 1 || fixture.bomb.bomb_circle_frame != 1 ||
		fixture.volley.boss_position != ShotPixelPoint{180, 100}) {
		return false;
	}
	return fixture.callbacks.event_count >= 4U &&
		fixture.callbacks.events[0] == Event{EventKind::shot_sound, 1} &&
		fixture.callbacks.events[1] == Event{EventKind::hud_bombs, 0} &&
		fixture.callbacks.events[2] == Event{EventKind::bomb_sound, 9} &&
		fixture.callbacks.events[3].kind == EventKind::circle_color;
}

[[nodiscard]] bool test_target_handoff_reaches_real_b_volley() noexcept
{
	Fixture fixture;
	fixture.input = 0x0010U;
	fixture.volley.type = PlayerShotType::b;
	fixture.stream.shot_level = 0U;
	fixture.score.bombs = 0;
	const auto outcome = run(fixture);
	return outcome.player_result == PlayerFrameResult::completed &&
		outcome.bomb_result == BombUpdateResult::inactive &&
		!outcome.bomb_request_result.has_value() &&
		fixture.volley.boss_position == ShotPixelPoint{180, 100} &&
		fixture.shots.shots[0].flag == player_shot_alive &&
		fixture.callbacks.events[0] == Event{EventKind::shot_sound, 1};
}

[[nodiscard]] bool test_hit_blocked_still_updates_active_bomb() noexcept
{
	Fixture fixture;
	fixture.player.raw_hit = 1U;
	fixture.bomb.bombing = true;
	fixture.bomb.bomb_frame = 10;
	fixture.input = 0x0010U | 0x0020U;
	const auto outcome = run(fixture);
	return outcome.player_result == PlayerFrameResult::hit_blocked &&
		outcome.bomb_result == BombUpdateResult::running &&
		outcome.bomb_update_called &&
		!outcome.bomb_request_result.has_value() &&
		fixture.bomb.bomb_frame == 11 && fixture.callbacks.event_count > 0U &&
		fixture.callbacks.events[0].kind == EventKind::circle_color;
}

[[nodiscard]] bool test_no_bombs_is_not_an_error() noexcept
{
	Fixture fixture;
	fixture.input = 0x0020U;
	fixture.score.bombs = 0;
	const auto outcome = run(fixture);
	return outcome.player_result == PlayerFrameResult::completed &&
		outcome.bomb_result == BombUpdateResult::inactive &&
		outcome.bomb_update_called && outcome.bomb_request_result.has_value() &&
		*outcome.bomb_request_result == PlayerBombResult::no_bombs &&
		fixture.callbacks.event_count == 0U && !fixture.bomb.bombing;
}

[[nodiscard]] bool test_already_bombing_is_not_an_error() noexcept
{
	Fixture fixture;
	fixture.input = 0x0020U;
	fixture.bomb.bombing = true;
	fixture.bomb.bomb_frame = 10;
	const auto outcome = run(fixture);
	return outcome.player_result == PlayerFrameResult::completed &&
		outcome.bomb_result == BombUpdateResult::running &&
		outcome.bomb_update_called && outcome.bomb_request_result.has_value() &&
		*outcome.bomb_request_result == PlayerBombResult::already_bombing &&
		fixture.bomb.bomb_frame == 11 && fixture.callbacks.event_count > 0U &&
		fixture.callbacks.events[0].kind == EventKind::circle_color;
}

[[nodiscard]] bool test_invalid_bomb_request_stops_before_update() noexcept
{
	Fixture fixture;
	fixture.input = 0x0020U;
	fixture.score.bombs = 1;
	fixture.frame.page_front = 2U;
	const auto outcome = run(fixture);
	return outcome.player_result == PlayerFrameResult::completed &&
		!outcome.bomb_update_called &&
		outcome.bomb_result == BombUpdateResult::inactive &&
		outcome.bomb_request_result.has_value() &&
		*outcome.bomb_request_result == PlayerBombResult::invalid_page &&
		fixture.score.bombs == 0 && fixture.bomb.bombing &&
		fixture.bomb.bomb_frame == 0 && fixture.callbacks.event_count == 2U &&
		fixture.callbacks.events[0] == Event{EventKind::hud_bombs, 0} &&
		fixture.callbacks.events[1] == Event{EventKind::bomb_sound, 9};
}

[[nodiscard]] bool test_hit_guard_precedes_invalid_page() noexcept
{
	Fixture fixture;
	fixture.input = 0x0020U;
	fixture.player.raw_hit = 1U;
	fixture.frame.page_back = 2U;
	fixture.bomb.bombing = true;
	fixture.bomb.bomb_frame = 10;
	const auto outcome = run(fixture);
	return outcome.player_result == PlayerFrameResult::hit_blocked &&
		outcome.bomb_update_called &&
		outcome.bomb_result == BombUpdateResult::invalid_page &&
		!outcome.bomb_request_result.has_value() &&
		fixture.bomb.bomb_frame == 11 && fixture.bomb.bomb_circle_frame == 1 &&
		fixture.callbacks.event_count == 0U;
}

[[nodiscard]] bool test_player_page_refusal_stops_bomb_update() noexcept
{
	Fixture fixture;
	fixture.input = 0x0010U | 0x0020U;
	fixture.frame.page_back = 2U;
	fixture.bomb.bombing = true;
	fixture.bomb.bomb_frame = 10;
	const auto outcome = run(fixture);
	return outcome.player_result == PlayerFrameResult::invalid_page &&
		!outcome.bomb_update_called &&
		outcome.bomb_result == BombUpdateResult::inactive &&
		!outcome.bomb_request_result.has_value() &&
		fixture.bomb.bomb_frame == 10 && fixture.callbacks.event_count == 0U;
}

} // namespace

int main()
{
	if(!test_real_shot_then_bomb_order()) {
		std::fputs("FAIL real shot/bomb order\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_target_handoff_reaches_real_b_volley()) {
		std::fputs("FAIL boss target handoff\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_hit_blocked_still_updates_active_bomb()) {
		std::fputs("FAIL hit-blocked bomb update\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_no_bombs_is_not_an_error()) {
		std::fputs("FAIL no-bombs no-op\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_already_bombing_is_not_an_error()) {
		std::fputs("FAIL already-bombing no-op\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_invalid_bomb_request_stops_before_update()) {
		std::fputs("FAIL invalid bomb request prefix\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_hit_guard_precedes_invalid_page()) {
		std::fputs("FAIL hit/page precedence\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_player_page_refusal_stops_bomb_update()) {
		std::fputs("FAIL player page refusal\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
