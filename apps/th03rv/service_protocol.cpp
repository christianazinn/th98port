#include "service_protocol.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <sstream>

#include "th03/formats/v14.hpp"
#include "th03/app/new_classic_story_route.hpp"

namespace th03rv {
namespace {

std::optional<std::uint64_t> score_value(
	const std::array<std::uint8_t, 8U> &digits
) noexcept
{
	std::uint64_t value = 0U;
	std::uint64_t place = 1U;
	for(const auto digit : digits) {
		if(digit > 9U) {
			return std::nullopt;
		}
		value += static_cast<std::uint64_t>(digit) * place;
		place *= 10U;
	}
	return value;
}

std::optional<std::string> display_name(
	const std::array<char, 8U> &source
)
{
	std::size_t length = source.size();
	while(length != 0U && (source[length - 1U] == ' ' ||
		source[length - 1U] == '\0')) {
		--length;
	}
	std::string result;
	result.reserve(length);
	for(std::size_t index = 0U; index < length; ++index) {
		const auto value = static_cast<unsigned char>(source[index]);
		const auto allowed =
			(value >= static_cast<unsigned char>('A') &&
			 value <= static_cast<unsigned char>('Z')) ||
			(value >= static_cast<unsigned char>('0') &&
			 value <= static_cast<unsigned char>('9')) ||
			value == static_cast<unsigned char>(' ') ||
			value == static_cast<unsigned char>('.') ||
			value == static_cast<unsigned char>(',') ||
			value == static_cast<unsigned char>('!') ||
			value == static_cast<unsigned char>('?');
		if(!allowed) {
			return std::nullopt;
		}
		result.push_back(source[index]);
	}
	return result;
}

std::string json_string(std::string_view value)
{
	std::string result{"\""};
	for(const auto character : value) {
		switch(character) {
		case '\\':
			result += "\\\\";
			break;
		case '"':
			result += "\\\"";
			break;
		default:
			result += character;
			break;
		}
	}
	result += '"';
	return result;
}

bool zero_scores(
	const std::array<std::array<std::uint8_t, 8U>, 2U> &scores
) noexcept
{
	return std::ranges::all_of(scores, [](const auto &score) {
		return std::ranges::all_of(score, [](const auto digit) {
			return digit == 0U;
		});
	});
}

bool stock_story_anchor(
	const th03::formats::ReplayDocument &document,
	const th03::replay::RunResult &run,
	const th03::GameContent &content
)
{
	if(document.checkpoints.empty() || run.round_starts.empty()) {
		return false;
	}
	const auto &checkpoint = document.checkpoints.front();
	const auto &init = document.session.init;
	const auto player = init.paletted_character[0U];
	const auto opponents = th03::app::new_classic_story_opponents(
		player, init.resident_rand
	);
	if(
		checkpoint.stage != 0U || checkpoint.round != 0U ||
		checkpoint.sample_index != 0U || init.global_frame != 0U ||
		init.game_mode != th03::game_mode::story || init.rank > 3U ||
		init.story_stage != 0U || init.story_lives != 2U ||
		init.remaining_credits != 3U ||
		init.skill != static_cast<std::uint8_t>(70U + (init.rank * 25U)) ||
		init.demo_number != 0U ||
		init.is_cpu != std::array<std::uint8_t, 2U>{0U, 1U} ||
		!opponents || init.story_opponents != *opponents ||
		init.paletted_character[1U] != init.story_opponents[0U] ||
		init.winner != 0 || init.show_score_menu != 0U ||
		init.fast_op_animation != 0U || !zero_scores(init.last_score) ||
		init.practice_rules
	) {
		return false;
	}

	// V14 checkpoint zero legitimately carries compact/runtime recipes, and its
	// top-level random_seed is the post-initialization live seed rather than the
	// pre-initialization resident seed. Rebuild the same launch without any
	// recipe, then compare the complete effective first-round observation. This
	// proves the live seed together with every ring and prevents a crafted
	// snapshot from injecting score, gauges, carry state, RNG tables, or other
	// non-stock state.
	auto stock = init;
	stock.runtime_snapshot.reset();
	stock.compact_runtime_snapshot.reset();
	stock.round_start_snapshot.reset();
	stock.practice_rules.reset();
	const auto options = th03::GameOptions{
		.input_protocol = th03::InputProtocol::recorded_replay,
	};
	auto game = th03::Game::create(stock, content, options);
	if(!game) {
		return false;
	}
	const std::array expected{th03::replay::RoundStartRecord{
		.sample_index = 0U,
		.observation = game->round_start_observation(),
	}};
	return !th03::replay::first_round_start_mismatch(
		expected, run.round_starts
	);
}

bool all_zero(std::span<const std::uint8_t> bytes) noexcept
{
	return std::ranges::all_of(bytes, [](const auto byte) {
		return byte == 0U;
	});
}

std::string uuid_string(const th03::formats::ReplayAccountUuid &uuid)
{
	constexpr char digits[] = "0123456789abcdef";
	std::string result;
	result.reserve(36U);
	for(std::size_t index = 0U; index < uuid.size(); ++index) {
		if(index == 4U || index == 6U || index == 8U || index == 10U) {
			result.push_back('-');
		}
		result.push_back(digits[uuid[index] >> 4U]);
		result.push_back(digits[uuid[index] & 0x0FU]);
	}
	return result;
}

bool stock_netplay_anchor(
	const th03::formats::ReplayDocument &document,
	const th03::replay::RunResult &run,
	const th03::GameContent &content
)
{
	if(document.checkpoints.empty() || run.round_starts.empty()) {
		return false;
	}
	const auto &checkpoint = document.checkpoints.front();
	const auto &init = document.session.init;
	if(
		checkpoint.stage != 0U || checkpoint.round != 0U ||
		checkpoint.sample_index != 0U || init.global_frame != 0U ||
		init.game_mode != th03::game_mode::vs_1p_2p || init.rank > 3U ||
		init.key_mode > 2U || init.story_stage != 0U ||
		init.story_lives != 0U || init.remaining_credits != 3U ||
		init.skill != 0U || init.demo_number != 0U ||
		init.is_cpu != std::array<std::uint8_t, 2U>{0U, 0U} ||
		init.paletted_character[0U] < 1 || init.paletted_character[0U] > 18 ||
		init.paletted_character[1U] < 1 || init.paletted_character[1U] > 18 ||
		!std::ranges::all_of(init.story_opponents, [](const auto opponent) {
			return opponent == 0;
		}) || init.winner != 0 || init.show_score_menu != 0U ||
		init.fast_op_animation != 0U || !zero_scores(init.last_score) ||
		init.practice_rules ||
		!th03::stock_gameplay_rules(init.gameplay_ruleset)
	) {
		return false;
	}

	auto stock = init;
	stock.runtime_snapshot.reset();
	stock.compact_runtime_snapshot.reset();
	stock.round_start_snapshot.reset();
	const auto options = th03::GameOptions{
		.input_protocol = th03::InputProtocol::recorded_replay,
	};
	auto game = th03::Game::create(stock, content, options);
	if(!game) {
		return false;
	}
	const std::array expected{th03::replay::RoundStartRecord{
		.sample_index = 0U,
		.observation = game->round_start_observation(),
	}};
	return !th03::replay::first_round_start_mismatch(
		expected, run.round_starts
	);
}

std::optional<ServiceAcceptedReport> stock_netplay_report(
	const th03::formats::ReplayDocument &document,
	const th03::replay::RunResult &run,
	const th03::GameContent &content
)
{
	const auto &metadata = *document.metadata;
	const bool netplay = (metadata.recording_flags &
		th03::formats::replay_recording_flag_netplay) != 0U;
	if(
		document.session.init.game_mode != th03::game_mode::vs_1p_2p ||
		!netplay || metadata.recording_flags !=
			th03::formats::replay_recording_flag_netplay ||
		(metadata.recorder_role != th03::formats::ReplayRecorderRole::player_one &&
		 metadata.recorder_role != th03::formats::ReplayRecorderRole::player_two) ||
		metadata.recorder_source != th03::formats::ReplayRecorderSource::native ||
		all_zero(metadata.player_uuid[0U]) ||
		all_zero(metadata.player_uuid[1U]) ||
		metadata.player_uuid[0U] == metadata.player_uuid[1U] ||
		all_zero(metadata.match_id) ||
		metadata.player_nametag_snapshot[0U].empty() ||
		metadata.player_nametag_snapshot[1U].empty() ||
		metadata.end_reason != th03::formats::ReplayEndReason::game_over ||
		run.terminal_state->route != 2U ||
		run.terminal_state->game_mode != th03::game_mode::vs_1p_2p ||
		run.terminal_state->winner < 0 || run.terminal_state->winner > 1 ||
		run.terminal_transition->action != th03::SessionAction::exit_to_operator ||
		run.terminal_transition->high_score_registration ||
		!stock_netplay_anchor(document, run, content)
	) {
		return std::nullopt;
	}
	const auto &summary = run.completed_rounds.back().result_summary;
	const auto winner = static_cast<std::size_t>(run.terminal_state->winner);
	if(!summary || !summary->summary_active || !summary->winner ||
		static_cast<std::size_t>(*summary->winner) != winner ||
		summary->rounds_won[winner] != 2U ||
		summary->rounds_won[1U - winner] > 1U) {
		return std::nullopt;
	}
	const auto recorder = metadata.recorder_role ==
		th03::formats::ReplayRecorderRole::player_one ? 0U : 1U;
	const auto score = score_value(recorder == 0U ?
		run.terminal_state->p1_score : run.terminal_state->p2_score);
	const auto name = display_name(metadata.name);
	const auto character = document.session.init.paletted_character[recorder];
	if(!score || !name || *score > 99'999'999U ||
		character < 1 || character > 18) {
		return std::nullopt;
	}
	return ServiceAcceptedReport{
		.display_name = *name,
		.mode = "vs_1p_2p",
		.rank = document.session.init.rank,
		.character = static_cast<std::uint8_t>((character - 1) / 2),
		.result_kind = "versus_complete",
		.score = *score * 10U,
		.end_stage = 0U,
		.continues_used = 0U,
		.autofire = document.session.init.autofire_mask,
		.timed_frames = document.slowdown->timed_frames,
		.slow_frames = document.slowdown->slow_frames,
		.dos_date = metadata.dos_date,
		.recorder_user_id = uuid_string(metadata.player_uuid[recorder]),
		.opponent_user_id = uuid_string(metadata.player_uuid[1U - recorder]),
	};
}

} // namespace

std::optional<ServiceAcceptedReport> make_service_accepted_report(
	const th03::formats::ReplayDocument &document,
	const th03::replay::RunResult &run,
	const th03::GameContent &content,
	bool splits_match,
	bool checkpoints_match
)
{
	if(
		!document.metadata || !document.slowdown || !splits_match ||
		!checkpoints_match ||
		document.session.source_version != th03::formats::replay_v14_version ||
		document.metadata->ruleset != th03::formats::ReplayRuleset::stock ||
		document.metadata->status != th03::formats::ReplayStatus::finalized ||
		document.practice || run.status != th03::replay::RunStatus::complete ||
		!run.terminal_state || !run.terminal_transition ||
		run.completed_rounds.empty() ||
		document.slowdown->slow_frames > document.slowdown->timed_frames
	) {
		return std::nullopt;
	}
	if(document.session.init.game_mode == th03::game_mode::vs_1p_2p) {
		return stock_netplay_report(document, run, content);
	}
	if(document.session.init.game_mode != th03::game_mode::story ||
		!stock_story_anchor(document, run, content)) {
		return std::nullopt;
	}
	const auto player = document.session.init.paletted_character[0U];
	const auto score = score_value(run.terminal_state->p1_score);
	const auto name = display_name(document.metadata->name);
	if(!score || !name || *score > 99'999'999U) {
		return std::nullopt;
	}
	std::string result_kind;
	if(
		run.terminal_state->route == 2U &&
		run.terminal_state->story_stage == 8U &&
		run.terminal_state->winner == static_cast<std::int8_t>(th03::PlayerId::p1) &&
		run.terminal_transition->action == th03::SessionAction::ending &&
		document.metadata->end_reason == th03::formats::ReplayEndReason::complete
	) {
		result_kind = "clear";
	} else if(
		run.terminal_state->route == 2U &&
		run.terminal_state->winner ==
			static_cast<std::int8_t>(th03::PlayerId::p2) &&
		run.terminal_transition->high_score_registration &&
		(run.terminal_transition->action == th03::SessionAction::request_continue ||
		 run.terminal_transition->action == th03::SessionAction::exit_to_operator) &&
		document.metadata->end_reason == th03::formats::ReplayEndReason::game_over
	) {
		result_kind = "game_over";
	} else {
		return std::nullopt;
	}
	const auto initial_credits = document.session.init.remaining_credits;
	auto remaining_credits = initial_credits;
	for(const auto &start : run.round_starts) {
		remaining_credits = std::min(
			remaining_credits, start.observation.start.remaining_credits
		);
	}
	return ServiceAcceptedReport{
		.display_name = *name,
		.rank = document.session.init.rank,
		.character = static_cast<std::uint8_t>((player - 1) / 2),
		.result_kind = result_kind,
		// Source score digits represent tens; the visible/UI score appends 0.
		.score = *score * 10U,
		.end_stage = run.terminal_state->story_stage,
		.continues_used = static_cast<std::uint8_t>(
			initial_credits - remaining_credits
		),
		.autofire = document.session.init.autofire_mask,
		.timed_frames = document.slowdown->timed_frames,
		.slow_frames = document.slowdown->slow_frames,
		.dos_date = document.metadata->dos_date,
	};
}

std::string service_accepted_json(const ServiceAcceptedReport &report)
{
	std::ostringstream output;
	output << "{\"protocol\":" << json_string(service_protocol)
		<< ",\"outcome\":\"accepted\""
		<< ",\"sourceVersion\":14,\"finalized\":true"
		<< ",\"splitsMatch\":true,\"checkpointsMatch\":true"
		<< ",\"displayName\":" << json_string(report.display_name)
		<< ",\"category\":{\"mode\":" << json_string(report.mode)
		<< ",\"rank\":"
		<< static_cast<unsigned int>(report.rank)
		<< ",\"character\":" << static_cast<unsigned int>(report.character)
		<< ",\"startStage\":0,\"startRound\":0}"
		<< ",\"result\":{\"kind\":" << json_string(report.result_kind)
		<< ",\"score\":" << report.score
		<< ",\"endStage\":" << static_cast<unsigned int>(report.end_stage)
		<< ",\"continuesUsed\":"
		<< static_cast<unsigned int>(report.continues_used) << '}'
		<< ",\"autofire\":" << static_cast<unsigned int>(report.autofire)
		<< ",\"timedFrames\":" << report.timed_frames
		<< ",\"slowFrames\":" << report.slow_frames
		<< ",\"dosDate\":" << report.dos_date;
	if(report.recorder_user_id) {
		output << ",\"recorderUserId\":"
			<< json_string(*report.recorder_user_id);
	}
	if(report.opponent_user_id) {
		output << ",\"opponentUserId\":"
			<< json_string(*report.opponent_user_id);
	}
	output << '}';
	return output.str();
}

std::string service_rejected_json(std::string_view reason)
{
	return "{\"protocol\":" + json_string(service_protocol) +
		",\"outcome\":\"rejected\",\"reason\":" + json_string(reason) + '}';
}

std::string service_error_json()
{
	return "{\"protocol\":" + json_string(service_protocol) +
		",\"outcome\":\"error\"}";
}

} // namespace th03rv
