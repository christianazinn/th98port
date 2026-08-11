#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <tuple>
#include <vector>

#include "th03/core/game.hpp"
#include "game_internal.hpp"

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

enum class ObservedPhase : std::uint8_t {
	shots_enemies,
	fireballs,
	extra_p1,
	charge_p1,
	extra_p2,
	charge_p2,
	players_bullets,
};

enum class ObservedCall : std::uint8_t {
	shot_pair_spawned,
	bullet16_spawned,
	player_defeat_step,
	player_pose_committed,
	hit_circles_advanced,
	hit_circle_spawned,
	common_effects_advanced,
	common_effect_spawned,
	bomb_compatibility_read,
	bomb_finished,
	bomb_frame_step,
	bomb_common_effect_step,
	warning_interstitial_started,
	warning_interstitial_sample,
	main_frame_footer_reached,
	shots_enemies,
	fireballs,
	extra_p1,
	charge_p1,
	extra_p2,
	charge_p2,
	players_bullets,
};

class DiscardObserver final : public th03::GameplayObserver {
};

enum class FeedbackObservedCall : std::uint8_t {
	request,
	dispatch_boundary,
};

class FeedbackRecordingObserver final : public th03::GameplayObserver {
public:
	void main_feedback_cue_requested(
		const th03::MainFeedbackCueRequested &request
	) noexcept override
	{
		requests.push_back(request);
		calls.push_back(FeedbackObservedCall::request);
	}

	void main_feedback_dispatch_boundary_reached() noexcept override
	{
		calls.push_back(FeedbackObservedCall::dispatch_boundary);
	}

	std::vector<th03::MainFeedbackCueRequested> requests;
	std::vector<FeedbackObservedCall> calls;
};

enum class BackgroundObservedCall : std::uint8_t {
	warning_sample,
	initialize,
	frame,
	random_read,
	bomb_frame_step,
};

enum class WarningGaugeObservedCall : std::uint8_t {
	gauge_attack_finished,
	warning_interstitial_sample,
	warning_frame_lifecycle,
	background_frame,
	shots_enemies,
	fireballs,
	players_bullets,
	main_gauge_state,
	warning_portrait_transition,
	main_frame_footer_reached,
};

enum class BossObservedCall : std::uint8_t {
	warning_frame_lifecycle,
	background_frame,
	background_random_read,
	palette_baseline_restored,
	activated,
	animation_step,
	bomb_frame_step,
	main_boss,
	shots_enemies,
};

class BackgroundRecordingObserver final : public th03::GameplayObserver {
public:
	BackgroundRecordingObserver()
	{
		initializations.reserve(1U);
		frames.reserve(2U);
		random_reads.reserve(256U);
		calls.reserve(258U);
	}

	void main_background_initialize(
		const th03::MainBackgroundInitialize &initialize
	) noexcept override
	{
		initializations.push_back(initialize);
		calls.push_back(BackgroundObservedCall::initialize);
	}

	void main_background_frame(
		const th03::MainBackgroundFrame &frame
	) noexcept override
	{
		frames.push_back(frame);
		calls.push_back(BackgroundObservedCall::frame);
	}

	void main_background_random_read(
		const th03::MainBackgroundRandomRead &read
	) noexcept override
	{
		random_reads.push_back(read);
		calls.push_back(BackgroundObservedCall::random_read);
	}

	void bomb_frame_step(const th03::BombFrameStep &) noexcept override
	{
		calls.push_back(BackgroundObservedCall::bomb_frame_step);
	}

	std::vector<th03::MainBackgroundInitialize> initializations;
	std::vector<th03::MainBackgroundFrame> frames;
	std::vector<th03::MainBackgroundRandomRead> random_reads;
	std::vector<BackgroundObservedCall> calls;
};

class RecordingObserver final : public th03::GameplayObserver {
public:
	explicit RecordingObserver(
		const th03::detail::DeterministicState *inspected_state = nullptr
	) : inspected_state_(inspected_state)
	{
		phases.reserve(8U);
		spawns.reserve(8U);
		bullet_spawns.reserve(64U);
		poses.reserve(8U);
		defeat_steps.reserve(8U);
		hit_circles.reserve(32U);
		common_effects.reserve(64U);
		bomb_compatibility_reads.reserve(32U);
		bomb_finishes.reserve(4U);
		bomb_frame_steps.reserve(8U);
		bomb_common_steps.reserve(8U);
		warning_interstitial_starts.reserve(2U);
		warning_interstitial_samples.reserve(27U);
		warning_lifecycles.reserve(2U);
		gauge_attack_finishes.reserve(2U);
		warning_portrait_transitions.reserve(2U);
		main_gauge_states.reserve(2U);
		warning_gauge_calls.reserve(16U);
		main_frame_footers.reserve(2U);
		boss_activations.reserve(2U);
		boss_animation_steps.reserve(8U);
		boss_palette_restores.reserve(4U);
		boss_views.reserve(2U);
		boss_calls.reserve(16U);
		background_bomb_calls.reserve(64U);
		calls.reserve(80U);
	}

	void shot_pair_spawned(const th03::ShotPairSpawned &spawn) noexcept override
	{
		spawns.push_back(spawn);
		calls.push_back(ObservedCall::shot_pair_spawned);
	}

	void bullet16_spawned(const th03::Bullet16Spawned &spawn) noexcept override
	{
		bullet_spawns.push_back(spawn);
		calls.push_back(ObservedCall::bullet16_spawned);
	}

	void player_pose_committed(
		const th03::PlayerPoseCommitted &pose
	) noexcept override
	{
		poses.push_back(pose);
		calls.push_back(ObservedCall::player_pose_committed);
	}

	void player_defeat_step(
		const th03::PlayerDefeatStep &step
	) noexcept override
	{
		defeat_steps.push_back(step);
		calls.push_back(ObservedCall::player_defeat_step);
	}

	void hit_circles_advanced() noexcept override
	{
		calls.push_back(ObservedCall::hit_circles_advanced);
	}

	void hit_circle_spawned(
		const th03::HitCircleSpawned &spawn
	) noexcept override
	{
		hit_circles.push_back(spawn);
		calls.push_back(ObservedCall::hit_circle_spawned);
	}

	void common_effects_advanced() noexcept override
	{
		calls.push_back(ObservedCall::common_effects_advanced);
	}

	void common_effect_spawned(
		const th03::CommonEffectSpawned &spawn
	) noexcept override
	{
		common_effects.push_back(spawn);
		calls.push_back(ObservedCall::common_effect_spawned);
	}

	void bomb_frame_step(const th03::BombFrameStep &step) noexcept override
	{
		bomb_frame_steps.push_back(step);
		background_bomb_calls.push_back(
			BackgroundObservedCall::bomb_frame_step
		);
		calls.push_back(ObservedCall::bomb_frame_step);
		boss_calls.push_back(BossObservedCall::bomb_frame_step);
	}

	void boss_activated(const th03::BossActivated &activated) noexcept override
	{
		boss_activations.push_back(activated);
		boss_calls.push_back(BossObservedCall::activated);
	}

	void boss_animation_step(
		const th03::BossAnimationStep &step
	) noexcept override
	{
		boss_animation_steps.push_back(step);
		boss_calls.push_back(BossObservedCall::animation_step);
	}

	void boss_palette_baseline_restored(
		const th03::BossPaletteBaselineRestored &restored
	) noexcept override
	{
		boss_palette_restores.push_back(restored);
		boss_calls.push_back(BossObservedCall::palette_baseline_restored);
	}

	void bomb_compatibility_read(
		const th03::BombCompatibilityRead &read
	) noexcept override
	{
		bomb_compatibility_reads.push_back(read);
		calls.push_back(ObservedCall::bomb_compatibility_read);
	}

	void bomb_finished(const th03::BombFinished &finished) noexcept override
	{
		bomb_finishes.push_back(finished);
		calls.push_back(ObservedCall::bomb_finished);
	}

	void bomb_common_effect_step(
		const th03::BombCommonEffectStep &step
	) noexcept override
	{
		bomb_common_steps.push_back(step);
		calls.push_back(ObservedCall::bomb_common_effect_step);
	}

	void warning_interstitial_started(
		const th03::WarningInterstitialStarted &started
	) noexcept override
	{
		warning_interstitial_starts.push_back(started);
		calls.push_back(ObservedCall::warning_interstitial_started);
	}

	void warning_frame_lifecycle(
		const th03::WarningFrameLifecycle &lifecycle
	) noexcept override
	{
		warning_lifecycles.push_back(lifecycle);
		warning_gauge_calls.push_back(
			WarningGaugeObservedCall::warning_frame_lifecycle
		);
		boss_calls.push_back(BossObservedCall::warning_frame_lifecycle);
	}

	void gauge_attack_finished(
		const th03::GaugeAttackFinished &finished
	) noexcept override
	{
		gauge_attack_finishes.push_back(finished);
		warning_gauge_calls.push_back(
			WarningGaugeObservedCall::gauge_attack_finished
		);
	}

	void warning_portrait_transition(
		const th03::WarningPortraitTransition &transition
	) noexcept override
	{
		warning_portrait_transitions.push_back(transition);
		warning_gauge_calls.push_back(
			WarningGaugeObservedCall::warning_portrait_transition
		);
	}

	void main_gauge_state(
		const th03::MainGaugeState &state
	) noexcept override
	{
		main_gauge_states.push_back(state);
		warning_gauge_calls.push_back(
			WarningGaugeObservedCall::main_gauge_state
		);
	}

	void warning_interstitial_sample(
		const th03::WarningInterstitialSample &sample
	) noexcept override
	{
		warning_interstitial_samples.push_back(sample);
		background_bomb_calls.push_back(
			BackgroundObservedCall::warning_sample
		);
		calls.push_back(ObservedCall::warning_interstitial_sample);
		warning_gauge_calls.push_back(
			WarningGaugeObservedCall::warning_interstitial_sample
		);
	}

	void main_background_frame(
		const th03::MainBackgroundFrame &
	) noexcept override
	{
		background_bomb_calls.push_back(BackgroundObservedCall::frame);
		warning_gauge_calls.push_back(
			WarningGaugeObservedCall::background_frame
		);
		boss_calls.push_back(BossObservedCall::background_frame);
	}

	void main_background_random_read(
		const th03::MainBackgroundRandomRead &
	) noexcept override
	{
		background_bomb_calls.push_back(BackgroundObservedCall::random_read);
		boss_calls.push_back(BossObservedCall::background_random_read);
	}

	void main_frame_footer_reached(
		const th03::MainFrameFooterReached &footer
	) noexcept override
	{
		main_frame_footers.push_back(footer);
		calls.push_back(ObservedCall::main_frame_footer_reached);
		warning_gauge_calls.push_back(
			WarningGaugeObservedCall::main_frame_footer_reached
		);
	}

	void observe(const th03::ShotsEnemiesView &view) noexcept override
	{
		phases.push_back(ObservedPhase::shots_enemies);
		calls.push_back(ObservedCall::shots_enemies);
		warning_gauge_calls.push_back(
			WarningGaugeObservedCall::shots_enemies
		);
		boss_calls.push_back(BossObservedCall::shots_enemies);
		shots_enemies_context = view.context();
		for(std::size_t slot = 0U; slot < view.shot_count; ++slot) {
			const auto fact = view.shot(
				static_cast<th03::ShotPairSlot>(slot)
			);
			if(fact) {
				shots[slot] = *fact;
			}
		}
		for(std::size_t slot = 0U; slot < view.enemy_count; ++slot) {
			const auto fact = view.enemy(static_cast<th03::EnemySlot>(slot));
			if(fact) {
				enemies[slot] = *fact;
			}
		}
	}

	void observe(const th03::MainBossView &view) noexcept override
	{
		boss_views.push_back(view.boss());
		boss_context = view.context();
		boss_round_frame_is_odd = view.round_frame_is_odd();
		boss_calls.push_back(BossObservedCall::main_boss);
		for(std::size_t point = 0U;
			point < view.reimu_orbit_point_count;
			++point) {
			reimu_boss_orbit_points[point] = view.reimu_orbit_point(
				static_cast<th03::ReimuBossOrbitPoint>(point)
			);
		}
	}

	void observe(const th03::FireballsView &view) noexcept override
	{
		phases.push_back(ObservedPhase::fireballs);
		calls.push_back(ObservedCall::fireballs);
		warning_gauge_calls.push_back(
			WarningGaugeObservedCall::fireballs
		);
		fireballs_context = view.context();
		for(std::size_t slot = 0U; slot < view.fireball_count; ++slot) {
			const auto fact = view.fireball(
				static_cast<th03::FireballSlot>(slot)
			);
			if(fact) {
				fireballs[slot] = *fact;
			}
		}
	}

	void observe(const th03::MainExtraAttackView &view) noexcept override
	{
		const auto player = static_cast<std::size_t>(view.player());
		const auto phase = (view.player() == th03::PlayerId::p1)
			? ObservedPhase::extra_p1
			: ObservedPhase::extra_p2;
		const auto call = (view.player() == th03::PlayerId::p1)
			? ObservedCall::extra_p1
			: ObservedCall::extra_p2;
		phases.push_back(phase);
		calls.push_back(call);
		extra_contexts[player] = view.context();
		extra_first_attacks[player] = view.attack(
			static_cast<th03::ExtraAttackSlot>(0U)
		);
		capture_extra_frames();
	}

	void observe(const th03::MainChargeShotView &view) noexcept override
	{
		const auto player = static_cast<std::size_t>(view.player());
		const auto phase = (view.player() == th03::PlayerId::p1)
			? ObservedPhase::charge_p1
			: ObservedPhase::charge_p2;
		const auto call = (view.player() == th03::PlayerId::p1)
			? ObservedCall::charge_p1
			: ObservedCall::charge_p2;
		phases.push_back(phase);
		calls.push_back(call);
		charge_contexts[player] = view.context();
		capture_extra_frames();
	}

	void observe(const th03::PlayersBulletsView &view) noexcept override
	{
		phases.push_back(ObservedPhase::players_bullets);
		calls.push_back(ObservedCall::players_bullets);
		warning_gauge_calls.push_back(
			WarningGaugeObservedCall::players_bullets
		);
		players_bullets_context = view.context();
		for(std::size_t player = 0U; player < view.player_count; ++player) {
			const auto fact = view.player(static_cast<th03::PlayerId>(player));
			if(fact) {
				players[player] = *fact;
			}
		}
		for(std::size_t slot = 0U; slot < view.bullet_count; ++slot) {
			const auto fact = view.bullet(static_cast<th03::BulletSlot>(slot));
			if(fact) {
				bullets[slot] = *fact;
			}
			for(std::size_t point = 0U;
				point < view.trail_point_count;
				++point) {
				const auto trail = view.trail_point(
					static_cast<th03::BulletSlot>(slot),
					static_cast<th03::TrailPoint>(point)
				);
				if(trail) {
					trail_points[slot][point] = *trail;
					trail_points_present[slot][point] = true;
				}
			}
		}
	}

	std::vector<ObservedPhase> phases;
	std::vector<ObservedCall> calls;
	std::vector<th03::ShotPairSpawned> spawns;
	std::vector<th03::Bullet16Spawned> bullet_spawns;
	std::vector<th03::PlayerPoseCommitted> poses;
	std::vector<th03::PlayerDefeatStep> defeat_steps;
	std::vector<th03::HitCircleSpawned> hit_circles;
	std::vector<th03::CommonEffectSpawned> common_effects;
	std::vector<th03::BombCompatibilityRead> bomb_compatibility_reads;
	std::vector<th03::BombFinished> bomb_finishes;
	std::vector<th03::BombFrameStep> bomb_frame_steps;
	std::vector<th03::BombCommonEffectStep> bomb_common_steps;
	std::vector<th03::WarningInterstitialStarted> warning_interstitial_starts;
	std::vector<th03::WarningInterstitialSample> warning_interstitial_samples;
	std::vector<th03::WarningFrameLifecycle> warning_lifecycles;
	std::vector<th03::GaugeAttackFinished> gauge_attack_finishes;
	std::vector<th03::WarningPortraitTransition>
		warning_portrait_transitions;
	std::vector<th03::MainGaugeState> main_gauge_states;
	std::vector<WarningGaugeObservedCall> warning_gauge_calls;
	std::vector<th03::MainFrameFooterReached> main_frame_footers;
	std::vector<th03::BossActivated> boss_activations;
	std::vector<th03::BossAnimationStep> boss_animation_steps;
	std::vector<th03::BossPaletteBaselineRestored> boss_palette_restores;
	std::vector<std::optional<th03::MainBossFact>> boss_views;
	std::vector<BossObservedCall> boss_calls;
	std::vector<BackgroundObservedCall> background_bomb_calls;
	th03::MainPhaseContext boss_context{};
	bool boss_round_frame_is_odd = false;
	std::array<
		std::optional<th03::RawPoint>,
		th03::MainBossView::reimu_orbit_point_count
	> reimu_boss_orbit_points{};
	th03::MainPhaseContext shots_enemies_context{};
	th03::MainPhaseContext fireballs_context{};
	std::array<th03::MainPhaseContext, 2> extra_contexts{};
	std::array<th03::MainPhaseContext, 2> charge_contexts{};
	std::array<std::optional<th03::MainExtraAttackFact>, 2>
		extra_first_attacks{};
	th03::MainPhaseContext players_bullets_context{};
	std::array<
		th03::ShotPairFact,
		th03::ShotsEnemiesView::shot_count
	> shots{};
	std::array<
		th03::EnemyFact,
		th03::ShotsEnemiesView::enemy_count
	> enemies{};
	std::array<
		th03::FireballFact,
		th03::FireballsView::fireball_count
	> fireballs{};
	std::array<
		th03::PlayerFact,
		th03::PlayersBulletsView::player_count
	> players{};
	std::array<
		th03::BulletFact,
		th03::PlayersBulletsView::bullet_count
	> bullets{};
	std::array<
		std::array<th03::RawPoint, th03::PlayersBulletsView::trail_point_count>,
		th03::PlayersBulletsView::bullet_count
	> trail_points{};
	std::array<
		std::array<bool, th03::PlayersBulletsView::trail_point_count>,
		th03::PlayersBulletsView::bullet_count
	> trail_points_present{};
	std::vector<std::array<std::uint8_t, 2>> extra_frames_at_late_phases;

private:
	void capture_extra_frames()
	{
		if(inspected_state_ == nullptr) {
			return;
		}
		extra_frames_at_late_phases.push_back({
			inspected_state_->extra_attacks[0U][0U].frame,
			inspected_state_->extra_attacks[1U][0U].frame,
		});
	}

	const th03::detail::DeterministicState *inspected_state_;
};

enum class MainHudObservedCall : std::uint8_t {
	hud_start_step,
	hud_start_read,
	player_defeat_step,
	hit_circle,
	player_value,
	story_lives,
	players_bullets,
	result,
	gauge,
	score,
	warning_portrait,
	hud_start,
	combo,
	result_transition,
	footer,
};

struct MainHudMutationSnapshot {
	th03::MainHudPlayerValueChanged fact{};
	std::uint8_t invincibility_time = 0;
	std::uint8_t skill = 0;
	std::uint8_t defeat_flag = 0;
	std::int8_t winner = 0;
	std::uint16_t round_or_result_frame = 0;
	bool knockback_active = false;
	std::uint8_t lose_anim_time = 0;
};

class MainHudRecordingObserver final : public th03::GameplayObserver {
public:
	explicit MainHudRecordingObserver(
		const th03::detail::DeterministicState *inspected_state = nullptr
	) : inspected_state_(inspected_state)
	{}

	void hud_start_frame_step(
		const th03::HudStartFrameStep &step
	) noexcept override
	{
		hud_start_steps.push_back(step);
		calls.push_back(MainHudObservedCall::hud_start_step);
	}

	void hud_start_compatibility_read(
		const th03::HudStartCompatibilityRead &read
	) noexcept override
	{
		hud_start_reads.push_back(read);
		calls.push_back(MainHudObservedCall::hud_start_read);
	}

	void player_defeat_step(
		const th03::PlayerDefeatStep &
	) noexcept override
	{
		calls.push_back(MainHudObservedCall::player_defeat_step);
	}

	void hit_circle_spawned(
		const th03::HitCircleSpawned &
	) noexcept override
	{
		calls.push_back(MainHudObservedCall::hit_circle);
	}

	void main_hud_player_value_changed(
		const th03::MainHudPlayerValueChanged &fact
	) noexcept override
	{
		player_values.push_back(fact);
		MainHudMutationSnapshot snapshot{.fact = fact};
		if(inspected_state_ != nullptr) {
			const auto player = static_cast<std::size_t>(fact.player);
			snapshot.invincibility_time =
				inspected_state_->players[player].invincibility_time;
			snapshot.skill = inspected_state_->skill;
			snapshot.defeat_flag = static_cast<std::uint8_t>(
				inspected_state_->defeat_flag
			);
			snapshot.winner = inspected_state_->winner;
			snapshot.round_or_result_frame =
				inspected_state_->round_or_result_frame;
			snapshot.knockback_active =
				inspected_state_->players[player].knockback_active;
			snapshot.lose_anim_time =
				inspected_state_->players[player].lose_anim_time;
		}
		player_value_snapshots.push_back(snapshot);
		calls.push_back(MainHudObservedCall::player_value);
	}

	void main_hud_story_lives_changed(
		const th03::MainHudStoryLivesChanged &fact
	) noexcept override
	{
		story_lives.push_back(fact);
		if(inspected_state_ != nullptr) {
			extends_at_lives.push_back(inspected_state_->extends_gained);
		}
		calls.push_back(MainHudObservedCall::story_lives);
	}

	void observe(const th03::PlayersBulletsView &) noexcept override
	{
		calls.push_back(MainHudObservedCall::players_bullets);
	}

	void main_result_state(const th03::MainResultState &state) noexcept override
	{
		results.push_back(state);
		if(inspected_state_ != nullptr) {
			result_bonus_remaining_at_view.push_back(
				inspected_state_->result_bonus_remaining
			);
			score_at_result_view.push_back(inspected_state_->score);
		}
		calls.push_back(MainHudObservedCall::result);
	}

	void main_gauge_state(const th03::MainGaugeState &) noexcept override
	{
		calls.push_back(MainHudObservedCall::gauge);
	}

	void main_score_state(const th03::MainScoreState &state) noexcept override
	{
		scores.push_back(state);
		if(inspected_state_ != nullptr) {
			story_lives_at_score.push_back(inspected_state_->story_lives);
		}
		calls.push_back(MainHudObservedCall::score);
	}

	void warning_portrait_transition(
		const th03::WarningPortraitTransition &
	) noexcept override
	{
		calls.push_back(MainHudObservedCall::warning_portrait);
	}

	void main_hud_start_state(
		const th03::MainHudStartState &state
	) noexcept override
	{
		hud_start_states.push_back(state);
		calls.push_back(MainHudObservedCall::hud_start);
	}

	void main_combo_state(const th03::MainComboState &state) noexcept override
	{
		combos.push_back(state);
		calls.push_back(MainHudObservedCall::combo);
	}

	void main_result_transition_state(
		const th03::MainResultTransitionState &state
	) noexcept override
	{
		result_transitions.push_back(state);
		calls.push_back(MainHudObservedCall::result_transition);
	}

	void main_frame_footer_reached(
		const th03::MainFrameFooterReached &
	) noexcept override
	{
		calls.push_back(MainHudObservedCall::footer);
	}

	std::vector<MainHudObservedCall> calls;
	std::vector<th03::HudStartFrameStep> hud_start_steps;
	std::vector<th03::HudStartCompatibilityRead> hud_start_reads;
	std::vector<th03::MainHudPlayerValueChanged> player_values;
	std::vector<MainHudMutationSnapshot> player_value_snapshots;
	std::vector<th03::MainHudStoryLivesChanged> story_lives;
	std::vector<std::uint8_t> extends_at_lives;
	std::vector<th03::MainResultState> results;
	std::vector<std::uint32_t> result_bonus_remaining_at_view;
	std::vector<decltype(th03::detail::DeterministicState::score)>
		score_at_result_view;
	std::vector<th03::MainScoreState> scores;
	std::vector<std::uint8_t> story_lives_at_score;
	std::vector<th03::MainHudStartState> hud_start_states;
	std::vector<th03::MainComboState> combos;
	std::vector<th03::MainResultTransitionState> result_transitions;

private:
	const th03::detail::DeterministicState *inspected_state_;
};

th03::detail::DeterministicState make_live_state()
{
	th03::detail::DeterministicState state{};
	for(std::size_t player_id = 0U;
		player_id < th03::detail::player_count;
		++player_id) {
		auto &player = state.players[player_id];
		player.center_x = th03::exact::Subpixel::from_integer_wrapping(144);
		player.center_y = th03::exact::Subpixel::from_integer_wrapping(336);
		player.halfhearts = 10U;
		player.shot_active = 1U;
		player.paletted_character = static_cast<std::int8_t>(
			1U + (player_id * 2U)
		);
	}
	return state;
}

