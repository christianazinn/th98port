#include "th03/present/main_rollback.hpp"

#include <limits>

namespace th03::present {

bool MainPresenterRollbackAccess::valid(
	const MainPresenterRollbackState &state
) noexcept
{
	const auto valid_page = [](Page page) {
		return page == Page::zero || page == Page::one;
	};
	if(!state.palette_baseline_ready || state.palette_dirty ||
		!state.static_started || !valid_page(state.page_roles.back) ||
		!valid_page(state.page_roles.front) ||
		state.page_roles.back == state.page_roles.front ||
		static_cast<std::uint8_t>(state.background_kind) >
			static_cast<std::uint8_t>(MainBackgroundKind::type_b) ||
		state.background_read_stream != 0U ||
		state.background_read_slot > main_background_record_count ||
		!state.background_initialized ||
		state.background_status != MainBackgroundStatus::ready ||
		state.boss_activation.character > 8U ||
		state.boss_status != MainBossStatus::ready ||
		state.entities_status != MainEntitiesStatus::ready ||
		state.effects_status != MainEffectsStatus::ready ||
		state.extra_charge_status != MainExtraChargeStatus::ready ||
		state.players_bullets_status != MainPlayersBulletsStatus::ready ||
		state.bomb_palette_status != MainBombPaletteStatus::ready ||
		state.bomb_pending.kind != 0U ||
		state.bombs_status != MainBombsStatus::ready ||
		state.warning_interstitial_active ||
		state.warning_status != MainWarningGaugeStatus::ready ||
		state.hud_particle_reads_pending ||
		state.hud_completed_batch_waiting ||
		state.hud_late_state_pending ||
		state.hud_status != MainHudResultStatus::ready) {
		return false;
	}
	for(const auto &effect : state.common_effects) {
		if(effect.type > 2U) {
			return false;
		}
	}
	for(const auto pose : state.player_poses) {
		if(pose > 2U) {
			return false;
		}
	}
	for(const auto &style : state.bullet16_styles) {
		if(static_cast<std::uint8_t>(style.construction) >
			static_cast<std::uint8_t>(
				Bullet16ConstructionKind::kana_extra_attack
			)) {
			return false;
		}
	}
	for(const auto &activation : state.bomb_activations) {
		if(activation.character > 8U) {
			return false;
		}
	}
	for(const auto &read : state.reimu_reads) {
		if(read.phase != 0U) {
			return false;
		}
	}
	return true;
}

bool MainPresenterRollbackAccess::boundary_ready(
	const MainPaletteState &palette,
	const MainStaticPresenter &static_presenter,
	const MainBackgroundPresenter &background,
	const MainBossPresenter &bosses,
	const MainEntitiesPresenter &entities,
	const MainEffectsPresenter &effects,
	const MainExtraChargePresenter &extra_charge,
	const MainPlayersBulletsPresenter &players_bullets,
	const MainBombsPresenter &bombs,
	const MainWarningGaugePresenter &warning_gauge,
	const MainHudResultPresenter &hud_result,
	const MainRoundPresenter &round
) noexcept
{
	const auto background_slot_fits = background.read_slot_ <=
		static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max());
	const auto reimu_reads_idle =
		bombs.reimu_reads_[0].phase == MainBombsPresenter::ReimuReadPhase::none &&
		bombs.reimu_reads_[1].phase == MainBombsPresenter::ReimuReadPhase::none;

