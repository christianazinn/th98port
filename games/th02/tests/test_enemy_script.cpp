#include "th02/core/enemy_script.hpp"

#include "pc98/masterlib_vector.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <utility>

namespace {

using namespace th02::core;

[[nodiscard]] Enemy patterned_enemy() noexcept
{
	Enemy enemy{};
	enemy.pos_on_page = {{{40, 50}, {100, 100}}};
	enemy.script_ip = 0;
	enemy.age = 0;
	enemy.template_id = 9;
	enemy.flag = enemy_alive;
	enemy.anim_frame = 8U;
	enemy.in_kill_anim = 0U;
	enemy.patnum_delta = 7;
	enemy.render_as = 1;
	enemy.angle = 0;
	enemy.spawned_in_left_half = 1;
	enemy.loop_i = 0;
	enemy.velocity_x = 3;
	enemy.velocity_y = -4;
	enemy.despawn_when_offscreen_vertically = 0U;
	enemy.damage = 6;
	enemy.not_shootable = 0U;
	enemy.no_player_collision = 0U;
	enemy.pellet_group = enemy_bullet_group::one;
	enemy.pellet_speed = 32U;
	return enemy;
}

[[nodiscard]] EnemyTemplate patterned_template() noexcept
{
	EnemyTemplate enemy_template{};
	enemy_template.bullet_origin_x = 12;
	enemy_template.bullet_origin_y = 4;
	return enemy_template;
}

[[nodiscard]] PlayerFrameState patterned_frame() noexcept
{
	PlayerFrameState frame{};
	frame.page_front = 0U;
	frame.page_back = 1U;
	frame.player.current = {124, 116};
	return frame;
}

[[nodiscard]] RandomState patterned_random() noexcept
{
	RandomState random{};
	for(std::size_t i = 0U; i < random.ring.size(); ++i) {
		random.ring[i] = static_cast<std::uint8_t>(i);
	}
	return random;
}

[[nodiscard]] std::int8_t narrow_signed_byte(std::int16_t value) noexcept
{
	return std::bit_cast<std::int8_t>(
		static_cast<std::uint8_t>(std::bit_cast<std::uint16_t>(value))
	);
}

struct RecordingCallbacks final : EnemyScriptCallbacks {
	const Enemy *enemy{};
	std::array<std::uint8_t, 4U> sounds{};
	std::size_t sound_count{};
	std::int16_t tile_left{};
	std::int16_t tile_vram_y{};
	std::int16_t tile_image{};
	std::size_t tile_count{};
	bool kill_sound_precedes_kill_flag{};

	explicit RecordingCallbacks(const Enemy *enemy_arg) noexcept : enemy(enemy_arg)
	{
	}

	void sound_effect(std::uint8_t sound_id) noexcept override
	{
		sounds[sound_count++] = sound_id;
		if(sound_id == 3U && enemy) {
			kill_sound_precedes_kill_flag =
				enemy->age == 0 && enemy->in_kill_anim == 0U;
		}
	}