void seed_extra_charge_state(th03::detail::DeterministicState &state)
{
	state.reimu_chargeshot_primary = 9U;
	for(std::size_t player = 0U; player < th03::detail::player_count; ++player) {
		state.players[player].reimu_chargeshot_active = true;
		state.players[player].reimu_chargeshot_age = static_cast<std::uint8_t>(
			20U + player
		);
		for(std::size_t slot = 0U;
			slot < state.extra_attacks[player].size(); ++slot) {
			auto &attack = state.extra_attacks[player][slot];
			attack.state = static_cast<std::uint8_t>(1U + (slot % 3U));
			attack.frame = static_cast<std::uint8_t>(30U + slot);
			attack.center_x = static_cast<std::int16_t>(100 + slot + player);
			attack.center_y = static_cast<std::int16_t>(
				-200 - static_cast<int>(slot) - static_cast<int>(player)
			);
			attack.velocity_x = static_cast<std::int16_t>(300 + slot);
			attack.velocity_y = static_cast<std::int16_t>(
				-400 - static_cast<int>(slot)
			);
			attack.target_x_for_target_player = static_cast<std::int16_t>(
				500 + slot
			);
			attack.target_x_for_origin_player = static_cast<std::int16_t>(
				-600 - static_cast<int>(slot)
			);
			attack.player_id = static_cast<std::uint8_t>(0xA0U + player);
			attack.angle = static_cast<std::uint8_t>(40U + slot);
			attack.speed = static_cast<std::uint8_t>(50U + slot);
			attack.behavior_variant = static_cast<std::uint8_t>(60U + slot);
			attack.mima_timer = static_cast<std::uint16_t>(0x1200U + slot);
			attack.yumemi_extent = static_cast<std::uint16_t>(70U + slot);
			attack.yumemi_duration = static_cast<std::uint16_t>(80U + slot);
			attack.kotohime_target_y = static_cast<std::int16_t>(
				-900 - static_cast<int>(slot)
			);
			attack.kotohime_velocity_y = static_cast<std::uint16_t>(90U + slot);
			attack.kotohime_milestone = static_cast<std::uint8_t>(100U + slot);
			for(std::size_t point = 0U;
				point < attack.ellen_trail_x.size(); ++point) {
				attack.ellen_trail_x[point] = static_cast<std::int16_t>(
					1000 + slot + point
				);
				attack.ellen_trail_y[point] = static_cast<std::int16_t>(
					-1100 - static_cast<int>(slot) - static_cast<int>(point)
				);
			}
			attack.ellen_radius = static_cast<std::int16_t>(1200 + slot);
			attack.ellen_angle_delta = static_cast<std::int8_t>(
				-10 - static_cast<int>(player)
			);
		}
		for(std::size_t group = 0U;
			group < state.reimu_charge[player].size(); ++group) {
			auto &charge = state.reimu_charge[player][group];
			charge.state = static_cast<std::uint8_t>(1U + (group % 3U));
			charge.frame = static_cast<std::uint8_t>(10U + group);
			for(std::size_t point = 0U; point < charge.x.size(); ++point) {
				charge.x[point] = static_cast<std::int16_t>(1300 + group + point);
				charge.y[point] = static_cast<std::int16_t>(
					-1400 - static_cast<int>(group) - static_cast<int>(point)
				);
			}
			charge.angle = static_cast<std::uint8_t>(20U + group);
			charge.speed = static_cast<std::uint8_t>(30U + group);
		}
		state.mima_charge_active[player] = static_cast<std::uint8_t>(2U + player);
		state.mima_charge_frames[player] = static_cast<std::uint8_t>(3U + player);
		for(std::size_t node = 0U;
			node < state.mima_charge[player].size(); ++node) {
			auto &charge = state.mima_charge[player][node];
			charge.active = static_cast<std::uint8_t>(1U + (node % 2U));
			charge.visual_frame = static_cast<std::uint8_t>(4U + node);
			charge.x = static_cast<std::int16_t>(1500 + node);
			charge.y = static_cast<std::int16_t>(
				-1600 - static_cast<int>(node)
			);
			charge.velocity_x = static_cast<std::int16_t>(1700 + node);
			charge.velocity_y = static_cast<std::int16_t>(
				-1800 - static_cast<int>(node)
			);
		}
		auto &marisa = state.marisa_charge[player];
		marisa.active = static_cast<std::uint8_t>(5U + player);
		marisa.age = static_cast<std::uint8_t>(6U + player);
		for(std::size_t node = 0U; node < marisa.x.size(); ++node) {
			marisa.x[node] = static_cast<std::int16_t>(1900 + node);
			marisa.y[node] = static_cast<std::int16_t>(
				-2000 - static_cast<int>(node)
			);
		}
		state.kana_charge_state[player] = static_cast<std::uint8_t>(7U + player);
		state.kana_charge_frames[player] = static_cast<std::uint8_t>(8U + player);
		for(std::size_t group = 0U;
			group < state.kana_charge[player].size(); ++group) {
			auto &charge = state.kana_charge[player][group];
			for(std::size_t point = 0U; point < charge.x.size(); ++point) {
				charge.x[point] = static_cast<std::int16_t>(2100 + group + point);
				charge.y[point] = static_cast<std::int16_t>(
					-2200 - static_cast<int>(group) - static_cast<int>(point)
				);
			}
			charge.angle = static_cast<std::uint8_t>(9U + group);
			charge.speed = static_cast<std::uint8_t>(10U + group);
		}
		state.rikako_charge_state[player] = static_cast<std::uint8_t>(11U + player);
		state.rikako_charge_frames[player] = static_cast<std::uint8_t>(12U + player);
		state.rikako_charge_radius[player] = static_cast<std::uint16_t>(
			2300U + player
		);
		state.rikako_charge_origin_x[player] = static_cast<std::int16_t>(
			2400 + player
		);
		state.rikako_charge_origin_y[player] = static_cast<std::int16_t>(
			-2500 - static_cast<int>(player)
		);
		for(std::size_t node = 0U;
			node < state.rikako_charge[player].size(); ++node) {
			auto &charge = state.rikako_charge[player][node];
			charge.x = static_cast<std::int16_t>(2600 + node);
			charge.y = static_cast<std::int16_t>(
				-2700 - static_cast<int>(node)
			);
			charge.angle = static_cast<std::uint8_t>(13U + node);
		}
		for(std::size_t node = 0U;
			node < state.chiyuri_charge[player].size(); ++node) {
			auto &charge = state.chiyuri_charge[player][node];
			charge.state = static_cast<std::uint8_t>(1U + (node % 2U));
			charge.frame = static_cast<std::uint8_t>(14U + node);
			charge.x = static_cast<std::int16_t>(2800 + node);
			charge.y = static_cast<std::int16_t>(
				-2900 - static_cast<int>(node)
			);
		}
		for(std::size_t node = 0U;
			node < state.ellen_charge[player].size(); ++node) {
			auto &charge = state.ellen_charge[player][node];
			charge.state = static_cast<std::uint8_t>(1U + (node % 2U));
			charge.phase = static_cast<std::uint8_t>(15U + node);
			charge.angle = static_cast<std::uint8_t>(16U + node);
			charge.speed = static_cast<std::uint8_t>(17U + node);
			charge.x = static_cast<std::int16_t>(3000 + node);
			charge.y = static_cast<std::int16_t>(
				-3100 - static_cast<int>(node)
			);
			charge.velocity_x = static_cast<std::int16_t>(3200 + node);
			charge.velocity_y = static_cast<std::int16_t>(
				-3300 - static_cast<int>(node)
			);
		}
		auto &kotohime = state.kotohime_charge[player];
		kotohime.active = static_cast<std::uint8_t>(18U + player);
		kotohime.x = static_cast<std::int16_t>(3400 + player);
		kotohime.y = static_cast<std::int16_t>(
			-3500 - static_cast<int>(player)
		);
		kotohime.velocity_y = static_cast<std::int16_t>(3600 + player);
		auto &yumemi = state.yumemi_charge[player];
		yumemi.state = static_cast<std::uint8_t>(19U + player);
		yumemi.frame = static_cast<std::uint8_t>(20U + player);
		yumemi.x = static_cast<std::int16_t>(3700 + player);
		yumemi.y = static_cast<std::int16_t>(
			-3800 - static_cast<int>(player)
		);
	}
	state.extra_attacks[0U][0U].frame = 0xFFU;
}

template <typename Callback>
void with_live_state(Callback &&callback)
{
	auto state = make_live_state();
	callback(state);
}

th03::GameInit base_game_init();

bool shotpools_equal(
	const th03::detail::DeterministicState &left,
	const th03::detail::DeterministicState &right
)
{
	for(std::size_t slot = 0U; slot < left.shotpairs.size(); ++slot) {
		const auto &a = left.shotpairs[slot];
		const auto &b = right.shotpairs[slot];
		if(std::tie(
			a.alive, a.topleft_x, a.topleft_y, a.velocity_y, a.player_id
		) != std::tie(
			b.alive, b.topleft_x, b.topleft_y, b.velocity_y, b.player_id
		)) {
			return false;
		}
	}
	return true;
}

bool enemy_pools_equal(
	const th03::detail::DeterministicState &left,
	const th03::detail::DeterministicState &right
)
{
	for(std::size_t slot = 0U; slot < left.enemies.size(); ++slot) {
		const auto &a = left.enemies[slot];
		const auto &b = right.enemies[slot];
		if(std::tie(
			a.flag, a.frame, a.center_x, a.center_y, a.hp, a.player_id,
			a.size_pixels, a.script_ip, a.script_op_frame, a.velocity_x,
			a.velocity_y, a.angle, a.angle_speed, a.formation_type,
			a.formation_index, a.speed, a.loop_index, a.position_type,
			a.chain_slot, a.explosion_max_enemy_hits_half
		) != std::tie(
			b.flag, b.frame, b.center_x, b.center_y, b.hp, b.player_id,
			b.size_pixels, b.script_ip, b.script_op_frame, b.velocity_x,
			b.velocity_y, b.angle, b.angle_speed, b.formation_type,
			b.formation_index, b.speed, b.loop_index, b.position_type,
			b.chain_slot, b.explosion_max_enemy_hits_half
		)) {
			return false;
		}
	}
	return true;
}

bool fireball_pools_equal(
	const th03::detail::DeterministicState &left,
	const th03::detail::DeterministicState &right
)
{
	for(std::size_t slot = 0U; slot < left.fireballs.size(); ++slot) {
		const auto &a = left.fireballs[slot];
		const auto &b = right.fireballs[slot];
		if(std::tie(
			a.flag, a.frame, a.center_x, a.center_y,
			a.variant_as_explosion_target, a.hp, a.player_id, a.size_pixels,
			a.velocity_x, a.velocity_y, a.target_x_for_target_player,
			a.target_x_for_origin_player, a.fall_velocity_y, a.chain_slot,
			a.generation
		) != std::tie(
			b.flag, b.frame, b.center_x, b.center_y,
			b.variant_as_explosion_target, b.hp, b.player_id, b.size_pixels,
			b.velocity_x, b.velocity_y, b.target_x_for_target_player,
			b.target_x_for_origin_player, b.fall_velocity_y, b.chain_slot,
			b.generation
		)) {
			return false;
		}
	}
	return true;
}

bool extra_attack_pools_equal(
	const th03::detail::DeterministicState &left,
	const th03::detail::DeterministicState &right
)
{
	for(std::size_t player = 0U; player < left.extra_attacks.size(); ++player) {
		for(std::size_t slot = 0U;
			slot < left.extra_attacks[player].size(); ++slot) {
			const auto &a = left.extra_attacks[player][slot];
			const auto &b = right.extra_attacks[player][slot];
			if(std::tie(
				a.state, a.frame, a.center_x, a.center_y, a.velocity_x,
				a.velocity_y, a.target_x_for_target_player,
				a.target_x_for_origin_player, a.player_id, a.angle, a.speed,
				a.behavior_variant, a.mima_timer, a.yumemi_extent,
				a.yumemi_duration, a.kotohime_target_y,
				a.kotohime_velocity_y, a.kotohime_milestone,
				a.ellen_trail_x, a.ellen_trail_y, a.ellen_radius,
				a.ellen_angle_delta
			) != std::tie(
				b.state, b.frame, b.center_x, b.center_y, b.velocity_x,
				b.velocity_y, b.target_x_for_target_player,
				b.target_x_for_origin_player, b.player_id, b.angle, b.speed,
				b.behavior_variant, b.mima_timer, b.yumemi_extent,
				b.yumemi_duration, b.kotohime_target_y,
				b.kotohime_velocity_y, b.kotohime_milestone,
				b.ellen_trail_x, b.ellen_trail_y, b.ellen_radius,
				b.ellen_angle_delta
			)) {
				return false;
			}
		}
	}
	return true;
}

bool charge_shot_pools_equal(
	const th03::detail::DeterministicState &left,
	const th03::detail::DeterministicState &right
)
{
	for(std::size_t player = 0U; player < th03::detail::player_count; ++player) {
		if(std::tie(
			left.players[player].reimu_chargeshot_active,
			left.players[player].reimu_chargeshot_age
		) != std::tie(
			right.players[player].reimu_chargeshot_active,
			right.players[player].reimu_chargeshot_age
		)) {
			return false;
		}
		for(std::size_t group = 0U;
			group < left.reimu_charge[player].size(); ++group) {
			const auto &a = left.reimu_charge[player][group];
			const auto &b = right.reimu_charge[player][group];
			if(std::tie(a.state, a.frame, a.x, a.y, a.angle, a.speed) !=
				std::tie(b.state, b.frame, b.x, b.y, b.angle, b.speed)) {
				return false;
			}
		}
		for(std::size_t node = 0U;
			node < left.mima_charge[player].size(); ++node) {
			const auto &a = left.mima_charge[player][node];
			const auto &b = right.mima_charge[player][node];
			if(std::tie(
				a.active, a.visual_frame, a.x, a.y, a.velocity_x, a.velocity_y
			) != std::tie(
				b.active, b.visual_frame, b.x, b.y, b.velocity_x, b.velocity_y
			)) {
				return false;
			}
		}
		const auto &marisa_a = left.marisa_charge[player];
		const auto &marisa_b = right.marisa_charge[player];
		if(std::tie(
			marisa_a.active, marisa_a.age, marisa_a.x, marisa_a.y
		) != std::tie(
			marisa_b.active, marisa_b.age, marisa_b.x, marisa_b.y
		)) {
			return false;
		}
		for(std::size_t group = 0U;
			group < left.kana_charge[player].size(); ++group) {
			const auto &a = left.kana_charge[player][group];
			const auto &b = right.kana_charge[player][group];
			if(std::tie(a.x, a.y, a.angle, a.speed) !=
				std::tie(b.x, b.y, b.angle, b.speed)) {
				return false;
			}
		}
		for(std::size_t node = 0U;
			node < left.rikako_charge[player].size(); ++node) {
			const auto &a = left.rikako_charge[player][node];
			const auto &b = right.rikako_charge[player][node];
			if(std::tie(a.x, a.y, a.angle) != std::tie(b.x, b.y, b.angle)) {
				return false;
			}
		}
		for(std::size_t node = 0U;
			node < left.chiyuri_charge[player].size(); ++node) {
			const auto &a = left.chiyuri_charge[player][node];
			const auto &b = right.chiyuri_charge[player][node];
			if(std::tie(a.state, a.frame, a.x, a.y) !=
				std::tie(b.state, b.frame, b.x, b.y)) {
				return false;
			}
		}
		for(std::size_t node = 0U;
			node < left.ellen_charge[player].size(); ++node) {
			const auto &a = left.ellen_charge[player][node];
			const auto &b = right.ellen_charge[player][node];
			if(std::tie(
				a.state, a.phase, a.angle, a.speed, a.x, a.y,
				a.velocity_x, a.velocity_y
			) != std::tie(
				b.state, b.phase, b.angle, b.speed, b.x, b.y,
				b.velocity_x, b.velocity_y
			)) {
				return false;
			}
		}
		const auto &kotohime_a = left.kotohime_charge[player];
		const auto &kotohime_b = right.kotohime_charge[player];
		const auto &yumemi_a = left.yumemi_charge[player];
		const auto &yumemi_b = right.yumemi_charge[player];
		if(std::tie(
			left.mima_charge_active[player], left.mima_charge_frames[player],
			left.kana_charge_state[player], left.kana_charge_frames[player],
			left.rikako_charge_state[player], left.rikako_charge_frames[player],
			left.rikako_charge_radius[player], left.rikako_charge_origin_x[player],
			left.rikako_charge_origin_y[player], kotohime_a.active,
			kotohime_a.x, kotohime_a.y, kotohime_a.velocity_y, yumemi_a.state,
			yumemi_a.frame, yumemi_a.x, yumemi_a.y
		) != std::tie(
			right.mima_charge_active[player], right.mima_charge_frames[player],
			right.kana_charge_state[player], right.kana_charge_frames[player],
			right.rikako_charge_state[player], right.rikako_charge_frames[player],
			right.rikako_charge_radius[player], right.rikako_charge_origin_x[player],
			right.rikako_charge_origin_y[player], kotohime_b.active,
			kotohime_b.x, kotohime_b.y, kotohime_b.velocity_y, yumemi_b.state,
			yumemi_b.frame, yumemi_b.x, yumemi_b.y
		)) {
			return false;
		}
	}
	return left.reimu_chargeshot_primary == right.reimu_chargeshot_primary;
}

bool player_pools_equal(
	const th03::detail::DeterministicState &left,
	const th03::detail::DeterministicState &right
)
{
	for(std::size_t player = 0U; player < left.players.size(); ++player) {
		const auto &a = left.players[player];
		const auto &b = right.players[player];
		if(std::tie(
			a.center_x, a.center_y, a.is_hit, a.halfhearts,
			a.invincibility_time, a.shot_mode, a.knockback_time,
			a.move_lock_time, a.knockback_angle, a.knockback_length,
			a.knockback_active, a.gauge_charged, a.gauge_avail, a.bombs,
			a.hyper_active, a.hyper_character_callback,
			a.hyper_effect_active_this_frame, a.lose_anim_time,
			a.rounds_won, a.cpu_frame, a.hit_damage_next, a.shot_active,
			a.cpu_dodge_strategy, a.human_movement_last,
			a.spell_ready_frames, a.combo_bonus_max, a.combo_hits_max,
			a.gauge_attacks_fired, a.boss_attacks_fired,
			a.boss_attacks_reversed, a.boss_panics_fired, a.is_cpu,
			a.paletted_character, a.gauge_charge_speed, a.cpu_safety_frames,
			a.speed_aligned_x, a.speed_aligned_y, a.speed_diagonal_x,
			a.speed_diagonal_y, a.reimu_chargeshot_active,
			a.reimu_chargeshot_age, a.cpu_charge_ring, a.cpu_charge_cursor
		) != std::tie(
			b.center_x, b.center_y, b.is_hit, b.halfhearts,
			b.invincibility_time, b.shot_mode, b.knockback_time,
			b.move_lock_time, b.knockback_angle, b.knockback_length,
			b.knockback_active, b.gauge_charged, b.gauge_avail, b.bombs,
			b.hyper_active, b.hyper_character_callback,
			b.hyper_effect_active_this_frame, b.lose_anim_time,
			b.rounds_won, b.cpu_frame, b.hit_damage_next, b.shot_active,
			b.cpu_dodge_strategy, b.human_movement_last,
			b.spell_ready_frames, b.combo_bonus_max, b.combo_hits_max,
			b.gauge_attacks_fired, b.boss_attacks_fired,
			b.boss_attacks_reversed, b.boss_panics_fired, b.is_cpu,
			b.paletted_character, b.gauge_charge_speed, b.cpu_safety_frames,
			b.speed_aligned_x, b.speed_aligned_y, b.speed_diagonal_x,
			b.speed_diagonal_y, b.reimu_chargeshot_active,
			b.reimu_chargeshot_age, b.cpu_charge_ring, b.cpu_charge_cursor
		)) {
			return false;
		}
	}
	return true;
}

bool bullet_pools_equal(
	const th03::detail::DeterministicState &left,
	const th03::detail::DeterministicState &right
)
{
	for(std::size_t slot = 0U; slot < left.bullets.size(); ++slot) {
		const auto &a = left.bullets[slot];
		const auto &b = right.bullets[slot];
		if(std::tie(
			a.flag, a.age, a.center_x, a.center_y,
			a.target_x_for_target_player, a.target_x_for_origin_player,
			a.velocity_x, a.velocity_y, a.speed_next, a.angle_next,
			a.player_id, a.group_next, a.accel_type, a.is_collidable,
			a.has_trail, a.trail_index
		) != std::tie(
			b.flag, b.age, b.center_x, b.center_y,
			b.target_x_for_target_player, b.target_x_for_origin_player,
			b.velocity_x, b.velocity_y, b.speed_next, b.angle_next,
			b.player_id, b.group_next, b.accel_type, b.is_collidable,
			b.has_trail, b.trail_index
		)) {
			return false;
		}
	}
	for(std::size_t slot = 0U; slot < left.bullet_trails.size(); ++slot) {
		if(
			left.bullet_trails[slot].center_x !=
				right.bullet_trails[slot].center_x ||
			left.bullet_trails[slot].center_y !=
				right.bullet_trails[slot].center_y
		) {
			return false;
		}
	}
	return left.bullet_trail_cursor == right.bullet_trail_cursor;
}

bool boss_initial_state_equal(
	const th03::detail::BossInitialState &left,
	const th03::detail::BossInitialState &right
)
{
	return std::tie(
		left.center_x, left.center_y, left.velocity_x, left.velocity_y,
		left.hp, left.state, left.motion_angle, left.pattern_index,
		left.render_state
	) == std::tie(
		right.center_x, right.center_y, right.velocity_x, right.velocity_y,
		right.hp, right.state, right.motion_angle, right.pattern_index,
		right.render_state
	);
}

bool boss_state_equal(
	const th03::detail::DeterministicState &left,
	const th03::detail::DeterministicState &right
)
{
	for(std::size_t player = 0U; player < left.boss_initial.size(); ++player) {
		if(!boss_initial_state_equal(
			left.boss_initial[player], right.boss_initial[player]
		)) {
			return false;
		}
	}
	if(
		!boss_initial_state_equal(left.boss, right.boss) ||
		std::tie(
			left.boss.hit,
			left.boss.pattern_previous,
			left.boss.pattern_count,
			left.boss.pattern_frame,
			left.boss.anchor_x,
			left.boss.anchor_y,
			left.boss.orbit_x,
			left.boss.orbit_y,
			left.boss.params,
			left.boss.render_frame,
			left.boss.direction,
			left.boss.angle_delta,
			left.boss.pattern_angle
		) != std::tie(
			right.boss.hit,
			right.boss.pattern_previous,
			right.boss.pattern_count,
			right.boss.pattern_frame,
			right.boss.anchor_x,
			right.boss.anchor_y,
			right.boss.orbit_x,
			right.boss.orbit_y,
			right.boss.params,
			right.boss.render_frame,
			right.boss.direction,
			right.boss.angle_delta,
			right.boss.pattern_angle
		)
	) {
		return false;
	}
	return std::tie(
		left.boss_panic_fired_in_combo,
		left.gba_flag_next,
		left.gba_flag_active,
		left.gba_gauge_level,
		left.gba_boss_level,
		left.gba_boss_launched_by,
		left.combo_points_for_boss_attack,
		left.mima_boss_spread_angle,
		left.mima_boss_pellet_angle,
		left.mima_boss_orbit_angle_delta,
		left.mima_boss_orbit_angle,
		left.marisa_boss_spread_angle,
		left.ellen_boss_spread_angle,
		left.ellen_boss_pellet_angle,
		left.chiyuri_boss_pattern_angle,
		left.chiyuri_boss_pattern_direction,
		left.yumemi_boss_speed_offset,
		left.yumemi_boss_angle_delta,
		left.yumemi_boss_angle,
		left.yumemi_boss_radius,
		left.yumemi_boss_muzzle_x,
		left.yumemi_boss_muzzle_y,
		left.yumemi_boss_target_x,
		left.yumemi_boss_target_y,
		left.boss_target_active,
		left.boss_target_x,
		left.boss_target_y
	) == std::tie(
		right.boss_panic_fired_in_combo,
		right.gba_flag_next,
		right.gba_flag_active,
		right.gba_gauge_level,
		right.gba_boss_level,
		right.gba_boss_launched_by,
		right.combo_points_for_boss_attack,
		right.mima_boss_spread_angle,
		right.mima_boss_pellet_angle,
		right.mima_boss_orbit_angle_delta,
		right.mima_boss_orbit_angle,
		right.marisa_boss_spread_angle,
		right.ellen_boss_spread_angle,
		right.ellen_boss_pellet_angle,
		right.chiyuri_boss_pattern_angle,
		right.chiyuri_boss_pattern_direction,
		right.yumemi_boss_speed_offset,
		right.yumemi_boss_angle_delta,
		right.yumemi_boss_angle,
		right.yumemi_boss_radius,
		right.yumemi_boss_muzzle_x,
		right.yumemi_boss_muzzle_y,
		right.yumemi_boss_target_x,
		right.yumemi_boss_target_y,
		right.boss_target_active,
		right.boss_target_x,
		right.boss_target_y
	);
}

