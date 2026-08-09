#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "th03/formats/enedat.hpp"
#include "th03/formats/replay.hpp"
#include "th03/replay/run.hpp"

namespace {

std::vector<std::byte> read_file(const std::filesystem::path &path)
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
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	return input ? bytes : std::vector<std::byte>{};
}

void add_enedat_candidates(
	std::vector<std::filesystem::path> &paths,
	std::filesystem::path start
)
{
	if(start.empty()) {
		return;
	}
	start = std::filesystem::absolute(start);
	if(!std::filesystem::is_directory(start)) {
		start = start.parent_path();
	}
	for(auto cursor = start; !cursor.empty(); cursor = cursor.parent_path()) {
		paths.push_back(cursor / "ENEDAT.DAT");
		paths.push_back(cursor / "originals" / "th03" / "dat" / "ENEDAT.DAT");
		if(cursor == cursor.root_path()) {
			break;
		}
	}
}

std::optional<std::filesystem::path> find_enedat(
	const std::optional<std::filesystem::path> &explicit_path,
	const std::filesystem::path &replay_path,
	const std::filesystem::path &executable_path
)
{
	if(explicit_path) {
		return explicit_path;
	}
	std::vector<std::filesystem::path> candidates;
#if defined(_MSC_VER)
	char *environment = nullptr;
	std::size_t environment_size = 0U;
	if(
		_dupenv_s(&environment, &environment_size, "TH03_ENEDAT") == 0 &&
		environment_size > 1U
	) {
		candidates.emplace_back(environment);
	}
	std::free(environment);
#else
	if(const auto *environment = std::getenv("TH03_ENEDAT")) {
		if(*environment != '\0') {
			candidates.emplace_back(environment);
		}
	}
#endif
	add_enedat_candidates(candidates, replay_path.parent_path());
	add_enedat_candidates(candidates, std::filesystem::current_path());
	add_enedat_candidates(candidates, executable_path.parent_path());
	for(const auto &candidate : candidates) {
		std::error_code error;
		if(std::filesystem::is_regular_file(candidate, error)) {
			return candidate;
		}
	}
	return std::nullopt;
}

const char *run_status_name(th03::replay::RunStatus status) noexcept
{
	switch(status) {
	case th03::replay::RunStatus::complete:
		return "complete";
	case th03::replay::RunStatus::input_context_mismatch:
		return "input-context-mismatch";
	case th03::replay::RunStatus::process_boundary_mismatch:
		return "process-boundary-mismatch";
	case th03::replay::RunStatus::simulation_not_implemented:
		return "simulation-not-implemented";
	case th03::replay::RunStatus::input_exhausted:
		return "input-exhausted";
	}
	return "unknown";
}

void print_packed_score(const std::array<std::uint8_t, 4> &score)
{
	for(auto byte = score.rbegin(); byte != score.rend(); ++byte) {
		std::cout << static_cast<unsigned int>((*byte >> 4U) & 0x0FU)
			<< static_cast<unsigned int>(*byte & 0x0FU);
	}
}

void print_split(
	std::size_t index,
	const th03::formats::ReplayRoundSplit &actual,
	const th03::formats::ReplayRoundSplit *expected
)
{
	const bool matches = expected != nullptr &&
		th03::formats::round_splits_match(*expected, actual);
	std::cout << "split=" << (index + 1U)
		<< " stage=" << static_cast<unsigned int>(actual.story_stage)
		<< " round=" << static_cast<unsigned int>(actual.round_id)
		<< " route=" << static_cast<unsigned int>(actual.route)
		<< " winner=" << static_cast<int>(actual.winner)
		<< " p1=";
	print_packed_score(actual.p1_score);
	std::cout << " p2=";
	print_packed_score(actual.p2_score);
	std::cout << " match=" << (matches ? "yes" : "no");
	if(expected != nullptr && !matches) {
		std::cout << " expected_stage="
			<< static_cast<unsigned int>(expected->story_stage)
			<< " expected_round="
			<< static_cast<unsigned int>(expected->round_id)
			<< " expected_route="
			<< static_cast<unsigned int>(expected->route)
			<< " expected_winner=" << static_cast<int>(expected->winner)
			<< " expected_p1=";
		print_packed_score(expected->p1_score);
		std::cout << " expected_p2=";
		print_packed_score(expected->p2_score);
	}
	std::cout << '\n';
}

} // namespace

