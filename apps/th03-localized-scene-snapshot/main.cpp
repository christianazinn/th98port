#include "classic_cutscene_scene_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <new>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/app/new_classic_cutscene_text.hpp"
#include "th03/app/new_classic_language.hpp"
#include "th03/app/resources.hpp"
#include "th03/present/raster.hpp"

namespace {

namespace fs = std::filesystem;
using th03::app::ClassicCutsceneInput;
using th03::app::ClassicCutscenePhase;
using th03::app::ClassicCutsceneScene;
using th03::app::MainCharacter;
using th03::app::MainResourceLoader;
using th03::app::MainlCutsceneKind;
using th03::app::MainlCutsceneResourceRequest;
using th03::app::NewClassicCutsceneTextCatalog;
using th03::present::Raster;

constexpr std::size_t scene_count = 28U;
constexpr std::size_t maximum_scene_ticks = 2'000'000U;
constexpr std::array<std::string_view, scene_count> scene_identifiers{
	"dm0.reimu", "dm0.mima", "dm0.marisa", "dm0.ellen",
	"dm0.kotohime", "dm0.kana", "dm0.rikako", "dm0.chiyuri",
	"dm0.yumemi", "dm1.reimu", "dm1.mima", "dm1.marisa",
	"dm1.ellen", "dm1.kotohime", "dm1.kana", "dm1.rikako",
	"dm1.chiyuri", "dm1.yumemi", "ending.reimu", "ending.mima",
	"ending.marisa", "ending.ellen", "ending.kotohime", "ending.kana",
	"ending.rikako", "ending.chiyuri", "ending.yumemi", "extra-ending",
};

struct RunSelection final {
	std::size_t index = 0U;
	std::size_t glyphs = 0U;
	std::size_t cumulative_glyphs = 0U;
	std::size_t total_glyphs = 0U;
	std::size_t total_runs = 0U;
};

struct SceneSnapshot final {
	std::string identifier{};
	std::string filename{};
	std::size_t script_index = 0U;
	std::size_t kind = 0U;
	std::size_t character = 0U;
	RunSelection selection{};
	std::size_t capture_tick = 0U;
	std::uint64_t display_state_hash = 0U;
	Raster raster{};
};

class PartialOutput final {
public:
	explicit PartialOutput(fs::path path) : path_(std::move(path)) {}
	~PartialOutput()
	{
		if(active_) {
			std::error_code ignored;
			fs::remove_all(path_, ignored);
		}
	}
	void release() noexcept { active_ = false; }

private:
	fs::path path_{};
	bool active_ = true;
};

[[nodiscard]] int fail(std::string_view stage, std::size_t code = 0U)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode=" << code << '\n';
	return 1;
}

void usage()
{
	std::cout
		<< "usage: th03-localized-scene-snapshot --content <extracted-root>\n"
		<< "       --cutscene-text-pack <locale.t3ctp> --output <new-dir>\n"
		<< "       [--language-overlay <TH3EN.DAT>]\n\n"
		<< "Renders one production-composed layout-stress snapshot for each of\n"
		<< "TH03's 28 authenticated cutscene scripts. The output directory must\n"
		<< "not already exist.\n";
}

