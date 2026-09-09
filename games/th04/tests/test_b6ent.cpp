#include "th04/core/b6ent.hpp"
#include "th04/core/state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using namespace th04::core;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};

	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.rank = 1U;
	state.resources.playperf = 16U;
	state.resources.playperf_max = 32U;
	state.boss.pos.cur = {3072, 1024};
	state.player_pos.cur = {3072, 5000};
	return state;
}

[[nodiscard]] std::size_t bullets_alive(const State &state, bool pellet) noexcept
{
	const auto first = pellet ? 0U : pellet_count;
	const auto last = pellet ? pellet_count : bullet_count;
	std::size_t count{};
	for(std::size_t index = first; index < last; ++index) {
		if(state.bullets.pool[index].flag == bullet_alive) ++count;
	}
	return count;
}

[[nodiscard]] bool chasecross_spawn_and_final_slot_alias()
{
	auto state = source_state();
	auto &first = state.yuuka6_customs.chasecrosses[0];
	first.velocity = {17, -29};
	first.unused_1 = {1, 2, 3, 4};
	first.unused_2 = {5, 6, 7, 8};
	yuuka6_chasecrosses_add(state, 0x51U, 0x24U);
	if(first.flag != Yuuka6ChasecrossFlag::alive || first.angle != 0x51U ||
		first.speed != 0x24U || first.hp != 100 || first.damage_this_frame != 0 ||
		first.age != 0U || first.center != state.boss.pos.cur ||
		first.velocity != Yuuka6CustomPoint{17, -29} ||
		first.unused_1 != std::array<std::int8_t, 4U>{1, 2, 3, 4} ||
		first.unused_2 != std::array<std::int8_t, 4U>{5, 6, 7, 8}) return false;

	for(auto &cross : state.yuuka6_customs.chasecrosses) {
		cross.flag = Yuuka6ChasecrossFlag::alive;
	}
	auto &circle = state.yuuka6_customs.safetycircle;
	circle.flag = Yuuka6SafetyCircleFlag::free;
	circle.unused_2 = {9, 8, 7, 6, 5, 4, 3, 2};
	circle.radius_filled = 71;
	yuuka6_chasecrosses_add(state, 0xA2U, 0x35U);
	return circle.flag == Yuuka6SafetyCircleFlag::grow &&
		static_cast<std::uint8_t>(circle.unused_1) == 0xA2U &&
		circle.center == Yuuka6ScreenPoint{3072, 1024} &&
		circle.shrink_frame == 0U &&
		circle.unused_2 == std::array<std::int8_t, 8U>{9, 8, 7, 6, 5, 4, 3, 2} &&
		circle.radius_filled == 71 &&
		circle.unused_3 == std::array<std::int8_t, 4U>{100, 0, 0, 0} &&
		circle.color_ring == 0x35U;
}

[[nodiscard]] bool chasecross_update_collision_and_clip_tail()
{
	{
		auto state = source_state();
		Trace presentation{};
		auto &cross = state.yuuka6_customs.chasecrosses[0];
		cross.flag = Yuuka6ChasecrossFlag::alive;
		cross.center = {3000, 2000};
		cross.angle = 0U;
		cross.speed = 0U;
		cross.age = 55U;
		cross.hp = 1;
		state.player_pos.cur = cross.center;
		state.shots.shots[0].flag = shot_alive;
		state.shots.shots[0].pos.cur = cross.center;
		state.shots.shots[0].damage = 1;
		state.shots.shots_alive[0] = {cross.center, 0U};
		state.shots.shots_alive_count = 1U;
		if(!yuuka6_customs_update(state, presentation)) return false;
		if(!state.player_is_hit || cross.flag != Yuuka6ChasecrossFlag::kill_anim ||
			cross.angle != 1U || cross.age != 1U || cross.hp != 0 ||
			cross.damage_this_frame != 1 || state.score.score_delta != 3001U ||
			state.shots.shots[0].flag != shot_hit ||
			state.items.items[0].flag != item_alive ||
			state.items.items[0].type != static_cast<std::uint8_t>(ItemType::big_power) ||
			presentation.sounds != std::vector<std::uint8_t>{4U, 3U}) return false;
		cross.flag = static_cast<Yuuka6ChasecrossFlag>(47U);
		cross.damage_this_frame = 9;
		yuuka6_customs_render_tail(state);
		if(cross.flag != Yuuka6ChasecrossFlag::free || cross.damage_this_frame != 9) {
			return false;
		}
	}
	{
		auto state = source_state();
		Trace presentation{};
		auto &cross = state.yuuka6_customs.chasecrosses[0];
		cross.flag = Yuuka6ChasecrossFlag::alive;
		cross.center = {-256, 2000};
		cross.angle = 0U;
		cross.speed = 0U;
		cross.age = 56U;
		cross.hp = 100;
		cross.damage_this_frame = 7;
		state.player_pos.cur = cross.center;
		if(!yuuka6_customs_update(state, presentation)) return false;
		if(cross.flag != Yuuka6ChasecrossFlag::free || cross.age != 57U ||
			cross.damage_this_frame != 0 || !state.player_is_hit ||
			state.shot_collision.radius != Yuuka6CustomPoint{192, 192}) return false;
	}
	return true;
}

