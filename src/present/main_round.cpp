#include "th03/present/main_round.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace th03::present {
namespace {

constexpr std::uint8_t warning_portrait_step = 1U;

} // namespace

MainRoundPresenter::MainRoundPresenter(
	Raster &raster,
	Tram &tram,
	MainStaticPresenter &static_presenter,
	MainPaletteState &palette,
	MainBackgroundPresenter &background,
	MainBossPresenter &bosses,
	MainEntitiesPresenter &entities,
	MainEffectsPresenter &effects,
	MainExtraChargePresenter &extra_charge,
	MainPlayersBulletsPresenter &players_bullets,
	MainBombsPresenter &bombs,
	MainWarningGaugePresenter &warning_gauge,
	MainHudResultPresenter &hud_result
) noexcept :
	raster_(&raster),
	tram_(&tram),
	static_(&static_presenter),
	palette_(&palette),
	background_(&background),
	bosses_(&bosses),
	entities_(&entities),
	effects_(&effects),
	extra_charge_(&extra_charge),
	players_bullets_(&players_bullets),
	bombs_(&bombs),
	warning_gauge_(&warning_gauge),
	hud_result_(&hud_result)
{
	if(!static_->started() || !palette_->baseline_ready()) {
		status_ = MainRoundStatus::invalid_setup;
		return;
	}
	if(!background_->ready()) {
		status_ = MainRoundStatus::background_failed;
		return;
	}
	if(!bosses_->ready()) {
		status_ = MainRoundStatus::bosses_failed;
		return;
	}
	if(!entities_->ready()) {
		status_ = MainRoundStatus::entities_failed;
		return;
	}
	if(!effects_->ready()) {
		status_ = MainRoundStatus::effects_failed;
		return;
	}
	if(!extra_charge_->ready()) {
		status_ = MainRoundStatus::extra_charge_failed;
		return;
	}
	if(!players_bullets_->ready()) {
		status_ = MainRoundStatus::players_bullets_failed;
		return;
	}
	if(!bombs_->ready()) {
		status_ = MainRoundStatus::bombs_failed;
		return;
	}
	if(!warning_gauge_->ready()) {
		status_ = MainRoundStatus::warning_gauge_failed;
		return;
	}
	if(!hud_result_->ready()) {
		status_ = MainRoundStatus::hud_result_failed;
		return;
	}
	if(!source_page_ready()) {
		status_ = MainRoundStatus::page_state_mismatch;
		return;
	}

	// entry.cpp follows the first MAIN setup with this immediate show before
	// entering sub_9778(). Invalid construction above never mutates the
	// caller-owned process state.
	palette_->set_tone_and_show_now(*raster_, 100);
}

void MainRoundPresenter::fail(MainRoundStatus status) noexcept
{
	if(ready()) {
		status_ = status;
	}
}

bool MainRoundPresenter::source_page_ready() const noexcept
{
	const auto pages = static_->page_roles();
	return raster_->access_page() == pages.back &&
		raster_->shown_page() == pages.front;
}

bool MainRoundPresenter::source_prefix_open() const noexcept
{
	return phase_ == Phase::boss && !warning_interstitial_active_ &&
		!warning_suffix_pending_ && !warning_lifecycle_seen_ &&
		!background_frame_seen_;
}

bool MainRoundPresenter::late_bombs_open() const noexcept
{
	return phase_ == Phase::boss && !warning_interstitial_active_ &&
		!warning_suffix_pending_ && warning_lifecycle_seen_ &&
		background_frame_seen_ && background_frame_reads_remaining_ == 0U;
}

bool MainRoundPresenter::same_frame(
	const MainPhaseContext &context
) const noexcept
{
	return context == frame_context_;
}

void MainRoundPresenter::shot_pair_spawned(
	const ShotPairSpawned &spawn
) noexcept
{
	if(!ready()) {
		return;
	}
	entities_->shot_pair_spawned(spawn);
	if(!entities_->ready()) {
		fail(MainRoundStatus::entities_failed);
	}
}

void MainRoundPresenter::bullet16_spawned(
	const Bullet16Spawned &spawn
) noexcept
{
	if(!ready()) {
		return;
	}
	players_bullets_->bullet16_spawned(spawn);
	if(!players_bullets_->ready()) {
		fail(MainRoundStatus::players_bullets_failed);
	}
}

