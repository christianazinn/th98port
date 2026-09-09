#include "th04/core/enemy.hpp"

#include "th04/core/item.hpp"
#include "th04/core/shot_collision.hpp"
#include "th04/core/shot_reimu_a.hpp"
#include "th04/core/state.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_sparks.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th04::core {
namespace {

constexpr std::int16_t enemy_random_position = 15984;
constexpr std::uint16_t playfield_width = 6144U;
constexpr std::uint16_t playfield_height = 5888U;
constexpr std::uint16_t enemy_width = 512U;
constexpr std::uint16_t enemy_height = 512U;
constexpr std::uint16_t enemy_clip_width = playfield_width + enemy_width;
constexpr std::uint16_t enemy_clip_height = playfield_height + enemy_height;
constexpr std::int16_t playfield_top = 256;
constexpr std::int16_t vram_height = 400;
constexpr std::int16_t tile_size = 16;
constexpr std::uint8_t rank_easy = 0U;
constexpr std::uint8_t rank_lunatic = 3U;
constexpr std::uint8_t pat_enemy_kill = 4U;
constexpr std::uint8_t enemy_kill_cels = 8U;

enum class EnemyRunResult : std::uint8_t { frame, killed, unsupported };

[[nodiscard]] constexpr std::int16_t word_add(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(left) + right
	));
}

[[nodiscard]] constexpr std::int16_t word_sub(
	std::int16_t left, std::int16_t right
) noexcept
{
	return word_add(left, std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		0U - std::bit_cast<std::uint16_t>(right)
	)));
}

[[nodiscard]] constexpr std::int16_t word_from(std::uint16_t bits) noexcept
{
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] constexpr std::int16_t sar4(std::int16_t value) noexcept
{
	auto bits = static_cast<std::uint16_t>(
		std::bit_cast<std::uint16_t>(value) >> 4U
	);
	if(value < 0) bits = static_cast<std::uint16_t>(bits | 0xF000U);
	return word_from(bits);
}

[[nodiscard]] constexpr std::uint16_t add_offset(
	std::uint16_t left, std::uint16_t right
) noexcept
{
	return static_cast<std::uint16_t>(left + right);
}

[[nodiscard]] constexpr std::uint8_t byte_add(
	std::uint8_t left, std::int8_t right
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<std::int16_t>(left) + static_cast<std::int16_t>(right)
	);
}

[[nodiscard]] constexpr std::uint8_t byte_add(
	std::uint8_t left, std::uint8_t right
) noexcept
{
	return static_cast<std::uint8_t>(static_cast<std::uint16_t>(left) + right);
}

[[nodiscard]] constexpr bool overlap_fast(
	std::int16_t left, std::int16_t right, std::uint16_t extent
) noexcept
{
	return static_cast<std::uint16_t>(static_cast<std::uint16_t>(
		std::bit_cast<std::uint16_t>(word_sub(left, right))
	) + (extent / 2U)) < extent;
}

[[nodiscard]] constexpr bool overlap_fast(
	pc98::th0405::SubpixelPoint left, pc98::th0405::SubpixelPoint right,
	std::uint16_t width, std::uint16_t height
) noexcept
{
	return overlap_fast(left.x, right.x, width) &&
		overlap_fast(left.y, right.y, height);
}

[[nodiscard]] bool instruction_available(
	const State &state, const Enemy &enemy, std::uint16_t length
) noexcept
{
	for(std::uint16_t i = 0; i < length; ++i) {
		const auto offset = add_offset(
			add_offset(enemy.script, enemy.script_ip), i
		);
		if(offset >= state.std_data.data.size()) return false;
	}
	return true;
}

[[nodiscard]] std::uint8_t instruction_byte(
	const State &state, const Enemy &enemy, std::uint16_t index
) noexcept
{
	const auto offset = add_offset(add_offset(enemy.script, enemy.script_ip), index);
	return state.std_data.data[offset];
}

[[nodiscard]] std::int16_t instruction_word(
	const State &state, const Enemy &enemy, std::uint16_t index
) noexcept
{
	const auto low = instruction_byte(state, enemy, index);
	const auto high = instruction_byte(state, enemy, static_cast<std::uint16_t>(index + 1U));
	return word_from(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(low) |
		(static_cast<std::uint16_t>(high) << 8U)
	));
}

