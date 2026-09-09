#include "th02/core/bomb.hpp"

#include "pc98/masterlib_vector.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace {

using namespace th02::core;

enum class EventKind : std::uint8_t {
	hud,
	sound,
	bullet_invalidation,
	bomb_invalidation,
	render_all,
};

struct Event {
	EventKind kind{};
	std::int32_t value{};
	EnemyBulletInvalidation bullet{};
	BombInvalidation bomb{};
};

struct Observation {
	bool valid{};
	bool bombing{};
	bool invincible_via_bomb{};
	std::int16_t bomb_frame{};
	ScreenPoint center{};
	std::int16_t circle_frame{};
	std::int16_t circle_done{};
	std::uint8_t stage_bombs_used{};
	std::uint8_t total_bombs_used{};
	std::int8_t bombs{};
};

class Recorder final : public BombPresentationCallbacks {
public:
	std::array<Event, 128U> events{};
	std::size_t count{};
	const BombState *observed_bomb{};
	const ScoreState *observed_score{};
	const PlayerHitState *observed_player{};
	Observation hud_observation{};
	Observation sound_observation{};
	Observation first_bomb_observation{};

	void hud_bombs(std::int8_t bombs) noexcept override
	{
		capture(hud_observation);
		add({EventKind::hud, bombs, {}, {}});
	}

	void sound_effect(std::uint8_t sound_id) noexcept override
	{
		capture(sound_observation);
		add({EventKind::sound, sound_id, {}, {}});
	}

	void invalidate_bullet(EnemyBulletInvalidation invalidation) noexcept override
	{
		add({EventKind::bullet_invalidation, 0, invalidation, {}});
	}

	void invalidate_bomb(BombInvalidation invalidation) noexcept override
	{
		capture(first_bomb_observation);
		add({EventKind::bomb_invalidation, 0, {}, invalidation});
	}

	void render_all() noexcept override
	{
		add({EventKind::render_all, 0, {}, {}});
	}

private:
	void capture(Observation &observation) const noexcept
	{
		if(observation.valid || observed_bomb == nullptr) {
			return;
		}
		observation = {
			true, observed_bomb->bombing,
			observed_player != nullptr &&
				observed_player->invincible_via_bomb,
			observed_bomb->bomb_frame,
			observed_bomb->bomb_circle_center,
			observed_bomb->bomb_circle_frame,
			observed_bomb->bomb_circle_done,
			observed_bomb->stage_bombs_used,
			observed_bomb->total_bombs_used,
			observed_score != nullptr ? observed_score->bombs :
				static_cast<std::int8_t>(0),
		};
	}

	void add(Event event) noexcept
	{
		if(count < events.size()) {
			events[count++] = event;
		}
	}
};

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] std::int16_t add_word(std::int16_t value,
	std::int16_t delta) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(value) + delta));
}

[[nodiscard]] std::int16_t wrap_word(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] EnemyBulletPoolState bullets_with_two_records() noexcept
{
	EnemyBulletPoolState bullets{};
	bullets.bullets[3U].flag = enemy_bullet_alive;
	bullets.bullets[3U].size_type = 1;
	bullets.bullets[3U].screen_topleft_on_page[1U] = {
		to_subpixels(20), to_subpixels(30)};
	bullets.bullets[3U].screen_topleft_on_page[0U] = {
		to_subpixels(120), to_subpixels(130)};
	bullets.bullets[149U].flag = static_cast<std::int8_t>(0x7F);
	bullets.bullets[149U].size_type = 2;
	bullets.bullets[149U].screen_topleft_on_page[1U] = {
		to_subpixels(40), to_subpixels(50)};
	bullets.bullets[149U].screen_topleft_on_page[0U] = {
		to_subpixels(140), to_subpixels(150)};
	return bullets;
}

[[nodiscard]] bool test_bomb_reset_only() noexcept
{
	BombState state{
		true, 29, {217, 193}, 7, 2, 12, 201,
	};
	const auto before = state;
	bomb_reset(state);
	if(state.bombing || state.stage_bombs_used != 0U ||
		state.bomb_frame != before.bomb_frame ||
		state.bomb_circle_center != before.bomb_circle_center ||
		state.bomb_circle_frame != before.bomb_circle_frame ||
		state.bomb_circle_done != before.bomb_circle_done ||
		state.total_bombs_used != before.total_bombs_used) {
		std::fputs("FAIL bomb_reset scope\n", stderr);
		return false;
	}
	return true;
}