void MainRoundPresenter::player_pose_committed(
	const PlayerPoseCommitted &pose
) noexcept
{
	if(!ready()) {
		return;
	}
	players_bullets_->player_pose_committed(pose);
	if(!players_bullets_->ready()) {
		fail(MainRoundStatus::players_bullets_failed);
	}
}

void MainRoundPresenter::player_defeat_step(
	const PlayerDefeatStep &step
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto player = static_cast<std::size_t>(step.player);
	if(player >= PlayersBulletsView::player_count ||
		step.lose_anim_time_before == 0U ||
		step.lose_anim_time_before > 0x30U) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}

	// sub_C0D8() updates the shared defeat geometry before its immediate
	// PaletteTone write and palette_show().
	players_bullets_->player_defeat_step(step);
	if(!players_bullets_->ready()) {
		fail(MainRoundStatus::players_bullets_failed);
		return;
	}
	const std::int16_t tone = (step.lose_anim_time_before == 1U) ?
		std::int16_t{100} :
		static_cast<std::int16_t>(
			100 + (30 * (step.round_or_result_frame_before & 1U))
		);
	palette_->set_tone_and_show_now(*raster_, tone);
}

void MainRoundPresenter::hit_circles_advanced() noexcept
{
	if(!ready()) {
		return;
	}
	effects_->hit_circles_advanced();
	if(!effects_->ready()) {
		fail(MainRoundStatus::effects_failed);
	}
}

void MainRoundPresenter::hit_circle_spawned(
	const HitCircleSpawned &spawn
) noexcept
{
	if(!ready()) {
		return;
	}
	effects_->hit_circle_spawned(spawn);
	if(!effects_->ready()) {
		fail(MainRoundStatus::effects_failed);
	}
}

void MainRoundPresenter::common_effects_advanced() noexcept
{
	if(!ready()) {
		return;
	}
	effects_->common_effects_advanced();
	if(!effects_->ready()) {
		fail(MainRoundStatus::effects_failed);
	}
}

void MainRoundPresenter::common_effect_spawned(
	const CommonEffectSpawned &spawn
) noexcept
{
	if(!ready()) {
		return;
	}
	if(phase_ != Phase::boss || warning_interstitial_active_ ||
		warning_suffix_pending_) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	// The source allocation completes in SUB_CE5B/SUB_CDBD/SUB_CE0C before
	// the active Bomb callback continues its dependent private drawing.
	effects_->common_effect_spawned(spawn);
	if(!effects_->ready()) {
		fail(MainRoundStatus::effects_failed);
		return;
	}
	bombs_->common_effect_spawned(spawn);
	if(!bombs_->ready()) {
		fail(MainRoundStatus::bombs_failed);
	}
}

void MainRoundPresenter::bomb_compatibility_read(
	const BombCompatibilityRead &read
) noexcept
{
	if(!ready()) {
		return;
	}
	const bool prefix_site =
		read.site == BombCompatibilityReadSite::mima_column_x ||
		read.site == BombCompatibilityReadSite::ellen_vector_angle;
	if((prefix_site && !source_prefix_open()) ||
		(!prefix_site && !late_bombs_open())) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	bombs_->bomb_compatibility_read(read);
	if(!bombs_->ready()) {
		fail(MainRoundStatus::bombs_failed);
	}
}

void MainRoundPresenter::bomb_finished(
	const BombFinished &finished
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!source_prefix_open()) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	bombs_->bomb_finished(finished);
	if(!bombs_->ready()) {
		fail(MainRoundStatus::bombs_failed);
	}
}

void MainRoundPresenter::bomb_frame_step(const BombFrameStep &step) noexcept
{
	if(!ready()) {
		return;
	}
	const auto player = static_cast<std::size_t>(step.player);
	if(!late_bombs_open() || player >= 2U ||
		player < next_late_bomb_player_) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	bombs_->bomb_frame_step(step);
	if(!bombs_->ready()) {
		fail(MainRoundStatus::bombs_failed);
		return;
	}
	next_late_bomb_player_ = static_cast<std::uint8_t>(player + 1U);
}

