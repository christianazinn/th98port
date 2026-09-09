#include "th05/core/shot_patterns.hpp"

#include "th05/core/state.hpp"

#include <cstdint>

namespace th05::core {
namespace {

using u8 = std::uint8_t;

constexpr std::int16_t option_distance = 24 * 16;
constexpr std::int16_t offset_8 = 8 * 16;
constexpr u8 pat_shot_sub = 22U;

constexpr u8 byte(int value) noexcept
{
	return static_cast<u8>(value);
}

void add_x(Shot &shot, std::int16_t delta) noexcept
{
	shot.pos.cur.x = detail::word_add(shot.pos.cur.x, delta);
}

void from_option_l(Shot &shot, std::int16_t extra = 0) noexcept
{
	add_x(shot, static_cast<std::int16_t>(-option_distance - extra));
}

void from_option_r(Shot &shot, std::int16_t extra = 0) noexcept
{
	add_x(shot, static_cast<std::int16_t>(option_distance + extra));
}

void set_option_sprite_and_damage(Shot &shot, std::int8_t damage) noexcept
{
	shot.patnum_base = pat_shot_sub;
	shot.damage = damage;
}

void set_homing(Shot &shot, std::int8_t damage) noexcept
{
	set_option_sprite_and_damage(shot, damage);
	shot.type = shot_homing;
}

void velocity_x(Shot &shot, std::int16_t value) noexcept
{
	shot.pos.velocity.x = value;
}

void velocity_y(Shot &shot, std::int16_t value) noexcept
{
	shot.pos.velocity.y = value;
}

[[nodiscard]] bool random_angle(State &state, u8 min, u8 max, u8 &angle) noexcept
{
	const auto width = static_cast<u8>(max - min);
	const auto random = randring_next16_and(
		state.random, static_cast<std::uint16_t>(width - 1U)
	);
	if(!random) return false;
	angle = static_cast<u8>(min + static_cast<u8>(*random));
	return true;
}

[[nodiscard]] u8 shot_cycle(State &state, Presentation &presentation) noexcept
{
	auto sound = [&presentation](u8 id) noexcept { presentation.sound(id); };
	return shot_cycle_init(state.shot_spawn, sound).flags;
}

[[nodiscard]] bool next_shot(State &state, Shot *&shot) noexcept
{
	const auto result = shots_add(state.shot_spawn, state.shots);
	if(result.status == ShotAddStatus::invalid_cursor) return false;
	shot = result.shot;
	return true;
}

[[nodiscard]] bool shot_l0_fire(State &state, Presentation &presentation) noexcept
{
	auto sound = [&presentation](u8 id) noexcept { presentation.sound(id); };
	const auto result = shot_l0(state.shot_spawn, state.shots, sound);
	return result.allocation.status != ShotAddStatus::invalid_cursor;
}

[[nodiscard]] bool shot_l1_fire(State &state, Presentation &presentation) noexcept
{
	auto sound = [&presentation](u8 id) noexcept { presentation.sound(id); };
	const auto result = shot_l1(state.shot_spawn, state.shots, state.random, sound);
	return result.allocation.status != ShotAddStatus::invalid_cursor && !result.random_refused;
}

[[nodiscard]] bool reimu_l2(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_1x)) == 0U) return true;
	u8 i = 1U;
	if((cycle & shot_cycle_1x) != 0U) i = static_cast<u8>(i + 2U);
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i == 1U) {
			u8 angle{};
			if(!random_angle(state, byte(-0x48), byte(-0x38), angle)) return false;
			shot_velocity_set(*shot, angle);
			shot->damage = 10;
		} else {
			const auto left = i == 3U;
			if(left) from_option_l(*shot); else from_option_r(*shot);
			shot_velocity_set(*shot, left ? byte(-0x48) : byte(-0x38));
			set_homing(*shot, 4);
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool reimu_l3(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_1x)) == 0U) return true;
	u8 i = 2U;
	if((cycle & shot_cycle_1x) != 0U) i = static_cast<u8>(i + 2U);
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 2U) {
			add_x(*shot, i == 2U ? static_cast<std::int16_t>(-offset_8) : offset_8);
			shot->damage = 9;
		} else {
			const auto left = i == 4U;
			if(left) from_option_l(*shot); else from_option_r(*shot);
			shot_velocity_set(*shot, left ? byte(-0x48) : byte(-0x38));
			set_homing(*shot, 4);
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool reimu_l4(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_1x)) == 0U) return true;
	u8 i = 3U;
	if((cycle & shot_cycle_1x) != 0U) i = static_cast<u8>(i + 2U);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 3U) {
			if(i == 3U) angle = byte(-0x4C);
			shot->damage = 9;
			angle = static_cast<u8>(angle + 0x06U);
		} else {
			const auto left = i == 5U;
			if(left) from_option_l(*shot); else from_option_r(*shot);
			angle = left ? byte(-0x48) : byte(-0x38);
			set_homing(*shot, 3);
		}
		shot_velocity_set(*shot, angle);
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool reimu_l5_or_l6(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_2x)) == 0U) return true;
	u8 i = 3U;
	if((cycle & shot_cycle_2x) != 0U) i = static_cast<u8>(i + 2U);
	const auto option_only = static_cast<u8>(cycle & shot_cycle_3x);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 3U) {
			shot->damage = 9;
			if(i == 3U) angle = byte(-0x4E);
			angle = static_cast<u8>(angle + 0x07U);
		} else {
			const auto left = i == 5U;
			if(left) {
				from_option_l(*shot);
				if(!random_angle(state, byte(-0x4A), byte(-0x46), angle)) return false;
			} else {
				from_option_r(*shot);
				if(!random_angle(state, byte(-0x3A), byte(-0x36), angle)) return false;
				if(option_only == 0U) i = 1U;
			}
			set_homing(*shot, 3);
		}
		shot_velocity_set(*shot, angle);
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool reimu_l7(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_2x)) == 0U) return true;
	u8 i = 5U;
	if((cycle & shot_cycle_2x) != 0U) i = static_cast<u8>(i + 2U);
	const auto option_only = static_cast<u8>(cycle & shot_cycle_3x);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 5U) {
			if(i == 5U) angle = byte(-0x4F);
			shot->damage = 9;
			angle = static_cast<u8>(angle + 0x05U);
		} else {
			const auto left = i == 7U;
			if(left) {
				from_option_l(*shot);
				if(!random_angle(state, byte(-0x4A), byte(-0x46), angle)) return false;
			} else {
				from_option_r(*shot);
				if(!random_angle(state, byte(-0x3A), byte(-0x36), angle)) return false;
				if(option_only == 0U) i = 1U;
			}
			set_homing(*shot, 3);
		}
		shot_velocity_set(*shot, angle);
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool reimu_l8(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & shot_cycle_3x) == 0U) return true;
	u8 i = 7U;
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 5U) {
			if(i == 5U) angle = byte(-0x4F);
			shot->damage = 9;
			angle = static_cast<u8>(angle + 0x05U);
		} else {
			const auto left = i == 7U;
			if(left) {
				from_option_l(*shot);
				if(!random_angle(state, byte(-0x4A), byte(-0x46), angle)) return false;
			} else {
				from_option_r(*shot);
				if(!random_angle(state, byte(-0x3A), byte(-0x36), angle)) return false;
			}
			set_homing(*shot, 3);
		}
		shot_velocity_set(*shot, angle);
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool reimu_l9(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_2x)) == 0U) return true;
	u8 i = 7U;
	if((cycle & shot_cycle_2x) != 0U) i = static_cast<u8>(i + 2U);
	const auto option_only = static_cast<u8>(cycle & shot_cycle_3x);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 5U) {
			if(i == 5U) angle = byte(-0x4F);
			shot->damage = 9;
			angle = static_cast<u8>(angle + 0x05U);
		} else if(i <= 7U) {
			const auto left = i == 7U;
			if(left) from_option_l(*shot); else from_option_r(*shot);
			angle = left ? byte(-0x48) : byte(-0x38);
			set_homing(*shot, 2);
		} else {
			const auto left = i == 9U;
			if(left) {
				from_option_l(*shot);
				angle = byte(-0x56);
			} else {
				from_option_r(*shot);
				angle = byte(-0x2A);
				if(option_only == 0U) i = 1U;
			}
			set_homing(*shot, 2);
		}
		shot_velocity_set(*shot, angle);
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

