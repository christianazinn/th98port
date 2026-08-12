#include "th03/app/new_classic_replay.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <utility>
#include <variant>

#include "th03/formats/v13.hpp"

namespace th03::app {
namespace {

[[nodiscard]] NewClassicReplayError replay_error(
	NewClassicReplayErrorCode code,
	std::optional<Error> format = std::nullopt
)
{
	return {.code = code, .format = std::move(format)};
}

[[nodiscard]] bool replay_game_mode(std::uint8_t mode) noexcept
{
	return mode == game_mode::story || mode == game_mode::vs_1p_cpu ||
		mode == game_mode::vs_1p_2p || mode == game_mode::vs_cpu_cpu;
}

[[nodiscard]] GameInit checkpoint_init(
	const RoundStartObservation &observation
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
	init.practice_rules.reset();
	return init;
}

[[nodiscard]] formats::ReplayMetadata initial_metadata(
	const GameInit &init,
	std::uint16_t dos_date
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

std::expected<void, NewClassicReplayError>
NewClassicReplayRecorder::round_started(
	const RoundStartObservation &observation,
	std::uint16_t dos_date
)
{
	if(completed_ || !replay_game_mode(observation.start.game_mode) ||
		observation.start.paletted_character[0U] < 1 ||
		observation.start.paletted_character[1U] < 1 ||
		observation.start.autofire_mask > 3U ||
		observation.runtime.formation_type_ring[0U] == 0xFFU) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::invalid_round_start
		));
	}
	if(!document_) {
		formats::ReplayDocument document{};
		document.session.source_version = formats::replay_v13_version;
		document.session.init = checkpoint_init(observation);
		document.metadata = initial_metadata(
			document.session.init,
			dos_date != 0U ? dos_date : recording_date_
		);
		document.stage_clear_bonuses =
			std::array<formats::ReplayStageClearBonus, 9U>{};
		document.slowdown = formats::ReplaySlowdown{};
		document_ = std::move(document);
	} else if(document_->session.records.empty() ||
		document_->session.sample_count == 0U) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::invalid_sequence
		));
	}

	auto &document = *document_;
	const auto round = observation.round.state.round_id;
	const auto stage = observation.start.game_mode == game_mode::story
		? observation.start.story_stage : static_cast<std::uint8_t>(0x0FU);
	auto init = checkpoint_init(observation);
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
	if(document.metadata->story) {
		document.metadata->stage_reached_count = std::max(
			document.metadata->stage_reached_count,
			static_cast<std::uint8_t>(stage + 1U)
		);
	}
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

std::expected<void, NewClassicReplayError>
NewClassicReplayRecorder::round_completed(
	const CanonicalState &state,
	std::optional<std::uint32_t> real_frames
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
	auto split = formats::make_round_split(state, false);
	split.real_frames = real_frames;
	document_->expected_round_splits.push_back(split);
	last_terminal_ = state;
	if(document_->metadata->story && state.route == 2U && state.story_stage < 9U) {
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
	auto &metadata = *document_->metadata;
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
		metadata.final_score = formats::make_round_split(
			*last_terminal_, false
		).p1_score;
	}
	auto encoded = formats::write_v13_replay(*document_);
	if(!encoded) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::document_invalid,
			std::move(encoded.error())
		));
	}
	auto parsed = formats::parse_v13_replay(*encoded);
	if(!parsed) {
		return std::unexpected(replay_error(
			NewClassicReplayErrorCode::document_invalid,
			std::move(parsed.error())
		));
	}
	completed_ = std::move(*parsed);
	document_.reset();
	last_terminal_.reset();
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

void NewClassicReplayRecorder::discard() noexcept
{
	document_.reset();
	completed_.reset();
	last_terminal_.reset();
}

} // namespace th03::app