void MainRoundPresenter::bomb_common_effect_step(
	const BombCommonEffectStep &step
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!late_bombs_open()) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	// MainEffects consumes the source-shared position first; MainBombs then
	// completes the remainder of that same character callback.
	effects_->bomb_common_effect_step(step);
	if(!effects_->ready()) {
		fail(MainRoundStatus::effects_failed);
		return;
	}
	bombs_->bomb_common_effect_step(step);
	if(!bombs_->ready()) {
		fail(MainRoundStatus::bombs_failed);
	}
}

void MainRoundPresenter::warning_interstitial_started(
	const WarningInterstitialStarted &started
) noexcept
{
	if(!ready()) {
		return;
	}
	if(
		phase_ != Phase::boss || warning_interstitial_active_ ||
		warning_suffix_pending_ || warning_lifecycle_seen_ ||
		!hud_start_step_seen_ || hud_start_reads_remaining_ != 0U ||
		(!started.flashing_players[0U] && !started.flashing_players[1U])
	) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	warning_gauge_->warning_interstitial_started(started);
	if(!warning_gauge_->ready()) {
		fail(MainRoundStatus::warning_gauge_failed);
		return;
	}
	warning_interstitial_active_ = true;
	next_warning_sample_ = 0U;
}

void MainRoundPresenter::warning_interstitial_sample(
	const WarningInterstitialSample &sample
) noexcept
{
	if(!ready()) {
		return;
	}
	if(
		!warning_interstitial_active_ || sample.ordinal > 26U ||
		sample.ordinal != next_warning_sample_
	) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}

	warning_gauge_->warning_interstitial_sample(sample);
	if(!warning_gauge_->ready()) {
		fail(MainRoundStatus::warning_gauge_failed);
		return;
	}
	if(sample.ordinal == 26U) {
		warning_interstitial_active_ = false;
		warning_suffix_pending_ = true;
		next_warning_sample_ = 0U;
		return;
	}
	next_warning_sample_++;
}

void MainRoundPresenter::warning_frame_lifecycle(
	const WarningFrameLifecycle &lifecycle
) noexcept
{
	if(!ready()) {
		return;
	}
	if(
		phase_ != Phase::boss || warning_interstitial_active_ ||
		warning_lifecycle_seen_ || !hud_start_step_seen_ ||
		hud_start_reads_remaining_ != 0U
	) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	warning_gauge_->warning_frame_lifecycle(lifecycle);
	if(!warning_gauge_->ready()) {
		fail(MainRoundStatus::warning_gauge_failed);
		return;
	}
	warning_lifecycle_seen_ = true;
	warning_suffix_pending_ = false;
	next_warning_portrait_player_ = 0U;
	expected_portrait_mask_ = 0U;
	for(std::size_t player = 0U; player < lifecycle.players.size(); ++player) {
		if(lifecycle.players[player].warning_step_before ==
			warning_portrait_step) {
			expected_portrait_mask_ = static_cast<std::uint8_t>(
				expected_portrait_mask_ | (1U << player)
			);
		}
	}
}

void MainRoundPresenter::gauge_attack_finished(
	const GaugeAttackFinished &finished
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto attacker = static_cast<std::size_t>(finished.attacker);
	if(
		phase_ != Phase::boss || warning_interstitial_active_ ||
		warning_suffix_pending_ || warning_lifecycle_seen_ ||
		background_frame_seen_ || attacker >= 2U ||
		attacker < next_gauge_finish_player_ || !hud_start_step_seen_ ||
		hud_start_reads_remaining_ != 0U
	) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	warning_gauge_->gauge_attack_finished(finished);
	if(!warning_gauge_->ready()) {
		fail(MainRoundStatus::warning_gauge_failed);
		return;
	}
	next_gauge_finish_player_ = static_cast<std::uint8_t>(attacker + 1U);
}

