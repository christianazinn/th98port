#include "th02/core/enemy.hpp"

#include "th02/core/random.hpp"

#include <bit>

namespace th02::core {
namespace {

constexpr std::uint8_t source_shot_level_max = 9U;
constexpr std::uint8_t player_hit_ordinary = 1U;
constexpr std::int16_t player_collision_half_extent = 16;
constexpr std::int16_t enemy_spawn_left_half_boundary = 208;
constexpr std::int16_t enemy_spawn_column_spacing = 8;
constexpr std::int16_t enemy_spawn_left_base = 16;

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

[[nodiscard]] bool inside_strict(
	std::int16_t value, std::int16_t origin
) noexcept
{
	return value > subtract_word(origin, player_collision_half_extent) &&
		value < add_word(origin, player_collision_half_extent);
}

} // namespace

void enemies_reset(EnemyPoolState &state) noexcept
{
	for(auto &enemy : state.enemies) {
		enemy.flag = enemy_free;
	}
	state.spawn_row_cur = 0;
}

EnemySpawnResult enemy_add(EnemyPoolState &state, RandomState &random,
	std::int16_t left, std::int16_t top, std::int16_t slot,
	std::int16_t template_id, std::size_t page_back
) noexcept
{
	if(page_back >= state.enemies[0].pos_on_page.size()) {
		return EnemySpawnResult::invalid_page;
	}
	if((slot < 0) ||
		(static_cast<std::size_t>(slot) >= state.enemies.size())) {
		return EnemySpawnResult::invalid_slot;
	}

	auto &enemy = state.enemies[static_cast<std::size_t>(slot)];
	enemy.flag = enemy_alive;
	enemy.pos_on_page[page_back] = {left, top};
	enemy.patnum_delta = 0;
	enemy.script_ip = 0;
	enemy.template_id = template_id;
	enemy.age = 0;
	enemy.anim_frame = randring_next8(random);
	enemy.angle = 0;
	enemy.not_shootable = 0U;
	enemy.no_player_collision = 0U;
	enemy.spawned_in_left_half =
		(left < enemy_spawn_left_half_boundary) ? 1 : 0;
	enemy.loop_i = 0;
	enemy.despawn_when_offscreen_vertically = 0U;
	enemy.render_as = 1;
	enemy.in_kill_anim = 0U;
	enemy.damage = 0;

	if(state.enemies_loop_bound < slot) {
		state.enemies_loop_bound = static_cast<std::uint8_t>(slot + 1);
	}
	return EnemySpawnResult::completed;
}

EnemySpawnResult enemies_spawn(EnemyPoolState &state, RandomState &random,
	const EnemySpawnGrid &grid, std::span<const EnemyTemplate> templates,
	const EnemySpawnContext &context
) noexcept
{
	if(state.spawn_row_cur >= grid.source_row_count) {
		return EnemySpawnResult::completed;
	}
	if((state.spawn_row_cur < 0) ||
		(static_cast<std::size_t>(state.spawn_row_cur) >= grid.rows.size())) {
		return EnemySpawnResult::invalid_row;
	}

	const auto &row = grid.rows[static_cast<std::size_t>(state.spawn_row_cur)];
	if(row.column[0U] != context.scroll_step) {
		return EnemySpawnResult::completed;
	}

	for(std::size_t column = 1U; column < row.column.size(); ++column) {
		const auto template_id = row.column[column];
		if(template_id == -1) continue;

		if((template_id < 0) ||
			(static_cast<std::size_t>(template_id) >= templates.size())) {
			return EnemySpawnResult::invalid_template;
		}
		const auto top = templates[static_cast<std::size_t>(template_id)].spawn_top;

		if(!context.midboss_active) {
			for(std::size_t slot = 0U; slot < state.enemies.size(); ++slot) {
				if(state.enemies[slot].flag != enemy_free) continue;

				const auto left = static_cast<std::int16_t>(
					(static_cast<std::int32_t>(column) *
						enemy_spawn_column_spacing) + enemy_spawn_left_base
				);
				const auto result = enemy_add(state, random, left, top,
					static_cast<std::int16_t>(slot), template_id,
					context.page_back);
				if(result != EnemySpawnResult::completed) return result;
				break;
			}
		}
	}
	state.spawn_row_cur = add_word(state.spawn_row_cur, 1);
	return EnemySpawnResult::completed;
}

std::optional<std::int16_t> enemy_hittest(
	Enemy &enemy, const EnemyTemplate &enemy_template,
	const PlayerFrameState &player_frame, PlayerHitState &player_hit,
	PlayerShotPoolState &shots, ScoreState &score,
	std::uint8_t shot_level, std::uint32_t stage_frame, bool bombing,
	std::uint8_t bomb_damage_frame_mask,
	PlayerShotHitCallbacks &callbacks
) noexcept
{
	// The source assumes these globals are valid. The native boundary refuses
	// all represented page/level violations before touching enemy/player state;
	// shots_hittest() applies the same level range to its own scan.
	if(player_frame.page_front >= player_frame.player.on_page.size() ||
		player_frame.page_back >= player_frame.player.on_page.size() ||
		player_frame.page_back >= enemy.pos_on_page.size() ||
		player_frame.page_back >= shots.shots.front().pos_on_page.size() ||
		shot_level > source_shot_level_max) {
		return std::nullopt;
	}

	const auto &enemy_position = enemy.pos_on_page[player_frame.page_back];
	const auto &player_position =
		player_frame.player.on_page[player_frame.page_front];
	if(enemy_template.no_player_collision == 0U &&
		enemy.no_player_collision == 0U &&
		inside_strict(player_position.x, enemy_position.x) &&
		inside_strict(player_frame.player.current.y, enemy_position.y)) {
		// Source player_is_hit is a raw byte; ordinary collision writes one and
		// does not normalize other caller-owned hit values in this body.
		enemy.age = 0;
		player_hit.raw_hit = player_hit_ordinary;
		enemy.in_kill_anim = 1U;
	}

	// The source calls shots_hittest regardless of whether player collision
	// happened. Its callback remains synchronous at the source spark position.
	return shots_hittest(
		shots, score, player_frame.page_back, shot_level, stage_frame, bombing,
		bomb_damage_frame_mask,
		{{enemy_position.x, enemy_position.y}, enemy_hittest_width,
			enemy_hittest_height}, callbacks
	);
}

} // namespace th02::core
