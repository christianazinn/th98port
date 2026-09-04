#include "th03/app/new_classic/replay/new_classic_replay.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <utility>
#include <variant>

#include "th03/formats/v14.hpp"

namespace th03::app {

std::optional<formats::ReplayAccountUuid>
parse_new_classic_replay_account_uuid(std::string_view text) noexcept
{
	if(text.size() != 36U || text[8U] != '-' || text[13U] != '-' ||
		text[18U] != '-' || text[23U] != '-') {
		return std::nullopt;
	}
	formats::ReplayAccountUuid uuid{};
	std::size_t output = 0U;
	std::uint8_t high = 0U;
	bool have_high = false;
	for(const char character : text) {
		if(character == '-') {
			continue;
		}
		const auto nibble = [&]() -> std::optional<std::uint8_t> {
			if(character >= '0' && character <= '9') {
				return static_cast<std::uint8_t>(character - '0');
			}
			if(character >= 'a' && character <= 'f') {
				return static_cast<std::uint8_t>(character - 'a' + 10);
			}
			if(character >= 'A' && character <= 'F') {
				return static_cast<std::uint8_t>(character - 'A' + 10);
			}
			return std::nullopt;
		}();
		if(!nibble) {
			return std::nullopt;
		}
		if(!have_high) {
			high = *nibble;
			have_high = true;
		} else {
			if(output >= uuid.size()) {
				return std::nullopt;
			}
			uuid[output++] = static_cast<std::uint8_t>((high << 4U) | *nibble);
			have_high = false;
		}
	}
	return (output == uuid.size() && !have_high) ?
		std::optional{uuid} : std::nullopt;
}

void preserve_new_classic_restart_random_seed(
	GameInit &resident,
	std::uint32_t live_random_seed
) noexcept
{
	resident.resident_rand = std::bit_cast<std::int32_t>(live_random_seed);
}

namespace {

[[nodiscard]] NewClassicReplayError replay_error(
	NewClassicReplayErrorCode code,
	std::optional<Error> format = std::nullopt
)
{
	return {
		.code = code,
		.format = std::move(format),
	};
}

[[nodiscard]] bool replay_game_mode(std::uint8_t mode) noexcept
{
	return mode == game_mode::story || mode == game_mode::vs_1p_cpu ||
		mode == game_mode::vs_1p_2p;
}

[[nodiscard]] std::array<std::uint8_t, 4U> pack_decimal(
	std::uint32_t value
) noexcept
{
	std::array<std::uint8_t, 4U> packed{};
	for(auto &byte : packed) {
		byte = static_cast<std::uint8_t>(
			(value % 10U) | (((value / 10U) % 10U) << 4U)
		);
		value /= 100U;
	}
	return packed;
}

[[nodiscard]] GameInit checkpoint_init(
	const RoundStartObservation &observation,
	bool practice
)
{
	auto init = observation.start;
	init.runtime_snapshot.reset();
	init.compact_runtime_snapshot = CompactRuntimeSnapshot{
		.round_reset_seed = observation.effective_reset_seed,
		.random_seed = observation.runtime.random_seed,
		.first_formation = observation.runtime.formation_type_ring[0U],
		.players = observation.runtime.players,
	};
	init.round_start_snapshot = observation.round;
	if(!practice) {
		init.practice_rules.reset();
	}
	return init;
}

[[nodiscard]] bool valid_practice_config(
	const formats::ReplayPracticeConfig &config
) noexcept
{
	const bool base = config.preset <= 1U && config.cpu_timer <= 2U &&
		config.round_speed <= 0x7FU && config.bullet_speed <= 0x7FU &&
		config.p1_spell >= 1U && config.p1_spell <= 16U &&
		config.cpu_spell >= 1U && config.cpu_spell <= 16U &&
		config.boss_level <= 15U && config.cpu_damage <= 3U &&
		config.p1_gauge >= 64U && config.cpu_gauge >= 64U;
	if(!base) {
		return false;
	}
	if(config.preset == 0U) {
		return config.stage == 0U && config.round == 0U &&
			config.stock == 0xFFU && config.extends_gained == 0U;
	}
	return config.stage < 9U && config.round <= 5U && config.stock <= 4U &&
		config.extends_gained <= 2U &&
		config.stock <= static_cast<std::uint8_t>(2U + config.extends_gained);
}

[[nodiscard]] bool practice_anchor_matches(
	const formats::ReplayPracticeConfig &config,
	const RoundStartObservation &observation
) noexcept
{
	if(!observation.start.practice_rules) {
		return false;
	}
	const auto &rules = *observation.start.practice_rules;
	const auto &state = observation.round.state;
	return observation.start.game_mode == game_mode::vs_1p_cpu &&
		observation.start.story_stage == config.stage &&
		observation.start.story_lives ==
			(config.stock == 0xFFU ? 0U : config.stock) &&
		state.round_id == config.round &&
		state.round_speed == config.round_speed &&
		state.bullet_base_speed == config.bullet_speed &&
		state.gauge_attack_level == std::array<std::uint8_t, 2U>{
			config.p1_spell, config.cpu_spell,
		} && state.boss_attack_level == config.boss_level &&
		state.cpu_hit_damage_additional == config.cpu_damage &&
		state.extends_gained ==
			(config.extends_gained >= 2U ? 0xFFU : config.extends_gained) &&
		state.cpu_safety_frames == std::array<std::uint16_t, 2U>{
			config.initial_cpu_safety_frames,
			config.initial_cpu_safety_frames,
		} && observation.runtime.players[0U].gauge_avail ==
			static_cast<std::uint16_t>(config.p1_gauge) * 16U &&
		observation.runtime.players[1U].gauge_avail ==
			static_cast<std::uint16_t>(config.cpu_gauge) * 16U &&
		rules.uses_stock == (config.stock != 0xFFU) &&
		rules.retry_cpu_timer == static_cast<CpuTimerPolicy>(config.cpu_timer) &&
		rules.retry_cpu_damage == config.cpu_damage;
}

[[nodiscard]] formats::ReplayMetadata initial_metadata(
	const GameInit &init,
	std::uint16_t dos_date,
	const NewClassicReplayIdentity &identity
)
{
	formats::ReplayMetadata metadata{};
	metadata.flags = 0x0003U;
	metadata.status = formats::ReplayStatus::recording;
	metadata.end_reason = formats::ReplayEndReason::none;
	metadata.game_mode = init.game_mode;
	metadata.rank = init.rank;
	metadata.key_mode = init.key_mode;
	metadata.paletted_character = init.paletted_character;
	metadata.story_stage = init.story_stage;
	metadata.is_cpu = init.is_cpu;
	metadata.resident_rand = init.resident_rand;
	metadata.random_seed = init.random_seed;
	metadata.summary_flags = 0x007FU;
	metadata.final_story_lives = init.story_lives;
	metadata.autofire = init.autofire_mask;
	metadata.dos_date = dos_date;
	metadata.ruleset = identity.ruleset;
	metadata.recording_flags = identity.recording_flags;
	metadata.recorder_role = identity.recorder_role;
	metadata.recorder_source = identity.recorder_source;
	metadata.player_uuid = identity.player_uuid;
	metadata.match_id = identity.match_id;
	metadata.player_nametag_snapshot = identity.player_nametag_snapshot;
	if(init.game_mode == game_mode::story) {
		formats::ReplayStorySummary story{};
		for(std::size_t stage = 0U; stage < story.stage_opponents.size(); ++stage) {
			story.stage_opponents[stage] = std::bit_cast<std::uint8_t>(
				init.story_opponents[stage]
			);
		}
		metadata.story = story;
	}
	return metadata;
}

} // namespace

bool NewClassicReplayRecorder::set_identity(
	NewClassicReplayIdentity identity
) noexcept
{
	if(completed_) {
		return false;
	}
	identity_ = identity;
	if(document_ && document_->metadata) {
		auto &metadata = *document_->metadata;
		metadata.ruleset = identity.ruleset;
		metadata.recording_flags = identity.recording_flags;
		metadata.recorder_role = identity.recorder_role;
		metadata.recorder_source = identity.recorder_source;
		metadata.player_uuid = identity.player_uuid;
		metadata.match_id = identity.match_id;
		metadata.player_nametag_snapshot = identity.player_nametag_snapshot;
	}
	return true;
}

std::expected<void, NewClassicReplayError>
NewClassicReplayRecorder::configure_practice(
	std::optional<formats::ReplayPracticeConfig> config
) noexcept
{
	if(!capture_enabled_) {
		return config ? std::unexpected(replay_error(
			NewClassicReplayErrorCode::invalid_sequence
		)) : std::expected<void, NewClassicReplayError>{};
	}
	if(document_ || completed_ || (config && !valid_practice_config(*config))) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::invalid_sequence
		));
	}
	practice_config_ = std::move(config);
	return {};
}

