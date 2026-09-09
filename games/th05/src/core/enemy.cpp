#include "th05/core/enemy.hpp"

#include "th05/core/item.hpp"
#include "th05/core/shot.hpp"
#include "th05/core/shot_collision.hpp"
#include "th05/core/state.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_random.hpp"
#include "pc98/th0405_sparks.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace th05::core {
namespace {

constexpr std::int16_t enemy_random_position = 15984;
constexpr std::uint16_t playfield_width = 6144U;
constexpr std::uint16_t playfield_height = 5888U;
constexpr std::uint16_t enemy_width = 512U;
constexpr std::uint16_t enemy_height = 512U;
constexpr std::uint16_t enemy_clip_width = playfield_width + enemy_width;
constexpr std::uint16_t enemy_clip_height = playfield_height + enemy_height;
constexpr std::uint16_t playfield_right = 6656U;
constexpr std::uint16_t playfield_bottom = 6144U;
constexpr std::uint16_t enemy_homing_extent = 256U;
constexpr std::uint16_t player_collision_extent = 384U;
constexpr std::uint16_t autofire_near_extent = 1536U;
constexpr std::int16_t autofire_bottom = 4864;
constexpr std::uint8_t rank_easy = 0U;
constexpr std::uint8_t rank_lunatic = 3U;
constexpr std::uint8_t pat_enemy_kill = 4U;
constexpr std::uint8_t enemy_kill_cels = 8U;
constexpr std::uint8_t clip_x = 0x01U;
constexpr std::uint8_t clip_y = 0x10U;

[[nodiscard]] constexpr std::int16_t word_from(std::uint16_t bits) noexcept
{
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] constexpr std::int16_t word_add(
	std::int16_t left, std::int16_t right
) noexcept
{
	return word_from(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) + static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::int16_t word_sub(
	std::int16_t left, std::int16_t right
) noexcept
{
	return word_from(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) - static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::uint8_t byte_add(
	std::uint8_t left, std::uint8_t right
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<unsigned>(left) + static_cast<unsigned>(right)
	);
}

[[nodiscard]] constexpr std::uint8_t byte_add(
	std::uint8_t left, std::int8_t right
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<int>(left) + static_cast<int>(right)
	);
}

[[nodiscard]] constexpr std::uint16_t offset_add(
	std::uint16_t left, std::uint16_t right
) noexcept
{
	return static_cast<std::uint16_t>(left + right);
}

[[nodiscard]] constexpr bool overlap_fast(
	std::int16_t left, std::int16_t right, std::uint16_t extent
) noexcept
{
	return static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(word_sub(left, right)) + (extent / 2U)
	) < extent;
}

[[nodiscard]] constexpr bool instruction_available(
	const State &state, const Enemy &enemy, std::uint16_t length
) noexcept
{
	for(std::uint16_t index = 0U; index < length; ++index) {
		const auto offset = offset_add(
			offset_add(enemy.script, enemy.script_ip), index
		);
		if(offset >= state.std_data.data.size()) return false;
	}
	return true;
}

[[nodiscard]] std::uint8_t instruction_byte(
	const State &state, const Enemy &enemy, std::uint16_t index
) noexcept
{
	return state.std_data.data[offset_add(
		offset_add(enemy.script, enemy.script_ip), index
	)];
}

[[nodiscard]] std::int16_t instruction_word(
	const State &state, const Enemy &enemy, std::uint16_t index
) noexcept
{
	return word_from(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(instruction_byte(state, enemy, index)) |
		(static_cast<std::uint16_t>(instruction_byte(
			state, enemy, static_cast<std::uint16_t>(index + 1U)
		)) << 8U)
	));
}

constexpr void script_advance(Enemy &enemy, std::uint16_t length) noexcept
{
	enemy.script_ip = offset_add(enemy.script_ip, length);
}