void marisa_missile(Shot &shot, u8 slot, std::int8_t damage) noexcept
{
	switch(slot) {
	case 0U: from_option_l(shot); shot.type = shot_missile_left; velocity_x(shot, -22); velocity_y(shot, 36); break;
	case 1U: from_option_r(shot); shot.type = shot_missile_right; velocity_x(shot, 22); velocity_y(shot, 36); break;
	case 2U: from_option_l(shot); shot.type = shot_missile_straight; velocity_x(shot, -16); velocity_y(shot, -48); break;
	case 3U: from_option_r(shot); shot.type = shot_missile_straight; velocity_x(shot, 16); velocity_y(shot, -48); break;
	case 4U: from_option_l(shot); shot.type = shot_missile_straight; velocity_x(shot, 0); velocity_y(shot, -64); break;
	default: from_option_r(shot); shot.type = shot_missile_straight; velocity_x(shot, 0); velocity_y(shot, -64); break;
	}
	set_option_sprite_and_damage(shot, damage);
}

[[nodiscard]] bool marisa_l2(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_1x)) == 0U) return true;
	u8 i = 1U;
	if((cycle & shot_cycle_1x) != 0U) i = static_cast<u8>(i + 2U);
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i == 1U) {
			u8 angle{};
			if(!random_angle(state, byte(-0x48), byte(-0x38), angle)) return false;
			shot_velocity_set(*shot, angle);
			shot->damage = 8;
		} else {
			if(i == 3U) {
				from_option_l(*shot);
				shot->type = shot_missile_left;
				velocity_x(*shot, -26);
			} else {
				from_option_r(*shot);
				shot->type = shot_missile_right;
				velocity_x(*shot, 26);
			}
			velocity_y(*shot, 32);
			set_option_sprite_and_damage(*shot, 16);
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool marisa_l3(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_1x)) == 0U) return true;
	u8 i = 2U;
	if((cycle & shot_cycle_1x) != 0U) i = static_cast<u8>(i + 4U);
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 2U) {
			add_x(*shot, i == 2U ? static_cast<std::int16_t>(-offset_8) : offset_8);
			shot->damage = 7;
		} else {
			marisa_missile(*shot, static_cast<u8>(i - 3U), 13);
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool marisa_l4_or_l5(State &state, Presentation &presentation, bool level_five) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_1x)) == 0U) return true;
	u8 i = 3U;
	if((cycle & shot_cycle_1x) != 0U) i = static_cast<u8>(i + 6U);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 3U) {
			if(i == 3U) angle = level_five ? byte(-0x4E) : byte(-0x4C);
			shot->damage = 6;
			angle = static_cast<u8>(angle + (level_five ? 0x07U : 0x06U));
			shot_velocity_set(*shot, angle);
		} else {
			const auto slot = static_cast<u8>(i - 4U);
			marisa_missile(*shot, slot, level_five ? 11 : 12);
			if(slot == 4U || slot == 5U) {
				velocity_x(*shot, level_five ? (slot == 4U ? 8 : -8) : 0);
			}
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

void marisa_l6_missile(Shot &shot, u8 slot) noexcept
{
	switch(slot) {
	case 0U: from_option_l(shot); shot.type = shot_missile_left; velocity_x(shot, -22); velocity_y(shot, 36); break;
	case 1U: from_option_r(shot); shot.type = shot_missile_right; velocity_x(shot, 22); velocity_y(shot, 36); break;
	case 2U: from_option_l(shot); shot.type = shot_missile_straight; velocity_x(shot, -16); velocity_y(shot, -48); break;
	case 3U: from_option_r(shot); shot.type = shot_missile_straight; velocity_x(shot, 16); velocity_y(shot, -48); break;
	case 4U: from_option_l(shot); shot.type = shot_missile_straight; velocity_y(shot, -80); break;
	case 5U: from_option_r(shot); shot.type = shot_missile_straight; velocity_y(shot, -80); break;
	case 6U: from_option_l(shot); shot.type = shot_missile_straight; velocity_y(shot, -32); break;
	default: from_option_r(shot); shot.type = shot_missile_straight; velocity_y(shot, -32); break;
	}
	set_option_sprite_and_damage(shot, 11);
}

[[nodiscard]] bool marisa_l6(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_1x)) == 0U) return true;
	u8 i = 4U;
	if((cycle & shot_cycle_1x) != 0U) i = static_cast<u8>(i + 8U);
	std::int16_t star_distance = -24 * 16;
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 4U) {
			add_x(*shot, star_distance);
			shot->damage = 5;
			velocity_y(*shot, -192);
			star_distance = detail::word_add(star_distance, 16 * 16);
		} else {
			marisa_l6_missile(*shot, static_cast<u8>(i - 5U));
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

void marisa_l7_missile(Shot &shot, u8 slot, std::int8_t damage) noexcept
{
	switch(slot) {
	case 0U: from_option_l(shot); shot.type = shot_missile_left; velocity_x(shot, -28); velocity_y(shot, 36); break;
	case 1U: from_option_r(shot); shot.type = shot_missile_right; velocity_x(shot, 28); velocity_y(shot, 36); break;
	case 2U: from_option_l(shot); shot.type = shot_missile_right; velocity_x(shot, 32); velocity_y(shot, 32); break;
	case 3U: from_option_r(shot); shot.type = shot_missile_left; velocity_x(shot, -32); velocity_y(shot, 32); break;
	case 4U: from_option_l(shot); shot.type = shot_missile_straight; velocity_y(shot, 48); velocity_x(shot, -16); break;
	case 5U: from_option_r(shot); shot.type = shot_missile_straight; velocity_y(shot, 48); velocity_x(shot, 16); break;
	case 6U: from_option_l(shot); shot.type = shot_missile_straight; velocity_y(shot, -80); break;
	case 7U: from_option_r(shot); shot.type = shot_missile_straight; velocity_y(shot, -80); break;
	case 8U: from_option_l(shot); shot.type = shot_missile_straight; velocity_y(shot, -32); break;
	default: from_option_r(shot); shot.type = shot_missile_straight; velocity_y(shot, -32); break;
	}
	set_option_sprite_and_damage(shot, damage);
}

[[nodiscard]] bool marisa_l7_or_l8(State &state, Presentation &presentation, bool level_eight) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_1x)) == 0U) return true;
	u8 i = 4U;
	if((cycle & shot_cycle_1x) != 0U) i = static_cast<u8>(i + 10U);
	std::int16_t star_distance = -24 * 16;
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 4U) {
			add_x(*shot, star_distance);
			shot->damage = level_eight ? 4 : 5;
			velocity_y(*shot, -192);
			star_distance = detail::word_add(star_distance, 16 * 16);
		} else {
			marisa_l7_missile(*shot, static_cast<u8>(i - 5U), level_eight ? 10 : 11);
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

void marisa_l9_missile(Shot &shot, u8 slot) noexcept
{
	switch(slot) {
	case 0U: from_option_l(shot); shot.type = shot_missile_left; velocity_x(shot, -32); velocity_y(shot, 36); break;
	case 1U: from_option_r(shot); shot.type = shot_missile_right; velocity_x(shot, 32); velocity_y(shot, 36); break;
	case 2U: from_option_l(shot); shot.type = shot_missile_right; velocity_x(shot, 32); velocity_y(shot, 32); break;
	case 3U: from_option_r(shot); shot.type = shot_missile_left; velocity_x(shot, -32); velocity_y(shot, 32); break;
	case 4U: from_option_l(shot); shot.type = shot_missile_straight; velocity_y(shot, 48); break;
	case 5U: from_option_r(shot); shot.type = shot_missile_straight; velocity_y(shot, 48); break;
	case 6U: from_option_l(shot); shot.type = shot_missile_straight; velocity_y(shot, 0); velocity_x(shot, -16); break;
	case 7U: from_option_r(shot); shot.type = shot_missile_straight; velocity_y(shot, 0); velocity_x(shot, 16); break;
	case 8U: from_option_l(shot); shot.type = shot_missile_straight; velocity_y(shot, -128); break;
	case 9U: from_option_r(shot); shot.type = shot_missile_straight; velocity_y(shot, -128); break;
	case 10U: from_option_l(shot); shot.type = shot_missile_straight; velocity_y(shot, -64); break;
	case 11U: from_option_r(shot); shot.type = shot_missile_straight; velocity_y(shot, -64); break;
	case 12U: from_option_l(shot); shot.type = shot_missile_straight; velocity_y(shot, -16); break;
	default: from_option_r(shot); shot.type = shot_missile_straight; velocity_y(shot, -16); break;
	}
	set_option_sprite_and_damage(shot, 9);
}

[[nodiscard]] bool marisa_l9(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_1x)) == 0U) return true;
	u8 i = 6U;
	if((cycle & shot_cycle_1x) != 0U) i = static_cast<u8>(i + 12U);
	std::int16_t star_distance = -40 * 16;
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 6U) {
			add_x(*shot, star_distance);
			shot->damage = 4;
			velocity_y(*shot, -192);
			star_distance = detail::word_add(star_distance, 16 * 16);
		} else {
			marisa_l9_missile(*shot, static_cast<u8>(i - 7U));
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

void mima_option_l2_or_l3(Shot &shot, u8 i) noexcept
{
	if(i == 2U) from_option_l(shot); else from_option_r(shot);
	velocity_y(shot, -320);
	set_option_sprite_and_damage(shot, 5);
}

[[nodiscard]] bool mima_l2(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_6x | shot_cycle_3x)) == 0U) return true;
	u8 i = 2U;
	if((cycle & shot_cycle_3x) != 0U) i = static_cast<u8>(i + 1U);
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 2U) {
			mima_option_l2_or_l3(*shot, i);
		} else {
			u8 angle{};
			if(!random_angle(state, byte(-0x48), byte(-0x38), angle)) return false;
			shot_velocity_set(*shot, angle);
			shot->damage = 8;
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool mima_l3(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_6x | shot_cycle_3x)) == 0U) return true;
	u8 i = 2U;
	if((cycle & shot_cycle_3x) != 0U) i = static_cast<u8>(i + 2U);
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 2U) {
			mima_option_l2_or_l3(*shot, i);
		} else {
			add_x(*shot, i == 4U ? static_cast<std::int16_t>(-offset_8) : offset_8);
			velocity_y(*shot, -192);
			shot->damage = 7;
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

void mima_l4_option(Shot &shot, u8 i) noexcept
{
	switch(static_cast<u8>(i - 1U)) {
	case 0U: velocity_x(shot, 16); from_option_r(shot); break;
	case 1U: velocity_x(shot, -16); from_option_l(shot); break;
	case 2U: from_option_r(shot); break;
	default: from_option_l(shot); break;
	}
	velocity_y(shot, -320);
	set_option_sprite_and_damage(shot, 4);
}

[[nodiscard]] bool mima_l4(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_6x | shot_cycle_3x)) == 0U) return true;
	u8 i = 4U;
	if((cycle & shot_cycle_3x) != 0U) i = static_cast<u8>(i + 2U);
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 4U) {
			mima_l4_option(*shot, i);
		} else {
			add_x(*shot, i == 6U ? static_cast<std::int16_t>(-offset_8) : offset_8);
			velocity_y(*shot, -192);
			shot->damage = 7;
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

void mima_l5_option(Shot &shot, u8 i) noexcept
{
	switch(static_cast<u8>(i - 1U)) {
	case 0U: velocity_x(shot, 16); from_option_l(shot); break;
	case 1U: velocity_x(shot, -16); from_option_r(shot); break;
	case 2U: from_option_r(shot); break;
	default: from_option_l(shot); break;
	}
	velocity_y(shot, -320);
	set_option_sprite_and_damage(shot, 4);
}

[[nodiscard]] bool mima_l5_or_l6(State &state, Presentation &presentation, bool level_six) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_6x | shot_cycle_3x)) == 0U) return true;
	u8 i = 4U;
	if((cycle & shot_cycle_3x) != 0U) i = static_cast<u8>(i + 3U);
	u8 angle = byte(-0x46);
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 4U) {
			if(!level_six) {
				mima_l5_option(*shot, i);
			} else {
				switch(static_cast<u8>(i - 1U)) {
				case 0U: velocity_x(*shot, 16); from_option_r(*shot); break;
				case 1U: velocity_x(*shot, -16); from_option_l(*shot); break;
				case 2U: velocity_x(*shot, -8); from_option_r(*shot); break;
				default: velocity_x(*shot, 8); from_option_l(*shot); break;
				}
				velocity_y(*shot, -320);
				set_option_sprite_and_damage(*shot, 4);
			}
		} else {
			shot_velocity_set(*shot, angle);
			angle = static_cast<u8>(angle + 0x06U);
			shot->damage = 7;
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

void mima_l7_or_l8_option(Shot &shot, u8 i, bool level_eight) noexcept
{
	if(level_eight) velocity_y(shot, -320);
	switch(static_cast<u8>(i - 1U)) {
	case 0U:
		velocity_x(shot, level_eight ? 28 : 16);
		if(level_eight) velocity_y(shot, -288);
		from_option_r(shot, level_eight ? 8 * 16 : 4 * 16);
		break;
	case 1U:
		velocity_x(shot, level_eight ? -28 : -16);
		if(level_eight) velocity_y(shot, -288);
		from_option_l(shot, level_eight ? 8 * 16 : 4 * 16);
		break;
	case 2U: velocity_x(shot, -8); from_option_r(shot); break;
	case 3U: velocity_x(shot, 8); from_option_l(shot); break;
	case 4U: from_option_r(shot); break;
	default: from_option_l(shot); break;
	}
	velocity_y(shot, -320);
	set_option_sprite_and_damage(shot, 4);
}

[[nodiscard]] bool mima_l7_or_l8(State &state, Presentation &presentation, bool level_eight) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_6x | shot_cycle_3x)) == 0U) return true;
	u8 i = 6U;
	if((cycle & shot_cycle_3x) != 0U) i = static_cast<u8>(i + 3U);
	u8 angle = byte(-0x46);
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 6U) {
			mima_l7_or_l8_option(*shot, i, level_eight);
		} else {
			shot_velocity_set(*shot, angle);
			angle = static_cast<u8>(angle + 0x06U);
			shot->damage = 7;
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

void mima_l9_option(Shot &shot, u8 i) noexcept
{
	velocity_y(shot, -320);
	switch(static_cast<u8>(i - 1U)) {
	case 0U: velocity_x(shot, 42); velocity_y(shot, -288); from_option_r(shot, 8 * 16); break;
	case 1U: velocity_x(shot, -42); velocity_y(shot, -288); from_option_l(shot, 8 * 16); break;
	case 2U: velocity_x(shot, -8); from_option_r(shot); break;
	case 3U: velocity_x(shot, 8); from_option_l(shot); break;
	case 4U: velocity_x(shot, 8); from_option_r(shot); break;
	default: velocity_x(shot, -8); from_option_l(shot); break;
	}
	set_option_sprite_and_damage(shot, 4);
}

[[nodiscard]] bool mima_l9(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_6x | shot_cycle_3x)) == 0U) return true;
	u8 i = 6U;
	if((cycle & shot_cycle_3x) != 0U) i = static_cast<u8>(i + 4U);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 6U) {
			mima_l9_option(*shot, i);
		} else {
			switch(static_cast<u8>(i - 7U)) {
			case 0U: angle = byte(-0x48); break;
			case 1U: angle = byte(-0x40); add_x(*shot, offset_8); break;
			case 2U: angle = byte(-0x40); add_x(*shot, static_cast<std::int16_t>(-offset_8)); break;
			default: angle = byte(-0x38); break;
			}
			shot_velocity_set(*shot, angle);
			shot->damage = 7;
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool yuuka_l2(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_1x)) == 0U) return true;
	u8 i = 1U;
	if((cycle & shot_cycle_1x) != 0U) i = static_cast<u8>(i + 2U);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i == 1U) {
			if(!random_angle(state, byte(-0x48), byte(-0x38), angle)) return false;
			shot->damage = 9;
		} else {
			const auto left = i == 3U;
			if(left) from_option_l(*shot); else from_option_r(*shot);
			angle = left ? byte(-0x48) : byte(-0x38);
			set_option_sprite_and_damage(*shot, 6);
		}
		shot_velocity_set(*shot, angle);
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool yuuka_l3(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_2x)) == 0U) return true;
	u8 i = 2U;
	if((cycle & shot_cycle_2x) != 0U) i = static_cast<u8>(i + 2U);
	const auto option_only = static_cast<u8>(cycle & shot_cycle_3x);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 2U) {
			add_x(*shot, i == 2U ? static_cast<std::int16_t>(-offset_8) : offset_8);
			shot->damage = 8;
		} else {
			const auto left = i == 4U;
			if(left) {
				from_option_l(*shot);
				angle = byte(-0x48);
			} else {
				from_option_r(*shot);
				angle = byte(-0x38);
				if(option_only == 0U) i = 1U;
			}
			set_option_sprite_and_damage(*shot, 6);
			shot_velocity_set(*shot, angle);
		}
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool yuuka_l4(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_2x)) == 0U) return true;
	u8 i = 3U;
	if((cycle & shot_cycle_2x) != 0U) i = static_cast<u8>(i + 2U);
	const auto option_only = static_cast<u8>(cycle & shot_cycle_3x);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 3U) {
			if(i == 3U) angle = byte(-0x4C);
			shot->damage = 7;
			angle = static_cast<u8>(angle + 0x06U);
		} else {
			const auto left = i == 5U;
			if(left) {
				from_option_l(*shot);
				if(!random_angle(state, byte(-0x4C), byte(-0x44), angle)) return false;
			} else {
				from_option_r(*shot);
				if(!random_angle(state, byte(-0x3C), byte(-0x34), angle)) return false;
				if(option_only == 0U) i = 1U;
			}
			velocity_y(*shot, 32);
			set_option_sprite_and_damage(*shot, 6);
		}
		shot_velocity_set(*shot, angle);
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool yuuka_l5(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_2x)) == 0U) return true;
	u8 i = 3U;
	if((cycle & shot_cycle_2x) != 0U) i = static_cast<u8>(i + 4U);
	const auto option_only = static_cast<u8>(cycle & shot_cycle_3x);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 3U) {
			shot->damage = 7;
			if(i == 3U) angle = byte(-0x4C);
			angle = static_cast<u8>(angle + 0x06U);
		} else {
			switch(static_cast<u8>(i - 4U)) {
			case 0U:
				if(option_only == 0U) i = 1U;
				from_option_l(*shot);
				angle = byte(-0x4E);
				break;
			case 1U: from_option_r(*shot); angle = byte(-0x32); break;
			case 2U: from_option_l(*shot); angle = byte(-0x46); break;
			default: from_option_r(*shot); angle = byte(-0x3A); break;
			}
			set_option_sprite_and_damage(*shot, 6);
		}
		shot_velocity_set(*shot, angle);
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

