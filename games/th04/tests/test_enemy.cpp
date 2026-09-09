#include "th04/core/enemy.hpp"
#include "th04/core/state.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
using namespace th04::core;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	std::vector<Point> splashes{};

	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
	void item_splash(Point point) noexcept override { splashes.push_back(point); }
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.rank = 1U;
	state.resources.playperf = 16U;
	state.resources.playperf_max = 32U;
	state.player_pos.cur = {3000, 5000};
	return state;
}

[[nodiscard]] bool add_and_blocking_movement()
{
	auto state = source_state();
	Trace presentation{};
	state.std_data.enemy_scripts[0] = 0U;
	state.std_data.data = {
		0x10U, 7U, 10U, 0U, 0x34U, 0x12U,
		0x12U, 0xC0U, 32U,
		0x01U, 0xC0U, 32U, 1U,
		0x00U,
	};
	if(!enemies_add(state, 0U, 1000, 2000,
		static_cast<std::uint8_t>(ItemType::point))) return false;
	auto &enemy = state.enemies.enemies[0];
	if(enemy.flag != enemy_alive_first_frame || enemy.pos.cur != Point{1000, 2000} ||
		enemy.bullet_template.group != BulletGroup::forcesingle_aimed ||
		enemy.bullet_template.speed != 42U || state.random.cursor != 1U) return false;
	if(!enemies_update(state, presentation) || enemy.flag != enemy_alive || enemy.hp != 10 ||
		enemy.score != 0x1234 || enemy.pos.cur != Point{1000, 1968} ||
		enemy.cur_instr_frame != 1U || enemy.script_ip != 9U || enemy.age != 1U) return false;
	if(!enemies_update(state, presentation) || enemy.pos.cur != Point{1000, 1936} ||
		enemy.cur_instr_frame != 0U || enemy.script_ip != 13U || enemy.age != 2U) return false;
	if(!enemies_update(state, presentation) || enemy.flag != enemy_killed || enemy.age != 3U ||
		!presentation.sounds.empty()) return false;
	if(!enemies_update(state, presentation) || enemy.flag != enemy_free) return false;

	state = source_state();
	state.enemies.enemies[0].script = 0xBEEFU;
	if(enemies_add(state, 32U, 1000, 2000, 0U)) return false;
	const auto &prefix = state.enemies.enemies[0];
	return prefix.flag == enemy_alive_first_frame && prefix.age == 0U &&
		prefix.cur_instr_frame == 0U && prefix.loop_i == 0U && prefix.script_ip == 0U &&
		prefix.script == 0xBEEFU;
}

[[nodiscard]] bool shot_damage_item_score_and_sparks()
{
	auto state = source_state();
	Trace presentation{};
	state.std_data.enemy_scripts[0] = 0U;
	state.std_data.data = {0x06U, 0U};
	auto &enemy = state.enemies.enemies[0];
	enemy.flag = enemy_alive;
	enemy.pos.cur = {2000, 1000};
	enemy.can_be_damaged = true;
	enemy.hp = 5;
	enemy.score = 100;
	enemy.item = static_cast<std::uint8_t>(ItemType::point);
	enemy.script = 0U;
	state.shots.shots[0].flag = shot_alive;
	state.shots.shots[0].damage = 5;
	state.shots.shots[0].pos.cur = enemy.pos.cur;
	state.shots.shots_alive[0] = {enemy.pos.cur, 0U};
	state.shots.shots_alive_count = 1U;
	if(!enemies_update(state, presentation) || enemy.flag != enemy_kill_anim ||
		state.enemies.gone != 1U || state.enemies.killed != 1U ||
		state.score.score_delta != 105U || state.random.cursor != 9U ||
		state.spark_ring_offset != 144U) return false;
	if(state.items.items_spawned != 1U || state.items.items[0].flag != item_alive ||
		state.items.items[0].type != static_cast<std::uint8_t>(ItemType::point) ||
		state.items.items[0].pos.cur != enemy.pos.cur ||
		presentation.sounds != std::vector<std::uint8_t>{3U} ||
		presentation.splashes != std::vector<Point>{enemy.pos.cur}) return false;
	return state.sparks[0].center.velocity == Point{128, 0} &&
		state.sparks[1].center.velocity == Point{64, 0};
}

