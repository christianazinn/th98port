#include "th03/core/observer.hpp"

#include "game_internal.hpp"

#include <algorithm>

namespace th03 {

static_assert(ShotsEnemiesView::shot_count == detail::shotpair_count);
static_assert(ShotsEnemiesView::enemy_count == detail::enemy_count);
static_assert(FireballsView::fireball_count == detail::fireball_count);
static_assert(MainExtraAttackView::max_count == detail::extra_attack_count);
static_assert(
	MainExtraAttackView::ellen_trail_count ==
	std::tuple_size_v<decltype(detail::ExtraAttackState::ellen_trail_x)>
);
static_assert(
	MainChargeShotView::reimu_group_count ==
	std::tuple_size_v<
		decltype(detail::DeterministicState::reimu_charge)::value_type
	>
);
static_assert(
	MainChargeShotView::mima_node_count ==
	std::tuple_size_v<
		decltype(detail::DeterministicState::mima_charge)::value_type
	>
);
static_assert(
	MainChargeShotView::marisa_node_count ==
	std::tuple_size_v<decltype(detail::MarisaChargeState::x)>
);
static_assert(
	MainChargeShotView::ellen_node_count ==
	std::tuple_size_v<
		decltype(detail::DeterministicState::ellen_charge)::value_type
	>
);
static_assert(
	MainChargeShotView::kana_group_count ==
	std::tuple_size_v<
		decltype(detail::DeterministicState::kana_charge)::value_type
	>
);
static_assert(
	MainChargeShotView::rikako_node_count ==
	std::tuple_size_v<
		decltype(detail::DeterministicState::rikako_charge)::value_type
	>
);
static_assert(
	MainChargeShotView::chiyuri_node_count ==
	std::tuple_size_v<
		decltype(detail::DeterministicState::chiyuri_charge)::value_type
	>
);
static_assert(PlayersBulletsView::player_count == detail::player_count);
static_assert(PlayersBulletsView::bullet_count == detail::bullet_count);
static_assert(
	std::tuple_size_v<decltype(MainScoreState::digits)> == detail::player_count
);
static_assert(
	std::tuple_size_v<decltype(MainScoreState::digits)::value_type> ==
	detail::score_digits
);
static_assert(
	static_cast<std::uint8_t>(MainResultTransitionPhase::idle) ==
	static_cast<std::uint8_t>(detail::ResultPhase::idle)
);
static_assert(
	static_cast<std::uint8_t>(MainResultTransitionPhase::opening) ==
	static_cast<std::uint8_t>(detail::ResultPhase::opening)
);
static_assert(
	static_cast<std::uint8_t>(MainResultTransitionPhase::closing) ==
	static_cast<std::uint8_t>(detail::ResultPhase::closing)
);
static_assert(
	MainBossView::reimu_orbit_point_count ==
	std::tuple_size_v<decltype(detail::BossState::orbit_x)>
);
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

namespace {

std::optional<std::size_t> player_index(PlayerId player) noexcept
{
	const auto index = static_cast<std::size_t>(player);
	if(index >= detail::player_count) {
		return std::nullopt;
	}
	return index;
}

bool selected_character_is(
	const detail::DeterministicState *state,
	PlayerId player,
	std::size_t character
) noexcept
{
	const auto index = player_index(player);
	if(!index) {
		return false;
	}
	const auto paletted_character = state->players[*index].paletted_character;
	if(paletted_character < 1 || paletted_character > 18) {
		return false;
	}
	return detail::character_index_from_paletted(paletted_character) == character;
}

MainPhaseContext main_phase_context(
	const detail::DeterministicState *state
) noexcept
{
	return MainPhaseContext{
		.round_id = state->round_id,
		.round_or_result_frame = state->round_or_result_frame,
		.playfield_fg_shift_x = state->playfield_fg_shift_x,
	};
}

} // namespace

MainPhaseContext MainExtraAttackView::context() const noexcept
{
	return main_phase_context(state_);
}

PlayerId MainExtraAttackView::player() const noexcept
{
	return player_;
}

std::int8_t MainExtraAttackView::paletted_character() const noexcept
{
	const auto index = player_index(player_);
	return index ? state_->players[*index].paletted_character : 0;
}

std::size_t MainExtraAttackView::slot_count() const noexcept
{
	const auto index = player_index(player_);
	if(!index) {
		return 0U;
	}
	const auto selected = state_->players[*index].paletted_character;
	if(selected < 1 || selected > 18) {
		return 0U;
	}
	switch(detail::character_index_from_paletted(selected)) {
	case detail::character_reimu:
		return detail::extra_attack_regular_count;
	case detail::character_mima:
		return detail::extra_attack_mima_count;
	case detail::character_marisa:
		return detail::extra_attack_marisa_count;
	case detail::character_ellen:
		return detail::extra_attack_ellen_count;
	case detail::character_kotohime:
		return 10U;
	case detail::character_kana:
		return detail::extra_attack_regular_count;
	case detail::character_rikako:
		return detail::extra_attack_rikako_count;
	case detail::character_chiyuri:
	case detail::character_yumemi:
		return detail::extra_attack_count;
	default:
		return 0U;
	}
}

std::optional<MainExtraAttackFact> MainExtraAttackView::attack(
	ExtraAttackSlot slot
) const noexcept
{
	const auto index = player_index(player_);
	const auto slot_id = static_cast<std::size_t>(slot);
	if(!index || slot_id >= slot_count()) {
		return std::nullopt;
	}
	const auto &attack = state_->extra_attacks[*index][slot_id];
	return MainExtraAttackFact{
		.slot = slot,
		.state = attack.state,
		.frame = attack.frame,
		.center_x = attack.center_x,
		.center_y = attack.center_y,
		.destination_raw = attack.player_id,
		.mima_timer_low = static_cast<std::uint8_t>(attack.mima_timer),
		.yumemi_extent = attack.yumemi_extent,
		.yumemi_duration = attack.yumemi_duration,
		.kotohime_target_y = attack.kotohime_target_y,
		.kotohime_variant = attack.behavior_variant,
		.ellen_trail_x = attack.ellen_trail_x,
		.ellen_trail_y = attack.ellen_trail_y,
	};
}

MainPhaseContext MainChargeShotView::context() const noexcept
{
	return main_phase_context(state_);
}

PlayerId MainChargeShotView::player() const noexcept
{
	return player_;
}

std::int8_t MainChargeShotView::paletted_character() const noexcept
{
	const auto index = player_index(player_);
	return index ? state_->players[*index].paletted_character : 0;
}

std::optional<ReimuChargeGroupFact> MainChargeShotView::reimu_group(
	ReimuChargeGroupSlot group
) const noexcept
{
	const auto index = player_index(player_);
	const auto group_id = static_cast<std::size_t>(group);
	if(
		!index || !selected_character_is(
			state_, player_, detail::character_reimu
		) || group_id >= reimu_group_count
	) {
		return std::nullopt;
	}
	return ReimuChargeGroupFact{
		.state = state_->reimu_charge[*index][group_id].state,
	};
}

std::optional<RawPoint> MainChargeShotView::reimu_point(
	ReimuChargeGroupSlot group,
	ReimuChargePoint point
) const noexcept
{
	if(!reimu_group(group)) {
		return std::nullopt;
	}
	const auto point_id = static_cast<std::size_t>(point);
	if(point_id != 0U && point_id != 2U && point_id != 4U && point_id != 6U) {
		return std::nullopt;
	}
	const auto index = *player_index(player_);
	const auto group_id = static_cast<std::size_t>(group);
	return RawPoint{
		.x = state_->reimu_charge[index][group_id].x[point_id],
		.y = state_->reimu_charge[index][group_id].y[point_id],
	};
}

std::optional<MimaChargeStateFact> MainChargeShotView::mima_state() const noexcept
{
	const auto index = player_index(player_);
	if(!index || !selected_character_is(state_, player_, detail::character_mima)) {
		return std::nullopt;
	}
	return MimaChargeStateFact{.active = state_->mima_charge_active[*index]};
}

std::optional<MimaChargeNodeFact> MainChargeShotView::mima_node(
	MimaChargeNodeSlot node
) const noexcept
{
	const auto index = player_index(player_);
	const auto node_id = static_cast<std::size_t>(node);
	if(!mima_state() || node_id >= mima_node_count) {
		return std::nullopt;
	}
	const auto &source = state_->mima_charge[*index][node_id];
	return MimaChargeNodeFact{
		.active = source.active,
		.center_x = source.x,
		.center_y = source.y,
	};
}

std::optional<MarisaChargeStateFact> MainChargeShotView::marisa_state() const noexcept
{
	const auto index = player_index(player_);
	if(
		!index || !selected_character_is(
			state_, player_, detail::character_marisa
		)
	) {
		return std::nullopt;
	}
	return MarisaChargeStateFact{.active = state_->marisa_charge[*index].active};
}

std::optional<MarisaChargeNodeFact> MainChargeShotView::marisa_node(
	MarisaChargeNodeSlot node
) const noexcept
{
	const auto index = player_index(player_);
	const auto node_id = static_cast<std::size_t>(node);
	if(!marisa_state() || node_id >= marisa_node_count) {
		return std::nullopt;
	}
	return MarisaChargeNodeFact{
		.center_x = state_->marisa_charge[*index].x[node_id],
		.center_y = state_->marisa_charge[*index].y[node_id],
	};
}

std::optional<EllenChargeNodeFact> MainChargeShotView::ellen_node(
	EllenChargeNodeSlot node
) const noexcept
{
	const auto index = player_index(player_);
	const auto node_id = static_cast<std::size_t>(node);
	if(
		!index || !selected_character_is(
			state_, player_, detail::character_ellen
		) || node_id >= ellen_node_count
	) {
		return std::nullopt;
	}
	const auto &source = state_->ellen_charge[*index][node_id];
	return EllenChargeNodeFact{
		.state = source.state,
		.angle = source.angle,
		.center_x = source.x,
		.center_y = source.y,
	};
}

std::optional<KotohimeChargeFact> MainChargeShotView::kotohime() const noexcept
{
	const auto index = player_index(player_);
	if(
		!index || !selected_character_is(
			state_, player_, detail::character_kotohime
		)
	) {
		return std::nullopt;
	}
	const auto &source = state_->kotohime_charge[*index];
	return KotohimeChargeFact{
		.active = source.active,
		.center_x = source.x,
		.center_y = source.y,
	};
}

std::optional<KanaChargeStateFact> MainChargeShotView::kana_state() const noexcept
{
	const auto index = player_index(player_);
	if(!index || !selected_character_is(state_, player_, detail::character_kana)) {
		return std::nullopt;
	}
	return KanaChargeStateFact{.state = state_->kana_charge_state[*index]};
}

std::optional<RawPoint> MainChargeShotView::kana_point(
	KanaChargeGroupSlot group,
	KanaChargePoint point
) const noexcept
{
	const auto index = player_index(player_);
	const auto group_id = static_cast<std::size_t>(group);
	if(!kana_state() || group_id >= kana_group_count) {
		return std::nullopt;
	}
	const auto point_id = static_cast<std::size_t>(point);
	if(
		point_id != 0U && point_id != 4U && point_id != 8U &&
		point_id != 12U
	) {
		return std::nullopt;
	}
	return RawPoint{
		.x = state_->kana_charge[*index][group_id].x[point_id],
		.y = state_->kana_charge[*index][group_id].y[point_id],
	};
}

std::optional<RikakoChargeStateFact> MainChargeShotView::rikako_state() const noexcept
{
	const auto index = player_index(player_);
	if(
		!index || !selected_character_is(
			state_, player_, detail::character_rikako
		)
	) {
		return std::nullopt;
	}
	return RikakoChargeStateFact{.state = state_->rikako_charge_state[*index]};
}

std::optional<RikakoChargeNodeFact> MainChargeShotView::rikako_node(
	RikakoChargeNodeSlot node
) const noexcept
{
	const auto index = player_index(player_);
	const auto node_id = static_cast<std::size_t>(node);
	if(!rikako_state() || node_id >= rikako_node_count) {
		return std::nullopt;
	}
	return RikakoChargeNodeFact{
		.center_x = state_->rikako_charge[*index][node_id].x,
		.center_y = state_->rikako_charge[*index][node_id].y,
	};
}

std::optional<ChiyuriChargeNodeFact> MainChargeShotView::chiyuri_node(
	ChiyuriChargeNodeSlot node
) const noexcept
{
	const auto index = player_index(player_);
	const auto node_id = static_cast<std::size_t>(node);
	if(
		!index || !selected_character_is(
			state_, player_, detail::character_chiyuri
		) || node_id >= chiyuri_node_count
	) {
		return std::nullopt;
	}
	const auto &source = state_->chiyuri_charge[*index][node_id];
	return ChiyuriChargeNodeFact{
		.state = source.state,
		.frame = source.frame,
		.center_x = source.x,
		.center_y = source.y,
	};
}

std::optional<YumemiChargeFact> MainChargeShotView::yumemi() const noexcept
{
	const auto index = player_index(player_);
	if(
		!index || !selected_character_is(
			state_, player_, detail::character_yumemi
		)
	) {
		return std::nullopt;
	}
	const auto &source = state_->yumemi_charge[*index];
	return YumemiChargeFact{
		.state = source.state,
		.frame = source.frame,
		.center_x = source.x,
		.center_y = source.y,
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

MainPhaseContext MainBossView::context() const noexcept
{
	return MainPhaseContext{
		.round_id = state_->round_id,
		.round_or_result_frame = state_->round_or_result_frame,
		.playfield_fg_shift_x = state_->playfield_fg_shift_x,
	};
}

bool MainBossView::round_frame_is_odd() const noexcept
{
	return (state_->round_frame & 1U) != 0U;
}

std::optional<MainBossFact> MainBossView::boss() const noexcept
{
	if(
		state_->gba_boss_launched_by < 0 ||
		state_->gba_boss_launched_by >=
			static_cast<std::int8_t>(detail::player_count)
	) {
		return std::nullopt;
	}
	const auto owner_index = static_cast<std::size_t>(
		state_->gba_boss_launched_by
	);
	const auto paletted_character =
		state_->players[owner_index].paletted_character;
	const auto character = detail::character_index_from_paletted(
		paletted_character
	);
	const auto is_reimu = (character == detail::character_reimu);
	const auto is_kotohime = (character == detail::character_kotohime);
	const auto is_yumemi = (character == detail::character_yumemi);
	return MainBossFact{
		.owner = static_cast<PlayerId>(owner_index),
		.paletted_character = paletted_character,
		.center_x = state_->boss.center_x,
		.center_y = state_->boss.center_y,
		.hit = state_->boss.hit,
		.state = state_->boss.state,
		.pattern_frame = state_->boss.pattern_frame,
		.render_state = state_->boss.render_state,
		.render_frame = state_->boss.render_frame,
		.pattern_angle = state_->boss.pattern_angle,
		.kotohime_radius = is_kotohime
			? state_->boss.anchor_x
			: std::int16_t{0},
		.kotohime_orbit_count = is_kotohime
			? state_->boss.params[1]
			: std::uint8_t{0},
		.reimu_orbit_count = is_reimu
			? static_cast<std::uint8_t>(std::min<std::size_t>(
				state_->boss.params[3], reimu_orbit_point_count
			))
			: std::uint8_t{0},
		.yumemi_radius = is_yumemi
			? state_->yumemi_boss_radius
			: std::uint16_t{0},
		.yumemi_muzzle = is_yumemi
			? RawPoint{
				.x = state_->yumemi_boss_muzzle_x,
				.y = state_->yumemi_boss_muzzle_y,
			}
			: RawPoint{},
		.yumemi_target = is_yumemi
			? RawPoint{
				.x = state_->yumemi_boss_target_x,
				.y = state_->yumemi_boss_target_y,
			}
			: RawPoint{},
	};
}

std::optional<RawPoint> MainBossView::reimu_orbit_point(
	ReimuBossOrbitPoint point
) const noexcept
{
	const auto point_id = static_cast<std::size_t>(point);
	if(point_id >= reimu_orbit_point_count) {
		return std::nullopt;
	}
	const auto fact = boss();
	if(
		!fact || detail::character_index_from_paletted(
			fact->paletted_character
		) != detail::character_reimu
	) {
		return std::nullopt;
	}
	return RawPoint{
		.x = state_->boss.orbit_x[point_id],
		.y = state_->boss.orbit_y[point_id],
	};
}

} // namespace th03