void MainRoundPresenter::warning_portrait_transition(
	const WarningPortraitTransition &transition
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto player = static_cast<std::size_t>(transition.player);
	const auto player_bit = (player < 2U)
		? static_cast<std::uint8_t>(1U << player)
		: std::uint8_t{0U};
	const auto lower_mask = static_cast<std::uint8_t>(player_bit - 1U);
	// The prefix lifecycle only predicts portraits already pending at that
	// point. Charge and combo work can raise another source portrait later in
	// this same frame, so the callback itself is authoritative for presence;
	// player order and any predicted lower-player callback remain strict.
	if(phase_ != Phase::hud_start || player >= 2U ||
		player < next_warning_portrait_player_ ||
		(expected_portrait_mask_ & lower_mask) != 0U) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	warning_gauge_->warning_portrait_transition(transition);
	if(!warning_gauge_->ready()) {
		fail(MainRoundStatus::warning_gauge_failed);
		return;
	}
	expected_portrait_mask_ = static_cast<std::uint8_t>(
		expected_portrait_mask_ & static_cast<std::uint8_t>(~player_bit)
	);
	next_warning_portrait_player_ = static_cast<std::uint8_t>(player + 1U);
}

void MainRoundPresenter::main_gauge_state(const MainGaugeState &state) noexcept
{
	if(!ready()) {
		return;
	}
	if(phase_ != Phase::result_gate || !same_frame(state.context)) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	warning_gauge_->main_gauge_state(state);
	if(!warning_gauge_->ready()) {
		fail(MainRoundStatus::warning_gauge_failed);
		return;
	}
	phase_ = Phase::score;
}

void MainRoundPresenter::hud_start_frame_step(
	const HudStartFrameStep &step
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!source_prefix_open() || hud_start_step_seen_ ||
		step.state_before > 4U ||
		(step.initialize &&
			(step.state_before != 0U || step.frame_before != 0U))) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	hud_result_->hud_start_frame_step(step);
	if(!hud_result_->ready()) {
		fail(MainRoundStatus::hud_result_failed);
		return;
	}
	hud_start_step_seen_ = true;
	hud_start_reads_remaining_ = (step.state_before == 2U) ? 512U : 0U;
}

void MainRoundPresenter::hud_start_compatibility_read(
	const HudStartCompatibilityRead &read
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!source_prefix_open() || !hud_start_step_seen_ ||
		hud_start_reads_remaining_ == 0U) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	hud_result_->hud_start_compatibility_read(read);
	if(!hud_result_->ready()) {
		fail(MainRoundStatus::hud_result_failed);
		return;
	}
	hud_start_reads_remaining_--;
}

void MainRoundPresenter::main_hud_player_value_changed(
	const MainHudPlayerValueChanged &changed
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!source_prefix_open() || !hud_start_step_seen_ ||
		hud_start_reads_remaining_ != 0U) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	hud_result_->main_hud_player_value_changed(changed);
	if(!hud_result_->ready()) {
		fail(MainRoundStatus::hud_result_failed);
	}
}

void MainRoundPresenter::main_hud_story_lives_changed(
	const MainHudStoryLivesChanged &changed
) noexcept
{
	if(!ready()) {
		return;
	}
	if(phase_ != Phase::hud_start || story_lives_changed_) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	hud_result_->main_hud_story_lives_changed(changed);
	if(!hud_result_->ready()) {
		fail(MainRoundStatus::hud_result_failed);
		return;
	}
	story_lives_changed_ = true;
}

void MainRoundPresenter::main_result_state(
	const MainResultState &state
) noexcept
{
	if(!ready()) {
		return;
	}
	if(phase_ != Phase::result || !same_frame(state.context)) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	hud_result_->main_result_state(state);
	if(!hud_result_->ready()) {
		fail(MainRoundStatus::hud_result_failed);
		return;
	}
	repeat_result_ready_ = state.defeated_player.has_value() &&
		state.winner.has_value();
	phase_ = Phase::result_gate;
}

void MainRoundPresenter::main_score_state(const MainScoreState &state) noexcept
{
	if(!ready()) {
		return;
	}
	if(phase_ != Phase::score || !same_frame(state.context)) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	hud_result_->main_score_state(state);
	if(!hud_result_->ready()) {
		fail(MainRoundStatus::hud_result_failed);
		return;
	}
	phase_ = Phase::hud_start;
}

