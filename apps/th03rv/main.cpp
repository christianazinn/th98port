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
#include "th03/formats/v13.hpp"
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

const char *replay_status_name(th03::formats::ReplayStatus status) noexcept
{
	switch(status) {
	case th03::formats::ReplayStatus::empty:
		return "empty";
	case th03::formats::ReplayStatus::recording:
		return "recording";
	case th03::formats::ReplayStatus::finalized:
		return "finalized";
	case th03::formats::ReplayStatus::partial:
		return "partial";
	case th03::formats::ReplayStatus::error:
		return "error";
	}
	return "unknown";
}

const char *end_reason_name(th03::formats::ReplayEndReason reason) noexcept
{
	switch(reason) {
	case th03::formats::ReplayEndReason::none:
		return "none";
	case th03::formats::ReplayEndReason::complete:
		return "complete";
	case th03::formats::ReplayEndReason::menu_return:
		return "menu-return";
	case th03::formats::ReplayEndReason::input_end:
		return "input-end";
	case th03::formats::ReplayEndReason::partial:
		return "partial";
	case th03::formats::ReplayEndReason::error:
		return "error";
	case th03::formats::ReplayEndReason::game_over:
		return "game-over";
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
	if(expected != nullptr && expected->real_frames) {
		std::cout << " real_frames=" << *expected->real_frames;
	}
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

void print_v13_metadata(const th03::formats::ReplayDocument &document)
{
	if(!document.metadata) {
		return;
	}
	const auto &metadata = *document.metadata;
	std::cout << "replay_status=" << replay_status_name(metadata.status) << '\n'
		<< "end_reason=" << end_reason_name(metadata.end_reason) << '\n'
		<< "replay_name=";
	auto name_length = metadata.name.size();
	while(
		name_length != 0U &&
		(metadata.name[name_length - 1U] == ' ' ||
		 metadata.name[name_length - 1U] == '\0')
	) {
		--name_length;
	}
	if(name_length == 0U) {
		std::cout << "(unnamed)";
	} else {
		for(std::size_t index = 0U; index < name_length; ++index) {
			std::cout << metadata.name[index];
		}
	}
	std::cout << '\n'
		<< "dos_date=" << metadata.dos_date << '\n'
		<< "final_score=";
	print_packed_score(metadata.final_score);
	std::cout << '\n';
	if(document.stage_clear_bonuses) {
		for(
			std::size_t stage = 0U;
			stage < document.stage_clear_bonuses->size();
			++stage
		) {
			const auto &bonus = (*document.stage_clear_bonuses)[stage];
			const bool empty = bonus.total == std::array<std::uint8_t, 4>{} &&
				bonus.max_combo == 0U && bonus.gauge_attacks == 0U &&
				bonus.boss_attacks == 0U && bonus.boss_reversals == 0U &&
				bonus.boss_panics == 0U && bonus.remaining_lives == 0U;
			if(empty) {
				continue;
			}
			std::cout << "clear_bonus_stage=" << (stage + 1U) << " total=";
			print_packed_score(bonus.total);
			std::cout << " max_combo=" << static_cast<unsigned int>(bonus.max_combo)
				<< " gauge_attacks="
				<< static_cast<unsigned int>(bonus.gauge_attacks)
				<< " boss_attacks="
				<< static_cast<unsigned int>(bonus.boss_attacks)
				<< " boss_reversals="
				<< static_cast<unsigned int>(bonus.boss_reversals)
				<< " boss_panics="
				<< static_cast<unsigned int>(bonus.boss_panics)
				<< " remaining_lives="
				<< static_cast<unsigned int>(bonus.remaining_lives) << '\n';
		}
	}
	if(document.slowdown) {
		const auto timed = document.slowdown->timed_frames;
		const auto slow = document.slowdown->slow_frames;
		std::cout << "timed_frames=" << timed << '\n'
			<< "slow_frames=" << slow << '\n';
		if(timed != 0U) {
			const auto basis_points =
				(static_cast<std::uint64_t>(slow) * 10'000U) / timed;
			std::cout << "slowdown_basis_points=" << basis_points << '\n';
		}
	}
}

} // namespace

int main(int argc, char **argv)
{
	const bool roundtrip_only = argc == 3 &&
		std::string_view(argv[2]) == "--roundtrip-only";
	if(argc != 2 && argc != 4 && !roundtrip_only) {
		std::cerr << "usage: th03rv <replay> [--enedat <ENEDAT.DAT> | "
			"--roundtrip-only]\n";
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
	if(roundtrip_only) {
		if(document->session.source_version !=
			th03::formats::replay_v13_version) {
			std::cerr << "error=roundtrip requires V13\n";
			return 2;
		}
		auto written = th03::formats::write_v13_replay(*document);
		if(!written) {
			std::cerr << "error=" << written.error().message
				<< "\noffset=" << written.error().offset << '\n';
			return 2;
		}
		auto reparsed = th03::formats::parse_v13_replay(*written);
		auto rewritten = reparsed
			? th03::formats::write_v13_replay(*reparsed)
			: std::expected<std::vector<std::byte>, th03::Error>{
				std::unexpected(reparsed.error())
			};
		if(!rewritten || *rewritten != *written) {
			std::cerr << "error=canonical V13 output is not parse/write stable\n";
			return 2;
		}
		const bool exact = *written == replay_bytes;
		std::cout << "roundtrip=" << (exact ? "exact" : "canonical")
			<< "\nbyte_exact=" << (exact ? "yes" : "no")
			<< "\nsource_bytes=" << replay_bytes.size()
			<< "\nwritten_bytes=" << written->size() << '\n';
		return 0;
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
	std::vector<th03::replay::RoundStartRecord> expected_round_starts;
	expected_round_starts.reserve(document->checkpoints.size());
	std::optional<th03::Error> checkpoint_construction_error;
	std::size_t checkpoint_construction_index = 0U;
	const auto replay_options = th03::GameOptions{
		.input_protocol = th03::InputProtocol::recorded_replay,
	};
	for(const auto &checkpoint : document->checkpoints) {
		auto checkpoint_game = th03::Game::create(
			checkpoint.init, *content, replay_options
		);
		if(!checkpoint_game) {
			checkpoint_construction_error = checkpoint_game.error();
			break;
		}
		expected_round_starts.push_back(th03::replay::RoundStartRecord{
			.sample_index = checkpoint.sample_index,
			.observation = checkpoint_game->round_start_observation(),
		});
		checkpoint_construction_index++;
	}

	std::cout << "replay_version=" << document->session.source_version << '\n'
		<< "mode=" << (document->practice ? "practice" :
			(document->session.init.game_mode == th03::game_mode::story
				? "story" : "versus")) << '\n'
		<< "samples=" << document->session.sample_count << '\n'
		<< "checkpoints=" << document->checkpoints.size() << '\n'
		<< "expected_splits=" << document->expected_round_splits.size() << '\n'
		<< "enedat=" << enedat_path->string() << '\n';
	print_v13_metadata(*document);

	auto run = th03::replay::run_session(document->session, *content);
	if(!run) {
		std::cerr << "error=" << run.error().message
			<< "\noffset=" << run.error().offset << '\n';
		return 3;
	}
	const auto checkpoint_mismatch = checkpoint_construction_error
		? std::optional<th03::replay::RoundStartMismatch>{}
		: th03::replay::first_round_start_mismatch(
			expected_round_starts, run->round_starts
		);
	if(checkpoint_construction_error) {
		const auto &checkpoint =
			document->checkpoints[checkpoint_construction_index];
		std::cout << "checkpoint_verification=mismatch"
			<< " checkpoint=" << (checkpoint_construction_index + 1U)
			<< " sample=" << checkpoint.sample_index
			<< " field=runtime.reconstruction"
			<< " detail=" << checkpoint_construction_error->message << '\n';
	} else if(checkpoint_mismatch) {
		std::cout << "checkpoint_verification=mismatch"
			<< " checkpoint=" << (checkpoint_mismatch->checkpoint_index + 1U)
			<< " sample=" << checkpoint_mismatch->sample_index
			<< " field=" << checkpoint_mismatch->field;
		if(checkpoint_mismatch->actual_sample_index) {
			std::cout << " actual_sample="
				<< *checkpoint_mismatch->actual_sample_index;
		}
		std::cout << '\n';
	} else {
		std::cout << "checkpoint_verification=match\n";
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
		th03::formats::replay_can_be_verified(*document) &&
		run->status == th03::replay::RunStatus::complete && all_splits_match &&
		!checkpoint_construction_error && !checkpoint_mismatch;
	std::cout << "verification=" << (verified ? "match" : "mismatch") << '\n';
	return verified ? 0 : 1;
}