int main(int argc, char **argv)
{
	if(argc != 2 && argc != 4) {
		std::cerr << "usage: th03rv <replay> [--enedat <ENEDAT.DAT>]\n";
		return 2;
	}
	const std::filesystem::path replay_path(argv[1]);
	std::optional<std::filesystem::path> requested_enedat;
	if(argc == 4) {
		if(std::string_view(argv[2]) != "--enedat") {
			std::cerr << "error=unknown option\n";
			return 2;
		}
		requested_enedat = std::filesystem::path(argv[3]);
	}

	const auto replay_bytes = read_file(replay_path);
	if(replay_bytes.empty()) {
		std::cerr << "error=could not read a non-empty replay\n";
		return 2;
	}
	auto document = th03::formats::parse_replay(replay_bytes);
	if(!document) {
		std::cerr << "error=" << document.error().message
			<< "\noffset=" << document.error().offset << '\n';
		return 2;
	}

	const auto enedat_path = find_enedat(
		requested_enedat, replay_path, std::filesystem::path(argv[0])
	);
	if(!enedat_path) {
		std::cerr << "error=could not locate ENEDAT.DAT; pass --enedat or set "
			"TH03_ENEDAT\n";
		return 2;
	}
	const auto enedat_bytes = read_file(*enedat_path);
	if(enedat_bytes.empty()) {
		std::cerr << "error=could not read a non-empty ENEDAT.DAT\n";
		return 2;
	}
	auto content = th03::formats::parse_enedat(enedat_bytes);
	if(!content) {
		std::cerr << "error=" << content.error().message
			<< "\noffset=" << content.error().offset << '\n';
		return 2;
	}

	std::cout << "replay_version=" << document->session.source_version << '\n'
		<< "mode=" << (document->practice ? "practice" :
			(document->session.init.game_mode == th03::game_mode::story
				? "story" : "versus")) << '\n'
		<< "samples=" << document->session.sample_count << '\n'
		<< "expected_splits=" << document->expected_round_splits.size() << '\n'
		<< "enedat=" << enedat_path->string() << '\n';

	auto run = th03::replay::run_session(document->session, *content);
	if(!run) {
		std::cerr << "error=" << run.error().message
			<< "\noffset=" << run.error().offset << '\n';
		return 3;
	}
	bool all_splits_match =
		run->completed_rounds.size() == document->expected_round_splits.size();
	for(std::size_t index = 0U; index < run->completed_rounds.size(); ++index) {
		const auto actual = th03::formats::make_round_split(
			run->completed_rounds[index].state, document->practice
		);
		const auto *expected = (index < document->expected_round_splits.size())
			? &document->expected_round_splits[index] : nullptr;
		print_split(index, actual, expected);
		if(expected == nullptr || !th03::formats::round_splits_match(*expected, actual)) {
			all_splits_match = false;
		}
	}
	for(
		std::size_t index = run->completed_rounds.size();
		index < document->expected_round_splits.size();
		++index
	) {
		std::cout << "split=" << (index + 1U) << " missing=yes match=no\n";
	}

	std::cout << "run_status=" << run_status_name(run->status) << '\n'
		<< "consumed_samples=" << run->consumed_samples << '\n';
	const bool verified =
		run->status == th03::replay::RunStatus::complete && all_splits_match;
	std::cout << "verification=" << (verified ? "match" : "mismatch") << '\n';
	return verified ? 0 : 1;
}
