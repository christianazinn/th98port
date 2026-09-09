#include "th02/core/enemy_update.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_top = 16;
constexpr std::int16_t playfield_right = 416;
constexpr std::int16_t playfield_bottom = 384;
constexpr std::int16_t playfield_height = 400;
constexpr std::int16_t enemy_extent = 32;
constexpr std::int16_t enemy_kill_frames = 24;
constexpr std::int16_t enemy_kill_cel_frames = 3;
constexpr std::int16_t enemy_kill_spark_speed = 20;
constexpr std::int16_t enemy_kill_spark_count = 8;
constexpr std::int16_t lunatic_death_pellet_speed = 60;
constexpr std::int8_t rank_lunatic = 3;

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

[[nodiscard]] std::int32_t add_dword(
	std::int32_t lhs, std::int32_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint32_t>(
		static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(lhs)) +
		static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(rhs))
	);
	return std::bit_cast<std::int32_t>(bits);
}

[[nodiscard]] EnemyUpdateResult map_run_result(EnemyRunResult result) noexcept
{
	switch(result) {
	case EnemyRunResult::frame_consumed:
	case EnemyRunResult::continue_same_frame:
	case EnemyRunResult::finished:
		return EnemyUpdateResult::completed;
	case EnemyRunResult::invalid_page:
		return EnemyUpdateResult::invalid_page;
	case EnemyRunResult::invalid_script_operand:
		return EnemyUpdateResult::invalid_script;
	case EnemyRunResult::unsupported_opcode:
		return EnemyUpdateResult::unsupported_opcode;
	case EnemyRunResult::source_divide_fault:
		return EnemyUpdateResult::source_divide_fault;
	case EnemyRunResult::source_iatan2_fault:
		return EnemyUpdateResult::source_iatan2_fault;
	}
	return EnemyUpdateResult::unsupported_opcode;
}

[[nodiscard]] EnemyUpdateResult map_bullet_result(
	EnemyBulletAddResult result
) noexcept
{
	switch(result) {
	case EnemyBulletAddResult::completed:
		return EnemyUpdateResult::completed;
	case EnemyBulletAddResult::invalid_page:
		return EnemyUpdateResult::invalid_page;
	case EnemyBulletAddResult::source_iatan2_fault:
		return EnemyUpdateResult::source_iatan2_fault;
	}
	return EnemyUpdateResult::source_iatan2_fault;
}

[[nodiscard]] std::int16_t scrolled_top(
	std::int16_t top, std::int16_t scroll_line, bool wrap_negative
) noexcept
{
	top = add_word(top, scroll_line);
	if(top >= playfield_height) top = add_word(top, -playfield_height);
	if(wrap_negative && top < 0) top = add_word(top, playfield_height);
	return top;
}

[[nodiscard]] EnemyUpdateResult add_pellet(
	const EnemyTemplate &enemy_template, EnemyRunContext &run,
	ScreenPoint position, std::uint8_t angle,
	std::uint8_t group, std::int16_t speed
) noexcept
{
	return map_bullet_result(bullets_add_pellet(
		run.bullets, run.random, run.player_frame, run.playperf, run.rank,
		run.bombing,
		{add_word(enemy_template.bullet_origin_x, position.x),
			add_word(enemy_template.bullet_origin_y, position.y)},
		angle, group, speed
	));
}

[[nodiscard]] EnemyUpdateResult render_enemy(
	Enemy &enemy, const EnemyTemplate &enemy_template, ScreenPoint position,
	EnemyRunContext &run, EnemyUpdateCallbacks &callbacks
) noexcept
{
	if(enemy_template.autofire_interval > 1 &&
		(static_cast<std::int16_t>(enemy.anim_frame) %
			enemy_template.autofire_interval) == 0) {
		const auto fired = add_pellet(enemy_template, run, position, 0U,
			enemy.pellet_group, static_cast<std::int16_t>(enemy.pellet_speed));
		if(fired != EnemyUpdateResult::completed) return fired;
	}
	if(enemy_template.patnum == 0) return EnemyUpdateResult::completed;

	const auto top = scrolled_top(position.y, run.scroll_line, true);
	const auto patnum = (enemy.render_as != 2)
		? add_word(enemy_template.patnum, enemy.patnum_delta)
		: enemy.patnum_delta;
	callbacks.render_enemy({{position.x, top}, patnum});
	return EnemyUpdateResult::completed;
}