constexpr void script_advance(Enemy &enemy, std::uint16_t length) noexcept
{
	enemy.script_ip = add_offset(enemy.script_ip, length);
}

constexpr EnemyRunResult script_blocking(
	Enemy &enemy, std::uint8_t frames, std::uint16_t length
) noexcept
{
	if(enemy.cur_instr_frame >= frames) {
		enemy.cur_instr_frame = 0;
		script_advance(enemy, length);
	} else {
		enemy.cur_instr_frame = static_cast<std::uint8_t>(enemy.cur_instr_frame + 1U);
	}
	return EnemyRunResult::frame;
}

constexpr void enemy_velocity_set(Enemy &enemy) noexcept
{
	const auto velocity = pc98::masterlib::vector2(
		enemy.angle, enemy.speed
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
	if(enemy.clip_x && static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(std::bit_cast<std::uint16_t>(position.x)) +
		(enemy_width / 2U)
	) >= enemy_clip_width) {
		++state.enemies.gone;
		enemy.flag = enemy_killed;
		return true;
	}
	if(enemy.clip_y && static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(std::bit_cast<std::uint16_t>(position.y)) +
		(enemy_height / 2U)
	) >= enemy_clip_height) {
		++state.enemies.gone;
		enemy.flag = enemy_killed;
		return true;
	}
	return false;
}

// th04/main/tile/set.cpp. The original forces scrolling active before the
// conversion, then writes the background tile ring directly.
[[nodiscard]] bool tile_ring_set_vo(State &state, const Enemy &enemy,
	std::uint8_t image_vo
) noexcept
{
	const auto x = sar4(enemy.pos.cur.x);
	auto y = sar4(word_add(playfield_top, enemy.pos.cur.y));
	y = word_add(y, state.scroll_line);
	if(y < 0) {
		y = word_add(y, vram_height);
	} else if(y >= vram_height) {
		y = word_add(y, static_cast<std::int16_t>(-vram_height));
	}
	if(x < 0 || x >= static_cast<std::int16_t>(32 * tile_size) || y < 0 ||
		y >= vram_height) return false;
	const auto row = static_cast<std::size_t>(y / tile_size);
	const auto column = static_cast<std::size_t>(x / tile_size);
	state.tile_ring[(row * 32U) + column] = image_vo;
	return true;
}

// script.cpp's inline push, deliberately distinct from the full word-copy
// used by update.cpp's enemy_bullet_template_push() autofire call.
constexpr void script_bullet_template_push(State &state, const Enemy &enemy) noexcept
{
	auto &destination = state.bullets.template_;
	const auto &source = enemy.bullet_template;
	destination.spawn_type = source.spawn_type;
	destination.patnum = source.patnum;
	destination.origin.x = word_add(source.origin.x, enemy.pos.cur.x);
	destination.origin.y = word_add(source.origin.y, enemy.pos.cur.y);
	destination.group = source.group;
	destination.angle = source.angle;
	destination.speed = source.speed;
	destination.count = source.count;
	destination.delta = source.delta;
}

constexpr void autofire_bullet_template_push(State &state, const Enemy &enemy) noexcept
{
	state.bullets.template_ = enemy.bullet_template;
	state.bullets.template_.origin.x = word_add(
		state.bullets.template_.origin.x, enemy.pos.cur.x
	);
	state.bullets.template_.origin.y = word_add(
		state.bullets.template_.origin.y, enemy.pos.cur.y
	);
}

