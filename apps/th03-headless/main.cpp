#include <algorithm>
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
#include "th03/formats/v11.hpp"
#include "th03/replay/session.hpp"

namespace {

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
	const th03::CanonicalState &state
)
{
	th03::formats::SplitRow row{};
	row.event = event;
	row.route = state.route;
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

} // namespace

int main(int argc, char **argv)
{
	if(argc < 3 || ((argc - 3) % 2) != 0) {
		std::cerr << "usage: th03-headless (--case|--v11) <file> "
			"[--enedat <file>] [--oracle <T3SPLIT.BIN>]\n";
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
	} else if(mode == "--v11") {
		session = th03::formats::parse_v11_replay(bytes);
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
	auto game = th03::Game::create(session->init, std::move(content));
	if(!game) {
		print_error(game.error());
		return 2;
	}
	auto simulation = std::move(*game);
	const auto initial_state = simulation.canonical_state();
	th03::formats::SplitStream candidate{};
	candidate.state_hash_schema = 3U;
	candidate.rows.push_back(split_row(
		th03::formats::SplitEvent::start, initial_state
	));
	std::cout << "initial_state_hash="
		<< std::uppercase << std::hex << std::setw(8) << std::setfill('0')
		<< initial_state.state_hash << std::dec << '\n';
	std::uint32_t consumed = 0U;
	for(const auto &record : session->records) {
		if(const auto *sample = std::get_if<th03::replay::InputSample>(&record)) {
			const auto context =
				(sample->phase == th03::replay::InputPhase::gameplay) ?
				th03::InputContext::gameplay : th03::InputContext::interstitial;
			const auto result = simulation.step(sample->input, context);
			if(result.status == th03::StepStatus::invalid_input_context) {
				std::cout << "result=input-context-mismatch\n"
					<< "consumed_samples=" << consumed << '\n';
				return 4;
			}
			if(result.status == th03::StepStatus::simulation_not_implemented) {
				std::cout << "result=simulation-not-implemented\n"
					<< "consumed_samples=" << consumed << '\n'
					<< "boundary=gameplay-subsystem\n";
				return 3;
			}
			consumed++;
			if(
				context == th03::InputContext::gameplay &&
				(consumed & 127U) == 0U
			) {
				const auto &state = result.input_boundary_state;
				candidate.rows.push_back(split_row(
					th03::formats::SplitEvent::checkpoint, state
				));
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
				const auto state = simulation.canonical_state();
				auto route_row = split_row(
					th03::formats::SplitEvent::route, state
				);
				candidate.rows.push_back(route_row);
				// MAIN's outer entry point stores round_frame in resident->rand
				// after t3case_route() and before t3case_finish().
				route_row.event = th03::formats::SplitEvent::finish;
				route_row.resident_rand = static_cast<std::int32_t>(state.round_frame);
				candidate.rows.push_back(route_row);
				if(oracle) {
					const auto comparison = th03::formats::compare_oracle_splits(
						*oracle, candidate, true
					);
					if(
						comparison.status == th03::formats::ComparisonStatus::different ||
						!comparison.terminal_hash_differences.empty() ||
						comparison.checkpoint_global_delta != 0
					) {
						return report_oracle_mismatch(comparison);
					}
					const auto expected_terminal = std::find_if(
						oracle->rows.begin(), oracle->rows.end(),
						[&](const th03::formats::SplitRow &row) {
							return row.event == th03::formats::SplitEvent::route &&
								row.story_stage == state.story_stage &&
								row.round_id == state.round_id;
						}
					);
					if(
						expected_terminal == oracle->rows.end() ||
						expected_terminal->global_frame != state.global_frame
					) {
						std::cout << "oracle_status=different\n"
							<< "oracle_field=terminal_global_frame\n";
						return 6;
					}
					std::cout << "oracle_status=" << (
						comparison.status == th03::formats::ComparisonStatus::same ?
						"same" : "same-prefix"
					) << "\noracle_rows=" << comparison.candidate_canonical_rows
						<< '/' << comparison.base_canonical_rows << '\n';
				}
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
				return 0;
			}
		}
	}
	std::cout << "result=input-exhausted\nconsumed_samples=" << consumed << '\n';
	return 5;
}
