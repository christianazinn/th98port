#include "th02/core/stage_continue.hpp"

#include "th02/core/stage1_state.hpp"

#include <array>
#include <cstdlib>

namespace {

using namespace th02::core;

enum class EventKind : std::uint8_t {
	hud,
	leave,
	tiles,
	bullet,
	palette,
	enter,
};

struct Event {
	EventKind kind{};
	std::size_t page{};
	std::uint16_t tone{};
	bool operator==(const Event &) const = default;
};

[[nodiscard]] bool same_speeds(MovementSpeeds left,
	MovementSpeeds right) noexcept
{
	return left.aligned_x == right.aligned_x && left.aligned_y == right.aligned_y &&
		left.diagonal_x == right.diagonal_x && left.diagonal_y == right.diagonal_y;
}

class Recorder final : public StageContinuePresentationCallbacks {
public:
	explicit Recorder(const Stage1State &state_arg) noexcept : state(&state_arg) {}

	const Stage1State *state{};
	std::array<Event, 8U> events{};
	std::size_t count{};
	bool tile_callbacks_saw_source_prefix{true};

	void hud_put() noexcept override { add({EventKind::hud}); }
	void overlay_stage_leave_animate() noexcept override { add({EventKind::leave}); }
	void tiles_render_all(std::size_t page) noexcept override
	{
		if(state != nullptr) {
			tile_callbacks_saw_source_prefix &=(
				state->frame.player.on_page[0U] == ScreenPoint{208, 336} &&
				state->frame.player.on_page[1U] == ScreenPoint{208, 336} &&
				state->player_hit.invincibility_time == 200U &&
				state->boss.target == ScreenPoint{-1, -1} &&
				state->tiles.copy_lines_top == 0U &&
				state->tiles.copy_lines_h == 16U && state->tile_mode == 0xA5U &&
				state->tiles_egc_render_all
			);
		}
		add({EventKind::tiles, page});
	}
	void invalidate_bullet(EnemyBulletInvalidation) noexcept override
	{
		add({EventKind::bullet});
	}
	void palette_100() noexcept override
	{
		add({EventKind::palette, 0U,
			static_cast<std::uint16_t>((state == nullptr) ? 0U :
				state->bomb_animation.palette_tone)});
	}
	void overlay_stage_enter_animate() noexcept override { add({EventKind::enter}); }

private:
	void add(Event event) noexcept
	{
		if(count < events.size()) events[count++] = event;
	}
};

[[nodiscard]] Stage1State warm_state() noexcept
{
	Stage1State state{};
	state.frame.page_front = 0U;
	state.frame.page_back = 1U;
	state.frame.player.on_page = {{{11, 12}, {13, 14}}};
	state.frame.player.current = {77, 88};
	state.frame.option_left_on_page = {{{21, 22}, {23, 24}}};
	state.player_hit = {0xFFU, 7U, true, true};
	state.player_miss = {12U, 52U, 3, -4, 5U, 6U, 7U};
	state.player_stream = {3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12};
	state.player_volley.type = PlayerShotType::invalid;
	state.player_volley.boss_position = {55, 56};
	state.player_volley.boss_pos_x_unused = 57;
	state.player_volley.shot_c_cycle = 58U;
	state.player_speeds = {12, 13, 14, 15};
	state.player_reset = {0xA1U, 0xA2U, 0xA3U, 0xA4U};
	state.quit = true;
	state.boss.target = {61, 62};
	state.enemy_target_projection.boss_position = {63, 64};
	state.bomb_animation.palette_tone = 27U;
	state.tiles.copy_lines_top = 12U;
	state.tiles.copy_lines_h = 13U;
	state.tile_mode = 0xA5U;
	state.tiles_egc_render_all = true;
	state.stage_frame = 123U;
	state.resident_frame = -456;
	state.scroll.line = 89;
	state.shottype = static_cast<std::uint8_t>(PlayerShotType::b);
	state.bullets.bullets[0U].flag = enemy_bullet_alive;
	state.bullets.bullets[0U].size_type = 1;
	state.bullets.bullets[0U].screen_topleft_on_page[0U] = {160, 320};
	state.bullets.bullets[0U].screen_topleft_on_page[1U] = {480, 640};
	state.bullets.bullets[1U].flag = enemy_bullet_remove;
	state.bullets.bullets[1U].size_type = 2;
	state.bullets.bullets[1U].screen_topleft_on_page[0U] = {176, 336};
	state.bullets.bullets[1U].screen_topleft_on_page[1U] = {496, 656};
	return state;
}

[[nodiscard]] bool test_complete_sequence_uses_real_reset_and_clear() noexcept
{
	auto state = warm_state();
	const auto before_current = state.frame.player.current;
	const auto before_options = state.frame.option_left_on_page;
	const auto before_enemy_projection = state.enemy_target_projection;
	const auto before_stage_frame = state.stage_frame;
	const auto before_resident_frame = state.resident_frame;
	const auto before_scroll = state.scroll;
	Recorder recorder{state};

	if(stage1_continue_resume(state, recorder) != StageContinueResult::completed ||
		recorder.count != 7U || !recorder.tile_callbacks_saw_source_prefix ||
		recorder.events[0U] != Event{EventKind::hud} ||
		recorder.events[1U] != Event{EventKind::leave} ||
		recorder.events[2U] != Event{EventKind::tiles, 0U} ||
		recorder.events[3U] != Event{EventKind::tiles, 1U} ||
		recorder.events[4U] != Event{EventKind::bullet} ||
		recorder.events[5U] != Event{EventKind::palette, 0U, 100U} ||
		recorder.events[6U] != Event{EventKind::enter}) {
		return false;
	}
	return state.frame.player.on_page[0U] == ScreenPoint{208, 336} &&
		state.frame.player.on_page[1U] == ScreenPoint{208, 336} &&
		state.frame.player.current == before_current &&
		state.frame.option_left_on_page == before_options &&
		state.player_volley.type == PlayerShotType::b &&
		same_speeds(state.player_speeds, {4, 4, 3, 3}) &&
		state.player_hit.invincibility_time == 200U &&
		state.boss.target == ScreenPoint{-1, -1} &&
		state.enemy_target_projection == before_enemy_projection &&
		state.bullets.bullets[0U].flag == enemy_bullet_remove &&
		state.bullets.bullets[0U].screen_topleft_on_page[1U] ==
			ShotSubpixelPoint{160, 320} &&
		state.bullets.bullets[1U].flag == enemy_bullet_free &&
		state.bomb_animation.palette_tone == 100U &&
		state.tiles.copy_lines_top == 0U && state.tiles.copy_lines_h == 16U &&
		state.tile_mode == 0xA5U && state.tiles_egc_render_all &&
		state.stage_frame == before_stage_frame &&
		state.resident_frame == before_resident_frame && state.scroll == before_scroll;
}

[[nodiscard]] bool test_invalid_bullet_page_keeps_reached_prefix() noexcept
{
	auto state = warm_state();
	state.frame.page_front = 0U;
	state.frame.page_back = 2U;
	state.shottype = 0xFFU;
	state.player_volley.type = PlayerShotType::c;
	const auto before_speeds = state.player_speeds;
	const auto before_bullets = state.bullets;
	Recorder recorder{state};

	if(stage1_continue_resume(state, recorder) !=
		StageContinueResult::bullet_invalid_page || recorder.count != 4U ||
		recorder.events[0U] != Event{EventKind::hud} ||
		recorder.events[1U] != Event{EventKind::leave} ||
		recorder.events[2U] != Event{EventKind::tiles, 0U} ||
		recorder.events[3U] != Event{EventKind::tiles, 2U}) {
		return false;
	}
	return state.frame.player.on_page[0U] == ScreenPoint{208, 336} &&
		state.frame.player.on_page[1U] == ScreenPoint{208, 336} &&
		state.player_hit.invincibility_time == 200U &&
		state.boss.target == ScreenPoint{-1, -1} &&
		state.player_volley.type == PlayerShotType::c &&
		same_speeds(state.player_speeds, before_speeds) &&
		state.bullets == before_bullets &&
		state.bomb_animation.palette_tone == 27U;
}

[[nodiscard]] bool test_same_page_is_not_normalized() noexcept
{
	auto state = warm_state();
	state.frame.page_front = 1U;
	state.frame.page_back = 1U;
	state.bullets.bullets[1U].flag = enemy_bullet_free;
	const auto old_back = state.bullets.bullets[0U].screen_topleft_on_page[1U];
	Recorder recorder{state};

	return stage1_continue_resume(state, recorder) ==
		StageContinueResult::completed && recorder.count == 7U &&
		recorder.events[2U] == Event{EventKind::tiles, 1U} &&
		recorder.events[3U] == Event{EventKind::tiles, 1U} &&
		state.frame.page_front == 1U && state.frame.page_back == 1U &&
		state.bullets.bullets[0U].flag == enemy_bullet_remove &&
		state.bullets.bullets[0U].screen_topleft_on_page[1U] == old_back;
}

} // namespace

int main()
{
	if(!test_complete_sequence_uses_real_reset_and_clear() ||
		!test_invalid_bullet_page_keeps_reached_prefix() ||
		!test_same_page_is_not_normalized()) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