[[nodiscard]] bool safetycircle_update_and_bullet_bursts()
{
	auto state = source_state();
	Trace presentation{};
	state.player_pos.cur = {1234, 2345};
	yuuka6_safetycircle_open(state, presentation);
	auto &circle = state.yuuka6_customs.safetycircle;
	if(circle.flag != Yuuka6SafetyCircleFlag::grow ||
		circle.center != Yuuka6ScreenPoint{109, 162} ||
		circle.radius_filled != 8 || circle.radius_ring_distance != 80 ||
		presentation.sounds != std::vector<std::uint8_t>{8U}) return false;
	if(!yuuka6_customs_update(state, presentation) || circle.radius_filled != 16) {
		return false;
	}
	circle.radius_filled = 136;
	if(!yuuka6_customs_update(state, presentation) ||
		circle.flag != Yuuka6SafetyCircleFlag::shrink) return false;
	circle.shrink_frame = 0U;
	circle.radius_filled = 100;
	circle.radius_ring_distance = 80;
	if(!yuuka6_customs_update(state, presentation) || circle.shrink_frame != 1U ||
		circle.radius_ring_distance != 72) return false;
	circle.shrink_frame = 8U;
	if(!yuuka6_customs_update(state, presentation) || circle.shrink_frame != 9U ||
		circle.color_ring != 9U) return false;

	circle.shrink_frame = 32U;
	circle.radius_filled = 100;
	circle.radius_ring_distance = 20;
	state.stage_frame = 10U;
	state.boss.pos.cur = {3072, 1024};
	state.player_pos.cur = {6000, 6000};
	if(!yuuka6_customs_update(state, presentation)) return false;
	if(circle.shrink_frame != 33U || circle.radius_filled != 99 ||
		circle.radius_ring_distance != 21 || circle.color_ring != 9U ||
		bullets_alive(state, false) != 32U || bullets_alive(state, true) != 84U ||
		state.bullets.template_.spawn_type != static_cast<std::uint8_t>(
			BulletSpawnType::pellet
		) || state.bullets.template_.group != BulletGroup::spread ||
		state.bullets.template_.count != 4U || state.bullets.template_.speed != 48U ||
		state.bullets.template_.delta != 12U ||
		presentation.sounds != std::vector<std::uint8_t>{8U, 3U}) return false;

	circle.flag = Yuuka6SafetyCircleFlag::shrink;
	circle.shrink_frame = 176U;
	const auto before = circle;
	yuuka6_customs_render_tail(state);
	if(circle != before) return false;
	if(!yuuka6_customs_update(state, presentation) ||
		circle.flag != Yuuka6SafetyCircleFlag::free || circle.shrink_frame != 177U) {
		return false;
	}
	return true;
}

} // namespace

int main()
{
	const std::array checks{
		chasecross_spawn_and_final_slot_alias(),
		chasecross_update_collision_and_clip_tail(),
		safetycircle_update_and_bullet_bursts(),
	};
	for(std::size_t index = 0U; index < checks.size(); ++index) {
		if(!checks[index]) {
			std::cerr << "TH04 Yuuka6 custom-entity check failed: " << index << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