bool rng_state_equal(
	const th03::detail::DeterministicState &left,
	const th03::detail::DeterministicState &right
)
{
	return (
		left.random_seed == right.random_seed &&
		left.randring == right.randring &&
		left.randring_cursor == right.randring_cursor &&
		left.formation_type_ring == right.formation_type_ring &&
		left.formation_position_ring == right.formation_position_ring &&
		left.formation_cursor == right.formation_cursor &&
		left.players[0U].cpu_charge_ring ==
			right.players[0U].cpu_charge_ring &&
		left.players[1U].cpu_charge_ring ==
			right.players[1U].cpu_charge_ring &&
		left.players[0U].cpu_charge_cursor ==
			right.players[0U].cpu_charge_cursor &&
		left.players[1U].cpu_charge_cursor ==
			right.players[1U].cpu_charge_cursor
	);
}

bool observed_gameplay_equal(
	const th03::detail::DeterministicState &left,
	const th03::detail::DeterministicState &right
)
{
	return (
		th03::detail::state_hash(left) == th03::detail::state_hash(right) &&
		shotpools_equal(left, right) &&
		enemy_pools_equal(left, right) &&
		fireball_pools_equal(left, right) &&
		extra_attack_pools_equal(left, right) &&
		charge_shot_pools_equal(left, right) &&
		player_pools_equal(left, right) &&
		bullet_pools_equal(left, right) &&
		boss_state_equal(left, right) &&
		rng_state_equal(left, right)
	);
}

bool canonical_equal(
	const th03::CanonicalState &left,
	const th03::CanonicalState &right
)
{
	return std::tie(
		left.route, left.game_mode, left.story_stage, left.round_id,
		left.winner, left.round_speed, left.global_frame, left.round_frame,
		left.round_or_result_frame, left.p1_score, left.p2_score,
		left.resident_rand, left.state_hash
	) == std::tie(
		right.route, right.game_mode, right.story_stage, right.round_id,
		right.winner, right.round_speed, right.global_frame, right.round_frame,
		right.round_or_result_frame, right.p1_score, right.p2_score,
		right.resident_rand, right.state_hash
	);
}

void test_indexed_views_and_classifications()
{
	auto state = make_live_state();
	state.round_id = 3U;
	state.round_or_result_frame = 0x1234U;
	state.playfield_fg_shift_x = {-6, 6};

	auto &shot = state.shotpairs[31U];
	shot.alive = true;
	shot.topleft_x = -123;
	shot.topleft_y = 456;
	shot.velocity_y = -192;
	shot.player_id = 1U;

	state.enemies[1U].flag = th03::detail::EnemyFlag::running_spawned;
	state.enemies[1U].frame = 7U;
	state.enemies[1U].center_x = -321;
	state.enemies[1U].center_y = 654;
	state.enemies[1U].player_id = 1U;
	state.enemies[1U].size_pixels = 32U;
	state.enemies[1U].hp = 2U;
	state.enemies[1U].explosion_max_enemy_hits_half = 2U;
	state.enemies[2U].flag = th03::detail::EnemyFlag::running_unspawned;
	state.enemies[3U].flag =
		th03::detail::EnemyFlag::explosion_ignoring_enemies;
	state.enemies[3U].frame = 10U;
	state.enemies[4U].flag = static_cast<th03::detail::EnemyFlag>(13U);
	state.enemies[4U].frame = 31U;
	state.enemies[5U].flag =
		th03::detail::EnemyFlag::explosion_ignoring_enemies;
	state.enemies[5U].frame = 32U;
	state.enemies[6U].flag =
		th03::detail::EnemyFlag::explosion_ignoring_enemies;
	state.enemies[6U].frame = 35U;

	state.fireballs[1U].flag = th03::detail::FireballFlag::fall;
	state.fireballs[1U].frame = 3U;
	state.fireballs[1U].center_x = -222;
	state.fireballs[1U].center_y = 777;
	state.fireballs[1U].player_id = 1U;
	state.fireballs[1U].variant_as_explosion_target = 3U;
	state.fireballs[2U].flag = th03::detail::FireballFlag::transfer;
	state.fireballs[3U].flag =
		th03::detail::FireballFlag::explosion_ignoring_enemies;
	state.fireballs[3U].frame = 12U;
	state.fireballs[4U].flag = static_cast<th03::detail::FireballFlag>(14U);
	state.fireballs[4U].frame = 35U;

	auto &player = state.players[1U];
	player.center_x = th03::exact::Subpixel::from_raw(-444);
	player.center_y = th03::exact::Subpixel::from_raw(555);
	player.paletted_character = 16;
	player.lose_anim_time = 0x2FU;
	player.invincibility_time = 73U;
	player.hyper_active = 4U;
	player.gauge_charged = 0xABCU;
	auto &trail_bullet = state.bullets[319U];
	trail_bullet.flag = th03::detail::BulletFlag::bullet16;
	trail_bullet.age = 17U;
	trail_bullet.center_x = -888;
	trail_bullet.center_y = 999;
	trail_bullet.player_id = 1U;
	trail_bullet.has_trail = true;
	trail_bullet.trail_index = 47U;
	for(std::size_t point = 0U; point < 6U; ++point) {
		state.bullet_trails[47U].center_x[point] =
			static_cast<std::int16_t>(100 + point);
		state.bullet_trails[47U].center_y[point] =
			static_cast<std::int16_t>(-200 - static_cast<int>(point));
	}
	state.bullets[318U].flag = static_cast<th03::detail::BulletFlag>(5U);
	state.bullets[317U].has_trail = true;
	state.bullets[317U].trail_index = 47U;

	const auto shots_enemies =
		th03::detail::ObserverAccess::shots_enemies(state);
	check(
		shots_enemies.context() == th03::MainPhaseContext{
			.round_id = 3U,
			.round_or_result_frame = 0x1234U,
			.playfield_fg_shift_x = {-6, 6},
		},
		"ShotsEnemies context preserves round timing and foreground shifts"
	);
	for(std::size_t slot_id = 0U;
		slot_id < th03::ShotsEnemiesView::shot_count;
		++slot_id) {
		const auto fact = shots_enemies.shot(
			static_cast<th03::ShotPairSlot>(slot_id)
		);
		check(
			fact && static_cast<std::size_t>(fact->slot) == slot_id,
			"shot accessor preserves the requested fixed-pool slot"
		);
	}
	check(
		shots_enemies.shot(static_cast<th03::ShotPairSlot>(31U)).value() ==
			th03::ShotPairFact{
				.slot = static_cast<th03::ShotPairSlot>(31U),
				.alive = true,
				.topleft_x = -123,
				.topleft_y = 456,
				.player_id = th03::PlayerId::p2,
			},
		"shot fact exposes only exact drawing inputs, not velocity"
	);
	check(
		shots_enemies.enemy(
			static_cast<th03::EnemySlot>(0U)
		)->classification() ==
			th03::EnemyClassification::free,
		"enemy flag 0 classifies as free"
	);
	const auto living = shots_enemies.enemy(
		static_cast<th03::EnemySlot>(1U)
	).value();
	check(
		living.classification() ==
			th03::EnemyClassification::running_spawned &&
		living.flag.raw == 1U && living.frame == 7U &&
		living.center_x == -321 && living.center_y == 654 &&
		living.player_id == th03::PlayerId::p2 &&
		living.size_pixels == 32U && living.hp == 2U &&
		living.explosion_max_enemy_hits_half == 2U,
		"living enemy fact preserves every source drawing input"
	);
	check(
		shots_enemies.enemy(
			static_cast<th03::EnemySlot>(2U)
		)->classification() ==
			th03::EnemyClassification::running_unspawned,
		"enemy flag 3 classifies as running but not yet spawned"
	);
	for(const auto slot_id : {3U, 4U, 5U, 6U}) {
		check(
			shots_enemies.enemy(
				static_cast<th03::EnemySlot>(slot_id)
			)->classification() == th03::EnemyClassification::explosion,
			"enemy explosion keyframe facts classify as explosions"
		);
	}
	check(
		shots_enemies.enemy(
			static_cast<th03::EnemySlot>(4U)
		)->flag.raw == 13U,
		"enemy fact preserves explosion flags above the named value 10"
	);

	const auto fireballs = th03::detail::ObserverAccess::fireballs(state);
	check(
		fireballs.context() == shots_enemies.context(),
		"both phase views expose the same MAIN phase context"
	);
	for(std::size_t slot_id = 0U;
		slot_id < th03::FireballsView::fireball_count;
		++slot_id) {
		const auto fact = fireballs.fireball(
			static_cast<th03::FireballSlot>(slot_id)
		);
		check(
			fact && static_cast<std::size_t>(fact->slot) == slot_id,
			"fireball accessor preserves the requested fixed-pool slot"
		);
	}
	check(
		fireballs.fireball(
			static_cast<th03::FireballSlot>(0U)
		)->classification() == th03::FireballClassification::free,
		"fireball flag 0 classifies as free"
	);
	const auto falling = fireballs.fireball(
		static_cast<th03::FireballSlot>(1U)
	).value();
	check(
		falling.classification() == th03::FireballClassification::fall &&
		falling.flag.raw == 1U && falling.frame == 3U &&
		falling.center_x == -222 && falling.center_y == 777 &&
		falling.player_id == th03::PlayerId::p2 &&
		falling.variant == th03::FireballVariant::red,
		"falling fireball fact preserves every source drawing input"
	);
	check(
		fireballs.fireball(
			static_cast<th03::FireballSlot>(2U)
		)->classification() == th03::FireballClassification::transfer,
		"fireball flag 2 classifies as transfer"
	);
	check(
		fireballs.fireball(
			static_cast<th03::FireballSlot>(3U)
		)->classification() == th03::FireballClassification::explosion &&
		fireballs.fireball(
			static_cast<th03::FireballSlot>(4U)
		)->flag.raw == 14U,
		"fireball explosions preserve their exact raw flags and frames"
	);

	const auto players_bullets =
		th03::detail::ObserverAccess::players_bullets(state);
	check(
		players_bullets.context() == shots_enemies.context(),
		"PlayersBullets exposes the same pre-result MAIN phase context"
	);
	check(
		players_bullets.player(th03::PlayerId::p2).value() ==
			th03::PlayerFact{
				.player_id = th03::PlayerId::p2,
				.center_x = -444,
				.center_y = 555,
				.paletted_character = 16,
				.lose_anim_time = 0x2FU,
				.invincibility_time = 73U,
				.hyper_active = 4U,
				.gauge_charged = 0xABCU,
			},
		"player fact preserves the complete gameplay-semantic draw input"
	);
	check(
		players_bullets.bullet(static_cast<th03::BulletSlot>(319U)).value() ==
			th03::BulletFact{
				.slot = static_cast<th03::BulletSlot>(319U),
				.flag = th03::BulletFlagFact{2U},
				.age = 17U,
				.center_x = -888,
				.center_y = 999,
				.player_id = th03::PlayerId::p2,
				.has_trail = true,
			},
		"bullet fact preserves exact flags, age, raw center, target, and trail presence"
	);
	check(
		players_bullets.trail_point(
			static_cast<th03::BulletSlot>(319U),
			static_cast<th03::TrailPoint>(5U)
		).value() == th03::RawPoint{.x = 105, .y = -205},
		"trail accessor resolves one of exactly six aliased-ring points by value"
	);
	const auto invalid_odd = players_bullets.bullet(
		static_cast<th03::BulletSlot>(318U)
	).value().flag;
	check(
		invalid_odd.raw == 5U && invalid_odd.is_rendered_as_pellet() &&
		!invalid_odd.is_bullet16() && !invalid_odd.is_pellet_cloud(),
		"bullet flag facts preserve invalid odd values and source pellet-pass semantics"
	);
	check(
		!shots_enemies.shot(static_cast<th03::ShotPairSlot>(255U)) &&
		!shots_enemies.enemy(static_cast<th03::EnemySlot>(255U)) &&
		!fireballs.fireball(static_cast<th03::FireballSlot>(255U)) &&
		!players_bullets.player(static_cast<th03::PlayerId>(255U)) &&
		!players_bullets.bullet(static_cast<th03::BulletSlot>(65535U)) &&
		!players_bullets.trail_point(
			static_cast<th03::BulletSlot>(65535U),
			static_cast<th03::TrailPoint>(0U)
		) &&
		!players_bullets.trail_point(
			static_cast<th03::BulletSlot>(319U),
			static_cast<th03::TrailPoint>(255U)
		) &&
		!players_bullets.trail_point(
			static_cast<th03::BulletSlot>(317U),
			static_cast<th03::TrailPoint>(0U)
		),
		"invalid indices and stale free-slot trail bits return no semantic fact"
	);
}

void test_extra_charge_view_contract()
{
	auto state = make_live_state();
	state.round_id = 6U;
	state.round_or_result_frame = 0x3456U;
	state.playfield_fg_shift_x = {-7, 9};
	const auto expected_context = th03::MainPhaseContext{
		.round_id = 6U,
		.round_or_result_frame = 0x3456U,
		.playfield_fg_shift_x = {-7, 9},
	};
	const std::array<std::size_t, 9> extra_counts{
		8U, 6U, 14U, 12U, 10U, 8U, 14U, 16U, 16U,
	};
	for(std::size_t character = 0U; character < extra_counts.size(); ++character) {
		state.players[0U].paletted_character = static_cast<std::int8_t>(
			1U + (character * 2U)
		);
		const auto view = th03::detail::ObserverAccess::main_extra_attack(
			state, th03::PlayerId::p1
		);
		check(
			view.context() == expected_context &&
			view.player() == th03::PlayerId::p1 &&
			view.paletted_character() == state.players[0U].paletted_character &&
			view.slot_count() == extra_counts[character] &&
			!view.attack(static_cast<th03::ExtraAttackSlot>(
				extra_counts[character]
			)),
			"Extra views expose each selected character's exact source slot range"
		);
	}
	auto &extra = state.extra_attacks[0U][15U];
	extra.state = 29U;
	extra.frame = 0xFEU;
	extra.center_x = -1234;
	extra.center_y = 2345;
	extra.player_id = 0xFEU;
	extra.mima_timer = 0xABCDU;
	extra.yumemi_extent = 0x3456U;
	extra.yumemi_duration = 0x4567U;
	extra.kotohime_target_y = -3210;
	extra.behavior_variant = 0x89U;
	for(std::size_t point = 0U; point < extra.ellen_trail_x.size(); ++point) {
		extra.ellen_trail_x[point] = static_cast<std::int16_t>(100 + point);
		extra.ellen_trail_y[point] = static_cast<std::int16_t>(-200 -
			static_cast<int>(point));
	}
	state.players[0U].paletted_character = 17;
	const auto extra_view = th03::detail::ObserverAccess::main_extra_attack(
		state, th03::PlayerId::p1
	);
	check(
		extra_view.attack(static_cast<th03::ExtraAttackSlot>(15U)) ==
			std::optional<th03::MainExtraAttackFact>{th03::MainExtraAttackFact{
				.slot = static_cast<th03::ExtraAttackSlot>(15U),
				.state = 29U,
				.frame = 0xFEU,
				.center_x = -1234,
				.center_y = 2345,
				.destination_raw = 0xFEU,
				.mima_timer_low = 0xCDU,
				.yumemi_extent = 0x3456U,
				.yumemi_duration = 0x4567U,
				.kotohime_target_y = -3210,
				.kotohime_variant = 0x89U,
				.ellen_trail_x = {100, 101, 102, 103, 104, 105, 106},
				.ellen_trail_y = {-200, -201, -202, -203, -204, -205, -206},
			}},
		"Extra facts preserve raw destination, render-specific unions, and Ellen's full trail"
	);
	const auto invalid_extra = th03::detail::ObserverAccess::main_extra_attack(
		state, static_cast<th03::PlayerId>(255U)
	);
	check(
		invalid_extra.slot_count() == 0U &&
		invalid_extra.paletted_character() == 0 &&
		!invalid_extra.attack(static_cast<th03::ExtraAttackSlot>(0U)),
		"invalid Extra player identities cannot index deterministic storage"
	);

	state.players[0U].paletted_character = 1;
	state.reimu_charge[0U][11U].state = 7U;
	state.reimu_charge[0U][11U].x = {10, 11, 12, 13, 14, 15, 16};
	state.reimu_charge[0U][11U].y = {-10, -11, -12, -13, -14, -15, -16};
	auto charge_view = th03::detail::ObserverAccess::main_charge_shot(
		state, th03::PlayerId::p1
	);
	check(
		charge_view.context() == expected_context &&
		charge_view.player() == th03::PlayerId::p1 &&
		charge_view.paletted_character() == 1 &&
		charge_view.reimu_group(
			static_cast<th03::ReimuChargeGroupSlot>(11U)
		) == std::optional<th03::ReimuChargeGroupFact>{{.state = 7U}} &&
		charge_view.reimu_point(
			static_cast<th03::ReimuChargeGroupSlot>(11U),
			th03::ReimuChargePoint::node_6
		) == std::optional<th03::RawPoint>{{16, -16}} &&
		!charge_view.reimu_point(
			static_cast<th03::ReimuChargeGroupSlot>(11U),
			static_cast<th03::ReimuChargePoint>(1U)
		) &&
		!charge_view.reimu_group(
			static_cast<th03::ReimuChargeGroupSlot>(12U)
		) && !charge_view.mima_state(),
		"Reimu Charge exposes only rendered sparse trail points and rejects other characters"
	);

	state.players[0U].paletted_character = 3;
	state.mima_charge_active[0U] = 4U;
	state.mima_charge[0U][14U].active = 5U;
	state.mima_charge[0U][14U].x = -111;
	state.mima_charge[0U][14U].y = 222;
	charge_view = th03::detail::ObserverAccess::main_charge_shot(
		state, th03::PlayerId::p1
	);
	check(
		charge_view.mima_state() ==
			std::optional<th03::MimaChargeStateFact>{{.active = 4U}} &&
		charge_view.mima_node(static_cast<th03::MimaChargeNodeSlot>(14U)) ==
			std::optional<th03::MimaChargeNodeFact>{{5U, -111, 222}} &&
		!charge_view.mima_node(static_cast<th03::MimaChargeNodeSlot>(15U)),
		"Mima Charge separates callback activity from per-node activity"
	);

	state.players[0U].paletted_character = 5;
	state.marisa_charge[0U].active = 6U;
	state.marisa_charge[0U].x[11U] = -333;
	state.marisa_charge[0U].y[11U] = 444;
	charge_view = th03::detail::ObserverAccess::main_charge_shot(
		state, th03::PlayerId::p1
	);
	check(
		charge_view.marisa_state() ==
			std::optional<th03::MarisaChargeStateFact>{{.active = 6U}} &&
		charge_view.marisa_node(static_cast<th03::MarisaChargeNodeSlot>(11U)) ==
			std::optional<th03::MarisaChargeNodeFact>{{-333, 444}} &&
		!charge_view.marisa_node(static_cast<th03::MarisaChargeNodeSlot>(12U)),
		"Marisa Charge exposes its active flag and all twelve draw positions"
	);

	state.players[0U].paletted_character = 7;
	state.ellen_charge[0U][31U].state = 8U;
	state.ellen_charge[0U][31U].angle = 9U;
	state.ellen_charge[0U][31U].x = -555;
	state.ellen_charge[0U][31U].y = 666;
	charge_view = th03::detail::ObserverAccess::main_charge_shot(
		state, th03::PlayerId::p1
	);
	check(
		charge_view.ellen_node(static_cast<th03::EllenChargeNodeSlot>(31U)) ==
			std::optional<th03::EllenChargeNodeFact>{{8U, 9U, -555, 666}} &&
		!charge_view.ellen_node(static_cast<th03::EllenChargeNodeSlot>(32U)),
		"Ellen Charge exposes state, angle, and position for all 32 nodes"
	);

	state.players[0U].paletted_character = 9;
	state.kotohime_charge[0U] = th03::detail::KotohimeChargeState{
		.active = 10U, .x = -777, .y = 888, .velocity_y = 999,
	};
	charge_view = th03::detail::ObserverAccess::main_charge_shot(
		state, th03::PlayerId::p1
	);
	check(
		charge_view.kotohime() ==
			std::optional<th03::KotohimeChargeFact>{{10U, -777, 888}},
		"Kotohime Charge omits its update-only velocity"
	);

	state.players[0U].paletted_character = 11;
	state.kana_charge_state[0U] = 11U;
	state.kana_charge[0U][3U].x[12U] = -999;
	state.kana_charge[0U][3U].y[12U] = 1111;
	charge_view = th03::detail::ObserverAccess::main_charge_shot(
		state, th03::PlayerId::p1
	);
	check(
		charge_view.kana_state() ==
			std::optional<th03::KanaChargeStateFact>{{.state = 11U}} &&
		charge_view.kana_point(
			static_cast<th03::KanaChargeGroupSlot>(3U),
			th03::KanaChargePoint::node_12
		) == std::optional<th03::RawPoint>{{-999, 1111}} &&
		!charge_view.kana_point(
			static_cast<th03::KanaChargeGroupSlot>(4U),
			th03::KanaChargePoint::node_0
		) && !charge_view.kana_point(
			static_cast<th03::KanaChargeGroupSlot>(3U),
			static_cast<th03::KanaChargePoint>(1U)
		),
		"Kana Charge exposes only four sparse draw points per group"
	);

	state.players[0U].paletted_character = 13;
	state.rikako_charge_state[0U] = 12U;
	state.rikako_charge[0U][3U].x = -1212;
	state.rikako_charge[0U][3U].y = 1313;
	charge_view = th03::detail::ObserverAccess::main_charge_shot(
		state, th03::PlayerId::p1
	);
	check(
		charge_view.rikako_state() ==
			std::optional<th03::RikakoChargeStateFact>{{.state = 12U}} &&
		charge_view.rikako_node(static_cast<th03::RikakoChargeNodeSlot>(3U)) ==
			std::optional<th03::RikakoChargeNodeFact>{{-1212, 1313}} &&
		!charge_view.rikako_node(static_cast<th03::RikakoChargeNodeSlot>(4U)),
		"Rikako Charge omits update-only angles and geometry history"
	);

	state.players[0U].paletted_character = 15;
	state.chiyuri_charge[0U][7U] = th03::detail::ChiyuriChargeNode{
		.state = 13U, .frame = 14U, .x = -1414, .y = 1515,
	};
	charge_view = th03::detail::ObserverAccess::main_charge_shot(
		state, th03::PlayerId::p1
	);
	check(
		charge_view.chiyuri_node(
			static_cast<th03::ChiyuriChargeNodeSlot>(7U)
		) == std::optional<th03::ChiyuriChargeNodeFact>{{13U, 14U, -1414, 1515}} &&
		!charge_view.chiyuri_node(
			static_cast<th03::ChiyuriChargeNodeSlot>(8U)
		),
		"Chiyuri Charge preserves the reverse-rendered node state and frame"
	);

	state.players[0U].paletted_character = 17;
	state.yumemi_charge[0U] = th03::detail::YumemiChargeState{
		.state = 15U, .frame = 16U, .x = -1616, .y = 1717,
	};
	charge_view = th03::detail::ObserverAccess::main_charge_shot(
		state, th03::PlayerId::p1
	);
	check(
		charge_view.yumemi() ==
			std::optional<th03::YumemiChargeFact>{{15U, 16U, -1616, 1717}} &&
		!charge_view.reimu_group(static_cast<th03::ReimuChargeGroupSlot>(0U)) &&
		!charge_view.mima_state() && !charge_view.marisa_state() &&
		!charge_view.ellen_node(static_cast<th03::EllenChargeNodeSlot>(0U)) &&
		!charge_view.kotohime() && !charge_view.kana_state() &&
		!charge_view.rikako_state() &&
		!charge_view.chiyuri_node(
			static_cast<th03::ChiyuriChargeNodeSlot>(0U)
		),
		"Charge accessors reject every non-selected character without leaking union storage"
	);
	const auto invalid_charge = th03::detail::ObserverAccess::main_charge_shot(
		state, static_cast<th03::PlayerId>(255U)
	);
	const auto all_charge_accessors_reject = [](const auto &view) noexcept {
		return !view.reimu_group(
			static_cast<th03::ReimuChargeGroupSlot>(0U)
		) && !view.reimu_point(
			static_cast<th03::ReimuChargeGroupSlot>(0U),
			th03::ReimuChargePoint::node_0
		) && !view.mima_state() && !view.mima_node(
			static_cast<th03::MimaChargeNodeSlot>(0U)
		) && !view.marisa_state() && !view.marisa_node(
			static_cast<th03::MarisaChargeNodeSlot>(0U)
		) && !view.ellen_node(
			static_cast<th03::EllenChargeNodeSlot>(0U)
		) && !view.kotohime() && !view.kana_state() && !view.kana_point(
			static_cast<th03::KanaChargeGroupSlot>(0U),
			th03::KanaChargePoint::node_0
		) && !view.rikako_state() && !view.rikako_node(
			static_cast<th03::RikakoChargeNodeSlot>(0U)
		) && !view.chiyuri_node(
			static_cast<th03::ChiyuriChargeNodeSlot>(0U)
		) && !view.yumemi();
	};
	check(
		invalid_charge.paletted_character() == 0 &&
		all_charge_accessors_reject(invalid_charge),
		"every Charge accessor rejects invalid player identities before indexing storage"
	);
	constexpr std::array<std::int8_t, 4U> invalid_paletted_characters{
		-1, 0, 19, 127,
	};
	for(const auto invalid_paletted_character : invalid_paletted_characters) {
		state.players[0U].paletted_character = invalid_paletted_character;
		const auto invalid_character_extra =
			th03::detail::ObserverAccess::main_extra_attack(
				state, th03::PlayerId::p1
			);
		const auto invalid_character_charge =
			th03::detail::ObserverAccess::main_charge_shot(
				state, th03::PlayerId::p1
			);
		check(
			invalid_character_extra.slot_count() == 0U &&
			!invalid_character_extra.attack(
				static_cast<th03::ExtraAttackSlot>(0U)
			) && all_charge_accessors_reject(invalid_character_charge),
			"every Extra/Charge accessor rejects an out-of-domain paletted character"
		);
	}
}