void yuuka_inner_l4(Shot &shot, u8 i, u8 &angle) noexcept
{
	switch(static_cast<u8>(i - 1U)) {
	case 0U: angle = byte(-0x46); break;
	case 1U: angle = byte(-0x40); add_x(shot, static_cast<std::int16_t>(-offset_8)); break;
	case 2U: angle = byte(-0x40); add_x(shot, offset_8); break;
	default: angle = byte(-0x3A); break;
	}
	shot.damage = 7;
}

void yuuka_inner_l5(Shot &shot, u8 i, u8 &angle) noexcept
{
	switch(static_cast<u8>(i - 1U)) {
	case 0U: angle = byte(-0x4C); break;
	case 1U: angle = byte(-0x46); break;
	case 2U: angle = byte(-0x40); break;
	case 3U: angle = byte(-0x3A); break;
	default: angle = byte(-0x34); break;
	}
	shot.damage = 7;
}

[[nodiscard]] bool yuuka_l6_or_l7(State &state, Presentation &presentation, bool level_seven) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_2x)) == 0U) return true;
	u8 i = 4U;
	if((cycle & shot_cycle_2x) != 0U) i = static_cast<u8>(i + (level_seven ? 6U : 4U));
	const auto option_only = static_cast<u8>(cycle & shot_cycle_3x);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 4U) {
			yuuka_inner_l4(*shot, i, angle);
		} else {
			const auto slot = static_cast<u8>(i - 5U);
			switch(slot) {
			case 0U:
				from_option_l(*shot);
				angle = byte(-0x50);
				if(option_only == 0U) i = 1U;
				break;
			case 1U: from_option_r(*shot); angle = byte(-0x30); break;
			case 2U: from_option_l(*shot); angle = level_seven ? byte(-0x49) : byte(-0x48); break;
			case 3U: from_option_r(*shot); angle = level_seven ? byte(-0x37) : byte(-0x38); break;
			case 4U: from_option_l(*shot); angle = byte(-0x42); break;
			default: from_option_r(*shot); angle = byte(-0x3E); break;
			}
			set_option_sprite_and_damage(*shot, 6);
		}
		shot_velocity_set(*shot, angle);
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool yuuka_l8(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_2x)) == 0U) return true;
	u8 i = 5U;
	if((cycle & shot_cycle_2x) != 0U) i = static_cast<u8>(i + 6U);
	const auto option_only = static_cast<u8>(cycle & shot_cycle_3x);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 5U) {
			yuuka_inner_l5(*shot, i, angle);
		} else {
			switch(static_cast<u8>(i - 6U)) {
			case 0U: from_option_l(*shot); angle = byte(-0x50); if(option_only == 0U) i = 1U; break;
			case 1U: from_option_r(*shot); angle = byte(-0x30); break;
			case 2U: from_option_l(*shot); angle = byte(-0x49); break;
			case 3U: from_option_r(*shot); angle = byte(-0x37); break;
			case 4U: from_option_l(*shot); angle = byte(-0x42); break;
			default: from_option_r(*shot); angle = byte(-0x3E); break;
			}
			set_option_sprite_and_damage(*shot, 6);
		}
		shot_velocity_set(*shot, angle);
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool yuuka_l9(State &state, Presentation &presentation) noexcept
{
	const auto cycle = shot_cycle(state, presentation);
	if((cycle & static_cast<u8>(shot_cycle_3x | shot_cycle_2x)) == 0U) return true;
	u8 i = 5U;
	if((cycle & shot_cycle_2x) != 0U) i = static_cast<u8>(i + 8U);
	const auto option_only = static_cast<u8>(cycle & shot_cycle_3x);
	u8 angle{};
	for(;;) {
		Shot *shot{};
		if(!next_shot(state, shot)) return false;
		if(shot == nullptr) return true;
		if(i <= 5U) {
			yuuka_inner_l5(*shot, i, angle);
		} else {
			switch(static_cast<u8>(i - 6U)) {
			case 0U: from_option_l(*shot); angle = byte(-0x58); if(option_only == 0U) i = 1U; break;
			case 1U: from_option_r(*shot); angle = byte(-0x28); break;
			case 2U: from_option_l(*shot); angle = byte(-0x50); break;
			case 3U: from_option_r(*shot); angle = byte(-0x30); break;
			case 4U: from_option_l(*shot); angle = byte(-0x48); break;
			case 5U: from_option_r(*shot); angle = byte(-0x38); break;
			case 6U: from_option_l(*shot); angle = byte(-0x3F); break;
			default: from_option_r(*shot); angle = byte(-0x41); break;
			}
			set_option_sprite_and_damage(*shot, 6);
		}
		shot_velocity_set(*shot, angle);
		i = static_cast<u8>(i - 1U);
		if(i == 0U) return true;
	}
}