[[nodiscard]] std::expected<std::vector<std::byte>, std::size_t> read_file(
	const fs::path &path,
	std::optional<std::size_t> exact_size = std::nullopt
)
{
	std::error_code system;
	const auto status = fs::status(path, system);
	if(system || !fs::is_regular_file(status)) {
		return std::unexpected(1U);
	}
	const auto size = fs::file_size(path, system);
	if(system || size > static_cast<std::uintmax_t>(SIZE_MAX) ||
		(exact_size && size != *exact_size)) {
		return std::unexpected(2U);
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(3U);
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	input.read(reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	if(!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
		return std::unexpected(4U);
	}
	return bytes;
}

[[nodiscard]] MainlCutsceneResourceRequest scene_request(
	std::size_t script
) noexcept
{
	constexpr std::array characters{
		MainCharacter::reimu,
		MainCharacter::mima,
		MainCharacter::marisa,
		MainCharacter::ellen,
		MainCharacter::kotohime,
		MainCharacter::kana,
		MainCharacter::rikako,
		MainCharacter::chiyuri,
		MainCharacter::yumemi,
	};
	if(script < 9U) {
		return {
			.kind = MainlCutsceneKind::interstage_chiyuri,
			.character = characters[script],
		};
	}
	if(script < 18U) {
		return {
			.kind = MainlCutsceneKind::interstage_yumemi,
			.character = characters[script - 9U],
		};
	}
	if(script < 27U) {
		return {
			.kind = MainlCutsceneKind::ending,
			.character = characters[script - 18U],
		};
	}
	return {
		.kind = MainlCutsceneKind::extra_ending,
		.character = MainCharacter::reimu,
	};
}

[[nodiscard]] std::expected<RunSelection, std::size_t> select_run(
	const NewClassicCutsceneTextCatalog &catalog,
	std::size_t script
)
{
	const auto count = catalog.run_count(script);
	if(count == 0U) {
		return std::unexpected(script);
	}
	RunSelection result{.total_runs = count};
	std::vector<std::size_t> glyph_counts{};
	glyph_counts.reserve(count);
	for(std::size_t run = 0U; run < count; ++run) {
		const auto bytes = catalog.replacement_run_bytes(script, run);
		if(bytes.empty() || (bytes.size() & 1U) != 0U) {
			return std::unexpected(script);
		}
		const auto glyphs = bytes.size() / 2U;
		glyph_counts.push_back(glyphs);
		result.total_glyphs += glyphs;
		result.glyphs = std::max(result.glyphs, glyphs);
	}
	const auto midpoint = count / 2U;
	std::size_t best_distance = count;
	for(std::size_t run = 0U; run < count; ++run) {
		if(glyph_counts[run] != result.glyphs) {
			continue;
		}
		const auto distance = (run > midpoint) ?
			(run - midpoint) : (midpoint - run);
		if(distance < best_distance) {
			result.index = run;
			best_distance = distance;
		}
	}
	for(std::size_t run = 0U; run <= result.index; ++run) {
		result.cumulative_glyphs += glyph_counts[run];
	}
	return result;
}

[[nodiscard]] std::expected<SceneSnapshot, std::size_t> render_scene(
	MainResourceLoader &loader,
	const NewClassicCutsceneTextCatalog &catalog,
	const th03::present::AnkGlyphProvider &font,
	std::size_t script
)
{
	const auto selection = select_run(catalog, script);
	if(!selection) {
		return std::unexpected(10U + selection.error());
	}
	const auto request = scene_request(script);
	auto resources = loader.load_mainl_cutscene(request);
	if(!resources) {
		return std::unexpected(100U + static_cast<std::size_t>(
			resources.error().code
		));
	}
	auto created = ClassicCutsceneScene::create(
		request, *resources, font, Raster{}, &catalog
	);
	if(!created) {
		return std::unexpected(200U + static_cast<std::size_t>(
			created.error().code
		));
	}
	auto scene = std::move(*created);
	std::optional<SceneSnapshot> snapshot{};
	std::size_t step = 0U;
	for(; step < maximum_scene_ticks && (
		scene->phase() == ClassicCutscenePhase::prelude ||
		scene->phase() == ClassicCutscenePhase::running
	); ++step) {
		while(scene->take_audio_command()) {
		}
		const bool held = (step & 1U) != 0U;
		auto advanced = scene->advance({
			.script = {
				.cancel_held = false,
				.any_key_held = held,
			},
			.shot_or_ok_held = held,
			.audio = {},
		});
		if(!advanced) {
			return std::unexpected(300U + static_cast<std::size_t>(
				advanced.error().code
			));
		}
		if(!snapshot && advanced->surface_changed &&
			scene->localized_glyphs_consumed() >=
				selection->cumulative_glyphs) {
			SceneSnapshot captured{};
			captured.identifier = std::string{scene_identifiers[script]};
			captured.filename = "scene-" +
				std::string{scene_identifiers[script]} + ".bmp";
			captured.script_index = script;
			captured.kind = static_cast<std::size_t>(request.kind);
			captured.character = static_cast<std::size_t>(request.character);
			captured.selection = *selection;
			captured.capture_tick = step;
			captured.display_state_hash = scene->raster().display_state_hash();
			captured.raster = scene->raster();
			snapshot.emplace(std::move(captured));
		}
	}
	while(scene->take_audio_command()) {
	}
	if(step >= maximum_scene_ticks ||
		scene->phase() != ClassicCutscenePhase::ready || !snapshot ||
		scene->localized_glyphs_consumed() != selection->total_glyphs) {
		return std::unexpected(400U + script);
	}
	return std::move(*snapshot);
}

void u16(std::span<std::uint8_t> bytes, std::size_t offset, std::uint16_t value)
{
	bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

void u32(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value)
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
	}
}

[[nodiscard]] bool write_bmp(const fs::path &path, const Raster &raster)
{
	constexpr std::size_t width = th03::present::graphics_width;
	constexpr std::size_t height = th03::present::graphics_height;
	constexpr std::size_t header_bytes = 14U + 40U + (16U * 4U);
	constexpr std::size_t row_bytes = width / 2U;
	constexpr std::size_t image_bytes = row_bytes * height;
	std::array<std::uint8_t, header_bytes> header{};
	header[0U] = 'B';
	header[1U] = 'M';
	u32(header, 2U, static_cast<std::uint32_t>(header_bytes + image_bytes));
	u32(header, 10U, static_cast<std::uint32_t>(header_bytes));
	u32(header, 14U, 40U);
	u32(header, 18U, static_cast<std::uint32_t>(width));
	u32(header, 22U, static_cast<std::uint32_t>(height));
	u16(header, 26U, 1U);
	u16(header, 28U, 4U);
	u32(header, 34U, static_cast<std::uint32_t>(image_bytes));
	u32(header, 46U, 16U);
	const auto palette = raster.applied_palette();
	for(std::size_t index = 0U; index < palette.colors.size(); ++index) {
		const auto base = 54U + (index * 4U);
		header[base + 0U] = static_cast<std::uint8_t>(
			palette.colors[index].b * 0x11U
		);
		header[base + 1U] = static_cast<std::uint8_t>(
			palette.colors[index].g * 0x11U
		);
		header[base + 2U] = static_cast<std::uint8_t>(
			palette.colors[index].r * 0x11U
		);
	}
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return false;
	}
	output.write(reinterpret_cast<const char *>(header.data()),
		static_cast<std::streamsize>(header.size()));
	const auto pixels = raster.shown_graphics();
	std::array<std::uint8_t, row_bytes> row{};
	for(std::size_t y = height; y-- > 0U;) {
		for(std::size_t x = 0U; x < width; x += 2U) {
			row[x / 2U] = static_cast<std::uint8_t>(
				(pixels[y * width + x] << 4U) | pixels[y * width + x + 1U]
			);
		}
		output.write(reinterpret_cast<const char *>(row.data()),
			static_cast<std::streamsize>(row.size()));
	}
	return static_cast<bool>(output);
}

