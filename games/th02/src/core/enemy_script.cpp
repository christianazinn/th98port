#include "th02/core/enemy_script.hpp"

#include "pc98/masterlib_vector.hpp"

#include <bit>
#include <cstddef>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_height = 400;
constexpr std::int16_t enemy_spawn_left_half_boundary = 208;

[[nodiscard]] std::int16_t add_word(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t subtract_word(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) -
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int8_t narrow_signed_byte(std::int16_t value) noexcept
{
	return std::bit_cast<std::int8_t>(
		static_cast<std::uint8_t>(std::bit_cast<std::uint16_t>(value))
	);
}

[[nodiscard]] bool dispatch_byte_at(
	std::span<const std::uint8_t> script, std::uint8_t ip,
	std::size_t relative_offset, std::uint8_t &value
) noexcept
{
	const auto offset = static_cast<std::size_t>(ip) + relative_offset;
	if(offset >= script.size()) return false;
	value = script[offset];
	return true;
}

[[nodiscard]] EnemyRunResult map_motion_result(EnemyMotionResult result) noexcept
{
	switch(result) {
	case EnemyMotionResult::completed:
		return EnemyRunResult::frame_consumed;
	case EnemyMotionResult::invalid_page:
		return EnemyRunResult::invalid_page;
	case EnemyMotionResult::invalid_script_operand:
		return EnemyRunResult::invalid_script_operand;
	case EnemyMotionResult::source_divide_fault:
		return EnemyRunResult::source_divide_fault;
	case EnemyMotionResult::source_iatan2_fault:
		return EnemyRunResult::source_iatan2_fault;
	}
	return EnemyRunResult::unsupported_opcode;
}

[[nodiscard]] EnemyRunResult map_bullet_result(
	EnemyBulletAddResult result
) noexcept
{
	switch(result) {
	case EnemyBulletAddResult::completed:
		return EnemyRunResult::frame_consumed;
	case EnemyBulletAddResult::invalid_page:
		return EnemyRunResult::invalid_page;
	case EnemyBulletAddResult::source_iatan2_fault:
		return EnemyRunResult::source_iatan2_fault;
	}
	return EnemyRunResult::unsupported_opcode;
}

[[nodiscard]] EnemyRunResult wait(
	Enemy &enemy, std::span<const std::uint8_t> script, std::uint8_t ip,
	std::uint8_t advance
) noexcept
{
	enemy.age = add_word(enemy.age, 1);
	std::uint8_t interval{};
	if(!dispatch_byte_at(script, ip, 1U, interval)) {
		return EnemyRunResult::invalid_script_operand;
	}
	if(static_cast<std::int16_t>(interval) <= enemy.age) {
		enemy.script_ip = add_word(enemy.script_ip,
			static_cast<std::int16_t>(advance));
		enemy.age = 0;
	}
	return EnemyRunResult::frame_consumed;
}

[[nodiscard]] EnemyRunResult advance_and_run(
	Enemy &enemy, std::uint8_t advance
) noexcept
{
	enemy.script_ip = add_word(enemy.script_ip,
		static_cast<std::int16_t>(advance));
	return EnemyRunResult::continue_same_frame;
}

[[nodiscard]] EnemyRunResult next(Enemy &enemy) noexcept
{
	enemy.script_ip = add_word(enemy.script_ip, 1);
	return EnemyRunResult::continue_same_frame;
}

[[nodiscard]] EnemyRunResult enemy_add_pellet(
	Enemy &enemy, const EnemyTemplate &enemy_template,
	EnemyRunContext &context, std::uint8_t angle
) noexcept
{
	if(context.player_frame.page_back >= enemy.pos_on_page.size()) {
		return EnemyRunResult::invalid_page;
	}
	const auto &position = enemy.pos_on_page[context.player_frame.page_back];
	return map_bullet_result(bullets_add_pellet(
		context.bullets, context.random, context.player_frame, context.playperf,
		context.rank, context.bombing,
		{add_word(enemy_template.bullet_origin_x, position.x),
			add_word(enemy_template.bullet_origin_y, position.y)},
		angle, enemy.pellet_group, static_cast<std::int16_t>(enemy.pellet_speed)
	));
}

[[nodiscard]] EnemyRunResult enemy_add_16x16(
	Enemy &enemy, const EnemyTemplate &enemy_template,
	EnemyRunContext &context, std::uint8_t angle, std::uint8_t group,
	std::int16_t patnum, std::int16_t speed
) noexcept
{
	if(context.player_frame.page_back >= enemy.pos_on_page.size()) {
		return EnemyRunResult::invalid_page;
	}
	const auto &position = enemy.pos_on_page[context.player_frame.page_back];
	return map_bullet_result(bullets_add_16x16(
		context.bullets, context.random, context.player_frame, context.playperf,
		context.rank, context.bombing,
		{add_word(enemy_template.bullet_origin_x, position.x),
			add_word(enemy_template.bullet_origin_y, position.y)},
		angle, group, patnum, speed
	));
}

[[nodiscard]] EnemyRunResult enemy_add_scrolled_pellet(
	Enemy &enemy, const EnemyTemplate &enemy_template,
	EnemyRunContext &context, std::uint8_t angle, std::uint8_t group,
	std::int16_t speed, std::int16_t top
) noexcept
{
	if(context.player_frame.page_back >= enemy.pos_on_page.size()) {
		return EnemyRunResult::invalid_page;
	}
	const auto &position = enemy.pos_on_page[context.player_frame.page_back];
	return map_bullet_result(bullets_add_pellet(
		context.bullets, context.random, context.player_frame, context.playperf,
		context.rank, context.bombing,
		{add_word(enemy_template.bullet_origin_x, position.x), top}, angle, group,
		speed
	));
}

[[nodiscard]] EnemyRunResult enemy_add_scrolled_16x16(
	Enemy &enemy, const EnemyTemplate &enemy_template,
	EnemyRunContext &context, std::uint8_t angle, std::uint8_t group,
	std::int16_t patnum, std::int16_t speed, std::int16_t top
) noexcept
{
	if(context.player_frame.page_back >= enemy.pos_on_page.size()) {
		return EnemyRunResult::invalid_page;
	}
	const auto &position = enemy.pos_on_page[context.player_frame.page_back];
	return map_bullet_result(bullets_add_16x16(
		context.bullets, context.random, context.player_frame, context.playperf,
		context.rank, context.bombing,
		{add_word(enemy_template.bullet_origin_x, position.x), top}, angle, group,
		patnum, speed
	));
}

[[nodiscard]] std::int16_t scrolled_top(
	std::uint8_t script_top, std::int16_t scroll_line
) noexcept
{
	auto top = subtract_word(static_cast<std::int16_t>(script_top), scroll_line);
	if(top < 0) top = add_word(top, playfield_height);
	return top;
}

} // namespace