void MainRoundPresenter::main_hud_start_state(
	const MainHudStartState &state
) noexcept
{
	if(!ready()) {
		return;
	}
	if(phase_ != Phase::hud_start || expected_portrait_mask_ != 0U ||
		!hud_start_step_seen_ || hud_start_reads_remaining_ != 0U ||
		state.round_id != frame_context_.round_id) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	hud_result_->main_hud_start_state(state);
	if(!hud_result_->ready()) {
		fail(MainRoundStatus::hud_result_failed);
		return;
	}
	phase_ = Phase::combo;
}

void MainRoundPresenter::main_combo_state(const MainComboState &state) noexcept
{
	if(!ready()) {
		return;
	}
	if(phase_ != Phase::combo || !same_frame(state.context)) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	hud_result_->main_combo_state(state);
	if(!hud_result_->ready()) {
		fail(MainRoundStatus::hud_result_failed);
		return;
	}
	phase_ = Phase::result_transition;
}

void MainRoundPresenter::main_result_transition_state(
	const MainResultTransitionState &state
) noexcept
{
	if(!ready()) {
		return;
	}
	if(phase_ != Phase::result_transition || !same_frame(state.context)) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	hud_result_->main_result_transition_state(state);
	if(!hud_result_->ready()) {
		fail(MainRoundStatus::hud_result_failed);
		return;
	}
	phase_ = Phase::footer;
}

void MainRoundPresenter::main_background_initialize(
	const MainBackgroundInitialize &initialize
) noexcept
{
	if(!ready()) {
		return;
	}
	background_->main_background_initialize(initialize);
	if(!background_->ready()) {
		fail(MainRoundStatus::background_failed);
	}
}

void MainRoundPresenter::main_background_frame(
	const MainBackgroundFrame &frame
) noexcept
{
	if(!ready()) {
		return;
	}
	if(
		phase_ != Phase::boss || warning_interstitial_active_ ||
		!warning_lifecycle_seen_ || background_frame_seen_ ||
		!hud_start_step_seen_ || hud_start_reads_remaining_ != 0U ||
		!source_page_ready()
	) {
		fail(
			!source_page_ready() ? MainRoundStatus::page_state_mismatch :
				MainRoundStatus::invalid_observation
		);
		return;
	}
	background_->main_background_frame(frame);
	if(!background_->ready()) {
		fail(MainRoundStatus::background_failed);
		return;
	}
	background_frame_seen_ = true;
	background_round_id_ = frame.round_id;
	background_round_or_result_frame_ = frame.round_or_result_frame;
	background_frame_reads_remaining_ =
		(frame.phase == MainBackgroundPhase::initial)
			? static_cast<std::uint8_t>(
				(frame.kind == MainBackgroundKind::type_a) ? 48U : 40U
			)
			: 0U;
}

void MainRoundPresenter::main_background_random_read(
	const MainBackgroundRandomRead &read
) noexcept
{
	if(!ready()) {
		return;
	}
	if(background_frame_seen_ && background_frame_reads_remaining_ == 0U) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	background_->main_background_random_read(read);
	if(!background_->ready()) {
		fail(MainRoundStatus::background_failed);
		return;
	}
	if(background_frame_seen_) {
		background_frame_reads_remaining_--;
	}
}

void MainRoundPresenter::boss_activated(
	const BossActivated &activated
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!source_prefix_open()) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	bosses_->boss_activated(activated);
	if(!bosses_->ready()) {
		fail(MainRoundStatus::bosses_failed);
	}
}

void MainRoundPresenter::boss_animation_step(
	const BossAnimationStep &step
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!source_prefix_open()) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	bosses_->boss_animation_step(step);
	if(!bosses_->ready()) {
		fail(MainRoundStatus::bosses_failed);
	}
}

void MainRoundPresenter::boss_palette_baseline_restored(
	const BossPaletteBaselineRestored &restored
) noexcept
{
	if(!ready()) {
		return;
	}
	const bool prefix_restore = source_prefix_open();
	const bool terminal_restore = phase_ == Phase::shots_enemies;
	if(!prefix_restore && !terminal_restore) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	bosses_->boss_palette_baseline_restored(restored);
	if(!bosses_->ready()) {
		fail(MainRoundStatus::bosses_failed);
	}
}