void test_main_boss_view_contract()
{
	auto state = make_live_state();
	state.round_id = 4U;
	state.round_frame = 7U;
	state.round_or_result_frame = 0x4321U;
	state.playfield_fg_shift_x = {-9, 11};
	auto view = th03::detail::ObserverAccess::main_boss(state);
	check(
		view.context() == th03::MainPhaseContext{
			.round_id = 4U,
			.round_or_result_frame = 0x4321U,
			.playfield_fg_shift_x = {-9, 11},
		} && view.round_frame_is_odd() && !view.boss(),
		"an inactive Boss view retains MAIN timing without fabricating a Boss"
	);

	state.gba_boss_launched_by = 2;
	view = th03::detail::ObserverAccess::main_boss(state);
	check(
		!view.boss() && !view.reimu_orbit_point(
			static_cast<th03::ReimuBossOrbitPoint>(0U)
		),
		"an invalid Boss owner returns no semantic fact"
	);

	state.gba_boss_launched_by = 1;
	state.players[1U].paletted_character = 2;
	state.boss.center_x = -1234;
	state.boss.center_y = 2345;
	state.boss.hit = 7U;
	state.boss.state = 16U;
	state.boss.pattern_frame = 0x3456U;
	state.boss.render_state = 5U;
	state.boss.render_frame = 6U;
	state.boss.pattern_angle = 0x78U;
	state.boss.anchor_x = 999;
	state.boss.params[3U] = 9U;
	state.yumemi_boss_radius = 888U;
	for(std::size_t point = 0U;
		point < th03::MainBossView::reimu_orbit_point_count;
		++point) {
		state.boss.orbit_x[point] = static_cast<std::int16_t>(100 + point);
		state.boss.orbit_y[point] = static_cast<std::int16_t>(
			-200 - static_cast<int>(point)
		);
	}
	view = th03::detail::ObserverAccess::main_boss(state);
	check(
		view.boss() == std::optional<th03::MainBossFact>{th03::MainBossFact{
			.owner = th03::PlayerId::p2,
			.paletted_character = 2,
			.center_x = -1234,
			.center_y = 2345,
			.hit = 7U,
			.state = 16U,
			.pattern_frame = 0x3456U,
			.render_state = 5U,
			.render_frame = 6U,
			.pattern_angle = 0x78U,
			.reimu_orbit_count = 6U,
		}} &&
		view.reimu_orbit_point(
			static_cast<th03::ReimuBossOrbitPoint>(5U)
		) == std::optional<th03::RawPoint>{th03::RawPoint{105, -205}} &&
		!view.reimu_orbit_point(
			static_cast<th03::ReimuBossOrbitPoint>(6U)
		) &&
		!view.reimu_orbit_point(
			static_cast<th03::ReimuBossOrbitPoint>(255U)
		),
		"Reimu Boss facts normalize the active orbit count and check all orbit-point indices"
	);

	state.players[1U].paletted_character = 10;
	state.boss.params[1U] = 11U;
	view = th03::detail::ObserverAccess::main_boss(state);
	check(
		view.boss()->kotohime_radius == 999 &&
		view.boss()->kotohime_orbit_count == 11U &&
		view.boss()->reimu_orbit_count == 0U &&
		!view.reimu_orbit_point(
			static_cast<th03::ReimuBossOrbitPoint>(0U)
		),
		"Kotohime exposes semantic radius and orbit count without leaking Reimu points"
	);

	state.players[1U].paletted_character = 18;
	state.yumemi_boss_radius = 0x6789U;
	state.yumemi_boss_muzzle_x = -321;
	state.yumemi_boss_muzzle_y = 654;
	state.yumemi_boss_target_x = 777;
	state.yumemi_boss_target_y = -888;
	view = th03::detail::ObserverAccess::main_boss(state);
	check(
		view.boss()->kotohime_radius == 0 &&
		view.boss()->kotohime_orbit_count == 0U &&
		view.boss()->yumemi_radius == 0x6789U &&
		view.boss()->yumemi_muzzle == th03::RawPoint{-321, 654} &&
		view.boss()->yumemi_target == th03::RawPoint{777, -888},
		"Yumemi Boss facts expose only the retained radius, muzzle, and target semantics"
	);
}

void test_observer_invariance_and_phase_order()
{
	auto base = make_live_state();
	base.round_id = 1U;
	base.round_or_result_frame = 99U;
	base.playfield_fg_shift_x = {6, -6};
	base.players[1U].paletted_character = 2;
	seed_extra_charge_state(base);
	auto null_state = base;
	auto discard_state = base;
	auto record_state = base;
	DiscardObserver discard;
	RecordingObserver recording(&record_state);

	th03::detail::gameplay_frame_suffix_after_interstitial(null_state);
	th03::detail::gameplay_frame_suffix_after_interstitial(
		discard_state, &discard
	);
	th03::detail::gameplay_frame_suffix_after_interstitial(
		record_state, &recording
	);
	check(
		observed_gameplay_equal(null_state, discard_state) &&
		observed_gameplay_equal(null_state, record_state),
		"null, discard, and recording observers leave hashes, entity pools, and RNG identical"
	);
	check(
		recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
			ObservedPhase::extra_p1,
			ObservedPhase::charge_p1,
			ObservedPhase::extra_p2,
			ObservedPhase::charge_p2,
			ObservedPhase::players_bullets,
		},
		"ordinary late suffix reports all seven source draw phases exactly once"
	);
	check(
		recording.main_frame_footers ==
			std::vector<th03::MainFrameFooterReached>{{}} &&
			recording.calls == std::vector<ObservedCall>{
			ObservedCall::shots_enemies,
			ObservedCall::fireballs,
			ObservedCall::extra_p1,
			ObservedCall::charge_p1,
			ObservedCall::extra_p2,
			ObservedCall::charge_p2,
			ObservedCall::players_bullets,
			ObservedCall::main_frame_footer_reached,
		},
		"the normal late suffix reaches its footer after all seven phase views"
	);
	check(
		recording.shots_enemies_context == th03::MainPhaseContext{
			.round_id = 1U,
			.round_or_result_frame = 99U,
			.playfield_fg_shift_x = {6, -6},
		} &&
		recording.fireballs_context == recording.shots_enemies_context &&
		recording.extra_contexts[0U] == recording.shots_enemies_context &&
		recording.charge_contexts[0U] == recording.shots_enemies_context &&
		recording.extra_contexts[1U] == recording.shots_enemies_context &&
		recording.charge_contexts[1U] == recording.shots_enemies_context &&
		recording.players_bullets_context == recording.shots_enemies_context,
		"all observations precede result control and the late frame-counter increment"
	);
	check(
		recording.extra_first_attacks[0U]->frame == 0xFFU &&
		recording.extra_first_attacks[1U]->frame == 30U &&
		recording.extra_frames_at_late_phases ==
			std::vector<std::array<std::uint8_t, 2>>{
				{0xFFU, 30U},
				{0U, 30U},
				{0U, 30U},
				{0U, 31U},
			} &&
		record_state.extra_attacks[0U][0U].frame == 0U &&
		record_state.extra_attacks[1U][0U].frame == 31U,
		"each Reimu Extra view precedes its wrapping frame increment and each Charge view follows it"
	);
}

void test_public_step_result_invariance()
{
	const auto init = base_game_init();
	auto null_game = th03::Game::create(init);
	auto discard_game = th03::Game::create(init);
	auto record_game = th03::Game::create(init);
	check(
		null_game && discard_game && record_game,
		"public observer invariance fixtures create"
	);
	if(!null_game || !discard_game || !record_game) {
		return;
	}
	DiscardObserver discard;
	RecordingObserver recording;
	const auto null_result = null_game->step({});
	const auto discard_result = discard_game->step(
		{}, th03::InputContext::gameplay, &discard
	);
	const auto record_result = record_game->step(
		{}, th03::InputContext::gameplay, &recording
	);
	check(
		null_result.status == discard_result.status &&
		null_result.status == record_result.status &&
		canonical_equal(
			null_result.input_boundary_state,
			discard_result.input_boundary_state
		) &&
		canonical_equal(
			null_result.input_boundary_state,
			record_result.input_boundary_state
		) &&
		canonical_equal(
			null_game->canonical_state(), discard_game->canonical_state()
		) &&
		canonical_equal(
			null_game->canonical_state(), record_game->canonical_state()
		),
		"public step returns identical boundaries and final canonical state for null, discard, and recording observers"
	);
}

void test_shot_spawn_and_reuse()
{
	auto null_state = make_live_state();
	auto record_state = null_state;
	null_state.input_player[0] = 0x0020U;
	record_state.input_player[0] = 0x0020U;
	null_state.autofire_shot_gate[0] = 4U;
	record_state.autofire_shot_gate[0] = 4U;
	RecordingObserver recording;

	(void)th03::detail::gameplay_frame_suffix_before_interstitial(null_state);
	(void)th03::detail::gameplay_frame_suffix_before_interstitial(
		record_state, &recording
	);
	check(
		observed_gameplay_equal(null_state, record_state),
		"ShotPairSpawned observation leaves hashes, entity pools, and RNG identical"
	);
	check(
		recording.spawns == std::vector<th03::ShotPairSpawned>{
			{
				.slot = static_cast<th03::ShotPairSlot>(0U),
				.player_id = th03::PlayerId::p1,
			},
		},
		"successful one-pair allocation reports its first free source slot"
	);

	record_state.shotpairs[0U].alive = false;
	record_state.autofire_shot_gate[0] = 4U;
	record_state.input_player[0] = 0x0020U;
	(void)th03::detail::gameplay_frame_suffix_before_interstitial(
		record_state, &recording
	);
	check(
		recording.spawns.size() == 2U &&
		recording.spawns[1U].slot ==
			static_cast<th03::ShotPairSlot>(0U),
		"reusing a freed shot slot emits a second cel-reset lifecycle fact"
	);

	for(auto &shot : record_state.shotpairs) {
		shot.alive = true;
	}
	record_state.autofire_shot_gate[0] = 4U;
	record_state.input_player[0] = 0x0020U;
	(void)th03::detail::gameplay_frame_suffix_before_interstitial(
		record_state, &recording
	);
	check(
		recording.spawns.size() == 2U,
		"a full shot pool emits no unsuccessful allocation event"
	);
}

void test_main_feedback_source_order()
{
	with_live_state([](auto &shot) {
		shot.input_player[0U] = 0x0020U;
		shot.autofire_shot_gate[0U] = 4U;
		FeedbackRecordingObserver recording;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(
			shot, &recording
		);
		check(
			recording.requests ==
				std::vector<th03::MainFeedbackCueRequested>{{
					.site = th03::MainFeedbackCueSite::shot_fired,
					.player = th03::PlayerId::p1,
				}} &&
			recording.calls == std::vector<FeedbackObservedCall>{
				FeedbackObservedCall::request,
				FeedbackObservedCall::dispatch_boundary,
			},
			"a shot request precedes the one source snd_se_update boundary"
		);
	});

	with_live_state([](auto &bomb) {
		bomb.players[0U].paletted_character = 1;
		bomb.bomb_flag[0U] = th03::detail::BombFlag::active;
		bomb.bomb_frame[0U] = 1U;
		FeedbackRecordingObserver recording;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(
			bomb, &recording
		);
		th03::detail::gameplay_frame_suffix_after_interstitial(
			bomb, &recording
		);
		check(
			recording.requests ==
				std::vector<th03::MainFeedbackCueRequested>{{
					.site = th03::MainFeedbackCueSite::bomb_started,
					.player = th03::PlayerId::p1,
				}} &&
			recording.calls == std::vector<FeedbackObservedCall>{
				FeedbackObservedCall::dispatch_boundary,
				FeedbackObservedCall::request,
			},
			"a Bomb start request remains pending after the source update boundary"
		);
	});

	with_live_state([](auto &yumemi) {
		yumemi.hud_start_flag = 1U;
		yumemi.players[0U].paletted_character = 17;
		yumemi.gba_boss_launched_by = 0;
		yumemi.boss.state = 0U;
		yumemi.boss.pattern_frame = 0x17U;
		yumemi.boss.hp = 100U;
		FeedbackRecordingObserver recording;
		(void)th03::detail::gameplay_frame_prefix(
			yumemi, th03::GameContent{}, &recording
		);
		check(
			recording.requests ==
				std::vector<th03::MainFeedbackCueRequested>{{
					.site = th03::MainFeedbackCueSite::
						yumemi_boss_materialization,
					.player = th03::PlayerId::p1,
				}},
			"Yumemi materialization exposes the source reset-then-request site"
		);
	});
}

void test_player_pose_and_defeat_lifecycle()
{
	with_live_state([](auto &state) {
		state.players[0U].speed_aligned_x = 32U;
		state.players[0U].speed_aligned_y = 32U;
		state.players[0U].center_x = th03::exact::Subpixel::from_raw(8 << 4);
		state.input_player[0U] = 0x0004U;
		RecordingObserver clamped;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(state,
																	  &clamped);
		check(clamped.poses.size() == 2U &&
				  clamped.poses[0U] ==
					  th03::PlayerPoseCommitted{
						  .player = th03::PlayerId::p1,
						  .final_velocity_x_write_q4_4 = std::int8_t{-32},
					  } &&
				  state.players[0U].center_x.raw() == (8 << 4),
			  "pose lifecycle preserves a nonzero source velocity write even "
			  "when position clamps");
		check(clamped.poses[1U].final_velocity_x_write_q4_4 == std::int8_t{0},
			  "no-input player_move reports an explicit zero write, not stale "
			  "reuse");
	});
	with_live_state([](auto &invalid_input) {
		invalid_input.input_player[0U] = 0x000CU;
		RecordingObserver invalid_movement;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(
			invalid_input, &invalid_movement);
		check(invalid_movement.poses[0U].final_velocity_x_write_q4_4 ==
				  std::int8_t{0},
			  "invalid human movement still performs source player_move's "
			  "explicit zero write");
	});

	with_live_state([](auto &move_locked) {
		move_locked.players[0U].move_lock_time = 1U;
		RecordingObserver locked;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(
			move_locked, &locked);
		check(locked.poses.size() == 2U &&
				  !locked.poses[0U].final_velocity_x_write_q4_4,
			  "move-lock path commits the pose while preserving the shared "
			  "stale velocity");
	});

	with_live_state([](auto &knockback) {
		knockback.players[0U].knockback_active = true;
		knockback.players[0U].knockback_time = 33U;
		knockback.players[0U].knockback_angle = 0U;
		knockback.players[0U].knockback_length = 64U;
		RecordingObserver knocked;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(knockback,
																	  &knocked);
		check(knocked.poses[0U].final_velocity_x_write_q4_4 == std::int8_t{62},
			  "later knockback vector write wins over the player's earlier "
			  "movement write");
	});
	with_live_state([](auto &knockback_no_write) {
		knockback_no_write.players[0U].move_lock_time = 1U;
		knockback_no_write.players[0U].knockback_active = true;
		knockback_no_write.players[0U].knockback_time = 32U;
		RecordingObserver knockback_stale;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(
			knockback_no_write, &knockback_stale);
		check(!knockback_stale.poses[0U].final_velocity_x_write_q4_4,
			  "late knockback half plus move-lock performs no source velocity "
			  "write");
	});

	with_live_state([](auto &cpu_retry) {
		cpu_retry.players[0U].is_cpu = 1U;
		cpu_retry.players[0U].cpu_safety_frames = 100U;
		cpu_retry.players[0U].speed_aligned_x = 32U;
		cpu_retry.players[0U].speed_aligned_y = 32U;
		for (auto &column : cpu_retry.collision_map[0U]) {
			column.fill(true);
		}
		RecordingObserver cpu;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(cpu_retry,
																	  &cpu);
		check(cpu.poses.size() == 2U &&
				  cpu.poses[0U].final_velocity_x_write_q4_4 == std::int8_t{0},
			  "CPU retry exhaustion commits the final retry's explicit "
			  "vertical X write");
	});

	with_live_state([](auto &defeat) {
		defeat.round_or_result_frame = 0x1235U;
		defeat.players[0U].lose_anim_time = 0x30U;
		defeat.players[0U].center_x = th03::exact::Subpixel::from_raw(111);
		defeat.players[0U].center_y = th03::exact::Subpixel::from_raw(222);
		defeat.players[1U].lose_anim_time = 0x30U;
		defeat.players[1U].center_x = th03::exact::Subpixel::from_raw(333);
		defeat.players[1U].center_y = th03::exact::Subpixel::from_raw(444);
		defeat.playfield_fg_shift_x = {12, -14};
		RecordingObserver defeated;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(
			defeat, &defeated);
		check(defeated.defeat_steps ==
				  std::vector<th03::PlayerDefeatStep>{
					  {
						.player = th03::PlayerId::p1,
						.lose_anim_time_before = 0x30U,
						.center_x = 111,
						.center_y = 222,
						.fg_shift_x_before_defeat_shake = 12,
						.round_or_result_frame_before = 0x1235U,
					  },
					  {
						.player = th03::PlayerId::p2,
						.lose_anim_time_before = 0x30U,
						.center_x = 333,
						.center_y = 444,
						.fg_shift_x_before_defeat_shake = -14,
						.round_or_result_frame_before = 0x1235U,
					  },
				  },
			  "simultaneous defeat steps expose pre-shake facts in P1-then-P2 "
			  "source order");
		check(defeated.calls ==
				  std::vector<ObservedCall>{
					  ObservedCall::player_defeat_step,
					  ObservedCall::player_pose_committed,
					  ObservedCall::player_defeat_step,
					  ObservedCall::player_pose_committed,
				  },
			  "each defeat mutation precedes its corresponding source pose "
			  "commit");
		check(!defeated.poses[0U].final_velocity_x_write_q4_4 &&
				  !defeated.poses[1U].final_velocity_x_write_q4_4,
			  "defeat update_patnums calls preserve the prior shared pose "
			  "velocity");
	});

	with_live_state([](auto &banner) {
		banner.defeat_flag = th03::detail::DefeatFlag::banner;
		RecordingObserver banner_recording;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(
			banner, &banner_recording);
		check(banner_recording.defeat_steps.empty() &&
				  banner_recording.poses.size() == 2U &&
				  !banner_recording.poses[0U].final_velocity_x_write_q4_4 &&
				  !banner_recording.poses[1U].final_velocity_x_write_q4_4,
			  "banner paths still commit two poses but perform no "
			  "defeat-update step");
	});
	with_live_state([](auto &lost) {
		lost.players[0U].lose_anim_time = 0xFFU;
		RecordingObserver lost_recording;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(
			lost, &lost_recording);
		check(lost_recording.defeat_steps.empty() &&
				  !lost_recording.poses[0U].final_velocity_x_write_q4_4,
			  "lose-animation sentinel commits a stale pose without a defeat "
			  "step");
	});
	with_live_state([](auto &banner_mid_pair) {
		banner_mid_pair.players[0U].lose_anim_time = 1U;
		banner_mid_pair.players[1U].lose_anim_time = 2U;
		RecordingObserver banner_order;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(
			banner_mid_pair, &banner_order);
		check(banner_order.defeat_steps.size() == 1U &&
				  banner_order.defeat_steps[0U].player == th03::PlayerId::p1 &&
				  banner_order.poses.size() == 2U,
			  "P1 timer 1 enters banner before P2 and suppresses P2's pending "
			  "defeat step");
	});

	with_live_state([](auto &fatal) {
		fatal.players[0U].halfhearts = 1U;
		fatal.players[0U].hit_damage_next = 3U;
		for (auto &column : fatal.collision_map[0U]) {
			column.fill(true);
		}
		RecordingObserver fatal_hit;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(
			fatal, &fatal_hit);
		check(fatal.players[0U].lose_anim_time == 0x30U &&
				  fatal_hit.defeat_steps.empty() &&
				  fatal_hit.hit_circles ==
					  std::vector<th03::HitCircleSpawned>{
						  {
							  .kind = th03::HitCircleKind::player,
							  .player = th03::PlayerId::p1,
							  .center_x = 144 << 4,
							  .center_y = 336 << 4,
							  .fg_shift_x_at_spawn = 0,
						  },
					  },
			  "fatal collision arms defeat without emitting a same-update "
			  "defeat step and emits the exact player hit circle");
		RecordingObserver fatal_next;
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(
			fatal, &fatal_next);
		check(
			fatal_next.defeat_steps.size() == 1U &&
				fatal_next.defeat_steps[0U].lose_anim_time_before == 0x30U,
			"the first defeat step is emitted on the following player update");
	});
}

