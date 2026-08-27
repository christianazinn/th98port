#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/content/resources.hpp"
#include "th03/app/new_classic/content/new_classic_language.hpp"
#include "th03/app/new_classic/content/new_classic_cutscene_text.hpp"
#include "th03/present/cutscene_script.hpp"

namespace {

namespace fs = std::filesystem;

constexpr std::uint64_t fnv_offset = 14695981039346656037ULL;
constexpr std::uint64_t fnv_prime = 1099511628211ULL;
constexpr std::array<std::string_view, 28U> script_identifiers{
	"dm0.reimu", "dm0.mima", "dm0.marisa", "dm0.ellen",
	"dm0.kotohime", "dm0.kana", "dm0.rikako", "dm0.chiyuri",
	"dm0.yumemi", "dm1.reimu", "dm1.mima", "dm1.marisa",
	"dm1.ellen", "dm1.kotohime", "dm1.kana", "dm1.rikako",
	"dm1.chiyuri", "dm1.yumemi", "ending.reimu", "ending.mima",
	"ending.marisa", "ending.ellen", "ending.kotohime", "ending.kana",
	"ending.rikako", "ending.chiyuri", "ending.yumemi", "extra-ending",
};

struct ScriptReport final {
	std::string_view identifier{};
	std::string filename{};
	std::size_t source_bytes = 0U;
	std::uint64_t source_fnv1a64 = 0U;
	std::vector<th03::present::CutsceneTextRun> runs{};
};

void fnv_append(std::uint64_t &hash, std::uint8_t value) noexcept
{
	hash ^= value;
	hash *= fnv_prime;
}

std::uint64_t fnv1a64(std::span<const std::uint8_t> bytes) noexcept
{
	auto result = fnv_offset;
	for(const auto value : bytes) {
		fnv_append(result, value);
	}
	return result;
}

void print_hex_u64(std::uint64_t value)
{
	std::cout << std::hex << std::uppercase << std::setfill('0')
		<< std::setw(16) << value << std::dec;
}

void print_hex_bytes(std::span<const std::uint8_t> bytes)
{
	std::cout << std::hex << std::uppercase << std::setfill('0');
	for(const auto value : bytes) {
		std::cout << std::setw(2) << static_cast<unsigned>(value);
	}
	std::cout << std::dec;
}

void usage()
{
	std::cout
		<< "usage: th03-cutscene-text --data <extracted-content-root>\n"
		<< "       [--language-overlay <authenticated-TH3EN.DAT>]\n"
		<< "       [--cutscene-text-pack <compiled-T3CTP001>]\n"
		<< "\n"
		<< "Emits one deterministic T3CTR001 JSON inventory to stdout. The\n"
		<< "source text is CP932 hex in the external report; do not commit an\n"
		<< "inventory generated from original game data.\n";
}

int fail(std::string_view stage, std::size_t code = 0U)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode=" << code << '\n';
	return 1;
}

} // namespace