void MainRoundPresenter::observe(const MainBossView &view) noexcept
{
	if(!ready()) {
		return;
	}
	if(
		phase_ != Phase::boss || warning_interstitial_active_ ||
		!warning_lifecycle_seen_ || !background_frame_seen_ ||
		background_frame_reads_remaining_ != 0U || !source_page_ready()
	) {
		fail(
			!source_page_ready() ? MainRoundStatus::page_state_mismatch :
				MainRoundStatus::invalid_observation
		);
		return;
	}
	const auto context = view.context();
	if(background_round_id_ != context.round_id ||
		background_round_or_result_frame_ != context.round_or_result_frame) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	frame_context_ = context;
	bosses_->observe(view);
	if(!bosses_->ready()) {
		fail(MainRoundStatus::bosses_failed);
		return;
	}
	phase_ = Phase::shots_enemies;
}

void MainRoundPresenter::observe(const ShotsEnemiesView &view) noexcept
{
	if(!ready()) {
		return;
	}
	if(
		phase_ != Phase::shots_enemies || warning_interstitial_active_ ||
		!source_page_ready()
	) {
		fail(
			!source_page_ready() ? MainRoundStatus::page_state_mismatch :
				MainRoundStatus::invalid_observation
		);
		return;
	}
	const auto context = view.context();
	if(!same_frame(context)) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	entities_->observe(view);
	if(!entities_->ready()) {
		fail(MainRoundStatus::entities_failed);
		return;
	}
	phase_ = Phase::fireballs;
}

void MainRoundPresenter::observe(const FireballsView &view) noexcept
{
	if(!ready()) {
		return;
	}
	const auto context = view.context();
	if(phase_ != Phase::fireballs || !same_frame(context)) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	entities_->observe(view);
	if(!entities_->ready()) {
		fail(MainRoundStatus::entities_failed);
		return;
	}
	if(!effects_->present_hit_circles(context)) {
		fail(MainRoundStatus::effects_failed);
		return;
	}
	phase_ = Phase::extra_p1;
}

void MainRoundPresenter::observe(const MainExtraAttackView &view) noexcept
{
	if(!ready()) {
		return;
	}
	const auto context = view.context();
	const auto expected_player = (phase_ == Phase::extra_p1)
		? PlayerId::p1
		: PlayerId::p2;
	if(
		(phase_ != Phase::extra_p1 && phase_ != Phase::extra_p2) ||
		view.player() != expected_player || !same_frame(context)
	) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	extra_charge_->observe(view);
	if(!extra_charge_->ready()) {
		fail(MainRoundStatus::extra_charge_failed);
		return;
	}
	phase_ = (phase_ == Phase::extra_p1) ? Phase::charge_p1 : Phase::charge_p2;
}

void MainRoundPresenter::observe(const MainChargeShotView &view) noexcept
{
	if(!ready()) {
		return;
	}
	const auto context = view.context();
	const auto expected_player = (phase_ == Phase::charge_p1)
		? PlayerId::p1
		: PlayerId::p2;
	if(
		(phase_ != Phase::charge_p1 && phase_ != Phase::charge_p2) ||
		view.player() != expected_player || !same_frame(context)
	) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	extra_charge_->observe(view);
	if(!extra_charge_->ready()) {
		fail(MainRoundStatus::extra_charge_failed);
		return;
	}
	phase_ = (phase_ == Phase::charge_p1)
		? Phase::extra_p2
		: Phase::players_bullets;
}

bool MainRoundPresenter::present_common_effects(
	void *context,
	const MainPhaseContext &phase
) noexcept
{
	auto &self = *static_cast<MainRoundPresenter *>(context);
	if(!self.effects_->present_common_effects(phase)) {
		self.fail(MainRoundStatus::effects_failed);
		return false;
	}
	return true;
}