[[nodiscard]] bool test_player_bomb_order_and_clear() noexcept
{
	BombState bomb{false, 22, {1, 2}, 9, 2, 0xFFU, 0xFFU};
	ScoreState score{};
	score.bombs = 2;
	PlayerHitState player{};
	player.raw_hit = 0x7F;
	player.invincibility_time = 23;
	player.miss_active = true;
	auto bullets = bullets_with_two_records();
	const auto before = bullets;
	Recorder recorder;
	recorder.observed_bomb = &bomb;
	recorder.observed_score = &score;
	recorder.observed_player = &player;

	if(player_bomb(bomb, score, player, bullets, 1U, 0U, recorder) !=
		PlayerBombResult::started || recorder.count != 4U ||
		recorder.events[0U].kind != EventKind::hud ||
		recorder.events[0U].value != 1 ||
		recorder.events[1U].kind != EventKind::sound ||
		recorder.events[1U].value != 9 ||
		recorder.events[2U].kind != EventKind::bullet_invalidation ||
		recorder.events[3U].kind != EventKind::bullet_invalidation ||
		recorder.events[2U].bullet !=
		EnemyBulletInvalidation{{20, 30}, 8, 8} ||
		recorder.events[3U].bullet !=
		EnemyBulletInvalidation{{40, 50}, 16, 16}) {
		std::fputs("FAIL player_bomb order\n", stderr);
		return false;
	}
	if(!recorder.hud_observation.valid ||
		recorder.hud_observation.bombing != true ||
		!recorder.hud_observation.invincible_via_bomb ||
		recorder.hud_observation.bomb_frame != 22 ||
		recorder.hud_observation.center != ScreenPoint{1, 2} ||
		recorder.hud_observation.circle_frame != 9 ||
		recorder.hud_observation.circle_done != 2 ||
		recorder.hud_observation.stage_bombs_used != 0xFFU ||
		recorder.hud_observation.total_bombs_used != 0xFFU ||
		recorder.hud_observation.bombs != 1 ||
		!recorder.sound_observation.valid ||
		recorder.sound_observation.bomb_frame != 0 ||
		recorder.sound_observation.stage_bombs_used != 0U ||
		recorder.sound_observation.total_bombs_used != 0U ||
		recorder.sound_observation.center != ScreenPoint{1, 2} ||
		recorder.sound_observation.circle_frame != 9 ||
		recorder.sound_observation.circle_done != 2 ||
		recorder.sound_observation.bombs != 1) {
		std::fputs("FAIL player_bomb prefix snapshots\n", stderr);
		return false;
	}
	if(!bomb.bombing || !player.invincible_via_bomb || score.bombs != 1 ||
		bomb.bomb_frame != 0 || bomb.stage_bombs_used != 0U ||
		bomb.total_bombs_used != 0U || bomb.bomb_circle_center !=
			ScreenPoint{220, 196} || bomb.bomb_circle_frame != 0 ||
		bomb.bomb_circle_done != 0 ||
		player.raw_hit != 0x7F || player.invincibility_time != 23 ||
		!player.miss_active ||
		bullets.bullets[3U].flag != enemy_bullet_remove ||
		bullets.bullets[3U].screen_topleft_on_page[1U] !=
			before.bullets[3U].screen_topleft_on_page[0U] ||
		bullets.bullets[149U].flag != static_cast<std::int8_t>(0x7F) ||
		bullets.bullets[149U].screen_topleft_on_page[1U] !=
			before.bullets[149U].screen_topleft_on_page[0U]) {
		std::fputs("FAIL player_bomb state\n", stderr);
		return false;
	}
	return true;
}

