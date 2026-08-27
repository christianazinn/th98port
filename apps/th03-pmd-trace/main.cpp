#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "th03/app/content/main_compiled_content.hpp"
#include "th03/audio/pmd_opna.hpp"

namespace {

namespace fs = std::filesystem;
using th03::audio::PmdOpnaConfig;
using th03::audio::PmdOpnaEngine;
using th03::audio::PmdOpnaFidelity;
using th03::audio::PmdOpnaRegisterTraceSink;
using th03::audio::PmdOpnaRegisterWrite;
using th03::audio::PmdOpnaStereoSample;

constexpr std::uint32_t opna_clock_hz = 7'987'200U;
constexpr std::uint32_t terminal_seconds = 5U;

struct Event final {
	std::string_view kind{};
	std::uint64_t sequence = 0U;
	std::uint64_t chip_clock = 0U;
};

struct Scenario final {
	std::uint8_t effect = 0U;
	std::vector<PmdOpnaRegisterWrite> writes{};
	std::array<Event, 4U> events{};
	std::uint64_t samples = 0U;
	std::uint32_t register_hash = 0U;
};

class Trace final : public PmdOpnaRegisterTraceSink {
public:
	void register_written(const PmdOpnaRegisterWrite &write) noexcept override
	{
		try {
			writes.push_back(write);
		} catch(const std::bad_alloc &) {
			allocation_failed = true;
		}
	}

	std::vector<PmdOpnaRegisterWrite> writes{};
	bool allocation_failed = false;
};

[[nodiscard]] int fail(std::string_view stage, std::size_t code = 0U)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode=" << code << '\n';
	return 1;
}

void usage()
{
	std::cout
		<< "usage: th03-pmd-trace --song <OP.M> --efc <YUME.EFC>\n"
		<< "       --output <directory> [--effect <1..22>]\n\n"
		<< "Writes deterministic native YM2608 register traces.\n";
}

[[nodiscard]] std::expected<std::vector<std::byte>, std::size_t> read_file(
	const fs::path &path,
	std::size_t maximum
)
{
	std::error_code system;
	const auto status = fs::status(path, system);
	if(system || !fs::is_regular_file(status)) {
		return std::unexpected(1U);
	}
	const auto size = fs::file_size(path, system);
	if(system || size == 0U || size > maximum) {
		return std::unexpected(2U);
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(3U);
	}
	std::vector<std::byte> result(static_cast<std::size_t>(size));
	input.read(reinterpret_cast<char *>(result.data()),
		static_cast<std::streamsize>(result.size()));
	if(!input || input.gcount() != static_cast<std::streamsize>(result.size())) {
		return std::unexpected(4U);
	}
	return result;
}

[[nodiscard]] std::string digest_hex(std::span<const std::byte> bytes)
{
	constexpr std::string_view hex = "0123456789ABCDEF";
	const auto digest = th03::app::app_sha256(bytes);
	std::string result{};
	result.reserve(digest.size() * 2U);
	for(const auto byte : digest) {
		result.push_back(hex[byte >> 4U]);
		result.push_back(hex[byte & 0x0FU]);
	}
	return result;
}

[[nodiscard]] std::string effect_name(std::uint8_t effect)
{
	std::string result{"effect-"};
	result.push_back(static_cast<char>('0' + (effect / 10U)));
	result.push_back(static_cast<char>('0' + (effect % 10U)));
	return result;
}

[[nodiscard]] std::expected<Scenario, std::size_t> run_scenario(
	std::span<const std::byte> song,
	std::span<const std::byte> bank,
	std::uint8_t effect
)
{
	auto created = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = opna_clock_hz,
		.fidelity = PmdOpnaFidelity::minimum,
	});
	if(!created) {
		return std::unexpected(1U);
	}
	auto engine = std::move(*created);
	Trace trace{};
	engine.set_register_trace_sink(&trace);
	if(!engine.load_song(song)) {
		return std::unexpected(2U);
	}
	if(!engine.load_effect_bank(bank)) {
		return std::unexpected(3U);
	}
	if(!engine.play()) {
		return std::unexpected(4U);
	}
	const auto played = engine.status();
	if(trace.allocation_failed) {
		return std::unexpected(6U);
	}
	const Event effect_call{
		"effect-call", played.register_writes, played.chip_clocks
	};
	if(!engine.play_effect(effect)) {
		return std::unexpected(5U);
	}
	const auto started = engine.status();
	if(trace.allocation_failed) {
		return std::unexpected(6U);
	}
	std::array<PmdOpnaStereoSample, 1'024U> scratch{};
	const auto deadline = started.samples_rendered +
		(static_cast<std::uint64_t>(started.native_sample_rate) * terminal_seconds);
	while(engine.status().active_effect &&
		engine.status().samples_rendered < deadline) {
		engine.render_native(scratch);
		if(trace.allocation_failed) {
			return std::unexpected(6U);
		}
	}
	const auto ended = engine.status();
	if(ended.active_effect) {
		return std::unexpected(7U);
	}
	Scenario result{
		.effect = effect,
		.writes = std::move(trace.writes),
		.events = {{
			{"play-complete", played.register_writes, played.chip_clocks},
			effect_call,
			{"effect-started", started.register_writes, started.chip_clocks},
			{"effect-ended", ended.register_writes, ended.chip_clocks},
		}},
		.samples = ended.samples_rendered,
		.register_hash = ended.register_hash,
	};
	return result;
}