void MainRoundPresenter::observe(const PlayersBulletsView &view) noexcept
{
	if(!ready()) {
		return;
	}
	const auto context = view.context();
	if(phase_ != Phase::players_bullets || !same_frame(context)) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}

	// The view is borrowed. Materialize its complete final phase, including
	// every required trail, before invoking the all-facts validation seam.
	player_facts_ = {};
	bullet_facts_ = {};
	bullet_trails_ = {};
	for(std::size_t player = 0U; player < player_facts_.size(); ++player) {
		const auto fact = view.player(static_cast<PlayerId>(player));
		if(!fact.has_value()) {
			fail(MainRoundStatus::invalid_observation);
			return;
		}
		player_facts_[player] = *fact;
	}
	for(std::size_t slot = 0U; slot < bullet_facts_.size(); ++slot) {
		const auto fact = view.bullet(static_cast<BulletSlot>(slot));
		if(!fact.has_value()) {
			fail(MainRoundStatus::invalid_observation);
			return;
		}
		bullet_facts_[slot] = *fact;
		if(!fact->flag.is_bullet16() || !fact->has_trail) {
			continue;
		}
		for(std::size_t point = 0U; point < bullet_trails_[slot].size(); ++point) {
			const auto position = view.trail_point(
				static_cast<BulletSlot>(slot),
				static_cast<TrailPoint>(point)
			);
			if(!position.has_value()) {
				fail(MainRoundStatus::invalid_observation);
				return;
			}
			bullet_trails_[slot][point] = *position;
		}
	}

	if(!players_bullets_->present_phase_with_middle(
		context,
		player_facts_,
		bullet_facts_,
		bullet_trails_,
		present_common_effects,
		this
	)) {
		if(ready()) {
			fail(MainRoundStatus::players_bullets_failed);
		}
		return;
	}
	phase_ = Phase::result;
}

void MainRoundPresenter::main_frame_footer_reached(
	const MainFrameFooterReached &
) noexcept
{
	if(!ready()) {
		return;
	}
	if(phase_ != Phase::footer || expected_portrait_mask_ != 0U) {
		fail(MainRoundStatus::invalid_observation);
		return;
	}
	if(!source_page_ready()) {
		fail(MainRoundStatus::page_state_mismatch);
		return;
	}

	// sub_9778() flushes palette_changed before publishing page_back.
	(void)palette_->apply_if_changed(*raster_);
	if(!static_->end_frame_swap_and_clear(*raster_)) {
		fail(MainRoundStatus::page_transaction_failed);
		return;
	}
	phase_ = Phase::boss;
	warning_lifecycle_seen_ = false;
	background_frame_seen_ = false;
	background_frame_reads_remaining_ = 0U;
	next_gauge_finish_player_ = 0U;
	next_late_bomb_player_ = 0U;
	next_warning_portrait_player_ = 0U;
	expected_portrait_mask_ = 0U;
	hud_start_reads_remaining_ = 0U;
	hud_start_step_seen_ = false;
	story_lives_changed_ = false;
	repeat_result_ready_ = false;
}

bool MainRoundPresenter::repeat_round_startup(
	const MainHudState &hud
) noexcept
{
	if(!ready()) {
		return false;
	}
	if(phase_ != Phase::result_gate || !repeat_result_ready_) {
		fail(MainRoundStatus::invalid_observation);
		return false;
	}
	if(!source_page_ready()) {
		fail(MainRoundStatus::page_state_mismatch);
		return false;
	}
	if(!static_->repeat_round_startup(*raster_, *tram_, hud, *palette_)) {
		fail(MainRoundStatus::repeat_startup_failed);
		return false;
	}
	palette_->set_tone_and_show_now(*raster_, 100);
	phase_ = Phase::boss;
	warning_interstitial_active_ = false;
	warning_suffix_pending_ = false;
	warning_lifecycle_seen_ = false;
	background_frame_seen_ = false;
	background_frame_reads_remaining_ = 0U;
	next_warning_sample_ = 0U;
	next_gauge_finish_player_ = 0U;
	next_late_bomb_player_ = 0U;
	next_warning_portrait_player_ = 0U;
	expected_portrait_mask_ = 0U;
	hud_start_reads_remaining_ = 0U;
	hud_start_step_seen_ = false;
	story_lives_changed_ = false;
	repeat_result_ready_ = false;
	return true;
}

std::expected<SourceSurface, SourceSurfaceError>
MainRoundPresenter::compose_shown_surface(
	std::span<ComposedPixel, graphics_page_pixels> output,
	const AnkGlyphProvider &ank
) const
{
	return compose_source_surface(output, *raster_, *tram_, ank);
}

MainRoundStatus MainRoundPresenter::status() const noexcept
{
	return status_;
}

bool MainRoundPresenter::ready() const noexcept
{
	return status_ == MainRoundStatus::ready;
}

} // namespace th03::present