int main(int argc, char **argv)
{
	if(argc == 2 && std::string_view(argv[1]) == "--help") {
		usage();
		return 0;
	}
	if((argc < 3 || argc > 7 || (argc & 1) == 0) ||
		std::string_view(argv[1]) != "--data" ||
		std::string_view(argv[2]).empty()) {
		usage();
		return 2;
	}
	std::optional<fs::path> language_overlay{};
	std::optional<fs::path> cutscene_text_pack{};
	for(int index = 3; index < argc; index += 2) {
		const auto option = std::string_view(argv[index]);
		if(std::string_view(argv[index + 1]).empty() ||
			(option == "--language-overlay" && language_overlay) ||
			(option == "--cutscene-text-pack" && cutscene_text_pack)) {
			usage();
			return 2;
		}
		if(option == "--language-overlay") {
			language_overlay.emplace(argv[index + 1]);
		} else if(option == "--cutscene-text-pack") {
			cutscene_text_pack.emplace(argv[index + 1]);
		} else {
			usage();
			return 2;
		}
	}

	try {
		th03::app::MainResourceLoader loader{fs::path(argv[2])};
		if(language_overlay) {
			auto overlay = th03::app::load_new_classic_language_overlay(
				*language_overlay
			);
			if(!overlay || !loader.install_authenticated_overrides(
				std::move(*overlay).release_members()
			)) {
				return fail("language-overlay");
			}
		}
		std::optional<th03::app::NewClassicCutsceneTextCatalog> catalog{};
		if(cutscene_text_pack) {
			auto loaded = th03::app::load_new_classic_cutscene_text_pack(
				*cutscene_text_pack
			);
			if(!loaded) {
				return fail("cutscene-text-pack", loaded.error().offset);
			}
			catalog.emplace(std::move(*loaded));
		}
		std::vector<ScriptReport> reports{};
		reports.reserve(script_identifiers.size());
		auto profile_fnv1a64 = fnv_offset;
		for(std::size_t index = 0U; index < script_identifiers.size(); ++index) {
			const auto key = th03::app::MainResourceKey{
				.role = th03::app::MainResourceRole::cutscene_script,
				.cutscene_script = static_cast<th03::app::MainCutsceneScript>(
					index + 1U
				),
			};
			const auto spec = th03::app::main_resource_spec(key);
			if(!spec) {
				return fail("resource-spec", index);
			}
			const auto loaded = loader.load(key);
			if(!loaded) {
				return fail(
					"resource-load",
					static_cast<std::size_t>(loaded.error().code)
				);
			}
			const auto source = std::span<const std::uint8_t>{
				reinterpret_cast<const std::uint8_t *>((*loaded)->data()),
				(*loaded)->size(),
			};
			for(const auto value : source) {
				fnv_append(profile_fnv1a64, value);
			}
			auto runs = th03::present::extract_cutscene_text_runs(source);
			if(!runs) {
				return fail("script-parse", runs.error().byte_offset);
			}
			if(runs->empty()) {
				return fail("script-empty", index);
			}
			if(catalog) {
				auto replacements = catalog->replacement_glyphs(index, source);
				if(!replacements) {
					return fail("cutscene-text-materialize", index);
				}
			}
			reports.push_back(ScriptReport{
				.identifier = script_identifiers[index],
				.filename = std::string(spec->filename),
				.source_bytes = source.size(),
				.source_fnv1a64 = fnv1a64(source),
				.runs = std::move(*runs),
			});
		}
		if(catalog &&
			catalog->source_profile_fnv1a64() != profile_fnv1a64) {
			return fail("cutscene-text-profile");
		}

		std::cout << "{\n  \"format\": \"T3CTR001\",\n"
			<< "  \"source_profile\": \"th03-"
			<< (language_overlay ? "english-overlay" : "original")
			<< "-28\",\n"
			<< "  \"source_profile_fnv1a64\": \"";
		print_hex_u64(profile_fnv1a64);
		std::cout << "\",\n  \"scripts\": [\n";
		for(std::size_t script_index = 0U;
			script_index < reports.size(); ++script_index) {
			const auto &report = reports[script_index];
			std::cout << "    {\n      \"id\": \"" << report.identifier
				<< "\",\n      \"resource\": \"" << report.filename
				<< "\",\n      \"source_bytes\": " << report.source_bytes
				<< ",\n      \"source_fnv1a64\": \"";
			print_hex_u64(report.source_fnv1a64);
			std::cout << "\",\n      \"runs\": [\n";
			for(std::size_t run_index = 0U;
				run_index < report.runs.size(); ++run_index) {
				const auto &run = report.runs[run_index];
				std::cout << "        {\"id\": \"cutscene."
					<< report.identifier << ".run." << std::setfill('0')
					<< std::setw(3) << run.ordinal << std::setfill(' ')
					<< "\", \"ordinal\": " << run.ordinal
					<< ", \"source_offset\": " << run.source_offset
					<< ", \"glyphs\": " << (run.shift_jis.size() / 2U)
					<< ", \"first\": [" << run.first_position.x << ", "
					<< run.first_position.y << "], \"last\": ["
					<< run.last_position.x << ", " << run.last_position.y
					<< "], \"source_fnv1a64\": \"";
				print_hex_u64(fnv1a64(run.shift_jis));
				std::cout << "\", \"cp932_hex\": \"";
				print_hex_bytes(run.shift_jis);
				std::cout << "\"}";
				if(run_index + 1U != report.runs.size()) {
					std::cout << ',';
				}
				std::cout << '\n';
			}
			std::cout << "      ]\n    }";
			if(script_index + 1U != reports.size()) {
				std::cout << ',';
			}
			std::cout << '\n';
		}
		std::cout << "  ]\n}\n";
		return 0;
	} catch(const std::bad_alloc &) {
		return fail("allocation");
	}
}