[[nodiscard]] bool test_player_bomb_guards_and_page_refusal() noexcept
{
	BombState active{true, 8, {220, 196}, 4, 0, 3, 4};
	ScoreState score{};
	score.bombs = 0;
	PlayerHitState player{};
	auto bullets = bullets_with_two_records();
	const auto before_active = active;
	const auto before_score = score;
	const auto before_player = player;
	const auto before_bullets = bullets;
	Recorder guard;
	if(player_bomb(active, score, player, bullets, 0U, 1U, guard) !=
		PlayerBombResult::already_bombing || guard.count != 0U ||
		active != before_active || score != before_score ||
		player != before_player || bullets != before_bullets) {
		std::fputs("FAIL active bomb guard\n", stderr);
		return false;
	}

	active.bombing = false;
	Recorder empty;
	if(player_bomb(active, score, player, bullets, 0U, 1U, empty) !=
		PlayerBombResult::no_bombs || empty.count != 0U ||
		active.bombing || score.bombs != 0 || player != before_player ||
		bullets != before_bullets) {
		std::fputs("FAIL empty bomb guard\n", stderr);
		return false;
	}

	active = {false, 99, {1, 2}, 7, 2, 0xFEU, 0xFEU};
	score.bombs = 1;
	player = {};
	bullets = bullets_with_two_records();
	const auto before_refusal = bullets;
	Recorder refusal;
	if(player_bomb(active, score, player, bullets, 2U, 0U, refusal) !=
		PlayerBombResult::invalid_page || refusal.count != 2U ||
		refusal.events[0U].kind != EventKind::hud ||
		refusal.events[1U].kind != EventKind::sound || !active.bombing ||
		score.bombs != 0 || !player.invincible_via_bomb ||
		active.bomb_frame != 0 || active.stage_bombs_used != 0xFFU ||
		active.total_bombs_used != 0xFFU ||
		active.bomb_circle_center != ScreenPoint{220, 196} ||
		active.bomb_circle_frame != 0 || active.bomb_circle_done != 0 ||
		bullets != before_refusal) {
		std::fputs("FAIL invalid page prefix\n", stderr);
		return false;
	}
	return true;
}

[[nodiscard]] std::size_t expected_ring_count(const BombState &state,
	std::array<BombInvalidation, 64U> &expected) noexcept
{
	const auto frame = add_word(state.bomb_frame, -1);
	const auto radius = wrap_word(256 -
		(static_cast<std::int32_t>(frame) * 8));
	std::uint8_t angle = static_cast<std::uint8_t>(frame);
	std::size_t count = 0U;
	for(std::uint8_t i = 0U; i < 64U; ++i) {
		const auto offset = pc98::masterlib::vector2(angle, radius);
		const ScreenPoint point{
			add_word(state.bomb_circle_center.x, offset.x),
			add_word(state.bomb_circle_center.y, offset.y),
		};
		if(point.y > 8 && point.y < 384) {
			expected[count++] = {point, 8, 8};
		}
		angle = static_cast<std::uint8_t>(angle + 4U);
	}
	return count;
}