[[nodiscard]] EnemyRunResult enemy_run(
	State &state, Presentation &presentation, Enemy &enemy
) noexcept
{
	// A successful nonblocking path cannot visit more than a segment's worth
	// of instruction starts without repeating source IP state. The native game
	// would remain in this loop; refuse the unrepresentable hang.
	for(std::uint32_t steps = 0; steps < 65536U; ++steps) {
		if(!instruction_available(state, enemy, 1U)) return EnemyRunResult::unsupported;
		switch(instruction_byte(state, enemy, 0U)) {
		case 0x01U:
			if(!instruction_available(state, enemy, 4U)) return EnemyRunResult::unsupported;
			if(enemy.cur_instr_frame == 0U) {
				enemy.speed = instruction_byte(state, enemy, 2U);
				enemy.angle = instruction_byte(state, enemy, 1U);
				enemy_velocity_set(enemy);
			}
			if(enemy_pos_update(state, enemy)) return EnemyRunResult::killed;
			return script_blocking(enemy, instruction_byte(state, enemy, 3U), 4U);

		case 0x02U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			if(enemy.cur_instr_frame == 0U) enemy_velocity_set(enemy);
			if(enemy_pos_update(state, enemy)) return EnemyRunResult::killed;
			return script_blocking(enemy, instruction_byte(state, enemy, 1U), 2U);

		case 0x03U:
			if(!instruction_available(state, enemy, 3U)) return EnemyRunResult::unsupported;
			if(enemy.cur_instr_frame == 0U) {
				enemy.speed = instruction_byte(state, enemy, 1U);
				enemy_velocity_set(enemy);
			}
			if(enemy_pos_update(state, enemy)) return EnemyRunResult::killed;
			return script_blocking(enemy, instruction_byte(state, enemy, 2U), 3U);

		case 0x04U:
		case 0x05U: {
			const auto opcode = instruction_byte(state, enemy, 0U);
			const auto length = static_cast<std::uint16_t>(opcode == 0x05U ? 7U : 5U);
			if(!instruction_available(state, enemy, length)) return EnemyRunResult::unsupported;
			if(enemy.cur_instr_frame == 0U) {
				enemy.angle = instruction_byte(state, enemy, 1U);
				enemy.angle_delta = instruction_byte(state, enemy, 3U);
				enemy.speed = instruction_byte(state, enemy, 2U);
			}
			enemy_velocity_set(enemy);
			std::uint8_t frames{};
			if(opcode == 0x05U) {
				enemy.pos.velocity.x = word_add(enemy.pos.velocity.x,
				static_cast<std::int8_t>(instruction_byte(state, enemy, 4U)));
				enemy.pos.velocity.y = word_add(enemy.pos.velocity.y,
				static_cast<std::int8_t>(instruction_byte(state, enemy, 5U)));
				frames = instruction_byte(state, enemy, 6U);
			} else {
				frames = instruction_byte(state, enemy, 4U);
			}
			if(enemy_pos_update(state, enemy)) return EnemyRunResult::killed;
			enemy.angle = byte_add(enemy.angle, enemy.angle_delta);
			return script_blocking(enemy, frames, length);
		}

		case 0x0DU:
		case 0x0EU: {
			const auto opcode = instruction_byte(state, enemy, 0U);
			const auto length = static_cast<std::uint16_t>(opcode == 0x0EU ? 4U : 2U);
			if(!instruction_available(state, enemy, length)) return EnemyRunResult::unsupported;
			enemy_velocity_set(enemy);
			std::uint8_t frames{};
			if(opcode == 0x0EU) {
				enemy.pos.velocity.x = word_add(enemy.pos.velocity.x,
				static_cast<std::int8_t>(instruction_byte(state, enemy, 1U)));
				enemy.pos.velocity.y = word_add(enemy.pos.velocity.y,
				static_cast<std::int8_t>(instruction_byte(state, enemy, 2U)));
				frames = instruction_byte(state, enemy, 3U);
			} else {
				frames = instruction_byte(state, enemy, 1U);
			}
			if(enemy_pos_update(state, enemy)) return EnemyRunResult::killed;
			enemy.angle = byte_add(enemy.angle, enemy.angle_delta);
			return script_blocking(enemy, frames, length);
		}

		case 0x06U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			if(enemy.cur_instr_frame == 0U) enemy.pos.prev = enemy.pos.cur;
			return script_blocking(enemy, instruction_byte(state, enemy, 1U), 2U);

		case 0x0BU:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			if(enemy.cur_instr_frame == 0U) enemy.pos.velocity.x = 0;
			enemy.pos.velocity.y = state.scroll_last_delta;
			if(enemy_pos_update(state, enemy)) return EnemyRunResult::killed;
			return script_blocking(enemy, instruction_byte(state, enemy, 1U), 2U);

		case 0x07U:
		case 0x08U: {
			if(!instruction_available(state, enemy, 5U)) return EnemyRunResult::unsupported;
			const auto opcode = instruction_byte(state, enemy, 0U);
			if(enemy.cur_instr_frame == 0U) {
				enemy.angle = 0;
				enemy.angle_delta = instruction_byte(state, enemy, 2U);
			}
			enemy.pos.velocity.x = pc98::masterlib::vector_component(
				static_cast<std::int16_t>(instruction_byte(state, enemy, 1U)),
				pc98::masterlib::cosine8(enemy.angle)
			);
			enemy.pos.velocity.y = static_cast<std::int8_t>(instruction_byte(state, enemy, 3U));
			if(opcode == 0x08U) {
				const auto temporary = enemy.pos.velocity.x;
				enemy.pos.velocity.x = enemy.pos.velocity.y;
				enemy.pos.velocity.y = temporary;
			}
			if(enemy_pos_update(state, enemy)) return EnemyRunResult::killed;
			enemy.angle = byte_add(enemy.angle, enemy.angle_delta);
			return script_blocking(enemy, instruction_byte(state, enemy, 4U), 5U);
		}

		case 0x09U:
			if(!instruction_available(state, enemy, 3U)) return EnemyRunResult::unsupported;
			enemy.angle = instruction_byte(state, enemy, 1U);
			enemy.speed = instruction_byte(state, enemy, 2U);
			if(!enemy_velocity_set_aimed(state, enemy)) return EnemyRunResult::unsupported;
			script_advance(enemy, 3U);
			break;

		case 0x0AU:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.angle = byte_add(enemy.angle, instruction_byte(state, enemy, 1U));
			enemy_velocity_set(enemy);
			script_advance(enemy, 2U);
			break;

		case 0x0CU:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.speed = word_add(enemy.speed,
				static_cast<std::int8_t>(instruction_byte(state, enemy, 1U)));
			enemy_velocity_set(enemy);
			script_advance(enemy, 2U);
			break;

		case 0x11U: {
			const auto angle = randring_next16(state.random);
			if(!angle) return EnemyRunResult::unsupported;
			enemy.angle = static_cast<std::uint8_t>(*angle);
			script_advance(enemy, 1U);
			break;
		}

		case 0x20U:
			script_bullet_template_push(state, enemy);
			if(!bullet_template_tune(state) || !bullets_add(state, false, false)) {
				return EnemyRunResult::unsupported;
			}
			script_advance(enemy, 1U);
			break;

		case 0x21U:
			if(!instruction_available(state, enemy, 11U)) return EnemyRunResult::unsupported;
			enemy.autofire = false;
			enemy.bullet_template.spawn_type = instruction_byte(state, enemy, 1U);
			enemy.bullet_template.origin.x = instruction_word(state, enemy, 2U);
			enemy.bullet_template.origin.y = instruction_word(state, enemy, 4U);
			enemy.bullet_template.group = static_cast<BulletGroup>(instruction_byte(state, enemy, 6U));
			enemy.bullet_template.angle = instruction_byte(state, enemy, 7U);
			enemy.bullet_template.speed = instruction_byte(state, enemy, 8U);
			enemy.bullet_template.patnum = instruction_byte(state, enemy, 9U);
			enemy.bullet_template.count = instruction_byte(state, enemy, 10U);
			script_advance(enemy, 11U);
			break;

		case 0x22U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.bullet_template.spawn_type = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x23U:
			if(!instruction_available(state, enemy, 5U)) return EnemyRunResult::unsupported;
			if(enemy.cur_instr_frame == 0U) enemy.pos.prev = enemy.pos.cur;
			enemy.bullet_template.origin.x = instruction_word(state, enemy, 1U);
			enemy.bullet_template.origin.y = instruction_word(state, enemy, 3U);
			script_advance(enemy, 5U);
			break;

		case 0x24U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.bullet_template.angle = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x25U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.bullet_template.angle = byte_add(
				enemy.bullet_template.angle, instruction_byte(state, enemy, 1U)
			);
			script_advance(enemy, 2U);
			break;

		case 0x2DU: {
			const auto angle = randring_next16(state.random);
			if(!angle) return EnemyRunResult::unsupported;
			enemy.bullet_template.angle = static_cast<std::uint8_t>(*angle);
			script_advance(enemy, 1U);
			break;
		}

		case 0x2AU:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.bullet_template.patnum = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x29U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.bullet_template.count = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x26U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.bullet_template.speed = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x27U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.bullet_template.speed = byte_add(
				enemy.bullet_template.speed, instruction_byte(state, enemy, 1U)
			);
			script_advance(enemy, 2U);
			break;

		case 0x28U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.bullet_template.group = static_cast<BulletGroup>(instruction_byte(state, enemy, 1U));
			script_advance(enemy, 2U);
			break;

		case 0x2CU: {
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			const auto operand = instruction_byte(state, enemy, 1U);
			int interval = operand;
			if(state.resources.playperf > 16U) {
				interval = static_cast<int>(state.resources.playperf - 16U) * interval;
				interval /= 32;
				interval = static_cast<int>(operand) - interval;
				if(interval < 16) interval = 16;
			} else if(state.resources.playperf < 16U) {
				interval = static_cast<int>(16U - state.resources.playperf) * interval;
				interval /= 32;
				interval = static_cast<int>(operand) + interval;
				if(interval >= 256) interval = 255;
			}
			if(state.rank == rank_easy) interval = 255;
			enemy.autofire_interval = static_cast<std::uint8_t>(interval);
			script_advance(enemy, 2U);
			break;
		}

		case 0x2BU:
			enemy.autofire = true;
			script_advance(enemy, 1U);
			break;

		case 0x2EU:
			enemy.autofire = false;
			script_advance(enemy, 1U);
			break;

		case 0x30U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.bullet_template.delta = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x00U:
			enemy.flag = enemy_killed;
			return EnemyRunResult::killed;

		case 0x10U:
			if(!instruction_available(state, enemy, 6U)) return EnemyRunResult::unsupported;
			enemy.flag = enemy_alive;
			enemy.patnum_base = instruction_byte(state, enemy, 1U);
			enemy.hp = instruction_word(state, enemy, 2U);
			enemy.score = instruction_word(state, enemy, 4U);
			enemy.can_be_damaged = true;
			enemy.kills_player_on_collision = true;
			script_advance(enemy, 6U);
			break;

		case 0x82U:
			enemy.clip_x = true;
			script_advance(enemy, 1U);
			break;

		case 0x84U:
			enemy.clip_x = true;
			[[fallthrough]];
		case 0x83U:
			enemy.clip_y = true;
			script_advance(enemy, 1U);
			break;

		case 0x85U:
			if(!instruction_available(state, enemy, 3U)) return EnemyRunResult::unsupported;
			enemy.anim_cels = instruction_byte(state, enemy, 1U);
			enemy.anim_frames_per_cel = instruction_byte(state, enemy, 2U);
			script_advance(enemy, 3U);
			break;

		case 0x86U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			presentation.sound(instruction_byte(state, enemy, 1U));
			script_advance(enemy, 2U);
			break;

		case 0x87U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.patnum_base = instruction_byte(state, enemy, 1U);
			script_advance(enemy, 2U);
			break;

		case 0x88U:
			enemy.can_be_damaged = false;
			enemy.autofire = false;
			script_advance(enemy, 1U);
			break;

		case 0x89U:
			enemy.can_be_damaged = true;
			enemy.autofire = state.rank == rank_lunatic;
			script_advance(enemy, 1U);
			break;

		case 0x8CU:
			enemy.kills_player_on_collision = false;
			script_advance(enemy, 1U);
			break;

		case 0x8DU:
			enemy.kills_player_on_collision = true;
			script_advance(enemy, 1U);
			break;

		case 0x8AU:
			if(!instruction_available(state, enemy, 5U)) return EnemyRunResult::unsupported;
			enemy.pos.prev = enemy.pos.cur;
			enemy.pos.cur.x = instruction_word(state, enemy, 1U);
			enemy.pos.cur.y = instruction_word(state, enemy, 3U);
			return script_blocking(enemy, 0U, 5U);

		case 0x8BU:
			if(!instruction_available(state, enemy, 5U)) return EnemyRunResult::unsupported;
			enemy.pos.prev = enemy.pos.cur;
			enemy.pos.cur.x = word_add(enemy.pos.cur.x, instruction_word(state, enemy, 1U));
			enemy.pos.cur.y = word_add(enemy.pos.cur.y, instruction_word(state, enemy, 3U));
			return script_blocking(enemy, 0U, 5U);

		case 0x8EU:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.patnum_base = byte_add(enemy.patnum_base, instruction_byte(state, enemy, 1U));
			script_advance(enemy, 2U);
			break;

		case 0x12U:
			if(!instruction_available(state, enemy, 3U)) return EnemyRunResult::unsupported;
			enemy.angle = instruction_byte(state, enemy, 1U);
			enemy.speed = instruction_byte(state, enemy, 2U);
			enemy_velocity_set(enemy);
			script_advance(enemy, 3U);
			break;

		case 0x13U:
			if(!instruction_available(state, enemy, 3U)) return EnemyRunResult::unsupported;
			enemy.angle = instruction_byte(state, enemy, 1U);
			enemy.speed = instruction_byte(state, enemy, 2U);
			if(!enemy.spawned_in_left_half) {
				enemy.angle = static_cast<std::uint8_t>(0x80U - enemy.angle);
			}
			enemy_velocity_set(enemy);
			script_advance(enemy, 3U);
			break;

		case 0x14U:
			if(!instruction_available(state, enemy, 2U)) return EnemyRunResult::unsupported;
			enemy.speed = instruction_byte(state, enemy, 1U);
			enemy_velocity_set(enemy);
			script_advance(enemy, 2U);
			break;

		case 0x8FU:
			if(!instruction_available(state, enemy, 2U) ||
				!tile_ring_set_vo(state, enemy, instruction_byte(state, enemy, 1U))) {
				return EnemyRunResult::unsupported;
			}
			script_advance(enemy, 2U);
			break;

		case 0x80U:
		case 0x81U:
			if(!instruction_available(state, enemy, 3U)) return EnemyRunResult::unsupported;
			if(enemy.loop_i >= instruction_byte(state, enemy, 2U)) {
				enemy.loop_i = 0;
				script_advance(enemy, 3U);
				break;
			}
			enemy.loop_i = static_cast<std::uint8_t>(enemy.loop_i + 1U);
			if(instruction_byte(state, enemy, 0U) == 0x80U) {
				enemy.script_ip = instruction_byte(state, enemy, 1U);
			} else {
				enemy.script_ip = static_cast<std::uint16_t>(
					enemy.script_ip - instruction_byte(state, enemy, 1U)
				);
			}
			break;

		default:
			return EnemyRunResult::unsupported;
		}
	}
	return EnemyRunResult::unsupported;
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
	items_add(state, presentation, enemy.pos.cur,
		static_cast<ItemType>(static_cast<std::int8_t>(enemy.item)));
	presentation.sound(3U);
	state.score.score_delta = static_cast<std::uint32_t>(
		state.score.score_delta + std::bit_cast<std::uint16_t>(enemy.score)
	);
	const auto sparks = pc98::th0405::sparks_add_random(
		pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset}, state.random,
		enemy.pos.cur, 64, 8U
	);
	if(!sparks) return false;
	++state.enemies.gone;
	++state.enemies.killed;
	return true;
}