[[nodiscard]] constexpr bool script_blocking(
	Enemy &enemy, std::uint8_t frames, std::uint16_t length
) noexcept
{
	if(enemy.cur_instr_frame >= frames) {
		enemy.cur_instr_frame = 0U;
		script_advance(enemy, length);
	} else {
		++enemy.cur_instr_frame;
	}
	return true;
}

constexpr void enemy_velocity_set(Enemy &enemy) noexcept
{
	const auto velocity = pc98::masterlib::vector2(
		enemy.angle, static_cast<std::int16_t>(enemy.speed)
	);
	enemy.pos.velocity = {velocity.x, velocity.y};
}

[[nodiscard]] bool enemy_velocity_set_aimed(State &state, Enemy &enemy) noexcept
{
	const auto angle = pc98::masterlib::iatan2_source(
		word_sub(state.player_pos.cur.y, enemy.pos.cur.y),
		word_sub(state.player_pos.cur.x, enemy.pos.cur.x)
	);
	if(!angle) return false;
	enemy.angle = byte_add(*angle, enemy.angle);
	enemy_velocity_set(enemy);
	return true;
}

[[nodiscard]] bool enemy_pos_update(State &state, Enemy &enemy) noexcept
{
	const auto position = pc98::th0405::update_motion(enemy.pos);
	if((enemy.clip & clip_x) != 0U && static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(position.x) + (enemy_width / 2U)
	) >= enemy_clip_width) {
		++state.enemies.gone;
		enemy.flag = enemy_killed;
		return true;
	}
	if((enemy.clip & clip_y) != 0U && static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(position.y) + (enemy_height / 2U)
	) >= enemy_clip_height) {
		++state.enemies.gone;
		enemy.flag = enemy_killed;
		return true;
	}
	return false;
}

constexpr void enemy_bullet_template_push(State &state, const Enemy &enemy) noexcept
{
	state.bullets.bullet_template = enemy.bullet_template;
	state.bullets.bullet_template.origin.x = word_add(
		state.bullets.bullet_template.origin.x, enemy.pos.cur.x
	);
	state.bullets.bullet_template.origin.y = word_add(
		state.bullets.bullet_template.origin.y, enemy.pos.cur.y
	);
}

enum class ScriptBulletAdd : std::uint8_t { regular, special, regular_fixedspeed };

[[nodiscard]] bool enemy_bullet_fire(
	State &state, const Enemy &enemy, ScriptBulletAdd add
) noexcept
{
	enemy_bullet_template_push(state, enemy);
	if(!bullet_template_tune(state)) return false;
	switch(add) {
	case ScriptBulletAdd::regular:
		return bullets_add_regular(state);
	case ScriptBulletAdd::special:
		return bullets_add_special(state);
	case ScriptBulletAdd::regular_fixedspeed:
		return bullets_add_regular_fixedspeed(state);
	}
	return false;
}