void test_bullet16_lifecycle_styles_order_and_full_arena()
{
	const auto configure_mima_gauge = [](auto &state) {
		state.hud_start_flag = 1U;
		state.players[0U].paletted_character = 3;
		state.gba_flag_active[0U] =
			th03::detail::GaugeBossAttackFlag::gauge_bullet_active;
		state.mima_gauge_pattern_frame[0U] = 8U;
		state.mima_gauge_pattern_angle[0U] = 0x40U;
		state.mima_gauge_pattern_angle_delta[0U] = 7;
		state.gauge_pattern_param_a[0U] = 4U;
		state.gauge_pattern_param_b[0U] = 48U;
	};
	with_live_state([&](auto &mima_base) {
		configure_mima_gauge(mima_base);
		auto mima_null = mima_base;
		auto mima_discard = mima_base;
		auto mima_record = mima_base;
		DiscardObserver discard;
		RecordingObserver mima;
		(void)th03::detail::gameplay_frame_prefix(mima_null,
												  th03::GameContent{});
		(void)th03::detail::gameplay_frame_prefix(
			mima_discard, th03::GameContent{}, &discard);
		(void)th03::detail::gameplay_frame_prefix(
			mima_record, th03::GameContent{}, &mima);
		check(observed_gameplay_equal(mima_null, mima_discard) &&
				  observed_gameplay_equal(mima_null, mima_record),
			  "null, discard, and recording bullet lifecycle paths leave pools "
			  "and RNG identical");
		check(
			mima.bullet_spawns.size() == 9U &&
				std::all_of(mima.bullet_spawns.begin(),
							mima.bullet_spawns.end(),
							[](const th03::Bullet16Spawned &spawn) {
								return spawn.source_player ==
										   th03::PlayerId::p1 &&
									   spawn.construction ==
										   th03::Bullet16ConstructionKind::
											   default_attack &&
									   !spawn.source_template_animation_enabled;
							}) &&
				std::is_sorted(mima.bullet_spawns.begin(),
							   mima.bullet_spawns.end(),
							   [](const auto &left, const auto &right) {
								   return static_cast<std::size_t>(left.slot) <
										  static_cast<std::size_t>(right.slot);
							   }),
			"one multi-add Mima Gauge callback emits strictly ordered, "
			"uniformly static default facts");
		check(mima.phases.empty() && mima.poses.empty() &&
				  mima.calls.size() == 13U &&
				  mima.calls.front() == ObservedCall::hit_circles_advanced &&
				  mima.calls[1U] == ObservedCall::common_effect_spawned &&
				  mima.calls[2U] == ObservedCall::common_effect_spawned &&
				  std::all_of(
					  mima.calls.begin() + 3, mima.calls.begin() + 12,
					  [](ObservedCall call) {
						  return call == ObservedCall::bullet16_spawned;
					  }
				  ) &&
				  mima.calls.back() == ObservedCall::common_effects_advanced,
			  "prefix Bullet16 facts are observable before a "
			  "replay-pause-style deferred suffix");
		(void)th03::detail::gameplay_frame_suffix_before_interstitial(
			mima_record, &mima);
		th03::detail::gameplay_frame_suffix_after_interstitial(mima_record,
															   &mima);
		check(mima.calls.size() == 23U &&
				  mima.calls[13U] == ObservedCall::player_pose_committed &&
				  mima.calls[14U] == ObservedCall::player_pose_committed &&
				  mima.calls[15U] == ObservedCall::shots_enemies &&
				  mima.calls[16U] == ObservedCall::fireballs &&
				  mima.calls[17U] == ObservedCall::extra_p1 &&
				  mima.calls[18U] == ObservedCall::charge_p1 &&
				  mima.calls[19U] == ObservedCall::extra_p2 &&
				  mima.calls[20U] == ObservedCall::charge_p2 &&
				  mima.calls[21U] == ObservedCall::players_bullets &&
				  mima.calls[22U] == ObservedCall::main_frame_footer_reached,
			  "lifecycle prefix, P1/P2 pose commits, phase views, and footer "
			  "retain source order");
	});

	with_live_state([](auto &chiyuri_state) {
		chiyuri_state.hud_start_flag = 1U;
		chiyuri_state.players[0U].paletted_character = 15;
		chiyuri_state.gba_flag_active[0U] =
			th03::detail::GaugeBossAttackFlag::gauge_bullet_active;
		chiyuri_state.chiyuri_gauge_pattern_frame[0U] = 1U;
		chiyuri_state.gauge_pattern_param_a[0U] = 64U;
		RecordingObserver chiyuri;
		(void)th03::detail::gameplay_frame_prefix(
			chiyuri_state, th03::GameContent{}, &chiyuri);
		check(chiyuri.bullet_spawns.size() == 2U &&
				  std::all_of(
					  chiyuri.bullet_spawns.begin(),
					  chiyuri.bullet_spawns.end(),
					  [](const th03::Bullet16Spawned &spawn) {
						  return !spawn.source_template_animation_enabled &&
								 spawn.construction ==
									 th03::Bullet16ConstructionKind::
										 default_attack;
					  }),
			  "one Chiyuri Gauge callback cannot mix animation or construction "
			  "styles");
	});

	with_live_state([](auto &mima_boss_state) {
		mima_boss_state.hud_start_flag = 1U;
		mima_boss_state.players[0U].paletted_character = 3;
		mima_boss_state.gba_boss_launched_by = 0;
		mima_boss_state.boss.state = 6U;
		mima_boss_state.boss.pattern_frame = 0x3FU;
		mima_boss_state.boss.center_x = 144 << 4;
		mima_boss_state.boss.center_y = 80 << 4;
		mima_boss_state.boss.hp = 110U;
		mima_boss_state.boss.params[1U] = 1U;
		mima_boss_state.boss.params[2U] = 40U;
		RecordingObserver mima_boss;
		(void)th03::detail::gameplay_frame_prefix(
			mima_boss_state, th03::GameContent{}, &mima_boss);
		check(mima_boss.bullet_spawns.size() > 32U &&
				  std::all_of(
					  mima_boss.bullet_spawns.begin(),
					  mima_boss.bullet_spawns.end(),
					  [](const th03::Bullet16Spawned &spawn) {
						  return !spawn.source_template_animation_enabled &&
								 spawn.construction ==
									 th03::Bullet16ConstructionKind::
										 default_attack;
					  }),
			  "same-tick Mima Boss random burst and ring remain uniformly "
			  "static");
	});

	with_live_state([](auto &marisa_boss_state) {
		marisa_boss_state.hud_start_flag = 1U;
		marisa_boss_state.players[0U].paletted_character = 5;
		marisa_boss_state.gba_boss_launched_by = 0;
		marisa_boss_state.gba_boss_level = 8U;
		marisa_boss_state.boss.state = 11U;
		marisa_boss_state.boss.pattern_frame = 0x1FU;
		marisa_boss_state.boss.center_x = 144 << 4;
		marisa_boss_state.boss.center_y = 80 << 4;
		marisa_boss_state.boss.hp = 110U;
		marisa_boss_state.boss.params[2U] = 40U;
		marisa_boss_state.boss.params[3U] = 18U;
		marisa_boss_state.boss.params[4U] = 30U;
		RecordingObserver marisa_boss;
		(void)th03::detail::gameplay_frame_prefix(
			marisa_boss_state, th03::GameContent{}, &marisa_boss);
		check(!marisa_boss.bullet_spawns.empty() &&
				  std::all_of(
					  marisa_boss.bullet_spawns.begin(),
					  marisa_boss.bullet_spawns.end(),
					  [](const th03::Bullet16Spawned &spawn) {
						  return spawn.source_template_animation_enabled &&
								 spawn.construction ==
									 th03::Bullet16ConstructionKind::
										 default_attack;
					  }),
			  "ordinary Boss bullet16 producers report default animated "
			  "construction");
	});

	with_live_state([](auto &kana_state) {
		kana_state.hud_start_flag = 1U;
		kana_state.players[0U].paletted_character = 11;
		auto &kana_attack = kana_state.extra_attacks[0U][0U];
		kana_attack.state = 1U;
		kana_attack.frame = 0U;
		kana_attack.center_x = 100 << 4;
		kana_attack.center_y = 100 << 4;
		kana_attack.angle = 0U;
		kana_attack.speed = 0U;
		RecordingObserver kana;
		(void)th03::detail::gameplay_frame_prefix(
			kana_state, th03::GameContent{}, &kana);
		check(kana.bullet_spawns ==
				  std::vector<th03::Bullet16Spawned>{
					  {
						  .slot = static_cast<th03::BulletSlot>(0U),
						  .source_player = th03::PlayerId::p1,
						  .construction =
							  th03::Bullet16ConstructionKind::kana_extra_attack,
						  .source_template_animation_enabled = true,
					  },
				  },
			  "Kana Extra successful allocation reports its custom animated "
			  "construction");
		kana_state.bullets[0U].flag = th03::detail::BulletFlag::free;
		kana_attack.state = 1U;
		kana_attack.frame = 0U;
		kana_attack.center_x = 100 << 4;
		kana_attack.center_y = 100 << 4;
		kana_attack.speed = 0U;
		(void)th03::detail::gameplay_frame_prefix(
			kana_state, th03::GameContent{}, &kana);
		check(kana.bullet_spawns.size() == 2U &&
				  kana.bullet_spawns[1U].slot ==
					  static_cast<th03::BulletSlot>(0U),
			  "reusing a freed bullet16 slot emits a second presenter-style "
			  "reset fact");
	});
	with_live_state([](auto &clipped) {
		clipped.hud_start_flag = 1U;
		clipped.players[0U].paletted_character = 11;
		auto &clipped_attack = clipped.extra_attacks[0U][0U];
		clipped_attack.state = 1U;
		clipped_attack.frame = 0U;
		clipped_attack.center_x = 100 << 4;
		clipped_attack.center_y = 500 << 4;
		RecordingObserver clipped_recording;
		(void)th03::detail::gameplay_frame_prefix(
			clipped, th03::GameContent{}, &clipped_recording);
		check(clipped_recording.bullet_spawns.empty(),
			  "a clipped Kana custom attempt emits no unsuccessful allocation "
			  "fact");
	});

	with_live_state([&](auto &partial) {
		configure_mima_gauge(partial);
		for (std::size_t slot = 0U; slot < 317U; ++slot) {
			partial.bullets[slot].flag = th03::detail::BulletFlag::pellet;
			partial.bullets[slot].center_x = 144 << 4;
			partial.bullets[slot].center_y = 100 << 4;
		}
		RecordingObserver partial_recording;
		(void)th03::detail::gameplay_frame_prefix(
			partial, th03::GameContent{}, &partial_recording);
		check(partial_recording.bullet_spawns.size() == 3U &&
				  partial_recording.bullet_spawns[0U].slot ==
					  static_cast<th03::BulletSlot>(317U) &&
				  partial_recording.bullet_spawns[2U].slot ==
					  static_cast<th03::BulletSlot>(319U),
			  "a partially full arena reports only successful first-free "
			  "allocations before exhaustion");
	});

	with_live_state([&](auto &full) {
		configure_mima_gauge(full);
		for (auto &bullet : full.bullets) {
			bullet.flag = th03::detail::BulletFlag::pellet;
			bullet.center_x = 144 << 4;
			bullet.center_y = 100 << 4;
		}
		RecordingObserver full_recording;
		(void)th03::detail::gameplay_frame_prefix(
			full, th03::GameContent{}, &full_recording);
		check(full_recording.bullet_spawns.empty(),
			  "a full bullet arena emits no unsuccessful Bullet16 allocation "
			  "fact");
	});
}

void test_post_hittest_fireball_view_and_terminal_order()
{
	auto state = make_live_state();
	constexpr std::int16_t center_x = 100 << 4;
	constexpr std::int16_t center_y = 120 << 4;
	auto &shot = state.shotpairs[0U];
	shot.alive = true;
	shot.topleft_x = center_x - (16 << 4);
	shot.topleft_y = center_y - (5 << 4);
	shot.player_id = 0U;
	auto &target = state.fireballs[23U];
	target.flag = th03::detail::FireballFlag::fall;
	target.center_x = center_x;
	target.center_y = center_y;
	target.hp = 0U;
	target.player_id = 0U;
	target.variant_as_explosion_target = 2U;
	state.session_exit_requested = true;

	RecordingObserver recording;
	th03::detail::gameplay_frame_suffix_after_interstitial(state, &recording);
	check(
		recording.shots[0U].alive,
		"ShotsEnemies observes a shot before the later fireball hittest consumes it"
	);
	check(
		recording.fireballs[23U].classification() ==
			th03::FireballClassification::free,
		"Fireballs observes the destroyed target after fireballs_hittest"
	);
	check(
		recording.fireballs[0U].classification() ==
			th03::FireballClassification::transfer &&
		recording.fireballs[0U].frame == 0U &&
		recording.fireballs[0U].variant == th03::FireballVariant::red,
		"Fireballs includes the age-0 transferred replacement in its first free slot"
	);
	check(
		state.round_complete &&
		recording.boss_views.size() == 1U &&
		recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
			ObservedPhase::extra_p1,
			ObservedPhase::charge_p1,
			ObservedPhase::extra_p2,
			ObservedPhase::charge_p2,
			ObservedPhase::players_bullets,
		},
		"all phase callbacks precede terminal session-exit completion"
	);
	check(
		recording.main_frame_footers ==
			std::vector<th03::MainFrameFooterReached>{{}},
		"a terminal session exit requested after normal updates still reaches the footer"
	);

	with_live_state([](auto &terminal) {
		terminal.defeat_flag = th03::detail::DefeatFlag::banner;
		terminal.players[0U].rounds_won = 2U;
		terminal.result_transition_complete = true;
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			terminal, &recording
		);
		check(
			terminal.round_complete && recording.main_frame_footers.empty() &&
			recording.boss_views.size() == 1U &&
			recording.phases == std::vector<ObservedPhase>{
				ObservedPhase::shots_enemies,
				ObservedPhase::fireballs,
				ObservedPhase::extra_p1,
				ObservedPhase::charge_p1,
				ObservedPhase::extra_p2,
				ObservedPhase::charge_p2,
				ObservedPhase::players_bullets,
			},
			"a result-control terminal early return skips the normal frame footer"
		);
	});
}

th03::GameInit base_game_init()
{
	th03::GameInit init{};
	init.game_mode = th03::game_mode::vs_1p_2p;
	init.paletted_character = {1, 3};
	return init;
}

std::uint16_t reference_irand(std::uint32_t &seed) noexcept
{
	seed = (seed * 0x015A4E35U) + 1U;
	return static_cast<std::uint16_t>((seed >> 16U) & 0x7FFFU);
}

void test_main_hud_start_observation()
{
	auto base = make_live_state();
	base.game_mode = th03::game_mode::story;
	base.story_stage = 7U;
	base.round_id = 9U;
	base.hud_start_flag = 2U;
	base.hud_start_state = 2U;
	base.hud_start_frame = 87U;
	base.random_seed = 0x12345678U;
	base.randring[0U] = 0x34U;
	base.randring[1U] = 0x12U;
	base.randring[2U] = 0x78U;
	base.randring[3U] = 0x56U;

	auto null_state = base;
	auto discard_state = base;
	auto record_state = base;
	DiscardObserver discard;
	MainHudRecordingObserver recording(&record_state);
	check(
		th03::detail::gameplay_frame_prefix(
			null_state, th03::GameContent{}, nullptr
		) &&
		th03::detail::gameplay_frame_prefix(
			discard_state, th03::GameContent{}, &discard
		) &&
		th03::detail::gameplay_frame_prefix(
			record_state, th03::GameContent{}, &recording
		),
		"HUD-start particle prefix fixtures complete"
	);
	check(
		observed_gameplay_equal(null_state, discard_state) &&
		observed_gameplay_equal(null_state, record_state),
		"HUD-start observation preserves gameplay, shared-ring, and IRAND state"
	);
	check(
		recording.hud_start_steps ==
			std::vector<th03::HudStartFrameStep>{{
				.initialize = false,
				.story_mode = true,
				.story_stage = 7U,
				.round_id = 9U,
				.state_before = 2U,
				.frame_before = 87U,
			}} &&
		record_state.hud_start_state == 3U &&
		record_state.hud_start_frame == 0U,
		"HUD-start step exposes the pre-update state and state 2 advances after reads"
	);
	check(
		recording.hud_start_reads.size() == 512U &&
		std::count_if(
			recording.hud_start_reads.begin(), recording.hud_start_reads.end(),
			[](const auto &read) {
				return read.site ==
					th03::HudStartCompatibilityReadSite::particle_y;
			}
		) == 128 &&
		std::count_if(
			recording.hud_start_reads.begin(), recording.hud_start_reads.end(),
			[](const auto &read) {
				return read.site ==
					th03::HudStartCompatibilityReadSite::particle_angle;
			}
		) == 128 &&
		std::count_if(
			recording.hud_start_reads.begin(), recording.hud_start_reads.end(),
			[](const auto &read) {
				return read.site ==
					th03::HudStartCompatibilityReadSite::angle_delta_first;
			}
		) == 128 &&
		std::count_if(
			recording.hud_start_reads.begin(), recording.hud_start_reads.end(),
			[](const auto &read) {
				return read.site ==
					th03::HudStartCompatibilityReadSite::angle_delta_second;
			}
		) == 128,
		"HUD-start state 2 exposes 128 reads at each of the four compatibility sites"
	);
	auto expected_seed = base.random_seed;
	const auto first_delta = reference_irand(expected_seed);
	const auto second_delta = reference_irand(expected_seed);
	check(
		recording.hud_start_reads[0U] == th03::HudStartCompatibilityRead{
			.site = th03::HudStartCompatibilityReadSite::particle_y,
			.particle_ordinal = 0U,
			.raw_value = 0x1234U,
		} &&
		recording.hud_start_reads[1U] == th03::HudStartCompatibilityRead{
			.site = th03::HudStartCompatibilityReadSite::particle_angle,
			.particle_ordinal = 0U,
			.raw_value = 0x7812U,
		} &&
		recording.hud_start_reads[2U] == th03::HudStartCompatibilityRead{
			.site = th03::HudStartCompatibilityReadSite::angle_delta_first,
			.particle_ordinal = 0U,
			.raw_value = first_delta,
		} &&
		recording.hud_start_reads[3U] == th03::HudStartCompatibilityRead{
			.site = th03::HudStartCompatibilityReadSite::angle_delta_second,
			.particle_ordinal = 0U,
			.raw_value = second_delta,
		} &&
		recording.hud_start_reads[4U].particle_ordinal == 1U &&
		recording.hud_start_reads[508U].particle_ordinal == 127U &&
		recording.hud_start_reads[511U].site ==
			th03::HudStartCompatibilityReadSite::angle_delta_second,
		"HUD-start raw reads retain ring-Y, ring-angle, first-delta, second-delta order"
	);
	check(
		recording.calls.front() == MainHudObservedCall::hud_start_step &&
		std::all_of(
			recording.calls.begin() + 1, recording.calls.end(),
			[](MainHudObservedCall call) {
				return call == MainHudObservedCall::hud_start_read;
			}
		),
		"HUD-start step precedes all 512 synchronous raw-read facts"
	);

	auto initializing = make_live_state();
	initializing.hud_start_flag = 1U;
	initializing.hud_start_state = 4U;
	initializing.hud_start_frame = 55U;
	MainHudRecordingObserver initialized;
	(void)th03::detail::gameplay_frame_prefix(
		initializing, th03::GameContent{}, &initialized
	);
	check(
		initialized.hud_start_steps ==
			std::vector<th03::HudStartFrameStep>{{
				.initialize = true,
				.story_mode = false,
				.story_stage = 0U,
				.round_id = 0U,
				.state_before = 0U,
				.frame_before = 0U,
			}} &&
		initializing.hud_start_state == 0U &&
		initializing.hud_start_frame == 1U,
		"HUD-start INIT reset is observed before the selected state-0 update"
	);

	MainHudRecordingObserver raw_passthrough;
	raw_passthrough.hud_start_compatibility_read(
		th03::HudStartCompatibilityRead{
			.site = th03::HudStartCompatibilityReadSite::angle_delta_first,
			.particle_ordinal = 127U,
			.raw_value = 0x8001U,
		}
	);
	check(
		raw_passthrough.hud_start_reads ==
			std::vector<th03::HudStartCompatibilityRead>{{
				.site = th03::HudStartCompatibilityReadSite::angle_delta_first,
				.particle_ordinal = 127U,
				.raw_value = 0x8001U,
			}},
		"HUD-start raw facts preserve high-bit values for presenter-owned signed IDIV semantics"
	);
}

void test_main_hud_result_late_order_and_terminal_split()
{
	auto base = make_live_state();
	base.game_mode = th03::game_mode::story;
	base.story_stage = 5U;
	base.story_lives = 2U;
	base.extends_gained = 0U;
	base.round_id = 6U;
	base.round_or_result_frame = 77U;
	base.playfield_fg_shift_x = {-5, 7};
	base.score[0U] = {1U, 2U, 3U, 4U, 5U, 6U, 1U, 8U};
	base.score[1U] = {8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U};
	base.warning_flag[0U] = th03::detail::WarningFlag::portrait;
	base.hud_start_state = 3U;
	base.hud_start_frame = 31U;
	base.combos[0U] = {.time = 1U, .hits_highest = 9U, .bonus_total = 12U};
	base.combos[1U] = {.time = 2U, .hits_highest = 8U, .bonus_total = 34U};
	base.result_phase = th03::detail::ResultPhase::opening;
	base.result_phase_frame = 15U;

	auto null_state = base;
	auto discard_state = base;
	auto record_state = base;
	DiscardObserver discard;
	MainHudRecordingObserver recording(&record_state);
	th03::detail::gameplay_frame_suffix_after_interstitial(null_state, nullptr);
	th03::detail::gameplay_frame_suffix_after_interstitial(
		discard_state, &discard
	);
	th03::detail::gameplay_frame_suffix_after_interstitial(
		record_state, &recording
	);
	check(
		observed_gameplay_equal(null_state, discard_state) &&
		observed_gameplay_equal(null_state, record_state),
		"late HUD/result observations preserve state and RNG for null, discard, and recording paths"
	);
	check(
		recording.calls == std::vector<MainHudObservedCall>{
			MainHudObservedCall::players_bullets,
			MainHudObservedCall::result,
			MainHudObservedCall::gauge,
			MainHudObservedCall::score,
			MainHudObservedCall::story_lives,
			MainHudObservedCall::warning_portrait,
			MainHudObservedCall::hud_start,
			MainHudObservedCall::combo,
			MainHudObservedCall::result_transition,
			MainHudObservedCall::footer,
		},
		"late HUD/result facts retain source draw and mutation order"
	);
	const auto context = th03::MainPhaseContext{
		.round_id = 6U,
		.round_or_result_frame = 77U,
		.playfield_fg_shift_x = {-5, 7},
	};
	check(
		recording.results == std::vector<th03::MainResultState>{{
			.context = context,
			.defeated_player = std::nullopt,
			.winner = std::nullopt,
			.summary_active = false,
			.story_stage = 5U,
			.story_lives = 2U,
			.rounds_won = {0U, 0U},
		}} &&
		recording.scores == std::vector<th03::MainScoreState>{{
			.context = context,
			.digits = base.score,
		}} &&
		recording.story_lives_at_score == std::vector<std::uint8_t>{2U} &&
		recording.story_lives ==
			std::vector<th03::MainHudStoryLivesChanged>{{.value_after = 3U}} &&
		recording.extends_at_lives == std::vector<std::uint8_t>{0U} &&
		record_state.story_lives == 3U && record_state.extends_gained == 1U,
		"score is observed before the Story life increment and life TRAM fact precedes extend accounting"
	);
	check(
		recording.hud_start_states ==
			std::vector<th03::MainHudStartState>{{
				.round_id = 6U,
				.story_mode = true,
				.story_stage = 5U,
				.state_after_prefix = 3U,
				.frame_after_prefix = 31U,
			}} &&
		recording.combos == std::vector<th03::MainComboState>{{
			.context = context,
			.players = {
				th03::MainComboPlayerState{1U, 9U, 12U},
				th03::MainComboPlayerState{2U, 8U, 34U},
			},
		}} &&
		record_state.combos[0U].time == 0U &&
		record_state.combos[0U].hits_highest == 0U &&
		record_state.combos[0U].bonus_total == 0U &&
		record_state.combos[1U].time == 1U,
		"combo fact exposes both players before P1-then-P2 decrement and settlement"
	);
	check(
		recording.result_transitions ==
			std::vector<th03::MainResultTransitionState>{{
				.context = context,
				.phase = th03::MainResultTransitionPhase::opening,
				.frame_before = 15U,
			}} &&
		record_state.result_transition_complete &&
		record_state.result_phase == th03::detail::ResultPhase::closing &&
		record_state.result_phase_frame == 0U,
		"result-transition fact precedes the opening frame-15 completion mutation"
	);

	auto terminal = make_live_state();
	terminal.defeat_flag = th03::detail::DefeatFlag::banner;
	terminal.last_fatal_player = 1U;
	terminal.winner = 0;
	terminal.players[0U].rounds_won = 2U;
	terminal.players[0U].combo_hits_max = 1U;
	terminal.round_or_result_frame = 1U;
	terminal.result_transition_complete = true;
	MainHudRecordingObserver terminal_recording(&terminal);
	th03::detail::gameplay_frame_suffix_after_interstitial(
		terminal, &terminal_recording
	);
	check(
		terminal_recording.calls == std::vector<MainHudObservedCall>{
			MainHudObservedCall::players_bullets,
			MainHudObservedCall::result,
		} &&
		terminal.round_complete && terminal.route == 2U,
		"terminal result frame emits its pre-control result view and cuts every later phase"
	);
	check(
		terminal_recording.results == std::vector<th03::MainResultState>{{
			.context = th03::MainPhaseContext{
				.round_id = 0U,
				.round_or_result_frame = 1U,
				.playfield_fg_shift_x = {0, 0},
			},
			.defeated_player = th03::PlayerId::p2,
			.winner = th03::PlayerId::p1,
			.summary_active = true,
			.story_stage = 0U,
			.story_lives = 0U,
			.rounds_won = {2U, 0U},
			.winner_stats = th03::MainResultWinnerStats{
				.combo_hits_max = 1U,
			},
		}} &&
		terminal_recording.result_bonus_remaining_at_view ==
			std::vector<std::uint32_t>{0U} &&
		terminal_recording.score_at_result_view[0U][0U][0U] == 0U &&
		terminal.result_bonus_per_frame == 5U &&
		terminal.result_bonus_remaining == 995U &&
		terminal.score[0U][0U] == 5U,
		"result view captures winner statistics before frame-1 bonus initialization and mutation"
	);

	auto exact_final_stage = make_live_state();
	exact_final_stage.game_mode = th03::game_mode::story;
	exact_final_stage.story_stage = 8U;
	exact_final_stage.defeat_flag = th03::detail::DefeatFlag::banner;
	exact_final_stage.last_fatal_player = 1U;
	exact_final_stage.winner = 0;
	MainHudRecordingObserver exact_final_recording;
	th03::detail::gameplay_frame_suffix_after_interstitial(
		exact_final_stage, &exact_final_recording
	);
	auto synthetic_stage = make_live_state();
	synthetic_stage.game_mode = th03::game_mode::story;
	synthetic_stage.story_stage = 9U;
	synthetic_stage.defeat_flag = th03::detail::DefeatFlag::banner;
	synthetic_stage.last_fatal_player = 1U;
	synthetic_stage.winner = 0;
	MainHudRecordingObserver synthetic_recording;
	th03::detail::gameplay_frame_suffix_after_interstitial(
		synthetic_stage, &synthetic_recording
	);
	check(
		exact_final_recording.results.size() == 1U &&
		exact_final_recording.results[0U].summary_active &&
		exact_final_recording.results[0U].story_stage == 8U &&
		synthetic_recording.results.size() == 1U &&
		synthetic_recording.results[0U].summary_active &&
		synthetic_recording.results[0U].story_stage == 9U,
		"result fact distinguishes the exact final stage from synthetic post-clear stages"
	);

	auto corrupt_player = make_live_state();
	corrupt_player.defeat_flag = th03::detail::DefeatFlag::banner;
	corrupt_player.last_fatal_player = 3U;
	MainHudRecordingObserver corrupt_recording;
	th03::detail::gameplay_frame_suffix_after_interstitial(
		corrupt_player, &corrupt_recording
	);
	check(
		corrupt_recording.results.size() == 1U &&
		!corrupt_recording.results[0U].defeated_player &&
		!corrupt_recording.results[0U].winner,
		"result fact closes invalid defeated-player and winner domains"
	);
}

