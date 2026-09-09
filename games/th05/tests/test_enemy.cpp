#include "th05/core/enemy.hpp"
#include "th05/core/shot_collision.hpp"
#include "th05/core/state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
using namespace th05::core;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	std::vector<HudField> hud_fields{};
	std::vector<Point> splashes{};

	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
	void hud(HudField field) noexcept override { hud_fields.push_back(field); }
	void item_splash(Point point) noexcept override { splashes.push_back(point); }
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.rank = 1U;
	state.resources.playperf = 32U;
	state.resources.playperf_max = 48U;
	state.playchar = static_cast<std::uint8_t>(ShotPlaychar::reimu);
	state.player_pos.cur = {3000, 5000};
	return state;
}

[[nodiscard]] bool std_spawn_and_blocking_movement()
{
	auto state = source_state();
	Trace presentation{};
	state.std_data.ip = 0U;
	state.std_data.enemy_scripts[0] = 8U;
	state.std_data.data = {
		0U, 0xE8U, 0x03U, 0xD0U, 0x07U,
		static_cast<std::uint8_t>(ItemType::point), 9U, 0U,
		0x01U, 7U, 10U, 0U, 0x34U, 0x12U,
		0x02U, 0xC0U, 32U, 1U,
		0x00U,
	};
	state.random.ring[0] = 0x5AU;
	if(!enemies_add(state)) return false;
	auto &enemy = state.enemies.enemies[0];
	if(enemy.flag != enemy_alive_first_frame || enemy.pos.cur != Point{1000, 2000} ||
		enemy.item != static_cast<std::uint8_t>(ItemType::point) || enemy.subtype != 9U ||
		enemy.bullet_template.group != static_cast<std::uint8_t>(BulletGroup::forcesingle_aimed) ||
		enemy.bullet_template.speed != 42U || state.random.cursor != 1U) return false;
	if(!enemies_update(state, presentation) || enemy.flag != enemy_alive || enemy.hp != 10 ||
		enemy.score != 0x1234 || enemy.pos.cur != Point{1000, 1968} ||
		enemy.cur_instr_frame != 1U || enemy.script_ip != 6U || enemy.age != 1U) return false;
	if(!enemies_update(state, presentation) || enemy.pos.cur != Point{1000, 1936} ||
		enemy.cur_instr_frame != 0U || enemy.script_ip != 10U || enemy.age != 2U) return false;
	if(!enemies_update(state, presentation) || enemy.flag != enemy_killed || enemy.age != 3U) return false;
	if(!enemies_update(state, presentation) || enemy.flag != enemy_free) return false;

	state = source_state();
	state.std_data.ip = 0U;
	state.std_data.data = {0U, 0U, 0U, 0U, 0U, 0U, 0U};
	return !enemies_add(state);
}

[[nodiscard]] bool script_bullets_subtype_and_random_spawn()
{
	auto state = source_state();
	Trace presentation{};
	state.std_data.ip = 0U;
	state.std_data.enemy_scripts[0] = 8U;
	state.std_data.data = {
		0U, 0x70U, 0x3EU, 0x70U, 0x3EU, 0U, 7U, 0U,
		0x34U, 7U, 0U,
		0x12U, bullet_spawn_type_normal, static_cast<std::uint8_t>(BulletGroup::single),
		0xC0U, 42U, 0U,
		0x11U, 0x09U, 0U,
	};
	state.random.ring[0] = 100U;
	state.random.ring[1] = 0U;
	state.random.ring[2] = 0U;
	state.random.ring[3] = 0U;
	state.random.ring[4] = 0xA5U;
	if(!enemies_add(state)) return false;
	auto &enemy = state.enemies.enemies[0];
	if(enemy.pos.cur != Point{100, 0} || !enemy.spawned_in_left_half ||
		state.random.cursor != 3U) return false;
	if(!enemies_update(state, presentation) || enemy.patnum_base != 0U ||
		enemy.flag != enemy_alive_first_frame || enemy.script_ip != 12U) return false;
	std::size_t active{};
	Point bullet_position{};
	for(const auto &bullet : state.bullets.pool.pellets) {
		if(bullet.flag == bullet_alive) {
			++active;
			bullet_position = bullet.pos.cur;
		}
	}
	return active == 1U && bullet_position == Point{100, 0};
}

