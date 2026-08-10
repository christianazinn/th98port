#include "th03/core/observer.hpp"

#include "game_internal.hpp"

namespace th03 {

static_assert(ShotsEnemiesView::shot_count == detail::shotpair_count);
static_assert(ShotsEnemiesView::enemy_count == detail::enemy_count);
static_assert(FireballsView::fireball_count == detail::fireball_count);
static_assert(PlayersBulletsView::player_count == detail::player_count);
static_assert(PlayersBulletsView::bullet_count == detail::bullet_count);
static_assert(
	PlayersBulletsView::trail_point_count ==
	std::tuple_size_v<decltype(detail::BulletTrailState::center_x)>
);

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

MainPhaseContext PlayersBulletsView::context() const noexcept
{
	return MainPhaseContext{
		.round_id = state_->round_id,
		.round_or_result_frame = state_->round_or_result_frame,
		.playfield_fg_shift_x = state_->playfield_fg_shift_x,
	};
}

std::optional<PlayerFact> PlayersBulletsView::player(
	PlayerId player_id
) const noexcept
{
	const auto player_index = static_cast<std::size_t>(player_id);
	if(player_index >= player_count) {
		return std::nullopt;
	}
	const auto &player = state_->players[player_index];
	return PlayerFact{
		.player_id = player_id,
		.center_x = player.center_x.raw(),
		.center_y = player.center_y.raw(),
		.paletted_character = player.paletted_character,
		.lose_anim_time = player.lose_anim_time,
		.invincibility_time = player.invincibility_time,
		.hyper_active = player.hyper_active,
		.gauge_charged = player.gauge_charged,
	};
}

std::optional<BulletFact> PlayersBulletsView::bullet(
	BulletSlot slot
) const noexcept
{
	const auto slot_id = static_cast<std::size_t>(slot);
	if(slot_id >= bullet_count) {
		return std::nullopt;
	}
	const auto &bullet = state_->bullets[slot_id];
	return BulletFact{
		.slot = slot,
		.flag = BulletFlagFact{static_cast<std::uint8_t>(bullet.flag)},
		.age = bullet.age,
		.center_x = bullet.center_x,
		.center_y = bullet.center_y,
		.player_id = static_cast<PlayerId>(bullet.player_id),
		.has_trail = bullet.has_trail,
	};
}

std::optional<RawPoint> PlayersBulletsView::trail_point(
	BulletSlot slot,
	TrailPoint point
) const noexcept
{
	const auto slot_id = static_cast<std::size_t>(slot);
	const auto point_id = static_cast<std::size_t>(point);
	if(slot_id >= bullet_count || point_id >= trail_point_count) {
		return std::nullopt;
	}
	const auto &bullet = state_->bullets[slot_id];
	if(
		bullet.flag != detail::BulletFlag::bullet16 || !bullet.has_trail ||
		bullet.trail_index >= state_->bullet_trails.size()
	) {
		return std::nullopt;
	}
	const auto &trail = state_->bullet_trails[bullet.trail_index];
	return RawPoint{
		.x = trail.center_x[point_id],
		.y = trail.center_y[point_id],
	};
}

} // namespace th03
