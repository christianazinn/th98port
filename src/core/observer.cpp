#include "th03/core/observer.hpp"

#include "game_internal.hpp"

namespace th03 {

static_assert(ShotsEnemiesView::shot_count == detail::shotpair_count);
static_assert(ShotsEnemiesView::enemy_count == detail::enemy_count);
static_assert(FireballsView::fireball_count == detail::fireball_count);

MainPhaseContext ShotsEnemiesView::context() const noexcept
{
	return MainPhaseContext{
		.round_id = state_->round_id,
		.round_or_result_frame = state_->round_or_result_frame,
		.playfield_fg_shift_x = state_->playfield_fg_shift_x,
	};
}

std::optional<ShotPairFact> ShotsEnemiesView::shot(
	ShotPairSlot slot
) const noexcept
{
	const auto slot_id = static_cast<std::size_t>(slot);
	if(slot_id >= shot_count) {
		return std::nullopt;
	}
	const auto &shot = state_->shotpairs[slot_id];
	return ShotPairFact{
		.slot = slot,
		.alive = shot.alive,
		.topleft_x = shot.topleft_x,
		.topleft_y = shot.topleft_y,
		.player_id = static_cast<PlayerId>(shot.player_id),
	};
}

std::optional<EnemyFact> ShotsEnemiesView::enemy(EnemySlot slot) const noexcept
{
	const auto slot_id = static_cast<std::size_t>(slot);
	if(slot_id >= enemy_count) {
		return std::nullopt;
	}
	const auto &enemy = state_->enemies[slot_id];
	return EnemyFact{
		.slot = slot,
		.flag = EfeFlag{static_cast<std::uint8_t>(enemy.flag)},
		.frame = enemy.frame,
		.center_x = enemy.center_x,
		.center_y = enemy.center_y,
		.player_id = static_cast<PlayerId>(enemy.player_id),
		.size_pixels = enemy.size_pixels,
		.hp = enemy.hp,
		.explosion_max_enemy_hits_half =
			enemy.explosion_max_enemy_hits_half,
	};
}

MainPhaseContext FireballsView::context() const noexcept
{
	return MainPhaseContext{
		.round_id = state_->round_id,
		.round_or_result_frame = state_->round_or_result_frame,
		.playfield_fg_shift_x = state_->playfield_fg_shift_x,
	};
}

std::optional<FireballFact> FireballsView::fireball(
	FireballSlot slot
) const noexcept
{
	const auto slot_id = static_cast<std::size_t>(slot);
	if(slot_id >= fireball_count) {
		return std::nullopt;
	}
	const auto &fireball = state_->fireballs[slot_id];
	return FireballFact{
		.slot = slot,
		.flag = EfeFlag{static_cast<std::uint8_t>(fireball.flag)},
		.frame = fireball.frame,
		.center_x = fireball.center_x,
		.center_y = fireball.center_y,
		.player_id = static_cast<PlayerId>(fireball.player_id),
		.variant = static_cast<FireballVariant>(
			fireball.variant_as_explosion_target
		),
	};
}

} // namespace th03