[[nodiscard]] bool enemy_autofire(State &state, Enemy &enemy) noexcept
{
	if(!enemy.autofire) return true;
	enemy.autofire_cur_frame = static_cast<std::uint8_t>(enemy.autofire_cur_frame + 1U);
	if(enemy.autofire_cur_frame < enemy.autofire_interval || enemy.pos.cur.y >= 4864 ||
		overlap_fast(enemy.pos.cur, state.player_pos.cur, 1536U, 1536U)) {
		return true;
	}
	enemy.autofire_cur_frame = 0;
	autofire_bullet_template_push(state, enemy);
	return bullet_template_tune(state) && bullets_add(state, false, false);
}

} // namespace

bool enemies_add(
	State &state, std::uint16_t script_id, std::int16_t center_x,
	std::int16_t center_y, std::uint8_t item
) noexcept
{
	for(auto &enemy : state.enemies.enemies) {
		if(enemy.flag != enemy_free) continue;
		enemy.flag = enemy_alive_first_frame;
		enemy.cur_instr_frame = 0;
		enemy.loop_i = 0;
		enemy.age = 0;
		enemy.script_ip = 0;
		if(script_id >= state.std_data.enemy_scripts.size()) return false;
		enemy.script = state.std_data.enemy_scripts[script_id];
		if(center_x == enemy_random_position) {
			const auto random_x = randring_next16_mod(state.random, playfield_width);
			if(!random_x) return false;
			center_x = word_from(*random_x);
		}
		if(center_y == enemy_random_position) {
			const auto random_y = randring_next16_mod(state.random, playfield_height);
			if(!random_y) return false;
			center_y = word_from(*random_y);
		}
		enemy.pos.cur = {center_x, center_y};
		enemy.item = item;
		enemy.damaged_this_frame = false;
		enemy.autofire = state.rank == rank_lunatic;
		enemy.clip_x = false;
		enemy.clip_y = false;
		enemy.anim_cels = 1U;
		enemy.anim_frames_per_cel = 4U;
		enemy.anim_cur_cel = 0U;
		enemy.can_be_damaged = false;
		enemy.kills_player_on_collision = false;
		enemy.spawned_in_left_half = center_x < 3072;
		const auto autofire_frame = randring_next16(state.random);
		if(!autofire_frame) return false;
		enemy.autofire_cur_frame = static_cast<std::uint8_t>(*autofire_frame);
		enemy.autofire_interval = 128U;
		enemy.bullet_template.group = BulletGroup::forcesingle_aimed;
		enemy.bullet_template.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		enemy.bullet_template.speed = 42U;
		enemy.bullet_template.origin = {};
		break;
	}
	return true;
}

