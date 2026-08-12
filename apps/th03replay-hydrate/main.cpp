#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "th03/formats/enedat.hpp"
#include "th03/tools/replay_hydrate.hpp"

namespace {

struct Options {
	std::filesystem::path input;
	std::filesystem::path enedat;
	std::filesystem::path output;
	bool force = false;
};

std::optional<Options> parse_options(int argc, char **argv)
{
	if(argc < 6) {
		return std::nullopt;
	}
	Options options{};
	options.input = argv[1];
	for(int index = 2; index < argc; ++index) {
		const std::string_view argument(argv[index]);
		if(argument == "--force") {
			options.force = true;
			continue;
		}
		if((argument == "--enedat" || argument == "--output" ||
			argument == "-o") && (index + 1) < argc) {
			const auto value = std::filesystem::path(argv[++index]);
			if(argument == "--enedat") {
				options.enedat = value;
			} else {
				options.output = value;
			}
			continue;
		}
		return std::nullopt;
	}
	if(options.input.empty() || options.enedat.empty() || options.output.empty()) {
		return std::nullopt;
	}
	return options;
}

std::vector<std::byte> read_file(const std::filesystem::path &path)
{
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if(!stream) {
		return {};
	}
	const auto end = stream.tellg();
	if(end <= 0) {
		return {};
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(end));
	stream.seekg(0);
	stream.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	if(!stream) {
		return {};
	}
	return bytes;
}

bool write_atomic(
	const std::filesystem::path &path,
	std::span<const std::byte> bytes,
	bool force
)
{
	std::error_code error;
	if(std::filesystem::exists(path, error) && !force) {
		std::cerr << "error=output already exists; pass --force to replace it\n";
		return false;
	}
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	auto temporary = path;
	temporary += ".tmp-" + std::to_string(stamp);
	{
		std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
		if(!stream) {
			std::cerr << "error=could not create temporary output\n";
			return false;
		}
		stream.write(
			reinterpret_cast<const char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
		stream.flush();
		if(!stream) {
			std::cerr << "error=could not write temporary output\n";
			stream.close();
			std::filesystem::remove(temporary, error);
			return false;
		}
	}
	if(force) {
		std::filesystem::remove(path, error);
		if(error) {
			std::cerr << "error=could not replace existing output\n";
			std::filesystem::remove(temporary, error);
			return false;
		}
	}
	std::filesystem::rename(temporary, path, error);
	if(error) {
		std::cerr << "error=could not finalize output\n";
		std::filesystem::remove(temporary, error);
		return false;
	}
	return true;
}

} // namespace

int main(int argc, char **argv)
{
	const auto options = parse_options(argc, argv);
	if(!options) {
		std::cerr << "usage: th03replay-hydrate <input.rpy> --enedat "
			"<ENEDAT.DAT> --output <output.rpy> [--force]\n";
		return 2;
	}
	const auto replay_bytes = read_file(options->input);
	if(replay_bytes.empty()) {
		std::cerr << "error=could not read a non-empty input replay\n";
		return 2;
	}
	const auto enedat_bytes = read_file(options->enedat);
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
	auto hydrated = th03::tools::hydrate_v13_replay(replay_bytes, *content);
	if(!hydrated) {
		std::cerr << "error=" << hydrated.error().message
			<< "\noffset=" << hydrated.error().offset << '\n';
		return 3;
	}
	if(!write_atomic(options->output, hydrated->bytes, options->force)) {
		return 4;
	}
	const auto &report = hydrated->report;
	std::cout << "source_version=" << report.source_version << '\n'
		<< "rounds_verified=" << report.rounds_verified << '\n'
		<< "round_timers_filled=" << report.round_timers_filled << '\n'
		<< "clear_bonuses_filled=" << report.clear_bonuses_filled << '\n'
		<< "slowdown_filled=" << (report.slowdown_filled ? "yes" : "no")
		<< '\n'
		<< "changed=" << (report.changed ? "yes" : "no") << '\n'
		<< "output=" << options->output.string() << '\n';
	return 0;
}