void test_main_hud_mutation_sites()
{
	auto bomb = make_live_state();
	bomb.players[0U].bombs = 2U;
	bomb.skill = 3U;
	bomb.input_player[0U] = 0x0010U;
	MainHudRecordingObserver bomb_recording(&bomb);
	(void)th03::detail::gameplay_frame_suffix_before_interstitial(
		bomb, &bomb_recording
	);
	check(
		bomb_recording.player_values ==
			std::vector<th03::MainHudPlayerValueChanged>{{
				.player = th03::PlayerId::p1,
				.kind = th03::MainHudPlayerValueKind::bombs,
				.value_after = 1U,
			}} &&
		bomb_recording.player_value_snapshots[0U].invincibility_time == 180U &&
		bomb_recording.player_value_snapshots[0U].skill == 3U &&
		bomb.skill == 2U,
		"Bomb HUD fact follows invincibility assignment and precedes P1 skill decrement"
	);

	auto hit = make_live_state();
	hit.players[0U].hit_damage_next = 3U;
	for(auto &column : hit.collision_map[0U]) {
		column.fill(true);
	}
	MainHudRecordingObserver hit_recording(&hit);
	(void)th03::detail::gameplay_frame_suffix_before_interstitial(
		hit, &hit_recording
	);
	check(
		hit_recording.player_values ==
			std::vector<th03::MainHudPlayerValueChanged>{{
				.player = th03::PlayerId::p1,
				.kind = th03::MainHudPlayerValueKind::halfhearts,
				.value_after = 7U,
			}} &&
		hit_recording.calls[0U] == MainHudObservedCall::hit_circle &&
		hit_recording.calls[1U] == MainHudObservedCall::player_value &&
		hit_recording.player_value_snapshots[0U].knockback_active,
		"halfheart HUD fact follows hit-circle allocation and knockback setup"
	);

	auto fatal = make_live_state();
	fatal.players[0U].halfhearts = 1U;
	fatal.players[0U].hit_damage_next = 3U;
	for(auto &column : fatal.collision_map[0U]) {
		column.fill(true);
	}
	MainHudRecordingObserver fatal_recording(&fatal);
	(void)th03::detail::gameplay_frame_suffix_before_interstitial(
		fatal, &fatal_recording
	);
	check(
		fatal_recording.player_values[0U].value_after == 0U &&
		fatal_recording.player_value_snapshots[0U].lose_anim_time == 0x30U &&
		!fatal_recording.player_value_snapshots[0U].knockback_active,
		"fatal halfheart HUD fact follows defeat-timer setup rather than fabricating knockback"
	);

	auto round = make_live_state();
	round.round_or_result_frame = 0x1234U;
	round.players[0U].lose_anim_time = 1U;
	MainHudRecordingObserver round_recording(&round);
	(void)th03::detail::gameplay_frame_suffix_before_interstitial(
		round, &round_recording
	);
	check(
		round_recording.player_values ==
			std::vector<th03::MainHudPlayerValueChanged>{{
				.player = th03::PlayerId::p2,
				.kind = th03::MainHudPlayerValueKind::rounds_won,
				.value_after = 1U,
			}} &&
		round_recording.calls[0U] == MainHudObservedCall::player_defeat_step &&
		round_recording.calls[1U] == MainHudObservedCall::player_value &&
		round_recording.player_value_snapshots[0U].defeat_flag == 2U &&
		round_recording.player_value_snapshots[0U].winner == 1 &&
		round_recording.player_value_snapshots[0U].round_or_result_frame == 0U,
		"round-win HUD fact follows banner, winner, and result-frame reset mutations"
	);
}

void test_main_hud_deferred_and_repeat_lifecycle()
{
	auto init = base_game_init();
	auto game = th03::Game::create(
		init,
		{},
		th03::GameOptions{
			.input_protocol = th03::InputProtocol::recorded_replay,
		}
	);
	check(game.has_value(), "HUD/result deferred fixture creates");
	if(game) {
		MainHudRecordingObserver recording;
		const auto deferred = game->step(
			th03::CoreInput{.system = 0x1000U},
			th03::InputContext::gameplay,
			&recording
		);
		check(
			deferred.status == th03::StepStatus::phase_transition &&
			recording.calls == std::vector<MainHudObservedCall>{
				MainHudObservedCall::hud_start_step,
			} &&
			recording.hud_start_steps[0U].initialize,
			"recorded pause emits the HUD-start prefix step but defers every late HUD/result fact"
		);
		const auto completed = game->finish_deferred_frame(
			th03::DeferredFrameAction::resume, &recording
		);
		check(
			completed && completed->status == th03::StepStatus::running &&
			recording.calls == std::vector<MainHudObservedCall>{
				MainHudObservedCall::hud_start_step,
				MainHudObservedCall::players_bullets,
				MainHudObservedCall::result,
				MainHudObservedCall::gauge,
				MainHudObservedCall::score,
				MainHudObservedCall::hud_start,
				MainHudObservedCall::combo,
				MainHudObservedCall::result_transition,
				MainHudObservedCall::footer,
			},
			"deferred completion emits the late HUD/result suffix exactly once without repeating prefix"
		);
		const auto call_count = recording.calls.size();
		check(
			!game->finish_deferred_frame(
				th03::DeferredFrameAction::resume, &recording
			) && recording.calls.size() == call_count,
			"invalid repeated deferred completion emits no HUD/result facts"
		);
	}

	auto repeat = make_live_state();
	repeat.round_id = 4U;
	repeat.round_complete = true;
	repeat.route = 1U;
	th03::detail::initialize_repeat_round(repeat, init, 0U);
	MainHudRecordingObserver repeat_recording;
	th03::detail::gameplay_frame_suffix_after_interstitial(
		repeat, &repeat_recording
	);
	check(
		repeat_recording.result_transitions ==
			std::vector<th03::MainResultTransitionState>{{
				.context = th03::MainPhaseContext{
					.round_id = 5U,
					.round_or_result_frame = 0U,
					.playfield_fg_shift_x = {0, 0},
				},
				.phase = th03::MainResultTransitionPhase::closing,
				.frame_before = 0U,
			}} &&
		repeat.result_phase == th03::detail::ResultPhase::closing &&
		repeat.result_phase_frame == 1U,
		"first repeated-round frame exposes the retained closing transition before advancing it"
	);

	auto result_selected = make_live_state();
	result_selected.defeat_flag = th03::detail::DefeatFlag::banner;
	result_selected.last_fatal_player = 0U;
	result_selected.winner = 1;
	result_selected.round_or_result_frame = 160U;
	result_selected.result_phase = th03::detail::ResultPhase::idle;
	result_selected.result_phase_frame = 7U;
	MainHudRecordingObserver result_selected_recording;
	th03::detail::gameplay_frame_suffix_after_interstitial(
		result_selected, &result_selected_recording
	);
	check(
		result_selected_recording.result_transitions ==
			std::vector<th03::MainResultTransitionState>{{
				.context = th03::MainPhaseContext{
					.round_id = 0U,
					.round_or_result_frame = 160U,
					.playfield_fg_shift_x = {0, 0},
				},
				.phase = th03::MainResultTransitionPhase::opening,
				.frame_before = 7U,
			}} &&
		result_selected.result_phase == th03::detail::ResultPhase::opening &&
		result_selected.result_phase_frame == 8U,
		"result control selects opening before the same frame's transition fact without resetting its phase byte"
	);

	auto demo_selected = make_live_state();
	demo_selected.game_mode = th03::game_mode::demo;
	demo_selected.demo_number = 1U;
	demo_selected.demo_frame = 7000U;
	demo_selected.result_phase = th03::detail::ResultPhase::idle;
	demo_selected.result_phase_frame = 9U;
	MainHudRecordingObserver demo_first;
	th03::detail::gameplay_frame_suffix_after_interstitial(
		demo_selected, &demo_first
	);
	check(
		demo_first.result_transitions[0U].phase ==
			th03::MainResultTransitionPhase::idle &&
		demo_first.result_transitions[0U].frame_before == 9U &&
		demo_selected.result_phase == th03::detail::ResultPhase::opening &&
		demo_selected.result_phase_frame == 9U,
		"demo control selects opening after the current frame's transition fact and preserves its phase byte"
	);
	MainHudRecordingObserver demo_second;
	th03::detail::gameplay_frame_suffix_after_interstitial(
		demo_selected, &demo_second
	);
	check(
		demo_second.result_transitions[0U].phase ==
			th03::MainResultTransitionPhase::opening &&
		demo_second.result_transitions[0U].frame_before == 9U &&
		demo_selected.result_phase_frame == 10U,
		"demo-selected opening is first exposed on the following frame"
	);
}

std::vector<th03::MainBackgroundRandomRead> expected_fresh_background_reads(
	std::uint32_t seed,
	std::size_t count
)
{
	// Fresh round startup fills the 256-byte shared ring and both 64-entry CPU
	// charge rings before invoking D092/D2E8. This fixture deliberately uses no
	// enemy formations, so no formation RNG lies between those consumers.
	for(std::size_t i = 0U; i < 256U + (2U * 64U); ++i) {
		(void)reference_irand(seed);
	}
	std::vector<th03::MainBackgroundRandomRead> reads;
	reads.reserve(count);
	for(std::size_t i = 0U; i < count; ++i) {
		reads.push_back(th03::MainBackgroundRandomRead{
			.raw_value = reference_irand(seed),
		});
	}
	return reads;
}

std::vector<th03::MainBackgroundRandomRead> expected_background_reads(
	std::uint32_t seed,
	std::size_t count
)
{
	std::vector<th03::MainBackgroundRandomRead> reads;
	reads.reserve(count);
	for(std::size_t i = 0U; i < count; ++i) {
		reads.push_back(th03::MainBackgroundRandomRead{
			.raw_value = reference_irand(seed),
		});
	}
	return reads;
}

void test_main_background_creation_observation()
{
	auto type_a_init = base_game_init();
	type_a_init.resident_rand = 0x12345678;
	BackgroundRecordingObserver type_a_recording;
	DiscardObserver discard;
	auto type_a_null = th03::Game::create(type_a_init);
	auto type_a_discard = th03::Game::create(
		type_a_init, {}, {}, &discard
	);
	auto type_a_record = th03::Game::create(
		type_a_init, {}, {}, &type_a_recording
	);
	check(
		type_a_null && type_a_discard && type_a_record,
		"fresh Type A background observer fixtures create"
	);
	if(type_a_null && type_a_discard && type_a_record) {
		check(
			canonical_equal(
				type_a_null->canonical_state(),
				type_a_discard->canonical_state()
			) && canonical_equal(
				type_a_null->canonical_state(),
				type_a_record->canonical_state()
			),
			"fresh background observation does not change state or RNG"
		);
	}
	check(
		type_a_recording.initializations ==
			std::vector<th03::MainBackgroundInitialize>{
				{.kind = th03::MainBackgroundKind::type_a},
			} &&
		type_a_recording.frames.empty() &&
		type_a_recording.random_reads == expected_fresh_background_reads(
			static_cast<std::uint32_t>(type_a_init.resident_rand), 48U * 4U
		),
		"fresh Type A creation reports the exact D092 read stream"
	);
	check(
		type_a_recording.calls.size() == 1U + (48U * 4U) &&
		type_a_recording.calls.front() ==
			BackgroundObservedCall::initialize &&
		std::all_of(
			type_a_recording.calls.begin() + 1,
			type_a_recording.calls.end(),
			[](BackgroundObservedCall call) {
				return call == BackgroundObservedCall::random_read;
			}
		),
		"fresh Type A Initialize precedes all 192 source-ordered reads"
	);

	auto type_b_init = type_a_init;
	// Paletted Marisa selects the D2E8/D340/D3F9 family.
	type_b_init.paletted_character[1] = 5;
	BackgroundRecordingObserver type_b_recording;
	auto type_b_game = th03::Game::create(
		type_b_init, {}, {}, &type_b_recording
	);
	check(type_b_game.has_value(), "fresh Type B background fixture creates");
	check(
		type_b_recording.initializations ==
			std::vector<th03::MainBackgroundInitialize>{
				{.kind = th03::MainBackgroundKind::type_b},
			} &&
		type_b_recording.random_reads == expected_fresh_background_reads(
			static_cast<std::uint32_t>(type_b_init.resident_rand), 50U * 2U
		) &&
		type_b_recording.calls.size() == 1U + (50U * 2U) &&
		type_b_recording.calls.front() ==
			BackgroundObservedCall::initialize,
		"fresh Type B creation reports Initialize then the exact 100 D2E8 reads"
	);
}

void test_main_background_snapshot_observation()
{
	auto full_init = base_game_init();
	full_init.runtime_snapshot = th03::RuntimeSnapshot{};
	full_init.round_start_snapshot = th03::RoundStartSnapshot{};
	full_init.round_start_snapshot->state.round_id = 4U;
	full_init.round_start_snapshot->carry.background_phase = 1U;
	BackgroundRecordingObserver full_recording;
	auto full_game = th03::Game::create(
		full_init, {}, {}, &full_recording
	);
	check(full_game.has_value(), "full-snapshot background fixture creates");
	check(
		full_recording.calls.empty() &&
		full_recording.initializations.empty() &&
		full_recording.random_reads.empty(),
		"full snapshot does not fabricate bypassed background initialization facts"
	);
	if(full_game) {
		(void)full_game->step(
			{}, th03::InputContext::gameplay, &full_recording
		);
		check(
			full_recording.frames == std::vector<th03::MainBackgroundFrame>{
				{
					.round_id = 4U,
					.round_or_result_frame = 0U,
					.kind = th03::MainBackgroundKind::type_a,
					.phase = th03::MainBackgroundPhase::steady,
				},
			} && full_recording.random_reads.empty(),
			"full snapshot still reports its first honest steady background frame"
		);
	}

	auto compact_init = base_game_init();
	compact_init.paletted_character[1] = 5;
	compact_init.compact_runtime_snapshot = th03::CompactRuntimeSnapshot{};
	compact_init.compact_runtime_snapshot->round_reset_seed = 0x10203040U;
	compact_init.compact_runtime_snapshot->random_seed = 0x50607080U;
	auto reconstruction_seed =
		compact_init.compact_runtime_snapshot->round_reset_seed;
	for(std::size_t i = 0U; i < 256U + (2U * 64U); ++i) {
		(void)reference_irand(reconstruction_seed);
	}
	compact_init.compact_runtime_snapshot->first_formation =
		static_cast<std::uint8_t>(reference_irand(reconstruction_seed) % 2U);
	compact_init.round_start_snapshot = th03::RoundStartSnapshot{};
	compact_init.round_start_snapshot->state.round_id = 7U;
	compact_init.round_start_snapshot->carry.background_phase = 2U;
	th03::GameContent content{};
	content.enemy_formations.resize(2U);
	BackgroundRecordingObserver compact_recording;
	auto compact_game = th03::Game::create(
		compact_init, std::move(content), {}, &compact_recording
	);
	check(compact_game.has_value(), "compact-snapshot background fixture creates");
	check(
		compact_recording.calls.empty() &&
		compact_recording.initializations.empty() &&
		compact_recording.random_reads.empty(),
		"compact reconstruction does not relabel its RNG work as D2E8 reads"
	);
	if(compact_game) {
		(void)compact_game->step(
			{}, th03::InputContext::gameplay, &compact_recording
		);
		check(
			compact_recording.frames == std::vector<th03::MainBackgroundFrame>{
				{
					.round_id = 7U,
					.round_or_result_frame = 0U,
					.kind = th03::MainBackgroundKind::type_b,
					.phase = th03::MainBackgroundPhase::steady,
				},
			} && compact_recording.random_reads.empty(),
			"compact snapshot still reports its first honest steady background frame"
		);
	}
}

void test_main_background_frame_schedule_and_repeat()
{
	auto type_a_before_transition = make_live_state();
	type_a_before_transition.round_frame = 128U;
	type_a_before_transition.round_or_result_frame = 128U;
	BackgroundRecordingObserver type_a_before_recording;
	th03::detail::gameplay_frame_suffix_after_interstitial(
		type_a_before_transition, &type_a_before_recording
	);
	check(
		type_a_before_recording.frames ==
			std::vector<th03::MainBackgroundFrame>{
				{
					.round_id = 0U,
					.round_or_result_frame = 128U,
					.kind = th03::MainBackgroundKind::type_a,
					.phase = th03::MainBackgroundPhase::initial,
				},
			} &&
		type_a_before_recording.random_reads.size() == 48U &&
		type_a_before_transition.background_phase ==
			th03::detail::BackgroundPhase::initial,
		"D135 frame 128 consumes 48 reads and keeps the initial callback"
	);

	auto type_a = make_live_state();
	type_a.round_id = 2U;
	type_a.round_frame = 129U;
	type_a.round_or_result_frame = 129U;
	type_a.random_seed = 0x89ABCDEFU;
	type_a.bomb_flag[0U] = th03::detail::BombFlag::active;
	type_a.bomb_frame[0U] = 32U;
	auto type_a_null = type_a;
	BackgroundRecordingObserver type_a_recording;
	th03::detail::gameplay_frame_suffix_after_interstitial(type_a_null);
	th03::detail::gameplay_frame_suffix_after_interstitial(
		type_a, &type_a_recording
	);
	check(
		observed_gameplay_equal(type_a_null, type_a),
		"initial background observation preserves gameplay and shared RNG"
	);
	check(
		type_a_recording.frames == std::vector<th03::MainBackgroundFrame>{
			{
				.round_id = 2U,
				.round_or_result_frame = 129U,
				.kind = th03::MainBackgroundKind::type_a,
				.phase = th03::MainBackgroundPhase::initial,
			},
		} && type_a_recording.random_reads == expected_background_reads(
			0x89ABCDEFU, 48U
		) &&
		type_a_recording.calls.size() == 50U &&
		type_a_recording.calls.front() == BackgroundObservedCall::frame &&
		type_a_recording.calls.back() ==
			BackgroundObservedCall::bomb_frame_step &&
		std::all_of(
			type_a_recording.calls.begin() + 1,
			type_a_recording.calls.end() - 1,
			[](BackgroundObservedCall call) {
				return call == BackgroundObservedCall::random_read;
			}
		),
		"D135 frame 129 reports Frame and all 48 reads before the first late BombFrameStep"
	);

	type_a.bomb_flag[0U] = th03::detail::BombFlag::inactive;
	BackgroundRecordingObserver steady_recording;
	th03::detail::gameplay_frame_suffix_after_interstitial(
		type_a, &steady_recording
	);
	check(
		steady_recording.frames == std::vector<th03::MainBackgroundFrame>{
			{
				.round_id = 2U,
				.round_or_result_frame = 130U,
				.kind = th03::MainBackgroundKind::type_a,
				.phase = th03::MainBackgroundPhase::steady,
			},
		} && steady_recording.random_reads.empty() &&
		steady_recording.calls == std::vector<BackgroundObservedCall>{
			BackgroundObservedCall::frame,
		},
		"D1E7 steady frames report one Frame and no compatibility reads"
	);

	auto type_b_before_transition = make_live_state();
	type_b_before_transition.background_initial_kind =
		th03::detail::BackgroundInitialKind::type_b;
	type_b_before_transition.round_frame = 144U;
	type_b_before_transition.round_or_result_frame = 144U;
	BackgroundRecordingObserver type_b_before_recording;
	th03::detail::gameplay_frame_suffix_after_interstitial(
		type_b_before_transition, &type_b_before_recording
	);
	check(
		type_b_before_recording.frames ==
			std::vector<th03::MainBackgroundFrame>{
				{
					.round_id = 0U,
					.round_or_result_frame = 144U,
					.kind = th03::MainBackgroundKind::type_b,
					.phase = th03::MainBackgroundPhase::initial,
				},
			} &&
		type_b_before_recording.random_reads.size() == 40U &&
		type_b_before_transition.background_phase ==
			th03::detail::BackgroundPhase::initial,
		"D340 frame 144 consumes 40 reads and keeps the initial callback"
	);

	auto type_b = make_live_state();
	type_b.background_initial_kind =
		th03::detail::BackgroundInitialKind::type_b;
	type_b.round_id = 5U;
	type_b.round_frame = 145U;
	type_b.round_or_result_frame = 145U;
	type_b.random_seed = 0x76543210U;
	BackgroundRecordingObserver type_b_recording;
	th03::detail::gameplay_frame_suffix_after_interstitial(
		type_b, &type_b_recording
	);
	check(
		type_b_recording.frames == std::vector<th03::MainBackgroundFrame>{
			{
				.round_id = 5U,
				.round_or_result_frame = 145U,
				.kind = th03::MainBackgroundKind::type_b,
				.phase = th03::MainBackgroundPhase::initial,
			},
		} && type_b_recording.random_reads == expected_background_reads(
			0x76543210U, 40U
		) &&
		type_b.background_phase ==
			th03::detail::BackgroundPhase::type_b_steady,
		"D340 frame 145 reports Frame before the exact 40-read stream and then becomes steady"
	);

	auto repeat = make_live_state();
	repeat.background_initial_kind =
		th03::detail::BackgroundInitialKind::type_b;
	repeat.background_phase = th03::detail::BackgroundPhase::type_b_steady;
	repeat.round_id = 8U;
	repeat.round_or_result_frame = 777U;
	repeat.round_complete = true;
	repeat.route = 1U;
	auto repeat_init = base_game_init();
	repeat_init.paletted_character[1] = 5;
	th03::detail::initialize_repeat_round(repeat, repeat_init, 0U);
	BackgroundRecordingObserver repeat_recording;
	th03::detail::gameplay_frame_suffix_after_interstitial(
		repeat, &repeat_recording
	);
	check(
		repeat_recording.frames == std::vector<th03::MainBackgroundFrame>{
			{
				.round_id = 9U,
				.round_or_result_frame = 0U,
				.kind = th03::MainBackgroundKind::type_b,
				.phase = th03::MainBackgroundPhase::steady,
			},
		} && repeat_recording.initializations.empty() &&
		repeat_recording.random_reads.empty(),
		"the first post-repeat frame carries the new round ID and retained steady callback without a reset event"
	);
}

void test_recorded_pause_completion()
{
	auto init = base_game_init();
	auto game = th03::Game::create(
		init,
		{},
		th03::GameOptions{
			.input_protocol = th03::InputProtocol::recorded_replay,
		}
	);
	check(game.has_value(), "recorded-pause observer fixture creates");
	if(!game) {
		return;
	}
	RecordingObserver recording;
	const auto deferred = game->step(
		th03::CoreInput{.system = 0x1000U},
		th03::InputContext::gameplay,
		&recording
	);
	check(
		deferred.status == th03::StepStatus::phase_transition &&
		recording.phases.empty() && recording.poses.empty() &&
		recording.main_frame_footers.empty() &&
		recording.boss_views.empty(),
		"recorded Cancel defers player updates, phase views, and the footer"
	);
	const auto completed = game->finish_deferred_frame(
		th03::DeferredFrameAction::resume,
		&recording
	);
	check(
		completed && completed->status == th03::StepStatus::running &&
		recording.poses.size() == 2U &&
		recording.boss_views.size() == 1U &&
		recording.main_frame_footers ==
			std::vector<th03::MainFrameFooterReached>{{}} &&
		recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
			ObservedPhase::extra_p1,
			ObservedPhase::charge_p1,
			ObservedPhase::extra_p2,
			ObservedPhase::charge_p2,
			ObservedPhase::players_bullets,
		},
		"finish_deferred_frame reports all delayed phase views once"
	);
	const auto phase_count = recording.phases.size();
	const auto call_count = recording.calls.size();
	const auto boss_view_count = recording.boss_views.size();
	check(
		!game->finish_deferred_frame(
			th03::DeferredFrameAction::resume,
			&recording
		) && recording.phases.size() == phase_count &&
		recording.calls.size() == call_count &&
		recording.boss_views.size() == boss_view_count,
		"an invalid repeated deferred completion emits no observer callback"
	);

	auto exit_game = th03::Game::create(
		init,
		{},
		th03::GameOptions{
			.input_protocol = th03::InputProtocol::recorded_replay,
		}
	);
	check(exit_game.has_value(), "recorded-pause exit fixture creates");
	if(!exit_game) {
		return;
	}
	RecordingObserver exit_recording;
	(void)exit_game->step(
		th03::CoreInput{.system = 0x1000U},
		th03::InputContext::gameplay,
		&exit_recording
	);
	const auto exited = exit_game->finish_deferred_frame(
		th03::DeferredFrameAction::exit_session,
		&exit_recording
	);
	check(
		exited && exited->status == th03::StepStatus::terminal &&
		exit_recording.boss_views.size() == 1U &&
		exit_recording.main_frame_footers ==
			std::vector<th03::MainFrameFooterReached>{{}} &&
		exit_recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
			ObservedPhase::extra_p1,
			ObservedPhase::charge_p1,
			ObservedPhase::extra_p2,
			ObservedPhase::charge_p2,
			ObservedPhase::players_bullets,
		},
		"deferred exit_session reports all phase views once before terminal"
	);
	const auto exit_phase_count = exit_recording.phases.size();
	const auto exit_call_count = exit_recording.calls.size();
	const auto exit_boss_view_count = exit_recording.boss_views.size();
	check(
		!exit_game->finish_deferred_frame(
			th03::DeferredFrameAction::exit_session,
			&exit_recording
		) && exit_recording.phases.size() == exit_phase_count &&
		exit_recording.calls.size() == exit_call_count &&
		exit_recording.boss_views.size() == exit_boss_view_count,
		"deferred exit_session cannot duplicate observer callbacks"
	);
	const auto already_complete = exit_game->step(
		{}, th03::InputContext::gameplay, &exit_recording
	);
	check(
		already_complete.status == th03::StepStatus::terminal &&
		exit_recording.phases.size() == exit_phase_count &&
		exit_recording.calls.size() == exit_call_count &&
		exit_recording.boss_views.size() == exit_boss_view_count,
		"an already-complete Game emits no observer callback"
	);

	auto invalid_game = th03::Game::create(init);
	check(invalid_game.has_value(), "invalid-context observer fixture creates");
	if(invalid_game) {
		RecordingObserver invalid_recording;
		const auto invalid = invalid_game->step(
			{}, th03::InputContext::interstitial, &invalid_recording
		);
		check(
		invalid.status == th03::StepStatus::invalid_input_context &&
			invalid_recording.phases.empty() &&
			invalid_recording.spawns.empty() &&
			invalid_recording.bullet_spawns.empty() &&
			invalid_recording.poses.empty() &&
			invalid_recording.defeat_steps.empty() &&
			invalid_recording.hit_circles.empty() &&
			invalid_recording.common_effects.empty() &&
			invalid_recording.bomb_compatibility_reads.empty() &&
			invalid_recording.bomb_finishes.empty() &&
			invalid_recording.bomb_frame_steps.empty() &&
			invalid_recording.bomb_common_steps.empty() &&
			invalid_recording.warning_interstitial_starts.empty() &&
			invalid_recording.warning_interstitial_samples.empty() &&
			invalid_recording.warning_lifecycles.empty() &&
			invalid_recording.gauge_attack_finishes.empty() &&
			invalid_recording.warning_portrait_transitions.empty() &&
			invalid_recording.main_gauge_states.empty() &&
			invalid_recording.warning_gauge_calls.empty() &&
			invalid_recording.main_frame_footers.empty() &&
			invalid_recording.boss_activations.empty() &&
			invalid_recording.boss_animation_steps.empty() &&
			invalid_recording.boss_palette_restores.empty() &&
			invalid_recording.boss_views.empty() &&
			invalid_recording.boss_calls.empty() &&
			invalid_recording.calls.empty(),
			"an invalid input context emits no observer callback"
		);
	}
}