bool enemies_update(State &state, Presentation &presentation) noexcept
{
	state.reimu_a.homing_target = {homing_target_none, homing_target_none};
	state.shot_collision.radius = {256, 192};
	for(auto &enemy : state.enemies.enemies) {
		if(enemy.flag == enemy_free) continue;
		if(enemy.flag == enemy_killed) {
			enemy.flag = enemy_free;
			continue;
		}
		if(enemy.flag < enemy_kill_anim) {
			if(enemy_run(state, presentation, enemy) == EnemyRunResult::unsupported) return false;
			if(enemy.kills_player_on_collision && overlap_fast(
			enemy.pos.cur, state.player_pos.cur, 384U, 384U
			)) {
				state.player_is_hit = true;
				if(!kill_enemy(state, presentation, enemy)) return false;
				continue;
			}
			if(enemy.can_be_damaged && enemy.hp != -1 &&
				static_cast<std::uint16_t>(
					static_cast<std::uint16_t>(std::bit_cast<std::uint16_t>(enemy.pos.cur.x)) + 256U
				) < 6656U &&
				static_cast<std::uint16_t>(
					static_cast<std::uint16_t>(std::bit_cast<std::uint16_t>(enemy.pos.cur.y)) + 256U
				) < 6144U
			) {
				if(enemy.pos.cur.y > state.reimu_a.homing_target.y &&
					enemy.pos.cur.y <= state.player_pos.cur.y) {
					state.reimu_a.homing_target = enemy.pos.cur;
				}
				state.shot_collision.center = enemy.pos.cur;
				state.shot_collision.bombing = state.bombing;
				state.shot_collision.against_boss = false;
				state.shot_collision.stage_frame_mod2 = static_cast<std::uint8_t>(state.stage_frame & 1U);
				state.shot_collision.stage_frame_mod4 = static_cast<std::uint8_t>(state.stage_frame & 3U);
				state.shot_collision.score_delta = state.score.score_delta;
				const auto hit = shots_hittest(
					state.shots, state.shot_collision,
					pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset}, state.random
				);
				if(!hit) return false;
				state.score.score_delta = state.shot_collision.score_delta;
				const auto damage = static_cast<std::uint8_t>(hit->damage);
				if(damage != 0U) {
					if(enemy.hp != -2) {
						if(static_cast<int>(damage) < static_cast<int>(enemy.hp)) {
							enemy.hp = static_cast<std::int16_t>(
							static_cast<int>(enemy.hp) - static_cast<int>(damage)
						);
							enemy.damaged_this_frame = true;
						} else if(!kill_enemy(state, presentation, enemy)) {
							return false;
						} else {
							continue;
						}
					} else {
						presentation.sound(10U);
					}
				}
			}
			if(!enemy_autofire(state, enemy)) return false;
			enemy.age = static_cast<std::uint8_t>(enemy.age + 1U);
		} else {
			pc98::th0405::update_motion(enemy.pos);
			const auto patnum = static_cast<std::uint8_t>(enemy.flag + 1U);
			enemy.flag = patnum;
			enemy.patnum_base = static_cast<std::uint8_t>(
				((static_cast<std::uint16_t>(patnum) - enemy_kill_anim) / 4U) + pat_enemy_kill
			);
			if(enemy.patnum_base >= static_cast<std::uint8_t>(pat_enemy_kill + enemy_kill_cels)) {
				enemy.flag = enemy_killed;
			}
		}
	}
	return true;
}

} // namespace th04::core