[[nodiscard]] bool hit_damage_is_a_source_byte()
{
	auto state = source_state();
	Trace presentation{};
	state.std_data.enemy_scripts[0] = 0U;
	state.std_data.data = {0x06U, 0U};
	auto &enemy = state.enemies.enemies[0];
	enemy.flag = enemy_alive;
	enemy.pos.cur = {2000, 1000};
	enemy.can_be_damaged = true;
	enemy.hp = 10;
	enemy.script = 0U;
	for(std::size_t i = 0; i < 4U; ++i) {
		state.shots.shots[i].flag = shot_alive;
		state.shots.shots[i].damage = 127;
		state.shots.shots[i].pos.cur = enemy.pos.cur;
		state.shots.shots_alive[i] = {enemy.pos.cur, static_cast<std::uint16_t>(i)};
	}
	state.shots.shots_alive_count = 4U;
	if(!enemies_update(state, presentation)) return false;
	// 127 + 127/2 + 127/3 + 127/4 is 263, but enemy_update stores AL.
	return enemy.flag == enemy_alive && enemy.hp == 3 && enemy.damaged_this_frame &&
		state.score.score_delta == 263U;
}

[[nodiscard]] bool player_collision_and_bullet_paths()
{
	{
		auto state = source_state();
		Trace presentation{};
		state.std_data.enemy_scripts[0] = 0U;
		state.std_data.data = {0x06U, 0U};
		auto &enemy = state.enemies.enemies[0];
		enemy.flag = enemy_alive;
		enemy.pos.cur = state.player_pos.cur;
		enemy.kills_player_on_collision = true;
		enemy.item = static_cast<std::uint8_t>(ItemType::power);
		enemy.script = 0U;
		if(!enemies_update(state, presentation) || !state.player_is_hit ||
			enemy.flag != enemy_kill_anim || state.enemies.killed != 1U ||
			presentation.sounds != std::vector<std::uint8_t>{3U}) return false;
	}

	{
		auto state = source_state();
		Trace presentation{};
		state.player_pos.cur = {3000, 4000};
		state.std_data.enemy_scripts[0] = 0U;
		state.std_data.data = {
			0x21U, static_cast<std::uint8_t>(BulletSpawnType::pellet),
			16U, 0U, 0xE0U, 0xFFU, static_cast<std::uint8_t>(BulletGroup::forcesingle),
			0xC0U, 42U, 8U, 0U,
			0x20U, 0x06U, 0U,
		};
		state.bullets.template_.special_motion = BulletSpecialMotion::none;
		if(!enemies_add(state, 0U, 1000, 1000, 0U) ||
			!enemies_update(state, presentation)) return false;
		const auto &bullet = state.bullets.pool[239];
		if(bullet.flag != bullet_alive || bullet.pos.cur != Point{1016, 968} ||
			bullet.patnum != 8 || bullet.from_group != BulletGroup::forcesingle ||
			state.enemies.enemies[0].autofire ||
			state.bullets.template_.special_motion != BulletSpecialMotion::none) return false;
	}

	{
		auto state = source_state();
		Trace presentation{};
		state.player_pos.cur = {3000, 4000};
		state.std_data.enemy_scripts[0] = 0U;
		state.std_data.data = {0x06U, 1U};
		auto &enemy = state.enemies.enemies[0];
		enemy.flag = enemy_alive;
		enemy.script = 0U;
		enemy.pos.cur = {1000, 1000};
		enemy.autofire = true;
		enemy.autofire_cur_frame = 127U;
		enemy.autofire_interval = 128U;
		enemy.bullet_template.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		enemy.bullet_template.patnum = 9U;
		enemy.bullet_template.origin = {};
		enemy.bullet_template.velocity = {13, -9};
		enemy.bullet_template.group = BulletGroup::forcesingle;
		enemy.bullet_template.angle = 0xC0U;
		enemy.bullet_template.speed = 42U;
		enemy.bullet_template.special_motion = BulletSpecialMotion::gravity;
		if(!enemies_update(state, presentation) || enemy.autofire_cur_frame != 0U ||
			state.bullets.pool[239].flag != bullet_alive ||
			state.bullets.pool[239].pos.cur != enemy.pos.cur ||
			state.bullets.template_.special_motion != BulletSpecialMotion::gravity) return false;
	}
	return true;
}

} // namespace

int main()
{
	const std::array checks{
		add_and_blocking_movement(), shot_damage_item_score_and_sparks(),
		hit_damage_is_a_source_byte(), player_collision_and_bullet_paths(),
	};
	for(std::size_t index = 0; index < checks.size(); ++index) {
		if(!checks[index]) {
			std::cerr << "TH04 enemy check failed: " << index << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