std::expected<void, NewClassicReplayError>
NewClassicReplayRecorder::round_started(
	const RoundStartObservation &observation,
	std::uint16_t dos_date
)
{
	if(!capture_enabled_) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::invalid_sequence
		));
	}
	const bool practice = practice_config_.has_value();
	if(completed_ || !replay_game_mode(observation.start.game_mode) ||
		observation.start.paletted_character[0U] < 1 ||
		observation.start.paletted_character[1U] < 1 ||
		observation.start.autofire_mask > 3U ||
		observation.runtime.formation_type_ring[0U] == 0xFFU ||
		(practice ? !practice_anchor_matches(*practice_config_, observation) :
			observation.start.practice_rules.has_value()) ||
		(practice && document_ && document_->checkpoints.size() >= 5U)) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::invalid_round_start
		));
	}
	if(!document_) {
		formats::ReplayDocument document{};
		document.session.source_version = formats::replay_v14_version;
		document.session.init = checkpoint_init(observation, practice);
		document.metadata = initial_metadata(
			document.session.init,
			dos_date != 0U ? dos_date : recording_date_,
			identity_
		);
		const bool extension_callbacks_complete =
			(extension_provider_.begin != nullptr) ==
			(extension_provider_.finish != nullptr);
		if(!extension_callbacks_complete ||
			(extension_provider_.begin && !extension_provider_.begin(
				extension_provider_.context,
				document.metadata->opaque_extension
			))) {
			return std::unexpected(replay_error(
				NewClassicReplayErrorCode::extension_failed
			));
		}
		document.stage_clear_bonuses =
			std::array<formats::ReplayStageClearBonus, 9U>{};
		document.slowdown = formats::ReplaySlowdown{};
		document.practice = practice;
		if(practice) {
			document.metadata->flags = 0x0007U;
			document.metadata->practice = *practice_config_;
		}
		document_ = std::move(document);
	} else if(document_->session.records.empty() ||
		document_->session.sample_count == 0U) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::invalid_sequence
		));
	}

	auto &document = *document_;
	const auto round = observation.round.state.round_id;
	const auto stage = (observation.start.game_mode == game_mode::story || practice)
		? observation.start.story_stage : static_cast<std::uint8_t>(0x0FU);
	auto init = checkpoint_init(observation, practice);
	init.global_frame = document.session.sample_count;
	document.checkpoints.push_back(formats::ReplayCheckpoint{
		.stage = stage,
		.round = round,
		.sample_index = document.session.sample_count,
		.global_frame = document.session.sample_count,
		.input_byte_cursor = 0U,
		.init = std::move(init),
	});
	if(document.checkpoints.size() == 1U) {
		document.session.init = document.checkpoints.front().init;
	}
	round_vsync_last_ = source_vsync_clock_;
	round_real_frames_ = 0U;
	round_timed_frames_ = 0U;
	round_timer_active_ = true;
	return {};
}