[[nodiscard]] bool dispatch_reimu(State &state, Presentation &presentation) noexcept
{
	switch(state.shot_level) {
	case 0U: return shot_l0_fire(state, presentation);
	case 1U: return shot_l1_fire(state, presentation);
	case 2U: return reimu_l2(state, presentation);
	case 3U: return reimu_l3(state, presentation);
	case 4U: return reimu_l4(state, presentation);
	case 5U: return reimu_l5_or_l6(state, presentation);
	case 6U: return reimu_l5_or_l6(state, presentation);
	case 7U: return reimu_l7(state, presentation);
	case 8U: return reimu_l8(state, presentation);
	case 9U: return reimu_l9(state, presentation);
	default: return false;
	}
}

[[nodiscard]] bool dispatch_marisa(State &state, Presentation &presentation) noexcept
{
	switch(state.shot_level) {
	case 0U: return shot_l0_fire(state, presentation);
	case 1U: return shot_l1_fire(state, presentation);
	case 2U: return marisa_l2(state, presentation);
	case 3U: return marisa_l3(state, presentation);
	case 4U: return marisa_l4_or_l5(state, presentation, false);
	case 5U: return marisa_l4_or_l5(state, presentation, true);
	case 6U: return marisa_l6(state, presentation);
	case 7U: return marisa_l7_or_l8(state, presentation, false);
	case 8U: return marisa_l7_or_l8(state, presentation, true);
	case 9U: return marisa_l9(state, presentation);
	default: return false;
	}
}

