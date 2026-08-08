#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "th03/formats/t3case.hpp"
#include "th03/formats/t3split.hpp"
#include "th03/formats/v11.hpp"

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

int fail(const th03::Error &error)
{
	std::cerr << "status=invalid\nerror=" << error.message
		<< "\noffset=" << error.offset << '\n';
	return 1;
}

const char *status_text(th03::formats::ComparisonStatus status)
{
	switch(status) {
	case th03::formats::ComparisonStatus::same: return "same";
	case th03::formats::ComparisonStatus::same_prefix: return "same-prefix";
	case th03::formats::ComparisonStatus::different: return "different";
	}
	return "different";
}

} // namespace

int main(int argc, char **argv)
{
	if(argc < 3) {
		std::cerr << "usage: th03-replay-check (--case|--v11|--split) <file>\n"
			<< "       th03-replay-check --compare-splits <base> <candidate> [--allow-candidate-prefix]\n";
		return 2;
	}
	const std::string_view mode(argv[1]);
	if(mode == "--compare-splits") {
		if(argc < 4 || argc > 5) {
			return 2;
		}
		const auto base_bytes = read_file(argv[2]);
		const auto candidate_bytes = read_file(argv[3]);
		if(base_bytes.empty() || candidate_bytes.empty()) {
			std::cerr << "error=could not read split input\n";
			return 2;
		}
		const auto base = th03::formats::parse_t3split(base_bytes);
		if(!base) {
			return fail(base.error());
		}
		const auto candidate = th03::formats::parse_t3split(candidate_bytes);
		if(!candidate) {
			return fail(candidate.error());
		}
		const bool allow_prefix = argc == 5 &&
			std::string_view(argv[4]) == "--allow-candidate-prefix";
		const auto comparison = th03::formats::compare_oracle_splits(
			*base, *candidate, allow_prefix
		);
		std::cout << "status=" << status_text(comparison.status) << '\n'
			<< "canonical_rows=" << comparison.compared_canonical_rows << '\n'
			<< "terminal_hash_differences="
			<< comparison.terminal_hash_differences.size() << '\n';
		if(comparison.checkpoint_global_delta) {
			std::cout << "checkpoint_global_delta="
				<< *comparison.checkpoint_global_delta << '\n';
		}
		if(comparison.first_mismatch) {
			std::cout << "first_mismatch_row="
				<< comparison.first_mismatch->canonical_row << '\n'
				<< "first_mismatch_field="
				<< comparison.first_mismatch->field << '\n';
		}
		return comparison.status == th03::formats::ComparisonStatus::different ? 1 : 0;
	}

	if(argc != 3) {
		return 2;
	}
	const auto bytes = read_file(argv[2]);
	if(bytes.empty()) {
		std::cerr << "error=could not read a non-empty input file\n";
		return 2;
	}
	if(mode == "--case") {
		const auto parsed = th03::formats::parse_t3case(bytes);
		if(!parsed) {
			return fail(parsed.error());
		}
		std::cout << "status=valid\nformat=T3CASE1\n"
			<< "case_id=" << parsed->header.case_id << '\n'
			<< "source_kind=" << static_cast<int>(parsed->header.source_kind) << '\n'
			<< "samples=" << parsed->header.sample_count << '\n'
			<< "records=" << parsed->header.record_count << '\n';
		return 0;
	}
	if(mode == "--v11") {
		const auto parsed = th03::formats::parse_v11_replay(bytes);
		if(!parsed) {
			return fail(parsed.error());
		}
		std::cout << "status=valid\nformat=T3RPLY11\n"
			<< "samples=" << parsed->sample_count << '\n'
			<< "records=" << parsed->records.size() << '\n';
		return 0;
	}
	if(mode == "--split") {
		const auto parsed = th03::formats::parse_t3split(bytes);
		if(!parsed) {
			return fail(parsed.error());
		}
		std::cout << "status=valid\nformat=T3SPLT1\n"
			<< "schema=" << parsed->state_hash_schema << '\n'
			<< "rows=" << parsed->rows.size() << '\n';
		return 0;
	}
	std::cerr << "error=unknown mode\n";
	return 2;
}