std::expected<void, NewClassicReplayError> NewClassicReplayRecorder::sample(
	replay::InputPhase phase,
	CoreInput input
)
{
	if(!document_ || completed_ || input.charge_mask > 3U) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::invalid_sequence
		));
	}
	auto &session = document_->session;
	session.records.push_back(replay::InputSample{
		.frame_index = session.sample_count,
		.phase = phase,
		.input = input,
		.round_frame = std::nullopt,
		.round_or_result_frame = std::nullopt,
	});
	++session.sample_count;
	document_->metadata->sample_count = session.sample_count;
	document_->metadata->final_frame_count = session.sample_count;
	return {};
}

void NewClassicReplayRecorder::source_vsync() noexcept
{
	++source_vsync_clock_;
}

void NewClassicReplayRecorder::frame_io(
	const StepResult &result,
	bool slowdown_eligible
) noexcept
{
	if(!document_ || completed_ || !round_timer_active_) {
		return;
	}
	if(result.defeat_none_before_frame) {
		round_real_frames_ += source_vsync_clock_ - round_vsync_last_;
	}
	round_vsync_last_ = source_vsync_clock_;
	if(slowdown_eligible && result.defeat_none_after_frame) {
		++round_timed_frames_;
		slow_sample_pending_ = true;
	}
	// replay_frame_io() samples real time before the player update. The first
	// explosion update begins with DF_NONE and is included, then changes the
	// post-frame state to DF_EXPLODE and closes both future timing domains.
	if(!result.defeat_none_after_frame) {
		round_timer_active_ = false;
	}
}