[[nodiscard]] bool dispatch_mima(State &state, Presentation &presentation) noexcept
{
	switch(state.shot_level) {
	case 0U: return shot_l0_fire(state, presentation);
	case 1U: return shot_l1_fire(state, presentation);
	case 2U: return mima_l2(state, presentation);
	case 3U: return mima_l3(state, presentation);
	case 4U: return mima_l4(state, presentation);
	case 5U: return mima_l5_or_l6(state, presentation, false);
	case 6U: return mima_l5_or_l6(state, presentation, true);
	case 7U: return mima_l7_or_l8(state, presentation, false);
	case 8U: return mima_l7_or_l8(state, presentation, true);
	case 9U: return mima_l9(state, presentation);
	default: return false;
	}
}

[[nodiscard]] bool dispatch_yuuka(State &state, Presentation &presentation) noexcept
{
	switch(state.shot_level) {
	case 0U: return shot_l0_fire(state, presentation);
	case 1U: return shot_l1_fire(state, presentation);
	case 2U: return yuuka_l2(state, presentation);
	case 3U: return yuuka_l3(state, presentation);
	case 4U: return yuuka_l4(state, presentation);
	case 5U: return yuuka_l5(state, presentation);
	case 6U: return yuuka_l6_or_l7(state, presentation, false);
	case 7U: return yuuka_l6_or_l7(state, presentation, true);
	case 8U: return yuuka_l8(state, presentation);
	case 9U: return yuuka_l9(state, presentation);
	default: return false;
	}
}

} // namespace

bool player_shots_fire(State &state, Presentation &presentation) noexcept
{
	if(state.playchar >= 4U || state.shot_level >= 10U) return false;
	// shots_add reads player_pos.cur directly in the source rather than either
	// rendered option position.
	state.shot_spawn.player_cur = state.player_pos.cur;
	switch(state.playchar) {
	case 0U: return dispatch_reimu(state, presentation);
	case 1U: return dispatch_marisa(state, presentation);
	case 2U: return dispatch_mima(state, presentation);
	case 3U: return dispatch_yuuka(state, presentation);
	default: return false;
	}
}

} // namespace th05::core