[[nodiscard]] bool enemy_run(
	State &state, Presentation &presentation, Enemy &enemy
) noexcept
{
	// The source keeps dispatching immediate commands until one blocks for a
	// frame. A bounded malformed host script must not turn that near-code loop
	// into unbounded native execution.
	for(std::size_t steps = 0U; steps < state.std_data.data.size(); ++steps) {
		if(!instruction_available(state, enemy, 1U)) return false;
		const auto opcode = instruction_byte(state, enemy, 0U);
		if(opcode > 0x3AU) return true;

		switch(opcode) {
		case 0x00U:
			enemy.flag = enemy_killed;
			return true;

		case 0x01U:
			if(!instruction_available(state, enemy, 6U)) return false;
			enemy.flag = enemy_alive;
			enemy.patnum_base = instruction_byte(state, enemy, 1U);
			enemy.hp = instruction_word(state, enemy, 2U);
			enemy.score = instruction_word(state, enemy, 4U);
			enemy.can_be_damaged = true;
			enemy.kills_player_on_collision = true;
			script_advance(enemy, 6U);
			break;

		case 0x02U:
			if(!instruction_available(state, enemy, 4U)) return false;
			if(enemy.cur_instr_frame == 0U) {
				enemy.angle = instruction_byte(state, enemy, 1U);
				enemy.speed = instruction_byte(state, enemy, 2U);
				enemy_velocity_set(enemy);
			}
			if(enemy_pos_update(state, enemy)) return true;
			return script_blocking(enemy, instruction_byte(state, enemy, 3U), 4U);

		case 0x03U:
			if(!instruction_available(state, enemy, 2U)) return false;
			if(enemy_pos_update(state, enemy)) return true;
			return script_blocking(enemy, instruction_byte(state, enemy, 1U), 2U);

		case 0x04U:
			if(!instruction_available(state, enemy, 3U)) return false;
			if(enemy.cur_instr_frame == 0U) {
				enemy.speed = instruction_byte(state, enemy, 1U);
				enemy_velocity_set(enemy);
			}
			if(enemy_pos_update(state, enemy)) return true;
			return script_blocking(enemy, instruction_byte(state, enemy, 2U), 3U);

		case 0x05U:
		case 0x06U: {
			const auto accelerated = opcode == 0x06U;
			const auto length = static_cast<std::uint16_t>(accelerated ? 7U : 5U);
			if(!instruction_available(state, enemy, length)) return false;
			if(enemy.cur_instr_frame == 0U) {
				enemy.angle = instruction_byte(state, enemy, 1U);
				enemy.speed = instruction_byte(state, enemy, 2U);
				enemy.angle_delta = instruction_byte(state, enemy, 3U);
			}
			enemy_velocity_set(enemy);
			const auto frame_index = static_cast<std::uint16_t>(
				accelerated ? 6U : 4U
			);
			if(accelerated) {
				enemy.pos.velocity.x = word_add(enemy.pos.velocity.x,
					static_cast<std::int8_t>(instruction_byte(state, enemy, 4U)));
				enemy.pos.velocity.y = word_add(enemy.pos.velocity.y,
					static_cast<std::int8_t>(instruction_byte(state, enemy, 5U)));
			}
			if(enemy_pos_update(state, enemy)) return true;
			enemy.angle = byte_add(enemy.angle, enemy.angle_delta);
			return script_blocking(enemy, instruction_byte(state, enemy, frame_index), length);
		}

		case 0x07U:
		case 0x08U: {
			const auto accelerated = opcode == 0x08U;
			const auto length = static_cast<std::uint16_t>(accelerated ? 4U : 2U);
			if(!instruction_available(state, enemy, length)) return false;
			enemy_velocity_set(enemy);
			if(accelerated) {
				enemy.pos.velocity.x = word_add(enemy.pos.velocity.x,
					static_cast<std::int8_t>(instruction_byte(state, enemy, 1U)));
				enemy.pos.velocity.y = word_add(enemy.pos.velocity.y,
					static_cast<std::int8_t>(instruction_byte(state, enemy, 2U)));
			}
			if(enemy_pos_update(state, enemy)) return true;
			enemy.angle = byte_add(enemy.angle, enemy.angle_delta);
			return script_blocking(enemy,
				instruction_byte(state, enemy, accelerated ? 3U : 1U), length);
		}

		case 0x09U:
			if(!instruction_available(state, enemy, 2U)) return false;
			if(enemy.cur_instr_frame == 0U) enemy.pos.prev = enemy.pos.cur;
			return script_blocking(enemy, instruction_byte(state, enemy, 1U), 2U);

		case 0x0AU:
			if(!instruction_available(state, enemy, 2U)) return false;
			if(enemy.cur_instr_frame == 0U) enemy.pos.velocity.x = 0;
			enemy.pos.velocity.y = state.scroll_last_delta;
			if(enemy_pos_update(state, enemy)) return true;
			return script_blocking(enemy, instruction_byte(state, enemy, 1U), 2U);

		case 0x0BU:
		case 0x0CU: {
			if(!instruction_available(state, enemy, 5U)) return false;
			if(enemy.cur_instr_frame == 0U) {
				enemy.angle = 0U;
				enemy.angle_delta = instruction_byte(state, enemy, 2U);
			}
			enemy.pos.velocity.x = pc98::masterlib::vector_component(
				static_cast<std::int16_t>(instruction_byte(state, enemy, 1U)),
				pc98::masterlib::cosine8(enemy.angle)
			);
			enemy.pos.velocity.y = static_cast<std::int8_t>(instruction_byte(state, enemy, 3U));
			if(opcode == 0x0CU) {
				const auto temporary = enemy.pos.velocity.x;
				enemy.pos.velocity.x = enemy.pos.velocity.y;
				enemy.pos.velocity.y = temporary;
			}
			if(enemy_pos_update(state, enemy)) return true;
			enemy.angle = byte_add(enemy.angle, enemy.angle_delta);
			return script_blocking(enemy, instruction_byte(state, enemy, 4U), 5U);
		}

		case 0x0DU:
			if(!instruction_available(state, enemy, 3U)) return false;
			enemy.angle = instruction_byte(state, enemy, 1U);
			enemy.speed = instruction_byte(state, enemy, 2U);
			if(!enemy_velocity_set_aimed(state, enemy)) return false;
			script_advance(enemy, 3U);
			break;

		case 0x0EU:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.angle = byte_add(enemy.angle, instruction_byte(state, enemy, 1U));
			enemy_velocity_set(enemy);
			script_advance(enemy, 2U);
			break;

		case 0x0FU:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.speed = byte_add(enemy.speed,
				static_cast<std::int8_t>(instruction_byte(state, enemy, 1U)));
			enemy_velocity_set(enemy);
			script_advance(enemy, 2U);
			break;

		case 0x10U: {
			const auto angle = pc98::th0405::randring_next16(state.random);
			if(!angle) return false;
			enemy.angle = static_cast<std::uint8_t>(*angle);
			script_advance(enemy, 1U);
			break;
		}

		case 0x11U:
			if(!enemy_bullet_fire(state, enemy, ScriptBulletAdd::regular)) return false;
			script_advance(enemy, 1U);
			break;

		case 0x12U:
			if(!instruction_available(state, enemy, 6U)) return false;
			enemy.autofire = false;
			enemy.bullet_template.spawn_type = instruction_byte(state, enemy, 1U);
			enemy.bullet_template.group = instruction_byte(state, enemy, 2U);
			enemy.bullet_template.angle = instruction_byte(state, enemy, 3U);
			enemy.bullet_template.speed = instruction_byte(state, enemy, 4U);
			enemy.bullet_template.patnum = instruction_byte(state, enemy, 5U);
			script_advance(enemy, 6U);
			break;

		case 0x13U:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.bullet_template.spawn_type = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x14U:
			if(!instruction_available(state, enemy, 5U)) return false;
			enemy.bullet_template.origin.x = instruction_word(state, enemy, 1U);
			enemy.bullet_template.origin.y = instruction_word(state, enemy, 3U);
			script_advance(enemy, 5U);
			break;

		case 0x15U:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.bullet_template.angle = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x16U:
		case 0x1FU:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.bullet_template.angle = byte_add(
				enemy.bullet_template.angle, instruction_byte(state, enemy, 1U)
			);
			script_advance(enemy, 2U);
			break;

		case 0x17U: {
			const auto angle = pc98::th0405::randring_next16(state.random);
			if(!angle) return false;
			enemy.bullet_template.angle = static_cast<std::uint8_t>(*angle);
			script_advance(enemy, 1U);
			break;
		}

		case 0x18U:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.bullet_template.patnum = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x19U:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.bullet_template.speed = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x1AU:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.bullet_template.speed = byte_add(
				enemy.bullet_template.speed, instruction_byte(state, enemy, 1U)
			);
			script_advance(enemy, 2U);
			break;

		case 0x1BU:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.bullet_template.group = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x1CU: {
			if(!instruction_available(state, enemy, 2U)) return false;
			const auto operand = instruction_byte(state, enemy, 1U);
			std::uint8_t interval{};
			if(state.resources.playperf > 32U) {
				const auto scaled = static_cast<std::uint16_t>(
					(static_cast<std::uint16_t>(operand) *
					static_cast<std::uint8_t>(state.resources.playperf - 32U)) >> 6U
				);
				const auto value = static_cast<std::uint16_t>(
					static_cast<std::uint16_t>(operand) - scaled
				);
				interval = static_cast<std::uint8_t>(value);
				if(interval < 16U) interval = 16U;
			} else {
				const auto scaled = static_cast<std::uint16_t>(
					(static_cast<std::uint16_t>(operand) *
					static_cast<std::uint8_t>(32U - state.resources.playperf)) >> 6U
				);
				const auto value = static_cast<std::uint16_t>(
					static_cast<std::uint16_t>(operand) + scaled
				);
				interval = value >= 256U ? 255U : static_cast<std::uint8_t>(value);
			}
			if(state.rank == rank_easy) interval = 255U;
			enemy.autofire_interval = interval;
			script_advance(enemy, 2U);
			break;
		}

		case 0x1DU:
			enemy.autofire = true;
			script_advance(enemy, 1U);
			break;

		case 0x1EU:
			enemy.autofire = false;
			script_advance(enemy, 1U);
			break;

		case 0x20U:
			if(!instruction_available(state, enemy, 3U)) return false;
			enemy.bullet_template.spread = instruction_byte(state, enemy, 1U);
			enemy.bullet_template.spread_angle_delta = instruction_byte(state, enemy, 2U);
			script_advance(enemy, 3U);
			break;

		case 0x21U:
			if(!instruction_available(state, enemy, 3U)) return false;
			enemy.bullet_template.stack = instruction_byte(state, enemy, 1U);
			enemy.bullet_template.stack_speed_delta = instruction_byte(state, enemy, 2U);
			script_advance(enemy, 3U);
			break;

		case 0x22U:
			enemy.clip = static_cast<std::uint8_t>(clip_x | clip_y);
			script_advance(enemy, 1U);
			break;

		case 0x23U:
			enemy.clip = static_cast<std::uint8_t>(enemy.clip | clip_x);
			script_advance(enemy, 1U);
			break;

		case 0x24U:
			enemy.clip = static_cast<std::uint8_t>(enemy.clip | clip_y);
			script_advance(enemy, 1U);
			break;

		case 0x25U:
			if(!instruction_available(state, enemy, 3U)) return false;
			enemy.anim_cels = instruction_byte(state, enemy, 1U);
			enemy.anim_frames_per_cel = instruction_byte(state, enemy, 2U);
			script_advance(enemy, 3U);
			break;

		case 0x26U:
			if(!instruction_available(state, enemy, 2U)) return false;
			presentation.sound(instruction_byte(state, enemy, 1U));
			script_advance(enemy, 2U);
			break;

		case 0x27U:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.patnum_base = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x28U:
			enemy.can_be_damaged = false;
			enemy.autofire = false;
			script_advance(enemy, 1U);
			break;

		case 0x29U:
			enemy.can_be_damaged = true;
			enemy.autofire = state.rank == rank_lunatic;
			script_advance(enemy, 1U);
			break;

		case 0x2AU:
			enemy.kills_player_on_collision = false;
			script_advance(enemy, 1U);
			break;

		case 0x2BU:
			enemy.kills_player_on_collision = true;
			script_advance(enemy, 1U);
			break;

		case 0x2CU:
			if(!instruction_available(state, enemy, 5U)) return false;
			enemy.pos.prev = enemy.pos.cur;
			enemy.pos.cur.x = instruction_word(state, enemy, 1U);
			enemy.pos.cur.y = instruction_word(state, enemy, 3U);
			return script_blocking(enemy, 0U, 5U);

		case 0x2DU:
			if(!instruction_available(state, enemy, 5U)) return false;
			enemy.pos.prev = enemy.pos.cur;
			enemy.pos.cur.x = word_add(enemy.pos.cur.x, instruction_word(state, enemy, 1U));
			enemy.pos.cur.y = word_add(enemy.pos.cur.y, instruction_word(state, enemy, 3U));
			return script_blocking(enemy, 0U, 5U);

		case 0x2EU:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.patnum_base = byte_add(
				enemy.patnum_base, instruction_byte(state, enemy, 1U)
			);
			script_advance(enemy, 2U);
			break;

		case 0x2FU:
			if(!instruction_available(state, enemy, 3U)) return false;
			enemy.angle = instruction_byte(state, enemy, 1U);
			enemy.speed = instruction_byte(state, enemy, 2U);
			enemy_velocity_set(enemy);
			script_advance(enemy, 3U);
			break;

		case 0x30U:
			if(!instruction_available(state, enemy, 3U)) return false;
			enemy.angle = instruction_byte(state, enemy, 1U);
			enemy.speed = instruction_byte(state, enemy, 2U);
			if(!enemy.spawned_in_left_half) {
				enemy.angle = static_cast<std::uint8_t>(0x80U - enemy.angle);
			}
			enemy_velocity_set(enemy);
			script_advance(enemy, 3U);
			break;

		case 0x31U:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.speed = instruction_byte(state, enemy, 1U);
			enemy_velocity_set(enemy);
			script_advance(enemy, 2U);
			break;

		case 0x32U:
		case 0x33U:
			if(!instruction_available(state, enemy, 3U)) return false;
			if(enemy.loop_i >= instruction_byte(state, enemy, 2U)) {
				enemy.loop_i = 0U;
				script_advance(enemy, 3U);
			} else {
				++enemy.loop_i;
				enemy.script_ip = static_cast<std::uint16_t>(
					enemy.script_ip - instruction_byte(state, enemy, 1U)
				);
			}
			break;

		case 0x34U:
			if(!instruction_available(state, enemy, 3U)) return false;
			if(instruction_byte(state, enemy, 1U) == enemy.subtype) {
				script_advance(enemy, 3U);
			} else {
				script_advance(enemy, static_cast<std::uint16_t>(
					3U + instruction_byte(state, enemy, 2U)
				));
			}
			break;

		case 0x35U:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.script_ip = static_cast<std::uint16_t>(
				enemy.script_ip - instruction_byte(state, enemy, 1U)
			);
			break;

		case 0x36U:
			if(!instruction_available(state, enemy, 2U)) return false;
			if(enemy_pos_update(state, enemy)) return true;
			return script_blocking(enemy, instruction_byte(state, enemy, 1U), 2U);

		case 0x37U:
			if(!enemy_bullet_fire(state, enemy, ScriptBulletAdd::special)) return false;
			script_advance(enemy, 1U);
			break;

		case 0x38U:
			if(!instruction_available(state, enemy, 2U)) return false;
			enemy.bullet_template.special_motion = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x39U:
			if(!enemy_bullet_fire(state, enemy, ScriptBulletAdd::regular_fixedspeed)) return false;
			script_advance(enemy, 1U);
			break;

		case 0x3AU: {
			if(!instruction_available(state, enemy, 2U)) return false;
			const auto angle = pc98::masterlib::iatan2_source(
				word_sub(state.player_pos.cur.y, word_add(
					enemy.pos.cur.y, enemy.bullet_template.origin.y
				)),
				word_sub(state.player_pos.cur.x, word_add(
					enemy.pos.cur.x, enemy.bullet_template.origin.x
				))
			);
			if(!angle) return false;
			enemy.bullet_template.angle = byte_add(*angle, instruction_byte(state, enemy, 1U));
			script_advance(enemy, 2U);
			break;
		}
		}
	}
	return false;
}