[[nodiscard]] bool test_bomb_invalidate_ring_and_restore() noexcept
{
	BombState state{true, 3, {220, 196}, 2, 0, 0, 0};
	const auto before = state;
	std::array<BombInvalidation, 64U> expected{};
	const auto expected_count = expected_ring_count(state, expected);
	Recorder recorder;
	recorder.observed_bomb = &state;
	bomb_invalidate(state, 0U, recorder);
	if(recorder.count != expected_count || state.bomb_frame != before.bomb_frame ||
		state.bomb_circle_frame != before.bomb_circle_frame ||
		state.bomb_circle_center != before.bomb_circle_center ||
		state.bombing != before.bombing || state.bomb_circle_done != 0 ||
		state.stage_bombs_used != before.stage_bombs_used ||
		state.total_bombs_used != before.total_bombs_used) {
		std::fputs("FAIL bomb ring restoration\n", stderr);
		return false;
	}
	for(std::size_t i = 0U; i < expected_count; ++i) {
		if(recorder.events[i].kind != EventKind::bomb_invalidation ||
			recorder.events[i].bomb != expected[i]) {
			std::fputs("FAIL bomb ring sample\n", stderr);
			return false;
		}
	}
	if(!recorder.first_bomb_observation.valid ||
		recorder.first_bomb_observation.bomb_frame != 2) {
		std::fputs("FAIL temporary bomb frame snapshot\n", stderr);
		return false;
	}

	state = {true, 1, {220, 196}, 2, 0, 0, 0};
	recorder = {};
	bomb_invalidate(state, 0U, recorder);
	if(recorder.count == 0U || recorder.events[0U].kind !=
		EventKind::bomb_invalidation || recorder.events[0U].bomb !=
		BombInvalidation{{476, 196}, 8, 8}) {
		std::fputs("FAIL cardinal bomb sample\n", stderr);
		return false;
	}

	state.bomb_frame = 33;
	state.bomb_circle_frame = 2;
	recorder = {};
	bomb_invalidate(state, 0U, recorder);
	if(state.bomb_frame != 33 || state.bomb_circle_frame != 0 ||
		recorder.count != 64U) {
		std::fputs("FAIL bomb circle-frame reset\n", stderr);
		return false;
	}
	for(std::size_t i = 0U; i < recorder.count; ++i) {
		if(recorder.events[i].bomb !=
			BombInvalidation{{220, 196}, 8, 8}) {
			std::fputs("FAIL zero-radius samples\n", stderr);
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool test_bomb_invalidate_redraw_gates() noexcept
{
	BombState state{true, 136, {220, 196}, 1, 2, 0, 0};
	Recorder recorder;
	bomb_invalidate(state, 0U, recorder);
	if(recorder.count != 1U || recorder.events[0U].kind !=
		EventKind::render_all) {
		std::fputs("FAIL shottype zero gate\n", stderr);
		return false;
	}
	state.bomb_frame = 164;
	recorder = {};
	bomb_invalidate(state, 1U, recorder);
	if(recorder.count != 1U || recorder.events[0U].kind !=
		EventKind::render_all) {
		std::fputs("FAIL shottype one gate\n", stderr);
		return false;
	}
	state.bomb_frame = 112;
	recorder = {};
	bomb_invalidate(state, 2U, recorder);
	if(recorder.count != 1U || recorder.events[0U].kind !=
		EventKind::render_all) {
		std::fputs("FAIL shottype default gate\n", stderr);
		return false;
	}
	state.bomb_frame = 138;
	recorder = {};
	bomb_invalidate(state, 0U, recorder);
	if(recorder.count != 0U) {
		std::fputs("FAIL shottype gate boundary\n", stderr);
		return false;
	}
	state.bomb_frame = 32;
	state.bomb_circle_done = 1;
	recorder = {};
	bomb_invalidate(state, 0U, recorder);
	if(recorder.count != 1U) {
		std::fputs("FAIL raw bool16 true gate\n", stderr);
		return false;
	}
	state.bomb_circle_done = 2;
	recorder = {};
	bomb_invalidate(state, 0U, recorder);
	if(recorder.count != 0U) {
		std::fputs("FAIL raw bool16 nontrue gate\n", stderr);
		return false;
	}
	state.bombing = false;
	state.bomb_circle_done = 1;
	recorder = {};
	bomb_invalidate(state, 0U, recorder);
	if(recorder.count != 0U) {
		std::fputs("FAIL inactive bomb invalidation\n", stderr);
		return false;
	}
	return true;
}

[[nodiscard]] bool test_bomb_invalidate_strict_y_bounds() noexcept
{
	// At bomb_frame 33 the temporary frame is 32, making the radius exactly
	// zero. This isolates the source's strict top > 8 and top < 384 tests.
	for(const auto [center_y, expected_count] : std::array{
		std::pair<std::int16_t, std::size_t>{
			static_cast<std::int16_t>(8), 0U},
		std::pair<std::int16_t, std::size_t>{
			static_cast<std::int16_t>(9), 64U},
		std::pair<std::int16_t, std::size_t>{
			static_cast<std::int16_t>(383), 64U},
		std::pair<std::int16_t, std::size_t>{
			static_cast<std::int16_t>(384), 0U},
	}) {
		BombState state{true, 33, {220, center_y}, 2, 0, 0, 0};
		Recorder recorder;
		bomb_invalidate(state, 0U, recorder);
		if(recorder.count != expected_count || state.bomb_frame != 33 ||
			state.bomb_circle_frame != 0) {
			std::fputs("FAIL strict bomb y bounds\n", stderr);
			return false;
		}
	}
	return true;
}

} // namespace

int main()
{
	if(!test_bomb_reset_only() || !test_player_bomb_order_and_clear() ||
		!test_player_bomb_guards_and_page_refusal() ||
		!test_bomb_invalidate_ring_and_restore() ||
		!test_bomb_invalidate_redraw_gates() ||
		!test_bomb_invalidate_strict_y_bounds()) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
