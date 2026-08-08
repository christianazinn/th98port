#include <cstddef>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "th03/core/game.hpp"
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

} // namespace

int main(int argc, char **argv)
{
	if(argc != 3) {
		std::cerr << "usage: th03-headless (--case|--v11) <file>\n";
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
	const auto game = th03::Game::create(session->init);
	if(!game) {
		print_error(game.error());
		return 2;
	}
	auto simulation = *game;
	th03::CoreInput first_input{};
	for(const auto &record : session->records) {
		if(const auto *sample = std::get_if<th03::replay::InputSample>(&record)) {
			first_input = sample->input;
			break;
		}
	}
	const auto result = simulation.step(first_input);
	if(result.status == th03::StepStatus::simulation_not_implemented) {
		std::cout << "result=simulation-not-implemented\n";
		return 3;
	}
	return 0;
}
