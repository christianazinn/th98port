#include <algorithm>
#include <charconv>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/core/game.hpp"
#include "th03/formats/enedat.hpp"
#include "th03/formats/t3case.hpp"
#include "th03/formats/t3split.hpp"
#include "th03/replay/session.hpp"

namespace {

constexpr std::uint16_t input_up = 0x0001U;
constexpr std::uint16_t input_down = 0x0002U;
constexpr std::uint16_t input_shot = 0x0020U;
constexpr std::uint16_t input_cancel = 0x1000U;
constexpr std::uint16_t input_ok = 0x2000U;
constexpr std::uint16_t input_q = 0x4000U;

enum class ReplayPausePhase : std::uint8_t {
	inactive,
	wait_initial_release,
	menu,
	wait_menu_release,
	wait_resume_release,
};

class ReplayPauseCoordinator {
public:
	void begin() noexcept
	{
		phase_ = ReplayPausePhase::wait_initial_release;
		selection_ = 0U;
	}

	bool active() const noexcept
	{
		return phase_ != ReplayPausePhase::inactive;
	}

	void reset() noexcept
	{
		phase_ = ReplayPausePhase::inactive;
		selection_ = 0U;
	}

	std::optional<th03::DeferredFrameAction> sample(
		const th03::CoreInput &input
	) noexcept
	{
		const bool held = input.p1 != 0U || input.p2 != 0U ||
			input.system != 0U;
		switch(phase_) {
		case ReplayPausePhase::wait_initial_release:
			if(!held) {
				phase_ = ReplayPausePhase::menu;
			}
			return std::nullopt;
		case ReplayPausePhase::wait_menu_release:
			if(!held) {
				phase_ = ReplayPausePhase::menu;
			}
			return std::nullopt;
		case ReplayPausePhase::wait_resume_release:
			if(held) {
				return std::nullopt;
			}
			phase_ = ReplayPausePhase::inactive;
			return th03::DeferredFrameAction::resume;
		case ReplayPausePhase::menu:
			break;
		case ReplayPausePhase::inactive:
			return std::nullopt;
		}
		if((input.system & input_q) != 0U) {
			phase_ = ReplayPausePhase::inactive;
			return th03::DeferredFrameAction::exit_session;
		}
		if((input.system & input_cancel) != 0U) {
			phase_ = ReplayPausePhase::wait_resume_release;
			return std::nullopt;
		}
		if((input.system & input_up) != 0U) {
			selection_ = (selection_ == 0U) ? 3U :
				static_cast<std::uint8_t>(selection_ - 1U);
			phase_ = ReplayPausePhase::wait_menu_release;
			return std::nullopt;
		}
		if((input.system & input_down) != 0U) {
			selection_ = (selection_ == 3U) ? 0U :
				static_cast<std::uint8_t>(selection_ + 1U);
			phase_ = ReplayPausePhase::wait_menu_release;
			return std::nullopt;
		}
		if((input.system & (input_ok | input_shot)) == 0U) {
			return std::nullopt;
		}
		if(selection_ == 0U) {
			phase_ = ReplayPausePhase::wait_resume_release;
			return std::nullopt;
		}
		phase_ = ReplayPausePhase::inactive;
		return th03::DeferredFrameAction::exit_session;
	}

private:
	ReplayPausePhase phase_ = ReplayPausePhase::inactive;
	std::uint8_t selection_ = 0U;
};

std::vector<std::byte> read_file(const std::string &path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if(!input) {
		return {};
	}
	const auto end = input.tellg();
	if(end < 0) {
		return {};
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(end));
	input.seekg(0);
	if(!bytes.empty()) {
		input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
	if(!input) {
		return {};
	}
	return bytes;
}

void print_error(const th03::Error &error)
{
	std::cerr << "error=" << error.message << "\noffset=" << error.offset << '\n';
}

void print_score(const std::array<std::uint8_t, 8> &score)
{
	for(auto digit = score.rbegin(); digit != score.rend(); ++digit) {
		std::cout << static_cast<unsigned int>(*digit);
	}
}

std::optional<std::uint32_t> parse_u32(std::string_view text)
{
	std::uint32_t value = 0U;
	const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
	if(result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
		return std::nullopt;
	}
	return value;
}

th03::formats::PackedScore pack_score(
	const std::array<std::uint8_t, 8> &score
)
{
	th03::formats::PackedScore packed{};
	for(std::size_t index = 0U; index < packed.bytes.size(); ++index) {
		packed.bytes[index] = static_cast<std::uint8_t>(
			score[index * 2U] | (score[(index * 2U) + 1U] << 4U)
		);
	}
	return packed;
}

th03::formats::SplitRow split_row(
	th03::formats::SplitEvent event,
	const th03::CanonicalState &state,
	std::optional<std::uint8_t> reported_route = std::nullopt
)
{
	th03::formats::SplitRow row{};
	row.event = event;
	row.route = reported_route.value_or(state.route);
	row.game_mode = state.game_mode;
	row.story_stage = state.story_stage;
	row.round_id = state.round_id;
	row.winner = state.winner;
	row.round_speed = state.round_speed;
	row.global_frame = state.global_frame;
	row.round_frame = state.round_frame;
	row.round_or_result_frame = state.round_or_result_frame;
	row.p1_score = pack_score(state.p1_score);
	row.p2_score = pack_score(state.p2_score);
	row.resident_rand = state.resident_rand;
	row.state_hash = state.state_hash;
	return row;
}

int report_oracle_mismatch(const th03::formats::SplitComparison &comparison)
{
	std::cout << "oracle_status=different\n";
	if(comparison.first_mismatch) {
		std::cout << "oracle_row=" << comparison.first_mismatch->canonical_row
			<< "\noracle_field=" << comparison.first_mismatch->field
			<< "\nexpected=" << comparison.first_mismatch->expected
			<< "\nactual=" << comparison.first_mismatch->actual << '\n';
	}
	return 6;
}

bool oracle_prefix_matches(
	const std::optional<th03::formats::SplitStream> &oracle,
	const th03::formats::SplitStream &candidate
)
{
	if(!oracle) {
		return true;
	}
	const auto comparison = th03::formats::compare_oracle_splits(
		*oracle, candidate, true
	);
	if(
		comparison.status == th03::formats::ComparisonStatus::different ||
		!comparison.terminal_hash_differences.empty() ||
		comparison.checkpoint_global_delta.value_or(0) != 0
	) {
		(void)report_oracle_mismatch(comparison);
		return false;
	}
	return true;
}

void append_route_row(
	th03::formats::SplitStream &candidate,
	const th03::CanonicalState &state
)
{
	candidate.rows.push_back(split_row(th03::formats::SplitEvent::route, state));
}

void append_finish_row(
	th03::formats::SplitStream &candidate,
	const th03::CanonicalState &state
)
{
	candidate.rows.push_back(split_row(th03::formats::SplitEvent::finish, state));
}

void append_repeat_route_row(
	th03::formats::SplitStream &candidate,
	const th03::CanonicalState &state
)
{
	candidate.rows.push_back(split_row(
		th03::formats::SplitEvent::route, state
	));
}

void print_terminal(
	std::uint32_t consumed,
	const th03::CanonicalState &state
)
{
	std::cout << "result=terminal\n"
		<< "consumed_samples=" << consumed << '\n'
		<< "route=" << static_cast<unsigned int>(state.route) << '\n'
		<< "winner=" << static_cast<int>(state.winner) << '\n'
		<< "round_frame=" << state.round_frame << '\n'
		<< "round_or_result_frame=" << state.round_or_result_frame
		<< "\np1_score=";
	print_score(state.p1_score);
	std::cout << "\np2_score=";
	print_score(state.p2_score);
	std::cout << "\nstate_hash=" << std::uppercase << std::hex
		<< std::setw(8) << std::setfill('0') << state.state_hash
		<< std::dec << '\n';
}

} // namespace

int main(int argc, char **argv)
{
	if(argc < 3 || ((argc - 3) % 2) != 0) {
		std::cerr << "usage: th03-headless --case <file> "
			"[--enedat <file>] [--oracle <T3SPLIT.BIN>] "
			"[--trace-every <samples>]\n";
		return 2;
	}
	const std::string_view mode(argv[1]);
	const std::string path(argv[2]);
	const auto bytes = read_file(path);
	if(bytes.empty()) {
		std::cerr << "error=could not read a non-empty input file\n";
		return 2;
	}

	std::expected<th03::replay::ReplaySession, th03::Error> session =
		std::unexpected(th03::Error{th03::ErrorCode::invalid_value, "unknown mode"});
	if(mode == "--case") {
		const auto parsed = th03::formats::parse_t3case(bytes);
		if(!parsed) {
			print_error(parsed.error());
			return 2;
		}
		session = th03::formats::make_replay_session(*parsed);
	} else {
		std::cerr << "error=unknown input mode\n";
		return 2;
	}
	if(!session) {
		print_error(session.error());
		return 2;
	}

	std::cout << "validation=ok\n"
		<< "source_version=" << session->source_version << '\n'
		<< "samples=" << session->sample_count << '\n'
		<< "records=" << session->records.size() << '\n';
	th03::GameContent content{};
	std::optional<th03::formats::SplitStream> oracle;
	std::uint32_t trace_every = 0U;
	for(int argument = 3; argument < argc; argument += 2) {
		const std::string_view option(argv[argument]);
		const std::string value(argv[argument + 1]);
		if(option == "--enedat") {
			const auto enedat_bytes = read_file(value);
			if(enedat_bytes.empty()) {
				std::cerr << "error=could not read a non-empty ENEDAT file\n";
				return 2;
			}
			auto parsed_content = th03::formats::parse_enedat(enedat_bytes);
			if(!parsed_content) {
				print_error(parsed_content.error());
				return 2;
			}
			content = std::move(*parsed_content);
			std::cout << "enemy_formations=" << content.enemy_formations.size() << '\n';
			continue;
		}
		if(option == "--trace-every") {
			const auto parsed_interval = parse_u32(value);
			if(!parsed_interval || *parsed_interval == 0U) {
				std::cerr << "error=trace interval must be a positive integer\n";
				return 2;
			}
			trace_every = *parsed_interval;
			continue;
		}
		if(option != "--oracle") {
			std::cerr << "error=unknown option\n";
			return 2;
		}
		const auto oracle_bytes = read_file(value);
		if(oracle_bytes.empty()) {
			std::cerr << "error=could not read a non-empty oracle file\n";
			return 2;
		}
		auto parsed_oracle = th03::formats::parse_t3split(oracle_bytes);
		if(!parsed_oracle) {
			print_error(parsed_oracle.error());
			return 2;
		}
		oracle = std::move(*parsed_oracle);
	}
	const auto game_options = th03::GameOptions{
		.input_protocol = th03::InputProtocol::recorded_replay,
	};
	auto game = th03::Game::create(session->init, content, game_options);
	if(!game) {
		print_error(game.error());
		return 2;
	}
	std::optional<th03::Game> simulation(std::move(*game));
	const auto initial_state = simulation->canonical_state();
	th03::formats::SplitStream candidate{};
	candidate.state_hash_schema = 3U;
	candidate.rows.push_back(split_row(
		th03::formats::SplitEvent::start, initial_state
	));
	// T3CASE reports its most recently returned source route on every row.
	// Route 1 therefore remains visible throughout the repeated active round,
	// independently of the core's active-round route sentinel.
	std::uint8_t reported_route = initial_state.route;
	std::cout << "initial_state_hash="
		<< std::uppercase << std::hex << std::setw(8) << std::setfill('0')
		<< initial_state.state_hash << std::dec << '\n';
	std::uint32_t consumed = 0U;
	auto process = th03::replay::ProcessId::main;
	bool main_terminal = false;
	std::optional<th03::GameInit> next_story_stage;
	std::optional<th03::CanonicalState> terminal_state;
	ReplayPauseCoordinator replay_pause;
	for(const auto &record : session->records) {
		if(const auto *sample = std::get_if<th03::replay::InputSample>(&record)) {
			if(process == th03::replay::ProcessId::mainl) {
				if(sample->phase != th03::replay::InputPhase::interstitial) {
					std::cout << "result=input-context-mismatch\n"
						<< "consumed_samples=" << consumed << '\n';
					return 4;
				}
				consumed++;
				continue;
			}
			if(main_terminal || !simulation) {
				std::cout << "result=process-boundary-mismatch\n"
					<< "consumed_samples=" << consumed << '\n';
				return 4;
			}
			const auto context =
				(sample->phase == th03::replay::InputPhase::gameplay) ?
				th03::InputContext::gameplay : th03::InputContext::interstitial;
			auto result = simulation->step(sample->input, context);
			if(result.status == th03::StepStatus::invalid_input_context) {
				std::cout << "result=input-context-mismatch\n"
					<< "consumed_samples=" << consumed << '\n'
					<< "recorded_context=" << (
						context == th03::InputContext::gameplay ?
						"gameplay" : "interstitial"
					) << '\n';
				return 4;
			}
			if(result.status == th03::StepStatus::simulation_not_implemented) {
				std::cout << "result=simulation-not-implemented\n"
					<< "consumed_samples=" << consumed << '\n'
					<< "boundary=gameplay-subsystem\n";
				return 3;
			}
			if(
				context == th03::InputContext::gameplay &&
				(sample->input.system & input_cancel) != 0U
			) {
				replay_pause.begin();
			} else if(
				context == th03::InputContext::interstitial &&
				replay_pause.active()
			) {
				if(const auto action = replay_pause.sample(sample->input)) {
					auto finished = simulation->finish_deferred_frame(*action);
					if(!finished) {
						print_error(finished.error());
						return 3;
					}
					result = *finished;
				}
			}
			consumed++;
			if(
				context == th03::InputContext::gameplay && trace_every != 0U &&
				(consumed % trace_every) == 0U
			) {
				const auto &state = result.input_boundary_state;
				std::cout << "trace=" << consumed
					<< " round_frame=" << state.round_frame
					<< " round_or_result_frame=" << state.round_or_result_frame
					<< " p1_score=";
				print_score(state.p1_score);
				std::cout << " p2_score=";
				print_score(state.p2_score);
				std::cout << " state_hash=" << std::uppercase << std::hex
					<< std::setw(8) << std::setfill('0') << state.state_hash
					<< std::dec << '\n';
			}
			if(
				context == th03::InputContext::gameplay &&
				(consumed & 127U) == 0U
			) {
				const auto &state = result.input_boundary_state;
				candidate.rows.push_back(split_row(
					th03::formats::SplitEvent::checkpoint, state, reported_route
				));
				if(!oracle_prefix_matches(oracle, candidate)) {
					return 6;
				}
				std::cout << "checkpoint=" << consumed
					<< " round_frame=" << state.round_frame
					<< " round_or_result_frame=" << state.round_or_result_frame
					<< " p1_score=";
				print_score(state.p1_score);
				std::cout << " p2_score=";
				print_score(state.p2_score);
				std::cout << " state_hash=" << std::uppercase << std::hex
					<< std::setw(8) << std::setfill('0') << state.state_hash
					<< std::dec << '\n';
			}
			if(result.status == th03::StepStatus::terminal) {
				const auto route_state = simulation->canonical_state();
				reported_route = route_state.route;
				append_route_row(candidate, route_state);
				auto finished = simulation->finish_main_process();
				if(!finished) {
					print_error(finished.error());
					return 3;
				}
				terminal_state = simulation->canonical_state();
				append_finish_row(candidate, *terminal_state);
				if(!oracle_prefix_matches(oracle, candidate)) {
					return 6;
				}
				const auto transition = simulation->session_transition();
				if(!transition) {
					print_error(transition.error());
					return 3;
				}
				if(transition->action == th03::SessionAction::start_gameplay) {
					next_story_stage = transition->resident;
				}
				main_terminal = true;
			}
			if(result.status == th03::StepStatus::round_transition) {
				const auto completed_round = simulation->canonical_state();
				reported_route = completed_round.route;
				append_repeat_route_row(candidate, completed_round);
				if(!oracle_prefix_matches(oracle, candidate)) {
					return 6;
				}
				auto repeated = simulation->start_next_round();
				if(!repeated) {
					print_error(repeated.error());
					return 3;
				}
				const auto repeated_state = simulation->canonical_state();
				auto round_start_row = split_row(
					th03::formats::SplitEvent::round_start,
					repeated_state,
					reported_route
				);
				candidate.rows.push_back(round_start_row);
				if(!oracle_prefix_matches(oracle, candidate)) {
					return 6;
				}
				std::cout << "round_start="
					<< static_cast<unsigned int>(repeated_state.round_id)
					<< " global_frame=" << repeated_state.global_frame
					<< " state_hash=" << std::uppercase << std::hex
					<< std::setw(8) << std::setfill('0')
					<< repeated_state.state_hash << std::dec << '\n';
			}
			continue;
		}

		const auto &boundary = std::get<th03::replay::ProcessBoundary>(record);
		if(boundary.sample_index != consumed || boundary.completed_process != process) {
			std::cout << "result=process-boundary-mismatch\n"
				<< "consumed_samples=" << consumed << '\n';
			return 4;
		}
		if(process == th03::replay::ProcessId::main) {
			if(replay_pause.active()) {
				auto finished = simulation->finish_deferred_frame(
					th03::DeferredFrameAction::exit_session
				);
				if(!finished) {
					print_error(finished.error());
					return 3;
				}
				replay_pause.reset();
			}
			if(!main_terminal) {
				const auto route_state = simulation->canonical_state();
				reported_route = route_state.route;
				append_route_row(candidate, route_state);
				auto finished = simulation->finish_main_process();
				if(!finished) {
					print_error(finished.error());
					return 3;
				}
				terminal_state = simulation->canonical_state();
				append_finish_row(candidate, *terminal_state);
				if(!oracle_prefix_matches(oracle, candidate)) {
					return 6;
				}
				main_terminal = true;
			}
			process = th03::replay::ProcessId::mainl;
			continue;
		}

		if(!main_terminal) {
			std::cout << "result=process-boundary-mismatch\n"
				<< "consumed_samples=" << consumed << '\n';
			return 4;
		}
		if(!next_story_stage) {
			const auto transition = simulation->session_transition();
			if(
				!transition ||
				transition->action == th03::SessionAction::start_gameplay
			) {
				std::cout << "result=process-boundary-mismatch\n"
					<< "consumed_samples=" << consumed << '\n';
				return 4;
			}
			continue;
		}
		next_story_stage->global_frame = consumed;
		auto next_game = th03::Game::create(
			*next_story_stage, content, game_options
		);
		if(!next_game) {
			print_error(next_game.error());
			return 3;
		}
		simulation = std::move(*next_game);
		const auto state = simulation->canonical_state();
		// A new MAIN process starts a new T3CASE session and clears last_route.
		reported_route = state.route;
		candidate.rows.push_back(split_row(
			th03::formats::SplitEvent::round_start, state, reported_route
		));
		if(!oracle_prefix_matches(oracle, candidate)) {
			return 6;
		}
		std::cout << "round_start=" << static_cast<unsigned int>(state.story_stage)
			<< " global_frame=" << state.global_frame
			<< " state_hash=" << std::uppercase << std::hex
			<< std::setw(8) << std::setfill('0') << state.state_hash
			<< std::dec << '\n';
		process = th03::replay::ProcessId::main;
		main_terminal = false;
		next_story_stage.reset();
		terminal_state.reset();
	}
	if(!terminal_state || consumed != session->sample_count) {
		std::cout << "result=input-exhausted\nconsumed_samples=" << consumed << '\n';
		return 5;
	}
	if(oracle) {
		const auto comparison = th03::formats::compare_oracle_splits(
			*oracle, candidate, false
		);
		if(
			comparison.status != th03::formats::ComparisonStatus::same ||
			!comparison.terminal_hash_differences.empty() ||
			comparison.checkpoint_global_delta.value_or(0) != 0
		) {
			return report_oracle_mismatch(comparison);
		}
		std::cout << "oracle_status=same\noracle_rows="
			<< comparison.candidate_canonical_rows << '/'
			<< comparison.base_canonical_rows << '\n';
	}
	print_terminal(consumed, *terminal_state);
	return 0;
}