void test_recorded_pause_prefix_bullet_once()
{
	auto init = base_game_init();
	init.paletted_character = {3, 1};
	init.runtime_snapshot = th03::RuntimeSnapshot{};
	for(auto &player : init.runtime_snapshot->players) {
		player.center_x = th03::exact::Subpixel::from_integer_wrapping(144);
		player.center_y = th03::exact::Subpixel::from_integer_wrapping(336);
		player.halfhearts = 10U;
		player.gauge_charge_speed = 32U;
		player.gauge_avail = 128U << 4U;
		player.shot_active = 1U;
	}
	init.runtime_snapshot->players[0U].gauge_charged = 192U << 4U;
	init.runtime_snapshot->players[0U].gauge_avail = 192U << 4U;
	init.round_start_snapshot = th03::RoundStartSnapshot{};
	init.round_start_snapshot->carry.shot_cycle[0U] = 4U;
	auto game = th03::Game::create(
		init,
		{},
		th03::GameOptions{
			.input_protocol = th03::InputProtocol::recorded_replay,
		}
	);
	check(game.has_value(), "recorded prefix-Bullet16 pause fixture creates");
	if(!game) {
		return;
	}
	RecordingObserver recording;
	(void)game->step({}, th03::InputContext::gameplay, &recording);
	const auto warning = game->step(
		{}, th03::InputContext::gameplay, &recording
	);
	check(
		warning.status == th03::StepStatus::phase_transition,
		"Mima gauge release enters its warning interstitial"
	);
	for(std::size_t sample = 0U; sample < 27U; ++sample) {
		(void)game->step({}, th03::InputContext::interstitial, &recording);
	}

	bool saw_prefix_spawn = false;
	for(std::size_t frame = 0U; frame < 40U && !saw_prefix_spawn; ++frame) {
		const auto before = recording.bullet_spawns.size();
		const auto paused = game->step(
			th03::CoreInput{.system = 0x1000U},
			th03::InputContext::gameplay,
			&recording
		);
		check(
			paused.status == th03::StepStatus::phase_transition,
			"recorded Cancel defers each probe frame before its gameplay suffix"
		);
		const auto after_initial_call = recording.bullet_spawns.size();
		const auto completed = game->finish_deferred_frame(
			th03::DeferredFrameAction::resume,
			&recording
		);
		check(completed.has_value(), "recorded probe frame resumes");
		if(after_initial_call > before) {
			saw_prefix_spawn = true;
			check(
				recording.bullet_spawns.size() == after_initial_call,
				"finishing a deferred frame does not repeat its prefix Bullet16 facts"
			);
		}
	}
	check(
		saw_prefix_spawn,
		"a Bullet16 fact is delivered on the initial replay-pause call before suffix deferral"
	);
}

void test_warning_completion()
{
	with_live_state([](auto &state) {
		RecordingObserver recording;
		check(
			!th03::detail::gameplay_frame_suffix_before_interstitial(
				state, &recording
			) &&
				recording.warning_interstitial_starts.empty(),
			"a rejected warning interstitial emits no start fact"
		);
		state.warning_flag[0U] = th03::detail::WarningFlag::flash_white;
		state.gba_flag_next = {
			th03::detail::GaugeBossAttackFlag::gauge_pellet_init,
			th03::detail::GaugeBossAttackFlag::boss,
		};
		state.gba_gauge_level = {7U, 13U};
		state.gba_boss_level = 11U;
		check(
			th03::detail::gameplay_frame_suffix_before_interstitial(
				state, &recording
			) &&
				recording.warning_interstitial_starts ==
					std::vector<th03::WarningInterstitialStarted>{
						{
							.flashing_players = {true, false},
							.next_attacks = {
								th03::GaugeBossAttackKind::gauge_pellet,
								th03::GaugeBossAttackKind::boss,
							},
							.gauge_levels = {7U, 13U},
							.boss_level = 11U,
						},
					} &&
				state.warning_flag[0U] ==
					th03::detail::WarningFlag::flash_red,
			"an accepted warning captures flash_white players before conversion"
		);
		check(
			!th03::detail::gameplay_frame_suffix_before_interstitial(
				state, &recording
			) &&
				recording.warning_interstitial_starts.size() == 1U,
			"converted warning flags cannot emit a duplicate start fact"
		);
	});

	auto init = base_game_init();
	init.runtime_snapshot = th03::RuntimeSnapshot{};
	for(auto &player : init.runtime_snapshot->players) {
		player.center_x = th03::exact::Subpixel::from_integer_wrapping(144);
		player.center_y = th03::exact::Subpixel::from_integer_wrapping(336);
		player.halfhearts = 10U;
		player.gauge_charge_speed = 32U;
		player.gauge_avail = 128U << 4U;
		player.shot_active = 1U;
	}
	init.runtime_snapshot->players[0U].gauge_charged = 128U << 4U;
	init.round_start_snapshot = th03::RoundStartSnapshot{};
	init.round_start_snapshot->carry.shot_cycle[0U] = 4U;
	auto game = th03::Game::create(init);
	check(game.has_value(), "warning observer fixture creates");
	if(!game) {
		return;
	}
	RecordingObserver recording;
	const auto release = game->step({}, th03::InputContext::gameplay, &recording);
	check(
		release.status == th03::StepStatus::running &&
		recording.phases.size() == 7U && recording.poses.size() == 2U &&
		recording.boss_views.size() == 1U,
		"gauge release frame reports its ordinary phase views before arming warning"
	);
	recording.phases.clear();
	recording.boss_views.clear();
	recording.main_frame_footers.clear();
	const auto calls_before_warning = recording.calls.size();
	const auto warning = game->step(
		th03::CoreInput{.p1 = 0x0020U},
		th03::InputContext::gameplay,
		&recording
	);
	check(
		warning.status == th03::StepStatus::phase_transition &&
		recording.phases.empty() && recording.spawns.size() == 1U &&
		recording.boss_views.empty() &&
		recording.poses.size() == 4U &&
		recording.warning_interstitial_starts.size() == 1U &&
		recording.warning_interstitial_starts[0U].flashing_players ==
			std::array<bool, 2>{true, false} &&
		recording.main_frame_footers.empty() &&
		std::vector<ObservedCall>(
			recording.calls.begin() +
				static_cast<std::ptrdiff_t>(calls_before_warning),
			recording.calls.end()
		) == std::vector<ObservedCall>{
			ObservedCall::hit_circles_advanced,
			ObservedCall::common_effects_advanced,
			ObservedCall::shot_pair_spawned,
			ObservedCall::player_pose_committed,
			ObservedCall::player_pose_committed,
			ObservedCall::warning_interstitial_started,
		},
		"warning-start captures flash-white players after P1/P2 poses and before deferral"
	);
	for(std::uint8_t sample = 0U; sample < 26U; ++sample) {
		const auto interstitial = game->step(
			{}, th03::InputContext::interstitial, &recording
		);
		check(
			interstitial.status == th03::StepStatus::running &&
			recording.phases.empty() &&
			recording.boss_views.empty() &&
			recording.warning_interstitial_samples.size() ==
				static_cast<std::size_t>(sample) + 1U &&
			recording.warning_interstitial_samples.back().ordinal == sample,
			"each nonfinal warning input latch emits its source ordinal"
		);
	}
	recording.background_bomb_calls.clear();
	recording.warning_gauge_calls.clear();
	const auto calls_before_final_sample = recording.calls.size();
	const auto completed = game->step(
		{}, th03::InputContext::interstitial, &recording
	);
	std::vector<BackgroundObservedCall> expected_resumed_order{
		BackgroundObservedCall::warning_sample,
		BackgroundObservedCall::frame,
	};
	expected_resumed_order.insert(
		expected_resumed_order.end(),
		48U,
		BackgroundObservedCall::random_read
	);
	check(
		completed.status == th03::StepStatus::phase_transition &&
		recording.boss_views.size() == 1U &&
		recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
			ObservedPhase::extra_p1,
			ObservedPhase::charge_p1,
			ObservedPhase::extra_p2,
			ObservedPhase::charge_p2,
			ObservedPhase::players_bullets,
		} &&
		recording.warning_interstitial_samples.size() == 27U &&
		recording.warning_interstitial_samples.back().ordinal == 26U &&
		recording.main_frame_footers ==
			std::vector<th03::MainFrameFooterReached>{{}} &&
		std::vector<ObservedCall>(
			recording.calls.begin() +
				static_cast<std::ptrdiff_t>(calls_before_final_sample),
			recording.calls.end()
		) == std::vector<ObservedCall>{
			ObservedCall::warning_interstitial_sample,
			ObservedCall::shots_enemies,
			ObservedCall::fireballs,
			ObservedCall::extra_p1,
			ObservedCall::charge_p1,
			ObservedCall::extra_p2,
			ObservedCall::charge_p2,
			ObservedCall::players_bullets,
			ObservedCall::main_frame_footer_reached,
		} && recording.background_bomb_calls == expected_resumed_order,
		"warning sample 26 resumes into BackgroundFrame and all initial reads before later suffix phases"
	);
	check(
		recording.warning_gauge_calls ==
			std::vector<WarningGaugeObservedCall>{
				WarningGaugeObservedCall::warning_interstitial_sample,
				WarningGaugeObservedCall::warning_frame_lifecycle,
				WarningGaugeObservedCall::background_frame,
				WarningGaugeObservedCall::shots_enemies,
				WarningGaugeObservedCall::fireballs,
				WarningGaugeObservedCall::players_bullets,
				WarningGaugeObservedCall::main_gauge_state,
				WarningGaugeObservedCall::main_frame_footer_reached,
			},
		"warning sample 26 precedes the lifecycle and resumed suffix"
	);
}

void test_warning_gauge_semantic_observation()
{
	with_live_state([](auto &base) {
		base.hud_start_flag = 1U;
		base.warning_flag = {
			th03::detail::WarningFlag::flash_red,
			th03::detail::WarningFlag::flash_red_end,
		};
		base.gba_flag_next = {
			th03::detail::GaugeBossAttackFlag::gauge_bullet_init,
			th03::detail::GaugeBossAttackFlag::boss,
		};
		base.gba_flag_active = {
			th03::detail::GaugeBossAttackFlag::gauge_pellet_active,
			th03::detail::GaugeBossAttackFlag::none,
		};
		base.gba_gauge_level = {4U, 13U};
		base.gba_boss_level = 11U;
		base.round_id = 2U;
		base.round_or_result_frame = 0x1234U;
		base.playfield_fg_shift_x = {-7, 9};
		base.players[0U].gauge_charged = 0x123U;
		base.players[0U].gauge_avail = 0x456U;
		base.players[0U].hyper_active = 7U;
		base.players[1U].gauge_charged = 0x789U;
		base.players[1U].gauge_avail = 0xABCU;
		base.players[1U].hyper_active = 8U;

		auto null_state = base;
		auto discard_state = base;
		auto record_state = base;
		DiscardObserver discard;
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			null_state, nullptr
		);
		th03::detail::gameplay_frame_suffix_after_interstitial(
			discard_state, &discard
		);
		th03::detail::gameplay_frame_suffix_after_interstitial(
			record_state, &recording
		);
		check(
			observed_gameplay_equal(null_state, discard_state) &&
			observed_gameplay_equal(null_state, record_state),
			"warning/Gauge facts preserve gameplay state and shared RNG"
		);
		check(
			recording.warning_lifecycles ==
				std::vector<th03::WarningFrameLifecycle>{{
					.players = {
						th03::WarningPlayerLifecycle{
							.warning_step_before = 3U,
							.warning_step_after = 4U,
							.next_attack =
								th03::GaugeBossAttackKind::gauge_bullet,
							.active_attack =
								th03::GaugeBossAttackKind::gauge_pellet,
						},
						th03::WarningPlayerLifecycle{
							.warning_step_before = 33U,
							.warning_step_after = 0U,
							.next_attack = th03::GaugeBossAttackKind::boss,
							.active_attack = th03::GaugeBossAttackKind::boss,
						},
					},
					.gauge_levels = {4U, 13U},
					.boss_level = 11U,
				}} &&
			recording.main_gauge_states ==
				std::vector<th03::MainGaugeState>{{
					.context = {
						.round_id = 2U,
						.round_or_result_frame = 0x1234U,
						.playfield_fg_shift_x = {-7, 9},
					},
					.players = {
						th03::MainGaugePlayerState{0x123U, 0x456U, 7U},
						th03::MainGaugePlayerState{0x789U, 0xABCU, 8U},
					},
				}} &&
			recording.warning_gauge_calls ==
				std::vector<WarningGaugeObservedCall>{
					WarningGaugeObservedCall::warning_frame_lifecycle,
					WarningGaugeObservedCall::background_frame,
					WarningGaugeObservedCall::shots_enemies,
					WarningGaugeObservedCall::fireballs,
					WarningGaugeObservedCall::players_bullets,
					WarningGaugeObservedCall::main_gauge_state,
					WarningGaugeObservedCall::main_frame_footer_reached,
				},
			"warning lifecycle precedes background and late Gauge state follows the result gate"
		);
	});

	with_live_state([](auto &portraits) {
		portraits.hud_start_flag = 1U;
		portraits.warning_flag = {
			th03::detail::WarningFlag::portrait,
			th03::detail::WarningFlag::portrait,
		};
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			portraits, &recording
		);
		check(
			recording.warning_portrait_transitions ==
				std::vector<th03::WarningPortraitTransition>{
					{th03::PlayerId::p1},
					{th03::PlayerId::p2},
				} &&
			portraits.warning_flag ==
				std::array<th03::detail::WarningFlag, 2>{
					th03::detail::WarningFlag::flash_white,
					th03::detail::WarningFlag::flash_white,
				} &&
			recording.warning_gauge_calls ==
				std::vector<WarningGaugeObservedCall>{
					WarningGaugeObservedCall::warning_frame_lifecycle,
					WarningGaugeObservedCall::background_frame,
					WarningGaugeObservedCall::shots_enemies,
					WarningGaugeObservedCall::fireballs,
					WarningGaugeObservedCall::players_bullets,
					WarningGaugeObservedCall::main_gauge_state,
					WarningGaugeObservedCall::warning_portrait_transition,
					WarningGaugeObservedCall::warning_portrait_transition,
					WarningGaugeObservedCall::main_frame_footer_reached,
				},
			"late warning portraits retain P1-then-P2 order after the Gauge state"
		);
	});

	with_live_state([](auto &terminal) {
		terminal.hud_start_flag = 1U;
		terminal.warning_flag[0U] = th03::detail::WarningFlag::portrait;
		terminal.defeat_flag = th03::detail::DefeatFlag::banner;
		terminal.players[0U].rounds_won = 2U;
		terminal.result_transition_complete = true;
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			terminal, &recording
		);
		check(
			recording.main_gauge_states.empty() &&
			recording.warning_portrait_transitions.empty() &&
			recording.warning_gauge_calls ==
				std::vector<WarningGaugeObservedCall>{
					WarningGaugeObservedCall::warning_frame_lifecycle,
					WarningGaugeObservedCall::background_frame,
					WarningGaugeObservedCall::shots_enemies,
					WarningGaugeObservedCall::fireballs,
					WarningGaugeObservedCall::players_bullets,
				},
			"the result-control early return suppresses Gauge and portrait facts"
		);
	});

	constexpr std::array<std::int8_t, 9> paletted_characters{
		1, 3, 5, 7, 9, 11, 13, 15, 17,
	};
	for(std::size_t character = 0U;
		character < paletted_characters.size();
		++character) {
		auto base = make_live_state();
		base.hud_start_flag = 1U;
		base.players[0U].paletted_character = paletted_characters[character];
		base.gba_flag_active[0U] =
			th03::detail::GaugeBossAttackFlag::gauge_bullet_active;
		base.gauge_pattern_param_a[0U] = 7U;
		base.gauge_pattern_param_b[0U] = 0xFFU;
		switch(character) {
		case 0U:
			base.reimu_gauge_pattern_frame[0U] = 0x41U;
			break;
		case 1U:
			base.mima_gauge_pattern_frame[0U] = 0x41U;
			break;
		case 2U:
			base.marisa_gauge_pattern_frame[0U] = 1U;
			base.marisa_gauge_pattern_y[0U] = 368 << 4;
			break;
		case 3U:
			base.ellen_gauge_pattern_frame[0U] = 8U;
			base.ellen_gauge_pattern_x[0U] = 0;
			base.ellen_gauge_pattern_y[0U] = 144 << 4;
			break;
		case 4U:
			base.kotohime_gauge_pattern_frame[0U] = 0x80U;
			break;
		case 5U:
			base.kana_gauge_pattern_frame[0U] = 0x80U;
			break;
		case 6U:
			base.rikako_gauge_pattern_frame[0U] = 0x80U;
			break;
		case 7U:
			base.chiyuri_gauge_pattern_frame[0U] = 1U;
			base.gauge_pattern_param_a[0U] = 2U;
			break;
		case 8U:
			base.yumemi_gauge_pattern_frame[0U] = 0x38U;
			break;
		default:
			break;
		}

		auto null_state = base;
		auto discard_state = base;
		auto record_state = base;
		DiscardObserver discard;
		RecordingObserver recording;
		check(
			th03::detail::gameplay_frame_prefix(
				null_state, th03::GameContent{}, nullptr
			) &&
			th03::detail::gameplay_frame_prefix(
				discard_state, th03::GameContent{}, &discard
			) &&
			th03::detail::gameplay_frame_prefix(
				record_state, th03::GameContent{}, &recording
			),
			"each character Gauge completion fixture reaches its prefix boundary"
		);
		check(
			observed_gameplay_equal(null_state, discard_state) &&
			observed_gameplay_equal(null_state, record_state) &&
			record_state.gba_flag_active[0U] ==
				th03::detail::GaugeBossAttackFlag::none &&
			recording.gauge_attack_finishes ==
				std::vector<th03::GaugeAttackFinished>{{
					.attacker = th03::PlayerId::p1,
					.target = th03::PlayerId::p2,
				}},
			"all nine character callbacks emit one observer-invariant Gauge completion"
		);
	}

	with_live_state([](auto &simultaneous) {
		simultaneous.hud_start_flag = 1U;
		simultaneous.players[0U].paletted_character = 1;
		simultaneous.players[1U].paletted_character = 2;
		simultaneous.gba_flag_active = {
			th03::detail::GaugeBossAttackFlag::gauge_pellet_active,
			th03::detail::GaugeBossAttackFlag::gauge_bullet_active,
		};
		simultaneous.reimu_gauge_pattern_frame = {0x41U, 0x41U};
		simultaneous.gauge_pattern_param_a = {7U, 7U};
		simultaneous.gauge_pattern_param_b = {0xFFU, 0xFFU};
		RecordingObserver recording;
		(void)th03::detail::gameplay_frame_prefix(
			simultaneous, th03::GameContent{}, &recording
		);
		check(
			recording.gauge_attack_finishes ==
				std::vector<th03::GaugeAttackFinished>{
					{th03::PlayerId::p1, th03::PlayerId::p2},
					{th03::PlayerId::p2, th03::PlayerId::p1},
				},
			"simultaneous Gauge completions retain source P1-then-P2 order"
		);
	});
}

void test_boss_observer_contract()
{
	with_live_state([](auto &base) {
		base.hud_start_flag = 1U;
		base.players[0U].paletted_character = 1;
		base.players[1U].paletted_character = 4;
		base.gba_flag_active = {
			th03::detail::GaugeBossAttackFlag::boss,
			th03::detail::GaugeBossAttackFlag::boss,
		};
		base.boss_initial[0U].center_x = 144 << 4;
		base.boss_initial[0U].center_y = 80 << 4;
		base.boss_initial[0U].hp = 100U;

		auto null_state = base;
		auto discard_state = base;
		auto record_state = base;
		DiscardObserver discard;
		RecordingObserver recording;
		check(
			th03::detail::gameplay_frame_prefix(
				null_state, th03::GameContent{}, nullptr
			) &&
			th03::detail::gameplay_frame_prefix(
				discard_state, th03::GameContent{}, &discard
			) &&
			th03::detail::gameplay_frame_prefix(
				record_state, th03::GameContent{}, &recording
			),
			"simultaneous Boss launch/reversal fixtures complete"
		);
		check(
			observed_gameplay_equal(null_state, discard_state) &&
			observed_gameplay_equal(null_state, record_state),
			"Boss lifecycle observation preserves gameplay and shared RNG for null, discard, and recording paths"
		);
		check(
			recording.boss_palette_restores ==
				std::vector<th03::BossPaletteBaselineRestored>{
					{th03::PlayerId::p2},
					{th03::PlayerId::p1},
				} &&
			recording.boss_activations ==
				std::vector<th03::BossActivated>{
					{th03::PlayerId::p1, 1},
				} &&
			recording.boss_calls == std::vector<BossObservedCall>{
				BossObservedCall::palette_baseline_restored,
				BossObservedCall::activated,
				BossObservedCall::palette_baseline_restored,
			},
			"a successful P1 launch restores P2 before activation, then a P2 reversal restores its actual P1 entry"
		);
	});

	with_live_state([](auto &p2_launch) {
		p2_launch.hud_start_flag = 1U;
		p2_launch.players[1U].paletted_character = 18;
		p2_launch.gba_flag_active[1U] =
			th03::detail::GaugeBossAttackFlag::boss;
		p2_launch.boss_initial[1U].hp = 100U;
		RecordingObserver recording;
		(void)th03::detail::gameplay_frame_prefix(
			p2_launch, th03::GameContent{}, &recording
		);
		check(
			recording.boss_palette_restores ==
				std::vector<th03::BossPaletteBaselineRestored>{
					{th03::PlayerId::p1},
				} &&
			recording.boss_activations ==
				std::vector<th03::BossActivated>{
					{th03::PlayerId::p2, 18},
				},
			"a successful P2 launch reports its own character after restoring P1"
		);
	});

	with_live_state([](auto &defeat) {
		defeat.hud_start_flag = 1U;
		defeat.players[0U].paletted_character = 1;
		defeat.gba_boss_launched_by = 0;
		defeat.boss.state = 1U;
		defeat.boss.hp = 0U;
		RecordingObserver recording;
		(void)th03::detail::gameplay_frame_prefix(
			defeat, th03::GameContent{}, &recording
		);
		check(
			defeat.boss.state == 0xFFU &&
			recording.boss_palette_restores ==
				std::vector<th03::BossPaletteBaselineRestored>{
					{th03::PlayerId::p2},
				},
			"Boss defeat restores the target playfield palette at the state transition"
		);
	});

	with_live_state([](auto &ellen) {
		ellen.hud_start_flag = 1U;
		ellen.players[0U].paletted_character = 7;
		ellen.gba_boss_launched_by = 0;
		ellen.boss.state = 0U;
		ellen.boss.pattern_frame = 0U;
		ellen.boss.render_state = 9U;
		ellen.boss.hp = 100U;
		RecordingObserver recording;
		(void)th03::detail::gameplay_frame_prefix(
			ellen, th03::GameContent{}, &recording
		);
		check(
			recording.boss_animation_steps ==
				std::vector<th03::BossAnimationStep>{
					{th03::PlayerId::p1, 0U, 1U, 9U},
				},
			"Ellen advances presenter animation after pattern-frame increment and before an early state-0 return"
		);
	});

	with_live_state([](auto &rikako) {
		rikako.hud_start_flag = 1U;
		rikako.players[0U].paletted_character = 13;
		rikako.gba_boss_launched_by = 0;
		rikako.boss.state = 0U;
		rikako.boss.hp = 100U;
		RecordingObserver early;
		(void)th03::detail::gameplay_frame_prefix(
			rikako, th03::GameContent{}, &early
		);
		check(
			early.boss_animation_steps.empty(),
			"Rikako materialization frames that skip the common tail do not advance presenter animation"
		);

		rikako.boss.pattern_frame = 99U;
		rikako.boss.render_state = 6U;
		RecordingObserver common_tail;
		(void)th03::detail::gameplay_frame_prefix(
			rikako, th03::GameContent{}, &common_tail
		);
		check(
			common_tail.boss_animation_steps ==
				std::vector<th03::BossAnimationStep>{
					{th03::PlayerId::p1, 0U, 100U, 6U},
				},
			"Rikako reports one animation step only when dispatch reaches its common tail"
		);
	});

	with_live_state([](auto &chiyuri) {
		chiyuri.hud_start_flag = 1U;
		chiyuri.players[0U].paletted_character = 15;
		chiyuri.gba_boss_launched_by = 0;
		chiyuri.boss.state = 0x80U;
		chiyuri.boss.center_y = 0;
		chiyuri.boss.hp = 100U;
		chiyuri.boss.render_state = 4U;
		RecordingObserver recording;
		(void)th03::detail::gameplay_frame_prefix(
			chiyuri, th03::GameContent{}, &recording
		);
		check(
			recording.boss_palette_restores ==
				std::vector<th03::BossPaletteBaselineRestored>{
					{th03::PlayerId::p2},
				} &&
			recording.boss_animation_steps ==
				std::vector<th03::BossAnimationStep>{
					{th03::PlayerId::p1, 0x80U, 1U, 4U},
				} &&
			recording.boss_calls == std::vector<BossObservedCall>{
				BossObservedCall::palette_baseline_restored,
				BossObservedCall::animation_step,
			},
			"Chiyuri state 0x80 restores the target palette before its common-tail animation step"
		);
	});

	with_live_state([](auto &late) {
		late.players[0U].paletted_character = 15;
		late.gba_boss_launched_by = 0;
		late.boss.state = 0xFFU;
		late.boss.pattern_frame = 25U;
		late.bomb_flag = {
			th03::detail::BombFlag::active,
			th03::detail::BombFlag::active,
		};
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			late, &recording
		);
		std::vector<BossObservedCall> expected_order{
			BossObservedCall::warning_frame_lifecycle,
			BossObservedCall::background_frame,
		};
		expected_order.insert(
			expected_order.end(),
			48U,
			BossObservedCall::background_random_read
		);
		expected_order.insert(
			expected_order.end(),
			{
				BossObservedCall::bomb_frame_step,
				BossObservedCall::bomb_frame_step,
				BossObservedCall::main_boss,
				BossObservedCall::palette_baseline_restored,
				BossObservedCall::shots_enemies,
			}
		);
		check(
			recording.boss_views.size() == 1U &&
			recording.boss_views[0U] &&
			recording.boss_views[0U]->state == 0xFFU &&
			late.gba_boss_launched_by == -1 &&
			recording.boss_palette_restores ==
				std::vector<th03::BossPaletteBaselineRestored>{
					{th03::PlayerId::p2},
				} &&
			recording.boss_calls == expected_order,
			"late order spans warning, BackgroundFrame and reads, P1/P2 Bombs, pre-mutation Boss view, Chiyuri terminal restore after clear, then Shots"
		);
	});
}