	return palette.baseline_ready_ && !palette.dirty_ &&
		static_presenter.started_ &&
		background.status_ == MainBackgroundStatus::ready &&
		background.initialized_ &&
		background.read_stream_ == MainBackgroundPresenter::ReadStream::none &&
		background_slot_fits &&
		bosses.status_ == MainBossStatus::ready &&
		entities.status_ == MainEntitiesStatus::ready &&
		effects.status_ == MainEffectsStatus::ready &&
		extra_charge.status_ == MainExtraChargeStatus::ready &&
		players_bullets.status_ == MainPlayersBulletsStatus::ready &&
		bombs.status_ == MainBombsStatus::ready &&
		bombs.palette_interpreter_.status_ == MainBombPaletteStatus::ready &&
		bombs.pending_.kind == MainBombsPresenter::PendingKind::none &&
		reimu_reads_idle &&
		warning_gauge.status_ == MainWarningGaugeStatus::ready &&
		!warning_gauge.interstitial_active_ &&
		warning_gauge.nonblocking_interstitial_frames_ == 0U &&
		hud_result.status_ == MainHudResultStatus::ready &&
		!hud_result.hud_particle_reads_pending_ &&
		!hud_result.hud_completed_batch_waiting_ &&
		!hud_result.hud_late_state_pending_ &&
		round.status_ == MainRoundStatus::ready &&
		round.phase_ == MainRoundPresenter::Phase::boss &&
		!round.last_result_state_.has_value() &&
		!round.warning_interstitial_active_ &&
		!round.warning_suffix_pending_ &&
		!round.warning_lifecycle_seen_ &&
		!round.background_frame_seen_ &&
		round.background_frame_reads_remaining_ == 0U &&
		round.next_gauge_finish_player_ == 0U &&
		round.next_late_bomb_player_ == 0U &&
		round.next_warning_portrait_player_ == 0U &&
		round.expected_portrait_mask_ == 0U &&
		round.hud_start_reads_remaining_ == 0U &&
		!round.hud_start_step_seen_ &&
		!round.story_lives_changed_ &&
		!round.repeat_result_ready_;
}