EnemyRunResult enemy_run(Enemy &enemy, const EnemyTemplate &enemy_template,
	std::span<const std::uint8_t> script, EnemyRunContext &context,
	EnemyScriptCallbacks &callbacks
) noexcept
{
	const auto ip = static_cast<std::uint8_t>(enemy.script_ip);
	std::uint8_t opcode{};
	if(!dispatch_byte_at(script, ip, 0U, opcode)) {
		return EnemyRunResult::invalid_script_operand;
	}

	switch(opcode) {
	case 0U:
		return wait(enemy, script, ip, 2U);

	case 1U: {
		std::uint8_t velocity_x{};
		if(!dispatch_byte_at(script, ip, 2U, velocity_x)) {
			return EnemyRunResult::invalid_script_operand;
		}
		enemy.velocity_x = narrow_signed_byte(static_cast<std::int16_t>(velocity_x));
		std::uint8_t velocity_y{};
		if(!dispatch_byte_at(script, ip, 3U, velocity_y)) {
			return EnemyRunResult::invalid_script_operand;
		}
		enemy.velocity_y = narrow_signed_byte(static_cast<std::int16_t>(velocity_y));
		const auto moved = enemy_pos_update(enemy, context.player_frame.page_back);
		if(moved != EnemyMotionResult::completed) return map_motion_result(moved);
		return wait(enemy, script, ip, 4U);
	}

	case 10U: {
		const auto moved = enemy_pos_update(enemy, context.player_frame.page_back);
		if(moved != EnemyMotionResult::completed) return map_motion_result(moved);
		return wait(enemy, script, ip, 2U);
	}

	case 170U: {
		const auto angle = randring_next8(context.random);
		std::uint8_t speed{};
		if(!dispatch_byte_at(script, ip, 1U, speed)) {
			return EnemyRunResult::invalid_script_operand;
		}
		const auto source_speed = static_cast<std::int16_t>(speed);
		enemy.velocity_x = narrow_signed_byte(pc98::masterlib::vector_component(
			source_speed, pc98::masterlib::cosine8(angle)
		));
		enemy.velocity_y = narrow_signed_byte(pc98::masterlib::vector_component(
			source_speed, pc98::masterlib::sine8(angle)
		));
		return advance_and_run(enemy, 2U);
	}

	case 8U: {
		const auto sign = (enemy.spawned_in_left_half == 0) ? -1 : 1;
		std::uint8_t velocity_x{};
		if(!dispatch_byte_at(script, ip, 2U, velocity_x)) {
			return EnemyRunResult::invalid_script_operand;
		}
		enemy.velocity_x = narrow_signed_byte(static_cast<std::int16_t>(
			static_cast<std::int16_t>(velocity_x) * sign
		));
		std::uint8_t velocity_y{};
		if(!dispatch_byte_at(script, ip, 3U, velocity_y)) {
			return EnemyRunResult::invalid_script_operand;
		}
		enemy.velocity_y = narrow_signed_byte(static_cast<std::int16_t>(velocity_y));
		const auto moved = enemy_pos_update(enemy, context.player_frame.page_back);
		if(moved != EnemyMotionResult::completed) return map_motion_result(moved);
		return wait(enemy, script, ip, 4U);
	}

	case 9U: {
		const auto moved = enemy_pos_update_aimed(
			enemy, context.player_frame, script, context.player_frame.page_back
		);
		if(moved != EnemyMotionResult::completed) return map_motion_result(moved);
		return wait(enemy, script, ip, 3U);
	}

	case 2U: {
		if(context.player_frame.page_back >= enemy.pos_on_page.size()) {
			return EnemyRunResult::invalid_page;
		}
		auto &position = enemy.pos_on_page[context.player_frame.page_back];
		position.y = add_word(position.y, context.scroll_delta);
		return wait(enemy, script, ip, 2U);
	}

	case 3U: {
		const auto raw_offset = add_word(enemy.script_ip, 2);
		if(raw_offset < 0 || static_cast<std::size_t>(raw_offset) >= script.size()) {
			return EnemyRunResult::invalid_script_operand;
		}
		const std::int16_t sign =
			(script[static_cast<std::size_t>(raw_offset)] == 0U) ? -1 : 1;
		const auto moved = enemy_angle_step_and_move(
			enemy, script, 3, sign, context.player_frame.page_back
		);
		if(moved != EnemyMotionResult::completed) return map_motion_result(moved);
		return wait(enemy, script, ip, 6U);
	}

	case 7U: {
		const std::int16_t sign =
			(enemy.spawned_in_left_half == 1) ? -1 : 1;
		const auto moved = enemy_angle_step_and_move(
			enemy, script, 2, sign, context.player_frame.page_back
		);
		if(moved != EnemyMotionResult::completed) return map_motion_result(moved);
		return wait(enemy, script, ip, 5U);
	}

	case 11U: {
		const auto moved = enemy_angle_spin_and_move(
			enemy, script, context.player_frame.page_back
		);
		if(moved != EnemyMotionResult::completed) return map_motion_result(moved);
		return wait(enemy, script, ip, 6U);
	}

	case 4U: {
		if(context.player_frame.page_back >= enemy.pos_on_page.size()) {
			return EnemyRunResult::invalid_page;
		}
		std::uint8_t left{};
		if(!dispatch_byte_at(script, ip, 1U, left)) {
			return EnemyRunResult::invalid_script_operand;
		}
		auto &position = enemy.pos_on_page[context.player_frame.page_back];
		position.x = static_cast<std::int16_t>(left << 3U);
		std::uint8_t top{};
		if(!dispatch_byte_at(script, ip, 2U, top)) {
			return EnemyRunResult::invalid_script_operand;
		}
		position.y = static_cast<std::int16_t>(top << 3U);
		return advance_and_run(enemy, 3U);
	}

	case 5U: {
		if(context.player_frame.page_back >= enemy.pos_on_page.size()) {
			return EnemyRunResult::invalid_page;
		}
		const auto random = randring_next16(context.random);
		std::uint8_t range{};
		if(!dispatch_byte_at(script, ip, 2U, range)) {
			return EnemyRunResult::invalid_script_operand;
		}
		const auto divisor = static_cast<std::uint16_t>(range) << 3U;
		if(divisor == 0U) return EnemyRunResult::source_divide_fault;
		std::uint8_t left_base{};
		if(!dispatch_byte_at(script, ip, 1U, left_base)) {
			return EnemyRunResult::invalid_script_operand;
		}
		auto &position = enemy.pos_on_page[context.player_frame.page_back];
		position.x = add_word(
			static_cast<std::int16_t>(random % divisor),
			static_cast<std::int16_t>(left_base << 3U)
		);
		std::uint8_t top{};
		if(!dispatch_byte_at(script, ip, 3U, top)) {
			return EnemyRunResult::invalid_script_operand;
		}
		position.y = static_cast<std::int16_t>(top << 3U);
		enemy.spawned_in_left_half =
			(position.x < enemy_spawn_left_half_boundary) ? 1 : 0;
		return advance_and_run(enemy, 4U);
	}

	case 6U: {
		if(context.player_frame.page_back >= enemy.pos_on_page.size()) {
			return EnemyRunResult::invalid_page;
		}
		const auto random = randring_next16(context.random);
		std::uint8_t range{};
		if(!dispatch_byte_at(script, ip, 2U, range)) {
			return EnemyRunResult::invalid_script_operand;
		}
		const auto divisor = static_cast<std::uint16_t>(range) << 3U;
		if(divisor == 0U) return EnemyRunResult::source_divide_fault;
		std::uint8_t top_base{};
		if(!dispatch_byte_at(script, ip, 1U, top_base)) {
			return EnemyRunResult::invalid_script_operand;
		}
		auto &position = enemy.pos_on_page[context.player_frame.page_back];
		position.y = add_word(
			static_cast<std::int16_t>(random % divisor),
			static_cast<std::int16_t>(top_base << 3U)
		);
		std::uint8_t left{};
		if(!dispatch_byte_at(script, ip, 3U, left)) {
			return EnemyRunResult::invalid_script_operand;
		}
		position.x = static_cast<std::int16_t>(left << 3U);
		return advance_and_run(enemy, 4U);
	}

	case 16U:
		enemy.flag = enemy_remove;
		return EnemyRunResult::finished;

	case 17U:
		enemy.age = 0;
		callbacks.sound_effect(3U);
		enemy.in_kill_anim = 1U;
		return EnemyRunResult::finished;

	case 32U: {
		const auto result = enemy_add_pellet(enemy, enemy_template, context, 0U);
		if(result != EnemyRunResult::frame_consumed) return result;
		return next(enemy);
	}

	case 33U: {
		std::uint8_t angle{};
		if(!dispatch_byte_at(script, ip, 1U, angle)) {
			return EnemyRunResult::invalid_script_operand;
		}
		const auto result = enemy_add_pellet(enemy, enemy_template, context, angle);
		if(result != EnemyRunResult::frame_consumed) return result;
		return advance_and_run(enemy, 2U);
	}

	case 34U: {
		const auto result = enemy_add_pellet(enemy, enemy_template, context,
			static_cast<std::uint8_t>(enemy.angle));
		if(result != EnemyRunResult::frame_consumed) return result;
		std::uint8_t delta{};
		if(!dispatch_byte_at(script, ip, 1U, delta)) {
			return EnemyRunResult::invalid_script_operand;
		}
		enemy.angle = add_word(enemy.angle, static_cast<std::int16_t>(delta));
		return advance_and_run(enemy, 2U);
	}

	case 37U: {
		std::uint8_t group{};
		if(!dispatch_byte_at(script, ip, 1U, group)) {
			return EnemyRunResult::invalid_script_operand;
		}
		enemy.pellet_group = group;
		std::uint8_t speed{};
		if(!dispatch_byte_at(script, ip, 2U, speed)) {
			return EnemyRunResult::invalid_script_operand;
		}
		enemy.pellet_speed = speed;
		return advance_and_run(enemy, 3U);
	}

	case 36U: {
		std::uint8_t angle{};
		if(!dispatch_byte_at(script, ip, 1U, angle)) {
			return EnemyRunResult::invalid_script_operand;
		}
		std::uint8_t patnum{};
		if(!dispatch_byte_at(script, ip, 2U, patnum)) {
			return EnemyRunResult::invalid_script_operand;
		}
		const auto result = enemy_add_16x16(enemy, enemy_template, context,
			angle, enemy.pellet_group, patnum,
			static_cast<std::int16_t>(enemy.pellet_speed));
		if(result != EnemyRunResult::frame_consumed) return result;
		return advance_and_run(enemy, 3U);
	}

	case 38U: {
		std::uint8_t script_top{};
		if(!dispatch_byte_at(script, ip, 1U, script_top)) {
			return EnemyRunResult::invalid_script_operand;
		}
		const auto top = scrolled_top(script_top, context.scroll_line);
		std::uint8_t angle{};
		if(!dispatch_byte_at(script, ip, 2U, angle)) {
			return EnemyRunResult::invalid_script_operand;
		}
		std::uint8_t group{};
		if(!dispatch_byte_at(script, ip, 3U, group)) {
			return EnemyRunResult::invalid_script_operand;
		}
		std::uint8_t speed{};
		if(!dispatch_byte_at(script, ip, 4U, speed)) {
			return EnemyRunResult::invalid_script_operand;
		}
		const auto result = enemy_add_scrolled_pellet(enemy, enemy_template,
			context, angle, group, static_cast<std::int16_t>(speed), top);
		if(result != EnemyRunResult::frame_consumed) return result;
		return advance_and_run(enemy, 5U);
	}

	case 39U: {
		std::uint8_t script_top{};
		if(!dispatch_byte_at(script, ip, 1U, script_top)) {
			return EnemyRunResult::invalid_script_operand;
		}
		const auto top = scrolled_top(script_top, context.scroll_line);
		std::uint8_t angle{};
		if(!dispatch_byte_at(script, ip, 2U, angle)) {
			return EnemyRunResult::invalid_script_operand;
		}
		std::uint8_t group{};
		if(!dispatch_byte_at(script, ip, 3U, group)) {
			return EnemyRunResult::invalid_script_operand;
		}
		std::uint8_t patnum{};
		if(!dispatch_byte_at(script, ip, 4U, patnum)) {
			return EnemyRunResult::invalid_script_operand;
		}
		std::uint8_t speed{};
		if(!dispatch_byte_at(script, ip, 5U, speed)) {
			return EnemyRunResult::invalid_script_operand;
		}
		const auto result = enemy_add_scrolled_16x16(enemy, enemy_template,
			context, angle, group, patnum, static_cast<std::int16_t>(speed), top);
		if(result != EnemyRunResult::frame_consumed) return result;
		return advance_and_run(enemy, 6U);
	}

	case 160U: {
		std::uint8_t target{};
		if(!dispatch_byte_at(script, ip, 1U, target)) {
			return EnemyRunResult::invalid_script_operand;
		}
		enemy.script_ip = static_cast<std::int16_t>(target);
		return EnemyRunResult::continue_same_frame;
	}

	case 161U: {
		const auto previous_angle = enemy.angle;
		enemy.angle = add_word(enemy.angle, 1);
		std::uint8_t limit{};
		if(!dispatch_byte_at(script, ip, 2U, limit)) {
			return EnemyRunResult::invalid_script_operand;
		}
		if(previous_angle < static_cast<std::int16_t>(limit)) {
			std::uint8_t target{};
			if(!dispatch_byte_at(script, ip, 1U, target)) {
				return EnemyRunResult::invalid_script_operand;
			}
			enemy.script_ip = static_cast<std::int16_t>(target);
		} else {
			enemy.script_ip = add_word(enemy.script_ip, 3);
		}
		return EnemyRunResult::continue_same_frame;
	}

	case 167U: {
		const auto previous_loop_i = enemy.loop_i;
		enemy.loop_i = add_word(enemy.loop_i, 1);
		std::uint8_t limit{};
		if(!dispatch_byte_at(script, ip, 2U, limit)) {
			return EnemyRunResult::invalid_script_operand;
		}
		if(previous_loop_i < static_cast<std::int16_t>(limit)) {
			std::uint8_t target{};
			if(!dispatch_byte_at(script, ip, 1U, target)) {
				return EnemyRunResult::invalid_script_operand;
			}
			enemy.script_ip = static_cast<std::int16_t>(target);
		} else {
			enemy.loop_i = 0;
			enemy.script_ip = add_word(enemy.script_ip, 3);
		}
		return EnemyRunResult::continue_same_frame;
	}

	case 162U: {
		std::uint8_t angle{};
		if(!dispatch_byte_at(script, ip, 1U, angle)) {
			return EnemyRunResult::invalid_script_operand;
		}
		enemy.angle = static_cast<std::int16_t>(angle);
		return advance_and_run(enemy, 2U);
	}

	case 163U:
		enemy.render_as = 0;
		return next(enemy);

	case 164U:
		enemy.render_as = 1;
		return next(enemy);

	case 165U: {
		std::uint8_t patnum_delta{};
		if(!dispatch_byte_at(script, ip, 1U, patnum_delta)) {
			return EnemyRunResult::invalid_script_operand;
		}
		enemy.patnum_delta = static_cast<std::int16_t>(patnum_delta);
		enemy.render_as = 0;
		return advance_and_run(enemy, 2U);
	}

	case 166U: {
		std::uint8_t patnum_delta{};
		if(!dispatch_byte_at(script, ip, 1U, patnum_delta)) {
			return EnemyRunResult::invalid_script_operand;
		}
		enemy.patnum_delta = static_cast<std::int16_t>(patnum_delta);
		enemy.render_as = 2;
		return advance_and_run(enemy, 2U);
	}

	case 169U:
		enemy.despawn_when_offscreen_vertically = 1U;
		return next(enemy);

	case 168U: {
		std::uint8_t sound_id{};
		if(!dispatch_byte_at(script, ip, 1U, sound_id)) {
			return EnemyRunResult::invalid_script_operand;
		}
		callbacks.sound_effect(sound_id);
		return advance_and_run(enemy, 2U);
	}

	case 171U:
		enemy.not_shootable = 1U;
		return next(enemy);

	case 172U:
		enemy.not_shootable = 0U;
		return next(enemy);

	case 173U:
		enemy.no_player_collision = 1U;
		return next(enemy);

	case 174U:
		enemy.no_player_collision = 0U;
		return next(enemy);

	case 175U: {
		std::uint8_t vram_y{};
		if(!dispatch_byte_at(script, ip, 1U, vram_y)) {
			return EnemyRunResult::invalid_script_operand;
		}
		std::uint8_t image{};
		if(!dispatch_byte_at(script, ip, 2U, image)) {
			return EnemyRunResult::invalid_script_operand;
		}
		if(context.player_frame.page_back >= enemy.pos_on_page.size()) {
			return EnemyRunResult::invalid_page;
		}
		callbacks.tile_ring_set_and_put_both_8(
			enemy.pos_on_page[context.player_frame.page_back].x,
			static_cast<std::int16_t>(vram_y), static_cast<std::int16_t>(image)
		);
		return advance_and_run(enemy, 3U);
	}

	default:
		return EnemyRunResult::unsupported_opcode;
	}
}

} // namespace th02::core