[[nodiscard]] bool kill_enemy(
	State &state, Presentation &presentation, Enemy &enemy
) noexcept
{
	enemy.flag = enemy_kill_anim;
	enemy.anim_cels = 1U;
	enemy.can_be_damaged = false;
	enemy.kills_player_on_collision = false;
	enemy.pos.velocity = {};
	if(enemy.item != static_cast<std::uint8_t>(ItemType::none)) {
		items_add(state, presentation, enemy.pos.cur,
			static_cast<ItemType>(static_cast<std::int8_t>(enemy.item)));
	}
	presentation.sound(3U);
	state.score.score_delta += static_cast<std::uint16_t>(enemy.score);
	const auto sparks = pc98::th0405::sparks_add_random(
		pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
		state.random, enemy.pos.cur, 64, 7U
	);
	if(!sparks) return false;
	++state.enemies.gone;
	++state.enemies.killed;
	return true;
}

[[nodiscard]] bool enemy_autofire(State &state, Enemy &enemy) noexcept
{
	if(!enemy.autofire) return true;
	++enemy.autofire_cur_frame;
	if(enemy.autofire_cur_frame < enemy.autofire_interval ||
		enemy.pos.cur.y >= autofire_bottom ||
		(overlap_fast(enemy.pos.cur.x, state.player_pos.cur.x, autofire_near_extent) &&
			overlap_fast(enemy.pos.cur.y, state.player_pos.cur.y, autofire_near_extent))) {
		return true;
	}
	enemy.autofire_cur_frame = 0U;
	return enemy_bullet_fire(state, enemy, ScriptBulletAdd::regular);
}

} // namespace