[[nodiscard]] bool collision_drop_score_sparks_and_dream()
{
	auto state = source_state();
	Trace presentation{};
	state.std_data.data = {0x09U, 0U};
	auto &enemy = state.enemies.enemies[0];
	enemy.flag = enemy_alive;
	enemy.script = 0U;
	enemy.pos.cur = {2000, 1000};
	enemy.can_be_damaged = true;
	enemy.hp = 5;
	enemy.score = 100;
	enemy.item = static_cast<std::uint8_t>(ItemType::point);
	state.shots.shots[0].flag = shot_alive;
	state.shots.shots[0].damage = 5;
	state.shots.shots[0].pos.cur = enemy.pos.cur;
	state.shots.shots_alive[0] = {enemy.pos.cur, 0U};
	state.shots.shots_alive_count = 1U;
	for(std::size_t index = 0U; index < 16U; ++index) {
		state.random.ring[index] = static_cast<std::uint8_t>(index);
	}
	if(!enemies_update(state, presentation) || enemy.flag != enemy_kill_anim ||
		state.enemies.gone != 1U || state.enemies.killed != 1U ||
		state.score.score_delta != 105U || state.items.items_spawned != 1U ||
		state.items.items[0].type != static_cast<std::uint8_t>(ItemType::point) ||
		state.items.items[0].pos.cur != Point{2000, 1000} ||
		state.random.cursor != 8U || state.spark_ring_offset != 128U ||
		state.items.dream != 1U || state.shots.homing_target != Point{2000, 1000}) return false;
	return presentation.sounds == std::vector<std::uint8_t>{3U} &&
		presentation.splashes == std::vector<Point>{Point{2000, 1000}} &&
		presentation.hud_fields == std::vector<HudField>{HudField::dream};
}

[[nodiscard]] bool autofire_and_source_byte_damage()
{
	auto state = source_state();
	Trace presentation{};
	state.std_data.data = {0x09U, 1U};
	auto &enemy = state.enemies.enemies[0];
	enemy.flag = enemy_alive;
	enemy.script = 0U;
	enemy.pos.cur = {1000, 1000};
	enemy.autofire = true;
	enemy.autofire_cur_frame = 127U;
	enemy.autofire_interval = 128U;
	enemy.bullet_template.spawn_type = bullet_spawn_type_normal;
	enemy.bullet_template.group = static_cast<std::uint8_t>(BulletGroup::single);
	enemy.bullet_template.angle = 0xC0U;
	enemy.bullet_template.speed = 42U;
	if(!enemies_update(state, presentation) || enemy.autofire_cur_frame != 0U) return false;
	std::size_t active{};
	for(const auto &bullet : state.bullets.pool.pellets) {
		active += bullet.flag == bullet_alive ? 1U : 0U;
	}
	if(active != 1U) return false;

	state = source_state();
	state.std_data.data = {0x09U, 0U};
	auto &damage_target = state.enemies.enemies[0];
	damage_target.flag = enemy_alive;
	damage_target.script = 0U;
	damage_target.pos.cur = {2000, 1000};
	damage_target.can_be_damaged = true;
	damage_target.hp = 10;
	for(std::size_t index = 0U; index < 4U; ++index) {
		state.shots.shots[index].flag = shot_alive;
		state.shots.shots[index].damage = 127;
		state.shots.shots_alive[index] = {damage_target.pos.cur,
			static_cast<std::uint16_t>(index)};
	}
	state.shots.shots_alive_count = 4U;
	if(!enemies_update(state, presentation)) return false;
	// The source stores the hittest accumulator's AL before comparing HP.
	return damage_target.flag == enemy_alive && damage_target.hp == 3 &&
		damage_target.damaged_this_frame && state.score.score_delta == 263U;
}

[[nodiscard]] bool clip_opcodes_keep_source_order()
{
	const auto check = [](std::uint8_t opcode, std::uint8_t expected) noexcept {
		auto state = source_state();
		Trace presentation{};
		state.std_data.data = {opcode, 0x09U, 0U};
		auto &enemy = state.enemies.enemies[0];
		enemy.flag = enemy_alive;
		enemy.script = 0U;
		return enemies_update(state, presentation) && enemy.clip == expected &&
			enemy.script_ip == 3U;
	};
	return check(0x22U, 0x01U) && check(0x23U, 0x10U) && check(0x24U, 0x11U);
}

} // namespace

int main()
{
	const std::array checks{
		std_spawn_and_blocking_movement(),
		script_bullets_subtype_and_random_spawn(),
		collision_drop_score_sparks_and_dream(),
		autofire_and_source_byte_damage(),
		clip_opcodes_keep_source_order(),
	};
	for(std::size_t index = 0U; index < checks.size(); ++index) {
		if(!checks[index]) {
			std::cerr << "TH05 enemy check failed: " << index << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
