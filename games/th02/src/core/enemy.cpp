#include "th02/core/enemy.hpp"

#include <bit>

namespace th02::core {
namespace {

constexpr std::uint8_t source_shot_level_max = 9U;
constexpr std::uint8_t player_hit_ordinary = 1U;
constexpr std::int16_t player_collision_half_extent = 16;

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