void NewClassicReplayRecorder::classify_pending_frame(
	bool deadline_missed
) noexcept
{
	if(slow_sample_pending_ && deadline_missed && document_ &&
		document_->slowdown) {
		++document_->slowdown->slow_frames;
	}
	slow_sample_pending_ = false;
}

std::expected<void, NewClassicReplayError>
NewClassicReplayRecorder::round_completed(
	const CanonicalState &state,
	std::optional<std::uint32_t> real_frames_override,
	std::optional<MainResultState> result_state
)
{
	if(!document_ || completed_ || document_->checkpoints.empty() ||
		state.game_mode != document_->metadata->game_mode || state.route > 2U ||
		state.winner < 0 || state.winner > 1) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::invalid_sequence
		));
	}
	if(last_terminal_ && last_terminal_->global_frame == state.global_frame &&
		last_terminal_->round_id == state.round_id &&
		last_terminal_->story_stage == state.story_stage) {
		return {};
	}
	auto split = formats::make_round_split(state, document_->practice);
	split.real_frames = real_frames_override ? real_frames_override :
		std::optional{round_real_frames_};
	document_->expected_round_splits.push_back(split);
	last_terminal_ = state;
	if(document_->slowdown) {
		document_->slowdown->timed_frames += round_timed_frames_;
	}
	if(result_state && document_->stage_clear_bonuses && state.route == 2U &&
		(state.game_mode == game_mode::story || document_->practice) &&
		state.story_stage < document_->stage_clear_bonuses->size() &&
		result_state->summary_active && result_state->winner == PlayerId::p1 &&
		result_state->story_stage == state.story_stage) {
		const auto &stats = result_state->winner_stats;
		auto &bonus = (*document_->stage_clear_bonuses)[state.story_stage];
		bonus.max_combo = stats.combo_hits_max;
		bonus.gauge_attacks = stats.gauge_attacks_fired;
		bonus.boss_attacks = stats.boss_attacks_fired;
		bonus.boss_reversals = stats.boss_attacks_reversed;
		bonus.boss_panics = stats.boss_panics_fired;
		bonus.remaining_lives = state.story_stage == 8U ?
			result_state->story_lives : 0U;
		const auto total =
			(static_cast<std::uint32_t>(bonus.max_combo) * 1'000U) +
			(static_cast<std::uint32_t>(bonus.gauge_attacks) * 10'000U) +
			(static_cast<std::uint32_t>(bonus.boss_attacks) * 15'000U) +
			(static_cast<std::uint32_t>(bonus.boss_reversals) * 20'000U) +
			(static_cast<std::uint32_t>(bonus.boss_panics) * 30'000U) +
			(static_cast<std::uint32_t>(bonus.remaining_lives) * 100'000U);
		bonus.total = pack_decimal(total);
	}
	round_timer_active_ = false;
	if(document_->metadata->story && state.story_stage < 9U) {
		document_->metadata->stage_reached_count = std::max(
			document_->metadata->stage_reached_count,
			static_cast<std::uint8_t>(state.story_stage + 1U)
		);
		document_->metadata->story->stage_scores[state.story_stage] = split.p1_score;
	}
	return {};
}

std::expected<void, NewClassicReplayError>
NewClassicReplayRecorder::process_completed(replay::ProcessId process)
{
	if(!document_ || completed_ || document_->session.sample_count == 0U) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::invalid_sequence
		));
	}
	if(!document_->session.records.empty()) {
		if(const auto *boundary = std::get_if<replay::ProcessBoundary>(
			&document_->session.records.back()
		); boundary && boundary->completed_process == process &&
			boundary->sample_index == document_->session.sample_count) {
			return {};
		}
	}
	document_->session.records.push_back(replay::ProcessBoundary{
		.sample_index = document_->session.sample_count,
		.completed_process = process,
	});
	return {};
}

std::expected<void, NewClassicReplayError> NewClassicReplayRecorder::finish(
	formats::ReplayEndReason reason,
	const GameInit &resident
)
{
	if(!document_ || completed_ || reason == formats::ReplayEndReason::none ||
		document_->session.records.empty() ||
		!std::holds_alternative<replay::ProcessBoundary>(
			document_->session.records.back()
		)) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::invalid_sequence
		));
	}
	auto candidate = *document_;
	auto &metadata = *candidate.metadata;
	metadata.status = formats::ReplayStatus::finalized;
	metadata.end_reason = reason;
	metadata.final_story_lives = resident.story_lives;
	if(last_terminal_) {
		metadata.final_route = last_terminal_->route;
		metadata.final_game_mode = last_terminal_->game_mode;
		metadata.final_story_stage =
			last_terminal_->game_mode == game_mode::story
			? std::optional{last_terminal_->story_stage} : std::nullopt;
		metadata.final_round_id = last_terminal_->round_id;
		metadata.final_winner = last_terminal_->winner;
		const auto split = formats::make_round_split(
			*last_terminal_, document_->practice
		);
		metadata.final_score = split.p1_score;
	}
	auto encoded = formats::write_v14_replay(candidate);
	if(!encoded) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::document_invalid,
			std::move(encoded.error())
		));
	}
	if(extension_provider_.finish) {
		auto extension = metadata.opaque_extension;
		if(!extension_provider_.finish(
			extension_provider_.context, *encoded, extension
		)) {
			return std::unexpected(replay_error(
				NewClassicReplayErrorCode::extension_failed
			));
		}
		metadata.opaque_extension = extension;
		encoded = formats::write_v14_replay(candidate);
		if(!encoded) {
			return std::unexpected(replay_error(
				NewClassicReplayErrorCode::document_invalid,
				std::move(encoded.error())
			));
		}
	}
	auto parsed = formats::parse_v14_replay(*encoded);
	if(!parsed) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::document_invalid,
			std::move(parsed.error())
		));
	}
	completed_ = std::move(*parsed);
	clear_memory();
	return {};
}

bool NewClassicReplayRecorder::recording() const noexcept
{
	return document_.has_value();
}

const formats::ReplayDocument *NewClassicReplayRecorder::document() const noexcept
{
	return document_ ? &*document_ : nullptr;
}

const CanonicalState *NewClassicReplayRecorder::last_terminal() const noexcept
{
	return last_terminal_ ? &*last_terminal_ : nullptr;
}

std::optional<formats::ReplayDocument>
NewClassicReplayRecorder::take_completed()
{
	auto result = std::move(completed_);
	completed_.reset();
	return result;
}

void NewClassicReplayRecorder::clear_memory() noexcept
{
	document_.reset();
	last_terminal_.reset();
	round_timer_active_ = false;
	slow_sample_pending_ = false;
}

void NewClassicReplayRecorder::discard() noexcept
{
	clear_memory();
	completed_.reset();
}

} // namespace th03::app