bool enemies_add(State &state) noexcept
{
	const auto record = state.std_data.ip;
	for(std::uint16_t index = 0U; index < 8U; ++index) {
		if(offset_add(record, index) >= state.std_data.data.size()) return false;
	}
	const auto script_id = state.std_data.data[record];
	if(script_id >= state.std_data.enemy_scripts.size()) return false;
	const auto read_word = [&state](std::uint16_t offset) noexcept {
		return word_from(static_cast<std::uint16_t>(
			static_cast<std::uint16_t>(state.std_data.data[offset]) |
			(static_cast<std::uint16_t>(state.std_data.data[
				offset_add(offset, 1U)
			]) << 8U)
		));
	};
	auto center_x = read_word(offset_add(record, 1U));
	auto center_y = read_word(offset_add(record, 3U));

	for(auto &enemy : state.enemies.enemies) {
		if(enemy.flag != enemy_free) continue;
		enemy.flag = enemy_alive_first_frame;
		enemy.age = 0U;
		enemy.cur_instr_frame = 0U;
		enemy.loop_i = 0U;
		enemy.script_ip = 0U;
		enemy.script = state.std_data.enemy_scripts[script_id];
		if(center_x == enemy_random_position) {
			const auto random = pc98::th0405::randring_next16_mod(state.random, playfield_width);
			if(!random) return false;
			center_x = word_from(*random);
		}
		enemy.pos.cur.x = center_x;
		enemy.spawned_in_left_half = center_x < 3072;
		if(center_y == enemy_random_position) {
			const auto random = pc98::th0405::randring_next16_mod(state.random, playfield_height);
			if(!random) return false;
			center_y = word_from(*random);
		}
		enemy.pos.cur.y = center_y;
		enemy.item = state.std_data.data[offset_add(record, 5U)];
		enemy.subtype = state.std_data.data[offset_add(record, 6U)];
		enemy.damaged_this_frame = false;
		enemy.autofire = state.rank == rank_lunatic;
		enemy.clip = 0U;
		enemy.anim_cels = 1U;
		enemy.anim_frames_per_cel = 4U;
		enemy.anim_cur_cel = 0U;
		enemy.can_be_damaged = false;
		enemy.kills_player_on_collision = false;
		const auto random = pc98::th0405::randring_next16(state.random);
		if(!random) return false;
		enemy.autofire_cur_frame = static_cast<std::uint8_t>(*random);
		enemy.autofire_interval = 128U;
		enemy.bullet_template.group = static_cast<std::uint8_t>(BulletGroup::forcesingle_aimed);
		enemy.bullet_template.spawn_type = bullet_spawn_type_normal;
		enemy.bullet_template.speed = 42U;
		enemy.bullet_template.origin = {};
		enemy.bullet_template.patnum = 0U;
		break;
	}
	return true;
}