[[nodiscard]] bool script_for_template(
	const EnemyTemplate &enemy_template, const EnemyUpdateScriptData &data,
	std::span<const std::uint8_t> &script
) noexcept
{
	const auto index = static_cast<std::size_t>(enemy_template.script_index);
	if(index >= data.scripts.size() || index >= data.script_lengths.size()) {
		return false;
	}
	const auto length = static_cast<std::size_t>(data.script_lengths[index]);
	if(length > data.scripts[index].size()) return false;
	script = {data.scripts[index].data(), length};
	return true;
}

} // namespace

EnemyKillUpdateResult enemy_kill_update_and_render(
	Enemy &enemy, const EnemyTemplate &enemy_template, ScreenPoint topleft,
	std::size_t page_back, std::int16_t scroll_line, SparkPoolState &sparks,
	EnemyUpdateCallbacks &callbacks
) noexcept
{
	std::int16_t patnum = enemy_template.explode_sprite;
	patnum = (patnum == 0) ? 10 : 0;
	patnum = add_word(patnum, static_cast<std::int16_t>(
		enemy.age / enemy_kill_cel_frames));
	if(enemy.age == 0) {
		if(!sparks_add(sparks, page_back,
			add_word(topleft.x, enemy_extent / 2),
			static_cast<std::uint16_t>(add_word(topleft.y, enemy_extent / 2)),
			enemy_kill_spark_speed, enemy_kill_spark_count, false)) {
			return EnemyKillUpdateResult::death_spark_allocation_refused;
		}
	}
	enemy.age = add_word(enemy.age, 1);
	if(enemy.age >= enemy_kill_frames) {
		enemy.flag = enemy_remove;
		return EnemyKillUpdateResult::removed;
	}
	callbacks.render_enemy({{topleft.x,
		scrolled_top(topleft.y, scroll_line, false)}, patnum});
	return EnemyKillUpdateResult::animating;
}

