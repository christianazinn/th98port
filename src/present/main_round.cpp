#include "th03/present/main_round.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace th03::present {

MainRoundPresenter::MainRoundPresenter(
	Raster &raster,
	Tram &tram,
	MainStaticPresenter &static_presenter,
	MainPaletteState &palette,
	MainEntitiesPresenter &entities,
	MainEffectsPresenter &effects,
	MainPlayersBulletsPresenter &players_bullets,
	MainBombsPresenter &bombs
) noexcept :
	raster_(&raster),
	tram_(&tram),
	static_(&static_presenter),
	palette_(&palette),
	entities_(&entities),
	effects_(&effects),
	players_bullets_(&players_bullets),
	bombs_(&bombs)
{
	if(!static_->started() || !palette_->baseline_ready()) {
		status_ = MainRoundStatus::invalid_setup;
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
	if(!players_bullets_->ready()) {
		status_ = MainRoundStatus::players_bullets_failed;
		return;
	}
	if(!bombs_->ready()) {
		status_ = MainRoundStatus::bombs_failed;
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
	bombs_->bomb_frame_step(step);
	if(!bombs_->ready()) {
		fail(MainRoundStatus::bombs_failed);
	}
}

void MainRoundPresenter::bomb_common_effect_step(
	const BombCommonEffectStep &step
) noexcept
{
	if(!ready()) {
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
		phase_ != Phase::shots_enemies || warning_interstitial_active_ ||
		warning_suffix_pending_ ||
		(!started.flashing_players[0U] && !started.flashing_players[1U])
	) {
		fail(MainRoundStatus::invalid_observation);
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

	// SUB_CA3C() shows 100, 150, ... immediately after each accepted input
	// latch. These direct shows preserve any deferred palette_changed latch.
	palette_->set_tone_and_show_now(
		*raster_,
		static_cast<std::int16_t>(100 + (50 * (sample.ordinal & 1U)))
	);
	if(sample.ordinal == 26U) {
		warning_interstitial_active_ = false;
		warning_suffix_pending_ = true;
		next_warning_sample_ = 0U;
		return;
	}
	next_warning_sample_++;
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
	frame_context_ = view.context();
	entities_->observe(view);
	if(!entities_->ready()) {
		fail(MainRoundStatus::entities_failed);
		return;
	}
	warning_suffix_pending_ = false;
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
	phase_ = Phase::players_bullets;
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
	phase_ = Phase::footer;
}

void MainRoundPresenter::main_frame_footer_reached(
	const MainFrameFooterReached &
) noexcept
{
	if(!ready()) {
		return;
	}
	if(phase_ != Phase::footer) {
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
	phase_ = Phase::shots_enemies;
}

bool MainRoundPresenter::repeat_round_startup(
	const MainHudState &hud
) noexcept
{
	if(!ready()) {
		return false;
	}
	if(phase_ != Phase::footer) {
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
	phase_ = Phase::shots_enemies;
	warning_interstitial_active_ = false;
	warning_suffix_pending_ = false;
	next_warning_sample_ = 0U;
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