bool enemies_update(State &state, Presentation &presentation) noexcept
{
	state.shots.homing_target = {homing_target_none, homing_target_none};
	std::uint8_t homing_enemy_count{};
	for(auto &enemy : state.enemies.enemies) {
		if(enemy.flag == enemy_free) continue;
		if(enemy.flag == enemy_killed) {
			enemy.flag = enemy_free;
			continue;
		}

		if(enemy.flag < enemy_kill_anim) {
			if(!enemy_run(state, presentation, enemy)) return false;
			if(enemy.kills_player_on_collision &&
			overlap_fast(enemy.pos.cur.x, state.player_pos.cur.x, player_collision_extent) &&
			overlap_fast(enemy.pos.cur.y, state.player_pos.cur.y, player_collision_extent)) {
				state.player_is_hit = true;
				if(!kill_enemy(state, presentation, enemy)) return false;
				continue;
			}

			if(enemy.can_be_damaged && enemy.hp != -1 &&
				static_cast<std::uint16_t>(
					static_cast<std::uint16_t>(enemy.pos.cur.x) + enemy_homing_extent
				) < playfield_right &&
				static_cast<std::uint16_t>(
					static_cast<std::uint16_t>(enemy.pos.cur.y) + enemy_homing_extent
				) < playfield_bottom) {
				++homing_enemy_count;
				if(enemy.pos.cur.y > state.shots.homing_target.y) {
					state.shots.homing_target = enemy.pos.cur;
				}
				const auto damage = shots_hittest(
					state.shots,
					{enemy.pos.cur, {256, 192}},
					state.shot_hit_spark_parity,
					{state.bombing, false, static_cast<ShotPlaychar>(state.playchar),
						state.stage_id, static_cast<std::uint8_t>(state.stage_frame & 3U),
						state.bomb_frame, state.player_pos.cur},
					pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
					state.random, state.score
				);
				if(!damage) return false;
				const auto damage_byte = static_cast<std::uint8_t>(*damage);
				if(damage_byte != 0U) {
					if(enemy.hp == -2) {
						presentation.sound(10U);
					} else if(static_cast<std::int16_t>(damage_byte) >= enemy.hp) {
						if(!kill_enemy(state, presentation, enemy)) return false;
						continue;
					} else {
						enemy.hp = static_cast<std::int16_t>(enemy.hp - damage_byte);
						enemy.damaged_this_frame = true;
					}
				}
			}
			if(!enemy_autofire(state, enemy)) return false;
			++enemy.age;
		} else {
			(void)pc98::th0405::update_motion(enemy.pos);
			++enemy.flag;
			enemy.patnum_base = static_cast<std::uint8_t>(
				((static_cast<unsigned>(enemy.flag) - enemy_kill_anim) / 4U) + pat_enemy_kill
			);
			if(enemy.patnum_base >= static_cast<std::uint8_t>(pat_enemy_kill + enemy_kill_cels)) {
				enemy.flag = enemy_killed;
			}
		}
	}

	if(state.shots.homing_target.x != homing_target_none) {
		std::uint8_t interval{};
		if(homing_enemy_count >= 8U) {
			interval = 80U;
		} else {
			interval = static_cast<std::uint8_t>(144U - (homing_enemy_count << 3U));
		}
		interval = static_cast<std::uint8_t>(interval - (state.stage_id << 4U));
		if(interval > 144U || interval < 4U) interval = 4U;
		if((state.stage_frame % interval) == 0U && state.items.dream < 128U) {
			++state.items.dream;
			presentation.hud(HudField::dream);
		}
	}
	return true;
}

} // namespace th05::core