[[nodiscard]] bool write_registers(
	const fs::path &path,
	const Scenario &scenario
)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return false;
	}
	output << "sequence\tchip_clock\taddress\tvalue\n" << std::uppercase
		<< std::hex << std::setfill('0');
	for(const auto &write : scenario.writes) {
		output << std::dec << write.sequence << '\t' << write.chip_clock << '\t'
			<< std::hex << std::setw(3) << write.address << '\t'
			<< std::setw(2) << static_cast<unsigned>(write.value) << '\n';
	}
	return static_cast<bool>(output);
}

[[nodiscard]] bool write_events(const fs::path &path, const Scenario &scenario)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return false;
	}
	output << "event\tsequence\tchip_clock\n";
	for(const auto &event : scenario.events) {
		output << event.kind << '\t' << event.sequence << '\t'
			<< event.chip_clock << '\n';
	}
	return static_cast<bool>(output);
}

[[nodiscard]] bool write_manifest(
	const fs::path &path,
	std::span<const std::byte> song,
	std::span<const std::byte> bank,
	std::span<const Scenario> scenarios
)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return false;
	}
	output << "{\n  \"format\": \"T3ATR001\",\n"
		<< "  \"opna_clock_hz\": " << opna_clock_hz << ",\n"
		<< "  \"fidelity\": \"minimum\",\n"
		<< "  \"song\": {\"bytes\": " << song.size()
		<< ", \"sha256\": \"" << digest_hex(song) << "\"},\n"
		<< "  \"efc\": {\"bytes\": " << bank.size()
		<< ", \"sha256\": \"" << digest_hex(bank) << "\"},\n"
		<< "  \"scenarios\": [\n";
	for(std::size_t index = 0U; index < scenarios.size(); ++index) {
		const auto &scenario = scenarios[index];
		const auto name = effect_name(scenario.effect);
		output << "    {\"id\": \"" << name << "\", \"effect\": "
			<< static_cast<unsigned>(scenario.effect)
			<< ", \"samples\": " << scenario.samples
			<< ", \"register_writes\": " << scenario.writes.size()
			<< ", \"register_hash\": " << scenario.register_hash
			<< ", \"registers\": \"" << name << "-registers.tsv\""
			<< ", \"events\": \"" << name << "-events.tsv\"}";
		if(index + 1U != scenarios.size()) {
			output << ',';
		}
		output << '\n';
	}
	output << "  ]\n}\n";
	return static_cast<bool>(output);
}

} // namespace

int main(int argc, char **argv)
{
	if(argc == 2 && std::string_view(argv[1]) == "--help") {
		usage();
		return 0;
	}
	if(argc != 7 && argc != 9) {
		usage();
		return 2;
	}
	fs::path song_path{};
	fs::path efc_path{};
	fs::path output_path{};
	std::uint8_t selected_effect = 0U;
	for(int index = 1; index < argc; index += 2) {
		const auto key = std::string_view(argv[index]);
		const auto value = std::string_view(argv[index + 1]);
		if(key == "--song" && song_path.empty()) {
			song_path = value;
		} else if(key == "--efc" && efc_path.empty()) {
			efc_path = value;
		} else if(key == "--output" && output_path.empty()) {
			output_path = value;
		} else if(key == "--effect" && selected_effect == 0U) {
			unsigned parsed = 0U;
			const auto converted = std::from_chars(
				value.data(), value.data() + value.size(), parsed
			);
			if(converted.ec != std::errc{} ||
				converted.ptr != value.data() + value.size() ||
				parsed < 1U || parsed > 22U) {
				return fail("arguments", 1U);
			}
			selected_effect = static_cast<std::uint8_t>(parsed);
		} else {
			return fail("arguments", 2U);
		}
	}
	if(song_path.empty() || efc_path.empty() || output_path.empty()) {
		return fail("arguments", 3U);
	}
	try {
		std::error_code output_system;
		const auto output_exists = fs::exists(output_path, output_system);
		if(output_system || output_exists) {
			return fail("output-exists",
				static_cast<std::size_t>(output_system.value()));
		}
		auto song = read_file(song_path, 1U << 20U);
		if(!song) {
			return fail("song-read", song.error());
		}
		auto efc = read_file(efc_path, th03::audio::pmd_effect_bank_max_bytes);
		if(!efc) {
			return fail("efc-read", efc.error());
		}
		const auto first = selected_effect == 0U ? 1U : selected_effect;
		const auto last = selected_effect == 0U ? 22U : selected_effect;
		std::vector<Scenario> scenarios{};
		scenarios.reserve(last - first + 1U);
		for(unsigned effect = first; effect <= last; ++effect) {
			auto scenario = run_scenario(
				*song, *efc, static_cast<std::uint8_t>(effect)
			);
			if(!scenario) {
				return fail("scenario", (effect * 100U) + scenario.error());
			}
			scenarios.push_back(std::move(*scenario));
		}
		std::error_code system;
		fs::create_directories(output_path, system);
		if(system || !fs::is_directory(output_path)) {
			return fail("output-directory", static_cast<std::size_t>(system.value()));
		}
		for(const auto &scenario : scenarios) {
			const auto name = effect_name(scenario.effect);
			if(!write_registers(output_path / (name + "-registers.tsv"), scenario) ||
				!write_events(output_path / (name + "-events.tsv"), scenario)) {
				return fail("output-write", scenario.effect);
			}
		}
		if(!write_manifest(
			output_path / "manifest.json", *song, *efc, scenarios
		)) {
			return fail("manifest-write");
		}
		std::cout << "status=complete\nscenarios=" << scenarios.size() << '\n';
		return 0;
	} catch(const std::bad_alloc &) {
		return fail("allocation");
	}
}