void MainPresenterRollbackAccess::capture(
	const MainPaletteState &palette,
	const MainStaticPresenter &static_presenter,
	const MainBackgroundPresenter &background,
	const MainBossPresenter &bosses,
	const MainEntitiesPresenter &entities,
	const MainEffectsPresenter &effects,
	const MainExtraChargePresenter &extra_charge,
	const MainPlayersBulletsPresenter &players_bullets,
	const MainBombsPresenter &bombs,
	const MainWarningGaugePresenter &warning_gauge,
	const MainHudResultPresenter &hud_result,
	MainPresenterRollbackState &state
) noexcept
{
	state.palette_baseline = palette.baseline_;
	state.palette_baseline_ready = palette.baseline_ready_;
	state.palette_dirty = palette.dirty_;
	state.page_roles = static_presenter.pages_;
	state.static_started = static_presenter.started_;
	state.accelerator_roles_crossed =
		static_presenter.accelerator_roles_crossed_;

	for(std::size_t slot = 0U; slot < state.background.size(); ++slot) {
		const auto &source = background.records_[slot];
		auto &target = state.background[slot];
		target = {
			source.x_q12_4,
			source.y_q12_4,
			source.velocity_x_q12_4,
			source.velocity_y_q12_4,
			source.speed_q12_4,
			source.sprite_variant,
		};
	}
	state.background_kind = background.kind_;
	state.background_read_stream =
		static_cast<std::uint8_t>(background.read_stream_);
	state.background_read_slot = static_cast<std::uint32_t>(background.read_slot_);
	state.background_read_field = background.read_field_;
	state.background_initial_frames_completed =
		background.initial_frames_completed_;
	state.background_direction = background.direction_;
	state.background_round_id = background.round_id_;
	state.background_initialized = background.initialized_;
	state.background_have_round_id = background.have_round_id_;
	state.background_vector_latch = background.vector_latch_;
	state.background_rendering_enabled = background.rendering_enabled_;
	state.background_status = background.status_;

	state.boss_activation = {
		bosses.activation_.active,
		bosses.activation_.owner,
		bosses.activation_.paletted_character,
		static_cast<std::uint8_t>(bosses.activation_.character),
	};
	state.boss_ellen_phase = bosses.ellen_phase_;
	state.boss_rikako_angle = bosses.rikako_angle_;
	state.boss_rikako_angle_delta = bosses.rikako_angle_delta_;
	state.boss_chiyuri_phase = bosses.chiyuri_phase_;
	state.boss_status = bosses.status_;

	state.entity_shot_cels = entities.shot_cels_;
	state.entity_shot_players = entities.shot_players_;
	state.entity_shot_spawn_observed = entities.shot_spawn_observed_;
	state.entities_status = entities.status_;

	for(std::size_t slot = 0U; slot < state.hit_circles.size(); ++slot) {
		const auto &source = effects.hit_circles_[slot];
		state.hit_circles[slot] = {
			source.age,
			source.player,
			source.left,
			source.top,
		};
	}
	for(std::size_t slot = 0U; slot < state.common_effects.size(); ++slot) {
		const auto &source = effects.common_effects_[slot];
		state.common_effects[slot] = {
			source.type,
			source.age,
			source.center_x,
			source.center_y,
			source.radius,
			source.radius_delta,
			source.angle,
		};
	}
	state.enemy_hit_circle_cursor = effects.enemy_hit_circle_cursor_;
	state.common_effect_cursor = effects.common_effect_cursor_;
	state.kana_bomb_angle = effects.kana_bomb_angle_;
	state.kotohime_bomb_y = effects.kotohime_bomb_y_;
	state.effects_status = effects.status_;

	state.kana_destination_cache = extra_charge.kana_destination_cache_;
	state.extra_charge_status = extra_charge.status_;

	state.shared_velocity_x_q4_4 = players_bullets.shared_velocity_x_q4_4_;
	for(std::size_t player = 0U; player < state.player_poses.size(); ++player) {
		state.player_poses[player] =
			static_cast<std::uint8_t>(players_bullets.player_poses_[player]);
	}
	state.defeat = {
		players_bullets.defeat_.anchor_left,
		players_bullets.defeat_.anchor_top,
		players_bullets.defeat_.radius,
	};
	for(std::size_t slot = 0U; slot < state.bullet16_styles.size(); ++slot) {
		const auto &source = players_bullets.bullet16_styles_[slot];
		state.bullet16_styles[slot] = {
			source.source_player,
			source.construction,
			source.animated,
			source.observed,
		};
	}
	state.players_bullets_status = players_bullets.status_;

	state.bomb_palette_status = bombs.palette_interpreter_.status_;
	for(std::size_t player = 0U; player < state.bomb_activations.size(); ++player) {
		const auto &source = bombs.activations_[player];
		state.bomb_activations[player] = {
			source.active,
			source.paletted_character,
			static_cast<std::uint8_t>(source.character),
			source.frame,
			source.entry_shift,
		};
	}
	state.bomb_pending = {
		static_cast<std::uint8_t>(bombs.pending_.kind),
		bombs.pending_.player,
		bombs.pending_.frame,
		bombs.pending_.received,
	};
	for(std::size_t player = 0U; player < state.reimu.size(); ++player) {
		for(std::size_t slot = 0U; slot < state.reimu[player].size(); ++slot) {
			const auto &source = bombs.reimu_[player][slot];
			state.reimu[player][slot] = {
				source.active,
				source.x,
				source.y,
				source.velocity_y,
			};
		}
		const auto &read = bombs.reimu_reads_[player];
		state.reimu_reads[player] = {
			static_cast<std::uint8_t>(read.phase),
			read.frame,
			read.velocity_y,
		};
		for(std::size_t slot = 0U; slot < state.mima[player].size(); ++slot) {
			const auto &source = bombs.mima_[player][slot];
			state.mima[player][slot] = {source.active, source.x};
		}
		for(std::size_t slot = 0U; slot < state.ellen[player].size(); ++slot) {
			const auto &source = bombs.ellen_[player][slot];
			state.ellen[player][slot] = {
				source.x,
				source.y,
				source.velocity_x,
				source.velocity_y,
			};
		}
		const auto &prefix = bombs.ellen_prefix_[player];
		state.ellen_prefix[player] = {
			prefix.active,
			prefix.frame,
			prefix.next_slot,
		};
	}
	state.mima_read_frame = bombs.mima_read_frame_;
	state.ellen_release_cursor = bombs.ellen_release_cursor_;
	state.marisa_point = {
		bombs.marisa_point_.initialized,
		bombs.marisa_point_.screen_x,
		bombs.marisa_point_.screen_y,
	};
	state.rikako_radius = bombs.rikako_radius_;
	state.bombs_status = bombs.status_;

	state.warning_ramp_value = warning_gauge.ramp_value_;
	state.warning_ramp_decreasing = warning_gauge.ramp_decreasing_;
	state.warning_gauge_level = warning_gauge.warning_gauge_level_;
	state.warning_level_valid = warning_gauge.warning_level_valid_;
	state.warning_boss_level = warning_gauge.warning_boss_level_;
	state.warning_next_interstitial_sample =
		warning_gauge.next_interstitial_sample_;
	state.warning_interstitial_active = warning_gauge.interstitial_active_;
	state.warning_status = warning_gauge.status_;

	for(std::size_t slot = 0U; slot < state.hud_banner.size(); ++slot) {
		const auto &source = hud_result.hud_banner_[slot];
		state.hud_banner[slot] = {
			source.x,
			source.y,
			source.velocity,
			source.active,
		};
	}
	for(std::size_t slot = 0U; slot < state.hud_particles.size(); ++slot) {
		const auto &source = hud_result.hud_particles_[slot];
		state.hud_particles[slot] = {
			source.x_q4,
			source.y_q4,
			source.angle,
			source.angle_delta_first,
			source.angle_delta_second,
			source.active,
		};
	}
	state.hud_compatibility_read_count =
		hud_result.hud_compatibility_read_count_;
	state.hud_banner_x = hud_result.hud_banner_x_;
	state.hud_pattern_base = hud_result.hud_pattern_base_;
	state.hud_particle_reads_pending = hud_result.hud_particle_reads_pending_;
	state.hud_completed_batch_waiting =
		hud_result.hud_completed_batch_waiting_;
	state.hud_late_state_pending = hud_result.hud_late_state_pending_;
	state.hud_expected_story_mode = hud_result.hud_expected_story_mode_;
	state.hud_expected_story_stage = hud_result.hud_expected_story_stage_;
	state.hud_expected_round_id = hud_result.hud_expected_round_id_;
	state.hud_expected_state_after = hud_result.hud_expected_state_after_;
	state.hud_expected_frame_after = hud_result.hud_expected_frame_after_;
	state.result_snapshot = hud_result.result_snapshot_;
	state.result_initial_remainder = hud_result.result_initial_remainder_;
	state.result_initial_quotient = hud_result.result_initial_quotient_;
	state.result_final_lives = hud_result.result_final_lives_;
	state.transition_coordinates = hud_result.transition_coordinates_;
	state.hud_status = hud_result.status_;
}

