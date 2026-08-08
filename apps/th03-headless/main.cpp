#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/core/game.hpp"
#include "th03/formats/enedat.hpp"
#include "th03/formats/t3case.hpp"
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

} // namespace

int main(int argc, char **argv)
{
	if(argc != 3 && argc != 5) {
		std::cerr << "usage: th03-headless (--case|--v11) <file> [--enedat <file>]\n";
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
	if(argc == 5) {
		if(std::string_view(argv[3]) != "--enedat") {
			std::cerr << "error=expected --enedat before gameplay-data path\n";
			return 2;
		}
		const auto enedat_bytes = read_file(argv[4]);
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
	}
	auto game = th03::Game::create(session->init, std::move(content));
	if(!game) {
		print_error(game.error());
		return 2;
	}
	auto simulation = std::move(*game);
	const auto initial_state = simulation.canonical_state();
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
			if((consumed & 127U) == 0U) {
				const auto &state = result.input_boundary_state;
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