	void tile_ring_set_and_put_both_8(
		std::int16_t left, std::int16_t vram_y, std::int16_t image
	) noexcept override
	{
		tile_left = left;
		tile_vram_y = vram_y;
		tile_image = image;
		++tile_count;
	}
};

[[nodiscard]] EnemyRunContext context_for(
	EnemyBulletPoolState &bullets, RandomState &random,
	const PlayerFrameState &frame, std::uint8_t scroll_delta = 0U,
	std::int16_t scroll_line = 0
) noexcept
{
	return {bullets, random, frame, 0, 1, false, scroll_delta, scroll_line};
}

[[nodiscard]] bool test_wait_and_direct_motion_opcodes() noexcept
{
	EnemyBulletPoolState bullets{};
	auto random = patterned_random();
	const auto frame = patterned_frame();
	auto context = context_for(bullets, random, frame);
	auto enemy = patterned_enemy();
	RecordingCallbacks callbacks{&enemy};

	const std::array<std::uint8_t, 2U> wait_script{{0U, 1U}};
	if(enemy_run(enemy, patterned_template(), wait_script, context, callbacks) !=
		EnemyRunResult::frame_consumed || enemy.script_ip != 2 || enemy.age != 0) {
		return false;
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 4U> velocity_script{{1U, 1U, 0xFFU, 2U}};
	if(enemy_run(enemy, patterned_template(), velocity_script, context, callbacks) !=
		EnemyRunResult::frame_consumed || enemy.pos_on_page[1U] !=
		ScreenPoint{99, 102} || enemy.velocity_x != -1 || enemy.velocity_y != 2 ||
		enemy.script_ip != 4 || enemy.age != 0) {
		return false;
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 2U> move_script{{10U, 1U}};
	if(enemy_run(enemy, patterned_template(), move_script, context, callbacks) !=
		EnemyRunResult::frame_consumed || enemy.pos_on_page[1U] !=
		ScreenPoint{103, 96} || enemy.script_ip != 2 || enemy.age != 0) {
		return false;
	}

	enemy = patterned_enemy();
	enemy.spawned_in_left_half = 0;
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 4U> mirrored_script{{8U, 1U, 0xFFU, 3U}};
	if(enemy_run(enemy, patterned_template(), mirrored_script, context, callbacks) !=
		EnemyRunResult::frame_consumed || enemy.velocity_x != 1 ||
		enemy.velocity_y != 3 || enemy.pos_on_page[1U] != ScreenPoint{101, 103} ||
		enemy.script_ip != 4) {
		return false;
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	auto scroll_context = context_for(bullets, random, frame, 255U);
	const std::array<std::uint8_t, 2U> scroll_script{{2U, 1U}};
	return enemy_run(enemy, patterned_template(), scroll_script, scroll_context,
		callbacks) == EnemyRunResult::frame_consumed &&
		enemy.pos_on_page[1U] == ScreenPoint{100, 355} && enemy.script_ip == 2;
}

[[nodiscard]] bool test_random_and_absolute_motion_opcodes() noexcept
{
	EnemyBulletPoolState bullets{};
	auto random = patterned_random();
	const auto frame = patterned_frame();
	auto context = context_for(bullets, random, frame);
	auto enemy = patterned_enemy();
	RecordingCallbacks callbacks{&enemy};

	const std::array<std::uint8_t, 2U> random_velocity{{170U, 255U}};
	if(enemy_run(enemy, patterned_template(), random_velocity, context, callbacks) !=
		EnemyRunResult::continue_same_frame || random.cursor != 1U ||
		enemy.velocity_x != -1 || enemy.velocity_y != 0 || enemy.script_ip != 2) {
		return false;
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 3U> absolute{{4U, 5U, 6U}};
	if(enemy_run(enemy, patterned_template(), absolute, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.pos_on_page[1U] !=
		ScreenPoint{40, 48} || enemy.script_ip != 3) {
		return false;
	}

	random = patterned_random();
	random.ring[0U] = 5U;
	random.ring[1U] = 0U;
	auto random_x_context = context_for(bullets, random, frame);
	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 4U> random_x{{5U, 3U, 2U, 4U}};
	if(enemy_run(enemy, patterned_template(), random_x, random_x_context, callbacks) !=
		EnemyRunResult::continue_same_frame || random.cursor != 1U ||
		enemy.pos_on_page[1U] != ScreenPoint{29, 32} ||
		enemy.spawned_in_left_half != 1 || enemy.script_ip != 4) {
		return false;
	}

	random = patterned_random();
	random.ring[0U] = 9U;
	random.ring[1U] = 0U;
	auto random_y_context = context_for(bullets, random, frame);
	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 4U> random_y{{6U, 4U, 2U, 3U}};
	const auto result = enemy_run(enemy, patterned_template(), random_y, random_y_context,
		callbacks);
	if(result != EnemyRunResult::continue_same_frame || random.cursor != 1U ||
		enemy.pos_on_page[1U] != ScreenPoint{24, 41} || enemy.script_ip != 4) {
		return false;
	}
	return true;
}

[[nodiscard]] bool test_aimed_stepped_and_spin_motion_opcodes() noexcept
{
	EnemyBulletPoolState bullets{};
	auto random = patterned_random();
	auto frame = patterned_frame();
	frame.player.current = {100, 100};
	auto context = context_for(bullets, random, frame);
	auto enemy = patterned_enemy();
	enemy.pos_on_page[1U] = {123, 116};
	RecordingCallbacks callbacks{&enemy};
	const std::array<std::uint8_t, 3U> aimed{{9U, 1U, 255U}};
	if(enemy_run(enemy, patterned_template(), aimed, context, callbacks) !=
		EnemyRunResult::frame_consumed || enemy.pos_on_page[1U] !=
		ScreenPoint{378, 116} || enemy.velocity_x != -1 ||
		enemy.script_ip != 3) {
		return false;
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 6U> stepped_a{{3U, 1U, 0U, 10U, 1U, 1U}};
	if(enemy_run(enemy, patterned_template(), stepped_a, context, callbacks) !=
		EnemyRunResult::frame_consumed || enemy.angle != -1 ||
		enemy.script_ip != 6) {
		return false;
	}

	enemy = patterned_enemy();
	enemy.spawned_in_left_half = 1;
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 5U> stepped_b{{7U, 1U, 10U, 1U, 1U}};
	if(enemy_run(enemy, patterned_template(), stepped_b, context, callbacks) !=
		EnemyRunResult::frame_consumed || enemy.angle != -1 ||
		enemy.script_ip != 5) {
		return false;
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 6U> spin{{11U, 1U, 0U, 10U, 2U, 255U}};
	const auto old_angle_vector = pc98::masterlib::vector2(10U, 255);
	return enemy_run(enemy, patterned_template(), spin, context, callbacks) ==
		EnemyRunResult::frame_consumed && enemy.angle == -2 &&
		enemy.script_ip == 6 && enemy.pos_on_page[1U] == ScreenPoint{
			static_cast<std::int16_t>(100 + old_angle_vector.x),
			static_cast<std::int16_t>(100 + old_angle_vector.y),
		} && enemy.velocity_x == narrow_signed_byte(old_angle_vector.x) &&
		enemy.velocity_y == narrow_signed_byte(old_angle_vector.y);
}

[[nodiscard]] bool test_finish_and_concrete_bullet_opcodes() noexcept
{
	const auto frame = patterned_frame();
	EnemyBulletPoolState bullets{};
	auto random = patterned_random();
	auto context = context_for(bullets, random, frame, 0, 390);
	auto enemy = patterned_enemy();
	RecordingCallbacks callbacks{&enemy};

	const std::array<std::uint8_t, 1U> remove{{16U}};
	if(enemy_run(enemy, patterned_template(), remove, context, callbacks) !=
		EnemyRunResult::finished || enemy.flag != enemy_remove) {
		return false;
	}

	enemy = patterned_enemy();
	enemy.age = 8;
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 1U> kill{{17U}};
	if(enemy_run(enemy, patterned_template(), kill, context, callbacks) !=
		EnemyRunResult::finished || enemy.age != 0 || enemy.in_kill_anim != 1U ||
		callbacks.sound_count != 1U || callbacks.sounds[0U] != 3U ||
		!callbacks.kill_sound_precedes_kill_flag) {
		return false;
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 1U> pellet_zero{{32U}};
	if(enemy_run(enemy, patterned_template(), pellet_zero, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.script_ip != 1 ||
		bullets.bullets[0U].flag != enemy_bullet_alive ||
		bullets.bullets[0U].angle != 0U) {
		return false;
	}

	bullets = {};
	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 2U> pellet_angle{{33U, 44U}};
	if(enemy_run(enemy, patterned_template(), pellet_angle, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.script_ip != 2 ||
		bullets.bullets[0U].angle != 44U) {
		return false;
	}

	bullets = {};
	enemy = patterned_enemy();
	enemy.angle = 250;
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 2U> pellet_current_angle{{34U, 8U}};
	if(enemy_run(enemy, patterned_template(), pellet_current_angle, context,
		callbacks) != EnemyRunResult::continue_same_frame || enemy.angle != 258 ||
		bullets.bullets[0U].angle != 250U || enemy.script_ip != 2) {
		return false;
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 3U> pellet_setup{{37U, 7U, 55U}};
	if(enemy_run(enemy, patterned_template(), pellet_setup, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.pellet_group != 7U ||
		enemy.pellet_speed != 55U || enemy.script_ip != 3) {
		return false;
	}

	// 37 changes the persistent pellet fields; a later frame consumes them in
	// 33, then the following wait consumes the frame without externally moving IP.
	bullets = {};
	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 7U> pellet_sequence{{
		37U, enemy_bullet_group::one, 32U, 33U, 44U, 0U, 1U,
	}};
	if(enemy_run(enemy, patterned_template(), pellet_sequence, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.script_ip != 3 ||
		enemy.pellet_group != enemy_bullet_group::one || enemy.pellet_speed != 32U ||
		enemy.age != 0) {
		return false;
	}
	if(enemy_run(enemy, patterned_template(), pellet_sequence, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.script_ip != 5 ||
		bullets.bullets[0U].flag != enemy_bullet_alive ||
		bullets.bullets[0U].angle != 44U || enemy.age != 0) {
		return false;
	}
	if(enemy_run(enemy, patterned_template(), pellet_sequence, context, callbacks) !=
		EnemyRunResult::frame_consumed || enemy.script_ip != 7 || enemy.age != 0) {
		return false;
	}

	bullets = {};
	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 3U> bullet16{{36U, 6U, 77U}};
	if(enemy_run(enemy, patterned_template(), bullet16, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.script_ip != 3 ||
		bullets.bullets[0U].size_type != enemy_bullet_16x16 ||
		bullets.bullets[0U].patnum != 77U || bullets.bullets[0U].angle != 6U) {
		return false;
	}

	bullets = {};
	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 5U> scrolled_pellet{{38U, 5U, 7U, 0x20U, 32U}};
	if(enemy_run(enemy, patterned_template(), scrolled_pellet, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.script_ip != 5 ||
		bullets.bullets[0U].screen_topleft_on_page[1U].y != 240) {
		return false;
	}

	bullets = {};
	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 6U> scrolled_bullet16{{39U, 6U, 8U, 0x20U, 9U, 32U}};
	const auto scrolled16_result = enemy_run(enemy, patterned_template(),
		scrolled_bullet16, context, callbacks);
	if(scrolled16_result != EnemyRunResult::continue_same_frame ||
		enemy.script_ip != 6 || bullets.bullets[0U].size_type != enemy_bullet_16x16 ||
		bullets.bullets[0U].patnum != 9U ||
		bullets.bullets[0U].screen_topleft_on_page[1U].y != 256) {
		return false;
	}
	return true;
}

[[nodiscard]] bool test_script_flow_flags_and_presentation_opcodes() noexcept
{
	const auto frame = patterned_frame();
	EnemyBulletPoolState bullets{};
	auto random = patterned_random();
	auto context = context_for(bullets, random, frame);
	auto enemy = patterned_enemy();
	RecordingCallbacks callbacks{&enemy};

	const std::array<std::uint8_t, 2U> jump{{160U, 44U}};
	if(enemy_run(enemy, patterned_template(), jump, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.script_ip != 44) return false;

	enemy = patterned_enemy();
	enemy.angle = 0;
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 3U> angle_loop{{161U, 22U, 1U}};
	if(enemy_run(enemy, patterned_template(), angle_loop, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.angle != 1 || enemy.script_ip != 22) {
		return false;
	}
	enemy.script_ip = 0;
	if(enemy_run(enemy, patterned_template(), angle_loop, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.angle != 2 || enemy.script_ip != 3) {
		return false;
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 3U> loop_i{{167U, 24U, 1U}};
	if(enemy_run(enemy, patterned_template(), loop_i, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.loop_i != 1 || enemy.script_ip != 24) {
		return false;
	}
	enemy.script_ip = 0;
	if(enemy_run(enemy, patterned_template(), loop_i, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.loop_i != 0 || enemy.script_ip != 3) {
		return false;
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 2U> set_angle{{162U, 99U}};
	if(enemy_run(enemy, patterned_template(), set_angle, context, callbacks) !=
		EnemyRunResult::continue_same_frame || enemy.angle != 99 || enemy.script_ip != 2) {
		return false;
	}

	for(const auto &[opcode, expected_render] : std::array<std::pair<
		std::uint8_t, std::int16_t
	>, 2U>{{
		{std::uint8_t{163U}, std::int16_t{0}},
		{std::uint8_t{164U}, std::int16_t{1}},
	}}) {
		enemy = patterned_enemy();
		callbacks.enemy = &enemy;
		const std::array<std::uint8_t, 1U> script{{opcode}};
		if(enemy_run(enemy, patterned_template(), script, context, callbacks) !=
			EnemyRunResult::continue_same_frame || enemy.render_as != expected_render ||
			enemy.script_ip != 1) {
			return false;
		}
	}

	for(const auto &[opcode, expected_render] : std::array<std::pair<
		std::uint8_t, std::int16_t
	>, 2U>{{
		{std::uint8_t{165U}, std::int16_t{0}},
		{std::uint8_t{166U}, std::int16_t{2}},
	}}) {
		enemy = patterned_enemy();
		callbacks.enemy = &enemy;
		const std::array<std::uint8_t, 2U> script{{opcode, 77U}};
		if(enemy_run(enemy, patterned_template(), script, context, callbacks) !=
			EnemyRunResult::continue_same_frame || enemy.patnum_delta != 77 ||
			enemy.render_as != expected_render || enemy.script_ip != 2) {
			return false;
		}
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 1U> vertical_despawn{{169U}};
	if(enemy_run(enemy, patterned_template(), vertical_despawn, context, callbacks) !=
		EnemyRunResult::continue_same_frame ||
		enemy.despawn_when_offscreen_vertically != 1U || enemy.script_ip != 1) {
		return false;
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 2U> sound{{168U, 13U}};
	if(enemy_run(enemy, patterned_template(), sound, context, callbacks) !=
		EnemyRunResult::continue_same_frame || callbacks.sounds[0U] != 13U ||
		enemy.script_ip != 2) {
		return false;
	}

	for(const auto &[opcode, expected_shootable, expected_collision] :
		std::array<std::tuple<std::uint8_t, std::uint8_t, std::uint8_t>, 4U>{{
			{std::uint8_t{171U}, std::uint8_t{1U}, std::uint8_t{0U}},
			{std::uint8_t{172U}, std::uint8_t{0U}, std::uint8_t{0U}},
			{std::uint8_t{173U}, std::uint8_t{0U}, std::uint8_t{1U}},
			{std::uint8_t{174U}, std::uint8_t{0U}, std::uint8_t{0U}},
		}}) {
		enemy = patterned_enemy();
		if(opcode == 172U) {
			enemy.not_shootable = 1U;
		}
		if(opcode == 174U) {
			enemy.no_player_collision = 1U;
		}
		callbacks.enemy = &enemy;
		const std::array<std::uint8_t, 1U> script{{opcode}};
		if(enemy_run(enemy, patterned_template(), script, context, callbacks) !=
			EnemyRunResult::continue_same_frame ||
			enemy.not_shootable != expected_shootable ||
			enemy.no_player_collision != expected_collision || enemy.script_ip != 1) {
			return false;
		}
	}

	enemy = patterned_enemy();
	callbacks.enemy = &enemy;
	const std::array<std::uint8_t, 3U> tile{{175U, 80U, 61U}};
	return enemy_run(enemy, patterned_template(), tile, context, callbacks) ==
		EnemyRunResult::continue_same_frame && callbacks.tile_count == 1U &&
		callbacks.tile_left == 100 && callbacks.tile_vram_y == 80 &&
		callbacks.tile_image == 61 && enemy.script_ip == 3;
}

[[nodiscard]] bool test_refusal_prefixes_and_ip_domains() noexcept
{
	auto frame = patterned_frame();
	frame.player.current = {100, 100};
	EnemyBulletPoolState bullets{};
	auto random = patterned_random();
	auto context = context_for(bullets, random, frame);
	auto enemy = patterned_enemy();
	RecordingCallbacks callbacks{&enemy};

	const std::array<std::uint8_t, 1U> unknown{{0xFEU}};
	const auto unknown_before = enemy;
	if(enemy_run(enemy, patterned_template(), unknown, context, callbacks) !=
		EnemyRunResult::unsupported_opcode || enemy != unknown_before) {
		return false;
	}

	const std::array<std::uint8_t, 1U> missing_wait{{0U}};
	enemy = patterned_enemy();
	enemy.age = 7;
	if(enemy_run(enemy, patterned_template(), missing_wait, context, callbacks) !=
		EnemyRunResult::invalid_script_operand || enemy.age != 8 ||
		enemy.script_ip != 0) {
		return false;
	}

	const std::array<std::uint8_t, 2U> age_wrap_wait{{0U, 255U}};
	enemy = patterned_enemy();
	enemy.age = 32767;
	if(enemy_run(enemy, patterned_template(), age_wrap_wait, context, callbacks) !=
		EnemyRunResult::frame_consumed || enemy.age != -32768 || enemy.script_ip != 0) {
		return false;
	}

	const std::array<std::uint8_t, 3U> missing_velocity_y{{1U, 1U, 0xFEU}};
	enemy = patterned_enemy();
	if(enemy_run(enemy, patterned_template(), missing_velocity_y, context,
		callbacks) != EnemyRunResult::invalid_script_operand ||
		enemy.velocity_x != -2 || enemy.velocity_y != -4 || enemy.age != 0) {
		return false;
	}

	const std::array<std::uint8_t, 3U> zero_random_range{{5U, 0U, 0U}};
	random = patterned_random();
	auto zero_range_context = context_for(bullets, random, frame);
	enemy = patterned_enemy();
	if(enemy_run(enemy, patterned_template(), zero_random_range, zero_range_context,
		callbacks) != EnemyRunResult::source_divide_fault || random.cursor != 1U ||
		enemy.pos_on_page[1U] != ScreenPoint{100, 100}) {
		return false;
	}

	// Dispatcher opcode reads narrow the stored word IP. The aimed helper's
	// length read stays raw-word-based, so its +255 vector comes from [258].
	std::array<std::uint8_t, 259U> mixed_ip{};
	mixed_ip[0U] = 9U;
	mixed_ip[1U] = 1U;
	mixed_ip[2U] = 1U;
	mixed_ip[258U] = 255U;
	enemy = patterned_enemy();
	enemy.script_ip = 256;
	enemy.pos_on_page[1U] = {123, 116};
	if(enemy_run(enemy, patterned_template(), mixed_ip, context, callbacks) !=
		EnemyRunResult::frame_consumed || enemy.pos_on_page[1U] !=
		ScreenPoint{378, 116} || enemy.script_ip != 259) {
		return false;
	}

	enemy = patterned_enemy();
	enemy.script_ip = 0;
	enemy.age = 0;
	enemy.angle = 5;
	const std::array<std::uint8_t, 5U> spin_prefix{{11U, 1U, 0U, 2U, 3U}};
	return enemy_run(enemy, patterned_template(), spin_prefix, context,
		callbacks) == EnemyRunResult::invalid_script_operand &&
		enemy.angle == 2 && enemy.pos_on_page[1U] == ScreenPoint{100, 100};
}

} // namespace

int main()
{
	if(!test_wait_and_direct_motion_opcodes()) {
		std::fputs("FAIL enemy script wait/direct motion\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_random_and_absolute_motion_opcodes()) {
		std::fputs("FAIL enemy script random/absolute motion\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_aimed_stepped_and_spin_motion_opcodes()) {
		std::fputs("FAIL enemy script aimed/step/spin motion\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_finish_and_concrete_bullet_opcodes()) {
		std::fputs("FAIL enemy script finish/bullet opcodes\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_script_flow_flags_and_presentation_opcodes()) {
		std::fputs("FAIL enemy script flow/flag/presentation opcodes\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_refusal_prefixes_and_ip_domains()) {
		std::fputs("FAIL enemy script refusal/IP domains\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