void MainPresenterRollbackAccess::restore(
	MainPaletteState &palette,
	MainStaticPresenter &static_presenter,
	MainBackgroundPresenter &background,
	MainBossPresenter &bosses,
	MainEntitiesPresenter &entities,
	MainEffectsPresenter &effects,
	MainExtraChargePresenter &extra_charge,
	MainPlayersBulletsPresenter &players_bullets,
	MainBombsPresenter &bombs,
	MainWarningGaugePresenter &warning_gauge,
	MainHudResultPresenter &hud_result,
	const MainPresenterRollbackState &state
) noexcept
{
	palette.baseline_ = state.palette_baseline;
	palette.baseline_ready_ = state.palette_baseline_ready;
	palette.dirty_ = state.palette_dirty;
	static_presenter.pages_ = state.page_roles;
	static_presenter.started_ = state.static_started;
	static_presenter.accelerator_roles_crossed_ =
		state.accelerator_roles_crossed;

	for(std::size_t slot = 0U; slot < state.background.size(); ++slot) {
		const auto &source = state.background[slot];
		background.records_[slot] = {
			source.x_q12_4,
			source.y_q12_4,
			source.velocity_x_q12_4,
			source.velocity_y_q12_4,
			source.speed_q12_4,
			source.sprite_variant,
		};
	}
	background.kind_ = state.background_kind;
	background.read_stream_ = static_cast<MainBackgroundPresenter::ReadStream>(
		state.background_read_stream
	);
	background.read_slot_ = state.background_read_slot;
	background.read_field_ = state.background_read_field;
	background.initial_frames_completed_ =
		state.background_initial_frames_completed;
	background.direction_ = state.background_direction;
	background.round_id_ = state.background_round_id;
	background.initialized_ = state.background_initialized;
	background.have_round_id_ = state.background_have_round_id;
	background.vector_latch_ = state.background_vector_latch;
	background.rendering_enabled_ = state.background_rendering_enabled;
	background.status_ = state.background_status;

	bosses.activation_ = {
		state.boss_activation.active,
		state.boss_activation.owner,
		state.boss_activation.paletted_character,
		static_cast<MainBossPresenter::Character>(state.boss_activation.character),
	};
	bosses.ellen_phase_ = state.boss_ellen_phase;
	bosses.rikako_angle_ = state.boss_rikako_angle;
	bosses.rikako_angle_delta_ = state.boss_rikako_angle_delta;
	bosses.chiyuri_phase_ = state.boss_chiyuri_phase;
	bosses.status_ = state.boss_status;

	entities.shot_cels_ = state.entity_shot_cels;
	entities.shot_players_ = state.entity_shot_players;
	entities.shot_spawn_observed_ = state.entity_shot_spawn_observed;
	entities.status_ = state.entities_status;

	for(std::size_t slot = 0U; slot < state.hit_circles.size(); ++slot) {
		const auto &source = state.hit_circles[slot];
		effects.hit_circles_[slot] = {
			source.age,
			source.player,
			source.left,
			source.top,
		};
	}
	for(std::size_t slot = 0U; slot < state.common_effects.size(); ++slot) {
		const auto &source = state.common_effects[slot];
		effects.common_effects_[slot] = {
			source.type,
			source.age,
			source.center_x,
			source.center_y,
			source.radius,
			source.radius_delta,
			source.angle,
		};
	}
	effects.enemy_hit_circle_cursor_ = state.enemy_hit_circle_cursor;
	effects.common_effect_cursor_ = state.common_effect_cursor;
	effects.kana_bomb_angle_ = state.kana_bomb_angle;
	effects.kotohime_bomb_y_ = state.kotohime_bomb_y;
	effects.status_ = state.effects_status;

	extra_charge.kana_destination_cache_ = state.kana_destination_cache;
	extra_charge.status_ = state.extra_charge_status;

	players_bullets.shared_velocity_x_q4_4_ = state.shared_velocity_x_q4_4;
	for(std::size_t player = 0U; player < state.player_poses.size(); ++player) {
		players_bullets.player_poses_[player] =
			static_cast<MainPlayersBulletsPresenter::PlayerPose>(
				state.player_poses[player]
			);
	}
	players_bullets.defeat_ = {
		state.defeat.anchor_left,
		state.defeat.anchor_top,
		state.defeat.radius,
	};
	for(std::size_t slot = 0U; slot < state.bullet16_styles.size(); ++slot) {
		const auto &source = state.bullet16_styles[slot];
		players_bullets.bullet16_styles_[slot] = {
			source.source_player,
			source.construction,
			source.animated,
			source.observed,
		};
	}
	players_bullets.status_ = state.players_bullets_status;

	bombs.palette_interpreter_.status_ = state.bomb_palette_status;
	for(std::size_t player = 0U; player < state.bomb_activations.size(); ++player) {
		const auto &source = state.bomb_activations[player];
		bombs.activations_[player] = {
			source.active,
			source.paletted_character,
			static_cast<MainBombsPresenter::Character>(source.character),
			source.frame,
			source.entry_shift,
		};
	}
	bombs.pending_ = {
		static_cast<MainBombsPresenter::PendingKind>(state.bomb_pending.kind),
		state.bomb_pending.player,
		state.bomb_pending.frame,
		state.bomb_pending.received,
	};
	for(std::size_t player = 0U; player < state.reimu.size(); ++player) {
		for(std::size_t slot = 0U; slot < state.reimu[player].size(); ++slot) {
			const auto &source = state.reimu[player][slot];
			bombs.reimu_[player][slot] = {
				source.active,
				source.x,
				source.y,
				source.velocity_y,
			};
		}
		const auto &read = state.reimu_reads[player];
		bombs.reimu_reads_[player] = {
			static_cast<MainBombsPresenter::ReimuReadPhase>(read.phase),
			read.frame,
			read.velocity_y,
		};
		for(std::size_t slot = 0U; slot < state.mima[player].size(); ++slot) {
			const auto &source = state.mima[player][slot];
			bombs.mima_[player][slot] = {source.active, source.x};
		}
		for(std::size_t slot = 0U; slot < state.ellen[player].size(); ++slot) {
			const auto &source = state.ellen[player][slot];
			bombs.ellen_[player][slot] = {
				source.x,
				source.y,
				source.velocity_x,
				source.velocity_y,
			};
		}
		const auto &prefix = state.ellen_prefix[player];
		bombs.ellen_prefix_[player] = {
			prefix.active,
			prefix.frame,
			prefix.next_slot,
		};
	}
	bombs.mima_read_frame_ = state.mima_read_frame;
	bombs.ellen_release_cursor_ = state.ellen_release_cursor;
	bombs.marisa_point_ = {
		state.marisa_point.initialized,
		state.marisa_point.screen_x,
		state.marisa_point.screen_y,
	};
	bombs.rikako_radius_ = state.rikako_radius;
	bombs.status_ = state.bombs_status;

	warning_gauge.ramp_value_ = state.warning_ramp_value;
	warning_gauge.ramp_decreasing_ = state.warning_ramp_decreasing;
	warning_gauge.warning_gauge_level_ = state.warning_gauge_level;
	warning_gauge.warning_level_valid_ = state.warning_level_valid;
	warning_gauge.warning_boss_level_ = state.warning_boss_level;
	warning_gauge.next_interstitial_sample_ =
		state.warning_next_interstitial_sample;
	warning_gauge.interstitial_active_ = state.warning_interstitial_active;
	warning_gauge.nonblocking_interstitial_frames_ = 0U;
	warning_gauge.nonblocking_interstitial_player_mask_ = 0U;
	warning_gauge.status_ = state.warning_status;

	for(std::size_t slot = 0U; slot < state.hud_banner.size(); ++slot) {
		const auto &source = state.hud_banner[slot];
		hud_result.hud_banner_[slot] = {
			source.x,
			source.y,
			source.velocity,
			source.active,
		};
	}
	for(std::size_t slot = 0U; slot < state.hud_particles.size(); ++slot) {
		const auto &source = state.hud_particles[slot];
		hud_result.hud_particles_[slot] = {
			source.x_q4,
			source.y_q4,
			source.angle,
			source.angle_delta_first,
			source.angle_delta_second,
			source.active,
		};
	}
	hud_result.hud_compatibility_read_count_ =
		state.hud_compatibility_read_count;
	hud_result.hud_banner_x_ = state.hud_banner_x;
	hud_result.hud_pattern_base_ = state.hud_pattern_base;
	hud_result.hud_particle_reads_pending_ = state.hud_particle_reads_pending;
	hud_result.hud_completed_batch_waiting_ =
		state.hud_completed_batch_waiting;
	hud_result.hud_late_state_pending_ = state.hud_late_state_pending;
	hud_result.hud_expected_story_mode_ = state.hud_expected_story_mode;
	hud_result.hud_expected_story_stage_ = state.hud_expected_story_stage;
	hud_result.hud_expected_round_id_ = state.hud_expected_round_id;
	hud_result.hud_expected_state_after_ = state.hud_expected_state_after;
	hud_result.hud_expected_frame_after_ = state.hud_expected_frame_after;
	hud_result.result_snapshot_ = state.result_snapshot;
	hud_result.result_initial_remainder_ = state.result_initial_remainder;
	hud_result.result_initial_quotient_ = state.result_initial_quotient;
	hud_result.result_final_lives_ = state.result_final_lives;
	hud_result.transition_coordinates_ = state.transition_coordinates;
	hud_result.status_ = state.hud_status;
}

} // namespace th03::present
