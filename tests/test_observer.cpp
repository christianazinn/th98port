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
};

class DiscardObserver final : public th03::GameplayObserver {
};

class RecordingObserver final : public th03::GameplayObserver {
public:
	RecordingObserver()
	{
		phases.reserve(8U);
		spawns.reserve(8U);
	}

	void shot_pair_spawned(const th03::ShotPairSpawned &spawn) noexcept override
	{
		spawns.push_back(spawn);
	}

	void observe(const th03::ShotsEnemiesView &view) noexcept override
	{
		phases.push_back(ObservedPhase::shots_enemies);
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

	std::vector<ObservedPhase> phases;
	std::vector<th03::ShotPairSpawned> spawns;
	th03::MainPhaseContext shots_enemies_context{};
	th03::MainPhaseContext fireballs_context{};
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
	check(
		!shots_enemies.shot(static_cast<th03::ShotPairSlot>(255U)) &&
		!shots_enemies.enemy(static_cast<th03::EnemySlot>(255U)) &&
		!fireballs.fireball(static_cast<th03::FireballSlot>(255U)),
		"out-of-range typed slots return no fact instead of indexing the pool"
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
		},
		"ordinary late suffix reports ShotsEnemies then Fireballs exactly once"
	);
	check(
		recording.shots_enemies_context == th03::MainPhaseContext{
			.round_id = 1U,
			.round_or_result_frame = 99U,
			.playfield_fg_shift_x = {6, -6},
		} &&
		recording.fireballs_context == recording.shots_enemies_context,
		"both observations precede the late frame-counter increment"
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
		},
		"both phase callbacks precede terminal session-exit completion"
	);
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
		recording.phases.empty(),
		"recorded Cancel defers both observer phase views"
	);
	const auto completed = game->finish_deferred_frame(
		th03::DeferredFrameAction::resume,
		&recording
	);
	check(
		completed && completed->status == th03::StepStatus::running &&
		recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
		},
		"finish_deferred_frame reports both delayed phase views once"
	);
	const auto phase_count = recording.phases.size();
	check(
		!game->finish_deferred_frame(
			th03::DeferredFrameAction::resume,
			&recording
		) && recording.phases.size() == phase_count,
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
		exit_recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
		},
		"deferred exit_session reports both phase views once before terminal"
	);
	const auto exit_phase_count = exit_recording.phases.size();
	check(
		!exit_game->finish_deferred_frame(
			th03::DeferredFrameAction::exit_session,
			&exit_recording
		) && exit_recording.phases.size() == exit_phase_count,
		"deferred exit_session cannot duplicate observer callbacks"
	);
	const auto already_complete = exit_game->step(
		{}, th03::InputContext::gameplay, &exit_recording
	);
	check(
		already_complete.status == th03::StepStatus::terminal &&
		exit_recording.phases.size() == exit_phase_count,
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
			invalid_recording.spawns.empty(),
			"an invalid input context emits no observer callback"
		);
	}
}

void test_warning_completion()
{
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
		recording.phases.size() == 2U,
		"gauge release frame reports its ordinary phase views before arming warning"
	);
	recording.phases.clear();
	const auto warning = game->step(
		th03::CoreInput{.p1 = 0x0020U},
		th03::InputContext::gameplay,
		&recording
	);
	check(
		warning.status == th03::StepStatus::phase_transition &&
		recording.phases.empty() && recording.spawns.size() == 1U,
		"warning-start gameplay call reports its shot spawn before deferring both phase views"
	);
	for(std::size_t sample = 0U; sample < 26U; ++sample) {
		const auto interstitial = game->step(
			{}, th03::InputContext::interstitial, &recording
		);
		check(
			interstitial.status == th03::StepStatus::running &&
			recording.phases.empty(),
			"warning interstitial samples 1 through 26 emit no phase view"
		);
	}
	const auto completed = game->step(
		{}, th03::InputContext::interstitial, &recording
	);
	check(
		completed.status == th03::StepStatus::phase_transition &&
		recording.phases == std::vector<ObservedPhase>{
			ObservedPhase::shots_enemies,
			ObservedPhase::fireballs,
		},
		"warning sample 27 reports both deferred phase views once"
	);
}

} // namespace

int main()
{
	test_indexed_views_and_classifications();
	test_observer_invariance_and_phase_order();
	test_public_step_result_invariance();
	test_shot_spawn_and_reuse();
	test_post_hittest_fireball_view_and_terminal_order();
	test_recorded_pause_completion();
	test_warning_completion();

	if(failures != 0) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << "All " << checks << " observer checks passed\n";
	return 0;
}
