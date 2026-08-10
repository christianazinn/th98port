#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
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
	players_bullets,
};

class DiscardObserver final : public th03::GameplayObserver {
};

class RecordingObserver final : public th03::GameplayObserver {
public:
	RecordingObserver()
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
		main_frame_footers.reserve(2U);
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
		calls.push_back(ObservedCall::bomb_frame_step);
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

	void warning_interstitial_sample(
		const th03::WarningInterstitialSample &sample
	) noexcept override
	{
		warning_interstitial_samples.push_back(sample);
		calls.push_back(ObservedCall::warning_interstitial_sample);
	}

	void main_frame_footer_reached(
		const th03::MainFrameFooterReached &footer
	) noexcept override
	{
		main_frame_footers.push_back(footer);
		calls.push_back(ObservedCall::main_frame_footer_reached);
	}

	void observe(const th03::ShotsEnemiesView &view) noexcept override
	{
		phases.push_back(ObservedPhase::shots_enemies);
		calls.push_back(ObservedCall::shots_enemies);
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

	void observe(const th03::FireballsView &view) noexcept override
	{
		phases.push_back(ObservedPhase::fireballs);
		calls.push_back(ObservedCall::fireballs);
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

	void observe(const th03::PlayersBulletsView &view) noexcept override
	{
		phases.push_back(ObservedPhase::players_bullets);
		calls.push_back(ObservedCall::players_bullets);
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
	std::vector<th03::MainFrameFooterReached> main_frame_footers;
	th03::MainPhaseContext shots_enemies_context{};
	th03::MainPhaseContext fireballs_context{};
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
		player_pools_equal(left, right) &&
		bullet_pools_equal(left, right) &&
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

void test_observer_invariance_and_phase_order()
{
	auto base = make_live_state();
	base.round_id = 1U;
	base.round_or_result_frame = 99U;
	base.playfield_fg_shift_x = {6, -6};
	auto null_state = base;
	auto discard_state = base;
	auto record_state = base;
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
		"null, discard, and recording observers leave hashes, entity pools, and RNG identical"
	);
	check(
		recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
			ObservedPhase::players_bullets,
		},
		"ordinary late suffix reports all three source draw phases exactly once"
	);
	check(
		recording.main_frame_footers ==
			std::vector<th03::MainFrameFooterReached>{{}} &&
		recording.calls == std::vector<ObservedCall>{
			ObservedCall::shots_enemies,
			ObservedCall::fireballs,
			ObservedCall::players_bullets,
			ObservedCall::main_frame_footer_reached,
		},
		"the normal late suffix reaches its footer after all three phase views"
	);
	check(
		recording.shots_enemies_context == th03::MainPhaseContext{
			.round_id = 1U,
			.round_or_result_frame = 99U,
			.playfield_fg_shift_x = {6, -6},
		} &&
		recording.fireballs_context == recording.shots_enemies_context &&
		recording.players_bullets_context == recording.shots_enemies_context,
		"all observations precede result control and the late frame-counter increment"
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
		check(mima.calls.size() == 19U &&
				  mima.calls[13U] == ObservedCall::player_pose_committed &&
				  mima.calls[14U] == ObservedCall::player_pose_committed &&
				  mima.calls[15U] == ObservedCall::shots_enemies &&
				  mima.calls[16U] == ObservedCall::fireballs &&
				  mima.calls[17U] == ObservedCall::players_bullets &&
				  mima.calls[18U] == ObservedCall::main_frame_footer_reached,
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
		recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
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
			recording.phases == std::vector<ObservedPhase>{
				ObservedPhase::shots_enemies,
				ObservedPhase::fireballs,
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
		recording.main_frame_footers.empty(),
		"recorded Cancel defers player updates, phase views, and the footer"
	);
	const auto completed = game->finish_deferred_frame(
		th03::DeferredFrameAction::resume,
		&recording
	);
	check(
		completed && completed->status == th03::StepStatus::running &&
		recording.poses.size() == 2U &&
		recording.main_frame_footers ==
			std::vector<th03::MainFrameFooterReached>{{}} &&
		recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
			ObservedPhase::players_bullets,
		},
		"finish_deferred_frame reports all delayed phase views once"
	);
	const auto phase_count = recording.phases.size();
	const auto call_count = recording.calls.size();
	check(
		!game->finish_deferred_frame(
			th03::DeferredFrameAction::resume,
			&recording
		) && recording.phases.size() == phase_count &&
		recording.calls.size() == call_count,
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
		exit_recording.main_frame_footers ==
			std::vector<th03::MainFrameFooterReached>{{}} &&
		exit_recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
			ObservedPhase::players_bullets,
		},
		"deferred exit_session reports all phase views once before terminal"
	);
	const auto exit_phase_count = exit_recording.phases.size();
	const auto exit_call_count = exit_recording.calls.size();
	check(
		!exit_game->finish_deferred_frame(
			th03::DeferredFrameAction::exit_session,
			&exit_recording
		) && exit_recording.phases.size() == exit_phase_count &&
		exit_recording.calls.size() == exit_call_count,
		"deferred exit_session cannot duplicate observer callbacks"
	);
	const auto already_complete = exit_game->step(
		{}, th03::InputContext::gameplay, &exit_recording
	);
	check(
		already_complete.status == th03::StepStatus::terminal &&
		exit_recording.phases.size() == exit_phase_count &&
		exit_recording.calls.size() == exit_call_count,
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
			invalid_recording.main_frame_footers.empty() &&
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
		check(
			th03::detail::gameplay_frame_suffix_before_interstitial(
				state, &recording
			) &&
				recording.warning_interstitial_starts ==
					std::vector<th03::WarningInterstitialStarted>{
						{.flashing_players = {true, false}},
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
		recording.phases.size() == 3U && recording.poses.size() == 2U,
		"gauge release frame reports its ordinary phase views before arming warning"
	);
	recording.phases.clear();
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
		recording.poses.size() == 4U &&
		recording.warning_interstitial_starts ==
			std::vector<th03::WarningInterstitialStarted>{
				{.flashing_players = {true, false}},
			} &&
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
			recording.warning_interstitial_samples.size() ==
				static_cast<std::size_t>(sample) + 1U &&
			recording.warning_interstitial_samples.back().ordinal == sample,
			"each nonfinal warning input latch emits its source ordinal"
		);
	}
	const auto calls_before_final_sample = recording.calls.size();
	const auto completed = game->step(
		{}, th03::InputContext::interstitial, &recording
	);
	check(
		completed.status == th03::StepStatus::phase_transition &&
		recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
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
			ObservedCall::players_bullets,
			ObservedCall::main_frame_footer_reached,
		},
		"warning sample 26 precedes its deferred suffix and normal footer"
	);
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
				ObservedCall::players_bullets,
				ObservedCall::main_frame_footer_reached,
			},
			"Bomb frame facts precede the retained character-specific common-effect steps"
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
	test_observer_invariance_and_phase_order();
	test_public_step_result_invariance();
	test_shot_spawn_and_reuse();
	test_player_pose_and_defeat_lifecycle();
	test_bullet16_lifecycle_styles_order_and_full_arena();
	test_post_hittest_fireball_view_and_terminal_order();
	test_recorded_pause_completion();
	test_recorded_pause_prefix_bullet_once();
	test_warning_completion();
	test_main_effect_observer_contract();

	if(failures != 0) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << "All " << checks << " observer checks passed\n";
	return 0;
}