void test_main_effect_observer_contract()
{
	with_live_state([](auto &boss_effect) {
		boss_effect.hud_start_flag = 1U;
		boss_effect.players[0U].paletted_character = 3;
		boss_effect.gba_boss_launched_by = 0;
		boss_effect.boss.state = 6U;
		boss_effect.boss.pattern_frame = 0x1FU;
		boss_effect.boss.center_x = 120 << 4;
		boss_effect.boss.center_y = 100 << 4;
		boss_effect.boss.hp = 100U;
		boss_effect.playfield_fg_shift_x[1U] = 5;
		RecordingObserver recording;
		(void)th03::detail::gameplay_frame_prefix(
			boss_effect, th03::GameContent{}, &recording
		);
		check(
			recording.common_effects ==
				std::vector<th03::CommonEffectSpawned>{
					{
						.kind = th03::CommonEffectKind::contracting_circle,
						.player = th03::PlayerId::p2,
						.center_x = (120 + 24) << 4,
						.center_y = (100 - 32) << 4,
						.fg_shift_x_at_spawn = 5,
					},
				} &&
			recording.calls == std::vector<ObservedCall>{
				ObservedCall::common_effect_spawned,
				ObservedCall::hit_circles_advanced,
				ObservedCall::common_effects_advanced,
			},
			"Boss common effects precede the hit-circle and common-effect prefix advances"
		);
	});

	with_live_state([](auto &base) {
		base.hud_start_flag = 1U;
		base.players[0U].paletted_character = 17;
		base.gba_flag_active[0U] =
			th03::detail::GaugeBossAttackFlag::gauge_bullet_active;
		base.yumemi_gauge_pattern_frame[0U] = 0U;
		base.gauge_pattern_param_a[0U] = 24U;
		base.gauge_pattern_param_b[0U] = 32U;
		base.playfield_fg_shift_x[1U] = -7;
		base.randring[0U] = 0xA5U;
		base.randring[1U] = 0x5AU;

		auto null_state = base;
		auto discard_state = base;
		auto record_state = base;
		DiscardObserver discard;
		RecordingObserver recording;
		check(
			th03::detail::gameplay_frame_prefix(
				null_state, th03::GameContent{}, nullptr
			) &&
			th03::detail::gameplay_frame_prefix(
				discard_state, th03::GameContent{}, &discard
			) &&
			th03::detail::gameplay_frame_prefix(
				record_state, th03::GameContent{}, &recording
			),
			"effect lifecycle prefix fixtures complete"
		);
		check(
			observed_gameplay_equal(null_state, discard_state) &&
			observed_gameplay_equal(null_state, record_state),
			"null, discard, and recording effect paths preserve gameplay and RNG"
		);
		check(
			recording.calls == std::vector<ObservedCall>{
				ObservedCall::hit_circles_advanced,
				ObservedCall::common_effect_spawned,
				ObservedCall::common_effects_advanced,
			},
			"hit-circle advance, late Gauge spawn, and common-effect advance retain prefix order"
		);
		check(
			recording.common_effects ==
				std::vector<th03::CommonEffectSpawned>{
					{
						.kind = th03::CommonEffectKind::contracting_square,
						.player = th03::PlayerId::p2,
						.center_x = 144 << 4,
						.center_y = 32 << 4,
						.fg_shift_x_at_spawn = -7,
						.initial_angle = 0xA5U,
					},
				},
			"Yumemi Gauge exposes the source effect center and consumed low RNG byte"
		);
	});

	with_live_state([](auto &bombs) {
		bombs.players[0U].paletted_character = 11;
		bombs.players[1U].paletted_character = 9;
		bombs.bomb_flag = {
			th03::detail::BombFlag::active,
			th03::detail::BombFlag::active,
		};
		bombs.bomb_frame = {63U, 64U};
		bombs.playfield_fg_shift_x = {7, -7};
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			bombs, &recording
		);
		check(
			recording.bomb_frame_steps ==
				std::vector<th03::BombFrameStep>{
					{
						.player = th03::PlayerId::p1,
						.paletted_character = 11,
						.bomb_frame = 63U,
						.fg_shift_x_at_entry = 7,
					},
					{
						.player = th03::PlayerId::p2,
						.paletted_character = 9,
						.bomb_frame = 64U,
						.fg_shift_x_at_entry = -7,
					},
				},
			"every active late Bomb callback exposes its P1-then-P2 inputs"
		);
		check(
			recording.bomb_common_steps ==
				std::vector<th03::BombCommonEffectStep>{
					{
						.kind = th03::BombCommonEffectKind::kana,
						.player = th03::PlayerId::p1,
						.bomb_frame = 63U,
						.fg_shift_x_at_effects = 7,
					},
					{
						.kind = th03::BombCommonEffectKind::kotohime,
						.player = th03::PlayerId::p2,
						.bomb_frame = 64U,
						.fg_shift_x_at_effects = 4,
					},
				},
			"simultaneous exceptional Bomb callbacks preserve shared P1-then-P2 frame-63/frame-64 order"
		);
		check(
			recording.common_effects.empty(),
			"Kana and Kotohime callbacks do not fabricate core-owned concrete effects"
		);
		check(
			recording.calls == std::vector<ObservedCall>{
				ObservedCall::bomb_frame_step,
				ObservedCall::bomb_common_effect_step,
				ObservedCall::bomb_frame_step,
				ObservedCall::bomb_common_effect_step,
				ObservedCall::shots_enemies,
				ObservedCall::fireballs,
				ObservedCall::extra_p1,
				ObservedCall::charge_p1,
				ObservedCall::extra_p2,
				ObservedCall::charge_p2,
				ObservedCall::players_bullets,
				ObservedCall::main_frame_footer_reached,
			},
			"Bomb frame facts precede the retained character-specific common-effect steps"
		);
	});

	with_live_state([](auto &bombs) {
		bombs.players[0U].paletted_character = 11;
		bombs.bomb_flag[0U] = th03::detail::BombFlag::preparing;
		bombs.bomb_frame[0U] = 180U;
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			bombs, &recording
		);
		check(
			recording.bomb_frame_steps.size() == 1U &&
			recording.bomb_frame_steps[0U].preparing &&
			recording.bomb_frame_steps[0U].bomb_frame == 180U,
			"a newly preparing Bomb exposes the retained prior frame without normalizing source order"
		);
	});

	with_live_state([](auto &bombs) {
		bombs.players[0U].paletted_character = 3;
		bombs.players[1U].paletted_character = 1;
		bombs.bomb_flag = {
			th03::detail::BombFlag::active,
			th03::detail::BombFlag::active,
		};
		bombs.bomb_frame = {88U, 31U};

		auto null_state = bombs;
		auto discard_state = bombs;
		auto record_state = bombs;
		DiscardObserver discard;
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(null_state);
		th03::detail::gameplay_frame_suffix_after_interstitial(
			discard_state, &discard
		);
		th03::detail::gameplay_frame_suffix_after_interstitial(
			record_state, &recording
		);
		check(
			observed_gameplay_equal(null_state, discard_state) &&
				observed_gameplay_equal(null_state, record_state),
			"Bomb frame observation preserves state and shared RNG for null, discard, and recording paths"
		);
		check(
			recording.bomb_frame_steps ==
				std::vector<th03::BombFrameStep>{
					{
						.player = th03::PlayerId::p1,
						.paletted_character = 3,
						.bomb_frame = 88U,
					},
					{
						.player = th03::PlayerId::p2,
						.paletted_character = 1,
						.bomb_frame = 31U,
					},
				} &&
				recording.calls == std::vector<ObservedCall>{
				ObservedCall::bomb_frame_step,
				ObservedCall::common_effect_spawned,
				ObservedCall::bomb_frame_step,
				ObservedCall::bomb_compatibility_read,
				ObservedCall::bomb_compatibility_read,
				ObservedCall::shots_enemies,
				ObservedCall::fireballs,
				ObservedCall::extra_p1,
				ObservedCall::charge_p1,
				ObservedCall::extra_p2,
				ObservedCall::charge_p2,
				ObservedCall::players_bullets,
				ObservedCall::main_frame_footer_reached,
			},
			"Bomb frame inputs precede Mima's character-specific effect work and cover ordinary callbacks"
		);
	});

	with_live_state([](auto &chiyuri) {
		chiyuri.players[0U].paletted_character = 15;
		chiyuri.bomb_flag[0U] = th03::detail::BombFlag::active;
		chiyuri.bomb_frame[0U] = 79U;
		chiyuri.playfield_fg_shift_x[0U] = -4;
		chiyuri.randring[0U] = 0x11U;
		chiyuri.randring[1U] = 0x22U;
		chiyuri.randring[2U] = 0x33U;
		chiyuri.randring[3U] = 0x44U;
		chiyuri.randring[4U] = 0x55U;
		RecordingObserver recording;
		chiyuri.hud_start_flag = 1U;
		(void)th03::detail::gameplay_frame_prefix(
			chiyuri, th03::GameContent{}, &recording
		);
		th03::detail::gameplay_frame_suffix_after_interstitial(
			chiyuri, &recording
		);
		check(
			recording.common_effects ==
				std::vector<th03::CommonEffectSpawned>{
					{th03::CommonEffectKind::contracting_square, th03::PlayerId::p1,
						std::int16_t{(144 - 32) << 4}, std::int16_t{184 << 4}, 4, 0x11U},
					{th03::CommonEffectKind::contracting_square, th03::PlayerId::p1,
						std::int16_t{(144 + 32) << 4}, std::int16_t{184 << 4}, 4, 0x22U},
					{th03::CommonEffectKind::contracting_square, th03::PlayerId::p1,
						std::int16_t{144 << 4}, std::int16_t{(184 - 32) << 4}, 4, 0x33U},
					{th03::CommonEffectKind::contracting_square, th03::PlayerId::p1,
						std::int16_t{144 << 4}, std::int16_t{(184 + 32) << 4}, 4, 0x44U},
				},
			"Chiyuri Bomb preserves four-spawn geometry, order, shift, and shared-ring angles"
		);
		const auto common_advance = std::find(
			recording.calls.begin(), recording.calls.end(),
			ObservedCall::common_effects_advanced
		);
		const auto first_bomb_effect = std::find(
			recording.calls.begin(), recording.calls.end(),
			ObservedCall::common_effect_spawned
		);
		check(
			common_advance < first_bomb_effect,
			"prefix common-effect advance precedes late Bomb spawns"
		);
	});

	with_live_state([](auto &yumemi) {
		yumemi.players[0U].paletted_character = 17;
		yumemi.bomb_flag[0U] = th03::detail::BombFlag::active;
		yumemi.bomb_frame[0U] = 72U;
		yumemi.playfield_fg_shift_x[0U] = -4;
		yumemi.randring[0U] = 0x9CU;
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			yumemi, &recording
		);
		check(
			recording.common_effects.size() == 1U &&
			recording.common_effects[0U].fg_shift_x_at_spawn == -4 &&
			recording.common_effects[0U].initial_angle == 0x9CU &&
			yumemi.playfield_fg_shift_x[0U] == 4,
			"Yumemi Bomb captures the prior shift before applying this frame's shake"
		);
	});

	with_live_state([](auto &ellen) {
		ellen.players[0U].paletted_character = 7;
		ellen.bomb_flag[0U] = th03::detail::BombFlag::active;
		ellen.bomb_frame[0U] = 56U;
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			ellen, &recording
		);
		check(
			recording.common_effects.empty(),
			"Ellen Bomb has no spurious frame-below-64 common effect"
		);
	});

	with_live_state([](auto &reimu) {
		reimu.players[0U].paletted_character = 1;
		reimu.bomb_flag[0U] = th03::detail::BombFlag::active;
		reimu.bomb_frame[0U] = 5U;
		reimu.playfield_fg_shift_x[0U] = -9;
		reimu.randring[0U] = 0x34U;
		reimu.randring[1U] = 0x12U;
		reimu.randring[2U] = 0x56U;
		auto null_state = reimu;
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			null_state, nullptr
		);
		th03::detail::gameplay_frame_suffix_after_interstitial(
			reimu, &recording
		);
		check(
			observed_gameplay_equal(null_state, reimu),
			"Bomb compatibility-read observation preserves late shared RNG state"
		);
		check(
			recording.bomb_frame_steps ==
				std::vector<th03::BombFrameStep>{{
					.player = th03::PlayerId::p1,
					.paletted_character = 1,
					.bomb_frame = 5U,
					.fg_shift_x_at_entry = -9,
				}} &&
			recording.bomb_compatibility_reads ==
				std::vector<th03::BombCompatibilityRead>{
					{
						.player = th03::PlayerId::p1,
						.site = th03::BombCompatibilityReadSite::
							reimu_falling_velocity,
						.bomb_frame = 5U,
						.raw_value = 0x1234U,
					},
					{
						.player = th03::PlayerId::p1,
						.site = th03::BombCompatibilityReadSite::
							reimu_falling_x,
						.bomb_frame = 5U,
						.raw_value = 0x5612U,
					},
				},
			"Reimu Bomb exposes entry shift and both already-consumed visual reads"
		);
		check(
			recording.calls[0U] == ObservedCall::bomb_frame_step &&
			recording.calls[1U] == ObservedCall::bomb_compatibility_read &&
			recording.calls[2U] == ObservedCall::bomb_compatibility_read,
			"Reimu Bomb compatibility reads follow its late callback step in source order"
		);
	});

	with_live_state([](auto &prefix_bombs) {
		prefix_bombs.hud_start_flag = 1U;
		prefix_bombs.players[0U].paletted_character = 3;
		prefix_bombs.players[1U].paletted_character = 7;
		prefix_bombs.bomb_flag = {
			th03::detail::BombFlag::active,
			th03::detail::BombFlag::active,
		};
		prefix_bombs.bomb_frame = {7U, 1U};
		prefix_bombs.ellen_bomb_vectors[1U][0U].x = 9999;
		for(std::size_t index = 1U;
			index < prefix_bombs.ellen_bomb_vectors[1U].size();
			++index) {
			prefix_bombs.ellen_bomb_vectors[1U][index].x = 19999;
		}
		prefix_bombs.randring[0U] = 0x78U;
		prefix_bombs.randring[1U] = 0x56U;
		prefix_bombs.randring[2U] = 0x34U;

		auto null_state = prefix_bombs;
		auto discard_state = prefix_bombs;
		auto record_state = prefix_bombs;
		DiscardObserver discard;
		RecordingObserver recording;
		check(
			th03::detail::gameplay_frame_prefix(
				null_state, th03::GameContent{}, nullptr
			) &&
			th03::detail::gameplay_frame_prefix(
				discard_state, th03::GameContent{}, &discard
			) &&
			th03::detail::gameplay_frame_prefix(
				record_state, th03::GameContent{}, &recording
			),
			"prefix Bomb compatibility-read fixtures complete"
		);
		check(
			observed_gameplay_equal(null_state, discard_state) &&
			observed_gameplay_equal(null_state, record_state),
			"prefix Bomb compatibility reads preserve state and RNG across observers"
		);
		check(
			recording.bomb_compatibility_reads ==
				std::vector<th03::BombCompatibilityRead>{
					{
						.player = th03::PlayerId::p1,
						.site = th03::BombCompatibilityReadSite::mima_column_x,
						.bomb_frame = 8U,
						.raw_value = 0x5678U,
					},
					{
						.player = th03::PlayerId::p2,
						.site = th03::BombCompatibilityReadSite::
							ellen_vector_angle,
						.bomb_frame = 2U,
						.raw_value = 0x3456U,
					},
				},
			"Mima and Ellen expose prefix visual reads in source order"
		);
	});

	with_live_state([](auto &finishing) {
		finishing.hud_start_flag = 1U;
		finishing.players[0U].paletted_character = 1;
		finishing.players[1U].paletted_character = 3;
		finishing.bomb_flag[0U] = th03::detail::BombFlag::active;
		finishing.bomb_flag[1U] = th03::detail::BombFlag::active;
		finishing.bomb_frame[0U] = 179U;
		finishing.bomb_frame[1U] = 7U;
		finishing.randring[0U] = 0xCDU;
		finishing.randring[1U] = 0xABU;
		auto null_state = finishing;
		auto discard_state = finishing;
		auto record_state = finishing;
		DiscardObserver discard;
		RecordingObserver recording;
		check(
			th03::detail::gameplay_frame_prefix(
				null_state, th03::GameContent{}, nullptr
			) &&
			th03::detail::gameplay_frame_prefix(
				discard_state, th03::GameContent{}, &discard
			) &&
			th03::detail::gameplay_frame_prefix(
				record_state, th03::GameContent{}, &recording
			),
			"finishing Bomb prefix fixture completes"
		);
		check(
			observed_gameplay_equal(null_state, discard_state) &&
			observed_gameplay_equal(null_state, record_state),
			"Bomb completion plus the following P2 visual read is observer-invariant"
		);
		check(
			record_state.bomb_flag[0U] == th03::detail::BombFlag::inactive &&
			recording.bomb_finishes ==
				std::vector<th03::BombFinished>{{
					.player = th03::PlayerId::p1,
					.paletted_character = 1,
				}},
			"Bomb completion emits the source palette-restore lifecycle fact"
		);
		check(
			recording.calls[0U] == ObservedCall::bomb_finished &&
			recording.calls[1U] == ObservedCall::bomb_compatibility_read &&
			recording.bomb_compatibility_reads ==
				std::vector<th03::BombCompatibilityRead>{{
					.player = th03::PlayerId::p2,
					.site = th03::BombCompatibilityReadSite::mima_column_x,
					.bomb_frame = 8U,
					.raw_value = 0xABCDU,
				}} &&
			std::find(
				recording.calls.begin(), recording.calls.end(),
				ObservedCall::bomb_frame_step
			) == recording.calls.end(),
			"P1 completion precedes P2 Bomb work and suppresses a final late step"
		);
	});

	with_live_state([](auto &fireball_hit) {
		auto &fireball = fireball_hit.fireballs[0U];
		fireball.flag = th03::detail::FireballFlag::fall;
		fireball.player_id = 0U;
		fireball.center_x = 100 << 4;
		fireball.center_y = 120 << 4;
		fireball.hp = 1U;
		auto &shot = fireball_hit.shotpairs[0U];
		shot.alive = true;
		shot.player_id = 0U;
		shot.topleft_x = 90 << 4;
		shot.topleft_y = 115 << 4;
		fireball_hit.playfield_fg_shift_x[0U] = 6;
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			fireball_hit, &recording
		);
		check(
			recording.hit_circles ==
				std::vector<th03::HitCircleSpawned>{
					{
						.kind = th03::HitCircleKind::enemy,
						.player = th03::PlayerId::p1,
						.center_x = 106 << 4,
						.center_y = 123 << 4,
						.fg_shift_x_at_spawn = 6,
					},
				},
			"ordinary shot collision exposes its exact center and spawn-time shift"
		);
		const auto hit_call = std::find(
			recording.calls.begin(), recording.calls.end(),
			ObservedCall::hit_circle_spawned
		);
		const auto shots_view = std::find(
			recording.calls.begin(), recording.calls.end(),
			ObservedCall::shots_enemies
		);
		const auto fireballs_view = std::find(
			recording.calls.begin(), recording.calls.end(),
			ObservedCall::fireballs
		);
		check(
			shots_view < hit_call && hit_call < fireballs_view,
			"late fireball hit-circle spawn remains between its prefix and post-hittest views"
		);
	});

	with_live_state([](auto &charge_hit) {
		auto &fireball = charge_hit.fireballs[0U];
		fireball.flag = th03::detail::FireballFlag::fall;
		fireball.player_id = 0U;
		fireball.center_x = 100 << 4;
		fireball.center_y = 120 << 4;
		fireball.hp = 1U;
		charge_hit.players[0U].paletted_character = 17;
		charge_hit.yumemi_charge[0U].state = 2U;
		charge_hit.yumemi_charge[0U].x = 100 << 4;
		charge_hit.yumemi_charge[0U].y = 120 << 4;
		charge_hit.playfield_fg_shift_x[0U] = -3;
		charge_hit.randring[0U] = 0x10U;
		charge_hit.randring[1U] = 0x01U;
		charge_hit.randring[2U] = 0x02U;
		auto no_observer = charge_hit;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			no_observer, nullptr
		);
		RecordingObserver recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			charge_hit, &recording
		);
		check(
			no_observer.randring_cursor == charge_hit.randring_cursor &&
			no_observer.randring == charge_hit.randring &&
			no_observer.random_seed == charge_hit.random_seed,
			"randomized hit-circle RNG consumption is observer-independent"
		);
		check(
			recording.hit_circles ==
				std::vector<th03::HitCircleSpawned>{
					{
						.kind = th03::HitCircleKind::enemy,
						.player = th03::PlayerId::p1,
						.center_x = 1680,
						.center_y = 2001,
						.fg_shift_x_at_spawn = -3,
					},
				} &&
			charge_hit.randring_cursor == 2U,
			"Charge collision exposes the two-ring-word randomized hit-circle center"
		);

		auto banner = charge_hit;
		banner.defeat_flag = th03::detail::DefeatFlag::banner;
		banner.randring_cursor = 0U;
		banner.fireballs[0U].flag = th03::detail::FireballFlag::fall;
		RecordingObserver banner_recording;
		th03::detail::gameplay_frame_suffix_after_interstitial(
			banner, &banner_recording
		);
		check(
			banner_recording.hit_circles.empty() &&
			banner.randring_cursor == 0U,
			"result banner suppresses Charge hit-circle events and their shared RNG reads"
		);
	});
}

} // namespace

int main()
{
	test_indexed_views_and_classifications();
	test_extra_charge_view_contract();
	test_main_boss_view_contract();
	test_observer_invariance_and_phase_order();
	test_public_step_result_invariance();
	test_main_background_creation_observation();
	test_main_background_snapshot_observation();
	test_main_background_frame_schedule_and_repeat();
	test_shot_spawn_and_reuse();
	test_main_feedback_source_order();
	test_player_pose_and_defeat_lifecycle();
	test_bullet16_lifecycle_styles_order_and_full_arena();
	test_post_hittest_fireball_view_and_terminal_order();
	test_main_hud_start_observation();
	test_main_hud_result_late_order_and_terminal_split();
	test_main_hud_mutation_sites();
	test_main_hud_deferred_and_repeat_lifecycle();
	test_recorded_pause_completion();
	test_recorded_pause_prefix_bullet_once();
	test_warning_completion();
	test_warning_gauge_semantic_observation();
	test_boss_observer_contract();
	test_main_effect_observer_contract();

	if(failures != 0) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << "All " << checks << " observer checks passed\n";
	return 0;
}