EnemyUpdateResult enemies_update_and_render(
	EnemyPoolState &enemies, EnemyUpdateState &state,
	EnemyUpdateContext &context, EnemyUpdateCallbacks &callbacks
) noexcept
{
	EnemyRunResult script_result = EnemyRunResult::frame_consumed;
	std::int16_t lowest_top = 0;
	state.boss_position = {-1, -1};

	for(std::size_t slot = 0U;
		slot < static_cast<std::size_t>(enemies.enemies_loop_bound); ++slot) {
		if(slot >= enemies.enemies.size()) return EnemyUpdateResult::invalid_loop_bound;
		auto &enemy = enemies.enemies[slot];
		if(enemy.flag != enemy_alive) continue;

		if(enemy.template_id < 0 || static_cast<std::size_t>(enemy.template_id) >=
			context.script_data.templates.size()) {
			return EnemyUpdateResult::invalid_template;
		}
		const auto &enemy_template = context.script_data.templates[
			static_cast<std::size_t>(enemy.template_id)
		];

		std::size_t vm_steps = 0U;
		do {
			if(enemy.in_kill_anim == 0U) {
				std::span<const std::uint8_t> script{};
				if(!script_for_template(enemy_template, context.script_data, script)) {
					return EnemyUpdateResult::invalid_script;
				}
				if(vm_steps >= context.max_vm_steps) {
					return EnemyUpdateResult::vm_step_limit;
				}
				++vm_steps;
				script_result = enemy_run(enemy, enemy_template, script, context.run,
					callbacks);
				const auto result = map_run_result(script_result);
				if(result != EnemyUpdateResult::completed) return result;
			} else if(script_result == EnemyRunResult::continue_same_frame) {
				return EnemyUpdateResult::carried_vm_status_stalled;
			}
			if(script_result == EnemyRunResult::finished) break;
		} while(script_result == EnemyRunResult::continue_same_frame);
		if(script_result == EnemyRunResult::finished) continue;

		const auto page_back = context.run.player_frame.page_back;
		if(page_back >= enemy.pos_on_page.size()) {
			return EnemyUpdateResult::invalid_page;
		}
		const auto position = enemy.pos_on_page[page_back];
		if(position.x < add_word(playfield_left, -enemy_extent) ||
			position.x > add_word(playfield_right, 2)) {
			enemy.flag = enemy_remove;
			continue;
		}
		if(enemy.despawn_when_offscreen_vertically != 0U &&
			(position.y >= playfield_bottom ||
				position.y <= add_word(playfield_top, -enemy_extent))) {
			enemy.flag = enemy_remove;
			continue;
		}

		enemy.anim_frame = static_cast<std::uint8_t>(enemy.anim_frame + 1U);
		if(enemy_template.anim_cels != 0 && enemy.render_as == 1) {
			if(enemy_template.anim_frames_per_cel == 0) {
				return EnemyUpdateResult::source_divide_fault;
			}
			if((static_cast<std::int16_t>(enemy.anim_frame) %
				enemy_template.anim_frames_per_cel) == 0) {
				enemy.patnum_delta = add_word(enemy.patnum_delta, 1);
				if(enemy.patnum_delta >= enemy_template.anim_cels) {
					enemy.patnum_delta = 0;
				}
			}
		}

		if(enemy_template.hp == -1 || enemy.not_shootable != 0U) {
			const auto rendered = render_enemy(enemy, enemy_template, position,
				context.run, callbacks);
			if(rendered != EnemyUpdateResult::completed) return rendered;
			continue;
		}
		if(enemy.in_kill_anim != 0U) {
			const auto killed = enemy_kill_update_and_render(enemy, enemy_template,
				position, page_back, context.run.scroll_line, context.sparks, callbacks);
			if(killed == EnemyKillUpdateResult::death_spark_allocation_refused) {
				return EnemyUpdateResult::death_spark_allocation_refused;
			}
			continue;
		}

		if(position.y < 360 && position.y > lowest_top) {
			state.boss_position = {
				add_word(position.x, 8), add_word(position.y, 8),
			};
			lowest_top = position.y;
		}

		PlayerShotSparkAllocatorCallbacks shot_sparks(context.sparks, page_back);
		const auto damage = enemy_hittest(enemy, enemy_template,
			context.run.player_frame, context.player_hit, context.shots,
			context.score, context.stream.shot_level, context.stage_frame,
			context.run.bombing, context.bomb_damage_frame_mask, shot_sparks);
		if(!damage) return EnemyUpdateResult::invalid_hittest_input;
		if(!shot_sparks.all_allocations_succeeded()) {
			return EnemyUpdateResult::shot_spark_allocation_refused;
		}

		if(*damage != 0) {
			if(enemy_template.hp == -2) {
				callbacks.sound_effect(4U);
			} else {
				enemy.damage = add_word(enemy.damage, *damage);
				if(enemy.damage >= enemy_template.hp) {
					enemy.age = 0;
					if(enemy_template.item > static_cast<std::uint8_t>(ItemType::point)) {
						items_add(context.items, context.score,
							add_word(position.x, 12), position.y,
							static_cast<std::int16_t>(enemy_template.item) -
								static_cast<std::int16_t>(ItemType::bomb));
					} else if(!items_add_semirandom(context.items, context.run.random,
						context.score, add_word(position.x, 12), position.y)) {
						return EnemyUpdateResult::semirandom_item_refused;
					}
					callbacks.sound_effect(3U);
					context.score.score_delta = add_dword(context.score.score_delta,
						enemy_template.score);
					if(context.run.rank == rank_lunatic) {
						const auto fired = add_pellet(enemy_template, context.run,
							position, 0U, enemy_bullet_group::one_aimed,
							lunatic_death_pellet_speed);
						if(fired != EnemyUpdateResult::completed) return fired;
					}
					enemy.in_kill_anim = 1U;
					continue;
				}
				callbacks.sound_effect(4U);
				callbacks.render_enemy_hit_white({{position.x,
					scrolled_top(position.y, context.run.scroll_line, true)},
					add_word(enemy_template.patnum, enemy.patnum_delta)});
				continue;
			}
		}

		const auto rendered = render_enemy(enemy, enemy_template, position,
			context.run, callbacks);
		if(rendered != EnemyUpdateResult::completed) return rendered;
	}
	return EnemyUpdateResult::completed;
}

} // namespace th02::core