[[nodiscard]] std::string hex_u64(std::uint64_t value)
{
	std::ostringstream output;
	output << std::uppercase << std::hex << std::setfill('0')
		<< std::setw(16) << value;
	return output.str();
}

[[nodiscard]] bool write_manifest(
	const fs::path &path,
	const NewClassicCutsceneTextCatalog &catalog,
	std::span<const SceneSnapshot> scenes
)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return false;
	}
	output << "{\n  \"format\": \"T3LSS001\",\n  \"locale\": \""
		<< catalog.locale() << "\",\n  \"source_profile_fnv1a64\": \""
		<< hex_u64(catalog.source_profile_fnv1a64())
		<< "\",\n  \"width\": 640,\n  \"height\": 400,\n"
		<< "  \"selection\": \"longest-localized-run-midpoint-tie\",\n"
		<< "  \"scenes\": [\n";
	for(std::size_t index = 0U; index < scenes.size(); ++index) {
		const auto &scene = scenes[index];
		output << "    {\"id\": \"" << scene.identifier
			<< "\", \"file\": \"" << scene.filename
			<< "\", \"script\": " << scene.script_index
			<< ", \"kind\": " << scene.kind
			<< ", \"character\": " << scene.character
			<< ", \"selected_run\": " << scene.selection.index
			<< ", \"selected_run_glyphs\": " << scene.selection.glyphs
			<< ", \"runs\": " << scene.selection.total_runs
			<< ", \"total_glyphs\": " << scene.selection.total_glyphs
			<< ", \"capture_tick\": " << scene.capture_tick
			<< ", \"display_state_fnv1a64\": \""
			<< hex_u64(scene.display_state_hash) << "\"}";
		if(index + 1U != scenes.size()) {
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
	fs::path content_path{};
	fs::path pack_path{};
	fs::path output_path{};
	fs::path overlay_path{};
	if(argc != 7 && argc != 9) {
		usage();
		return 2;
	}
	for(int index = 1; index < argc; index += 2) {
		const auto key = std::string_view(argv[index]);
		const auto value = fs::path(argv[index + 1]);
		if(value.empty()) {
			return 2;
		}
		if(key == "--content" && content_path.empty()) {
			content_path = value;
		} else if(key == "--cutscene-text-pack" && pack_path.empty()) {
			pack_path = value;
		} else if(key == "--output" && output_path.empty()) {
			output_path = value;
		} else if(key == "--language-overlay" && overlay_path.empty()) {
			overlay_path = value;
		} else {
			usage();
			return 2;
		}
	}
	if(content_path.empty() || pack_path.empty() || output_path.empty()) {
		usage();
		return 2;
	}
	try {
		std::error_code system;
		const auto absolute_output = fs::absolute(output_path, system);
		if(system || fs::exists(absolute_output, system) || system) {
			return fail("output-exists", static_cast<std::size_t>(system.value()));
		}
		const auto parent = absolute_output.parent_path();
		const auto partial_path = parent /
			(absolute_output.filename().string() + ".partial-T3LSS001");
		if(fs::exists(partial_path, system) || system) {
			return fail("partial-output-exists",
				static_cast<std::size_t>(system.value()));
		}
		auto font_bytes = read_file(
			content_path / "FONT.ROM",
			th03::app::Pc98FontRomProvider::font_rom_size
		);
		if(!font_bytes) {
			return fail("font-read", font_bytes.error());
		}
		auto font = th03::app::Pc98FontRomProvider::from_exact_font_rom(
			*font_bytes
		);
		if(!font) {
			return fail("font-validate", static_cast<std::size_t>(
				font.error().code
			));
		}
		auto catalog = th03::app::load_new_classic_cutscene_text_pack(pack_path);
		if(!catalog) {
			return fail("cutscene-text-pack", static_cast<std::size_t>(
				catalog.error().code
			));
		}
		MainResourceLoader loader(content_path);
		if(!overlay_path.empty()) {
			auto overlay = th03::app::load_new_classic_language_overlay(overlay_path);
			if(!overlay) {
				return fail("language-overlay", static_cast<std::size_t>(
					overlay.error().code
				));
			}
			if(!loader.install_authenticated_overrides(
				std::move(*overlay).release_members()
			)) {
				return fail("language-overlay-install");
			}
		}
		std::vector<SceneSnapshot> scenes{};
		scenes.reserve(scene_count);
		for(std::size_t script = 0U; script < scene_count; ++script) {
			auto rendered = render_scene(loader, *catalog, *font, script);
			if(!rendered) {
				return fail("scene-render", rendered.error());
			}
			scenes.push_back(std::move(*rendered));
		}
		fs::create_directories(parent, system);
		if(system || !fs::is_directory(parent)) {
			return fail("output-parent", static_cast<std::size_t>(system.value()));
		}
		fs::create_directory(partial_path, system);
		if(system) {
			return fail("partial-output-create",
				static_cast<std::size_t>(system.value()));
		}
		PartialOutput partial(partial_path);
		for(const auto &scene : scenes) {
			if(!write_bmp(partial_path / scene.filename, scene.raster)) {
				return fail("snapshot-write", scene.script_index);
			}
		}
		if(!write_manifest(partial_path / "snapshots.json", *catalog, scenes)) {
			return fail("manifest-write");
		}
		fs::rename(partial_path, absolute_output, system);
		if(system) {
			return fail("output-publish", static_cast<std::size_t>(system.value()));
		}
		partial.release();
		std::cout << "status=complete\nlocale=" << catalog->locale()
			<< "\nscenes=" << scenes.size() << "\noutput="
			<< absolute_output.string() << '\n';
		return 0;
	} catch(const std::bad_alloc &) {
		return fail("allocation");
	} catch(const fs::filesystem_error &error) {
		return fail("filesystem", static_cast<std::size_t>(error.code().value()));
	}
}
